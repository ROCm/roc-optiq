// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_controller_profiler_process.h"
#include "rocprofvis_controller_profiler_cmdline.h"
#include "rocprofvis_controller_profiler_scrape_rules.h"
#include "rocprofvis_controller_profiler_tool.h"
// TEMPORARY (remote/SSH): remove guard when remote graduates.
#ifdef ROCPROFVIS_ENABLE_REMOTE
#include "rocprofvis_controller_ssh_profiler_executor.h"
#endif
#include "rocprofvis_controller_future.h"
#include "rocprofvis_controller_job_system.h"
#include "spdlog/spdlog.h"
#include <chrono>
#include <filesystem>
#include <system_error>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#endif

namespace RocProfVis
{
namespace Controller
{

namespace
{

// Controller-authored line in the run output, marked so it is not mistaken for
// something the profiler said. Matches the scrape engine's own sink format.
void append_diagnostic(std::string& output_text, std::string const& text)
{
    if (!output_text.empty() && output_text.back() != '\n')
    {
        output_text.push_back('\n');
    }
    output_text += "[optiq] ";
    output_text += text;
    output_text.push_back('\n');
}

} // namespace

// ==================================================================================
// ProfilerConfig Implementation
// ==================================================================================

ProfilerConfig::ProfilerConfig()
    : Handle(0, 0)
    , m_tool(kRPVProfilerToolNone)
    , m_tool_directory()
    , m_resolved_tool_path()
    , m_output_directory()
    , m_working_directory()
    , m_env_vars()
    , m_profiler_argv()
    , m_connection_type(ConnectionType::kLocal)
    , m_ssh_info()
{
}

ProfilerConfig::~ProfilerConfig()
{
}

rocprofvis_controller_object_type_t ProfilerConfig::GetType(void)
{
    return kRPVProfilerConfig;
}

rocprofvis_result_t ProfilerConfig::SetTool(rocprofvis_profiler_tool_t tool)
{
    if (ProfilerTool::GetBinaryName(tool) == nullptr)
    {
        return kRocProfVisResultInvalidEnum;
    }
    m_tool = tool;
    return kRocProfVisResultSuccess;
}

rocprofvis_result_t ProfilerConfig::SetToolDirectory(char const* directory)
{
    if (directory == nullptr)
    {
        return kRocProfVisResultInvalidArgument;
    }
    m_tool_directory = directory;
    return kRocProfVisResultSuccess;
}

rocprofvis_result_t ProfilerConfig::ResolveToolPath()
{
    return ProfilerTool::ResolvePath(m_tool, m_tool_directory, m_resolved_tool_path);
}

rocprofvis_result_t ProfilerConfig::ResolveToolPathRemote()
{
    char const* binary_name = ProfilerTool::GetBinaryName(m_tool);
    if (binary_name == nullptr)
    {
        spdlog::error("ProfilerConfig::ResolveToolPathRemote: no tool selected");
        return kRocProfVisResultInvalidArgument;
    }

    if (!m_tool_directory.empty())
    {
        // POSIX-absolute: std::filesystem would apply this host's rules (and
        // join with '\\' on Windows). Remote is always POSIX.
        if (m_tool_directory.front() != '/')
        {
            spdlog::error("Remote profiler tool directory must be an absolute POSIX path, "
                          "got '{}'", m_tool_directory);
            return kRocProfVisResultInvalidArgument;
        }

        std::string directory = m_tool_directory;
        while (directory.size() > 1 && directory.back() == '/')
        {
            directory.pop_back();
        }
        m_resolved_tool_path =
            (directory == "/") ? ("/" + std::string(binary_name))
                               : (directory + "/" + binary_name);
        spdlog::warn("Using configured remote tool directory for '{}': '{}'", binary_name,
                     m_tool_directory);
        return kRocProfVisResultSuccess;
    }

    m_resolved_tool_path = binary_name;
    return kRocProfVisResultSuccess;
}

rocprofvis_result_t ProfilerConfig::SetOutputDirectory(char const* path)
{
    if (path == nullptr)
    {
        return kRocProfVisResultInvalidArgument;
    }
    m_output_directory = path;
    return kRocProfVisResultSuccess;
}

rocprofvis_result_t ProfilerConfig::SetWorkingDirectory(char const* path)
{
    if (path == nullptr)
    {
        return kRocProfVisResultInvalidArgument;
    }
    m_working_directory = path;
    return kRocProfVisResultSuccess;
}

rocprofvis_result_t ProfilerConfig::ValidateWorkingDirectory() const
{
    // Empty inherits this process's cwd.
    if (m_working_directory.empty())
    {
        return kRocProfVisResultSuccess;
    }

    std::error_code ec;
    if (!std::filesystem::is_directory(m_working_directory, ec) || ec)
    {
        spdlog::error("Profiler working directory '{}' is not an existing directory",
                      m_working_directory);
        return kRocProfVisResultInvalidArgument;
    }

    return kRocProfVisResultSuccess;
}

rocprofvis_result_t ProfilerConfig::AddEnvVar(char const* name, char const* value)
{
    if (name == nullptr || value == nullptr)
    {
        return kRocProfVisResultInvalidArgument;
    }
    // POSIX identifier only: the name is emitted unquoted in ToPosixShellCommand.
    if (!Cmdline::IsValidEnvName(name))
    {
        spdlog::warn("Ignoring environment variable with invalid name '{}'", name);
        return kRocProfVisResultInvalidArgument;
    }
    m_env_vars.emplace_back(std::string(name), std::string(value));
    return kRocProfVisResultSuccess;
}

rocprofvis_result_t ProfilerConfig::AddProfilerArg(char const* arg)
{
    if (arg == nullptr)
    {
        return kRocProfVisResultInvalidArgument;
    }
    m_profiler_argv.emplace_back(arg);
    return kRocProfVisResultSuccess;
}

rocprofvis_result_t ProfilerConfig::SetConnectionLocal()
{
    m_connection_type = ConnectionType::kLocal;
    m_ssh_info = SshConnectionInfo{};
    return kRocProfVisResultSuccess;
}

rocprofvis_result_t ProfilerConfig::SetConnectionSsh(char const* host, char const* user,
                                                     int port, char const* identity_file,
                                                     char const* remote_stage_dir)
{
    if (host == nullptr || user == nullptr)
    {
        return kRocProfVisResultInvalidArgument;
    }
    m_connection_type = ConnectionType::kSsh;
    m_ssh_info.host = host;
    m_ssh_info.user = user;
    m_ssh_info.port = port;
    m_ssh_info.identity_file = identity_file ? identity_file : "";
    m_ssh_info.remote_stage_dir = remote_stage_dir ? remote_stage_dir : "";
    return kRocProfVisResultSuccess;
}

rocprofvis_result_t ProfilerConfig::AddStage(ProfilerStageSpec const& stage)
{
    if (stage.tool == kRPVProfilerToolNone)
    {
        spdlog::error("Profiler stage '{}' names no tool", stage.label);
        return kRocProfVisResultInvalidArgument;
    }
    m_stages.push_back(stage);
    return kRocProfVisResultSuccess;
}

rocprofvis_result_t ProfilerConfig::SetArtifactKey(char const* key)
{
    if (key == nullptr)
    {
        return kRocProfVisResultInvalidArgument;
    }
    m_artifact_key = key;
    return kRocProfVisResultSuccess;
}

void ProfilerConfig::ApplyStage(ProfilerStageSpec const& stage,
                                std::string const&       resolved_tool_path)
{
    m_tool                = stage.tool;
    m_tool_directory      = stage.tool_directory;
    m_resolved_tool_path  = resolved_tool_path;
    m_working_directory   = stage.working_directory;
    m_profiler_argv       = stage.argv;
    m_env_vars            = stage.env;
    // A stage's own list is not the pipeline's; carrying it would make the
    // per-stage config look like a pipeline and invite a nested launch.
    m_stages.clear();
}

// ==================================================================================
// LocalProfilerExecutor Implementation - Windows
// ==================================================================================

#ifdef _WIN32

LocalProfilerExecutor::LocalProfilerExecutor()
    : m_process_handle(nullptr)
    , m_stdout_read_handle(nullptr)
    , m_stdout_write_handle(nullptr)
    , m_stderr_read_handle(nullptr)
    , m_stderr_write_handle(nullptr)
    , m_is_running(false)
    , m_exit_code(0)
{
}

LocalProfilerExecutor::~LocalProfilerExecutor()
{
    if (IsRunning())
    {
        Cancel();
    }
    CloseHandles();
}

void LocalProfilerExecutor::CloseHandles()
{
    if (m_stdout_read_handle)
    {
        CloseHandle(m_stdout_read_handle);
        m_stdout_read_handle = nullptr;
    }
    if (m_stdout_write_handle)
    {
        CloseHandle(m_stdout_write_handle);
        m_stdout_write_handle = nullptr;
    }
    if (m_stderr_read_handle)
    {
        CloseHandle(m_stderr_read_handle);
        m_stderr_read_handle = nullptr;
    }
    if (m_stderr_write_handle)
    {
        CloseHandle(m_stderr_write_handle);
        m_stderr_write_handle = nullptr;
    }
    if (m_process_handle)
    {
        CloseHandle(m_process_handle);
        m_process_handle = nullptr;
    }
}

bool LocalProfilerExecutor::Start(const ProfilerConfig& config)
{
    if (config.GetConnectionType() != ConnectionType::kLocal)
    {
        spdlog::error("LocalProfilerExecutor: SSH connections are not yet supported");
        return false;
    }

    std::string cmd_line_str = Cmdline::ToWindowsCommandLine(Cmdline::BuildArgv(config));

    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = nullptr;

    if (!CreatePipe(&m_stdout_read_handle, &m_stdout_write_handle, &sa, 0))
    {
        return false;
    }
    SetHandleInformation(m_stdout_read_handle, HANDLE_FLAG_INHERIT, 0);

    if (!CreatePipe(&m_stderr_read_handle, &m_stderr_write_handle, &sa, 0))
    {
        CloseHandles();
        return false;
    }
    SetHandleInformation(m_stderr_read_handle, HANDLE_FLAG_INHERIT, 0);

    std::string env_block;
    if (!config.GetEnvVars().empty())
    {
        // Inherit the current environment, then append ours.
        char* current_env = GetEnvironmentStrings();
        if (current_env)
        {
            char* p = current_env;
            while (*p)
            {
                size_t len = strlen(p);
                env_block.append(p, len + 1);
                p += len + 1;
            }
            FreeEnvironmentStrings(current_env);
        }
        for (auto const& kv : config.GetEnvVars())
        {
            std::string entry = kv.first + "=" + kv.second;
            env_block.append(entry.c_str(), entry.size() + 1);
        }
        env_block.push_back('\0');
    }

    STARTUPINFOA si = {};
    si.cb = sizeof(STARTUPINFOA);
    si.hStdOutput = m_stdout_write_handle;
    si.hStdError = m_stderr_write_handle;
    si.dwFlags |= STARTF_USESTDHANDLES;

    PROCESS_INFORMATION pi = {};

    std::vector<char> cmd_line_buffer(cmd_line_str.begin(), cmd_line_str.end());
    cmd_line_buffer.push_back('\0');

    LPVOID env_ptr = env_block.empty() ? nullptr : env_block.data();

    // Child cwd only. CreateProcessA fails if the directory does not exist.
    LPCSTR working_dir_ptr =
        config.GetWorkingDirectory().empty() ? nullptr : config.GetWorkingDirectory().c_str();

    BOOL success = CreateProcessA(
        nullptr,
        cmd_line_buffer.data(),
        nullptr,
        nullptr,
        TRUE,
        0,
        env_ptr,
        working_dir_ptr,
        &si,
        &pi);

    if (!success)
    {
        CloseHandles();
        return false;
    }

    m_process_handle = pi.hProcess;
    CloseHandle(pi.hThread);

    CloseHandle(m_stdout_write_handle);
    m_stdout_write_handle = nullptr;
    CloseHandle(m_stderr_write_handle);
    m_stderr_write_handle = nullptr;

    m_is_running = true;
    return true;
}

bool LocalProfilerExecutor::IsRunning()
{
    if (!m_is_running || m_process_handle == nullptr)
    {
        return false;
    }

    DWORD exit_code = 0;
    if (GetExitCodeProcess(m_process_handle, &exit_code))
    {
        if (exit_code != STILL_ACTIVE)
        {
            m_is_running = false;
            m_exit_code = static_cast<int>(exit_code);
            return false;
        }
    }

    return true;
}

bool LocalProfilerExecutor::Cancel()
{
    if (m_process_handle == nullptr)
    {
        return false;
    }

    if (TerminateProcess(m_process_handle, 1))
    {
        m_is_running = false;
        m_exit_code = 1;
        return true;
    }

    return false;
}

int LocalProfilerExecutor::GetExitCode() const
{
    return m_exit_code;
}

// Drain one pipe until it reports nothing pending. Reading a single buffer per
// call would cap a stage at 4 KiB per poll and, worse, leave bytes behind after
// the child exited: the caller ends the stage as soon as the process is gone,
// so whatever is still in the pipe would never be read at all.
static void read_pipe_available(HANDLE pipe, std::string& output)
{
    if (pipe == nullptr)
    {
        return;
    }

    char buffer[4096];
    for (;;)
    {
        DWORD bytes_available = 0;
        if (!PeekNamedPipe(pipe, nullptr, 0, nullptr, &bytes_available, nullptr) ||
            bytes_available == 0)
        {
            return;
        }

        DWORD bytes_read = 0;
        if (!ReadFile(pipe, buffer, sizeof(buffer), &bytes_read, nullptr) || bytes_read == 0)
        {
            return;
        }
        output.append(buffer, bytes_read);
    }
}

std::string LocalProfilerExecutor::ReadOutput()
{
    std::lock_guard<std::mutex> lock(m_output_mutex);

    std::string output;
    read_pipe_available(m_stdout_read_handle, output);
    read_pipe_available(m_stderr_read_handle, output);
    return output;
}

#else

// ==================================================================================
// LocalProfilerExecutor - POSIX (Linux today).
//
// fork()+execvp() is not safe on macOS (Obj-C runtime, non-async-signal-safe
// setenv/alloc in the child, Mach VM duplication). Refactor to posix_spawnp()
// before enabling the profiler there. Linux is the only POSIX target today.
// ==================================================================================

// Grace period after SIGTERM before escalating to SIGKILL during Cancel().
static constexpr int SIGTERM_GRACE_MS = 100;

// Pre-exec child statuses (POSIX shell convention): not a profiled-command code.
static constexpr int EXIT_CODE_CANNOT_EXECUTE    = 126;  // found, but setup failed
static constexpr int EXIT_CODE_COMMAND_NOT_FOUND = 127;  // execvp could not find it
static constexpr int EXIT_CODE_SIGNAL_BASE       = 128;  // + signal number

static void set_exit_code_from_status(int status, int& exit_code)
{
    if (WIFEXITED(status))
    {
        exit_code = WEXITSTATUS(status);
    }
    else if (WIFSIGNALED(status))
    {
        exit_code = EXIT_CODE_SIGNAL_BASE + WTERMSIG(status);
    }
}

LocalProfilerExecutor::LocalProfilerExecutor()
    : m_process_id(-1)
    , m_stdout_fd(-1)
    , m_stderr_fd(-1)
    , m_is_running(false)
    , m_exit_code(0)
{
}

LocalProfilerExecutor::~LocalProfilerExecutor()
{
    if (IsRunning())
    {
        Cancel();
    }
    CloseHandles();
}

void LocalProfilerExecutor::CloseHandles()
{
    if (m_stdout_fd != -1)
    {
        close(m_stdout_fd);
        m_stdout_fd = -1;
    }
    if (m_stderr_fd != -1)
    {
        close(m_stderr_fd);
        m_stderr_fd = -1;
    }
}

bool LocalProfilerExecutor::Start(const ProfilerConfig& config)
{
    if (config.GetConnectionType() != ConnectionType::kLocal)
    {
        spdlog::error("LocalProfilerExecutor: SSH connections are not yet supported");
        return false;
    }

    // Build argv before fork so the child does not allocate between fork and exec.
    std::vector<std::string> argv_tokens = Cmdline::BuildArgv(config);

    std::vector<char*> argv_cstr;
    argv_cstr.reserve(argv_tokens.size() + 1);
    for (auto& tok : argv_tokens)
    {
        argv_cstr.push_back(const_cast<char*>(tok.c_str()));
    }
    argv_cstr.push_back(nullptr);

    int stdout_pipe[2];
    int stderr_pipe[2];

    if (pipe(stdout_pipe) != 0 || pipe(stderr_pipe) != 0)
    {
        return false;
    }

    // Linux-only today; posix_spawnp before enabling on macOS (section banner).
    m_process_id = fork();

    if (m_process_id == -1)
    {
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
        close(stderr_pipe[0]);
        close(stderr_pipe[1]);
        return false;
    }

    if (m_process_id == 0)
    {
        close(stdout_pipe[0]);
        close(stderr_pipe[0]);

        dup2(stdout_pipe[1], STDOUT_FILENO);
        dup2(stderr_pipe[1], STDERR_FILENO);

        close(stdout_pipe[1]);
        close(stderr_pipe[1]);

        // Child cwd only. Fail rather than exec in the wrong directory.
        if (!config.GetWorkingDirectory().empty())
        {
            if (chdir(config.GetWorkingDirectory().c_str()) != 0)
            {
                int const chdir_errno = errno;
                // Writes to the capture pipe (dup2 above). Allocates; same
                // post-fork risk as setenv. posix_spawn is the real fix.
                std::string msg = "chdir failed for working directory '";
                msg += config.GetWorkingDirectory();
                msg += "': errno ";
                msg += std::to_string(chdir_errno);
                msg += '\n';
                (void)write(STDERR_FILENO, msg.c_str(), msg.size());
                _exit(EXIT_CODE_CANNOT_EXECUTE);
            }
        }

        // setenv is not async-signal-safe; see the POSIX section banner.
        for (auto const& kv : config.GetEnvVars())
        {
            setenv(kv.first.c_str(), kv.second.c_str(), 1);
        }

        execvp(argv_cstr[0], argv_cstr.data());

        int const exec_errno = errno;
        std::string msg = "execvp failed for '";
        msg += config.GetResolvedToolPath();
        msg += "': errno ";
        msg += std::to_string(exec_errno);
        msg += '\n';
        (void)write(STDERR_FILENO, msg.c_str(), msg.size());
        _exit(EXIT_CODE_COMMAND_NOT_FOUND);
    }

    close(stdout_pipe[1]);
    close(stderr_pipe[1]);

    m_stdout_fd = stdout_pipe[0];
    m_stderr_fd = stderr_pipe[0];

    fcntl(m_stdout_fd, F_SETFL, O_NONBLOCK);
    fcntl(m_stderr_fd, F_SETFL, O_NONBLOCK);

    m_is_running = true;
    return true;
}

bool LocalProfilerExecutor::IsRunning()
{
    if (!m_is_running || m_process_id == -1)
    {
        return false;
    }

    int   status = 0;
    pid_t result = waitpid(m_process_id, &status, WNOHANG);

    if (result == m_process_id)
    {
        set_exit_code_from_status(status, m_exit_code);
        m_process_id = -1;  // reaped; do not wait/kill a recycled pid
        m_is_running = false;
        return false;
    }

    return true;
}

bool LocalProfilerExecutor::Cancel()
{
    if (m_process_id == -1)
    {
        return false;
    }

    if (kill(m_process_id, SIGTERM) != 0)
    {
        m_process_id = -1;  // gone already
        m_is_running = false;
        return false;
    }

    // SIGTERM grace, then SIGKILL. Always reap so the child is not a zombie.
    std::this_thread::sleep_for(std::chrono::milliseconds(SIGTERM_GRACE_MS));

    int   status = 0;
    pid_t result = waitpid(m_process_id, &status, WNOHANG);
    if (result == 0)
    {
        kill(m_process_id, SIGKILL);
        result = waitpid(m_process_id, &status, 0);
    }

    if (result == m_process_id)
    {
        set_exit_code_from_status(status, m_exit_code);
    }
    else
    {
        m_exit_code = 1;  // e.g. ECHILD
    }

    m_process_id = -1;
    m_is_running = false;
    return true;
}

int LocalProfilerExecutor::GetExitCode() const
{
    return m_exit_code;
}

std::string LocalProfilerExecutor::ReadOutput()
{
    std::lock_guard<std::mutex> lock(m_output_mutex);

    std::string output;
    char buffer[4096];

    if (m_stdout_fd != -1)
    {
        ssize_t bytes_read;
        while ((bytes_read = read(m_stdout_fd, buffer, sizeof(buffer))) > 0)
        {
            output.append(buffer, static_cast<size_t>(bytes_read));
        }
    }

    if (m_stderr_fd != -1)
    {
        ssize_t bytes_read;
        while ((bytes_read = read(m_stderr_fd, buffer, sizeof(buffer))) > 0)
        {
            output.append(buffer, static_cast<size_t>(bytes_read));
        }
    }

    return output;
}

#endif

// ==================================================================================
// ProfilerProcessController Implementation
// ==================================================================================

ProfilerProcessController::ProfilerProcessController()
    : m_executor(nullptr)
    , m_config(nullptr)
    , m_state(kRPVProfilerStateIdle)
    , m_output_text()
    , m_exit_code(-1)
{
}

ProfilerProcessController::~ProfilerProcessController()
{
    // Cancel, then wait until the monitor Job (raw `this`) is destroyed.
    // Caller must free the bound future first or concurrently; the View does.
    Cancel();
    std::unique_lock<std::mutex> lock(m_job_mutex);
    m_job_cv.wait(lock, [this] { return !m_job_active; });
}

void ProfilerProcessController::BeginMonitorJob()
{
    std::lock_guard<std::mutex> lock(m_job_mutex);
    m_job_active = true;
}

void ProfilerProcessController::EndMonitorJob()
{
    {
        std::lock_guard<std::mutex> lock(m_job_mutex);
        m_job_active = false;
    }
    m_job_cv.notify_all();
}

rocprofvis_result_t ProfilerProcessController::PreparePipeline(bool resolve_tools_locally)
{
    m_stages.clear();
    m_stage_tool_paths.clear();
    m_stage_artifact_keys.clear();
    m_stage_states.clear();
    m_current_stage    = 0;
    m_failing_stage    = -1;
    m_cancel_requested = false;

    if (m_config->GetStages().empty())
    {
        // A config with no stages is the flat tool/argv/env/cwd every caller
        // uses today. Wrapping it as a one-element pipeline means there is only
        // one execution path to maintain, and no banner keeps its console
        // byte-identical to before.
        ProfilerStageSpec stage;
        stage.tool              = m_config->GetTool();
        stage.tool_directory    = m_config->GetToolDirectory();
        stage.argv              = m_config->GetProfilerArgv();
        stage.working_directory = m_config->GetWorkingDirectory();
        stage.env               = m_config->GetEnvVars();
        m_stages.push_back(std::move(stage));
        m_emit_banners = false;
    }
    else
    {
        m_stages       = m_config->GetStages();
        m_emit_banners = m_stages.size() > 1;
    }

    ProfilerScrapeRules::ApplyAll(m_stages, m_stage_artifact_keys, m_artifact_key);
    if (!m_config->GetArtifactKey().empty())
    {
        m_artifact_key = m_config->GetArtifactKey();
    }

    rocprofvis_result_t compiled = m_scrape.Compile(m_stages);
    if (compiled != kRocProfVisResultSuccess)
    {
        return compiled;
    }
    m_scrape.SetDiagnosticSink(&m_output_text);

    m_stage_states.assign(m_stages.size(), kRPVProfilerStateIdle);
    m_stage_tool_paths.assign(m_stages.size(), std::string());

    if (!resolve_tools_locally)
    {
        // Remote drives its own executor and cannot use StartStageLocked, which
        // would spawn a local process for stage 1. Multi-stage remote is Phase 6.
        if (m_stages.size() > 1)
        {
            spdlog::error("Remote profiler launches are single-stage for now ({} requested)",
                          m_stages.size());
            return kRocProfVisResultNotSupported;
        }
        m_stage_tool_paths[0] = m_config->GetResolvedToolPath();
        return kRocProfVisResultSuccess;
    }

    // Every tool up front. A missing analyze binary must not be discovered
    // after a capture that took twenty minutes, and attributing it to its own
    // stage is what lets the caller say which one is misconfigured.
    for (size_t i = 0; i < m_stages.size(); ++i)
    {
        rocprofvis_result_t resolved = ProfilerTool::ResolvePath(
            m_stages[i].tool, m_stages[i].tool_directory, m_stage_tool_paths[i]);
        if (resolved != kRocProfVisResultSuccess)
        {
            m_failing_stage = static_cast<int32_t>(i);
            m_scrape.SkipRemainingFrom(0);
            spdlog::error("Profiler stage {} tool could not be resolved", i);
            return resolved;
        }
    }

    return kRocProfVisResultSuccess;
}

rocprofvis_result_t ProfilerProcessController::StartStageLocked(uint32_t stage_index)
{
    m_current_stage = stage_index;

    ProfilerStageSpec stage = m_stages[stage_index];

    // Substituted into the stage we keep, so EndStage resolves relative paths
    // against the directory the child actually ran in.
    std::string error_message;
    rocprofvis_result_t substituted =
        resolve_stage_placeholders(stage.argv, m_scrape, error_message);
    if (substituted == kRocProfVisResultSuccess)
    {
        // The working directory and env values carry the same tokens: steering
        // compute's analyze means pointing its cwd at a directory that an
        // earlier stage reported.
        std::vector<std::string> deferred;
        deferred.push_back(stage.working_directory);
        for (auto const& kv : stage.env)
        {
            deferred.push_back(kv.second);
        }
        substituted = resolve_stage_placeholders(deferred, m_scrape, error_message);
        if (substituted == kRocProfVisResultSuccess)
        {
            stage.working_directory = deferred[0];
            for (size_t i = 0; i < stage.env.size(); ++i)
            {
                stage.env[i].second = deferred[i + 1];
            }
        }
    }

    if (substituted != kRocProfVisResultSuccess)
    {
        m_failing_stage = static_cast<int32_t>(stage_index);
        m_stage_states[stage_index] = kRPVProfilerStateFailed;
        m_scrape.SkipRemainingFrom(stage_index);
        append_diagnostic(m_output_text, error_message);
        spdlog::error("Profiler stage {}: {}", stage_index, error_message);
        return substituted;
    }

    m_stages[stage_index] = stage;

    m_stage_config = std::make_unique<ProfilerConfig>(*m_config);
    m_stage_config->ApplyStage(stage, m_stage_tool_paths[stage_index]);

    // Fail here rather than as child exit 126 after chdir.
    rocprofvis_result_t working_directory_valid = m_stage_config->ValidateWorkingDirectory();
    if (working_directory_valid != kRocProfVisResultSuccess)
    {
        m_failing_stage = static_cast<int32_t>(stage_index);
        m_stage_states[stage_index] = kRPVProfilerStateFailed;
        m_scrape.SkipRemainingFrom(stage_index);
        // Checking up front means no process is created, so there is no child
        // output to explain the failure; the console still has to say why.
        append_diagnostic(m_output_text,
                          "The working directory '" + stage.working_directory +
                              "' does not exist, so nothing was run.");
        return working_directory_valid;
    }

    if (m_emit_banners)
    {
        std::string banner = "=== Stage " + std::to_string(stage_index + 1) + "/" +
                             std::to_string(m_stages.size());
        if (!stage.label.empty())
        {
            banner += ": " + stage.label;
        }
        banner += " ===";
        if (!m_output_text.empty() && m_output_text.back() != '\n')
        {
            m_output_text.push_back('\n');
        }
        m_output_text += banner;
        m_output_text.push_back('\n');
    }

    for (auto const& kv : stage.env)
    {
        spdlog::info("Profiler env: {}={}", kv.first, kv.second);
    }

    if (!stage.working_directory.empty())
    {
        spdlog::info("Profiler working directory: {}", stage.working_directory);
    }

    spdlog::info("Profiler launch: {}",
                 Cmdline::ToDisplayString(Cmdline::BuildArgv(*m_stage_config)));

    // The previous stage's executor has already stopped; releasing it here,
    // under the lock, is what keeps GetOutput from ever seeing a half-swapped
    // pair.
    m_executor = std::make_unique<LocalProfilerExecutor>();
    m_scrape.BeginStage(stage_index);

    if (!m_executor->Start(*m_stage_config))
    {
        spdlog::error("ProfilerProcessController::StartStageLocked: failed to start process "
                      "(executable='{}')", m_stage_config->GetResolvedToolPath());
        m_failing_stage = static_cast<int32_t>(stage_index);
        m_stage_states[stage_index] = kRPVProfilerStateFailed;
        m_scrape.SkipRemainingFrom(stage_index);
        m_executor.reset();
        return kRocProfVisResultUnknownError;
    }

    m_stage_states[stage_index] = kRPVProfilerStateRunning;
    spdlog::info("Profiler stage {} launched successfully", stage_index);
    return kRocProfVisResultSuccess;
}

rocprofvis_result_t ProfilerProcessController::LaunchAsync(ProfilerConfig const* config)
{
    if (config == nullptr)
    {
        spdlog::error("ProfilerProcessController::LaunchAsync: config is null");
        return kRocProfVisResultInvalidArgument;
    }

    if (m_state != kRPVProfilerStateIdle)
    {
        spdlog::error("ProfilerProcessController::LaunchAsync: already running (state={})",
                      static_cast<int>(m_state.load()));
        return kRocProfVisResultNotSupported;
    }

    m_config = std::make_unique<ProfilerConfig>(*config);

    if (config->GetConnectionType() == ConnectionType::kSsh)
    {
        spdlog::error("ProfilerProcessController::LaunchAsync: SSH connections are not yet supported");
        m_state = kRPVProfilerStateFailed;
        return kRocProfVisResultNotSupported;
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    // Resolving every stage's tool, validating its working directory, logging
    // the launch and starting the process all happen inside the pipeline now,
    // so a flat config and a staged one take the same path.
    rocprofvis_result_t prepared = PreparePipeline(true);
    if (prepared != kRocProfVisResultSuccess)
    {
        m_state = kRPVProfilerStateFailed;
        return prepared;
    }

    rocprofvis_result_t started = StartStageLocked(0);
    if (started != kRocProfVisResultSuccess)
    {
        m_state = kRPVProfilerStateFailed;
        return started;
    }

    m_state = kRPVProfilerStateRunning;
    return kRocProfVisResultSuccess;
}

#ifdef ROCPROFVIS_ENABLE_REMOTE
// TEMPORARY (remote/SSH): remote profiler launch. Remove guard when remote
// graduates.
rocprofvis_result_t ProfilerProcessController::LaunchAsyncRemote(ProfilerConfig const* config,
                                                                 SshConnection*        connection,
                                                                 Future*               future)
{
    if (config == nullptr || connection == nullptr)
    {
        spdlog::error("ProfilerProcessController::LaunchAsyncRemote: null config/connection");
        return kRocProfVisResultInvalidArgument;
    }

    if (m_state != kRPVProfilerStateIdle)
    {
        spdlog::error("ProfilerProcessController::LaunchAsyncRemote: already running (state={})",
                      static_cast<int>(m_state.load()));
        return kRocProfVisResultNotSupported;
    }

    m_config = std::make_unique<ProfilerConfig>(*config);

    rocprofvis_result_t resolved = m_config->ResolveToolPathRemote();
    if (resolved != kRocProfVisResultSuccess)
    {
        m_state = kRPVProfilerStateFailed;
        return resolved;
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    // Remote runs one stage, but it goes through the same bookkeeping so the
    // scrape engine and the stage getters behave identically either way.
    rocprofvis_result_t prepared = PreparePipeline(false);
    if (prepared != kRocProfVisResultSuccess)
    {
        m_state = kRPVProfilerStateFailed;
        return prepared;
    }

    // Worker reads `future` after Start; ABI binds the job immediately after this.
    m_executor = std::make_unique<SshProfilerExecutor>(connection, future);
    m_scrape.BeginStage(0);

    bool launched = m_executor->Start(*m_config);
    if (!launched)
    {
        spdlog::error("ProfilerProcessController::LaunchAsyncRemote: failed to start remote profiler");
        m_stage_states[0] = kRPVProfilerStateFailed;
        m_failing_stage   = 0;
        m_state           = kRPVProfilerStateFailed;
        m_executor.reset();
        return kRocProfVisResultUnknownError;
    }

    spdlog::info("Remote profiler launched successfully");
    m_stage_states[0] = kRPVProfilerStateRunning;
    m_state           = kRPVProfilerStateRunning;
    return kRocProfVisResultSuccess;
}
#endif  // ROCPROFVIS_ENABLE_REMOTE

rocprofvis_profiler_state_t ProfilerProcessController::GetState() const
{
    return m_state;
}

void ProfilerProcessController::DrainExecutorLocked()
{
    if (!m_executor)
    {
        return;
    }

    std::string new_output = m_executor->ReadOutput();
    if (new_output.empty())
    {
        return;
    }

    // Appended before feeding, so a diagnostic the engine injects lands after
    // the output that provoked it rather than in front of it.
    m_output_text += new_output;
    m_scrape.Feed(new_output);
}

bool ProfilerProcessController::ExecutorRunning() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_executor && m_executor->IsRunning();
}

std::string ProfilerProcessController::GetOutput()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    DrainExecutorLocked();
    return m_output_text;
}

void ProfilerProcessController::ClearOutput()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_output_text.clear();
}

int ProfilerProcessController::GetExitCode() const
{
    // Written under m_mutex when a stage ends, read from the UI thread through
    // the C ABI.
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_exit_code;
}

rocprofvis_result_t ProfilerProcessController::Cancel()
{
    IProfilerExecutor* executor = nullptr;
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (m_state != kRPVProfilerStateRunning || m_cancel_requested)
        {
            return kRocProfVisResultNotSupported;
        }

        if (!m_executor)
        {
            return kRocProfVisResultUnknownError;
        }

        m_cancel_requested = true;
        executor           = m_executor.get();
    }

