// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_ssh_session.h"
#include "rocprofvis_event_manager.h"

#include <functional>
#include <memory>
#include <string>

namespace RocProfVis
{
namespace View
{

class RemoteUri;

// Drives the "open remote trace" workflow as a non-blocking state machine:
//
//   connect -> authenticate -> (execute if a command is set)
//           -> (download if a result path is set) -> on_open_file(local_path)
//
// The orchestrator owns one SshSession and subscribes to kRemoteStatusChanged.
// Each phase is started via the session (which registers a MonitorOperation);
// when the AppMonitor reports that phase completed, the orchestrator advances
// to the next one. All work happens on the main thread inside event dispatch -
// there is no worker thread.
//
// Prompt / host-key dialogs are unaffected: callers keep polling the session's
// PromptRequest / HostKeyRequest (e.g. via RenderSshAuthModal).
class RemoteTraceOrchestrator
{
public:
    // on_open_file is invoked with the local path once a trace has been
    // downloaded (or immediately if the workflow only executes a command).
    RemoteTraceOrchestrator(std::shared_ptr<RemoteUri> uri, std::function<void(const std::string&)> on_open_file);
    ~RemoteTraceOrchestrator();

    // Begins the workflow (connect phase). Returns false if the session could
    // not be created / connected.
    bool Start();
    bool StartBrowsing();

    // True while a phase is in flight or pending.
    bool IsRunning() const { return m_running; }

    // Human-readable status for the open dialog.
    const std::string& GetStatusMessage() const { return m_status_message; }

    // Accessors used by the auth modal / progress dialogs.
    SshSession* GetSession() { return m_session.get(); }

private:
    enum class Phase
    {
        Idle,
        Connecting,
        Authenticating,
        Executing,
        Downloading,
        Browsing,
        Done,
        Failed,
    };

    void OnRemoteStatus(uint64_t status, rocprofvis_result_t result);
    void AdvanceAfterConnect();
    void AdvanceAfterAuthenticate();
    void AdvanceAfterExecute();
    void AdvanceAfterDownload();
    void AdvanceAfterBrowsing();
    void Browse();
    void Fail(const std::string& message);

    std::shared_ptr<RemoteUri>               m_uri;
    std::function<void(const std::string&)>  m_on_open_file;
    std::unique_ptr<SshSession>              m_session;
    EventManager::SubscriptionToken          m_status_token;
    Phase                                    m_phase;
    Phase                                    m_task;
    bool                                     m_running;
    std::string                              m_status_message;
};

}  // namespace View
}  // namespace RocProfVis
