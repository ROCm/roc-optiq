// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_controller_profiler_process.h"
#include "rocprofvis_controller_profiler_cmdline.h"
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

std::string LocalProfilerExecutor::ReadOutput()
{
    std::lock_guard<std::mutex> lock(m_output_mutex);

    std::string output;
    DWORD bytes_available = 0;
    char buffer[4096];

    if (m_stdout_read_handle)
    {
        if (PeekNamedPipe(m_stdout_read_handle, nullptr, 0, nullptr, &bytes_available, nullptr))
        {
            if (bytes_available > 0)
            {
                DWORD bytes_read = 0;
                if (ReadFile(m_stdout_read_handle, buffer, sizeof(buffer) - 1, &bytes_read, nullptr))
                {
                    buffer[bytes_read] = '\0';
                    output += buffer;
                }
            }
        }
    }

    if (m_stderr_read_handle)
    {
        bytes_available = 0;
        if (PeekNamedPipe(m_stderr_read_handle, nullptr, 0, nullptr, &bytes_available, nullptr))
        {
            if (bytes_available > 0)
            {
                DWORD bytes_read = 0;
                if (ReadFile(m_stderr_read_handle, buffer, sizeof(buffer) - 1, &bytes_read, nullptr))
                {
                    buffer[bytes_read] = '\0';
                    output += buffer;
                }
            }
        }
    }

    m_output_buffer += output;
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
        while ((bytes_read = read(m_stdout_fd, buffer, sizeof(buffer) - 1)) > 0)
        {
            buffer[bytes_read] = '\0';
            output += buffer;
        }
    }

    if (m_stderr_fd != -1)
    {
        ssize_t bytes_read;
        while ((bytes_read = read(m_stderr_fd, buffer, sizeof(buffer) - 1)) > 0)
        {
            buffer[bytes_read] = '\0';
            output += buffer;
        }
    }

    m_output_buffer += output;
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

    // Resolve before spawn so a missing tool is ToolNotFound, not child exit 127.
    rocprofvis_result_t resolved = m_config->ResolveToolPath();
    if (resolved != kRocProfVisResultSuccess)
    {
        m_state = kRPVProfilerStateFailed;
        return resolved;
    }

    // Same for cwd: fail here rather than as child exit 126 after chdir.
    rocprofvis_result_t working_directory_valid = m_config->ValidateWorkingDirectory();
    if (working_directory_valid != kRocProfVisResultSuccess)
    {
        m_state = kRPVProfilerStateFailed;
        return working_directory_valid;
    }

    m_executor = std::make_unique<LocalProfilerExecutor>();

    for (auto const& kv : m_config->GetEnvVars())
    {
        spdlog::info("Profiler env: {}={}", kv.first, kv.second);
    }

    if (!m_config->GetWorkingDirectory().empty())
    {
        spdlog::info("Profiler working directory: {}", m_config->GetWorkingDirectory());
    }

    spdlog::info("Profiler launch: {}", Cmdline::ToDisplayString(Cmdline::BuildArgv(*m_config)));

    bool launched = m_executor->Start(*m_config);

    if (!launched)
    {
        spdlog::error("ProfilerProcessController::LaunchAsync: failed to start process "
                      "(executable='{}')", m_config->GetResolvedToolPath());
        m_state = kRPVProfilerStateFailed;
        m_executor.reset();
        return kRocProfVisResultUnknownError;
    }

    spdlog::info("Profiler process launched successfully");
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

    // Worker reads `future` after Start; ABI binds the job immediately after this.
    m_executor = std::make_unique<SshProfilerExecutor>(connection, future);

    bool launched = m_executor->Start(*m_config);
    if (!launched)
    {
        spdlog::error("ProfilerProcessController::LaunchAsyncRemote: failed to start remote profiler");
        m_state = kRPVProfilerStateFailed;
        m_executor.reset();
        return kRocProfVisResultUnknownError;
    }

    spdlog::info("Remote profiler launched successfully");
    m_state = kRPVProfilerStateRunning;
    return kRocProfVisResultSuccess;
}
#endif  // ROCPROFVIS_ENABLE_REMOTE

rocprofvis_profiler_state_t ProfilerProcessController::GetState() const
{
    return m_state;
}

std::string ProfilerProcessController::GetOutput()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_executor)
    {
        std::string new_output = m_executor->ReadOutput();
        if (!new_output.empty())
        {
            m_output_text += new_output;
        }
    }

    return m_output_text;
}

void ProfilerProcessController::ClearOutput()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_output_text.clear();
}

int ProfilerProcessController::GetExitCode() const
{
    return m_exit_code;
}

rocprofvis_result_t ProfilerProcessController::Cancel()
{
    if (m_state != kRPVProfilerStateRunning)
    {
        return kRocProfVisResultNotSupported;
    }

    if (m_executor && m_executor->Cancel())
    {
        m_state = kRPVProfilerStateCancelled;
        return kRocProfVisResultSuccess;
    }

    return kRocProfVisResultUnknownError;
}

void ProfilerProcessController::UpdateState()
{
    if (m_state != kRPVProfilerStateRunning)
    {
        return;
    }

    if (m_executor && !m_executor->IsRunning())
    {
        int exit_code = m_executor->GetExitCode();
        m_exit_code = exit_code;

        if (exit_code == 0)
        {
            m_state = kRPVProfilerStateCompleted;
            spdlog::info("Profiler completed successfully");
        }
        else
        {
            m_state = kRPVProfilerStateFailed;
            spdlog::error("Profiler process exited with code {}", exit_code);
        }
    }
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
    // only signals it; teardown keys on the future.
    while (controller->m_executor && controller->m_executor->IsRunning())
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
