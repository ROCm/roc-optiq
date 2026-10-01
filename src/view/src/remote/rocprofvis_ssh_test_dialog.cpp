// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_ssh_test_dialog.h"
#include "rocprofvis_appwindow.h"
#include "widgets/rocprofvis_gui_helpers.h"
#include "widgets/rocprofvis_notification_manager.h"

#include "imgui.h"

namespace RocProfVis
{
namespace View
{

SshTestDialog::SshTestDialog(AppWindow* app_window)
: m_app_window(app_window)
, m_uri(std::make_shared<RemoteUri>())
, m_settings_dialog(nullptr)
, m_orchestrator(nullptr)
, m_file_browser(m_uri)
, m_failure_reported(false)
, m_show_progress_popup(false)
, m_last_progress()
{
    m_file_browser.SetConnectionAction([this]() { OpenConnectionSettings(); });
    m_file_browser.SetTypeFilter(RemoteFileBrowser::TypeFilter::kTraces);
}

SshTestDialog::~SshTestDialog()
{
    // Destroy the orchestrator (which owns the monitored SshSession) before the
    // shared RemoteUri reference held here is released, mirroring the prior
    // AppWindow teardown ordering.
    m_orchestrator.reset();
}

void
SshTestDialog::ApplySelectedConnection()
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

void
SshTestDialog::Show()
{
    // Other dialogs keep their own store over the same saved profiles, so pick
    // up anything they added or edited since this one last loaded.
    m_connection_store.Load();
    if(m_connection_store.Get(m_selected_connection_id) == nullptr)
    {
        m_selected_connection_id =
            m_connection_store.Empty() ? std::string() : m_connection_store.List().front().id;
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
SshTestDialog::OpenBrowser()
{
    // Seeded with the last opened trace so the browser starts in its folder.
    m_file_browser.Open(m_uri->GetRemoteResultPathString(), RemoteFileBrowser::PickMode::kFile,
                        [this](const std::string& path) { StartDownload(path); });
}

void
SshTestDialog::OpenConnectionSettings()
{
    m_settings_dialog = std::make_unique<SshSettingsDialog>(
        m_connection_store, m_selected_connection_id,
        [this](const std::string& id) { m_selected_connection_id = id; });
}

void
SshTestDialog::StartDownload(const std::string& remote_path)
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
    m_orchestrator->SetOnOpenFile(
        [this](const std::string& local_path)
        {
            if(m_app_window)
            {
                m_app_window->OpenFile(local_path);
            }
        });
    m_orchestrator->DownloadPath();
}

void
SshTestDialog::Render()
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
SshTestDialog::RenderProgressPopup()
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

    // The popup has no button of its own, so it closes when the download ends
    // either way; a failure is reported as a notification.
    RenderRemoteDownloadPopup("Remote Download", m_last_progress.name.c_str(),
                              m_last_progress.downloaded, m_last_progress.size, !downloading,
                              m_show_progress_popup);
}

}  // namespace View
}  // namespace RocProfVis
