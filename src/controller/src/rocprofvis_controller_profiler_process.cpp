// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_controller_profiler_process.h"
#include "rocprofvis_controller_future.h"
#include "rocprofvis_controller_job_system.h"
#include "spdlog/spdlog.h"
#include <sstream>
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

// ==================================================================================
// LocalProcessExecutor Implementation - Windows
// ==================================================================================

#ifdef _WIN32

LocalProcessExecutor::LocalProcessExecutor()
    : m_process_handle(nullptr)
    , m_stdout_read_handle(nullptr)
    , m_stdout_write_handle(nullptr)
    , m_stderr_read_handle(nullptr)
    , m_stderr_write_handle(nullptr)
    , m_is_running(false)
    , m_exit_code(0)
{
}

LocalProcessExecutor::~LocalProcessExecutor()
{
    if (IsRunning())
    {
        Terminate();
    }
    CloseHandles();
}

void LocalProcessExecutor::CloseHandles()
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

bool LocalProcessExecutor::Launch(std::string const& executable_path,
                                   std::vector<std::string> const& args,
                                   std::string const& working_directory)
{
    // Build command line
    std::ostringstream cmd_line;
    cmd_line << "\"" << executable_path << "\"";
    for (auto const& arg : args)
    {
        cmd_line << " " << arg;
    }

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

    // Setup process startup info
    STARTUPINFOA si = {};
    si.cb = sizeof(STARTUPINFOA);
    si.hStdOutput = m_stdout_write_handle;
    si.hStdError = m_stderr_write_handle;
    si.dwFlags |= STARTF_USESTDHANDLES;

    PROCESS_INFORMATION pi = {};

    // Launch process
    std::string cmd_line_str = cmd_line.str();
    char* cmd_line_buffer = new char[cmd_line_str.size() + 1];
    strcpy_s(cmd_line_buffer, cmd_line_str.size() + 1, cmd_line_str.c_str());

    char const* working_dir = working_directory.empty() ? nullptr : working_directory.c_str();

    BOOL success = CreateProcessA(
        nullptr,
        cmd_line_buffer,
        nullptr,
        nullptr,
        TRUE,
        0,
        nullptr,
        working_dir,
        &si,
        &pi);

    delete[] cmd_line_buffer;

    if (!success)
    {
        CloseHandles();
        return false;
    }

    m_process_handle = pi.hProcess;
    CloseHandle(pi.hThread);

    // Close write ends of pipes (child process owns them now)
    CloseHandle(m_stdout_write_handle);
    m_stdout_write_handle = nullptr;
    CloseHandle(m_stderr_write_handle);
    m_stderr_write_handle = nullptr;

    m_is_running = true;
    return true;
}

bool LocalProcessExecutor::IsRunning()
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

bool LocalProcessExecutor::Wait(uint32_t timeout_ms)
{
    if (m_process_handle == nullptr)
    {
        return false;
    }

    DWORD timeout = (timeout_ms == 0) ? INFINITE : timeout_ms;
    DWORD result = WaitForSingleObject(m_process_handle, timeout);

    if (result == WAIT_OBJECT_0)
    {
        DWORD exit_code = 0;
        if (GetExitCodeProcess(m_process_handle, &exit_code))
        {
            m_exit_code = static_cast<int>(exit_code);
        }
        m_is_running = false;
        return true;
    }

    return false;
}

bool LocalProcessExecutor::Terminate()
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

int LocalProcessExecutor::GetExitCode() const
{
    return m_exit_code;
}

