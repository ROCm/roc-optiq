// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_controller.h"
#include "rocprofvis_controller_handle.h"
#include "rocprofvis_controller_profiler_executor.h"
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>
#include <thread>
#include <utility>

#ifdef _WIN32
#define NOMINMAX
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
class SshConnection;

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
 * Derives from Handle so it participates in the Reference<> validation pattern.
 */
class ProfilerConfig : public Handle
{
public:
    ProfilerConfig();
    ~ProfilerConfig() override;

    rocprofvis_controller_object_type_t GetType(void) final;

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

    // Remote variant: runs the profiler over the supplied (already connected and
    // authenticated) SSH connection via an SshProfilerExecutor. The connection
    // is borrowed; the caller (View/SshSession) owns its lifetime. `future` is
    // the bound profiler future, observed by the remote exec loop for
    // cancellation; it is borrowed and may be null.
    rocprofvis_result_t LaunchAsyncRemote(ProfilerConfig const* config,
                                          SshConnection*        connection,
                                          Future*               future);

    rocprofvis_profiler_state_t GetState() const;

    std::string GetOutput();

    void ClearOutput();

    std::string GetTracePath() const;

    int GetExitCode() const;

    rocprofvis_result_t Cancel();

    static void ExecuteJob(ProfilerProcessController* controller, Future* future);

private:
    void UpdateOutput();
    void UpdateState();
    std::string DetermineTracePath(ProfilerConfig const* config);

    std::unique_ptr<IProfilerExecutor> m_executor;
    std::unique_ptr<ProfilerConfig> m_config;
    std::atomic<rocprofvis_profiler_state_t> m_state;
    std::string m_output_text;
    std::string m_trace_path;
    int m_exit_code;
    std::mutex m_mutex;
};

/*
 * ProfilerSession - C-ABI handle that owns one ProfilerProcessController plus
 * a weak reference to the Future bound at launch time.
 *
 * The session is what rocprofvis_profiler_t maps to. It exists for the
 * lifetime the caller wants to query profiler state, independent of the
 * Future lifetime: status queries (get_state/output/trace_path/exit_code)
 * are routed through the session, not the future. Cancelling the session
 * forwards to both the controller and the bound future.
 */
class ProfilerSession : public Handle
{
public:
    ProfilerSession();
    ~ProfilerSession() override;

    rocprofvis_controller_object_type_t GetType(void) final;

    ProfilerProcessController& GetController() { return m_controller; }
    ProfilerProcessController const& GetController() const { return m_controller; }

    // Bound at launch_async; non-owning. The caller frees the future via
    // rocprofvis_controller_future_free independent of this session.
    void SetBoundFuture(Future* future) { m_bound_future = future; }
    Future* GetBoundFuture() const { return m_bound_future; }

private:
    ProfilerProcessController m_controller;
    Future*                   m_bound_future;
};

} // namespace Controller
} // namespace RocProfVis