    /*
     * Killing happens with m_mutex released. It is not quick: the executor
     * gives the child a SIGTERM grace period and then blocks in waitpid, which
     * a process stuck in a driver call does not leave promptly. GetOutput takes
     * the same lock, so holding it here would freeze the console the user is
     * watching for as long as the child takes to die.
     *
     * Reading the raw pointer outside the lock is safe because m_cancel_requested
     * is already set: the only code that replaces m_executor is a stage
     * boundary, and that now declines to start anything once cancel is
     * requested.
     */
    bool const stopped = executor->Cancel();

    std::lock_guard<std::mutex> lock(m_mutex);

    if (!stopped)
    {
        // Nothing to kill - the child had already exited and been reaped. Hand
        // the ending back to UpdateState, which will settle the stage normally.
        m_cancel_requested = false;
        return kRocProfVisResultUnknownError;
    }

    // Whatever the child managed to print before it died is still real, and on
    // POSIX the pipe outlives it. Draining and ending the stage here is what
    // resolves those values against the stage's working directory; leaving it
    // to UpdateState would not work, since it no longer finalises a cancelled
    // run.
    DrainExecutorLocked();
    m_scrape.EndStage(m_stages[m_current_stage].working_directory);

    m_exit_code = executor->GetExitCode();

    if (m_current_stage < m_stage_states.size())
    {
        m_stage_states[m_current_stage] = kRPVProfilerStateCancelled;
    }
    // The stages after this one never start, so nothing they declare can
    // arrive. The current stage keeps what it scraped: a capture killed part
    // way through still names the file it was writing, and that file is on
    // disk.
    m_scrape.SkipRemainingFrom(m_current_stage + 1);
    m_state = kRPVProfilerStateCancelled;
    return kRocProfVisResultSuccess;
}

