// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_remote_file_browser.h"
#include "rocprofvis_ssh_uri.h"
#include "rocprofvis_ssh_connection_store.h"
#include "rocprofvis_ssh_session.h"
#include "rocprofvis_ssh_settings_dialog.h"
#include "rocprofvis_remote_trace_orchestrator.h"

#include <memory>
#include <string>

namespace RocProfVis
{
namespace View
{

class AppWindow;

// Floating (non-modal) window that drives a remote SSH "open trace" workflow.
//
// The window holds the per-profiler fields (command line, output database) and
// a summary of the connection target. Connection settings (host/user/auth) are
// edited in a separate, transient SshSettingsDialog launched from here via the
// "Configure SSH Connection..." button.
//
// The connection configuration is owned as a std::shared_ptr<RemoteUri> so it
// can be shared with the spawned RemoteTraceOrchestrator / SshSession, which
// read it lazily across the whole non-blocking workflow.
class SshTestDialog
{
public:
    explicit SshTestDialog(AppWindow* app_window);
    ~SshTestDialog();

    // Makes the floating window visible (e.g. from the File menu).
    void Show();

    // Renders the floating window, the on-demand settings dialog, the SSH auth
    // modal, and the download/output popups. Call every frame.
    void Render();

private:
    void RenderProgressPopup();
    void RenderOutputPopup();

    // Wires the file-browser widget's callbacks to the SSH transport (listing,
    // recursive search, file selection, cancel). Called once at construction.
    void SetupFileBrowserCallbacks();

    // Lazily creates the orchestrator bound to the directory-path and
    // search-results callbacks. Reused for subsequent navigation/search so the
    // SSH session stays connected + authenticated across clicks.
    void EnsureOrchestrator();

    // Browses the current m_uri browsing path via the (reused) orchestrator.
    void BrowseRemotePath();

    // Feeds completed directory listings / search results / failures from the
    // live SSH session into the file browser. Call every frame.
    void PollFileBrowser();

    // Parses `find -printf` stdout into browser entries and pushes them in.
    void HandleSearchResults(const std::string& output);

    // Binds the currently selected SSH connection profile into m_uri so the
    // spawned orchestrator/session read the right host/credentials.
    void ApplySelectedConnection();

    AppWindow*                               m_app_window;
    SshConnectionStore                       m_connection_store;
    std::string                              m_selected_connection_id;
    std::shared_ptr<RemoteUri>               m_uri;
    std::unique_ptr<SshSettingsDialog>       m_settings_dialog;
    std::unique_ptr<RemoteTraceOrchestrator> m_orchestrator;

    bool                                     m_show_window;
    std::string                              m_status_msg;

    bool                                     m_show_stdout_popup;
    ExecutionOutput::Snapshot                m_last_stdout;

    bool                                     m_show_progress_popup;
    FileStat::Snapshot                       m_last_progress;

    RemoteFileBrowser                        m_file_browser;
    bool                                     m_search_in_progress;
};

}  // namespace View
}  // namespace RocProfVis
