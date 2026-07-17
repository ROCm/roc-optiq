// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_ssh_uri.h"
#include "rocprofvis_ssh_connection_store.h"
#include "rocprofvis_ssh_session.h"
#include "rocprofvis_ssh_settings_dialog.h"
#include "rocprofvis_remote_trace_orchestrator.h"

#include <memory>
#include <string>
#include <vector>

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
    void RenderRemoteFilePopup();

    // Browses the current m_uri browsing path, lazily creating the orchestrator
    // (bound to the directory-path callback) on first use and reusing it for
    // subsequent folder navigation. Reuse keeps the SSH session connected +
    // authenticated across clicks instead of reconnecting every time.
    void BrowseRemotePath();

    // Binds the currently selected SSH connection profile into m_uri so the
    // spawned orchestrator/session read the right host/credentials.
    void ApplySelectedConnection();

    // Remote file browser helpers.
    void EnsureBrowseOrchestrator();
    void OpenRemoteFileBrowser();
    void NavigateBrowserTo(const std::string& path, bool record_history);
    void RunRemoteSearch();
    void HandleSearchResults(const std::string& output);
    void ActivateBrowserEntry(const RemoteDir::FileEntry& entry);

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

    // ----- Remote file browser state -----
    bool                              m_show_remote_filesystem_popup;
    bool                              m_should_open_browser_popup; // defer OpenPopup to render scope
    bool                              m_should_close_browser_popup;
    bool                              m_browser_busy;          // a listing/search is in flight
    std::string                       m_browser_error;         // last listing/search failure
    std::string                       m_browser_dir;           // resolved current directory
    RemoteDir::Snapshot               m_last_directory_state;  // entries for m_browser_dir
    std::vector<RemoteDir::FileEntry> m_search_results;        // recursive-search hits
    bool                              m_in_search_mode;
    std::string                       m_search_root;           // dir the search ran in
    std::vector<std::string>          m_history_back;
    std::vector<std::string>          m_history_forward;
    std::string                       m_remote_file_filter;    // live filter text
    std::string                       m_address_edit;          // editable address-bar buffer
    bool                              m_address_editing;       // breadcrumb vs. editable field
    bool                              m_show_hidden;
    int                               m_type_filter;           // index into extension presets
    // Selection: "" = none, ".." = the parent row, otherwise the entry's name
    // (basename in directory mode, full path in search mode).
    std::string                       m_selected_name;
    bool                              m_scroll_to_selected;
};

}  // namespace View
}  // namespace RocProfVis