void ProfilerProcessController::RelocateArtifactLocked(uint32_t stage_index)
{
    ProfilerStageSpec const& stage = m_stages[stage_index];
    if (stage.relocate_to.empty())
    {
        return;
    }

    std::string const& key = m_stage_artifact_keys[stage_index];
    if (key.empty())
    {
        return;
    }

    std::string source;
    if (!m_scrape.GetStageValue(stage_index, key, source) || source.empty())
    {
        return;
    }

    std::filesystem::path const source_path(source);
    std::error_code             ec;
    std::filesystem::create_directories(stage.relocate_to, ec);

    std::filesystem::path const dest_path =
        std::filesystem::path(stage.relocate_to) / source_path.filename();

    // Checked rather than left to rename, which would silently overwrite on
    // POSIX. Whatever is already there was not produced by this run.
    if (std::filesystem::exists(dest_path))
    {
        std::string const text = "Could not move the artifact to '" + dest_path.string() +
                                 "' because a file is already there; leaving it at the original "
                                 "path '" + source + "'";
        spdlog::warn("{}", text);
        append_diagnostic(m_output_text, text);
        return;
    }

    ec.clear();
    std::filesystem::rename(source_path, dest_path, ec);
    if (ec)
    {
        // rename cannot cross filesystems, which a scratch directory under
        // /tmp and a destination on a home volume routinely do.
        std::error_code copy_ec;
        std::filesystem::copy_file(source_path, dest_path,
                                   std::filesystem::copy_options::none, copy_ec);
        if (copy_ec)
        {
            // The run succeeded and a valid file exists; reporting the path it
            // is actually at beats reporting a path that holds nothing.
            std::string const text = "Could not move the artifact to '" + dest_path.string() +
                                     "' (" + copy_ec.message() +
                                     "); leaving it at the original path '" + source + "'";
            spdlog::warn("{}", text);
            append_diagnostic(m_output_text, text);
            return;
        }

        std::error_code remove_ec;
        std::filesystem::remove(source_path, remove_ec);
        if (remove_ec)
        {
            spdlog::warn("Copied the artifact to '{}' but could not remove '{}': {}",
                         dest_path.string(), source, remove_ec.message());
        }
    }

    spdlog::info("Moved the artifact to '{}'", dest_path.string());
    m_scrape.SetValue(stage_index, key, dest_path.string());
}

