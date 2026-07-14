// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_ssh_test_dialog.h"
#include "rocprofvis_ssh_auth_modal.h"
#include "rocprofvis_appwindow.h"
#include "rocprofvis_settings_manager.h"
#include "widgets/rocprofvis_widget.h"
#include "widgets/rocprofvis_gui_helpers.h"

#include "imgui.h"

#include <cfloat>
#include <cstdio>
#include <ctime>
#include <string>

namespace RocProfVis
{
namespace View
{

namespace
{
    // Formats a byte count as a compact human-readable size (e.g. "4.0 KiB").
    std::string format_file_size(uint64_t bytes)
    {
        constexpr const char* UNITS[] = { "B", "KiB", "MiB", "GiB", "TiB" };
        double size = static_cast<double>(bytes);
        int    unit = 0;
        while (size >= 1024.0 && unit < 4)
        {
            size /= 1024.0;
            unit++;
        }

        char buf[32];
        if (unit == 0)
        {
            std::snprintf(buf, sizeof(buf), "%llu B", static_cast<unsigned long long>(bytes));
        }
        else
        {
            std::snprintf(buf, sizeof(buf), "%.1f %s", size, UNITS[unit]);
        }
        return std::string(buf);
    }

    // Formats a Unix epoch (seconds) as local "YYYY-MM-DD HH:MM"; "-" if zero.
    std::string format_file_time(uint64_t epoch_seconds)
    {
        if (epoch_seconds == 0)
        {
            return "-";
        }

        std::time_t t = static_cast<std::time_t>(epoch_seconds);
        std::tm     tm_buf{};
#ifdef _WIN32
        if (localtime_s(&tm_buf, &t) != 0)
        {
            return "-";
        }
#else
        if (localtime_r(&t, &tm_buf) == nullptr)
        {
            return "-";
        }
#endif
        char buf[32];
        if (std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", &tm_buf) == 0)
        {
            return "-";
        }
        return std::string(buf);
    }
}  // namespace

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
, m_last_directory_state()
, m_selected_file_index(0)
{
    m_connection_store.Load();
    if(!m_connection_store.Empty())
    {
        m_selected_connection_id = m_connection_store.List().front().id;
    }
    ApplySelectedConnection();
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
                // Create the transient profile-managing dialog on demand, seeded
                // with the currently selected connection; on commit, bind the
                // chosen connection into m_uri.
                m_settings_dialog = std::make_unique<SshSettingsDialog>(
                    m_connection_store, m_selected_connection_id,
                    [this](const std::string& id)
                    {
                        m_selected_connection_id = id;
                        ApplySelectedConnection();
                    });
            }

            ImGui::Spacing();

            // Per-profiler fields that stay on this dialog.
            ImGui::AlignTextToFramePadding(); ImGui::Text("Profiler command line"); ImGui::SameLine(label_w);
            ImGui::SetNextItemWidth(-FLT_MIN);
            InputTextStringWithHint("##rcommand", "/path/to/executable [parameters]",
                m_uri->GetRemoteCommandLine());

