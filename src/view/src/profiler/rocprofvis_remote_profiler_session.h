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
//   connect -> authenticate -> launch remote profiler -> download trace -> open
//
// Connection/auth/download run on an owned SshSession (so prompts / host-key
// requests surface via RenderSshAuthModal and download progress via the
// session's FileStat). The remote profiler exec runs through the profiler C API
// (rocprofvis_profiler_launch_remote_async) on the SAME connection - so the
// SshSession must be idle while the profiler op runs, and the download only
// starts after the profiler reports Completed.
//
// All phase transitions happen on the main thread inside event dispatch
// (kRemoteStatusChanged for SSH ops, kProfilerStatusChanged for the profiler
// op); there is no worker thread here.
class RemoteProfilerSession : public ProfilerSessionBase
{
public:
    // on_open_file is invoked with the local trace path once downloaded.
    // parse_trace_path is invoked with the remote profiler's captured stdout
    // once profiling completes; it must return the remote trace file path the
    // profiler produced (or empty if it cannot be determined), which drives the
    // SFTP download. May be null, in which case any path already set on the URI
    // is used.
    RemoteProfilerSession(std::shared_ptr<RemoteUri>                     uri,
                          std::function<void(const std::string&)>       on_open_file,
                          std::function<std::string(const std::string&)> parse_trace_path = nullptr);
    ~RemoteProfilerSession() override;

    // Begins the workflow (connect phase). Profiler parameters mirror the local
    // ProfilerSession::Launch. Returns false if the session/connection could not
    // be created.
    bool Launch(rocprofvis_profiler_type_t profiler_type,
                const std::string&         profiler_path,
                const std::string&         target_executable,
                const std::string&         target_args,
                const std::string&         output_directory,
                const std::string&         profiler_args,
                const std::vector<std::pair<std::string, std::string>>& env_vars = {}) override;

    bool                        IsRunning() const { return m_running; }
    // True only while the trace download phase is active. The dialog uses this
    // to drive the download-progress; relying on the FileStat snapshot
    // reaching downloaded==size is unreliable for small/fast transfers that jump
    // straight to the Completed status.
    bool                        IsDownloading() const { return m_phase == Phase::Downloading; }
    const std::string&          GetStatusMessage() const { return m_status_message; }
    // Cached profiler state tracked across the phase machine (distinct from the
    // base GetState(), which reads the live controller state).
    rocprofvis_profiler_state_t GetProfilerState() const { return m_profiler_state; }

    // Accessor used by the dialog to render the auth modal / download progress.
    SshSession* GetSession() { return m_session.get(); }

private:
    enum class Phase
    {
        Idle,
        Connecting,
        Authenticating,
        Profiling,
        Downloading,
        Done,
        Failed,
    };

    void OnRemoteStatus(uint64_t operation_id, uint64_t status, rocprofvis_result_t result);
    void OnProfilerStatus(uint64_t operation_id, rocprofvis_profiler_state_t state);

    void StartProfiler();
    void StartDownload();
    void Fail(const std::string& message);

    std::shared_ptr<RemoteUri>                     m_uri;
    std::function<void(const std::string&)>        m_on_open_file;
    std::function<std::string(const std::string&)> m_parse_trace_path;
    std::unique_ptr<SshSession>                    m_session;

    EventManager::SubscriptionToken m_remote_status_token;
    EventManager::SubscriptionToken m_profiler_status_token;

    Phase                       m_phase;
    bool                        m_running;
    std::string                 m_status_message;
    rocprofvis_profiler_state_t m_profiler_state;
};

}  // namespace View
}  // namespace RocProfVis