void ProfilerProcessController::FinishStageLocked(int exit_code)
{
    m_exit_code = exit_code;

    m_scrape.EndStage(m_stages[m_current_stage].working_directory);

    if (exit_code != 0)
    {
        m_stage_states[m_current_stage] = kRPVProfilerStateFailed;
        m_failing_stage                 = static_cast<int32_t>(m_current_stage);
        m_scrape.SkipRemainingFrom(m_current_stage + 1);
        m_state = kRPVProfilerStateFailed;
        spdlog::error("Profiler stage {} exited with code {}", m_current_stage, exit_code);
        return;
    }

    m_stage_states[m_current_stage] = kRPVProfilerStateCompleted;
    RelocateArtifactLocked(m_current_stage);

    if (m_current_stage + 1 >= m_stages.size())
    {
        m_state = kRPVProfilerStateCompleted;
        spdlog::info("Profiler completed successfully");
        return;
    }

    if (StartStageLocked(m_current_stage + 1) != kRocProfVisResultSuccess)
    {
        m_state = kRPVProfilerStateFailed;
    }
}

void ProfilerProcessController::UpdateState()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_state != kRPVProfilerStateRunning)
    {
        return;
    }

    /*
     * Cancel is mid-flight, with m_mutex released while it kills the child. It
     * ends the stage itself once the child is gone, so finalising here as well
     * would settle the same stage twice and race the verdict.
     *
     * This is also what keeps a stage boundary from starting the next stage
     * after the user has asked to stop: advancing only ever happens below, and
     * Cancel can only set this flag while holding m_mutex, so a boundary is
     * either committed before the cancel or abandoned by it, never both.
     */
    if (m_cancel_requested)
    {
        return;
    }

    if (!m_executor || m_executor->IsRunning())
    {
        return;
    }

    // Everything the stage printed before exiting has to reach the engine
    // before EndStage decides what went unmatched.
    DrainExecutorLocked();
    FinishStageLocked(m_executor->GetExitCode());
}

