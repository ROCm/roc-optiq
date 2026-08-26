// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_controller_enums.h"
#include "rocprofvis_events.h"
#include "rocprofvis_event_manager.h"
#include "rocprofvis_profiler_session.h"
// TEMPORARY (remote/SSH): the remote profiler session is only available when
// remote support is built. Remove this guard when the remote feature graduates.
#ifdef ROCPROFVIS_ENABLE_REMOTE
#include "rocprofvis_remote_profiler_session.h"
#endif
#include "rocprofvis_launch_shared_tabs.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace RocProfVis
{
namespace View
{

class AppWindow;
#ifdef ROCPROFVIS_ENABLE_REMOTE
class RemoteUri;  // TEMPORARY (remote/SSH)
#endif

// View-layer orchestrator for a profiling run. Owns the local / remote profiler
// sessions and normalizes their two very different control flows (local is
// event-driven via kProfilerStatusChanged; remote is a polled phase machine)
// into a single run state, output stream, and trace path that the dialog can
// read without knowing which mechanism is active.
//
// Scope: this class is the "run engine" only. Config authoring, presets,
// command-preview, console-text composition (preamble/epilogue/ANSI strip), and
// all rendering remain in the ProfilerLauncherDialog. The dialog never touches
// the underlying sessions directly; it drives the orchestrator through Launch /
// Cancel / Close / Update and reads state back through the getters below.
class ProfilerLaunchOrchestrator
{
public:
    // Everything the orchestrator needs to start a run. The config/preset layer
    // (the dialog) assembles this from its LaunchConfig + execution cache.
    struct LaunchRequest
    {
        // What to run: binary, argv, env, working directory. Handed to the
        // session as-is.
        ProfilerLaunchSpec         spec;

        bool                       is_remote       = false;
        bool                       auto_load_trace = true;

#ifdef ROCPROFVIS_ENABLE_REMOTE
        // Remote only: the connection/operation state for the SSH workflow.
        std::shared_ptr<RemoteUri> remote_uri;
#endif

        // Backend-specific scraper that derives the produced trace path from the
        // profiler's stdout. Used for the local trace resolution and forwarded
        // to the remote session for its download target.
        std::function<std::string(const std::string&)> parse_trace;
    };

    explicit ProfilerLaunchOrchestrator(AppWindow* app_window);
    ~ProfilerLaunchOrchestrator();

    ProfilerLaunchOrchestrator(const ProfilerLaunchOrchestrator&)            = delete;
    ProfilerLaunchOrchestrator& operator=(const ProfilerLaunchOrchestrator&) = delete;

    // Starts a run. Returns false on immediate failure (validation, allocation,
    // or could-not-start); GetLaunchError() then carries the reason.
    bool Launch(const LaunchRequest& request);

    // Cancels the in-flight run (local process or remote workflow). No-op when
    // nothing is running.
    void Cancel();

    // Tears down all run state (sessions, futures) and resets to Idle. The
    // orchestrator is reusable afterward.
    void Close();

    // Per-frame pump. Drives the remote phase machine, pulls streamed output,
    // and normalizes the run state. Must be called once per frame while the
    // owning view is active.
    void Update();

    // --- Normalized, presentation-agnostic run state ------------------------

    rocprofvis_profiler_state_t GetState() const { return m_profiler_state; }
    bool                        IsRunning() const { return m_is_running; }
    bool                        IsRemote() const { return m_is_remote; }

    // Latest accumulated profiler stdout (raw, including ANSI). The view strips
    // and composes for display.
    std::string GetRawOutput() const { return m_raw_output; }

    // Returns true exactly once after the raw output changed, so the view knows
    // to re-strip / recompose. Clears the dirty flag.
    bool ConsumeOutputDirty();

    // Trace path resolved on successful completion (local: scraper -> filesystem
    // scan fallback; remote: resolved + downloaded by the remote session).
    std::string GetTracePath() const { return m_trace_path; }

    // Exit code of the most recent local run (valid after a failed local run).
    int32_t GetExitCode() const;

    // Human-readable reason for an immediate launch failure (empty otherwise).
    const std::string& GetLaunchError() const { return m_launch_error; }

    // Human-readable status/error message from the remote workflow (e.g. "SSH
    // authentication failed."). Empty for local runs or when no remote session
    // exists. Always compiled so the dialog can surface remote failures without
    // remote-specific guards.
    std::string GetRemoteStatusMessage() const;

    // --- Remote-specific accessors for the download popup / auth modal ------
    // TEMPORARY (remote/SSH): remove these guards when remote graduates.
#ifdef ROCPROFVIS_ENABLE_REMOTE
    SshSession* GetRemoteSshSession() const;
    bool        IsRemoteDownloading() const;

    // Fills the status badge for a remote run (label + level + phase detail).
    // Returns false when no remote run is active (caller maps local state).
    bool GetRemotePhaseBadge(std::string& out_label, ConsoleStatusLevel& out_level,
                             std::string& out_detail) const;
#endif

private:
    void OnProfilerStateChanged(rocprofvis_profiler_state_t new_state);
    bool LaunchLocal(const LaunchRequest& request);
#ifdef ROCPROFVIS_ENABLE_REMOTE
    bool LaunchRemote(const LaunchRequest& request);
#endif
    void ResolveLocalTracePath();

    AppWindow*                              m_app_window;
    ProfilerSession                         m_profiler_session;
#ifdef ROCPROFVIS_ENABLE_REMOTE
    std::unique_ptr<RemoteProfilerSession>  m_remote_session;
#endif
    EventManager::SubscriptionToken         m_profiler_status_token;

#ifdef ROCPROFVIS_ENABLE_REMOTE
    // Orchestrator holds its own reference to the shared RemoteUri so the remote
    // session (and its deferred SSH teardown) can outlive the dialog's copy.
    std::shared_ptr<RemoteUri>              m_remote_uri;
#endif

    rocprofvis_profiler_state_t m_profiler_state;
    bool                        m_is_running;
    bool                        m_is_remote;
    bool                        m_auto_load_trace;

    std::function<std::string(const std::string&)> m_parse_trace;

    std::string m_raw_output;
    bool        m_output_dirty;
    std::string m_trace_path;
    std::string m_launch_error;
};

}  // namespace View
}  // namespace RocProfVis
