// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_controller.h"
#include "rocprofvis_controller_profiler_executor.h"
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>
#include <utility>

#ifdef _WIN32
#include <windows.h>
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

enum class ConnectionType
{
    kLocal,
    kSsh
};

struct SshConnectionInfo
{
    std::string host;
    std::string user;
    int         port = 22;
    std::string identity_file;
    std::string remote_stage_dir;
};

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
    rocprofvis_result_t AddEnvVar(char const* name, char const* value);
    rocprofvis_result_t AddProfilerArg(char const* arg);
    rocprofvis_result_t SetConnectionLocal();
    rocprofvis_result_t SetConnectionSsh(char const* host, char const* user,
                                         int port, char const* identity_file,
                                         char const* remote_stage_dir);

    rocprofvis_profiler_type_t GetProfilerType() const { return m_profiler_type; }
    std::string const& GetProfilerPath() const { return m_profiler_path; }
    std::string const& GetTargetExecutable() const { return m_target_executable; }
    std::string const& GetTargetArgs() const { return m_target_args; }
    std::string const& GetProfilerArgs() const { return m_profiler_args; }
    std::string const& GetOutputDirectory() const { return m_output_directory; }

    std::vector<std::pair<std::string, std::string>> const& GetEnvVars() const { return m_env_vars; }
    std::vector<std::string> const& GetProfilerArgv() const { return m_profiler_argv; }

    ConnectionType GetConnectionType() const { return m_connection_type; }
    SshConnectionInfo const& GetSshInfo() const { return m_ssh_info; }

private:
    rocprofvis_profiler_type_t m_profiler_type;
    std::string m_profiler_path;
    std::string m_target_executable;
    std::string m_target_args;
    std::string m_profiler_args;
    std::string m_output_directory;

    std::vector<std::pair<std::string, std::string>> m_env_vars;
    std::vector<std::string> m_profiler_argv;

    ConnectionType    m_connection_type;
    SshConnectionInfo m_ssh_info;
};

/*
 * LocalProfilerExecutor - Platform-specific local process execution
 * Implements IProfilerExecutor for launching profiler processes on the local machine.
 */
class LocalProfilerExecutor : public IProfilerExecutor
{
public:
    LocalProfilerExecutor();
    ~LocalProfilerExecutor() override;

    bool Start(const ProfilerConfig& config) override;
    bool IsRunning() override;
    std::string ReadOutput() override;
    int GetExitCode() const override;
    bool Cancel() override;

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

    rocprofvis_result_t LaunchAsync(ProfilerConfig const* config);

    rocprofvis_profiler_state_t GetState() const;

    std::string GetOutput();

    std::string GetTracePath() const;

    int GetExitCode() const;

    rocprofvis_result_t Cancel();

    static void ExecuteJob(ProfilerProcessController* controller, Future* future);

private:
    void UpdateOutput();
    void UpdateState();
    std::string DetermineTracePath(ProfilerConfig const* config);
    std::vector<std::string> BuildCommandArgs(ProfilerConfig const* config);

    std::unique_ptr<IProfilerExecutor> m_executor;
    std::unique_ptr<ProfilerConfig> m_config;
    std::atomic<rocprofvis_profiler_state_t> m_state;
    std::string m_output_text;
    std::string m_trace_path;
    int m_exit_code;
    std::mutex m_mutex;
};

} // namespace Controller
} // namespace RocProfVis
