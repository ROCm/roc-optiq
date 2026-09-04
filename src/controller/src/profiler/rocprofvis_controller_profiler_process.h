// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_controller.h"
#include "rocprofvis_controller_handle.h"
#include "rocprofvis_controller_profiler_executor.h"
#include "rocprofvis_controller_profiler_scrape.h"
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

    // Stages run in the order added. A config with none runs its flat
    // tool/argv/env/cwd as a single stage, which is what every caller does
    // today, so the one-element case stays the existing code path.
    rocprofvis_result_t AddStage(ProfilerStageSpec const& stage);
    // Overrides the key the rule table would otherwise pick for the pipeline.
    rocprofvis_result_t SetArtifactKey(char const* key);

    std::vector<ProfilerStageSpec> const& GetStages() const { return m_stages; }
    std::string const& GetArtifactKey() const { return m_artifact_key; }

    /*
     * Overwrite the per-child fields so this config describes one stage's
     * process, leaving connection settings alone - those belong to the
     * pipeline, not to a stage. `resolved_tool_path` comes from the pipeline's
     * pre-flight resolution so a stage is never resolved twice.
     */
    void ApplyStage(ProfilerStageSpec const& stage, std::string const& resolved_tool_path);

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

    std::vector<ProfilerStageSpec> m_stages;
    std::string                    m_artifact_key;

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
    // Serialises the pipe drain. The controller accumulates the text itself;
    // keeping a second copy here would double a long run's output in memory
    // for no reader.
    std::mutex m_output_mutex;
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

    // Stages the pipeline actually ran with, so a flat config reports 1.
    uint32_t GetStageCount() const;

    rocprofvis_result_t GetStageState(uint32_t                     stage_index,
                                      rocprofvis_profiler_state_t& out) const;

    /*
     * Index of the stage that failed, or -1 while none has. "Capture
     * succeeded, analyze failed" leaves an expensive capture on disk that only
     * needs the cheap stage re-run, and the UI can only offer that if it knows
     * which stage went wrong.
     */
    int32_t GetFailingStage() const;

    /*
     * The pipeline's artifact - the file a successful run is understood to have
     * produced. NotAvailable when the run finished without it being scraped,
     * Pending while it may still arrive.
     */
    rocprofvis_result_t GetArtifactPath(std::string& out) const;

    rocprofvis_result_t GetScrapedValue(std::string const& key, std::string& out) const;

    rocprofvis_result_t GetScrapeStatus(std::string const&                   key,
                                        rocprofvis_profiler_scrape_status_t& out) const;

    // C ABI calls this just before issuing the monitor job (raw `this` in flight).
    void BeginMonitorJob();
    // Fired when the Job is destroyed (~Future), even if ExecuteJob never ran.
    // Unblocks the destructor. Idempotent.
    void EndMonitorJob();

    static rocprofvis_result_t ExecuteJob(ProfilerProcessController* controller, Future* future);

private:
    void UpdateOutput();
    void UpdateState();

    /*
     * Build the stage list, fill its scrape rules from the rule table, and
     * compile them.
     *
     * With `resolve_tools_locally`, every stage's tool is also resolved to a
     * local path up front. That is the point of doing it here: a typo in the
     * analyze stage's tool directory should fail before a twenty-minute capture
     * starts, not after it. A remote launch has already resolved its own
     * argv[0] against the remote filesystem and passes false.
     */
    rocprofvis_result_t PreparePipeline(bool resolve_tools_locally);

    // The `_locked` suffix means m_mutex is already held. UpdateState holds it
    // across a stage boundary so an executor swap is never observed half-done
    // by GetOutput, so nothing on that path may re-lock.
    rocprofvis_result_t StartStageLocked(uint32_t stage_index);
    void                FinishStageLocked(int exit_code);
    void                RelocateArtifactLocked(uint32_t stage_index);
    void                DrainExecutorLocked();
    bool                ExecutorRunning() const;
    // Maps a slot's scrape status onto the getter contract: Success with the
    // value when resolved, Pending while it may still arrive, NotAvailable once
    // it cannot, InvalidArgument when no stage declares the key at all.
    rocprofvis_result_t ScrapedValueLocked(std::string const& key, std::string& out) const;

    std::unique_ptr<IProfilerExecutor> m_executor;
    std::unique_ptr<ProfilerConfig> m_config;
    // Rebuilt per stage; the executor holds a reference only during Start.
    std::unique_ptr<ProfilerConfig> m_stage_config;
    std::atomic<rocprofvis_profiler_state_t> m_state;
    std::string m_output_text;
    int m_exit_code;
    mutable std::mutex m_mutex;
    /*
     * Set by Cancel before it drops m_mutex to kill the child, and never
     * cleared while that run is ending. It makes Cancel the sole owner of the
     * ending: UpdateState stops finalising, so the stage cannot be settled
     * twice, and no later stage can be started by a boundary that was already
     * in flight when the user asked to stop.
     */
    std::atomic<bool> m_cancel_requested{false};

    std::vector<ProfilerStageSpec>           m_stages;
    std::vector<std::string>                 m_stage_tool_paths;
    std::vector<std::string>                 m_stage_artifact_keys;
    std::vector<rocprofvis_profiler_state_t> m_stage_states;
    uint32_t                                 m_current_stage = 0;
    int32_t                                  m_failing_stage = -1;
    ProfilerScrapeEngine                     m_scrape;
    std::string                              m_artifact_key;
    // Banners would change the console of every single-stage run for no reason,
    // and every caller today is single-stage.
    bool                                     m_emit_banners = false;

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
