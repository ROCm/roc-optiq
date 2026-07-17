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

    // Browses the current m_uri browsing path. If this orchestrator already
    // owns a connected + authenticated session (e.g. from a previous browse),
    // it skips straight to the browse phase and reuses that session instead of
    // reconnecting and re-authenticating. Otherwise it falls back to the full
    // StartBrowsing() pipeline. This is what folder-to-folder navigation should
    // call so each click does not tear down and re-auth the SSH session.
    bool BrowsePath();

    // Runs a one-shot remote shell command (e.g. a `find`) whose stdout is
    // delivered to the search-results callback when it completes. Reuses a live
    // authenticated session when available, otherwise connects + authenticates
    // first. Used by the file browser's "search subfolders" feature.
    bool SearchPath(const std::string& command);

    // Registers the callback invoked with the raw stdout of a SearchPath run.
    void SetSearchResultsCallback(std::function<void(const std::string&)> on_search_results)
    {
        m_on_search_results = std::move(on_search_results);
    }

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
        Searching,
        Done,
        Failed,
    };

    void OnRemoteStatus(uint64_t status, rocprofvis_result_t result);
    void AdvanceAfterConnect();
    void AdvanceAfterAuthenticate();
    void AdvanceAfterExecute();
    void AdvanceAfterDownload();
    void AdvanceAfterBrowsing();
    void AdvanceAfterSearch();
    void Browse();
    void RunSearch();
    void Fail(const std::string& message);

    std::shared_ptr<RemoteUri>               m_uri;
    std::function<void(const std::string&)>  m_on_open_file;
    std::function<void(const std::string&)>  m_on_search_results;
    std::string                              m_search_command;
    std::unique_ptr<SshSession>              m_session;
    EventManager::SubscriptionToken          m_status_token;
    Phase                                    m_phase;
    Phase                                    m_task;
    bool                                     m_running;
    // True once the owned session has completed its authenticate phase, so a
    // subsequent BrowsePath() can reuse the live connection without redoing
    // connect + authenticate. Reset whenever a fresh session is started.
    bool                                     m_authenticated;
    std::string                              m_status_message;
};

}  // namespace View
}  // namespace RocProfVis