            ImGui::AlignTextToFramePadding(); ImGui::Text("Profiler output database"); ImGui::SameLine(label_w);
            ImGui::SetNextItemWidth(-FLT_MIN-90);
            InputTextStringWithHint("##rpath", "/path/to/file.db",
                m_uri->GetRemoteResultPath());
            ImGui::SameLine();
            if (ImGui::Button("Browse", ImVec2(80, 0)))
            {
                m_uri->InitRemoteBrowsingPathString(m_uri->GetRemoteResultPathString().c_str());
                // Start a fresh browsing session from the top-level Browse
                // button; subsequent folder navigation reuses this session.
                m_orchestrator.reset();
                BrowseRemotePath();
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
                    // Connection config is persisted by the settings dialog; bind
                    // the selected profile in case it changed since last apply.
                    ApplySelectedConnection();
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

    if(auto fetch = ssh_session->GetFileStat()->ConsumeIfUpdated())
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

    if(auto fetch = ssh_session->GetExecutionOutput()->ConsumeIfUpdated())
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
            const float footer_height =
                ImGui::GetFrameHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y;
            ImGui::BeginChild("output", ImVec2(0, -footer_height), true);
            ImGui::TextUnformatted(m_last_stdout.text.c_str());
            ImGui::EndChild();

            const bool finished = m_last_stdout.finished;
            ImGui::TextUnformatted(finished ? "Execution finished." : "Executing...");
            ImGui::SameLine();

            // Always offer a manual Close so the popup can never wedge open, even
            // if the terminal snapshot is missed (e.g. connection dropped).
            if(ImGui::Button("Close"))
            {
                ImGui::CloseCurrentPopup();
                m_show_stdout_popup = false;
            }

            ImGui::EndPopup();
        }
    }
}

void
SshTestDialog::BrowseRemotePath()
{
    // Create the orchestrator on first use, bound to the callback that mirrors
    // the browsed directory back into m_uri. Reusing the same orchestrator (and
    // its SshSession) across folder navigation keeps the connection open and
    // authenticated; BrowsePath() only reconnects when there is no live session.
    if (!m_orchestrator)
    {
        m_orchestrator = std::make_unique<RemoteTraceOrchestrator>(
            m_uri,
            [this](const std::string& path)
            {
                m_uri->SetCurrentDirectoryPath(path.c_str());
            });
    }
    m_orchestrator->BrowsePath();
}

void SshTestDialog::RenderRemoteFilePopup()
{
    SshSession* ssh_session =
        m_orchestrator ? m_orchestrator->GetSession() : nullptr;
    if (!ssh_session) return;

    if (auto fetch = ssh_session->GetRemoteDir()->ConsumeIfUpdated())
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

            // --- File table (scrollable) ---
            // Folders are tinted with the theme accent color so they stand out
            // from files; the selectable spans all columns so a click anywhere
            // on the row selects/opens the entry.
            SettingsManager& settings      = SettingsManager::GetInstance();
            const ImU32      folder_color  = settings.GetColor(Colors::kAccent);

            const ImGuiTableFlags table_flags =
                ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg |
                ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable;

            if (ImGui::BeginTable("RemoteFiles", 3, table_flags,
                    ImVec2(0, -button_area_height)))
            {
                ImGui::TableSetupScrollFreeze(0, 1);
                ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, 90.0f);
                ImGui::TableSetupColumn("Modified", ImGuiTableColumnFlags_WidthFixed, 140.0f);
                ImGui::TableHeadersRow();

                uint32_t index = 0;

                // --- ".." parent entry (always a directory) ---
                {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);

                    bool selected = (m_selected_file_index == index);
                    ImGui::PushStyleColor(ImGuiCol_Text, folder_color);
                    if (ImGui::Selectable("..", selected,
                            ImGuiSelectableFlags_SpanAllColumns |
                            ImGuiSelectableFlags_AllowDoubleClick))
                    {
                        m_selected_file_index = index;

                        if (ImGui::IsMouseDoubleClicked(0))
                        {
                            m_uri->MakeRemoteBrowsingPath("..");
                            BrowseRemotePath();
                        }
                    }
                    ImGui::PopStyleColor();

                    ImGui::TableSetColumnIndex(1);
                    ImGui::TextUnformatted("-");
                    ImGui::TableSetColumnIndex(2);
                    ImGui::TextUnformatted("-");

                    index++;
                }

                for (auto& f : m_last_directory_state.list_dir)
                {
                    ImGui::TableNextRow();

                    // --- Name column (folders get a trailing slash + accent) ---
                    ImGui::TableSetColumnIndex(0);
                    bool        selected = (m_selected_file_index == index);
                    std::string label    = f.is_dir ? (f.name + "/") : f.name;

                    if (f.is_dir)
                    {
                        ImGui::PushStyleColor(ImGuiCol_Text, folder_color);
                    }
                    bool clicked = ImGui::Selectable(label.c_str(), selected,
                        ImGuiSelectableFlags_SpanAllColumns |
                        ImGuiSelectableFlags_AllowDoubleClick);
                    if (f.is_dir)
                    {
                        ImGui::PopStyleColor();
                    }

                    if (clicked)
                    {
                        m_selected_file_index = index;

                        if (ImGui::IsMouseDoubleClicked(0))
                        {
                            m_uri->MakeRemoteBrowsingPath(f.name.c_str());
                            if (f.is_dir)
                            {
                                BrowseRemotePath();
                            }
                            else
                            {
                                m_orchestrator.reset();
                                m_uri->UseRemoteBrowsingPathString();
                                ImGui::CloseCurrentPopup();
                                m_show_remote_filesystem_popup = false;
                            }
                        }
                    }

                    // --- Size column (directories have no meaningful size) ---
                    ImGui::TableSetColumnIndex(1);
                    ImGui::TextUnformatted(f.is_dir ? "-" : format_file_size(f.size).c_str());

                    // --- Modified column ---
                    ImGui::TableSetColumnIndex(2);
                    ImGui::TextUnformatted(format_file_time(f.time).c_str());

                    index++;
                }

                ImGui::EndTable();
            }

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
                    BrowseRemotePath();
                }
                else
                {
                    auto& f = m_last_directory_state.list_dir[m_selected_file_index - 1];
                    m_uri->MakeRemoteBrowsingPath(f.name.c_str());
                    if (f.is_dir)
                    {
                        BrowseRemotePath();
                    }
                    else
                    {
                        m_orchestrator.reset();
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
