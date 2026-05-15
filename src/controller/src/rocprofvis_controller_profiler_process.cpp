// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_controller_profiler_process.h"
#include "rocprofvis_controller_profiler_cmdline.h"
#include "rocprofvis_controller_future.h"
#include "rocprofvis_controller_job_system.h"
#include "spdlog/spdlog.h"
#include <filesystem>
#include <chrono>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <cstring>
#endif

namespace RocProfVis
{
namespace Controller
{

// ==================================================================================
// ProfilerConfig Implementation
// ==================================================================================

ProfilerConfig::ProfilerConfig()
    : m_profiler_type(kRPVProfilerTypeRocprofSysRun)
    , m_profiler_path()
    , m_target_executable()
    , m_target_args()
    , m_profiler_args()
    , m_output_directory()
    , m_env_vars()
    , m_profiler_argv()
    , m_connection_type(ConnectionType::kLocal)
    , m_ssh_info()
{
}

ProfilerConfig::~ProfilerConfig()
{
}

rocprofvis_result_t ProfilerConfig::SetProfilerType(rocprofvis_profiler_type_t type)
{
    m_profiler_type = type;
    return kRocProfVisResultSuccess;
}

rocprofvis_result_t ProfilerConfig::SetProfilerPath(char const* path)
{
    if (path == nullptr)
    {
        return kRocProfVisResultInvalidArgument;
    }
    m_profiler_path = path;
    return kRocProfVisResultSuccess;
}

rocprofvis_result_t ProfilerConfig::SetTargetExecutable(char const* path)
{
    if (path == nullptr)
    {
        return kRocProfVisResultInvalidArgument;
    }
    m_target_executable = path;
    return kRocProfVisResultSuccess;
}

rocprofvis_result_t ProfilerConfig::SetTargetArgs(char const* args)
{
    if (args == nullptr)
    {
        return kRocProfVisResultInvalidArgument;
    }
    m_target_args = args;
    return kRocProfVisResultSuccess;
}

rocprofvis_result_t ProfilerConfig::SetProfilerArgs(char const* args)
{
    if (args == nullptr)
    {
        return kRocProfVisResultInvalidArgument;
    }
    m_profiler_args = args;
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

rocprofvis_result_t ProfilerConfig::AddEnvVar(char const* name, char const* value)
{
    if (name == nullptr || value == nullptr)
    {
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

    // Build command line via the shared helper. Tokens are quoted following
    // the Windows CRT / CommandLineToArgvW reverse rules, so paths or
    // arguments containing whitespace or quotes are preserved correctly.
    std::string cmd_line_str = Cmdline::ToWindowsCommandLine(Cmdline::BuildArgv(config));

    // Create pipes for stdout and stderr
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

    // Build environment block if we have custom env vars
    std::string env_block;
    if (!config.GetEnvVars().empty())
    {
        // Inherit current environment and add our vars
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

    BOOL success = CreateProcessA(
        nullptr,
        cmd_line_buffer.data(),
        nullptr,
        nullptr,
        TRUE,
        0,
        env_ptr,
        nullptr,
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
// LocalProfilerExecutor Implementation - POSIX (Linux, and macOS if ROCm gains
// macOS support in the future).
//
// This implementation uses fork() + execvp() with pipes for stdout/stderr capture.
// The pattern is correct and portable POSIX, but if/when macOS support is added
// for the ROCm Systems Profiler, this code should be refactored to use
// posix_spawnp() with posix_spawn_file_actions_* for the fd plumbing. Reasons:
//
//   - On macOS, the window between fork() and execvp() is unsafe with respect
//     to most Apple frameworks (Cocoa, Core Foundation, libdispatch/GCD, Metal,
//     Mach ports). Roc-optiq links these transitively via MoltenVK / GLFW /
//     native file dialog, so the Objective-C runtime can abort the forked
//     child before exec runs (look for OBJC_DISABLE_INITIALIZE_FORK_SAFETY).
//   - The setenv() and std::string allocations in the child branch below are
//     not async-signal-safe; they can deadlock on macOS if another roc-optiq
//     thread held the malloc lock at fork time.
//   - posix_spawn skips the address-space duplication entirely, which matters
//     for a GUI/Vulkan process with many Mach VM regions.
//   - posix_spawn with POSIX_SPAWN_CLOEXEC_DEFAULT / POSIX_SPAWN_SETSIGDEF
//     gives cleaner fd and signal-handler hygiene than fork+exec.
//
// macOS does not provide execvpe(3), so any env-passing refactor on Linux
// (e.g. switching from per-child setenv to passing envp directly) should
// target posix_spawnp instead of execvpe for portability.
// ==================================================================================

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

    // Build argv tokens BEFORE fork() so we don't allocate in the child between
    // fork and execvp. The string data is copy-on-written into the child by
    // fork, and the c_str() pointers we capture in argv_cstr remain valid there.
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
        // Child process
        close(stdout_pipe[0]);
        close(stderr_pipe[0]);

        dup2(stdout_pipe[1], STDOUT_FILENO);
        dup2(stderr_pipe[1], STDERR_FILENO);

        close(stdout_pipe[1]);
        close(stderr_pipe[1]);

        // Set environment variables. setenv() takes the libc env lock, which
        // is not async-signal-safe; see the section banner above for the
        // posix_spawn-based refactor that will eliminate this risk.
        for (auto const& kv : config.GetEnvVars())
        {
            setenv(kv.first.c_str(), kv.second.c_str(), 1);
        }

        execvp(argv_cstr[0], argv_cstr.data());

        int exec_errno = errno;
        char err_buf[256];
        int n = snprintf(err_buf, sizeof(err_buf),
                         "execvp failed for '%s': %s (errno %d)\n",
                         config.GetProfilerPath().c_str(), strerror(exec_errno), exec_errno);
        if (n > 0)
        {
            (void)write(STDERR_FILENO, err_buf, static_cast<size_t>(n));
        }
        _exit(127);
    }

    // Parent process
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

    int status;
    pid_t result = waitpid(m_process_id, &status, WNOHANG);

    if (result == m_process_id)
    {
        if (WIFEXITED(status))
        {
            m_exit_code = WEXITSTATUS(status);
        }
        else if (WIFSIGNALED(status))
        {
            m_exit_code = 128 + WTERMSIG(status);
        }
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

    if (kill(m_process_id, SIGTERM) == 0)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        if (IsRunning())
        {
            kill(m_process_id, SIGKILL);
        }

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
    , m_trace_path()
    , m_exit_code(-1)
{
}

ProfilerProcessController::~ProfilerProcessController()
{
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

    m_executor = std::make_unique<LocalProfilerExecutor>();

    for (auto const& kv : config->GetEnvVars())
    {
        spdlog::info("Profiler env: {}={}", kv.first, kv.second);
    }

    spdlog::info("Profiler launch: {}", Cmdline::ToDisplayString(Cmdline::BuildArgv(*config)));

    bool launched = m_executor->Start(*config);

    if (!launched)
    {
        spdlog::error("ProfilerProcessController::LaunchAsync: failed to start process "
                      "(executable='{}')", config->GetProfilerPath());
        m_state = kRPVProfilerStateFailed;
        m_executor.reset();
        return kRocProfVisResultUnknownError;
    }

    spdlog::info("Profiler process launched successfully");
    m_state = kRPVProfilerStateRunning;
    return kRocProfVisResultSuccess;
}

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

std::string ProfilerProcessController::GetTracePath() const
{
    return m_trace_path;
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
            m_trace_path = DetermineTracePath(m_config.get());
            spdlog::info("Profiler completed successfully, trace_path='{}'", m_trace_path);
        }
        else
        {
            m_state = kRPVProfilerStateFailed;
            spdlog::error("Profiler process exited with code {}", exit_code);
        }
    }
}

std::string ProfilerProcessController::DetermineTracePath(ProfilerConfig const* config)
{
    if (config == nullptr)
    {
        return "";
    }

    std::string output_dir = config->GetOutputDirectory();
    if (output_dir.empty())
    {
        output_dir = std::filesystem::current_path().string();
    }

    std::filesystem::path output_path(output_dir);
    if (!std::filesystem::exists(output_path))
    {
        return "";
    }

    auto is_trace_extension = [&](std::string const& ext) -> bool
    {
        //TODO: review extensions
        switch (config->GetProfilerType())
        {
            case kRPVProfilerTypeRocprofSysRun:
            case kRPVProfilerTypeRocprofSysInstrument:
                return (ext == ".db" || ext == ".rpd");
            case kRPVProfilerTypeRocprofCompute:
            case kRPVProfilerTypeRocprofV3:
                return (ext == ".db");
            default:
                return (ext == ".db" || ext == ".rpd");
        }
    };

    std::filesystem::path best_path;
    std::filesystem::file_time_type best_time{};

    std::error_code ec;
    for (auto const& entry : std::filesystem::recursive_directory_iterator(output_path, ec))
    {
        if (!entry.is_regular_file())
        {
            continue;
        }

        std::string ext = entry.path().extension().string();
        if (!is_trace_extension(ext))
        {
            continue;
        }

        auto write_time = entry.last_write_time();
        if (best_path.empty() || write_time > best_time)
        {
            best_path = entry.path();
            best_time = write_time;
        }
    }

    return best_path.string();
}

void ProfilerProcessController::ExecuteJob(ProfilerProcessController* controller, Future* future)
{
    if (controller == nullptr)
    {
        spdlog::error("ProfilerProcessController::ExecuteJob: controller is null");
        return;
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

    controller->GetOutput();
    spdlog::info("Profiler monitor job finished (state={})", static_cast<int>(controller->m_state.load()));
}

} // namespace Controller
} // namespace RocProfVis
