// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_ssh_file_dialog.h"
#include "rocprofvis_appwindow.h"
#include "widgets/rocprofvis_gui_helpers.h"
#include "widgets/rocprofvis_notification_manager.h"

#include "imgui.h"

namespace RocProfVis
{
namespace View
{

SshFileDialog::SshFileDialog(AppWindow* app_window)
: m_app_window(app_window)
, m_uri(std::make_shared<RemoteUri>())
, m_settings_dialog(nullptr)
, m_orchestrator(nullptr)
, m_file_browser(m_uri)
, m_failure_reported(false)
, m_show_progress_popup(false)
, m_last_progress()
{
    m_connection_store.Load();
    m_file_browser.SetConnectionAction([this]() { OpenConnectionSettings(); });
    m_file_browser.SetTypeFilter(RemoteFileBrowser::TypeFilter::kTraces);
    m_file_browser.SetTitle("Open Remote Trace");
    m_file_browser.SetAcceptLabel("Open");
}

void
SshFileDialog::ApplySelectedConnection()
{
    const SshConnectionConfig* cfg = m_connection_store.Get(m_selected_connection_id);
    if(cfg)
    {
        m_uri->SetConnection(*cfg);
    }
    else
    {
        m_uri->SetConnection(SshConnectionConfig());
    }
}

SshFileDialog::~SshFileDialog()
{
    // Destroy the orchestrator (which owns the monitored SshSession) before the
    // shared RemoteUri reference held here is released, mirroring the prior
    // AppWindow teardown ordering.
    m_orchestrator.reset();
}

void
SshFileDialog::Show()
{
    if(m_connection_store.Get(m_selected_connection_id) == nullptr)
    {
        m_selected_connection_id = m_connection_store.Empty()
                                       ? std::string()
                                       : m_connection_store.List().front().id;
    }
    ApplySelectedConnection();

    if(m_orchestrator && !m_orchestrator->IsRunning())
    {
        m_orchestrator.reset();
    }

    if(m_selected_connection_id.empty())
    {
        OpenConnectionSettings();
    }
    else
    {
        OpenBrowser();
    }
}

void
SshFileDialog::OpenBrowser()
{
    // Seeded with the last opened trace so the browser starts in its folder.
    m_file_browser.Open(m_uri->GetRemoteResultPathString(),
                        RemoteFileBrowser::PickMode::kFile,
                        [this](const std::string& path) { StartDownload(path); });
}

void
SshFileDialog::OpenConnectionSettings()
{
    m_settings_dialog = std::make_unique<SshSettingsDialog>(
        m_connection_store, m_selected_connection_id,
        [this](const std::string& id) { m_selected_connection_id = id; });
}

void
SshFileDialog::StartDownload(const std::string& remote_path)
{
    m_uri->SetRemoteResultPathString(remote_path.c_str());
    m_failure_reported    = false;
    m_show_progress_popup = false;
    m_last_progress       = FileStat::Snapshot();

    // Reuse the session the browser just listed with, so the download neither
    // reconnects nor prompts for credentials again. A session still busy with a
    // listing cannot start a download, so that case gets a fresh one.
    m_orchestrator = m_file_browser.TakeSession();
    if(!m_orchestrator || m_orchestrator->IsRunning())
    {
        m_orchestrator = std::make_unique<RemoteTraceOrchestrator>(m_uri, nullptr);
    }
    m_orchestrator->SetOnOpenFile([this](const std::string& local_path) {
        if(m_app_window)
        {
            m_app_window->OpenFile(local_path);
        }
    });
    m_orchestrator->DownloadPath();
}

void
SshFileDialog::Render()
{
    m_file_browser.Render();

    // The editor is a step of the open flow, so closing it - saved or not -
    // returns to the browser as long as there is a connection to browse.
    if(m_settings_dialog && !m_settings_dialog->Render())
    {
        m_settings_dialog.reset();
        if(m_connection_store.Get(m_selected_connection_id) != nullptr)
        {
            ApplySelectedConnection();
            OpenBrowser();
        }
    }

    if(m_orchestrator && m_orchestrator->HasFailed() && !m_failure_reported)
    {
        m_failure_reported = true;
        NotificationManager::GetInstance().Show(m_orchestrator->GetStatusMessage(),
                                                NotificationLevel::Error);
    }

    RenderProgressPopup();
}

void
SshFileDialog::RenderProgressPopup()
{
    SshSession* ssh_session = m_orchestrator ? m_orchestrator->GetSession() : nullptr;
    if(!ssh_session)
    {
        return;
    }

    const bool downloading = m_orchestrator->IsRunning();
    if(auto fetch = ssh_session->GetFileStat()->ConsumeIfUpdated())
    {
        m_last_progress = *fetch;

        if(!m_show_progress_popup && downloading)
        {
            m_show_progress_popup = true;
            ImGui::OpenPopup("Remote Download");
        }
    }

    if(RenderRemoteDownloadPopup("Remote Download", m_last_progress.name.c_str(),
                                 m_last_progress.downloaded, m_last_progress.size,
                                 !downloading, m_show_progress_popup))
    {
        m_orchestrator->Cancel();
    }
}

}  // namespace View
}  // namespace RocProfVis
