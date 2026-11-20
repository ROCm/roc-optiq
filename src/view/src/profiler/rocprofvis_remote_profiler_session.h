// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_profiler_session_base.h"
#include "rocprofvis_event_manager.h"
#include "remote/rocprofvis_ssh_session.h"

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

class RemoteUri;

// Drives a remote profiling workflow as a non-blocking state machine:
//
//   connect -> authenticate -> launch remote profiler
//
// Connection/auth run on an owned SshSession (so prompts / host-key requests
// surface via RenderSshAuthModal). The remote profiler exec runs through the
// profiler C API (rocprofvis_profiler_launch_remote_async) on the SAME
// connection - so the SshSession must be idle while the profiler op runs. Once
// profiling completes and the profiler's worker has finished with the
// connection, the remote trace path is stored in the URI, and the connection
// can be handed over (TakeSession) to download it.
//
// All phase transitions happen on the main thread, inside event dispatch
// (kRemoteStatusChanged for SSH ops, kProfilerStatusChanged for the profiler
// op) or Update(); there is no worker thread here.
class RemoteProfilerSession : public ProfilerSessionBase
{
public:
    // The remote workflow runs as a sequential phase machine. The dialog maps
    // the current phase to the console status badge.
    enum class Phase
    {
        Idle,
        Connecting,
        Authenticating,
        Profiling,
        Done,
        Failed,
    };

    // Once profiling completes, the remote trace path comes from the
    // controller's scrape of the profiler's output and is stored as the URI's
    // result path. A run that produces no trace file clears it.
    // @param produces_trace: whether the run is expected to write a trace file;
    // when false, finding none is not a failure.
    RemoteProfilerSession(std::shared_ptr<RemoteUri> uri, bool produces_trace);
    ~RemoteProfilerSession() override;

    // Begins the workflow (connect phase). Profiler parameters mirror the local
    // ProfilerSession::Launch. Returns false if the session/connection could not
    // be created.
    bool Launch(const ProfilerLaunchSpec& spec) override;

    /*
     * Per-frame poll. Finishes the run (Phase::Done) once the profiler has
     * reported Completed and its worker has returned, which no event reports.
     */
    void Update();

    bool                        IsRunning() const { return m_running; }
    const std::string&          GetStatusMessage() const { return m_status_message; }
    // Cached profiler state tracked across the phase machine (distinct from the
    // base GetState(), which reads the live controller state).
    rocprofvis_profiler_state_t GetProfilerState() const { return m_profiler_state; }

    // Current workflow phase, used by the dialog to drive the status badge.
    Phase GetPhase() const { return m_phase; }

    // Accessor used by the dialog to render the auth modal.
    SshSession* GetSession() { return m_session.get(); }

    /*
     * Hands over the run's authenticated SshSession once the run is Done (the
     * profiler's worker has finished with it), freeing the profiler objects
     * first. Returns null otherwise, keeping the session for this run's own
     * teardown.
     */
    std::unique_ptr<SshSession> TakeSession();

private:
    void OnRemoteStatus(uint64_t operation_id, uint64_t status, rocprofvis_result_t result);
    void OnProfilerStatus(uint64_t operation_id, rocprofvis_profiler_state_t state);

    void StartProfiler();
    // Stores the remote trace path the controller scraped from the run.
    void FinishProfiling();

    void Fail(const std::string& message);

    std::shared_ptr<RemoteUri>  m_uri;
    std::unique_ptr<SshSession> m_session;
    bool                        m_produces_trace;

    EventManager::SubscriptionToken m_remote_status_token;
    EventManager::SubscriptionToken m_profiler_status_token;

    Phase                       m_phase;
    bool                        m_running;
    std::string                 m_status_message;
    rocprofvis_profiler_state_t m_profiler_state;
};

}  // namespace View
}  // namespace RocProfVis
