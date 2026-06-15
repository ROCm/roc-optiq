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

    bool                                     m_show_remote_filesystem_popup;
    RemoteDir::Snapshot                      m_last_directory_state;
    uint32_t                                 m_selected_file_index;
};

}  // namespace View
}  // namespace RocProfVis
