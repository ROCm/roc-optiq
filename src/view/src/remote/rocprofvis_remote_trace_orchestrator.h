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

// Drives one remote task as a non-blocking state machine:
//
//   connect -> authenticate -> { stop | list a folder | download a file }
//
// The orchestrator owns one SshSession and subscribes to kRemoteStatusChanged.
// Each phase is started via the session (which registers a MonitorOperation);
// when the AppMonitor reports that phase completed, the orchestrator advances
// to the next one. All work happens on the main thread inside event dispatch -
// there is no worker thread.
//
// Once authenticated, later tasks reuse the live session, so browsing folder
// to folder and then downloading never reconnects or prompts again.
//
// Prompt / host-key dialogs are unaffected: callers keep polling the session's
// PromptRequest / HostKeyRequest (e.g. via RenderSshAuthModal).
class RemoteTraceOrchestrator
{
public:
    // on_result is invoked when a task succeeds: with the listed folder after
    // BrowsePath(), the local copy after DownloadPath(), and not at all after
    // Connect(). May be null.
    RemoteTraceOrchestrator(std::shared_ptr<RemoteUri>               uri,
                            std::function<void(const std::string&)> on_result);
    ~RemoteTraceOrchestrator();

    // Connects and authenticates, then stops (a connection test).
    bool Connect();

    // Lists m_uri's browsing path.
    bool BrowsePath();

    // Downloads m_uri's result path into the local cache.
    bool DownloadPath();

    // Replaces the on_result callback, e.g. once a session has been taken over
    // from the remote file browser.
    void SetOnResult(std::function<void(const std::string&)> on_result);

    // True while a phase is in flight or pending.
    bool IsRunning() const { return m_running; }

    // True once the last task ended in failure; GetStatusMessage() says why.
    bool HasFailed() const { return m_phase == Phase::Failed; }

    // Human-readable status of the current or last task.
    const std::string& GetStatusMessage() const { return m_status_message; }

    // Accessors used by the auth modal / progress dialogs.
    SshSession* GetSession() { return m_session.get(); }

private:
    enum class Phase
    {
        Idle,
        Connecting,
        Authenticating,
        Downloading,
        Browsing,
        Done,
        Failed,
    };

    enum class Task
    {
        kConnect,
        kBrowse,
        kDownload,
    };

    // Starts `task` on the live authenticated session, or connects first.
    bool Run(Task task);
    // Starts the task's own phase once authenticated.
    void RunTask();
    void OnRemoteStatus(uint64_t status);
    void Succeed(const std::string& result);
    void Fail(const std::string& message);

    std::shared_ptr<RemoteUri>               m_uri;
    std::function<void(const std::string&)>  m_on_result;
    std::unique_ptr<SshSession>              m_session;
    EventManager::SubscriptionToken          m_status_token;
    Phase                                    m_phase;
    Task                                     m_task;
    bool                                     m_running;
    // True once the owned session has authenticated, so later tasks can reuse
    // the live connection. Reset whenever a fresh session is started.
    bool                                     m_authenticated;
    std::string                              m_status_message;
};

}  // namespace View
}  // namespace RocProfVis
