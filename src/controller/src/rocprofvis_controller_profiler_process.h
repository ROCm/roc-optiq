// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_controller.h"
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>

#ifdef _WIN32
#include <windows.h>
// Avoid colliding with Handle::GetObject and derived overrides in controller headers.
#ifdef GetObject
#undef GetObject
#endif
#else
#include <unistd.h>
#endif

namespace RocProfVis
{
namespace Controller
{

class Future;

/*
 * ProfilerConfig - Configuration for launching a profiler
 * Simple data container, not part of Handle/property system
 */
class ProfilerConfig
{
public:
    ProfilerConfig();
    ~ProfilerConfig();

    rocprofvis_result_t SetProfilerType(rocprofvis_profiler_type_t type);
    rocprofvis_result_t SetProfilerPath(char const* path);
    rocprofvis_result_t SetTargetExecutable(char const* path);
    rocprofvis_result_t SetTargetArgs(char const* args);
    rocprofvis_result_t SetProfilerArgs(char const* args);
    rocprofvis_result_t SetOutputDirectory(char const* path);

    rocprofvis_profiler_type_t GetProfilerType() const { return m_profiler_type; }
    std::string const& GetProfilerPath() const { return m_profiler_path; }
    std::string const& GetTargetExecutable() const { return m_target_executable; }
    std::string const& GetTargetArgs() const { return m_target_args; }
    std::string const& GetProfilerArgs() const { return m_profiler_args; }
    std::string const& GetOutputDirectory() const { return m_output_directory; }

private:
    rocprofvis_profiler_type_t m_profiler_type;
    std::string m_profiler_path;
    std::string m_target_executable;
    std::string m_target_args;
    std::string m_profiler_args;
    std::string m_output_directory;
};

/*
 * LocalProcessExecutor - Platform-specific process execution
 */
class LocalProcessExecutor
{
public:
    LocalProcessExecutor();
    ~LocalProcessExecutor();

    // Launches the process with stdout/stderr captured
    bool Launch(std::string const& executable_path,
                std::vector<std::string> const& args,
                std::string const& working_directory);

    // Checks if process is still running
    bool IsRunning();

    // Waits for process to complete (with timeout in milliseconds, 0 = no timeout)
    bool Wait(uint32_t timeout_ms = 0);

    // Terminates the process
    bool Terminate();

    // Gets the process exit code (only valid after process has completed)
    int GetExitCode() const;

    // Reads available stdout/stderr output (non-blocking)
    std::string ReadOutput();

private:
    void CloseHandles();

#ifdef _WIN32
    HANDLE m_process_handle;
    HANDLE m_stdout_read_handle;
    HANDLE m_stdout_write_handle;
    HANDLE m_stderr_read_handle;
    HANDLE m_stderr_write_handle;
#else
    pid_t m_process_id;
    int m_stdout_fd;
    int m_stderr_fd;
#endif

    std::atomic<bool> m_is_running;
    int m_exit_code;
    std::mutex m_output_mutex;
    std::string m_output_buffer;
};

/*
 * ProfilerProcessController - Manages profiler execution lifecycle
 */
class ProfilerProcessController
{
public:
    ProfilerProcessController();
    ~ProfilerProcessController();

    // Launches profiler asynchronously
    rocprofvis_result_t LaunchAsync(ProfilerConfig const* config);

    // Gets current profiler state
    rocprofvis_profiler_state_t GetState() const;

    // Gets accumulated output
    std::string GetOutput();

    // Gets the path to the generated trace file (only valid when state = Completed)
    std::string GetTracePath() const;

    // Gets the process exit code (only meaningful after state != Running)
    int GetExitCode() const;

    // Cancels the running profiler
    rocprofvis_result_t Cancel();

    // Job execution function (called by JobSystem worker thread)
    static void ExecuteJob(ProfilerProcessController* controller, Future* future);

private:
    void UpdateOutput();
    void UpdateState();
    std::string DetermineTracePath(ProfilerConfig const* config);
    std::vector<std::string> BuildCommandArgs(ProfilerConfig const* config);

    std::unique_ptr<LocalProcessExecutor> m_executor;
    std::unique_ptr<ProfilerConfig> m_config;
    std::atomic<rocprofvis_profiler_state_t> m_state;
    std::string m_output_text;
    std::string m_trace_path;
    int m_exit_code;
    std::mutex m_mutex;
};

} // namespace Controller
} // namespace RocProfVis