std::string LocalProcessExecutor::ReadOutput()
{
    std::lock_guard<std::mutex> lock(m_output_mutex);

    std::string output;
    DWORD bytes_available = 0;
    char buffer[4096];

    // Read from stdout
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

    // Read from stderr
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
// LocalProcessExecutor Implementation - Linux
// ==================================================================================

LocalProcessExecutor::LocalProcessExecutor()
    : m_process_id(-1)
    , m_stdout_fd(-1)
    , m_stderr_fd(-1)
    , m_is_running(false)
    , m_exit_code(0)
{
}

LocalProcessExecutor::~LocalProcessExecutor()
{
    if (IsRunning())
    {
        Terminate();
    }
    CloseHandles();
}

void LocalProcessExecutor::CloseHandles()
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

bool LocalProcessExecutor::Launch(std::string const& executable_path,
                                   std::vector<std::string> const& args,
                                   std::string const& working_directory)
{
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

        // Change working directory if specified
        if (!working_directory.empty())
        {
            if (chdir(working_directory.c_str()) != 0)
            {
                _exit(1);
            }
        }

        // Build argv array
        std::vector<char*> argv;
        argv.push_back(const_cast<char*>(executable_path.c_str()));
        for (auto const& arg : args)
        {
            argv.push_back(const_cast<char*>(arg.c_str()));
        }
        argv.push_back(nullptr);

        execvp(executable_path.c_str(), argv.data());

        // execvp only returns on failure — write the error to stderr so the
        // parent process can capture it in the output pipe.
        int exec_errno = errno;
        char err_buf[256];
        int n = snprintf(err_buf, sizeof(err_buf),
                         "execvp failed for '%s': %s (errno %d)\n",
                         executable_path.c_str(), strerror(exec_errno), exec_errno);
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

    // Set non-blocking mode
    fcntl(m_stdout_fd, F_SETFL, O_NONBLOCK);
    fcntl(m_stderr_fd, F_SETFL, O_NONBLOCK);

    m_is_running = true;
    return true;
}

bool LocalProcessExecutor::IsRunning()
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

bool LocalProcessExecutor::Wait(uint32_t timeout_ms)
{
    if (m_process_id == -1)
    {
        return false;
    }

    auto start_time = std::chrono::steady_clock::now();

    while (true)
    {
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
            return true;
        }

        if (timeout_ms > 0)
        {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start_time).count();
            if (static_cast<uint32_t>(elapsed) >= timeout_ms)
            {
                return false;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

bool LocalProcessExecutor::Terminate()
{
    if (m_process_id == -1)
    {
        return false;
    }

    if (kill(m_process_id, SIGTERM) == 0)
    {
        // Give process time to terminate gracefully
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

int LocalProcessExecutor::GetExitCode() const
{
    return m_exit_code;
}

std::string LocalProcessExecutor::ReadOutput()
{
    std::lock_guard<std::mutex> lock(m_output_mutex);

    std::string output;
    char buffer[4096];

    // Read from stdout
    if (m_stdout_fd != -1)
    {
        ssize_t bytes_read;
        while ((bytes_read = read(m_stdout_fd, buffer, sizeof(buffer) - 1)) > 0)
        {
            buffer[bytes_read] = '\0';
            output += buffer;
        }
    }

    // Read from stderr
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
    m_executor = std::make_unique<LocalProcessExecutor>();

    std::vector<std::string> args = BuildCommandArgs(config);

    // Log the full command line for diagnostics
    {
        std::ostringstream cmd_log;
        cmd_log << config->GetProfilerPath();
        for (auto const& arg : args)
        {
            cmd_log << " " << arg;
        }
        spdlog::info("Profiler launch: executable='{}' (inherits application working directory)",
                     config->GetProfilerPath());
        spdlog::info("Profiler command line: {}", cmd_log.str());
    }

    bool launched = m_executor->Launch(
        config->GetProfilerPath(),
        args,
        std::string());

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

    if (m_executor && m_executor->Terminate())
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
        switch (config->GetProfilerType())
        {
            case kRPVProfilerTypeRocprofSysRun:
            case kRPVProfilerTypeRocprofSysInstrument:
                return (ext == ".db" || ext == ".rpd");
            case kRPVProfilerTypeRocprofCompute:
            case kRPVProfilerTypeRocprofV3:
                return (ext == ".csv" || ext == ".json" || ext == ".db");
            default:
                return (ext == ".db" || ext == ".rpd");
        }
    };

    std::filesystem::path best_path;
    std::filesystem::file_time_type best_time{};

    for (auto const& entry : std::filesystem::directory_iterator(output_path))
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

std::vector<std::string> ProfilerProcessController::BuildCommandArgs(ProfilerConfig const* config)
{
    std::vector<std::string> args;

    if (config == nullptr)
    {
        return args;
    }

    if (!config->GetProfilerArgs().empty())
    {
        std::istringstream iss(config->GetProfilerArgs());
        std::string token;
        while (iss >> token)
        {
            args.push_back(token);
        }
    }

    if (!config->GetOutputDirectory().empty())
    {
        args.push_back("--output");
        args.push_back(config->GetOutputDirectory());
    }

    // All ROCm profilers use "--" to separate profiler flags from the target
    if (!config->GetTargetExecutable().empty())
    {
        args.push_back("--");
        args.push_back(config->GetTargetExecutable());
    }

    if (!config->GetTargetArgs().empty())
    {
        std::istringstream iss(config->GetTargetArgs());
        std::string token;
        while (iss >> token)
        {
            args.push_back(token);
        }
    }

    return args;
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
