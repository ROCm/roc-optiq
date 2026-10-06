// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_ssh_uri.h"
#include "rocprofvis_ssh_connection_store.h"
#include "rocprofvis_ssh_session.h"
#include "rocprofvis_ssh_settings_dialog.h"
#include "rocprofvis_remote_trace_orchestrator.h"
#include "rocprofvis_remote_file_browser.h"

#include <memory>
#include <string>

namespace RocProfVis
{
namespace View
{

class AppWindow;

// Drives File > Open Remote...: Show() opens the remote file browser (or the
// connection editor first when no connection is saved), and picking a file
// downloads it over the browser's already-authenticated session and opens it.
// The browser's connection chip reopens the editor to switch or edit the
// connection; closing the editor returns to the browser.
//
// The connection configuration is owned as a std::shared_ptr<RemoteUri> so it
// can be shared with the spawned RemoteTraceOrchestrator / SshSession, which
// read it lazily across the whole non-blocking workflow.
class SshTestDialog
{
public:
    explicit SshTestDialog(AppWindow* app_window);
    ~SshTestDialog();

    // Starts the open-remote flow (from the File menu).
    void Show();

    // Renders the browser, the connection editor and the download progress
    // popup. Call every frame.
    void Render();

private:
    void RenderProgressPopup();
    void OpenBrowser();
    void OpenConnectionSettings();
    void StartDownload(const std::string& remote_path);

    // Binds the currently selected SSH connection profile into m_uri so the
    // spawned orchestrator/session read the right host/credentials.
    void ApplySelectedConnection();

    AppWindow*                               m_app_window;
    SshConnectionStore&                      m_connection_store;
    std::string                              m_selected_connection_id;
    std::shared_ptr<RemoteUri>               m_uri;
    std::unique_ptr<SshSettingsDialog>       m_settings_dialog;
    std::unique_ptr<RemoteTraceOrchestrator> m_orchestrator;
    // Shared remote file/directory picker. Owns its own SSH session for listing
    // directories, reading the connection from m_uri.
    RemoteFileBrowser                        m_file_browser;

    bool                                     m_failure_reported;
    bool                                     m_show_progress_popup;
    FileStat::Snapshot                       m_last_progress;
};

}  // namespace View
}  // namespace RocProfVis