uint32_t ProfilerProcessController::GetStageCount() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return static_cast<uint32_t>(m_stages.size());
}

rocprofvis_result_t ProfilerProcessController::GetStageState(
    uint32_t stage_index, rocprofvis_profiler_state_t& out) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (stage_index >= m_stage_states.size())
    {
        return kRocProfVisResultInvalidArgument;
    }
    out = m_stage_states[stage_index];
    return kRocProfVisResultSuccess;
}

int32_t ProfilerProcessController::GetFailingStage() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_failing_stage;
}

rocprofvis_result_t ProfilerProcessController::ScrapedValueLocked(std::string const& key,
                                                                 std::string&       out) const
{
    rocprofvis_profiler_scrape_status_t status = kRPVProfilerScrapePending;

    rocprofvis_result_t declared = m_scrape.GetStatus(key, status);
    if (declared != kRocProfVisResultSuccess)
    {
        return declared;
    }

    switch (status)
    {
        case kRPVProfilerScrapeResolved:
            return m_scrape.GetValue(key, out);
        case kRPVProfilerScrapePending:
            return kRocProfVisResultPending;
        default:
            return kRocProfVisResultNotAvailable;
    }
}

rocprofvis_result_t ProfilerProcessController::GetArtifactPath(std::string& out) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_artifact_key.empty())
    {
        return kRocProfVisResultNotAvailable;
    }
    return ScrapedValueLocked(m_artifact_key, out);
}

