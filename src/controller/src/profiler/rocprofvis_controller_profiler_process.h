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
 * ProfilerConfig - Configuration for launching a profiler
 * Derives from Handle so it participates in the Reference<> validation pattern.
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

    // Resolves m_tool (in m_tool_directory if set) to an absolute executable path
    // on this machine and caches it for GetResolvedToolPath. Must succeed before a
    // launch: argv[0] comes from the cached result, so an unresolved tool must
    // never reach exec.
    rocprofvis_result_t ResolveToolPath();

    // Remote equivalent. The tool lives on the remote host, so its filesystem
    // cannot be searched from here without an extra round trip. argv[0] becomes
    // <m_tool_directory>/<name> when a directory is configured (joined with '/',
    // since the remote is addressed as POSIX regardless of what this host is), and
    // otherwise the bare name for the remote $PATH to resolve. Consequently a
    // missing remote tool still surfaces as the remote shell's "command not
    // found" rather than kRocProfVisResultToolNotFound.
    rocprofvis_result_t ResolveToolPathRemote();

    // argv[0]. Empty until one of the two resolve calls above has succeeded -
    // hence "resolved" in the name, since reading it earlier yields nothing.
    std::string const& GetResolvedToolPath() const { return m_resolved_tool_path; }

    std::string const& GetOutputDirectory() const { return m_output_directory; }
    std::string const& GetWorkingDirectory() const { return m_working_directory; }

    std::vector<std::pair<std::string, std::string>> const& GetEnvVars() const { return m_env_vars; }
    std::vector<std::string> const& GetProfilerArgv() const { return m_profiler_argv; }

    ConnectionType GetConnectionType() const { return m_connection_type; }
    SshConnectionInfo const& GetSshInfo() const { return m_ssh_info; }

private:
    // Which binary to run, named rather than pathed so that no caller-supplied
    // string becomes argv[0]. m_tool_directory narrows *where* to look for it
    // (for a ROCm install in a non-standard location) and is set through its own
    // setter, never as a side effect of naming a tool; the filename still comes
    // from the tool table, so a directory cannot name a different program.
    rocprofvis_profiler_tool_t m_tool;
    std::string m_tool_directory;
    std::string m_resolved_tool_path;
    // Where output is expected. Does not contribute to argv (see
    // Cmdline::BuildArgv) - a caller that wants it on the command line adds it
    // as an explicit argv entry - and has no reader yet; see
    // rocprofvis_profiler_config_set_output_directory for why it is kept.
    std::string m_output_directory;
    // Directory the child process runs in. Applied in the child only (chdir
    // after fork / lpCurrentDirectory / a remote "cd" prefix) - never by
    // chdir()ing this process, whose cwd is global shared state.
    std::string m_working_directory;

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

#ifdef ROCPROFVIS_ENABLE_REMOTE
    // TEMPORARY (remote/SSH): remote variant runs the profiler over the supplied
    // (already connected and authenticated) SSH connection via an
    // SshProfilerExecutor. The connection is borrowed; the caller
    // (View/SshSession) owns its lifetime. `future` is the bound profiler
    // future, observed by the remote exec loop for cancellation; it is borrowed
    // and may be null.
    rocprofvis_result_t LaunchAsyncRemote(ProfilerConfig const* config,
                                          SshConnection*        connection,
                                          Future*               future);
#endif

    rocprofvis_profiler_state_t GetState() const;

    std::string GetOutput();

    void ClearOutput();

    int GetExitCode() const;

    rocprofvis_result_t Cancel();

    // Called by the C ABI immediately BEFORE the monitor job is issued, so the
    // destructor knows a JobSystem job holding a raw pointer to this controller
    // (and its executor) is in flight.
    void BeginMonitorJob();
    // Called via a shared_ptr scope-guard captured in the job lambda, when the
    // Job object is destroyed (in ~Future at future_free). This fires whether or
    // not ExecuteJob ever ran - a job cancelled before it is dequeued never runs
    // its function - so it reliably releases a destructor blocked below. Safe to
    // call more than once (idempotent).
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

    // Guards the "a monitor job references this controller" flag. The destructor
    // cancels the run and blocks on m_job_cv until the owning Job object is
    // destroyed (at future_free), so the controller/executor are never destroyed
    // out from under ExecuteJob. Contract: the bound future must be freed before
    // (or concurrently with) the profiler; the view teardown frees the future
    // first, so the wait returns without blocking.
    std::mutex              m_job_mutex;
    std::condition_variable m_job_cv;
    bool                    m_job_active = false;
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
