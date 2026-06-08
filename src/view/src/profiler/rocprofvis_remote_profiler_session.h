// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_controller_enums.h"
#include "rocprofvis_controller_types.h"
#include "rocprofvis_profiler.h"
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
class RemoteProfilerSession
{
public:
    // on_open_file is invoked with the local trace path once downloaded.
    RemoteProfilerSession(std::shared_ptr<RemoteUri>               uri,
                          std::function<void(const std::string&)> on_open_file);
    ~RemoteProfilerSession();

    RemoteProfilerSession(const RemoteProfilerSession&)            = delete;
    RemoteProfilerSession& operator=(const RemoteProfilerSession&) = delete;

    // Begins the workflow (connect phase). Profiler parameters mirror the local
    // ProfilerSession::Launch. Returns false if the session/connection could not
    // be created.
    bool Launch(rocprofvis_profiler_type_t profiler_type,
                const std::string&         profiler_path,
                const std::string&         target_executable,
                const std::string&         target_args,
                const std::string&         output_directory,
                const std::string&         profiler_args,
                const std::vector<std::pair<std::string, std::string>>& env_vars = {});

    bool                        IsRunning() const { return m_running; }
    // True only while the trace download phase is active. The dialog uses this
    // to drive the download-progress popup; relying on the FileStat snapshot
    // reaching downloaded==size is unreliable for small/fast transfers that jump
    // straight to the Completed status.
    bool                        IsDownloading() const { return m_phase == Phase::Downloading; }
    const std::string&          GetStatusMessage() const { return m_status_message; }
    rocprofvis_profiler_state_t GetProfilerState() const { return m_profiler_state; }
    std::string                 GetProfilerOutput();

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
    uint64_t                  ReadProfilerState() const;
    std::shared_ptr<RocEvent> BuildProfilerStatusEvent(uint64_t state) const;
    static bool               IsTerminalProfilerState(uint64_t state);
    void StartDownload();
    void Fail(const std::string& message);

    void CloseProfiler();

    std::shared_ptr<RemoteUri>               m_uri;
    std::function<void(const std::string&)>  m_on_open_file;
    std::unique_ptr<SshSession>              m_session;

    // Profiler C-objects (owned, mirroring the local ProfilerSession).
    rocprofvis_profiler_config_t*   m_config;
    rocprofvis_profiler_t*          m_profiler;
    rocprofvis_controller_future_t* m_future;
    uint64_t                        m_profiler_op_id;

    EventManager::SubscriptionToken m_remote_status_token;
    EventManager::SubscriptionToken m_profiler_status_token;

    Phase                       m_phase;
    bool                        m_running;
    std::string                 m_status_message;
    rocprofvis_profiler_state_t m_profiler_state;
};

}  // namespace View
}  // namespace RocProfVis
