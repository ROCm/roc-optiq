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
#include <condition_variable>
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
#ifdef ROCPROFVIS_ENABLE_REMOTE
class SshConnection;  // TEMPORARY (remote/SSH)
#endif

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
 * Launch config. Handle so it participates in Reference<> validation.
 */
class ProfilerConfig : public Handle
{
public:
    ProfilerConfig();
    ~ProfilerConfig() override;

    rocprofvis_controller_object_type_t GetType(void) final;

    rocprofvis_result_t SetTool(rocprofvis_profiler_tool_t tool);
    rocprofvis_result_t SetToolDirectory(char const* directory);
    rocprofvis_result_t SetOutputDirectory(char const* path);
    rocprofvis_result_t SetWorkingDirectory(char const* path);
    rocprofvis_result_t AddEnvVar(char const* name, char const* value);
    rocprofvis_result_t AddProfilerArg(char const* arg);
    rocprofvis_result_t SetConnectionLocal();
    rocprofvis_result_t SetConnectionSsh(char const* host, char const* user,
                                         int port, char const* identity_file,
                                         char const* remote_stage_dir);

    rocprofvis_profiler_tool_t GetTool() const { return m_tool; }
    std::string const& GetToolDirectory() const { return m_tool_directory; }

    // Local absolute path for argv[0]. Must succeed before exec.
    rocprofvis_result_t ResolveToolPath();

    // Remote argv[0]: "<tool_directory>/<name>" (POSIX join) or the bare name.
    // A missing remote tool is still the shell's "command not found".
    rocprofvis_result_t ResolveToolPathRemote();

    // Local-only: the directory exists, so a bad cwd is a launch error, not
    // child exit 126. Remote cwd cannot be checked from here.
    rocprofvis_result_t ValidateWorkingDirectory() const;

    // argv[0]; empty until ResolveToolPath or ResolveToolPathRemote succeeds.
    std::string const& GetResolvedToolPath() const { return m_resolved_tool_path; }

    std::string const& GetOutputDirectory() const { return m_output_directory; }
    std::string const& GetWorkingDirectory() const { return m_working_directory; }

    std::vector<std::pair<std::string, std::string>> const& GetEnvVars() const { return m_env_vars; }
    std::vector<std::string> const& GetProfilerArgv() const { return m_profiler_argv; }

    ConnectionType GetConnectionType() const { return m_connection_type; }
    SshConnectionInfo const& GetSshInfo() const { return m_ssh_info; }

private:
    // Named tool; filename comes from the tool table, never from a caller path.
    rocprofvis_profiler_tool_t m_tool;
    std::string m_tool_directory;      // search dir only; empty = default search
    std::string m_resolved_tool_path;  // argv[0] after resolve
    // Not on argv (see BuildArgv). Unread for now; kept for a later per-stage
    // artifact destination. See rocprofvis_profiler_config_set_output_directory.
    std::string m_output_directory;
    // Child cwd only (chdir after fork / lpCurrentDirectory / remote "cd").
    std::string m_working_directory;

    std::vector<std::pair<std::string, std::string>> m_env_vars;
    std::vector<std::string> m_profiler_argv;

    ConnectionType    m_connection_type;
    SshConnectionInfo m_ssh_info;
};

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

class ProfilerProcessController
{
public:
    ProfilerProcessController();
    ~ProfilerProcessController();

    rocprofvis_result_t LaunchAsync(ProfilerConfig const* config);

#ifdef ROCPROFVIS_ENABLE_REMOTE
    // TEMPORARY (remote/SSH): borrowed connected SshConnection; `future` is
    // observed for cancel and may be null. Caller owns both lifetimes.
    rocprofvis_result_t LaunchAsyncRemote(ProfilerConfig const* config,
                                          SshConnection*        connection,
                                          Future*               future);
#endif

    rocprofvis_profiler_state_t GetState() const;

    std::string GetOutput();

    void ClearOutput();

    int GetExitCode() const;

    rocprofvis_result_t Cancel();

    // C ABI calls this just before issuing the monitor job (raw `this` in flight).
    void BeginMonitorJob();
    // Fired when the Job is destroyed (~Future), even if ExecuteJob never ran.
    // Unblocks the destructor. Idempotent.
    void EndMonitorJob();

    static rocprofvis_result_t ExecuteJob(ProfilerProcessController* controller, Future* future);

private:
    void UpdateOutput();
    void UpdateState();

    std::unique_ptr<IProfilerExecutor> m_executor;
    std::unique_ptr<ProfilerConfig> m_config;
    std::atomic<rocprofvis_profiler_state_t> m_state;
    std::string m_output_text;
    int m_exit_code;
    std::mutex m_mutex;

    // Destructor waits on m_job_cv until the monitor Job is destroyed. Free the
    // bound future before (or with) the profiler; the View already does.
    std::mutex              m_job_mutex;
    std::condition_variable m_job_cv;
    bool                    m_job_active = false;
};

/*
 * C-ABI handle (rocprofvis_profiler_t): owns the controller and a non-owning
 * pointer to the Future bound at launch. Status queries go through the session;
 * Cancel forwards to both controller and future.
 */
class ProfilerSession : public Handle
{
public:
    ProfilerSession();
    ~ProfilerSession() override;

    rocprofvis_controller_object_type_t GetType(void) final;

    ProfilerProcessController& GetController() { return m_controller; }
    ProfilerProcessController const& GetController() const { return m_controller; }

    // Non-owning; caller frees via rocprofvis_controller_future_free.
    void SetBoundFuture(Future* future) { m_bound_future = future; }
    Future* GetBoundFuture() const { return m_bound_future; }

private:
    ProfilerProcessController m_controller;
    Future*                   m_bound_future;
};

} // namespace Controller
} // namespace RocProfVis
