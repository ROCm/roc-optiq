// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_ssh_test_dialog.h"
#include "rocprofvis_ssh_auth_modal.h"
#include "rocprofvis_appwindow.h"
#include "widgets/rocprofvis_widget.h"

#include "imgui.h"

#include <cfloat>
#include <string>

namespace RocProfVis
{
namespace View
{

SshTestDialog::SshTestDialog(AppWindow* app_window)
: m_app_window(app_window)
, m_uri(std::make_shared<RemoteUri>())
, m_settings_dialog(nullptr)
, m_orchestrator(nullptr)
, m_show_window(false)
, m_status_msg()
, m_show_stdout_popup(false)
, m_last_stdout()
, m_show_progress_popup(false)
, m_last_progress()
, m_show_remote_filesystem_popup(false)
{
    m_uri->LoadFromJson();
}

SshTestDialog::~SshTestDialog()
{
    // Destroy the orchestrator (which owns the monitored SshSession) before the
    // shared RemoteUri reference held here is released, mirroring the prior
    // AppWindow teardown ordering.
    m_orchestrator.reset();
}

void
SshTestDialog::Show()
{
    m_show_window = true;
    m_status_msg.clear();
}

void
SshTestDialog::Render()
{
    if(m_show_window)
    {
        ImGui::SetNextWindowSize(ImVec2(560, 0), ImGuiCond_FirstUseEver);
        if(ImGui::Begin("SSH Test", &m_show_window))
        {
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            const float label_w = 170.0f;

            // Connection target summary (edited via the settings dialog).
            ImGui::AlignTextToFramePadding(); ImGui::Text("Connection"); ImGui::SameLine(label_w);
            std::string host = m_uri->GetRemoteHostString();
            std::string user = m_uri->GetRemoteUserString();
            if(host.empty())
            {
                ImGui::TextDisabled("Not configured");
            }
            else
            {
                ImGui::Text("%s@%s:%s",
                            user.empty() ? "?" : user.c_str(),
                            host.c_str(),
                            m_uri->GetRemotePortString().c_str());
            }

            if(ImGui::Button("Configure SSH Connection..."))
            {
                // Create the transient settings dialog on demand, seeded with
                // the current config; commit copies the edited values back.
                m_settings_dialog = std::make_unique<SshSettingsDialog>(
                    *m_uri,
                    [this](RemoteUri edited) { *m_uri = std::move(edited); });
            }

            ImGui::Spacing();

            // Per-profiler fields that stay on this dialog.
            ImGui::AlignTextToFramePadding(); ImGui::Text("Profiler command line"); ImGui::SameLine(label_w);
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::InputTextWithHint("##rcommand", "/path/to/executable [parameters]",
                m_uri->GetRemoteCommandLineBuffer(), m_uri->GetRemoteCommandLineBufferSize());

            ImGui::AlignTextToFramePadding(); ImGui::Text("Profiler output database"); ImGui::SameLine(label_w);
            ImGui::SetNextItemWidth(-FLT_MIN-90);
            ImGui::InputTextWithHint("##rpath", "/path/to/file.db",
                m_uri->GetRemoteResultPathBuffer(), m_uri->GetRemoteResultPathBufferSize());
            ImGui::SameLine();
            if (ImGui::Button("Browse", ImVec2(80, 0)))
            {
                m_uri->InitRemoteBrowsingPathString(m_uri->GetRemoteResultPathArray().data());
                m_orchestrator = std::make_unique<RemoteTraceOrchestrator>(
                    m_uri,
                    [this](const std::string& path)
                    {
                        m_uri->SetCurrentDirectoryPath(path.c_str());
                    });
                m_orchestrator->StartBrowsing();
            }

            ImGui::Spacing();

            const std::string& status_msg =
                m_orchestrator ? m_orchestrator->GetStatusMessage() : m_status_msg;
            if(!status_msg.empty())
            {
                ImVec4 color = ImVec4(1.0f, 0.5f, 0.3f, 1.0f);
                ImGui::TextColored(color, "%s", status_msg.c_str());
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            bool can_authenticate = !m_uri->GetRemotePasswordString().empty() ||
                                    !m_uri->GetRemoteIdentityFileString().empty();
            bool has_remote_path = !m_uri->GetRemoteCommandLineString().empty() ||
                                   !m_uri->GetRemoteResultPathString().empty();
            bool can_open = !m_uri->GetRemoteHostString().empty() &&
                            !m_uri->GetRemoteUserString().empty() &&
                            has_remote_path && can_authenticate;

            bool running = m_orchestrator && m_orchestrator->IsRunning();

            if(!can_open) ImGui::BeginDisabled();
            if(!running)
            {
                if(ImGui::Button("Open", ImVec2(110, 0)))
                {
                    if(!m_uri->SaveToJson())
                    {
                        m_status_msg = "Failed to backup authentication parameters.";
                    }
                    m_show_stdout_popup   = false;
                    m_show_progress_popup = false;

                    // Drive the connect -> authenticate -> execute -> download
                    // chain on the main thread via the AppMonitor; OpenFile is
                    // invoked when the trace has been downloaded.
                    m_orchestrator = std::make_unique<RemoteTraceOrchestrator>(
                        m_uri,
                        [this](const std::string& local_path)
                        {
                            if(m_app_window)
                            {
                                m_app_window->OpenFile(local_path);
                            }
                        });
                    m_orchestrator->Start();
                }
            }
            else
            {
                ImGui::Text("%s", m_orchestrator->GetStatusMessage().c_str());
            }
            if(!can_open) ImGui::EndDisabled();

            ImGui::SameLine();
            if(!running)
            {
                if(ImGui::Button("Close", ImVec2(110, 0)))
                {
                    m_show_window = false;
                }
            }
        }
        ImGui::End();
    }

    // Render the transient settings dialog; destroy it once it reports closed.
    if(m_settings_dialog)
    {
        if(!m_settings_dialog->Render())
        {
            m_settings_dialog.reset();
        }
    }

    // Auth prompts / host-key requests, and download/output popups.
    RenderSshAuthModal(m_orchestrator ? m_orchestrator->GetSession() : nullptr);
    RenderProgressPopup();
    RenderOutputPopup();
    RenderRemoteFilePopup();
}

void
SshTestDialog::RenderProgressPopup()
{
    SshSession* ssh_session = m_orchestrator ? m_orchestrator->GetSession() : nullptr;
    if(!ssh_session) return;

    if(auto fetch = ssh_session->GetFileStat()->consume_if_updated())
    {
        m_last_progress = *fetch;

        if(!m_show_progress_popup)
        {
            m_show_progress_popup = true;
            ImGui::OpenPopup("Remote Download");
        }
    }

    if(m_show_progress_popup)
    {
        PopUpStyle popup_style;
        popup_style.PushPopupStyles();
        popup_style.PushTitlebarColors();
        popup_style.CenterPopup();
        ImGui::SetNextWindowSize(ImVec2(440, 0));

        if(ImGui::BeginPopupModal("Remote Download", nullptr,
            ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoTitleBar))
        {
            const auto& fetch = m_last_progress;

            ImGui::Text("Downloading: %s", fetch.name.c_str());

            uint64_t done  = fetch.downloaded;
            uint64_t total = fetch.size;

            if(total > 0)
            {
                float frac = static_cast<float>(done) / static_cast<float>(total);

                std::string label =
                    std::to_string(done / 1024) + " / " +
                    std::to_string(total / 1024) + " KiB";

                ImGui::ProgressBar(frac, ImVec2(-FLT_MIN, 0), label.c_str());
            }
            else
            {
                ImGui::Text("Connecting...");
            }

            if(total > 0 && done >= total)
            {
                ImGui::CloseCurrentPopup();
                m_show_progress_popup = false;
            }

            ImGui::EndPopup();
        }

        popup_style.PopStyles();
    }
}

void
SshTestDialog::RenderOutputPopup()
{
    SshSession* ssh_session = m_orchestrator ? m_orchestrator->GetSession() : nullptr;
    if(!ssh_session) return;

    if(auto fetch = ssh_session->GetExecutionOutput()->consume_if_updated())
    {
        m_last_stdout = *fetch;
        if(!m_show_stdout_popup)
        {
            m_show_stdout_popup = true;
            ImGui::OpenPopup("Remote Execute");
        }
    }

    if(m_show_stdout_popup)
    {
        ImGui::SetNextWindowSize(ImVec2(600, 400));
        if(ImGui::BeginPopupModal("Remote Execute", nullptr))
        {
            ImGui::BeginChild("output", ImVec2(0, 0), true);
            ImGui::TextUnformatted(m_last_stdout.text.c_str());
            ImGui::EndChild();

            if(m_last_stdout.finished)
            {
                ImGui::CloseCurrentPopup();
                m_show_stdout_popup = false;
            }

            ImGui::EndPopup();
        }
    }
}

void SshTestDialog::RenderRemoteFilePopup()
{
    SshSession* ssh_session =
        m_orchestrator ? m_orchestrator->GetSession() : nullptr;
    if (!ssh_session) return;

    if (auto fetch = ssh_session->GetRemoteDir()->consume_if_updated())
    {
        m_last_directory_state = *fetch;
        if (!m_show_remote_filesystem_popup)
        {
            m_show_remote_filesystem_popup = true;
            ImGui::OpenPopup("Remote File System");
        }
    }

    if (m_show_remote_filesystem_popup)
    {
        ImGui::SetNextWindowSize(ImVec2(600, 400));
        if (ImGui::BeginPopupModal("Remote File System", nullptr))
        {
            float button_area_height = ImGui::GetFrameHeightWithSpacing() * 2.5f;

            // --- Scroll area ---
            ImGui::BeginChild("FileList", ImVec2(0, -button_area_height), true);

            ImGui::Indent(10.0f);

            int index = 0;

            // --- ".." entry ---
            {
                bool selected = (m_selected_file_index == index);
                if (ImGui::Selectable("..", selected, ImGuiSelectableFlags_AllowDoubleClick))
                {
                    m_selected_file_index = index;

                    if (ImGui::IsMouseDoubleClicked(0))
                    {
                        m_uri->MakeRemoteBrowsingPath("..");
                        m_orchestrator = std::make_unique<RemoteTraceOrchestrator>(
                            m_uri,
                            [this](const std::string& path)
                            {
                                m_uri->SetCurrentDirectoryPath(path.c_str());
                            });
                        m_orchestrator->StartBrowsing();
                    }
                }
                index++;
            }

            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 6));

            for (auto& f : m_last_directory_state.list_dir)
            {
                bool selected = (m_selected_file_index == index);

                std::string label = f.is_dir
                    ? (f.name + "/")
                    : f.name;

                if (ImGui::Selectable(label.c_str(), selected, ImGuiSelectableFlags_AllowDoubleClick))
                {
                    m_selected_file_index = index;

                    if (ImGui::IsMouseDoubleClicked(0))
                    {
                        m_uri->MakeRemoteBrowsingPath(f.name.c_str());
                        if (f.is_dir)
                        {
                            m_orchestrator = std::make_unique<RemoteTraceOrchestrator>(
                                m_uri,
                                [this](const std::string& path)
                                {
                                    m_uri->SetCurrentDirectoryPath(path.c_str());
                                });
                            m_orchestrator->StartBrowsing();
                        }
                        else
                        {
                            m_orchestrator.release();
                            m_uri->UseRemoteBrowsingPathString();
                            ImGui::CloseCurrentPopup();
                            m_show_remote_filesystem_popup = false;
                        }
                    }
                }

                index++;
            }

            ImGui::PopStyleVar();
            ImGui::Unindent(10.0f);

            ImGui::EndChild();

            // --- Bottom buttons ---
            ImGui::Separator();

            float button_width = 110.0f;
            float spacing = ImGui::GetStyle().ItemSpacing.x;
            float total_width = button_width * 2 + spacing;

            ImGui::SetCursorPosX(ImGui::GetContentRegionAvail().x - total_width);

            if (ImGui::Button("Cancel", ImVec2(button_width, 0)))
            {
                ImGui::CloseCurrentPopup();
                m_show_remote_filesystem_popup = false;
            }

            ImGui::SameLine();

            if (ImGui::Button("Open", ImVec2(button_width, 0)))
            {

                if (m_selected_file_index == 0)
                {
                    m_uri->MakeRemoteBrowsingPath("..");
                    m_orchestrator = std::make_unique<RemoteTraceOrchestrator>(
                        m_uri,
                        [this](const std::string& path)
                        {
                            m_uri->SetCurrentDirectoryPath(path.c_str());
                        });
                    m_orchestrator->StartBrowsing();
                }
                else
                {
                    auto& f = m_last_directory_state.list_dir[m_selected_file_index - 1];
                    m_uri->MakeRemoteBrowsingPath(f.name.c_str());
                    if (f.is_dir)
                    {
                        m_orchestrator = std::make_unique<RemoteTraceOrchestrator>(
                            m_uri,
                            [this](const std::string& path)
                            {
                                m_uri->SetCurrentDirectoryPath(path.c_str());
                            });
                        m_orchestrator->StartBrowsing();
                    }
                    else
                    {
                        m_orchestrator.release();
                        m_uri->UseRemoteBrowsingPathString();
                        ImGui::CloseCurrentPopup();
                        m_show_remote_filesystem_popup = false;
                    }
                }
            }

            ImGui::EndPopup();
        }
    }
}


}  // namespace View
}  // namespace RocProfVis