rocprofvis_result_t ProfilerProcessController::GetScrapedValue(std::string const& key,
                                                              std::string&       out) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return ScrapedValueLocked(key, out);
}

rocprofvis_result_t ProfilerProcessController::GetScrapeStatus(
    std::string const& key, rocprofvis_profiler_scrape_status_t& out) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_scrape.GetStatus(key, out);
}

rocprofvis_result_t ProfilerProcessController::ExecuteJob(ProfilerProcessController* controller, Future* future)
{
    if (controller == nullptr)
    {
        spdlog::error("ProfilerProcessController::ExecuteJob: controller is null");
        return kRocProfVisResultInvalidArgument;
    }

    spdlog::info("Profiler monitor job started");

    while (controller->m_state == kRPVProfilerStateRunning)
    {
        if (future && future->IsCancelled())
        {
            spdlog::info("Profiler job cancelled by user");
            controller->Cancel();
            break;
        }

        controller->GetOutput();
        controller->UpdateState();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Do not resolve the future until the executor worker has stopped. Cancel()
    // only signals it; teardown keys on the future. m_executor is the last
    // stage's by now, which is the one that has to be observed.
    while (controller->ExecutorRunning())
    {
        controller->GetOutput();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    controller->GetOutput();
    rocprofvis_profiler_state_t final_state = controller->m_state.load();
    spdlog::info("Profiler monitor job finished (state={})", static_cast<int>(final_state));

    switch (final_state)
    {
        case kRPVProfilerStateCompleted:
            return kRocProfVisResultSuccess;
        case kRPVProfilerStateCancelled:
            return kRocProfVisResultCancelled;
        default:
            return kRocProfVisResultUnknownError;
    }
}

// ==================================================================================
// ProfilerSession Implementation
// ==================================================================================

ProfilerSession::ProfilerSession()
    : Handle(0, 0)
    , m_controller()
    , m_bound_future(nullptr)
{
}

ProfilerSession::~ProfilerSession()
{
    // Controller dtor joins the monitor job. The bound Future is caller-owned.
}

rocprofvis_controller_object_type_t ProfilerSession::GetType(void)
{
    return kRPVProfiler;
}

} // namespace Controller
} // namespace RocProfVis
