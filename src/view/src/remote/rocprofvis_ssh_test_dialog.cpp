// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_ssh_test_dialog.h"
#include "rocprofvis_ssh_auth_modal.h"
#include "rocprofvis_appwindow.h"
#include "rocprofvis_settings_manager.h"
#include "rocprofvis_font_manager.h"
#include "icons/rocprovfis_icon_defines.h"
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
        SettingsManager&  settings  = SettingsManager::GetInstance();
        ImFont*           icon_font = settings.GetFontManager().GetFont(FontType::kIcon);
        const ImGuiStyle& style     = ImGui::GetStyle();

        const ImU32 accent         = settings.GetColor(Colors::kAccent);
        const ImU32 accent_hover   = settings.GetColor(Colors::kAccentHover);
        const ImU32 accent_active  = settings.GetColor(Colors::kAccentActive);
        const ImU32 text_on_accent = settings.GetColor(Colors::kTextOnAccent);
        const ImU32 text_dim       = settings.GetColor(Colors::kTextDim);

        // Fixed width, auto height, so the window hugs its content with no empty band.
        const float dialog_width =
            GetResponsiveWindowSize(ImVec2(720.0f, 0.0f), ImVec2(620.0f, 0.0f)).x;
        if(const ImGuiViewport* viewport = ImGui::GetMainViewport())
        {
            ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing,
                                    ImVec2(0.5f, 0.5f));
        }
        ImGui::SetNextWindowSizeConstraints(ImVec2(dialog_width, 0.0f),
                                            ImVec2(dialog_width, FLT_MAX));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 12.0f);
        if(ImGui::Begin("Open Remote Trace", &m_show_window,
               ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_AlwaysAutoResize |
                   ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoTitleBar))
        {
            const bool running = m_orchestrator && m_orchestrator->IsRunning();

            auto render_icon = [&](const char* glyph, ImU32 color) {
                ImGui::PushFont(icon_font, ImGui::GetFontSize());
                ImGui::PushStyleColor(ImGuiCol_Text, color);
                ImGui::TextUnformatted(glyph);
                ImGui::PopStyleColor();
                ImGui::PopFont();
            };
            auto field_label = [&](const char* label) {
                ImGui::PushStyleColor(ImGuiCol_Text, text_dim);
                ImGui::TextUnformatted(label);
                ImGui::PopStyleColor();
            };
            auto open_connection_settings = [&]() {
                m_settings_dialog = std::make_unique<SshSettingsDialog>(
                    m_connection_store, m_selected_connection_id,
                    [this](const std::string& id)
                    {
                        m_selected_connection_id = id;
                        ApplySelectedConnection();
                    });
            };

            const bool can_authenticate = !m_uri->GetRemotePasswordString().empty() ||
                                          !m_uri->GetRemoteIdentityFileString().empty();
            const bool has_remote_path = !m_uri->GetRemoteResultPathString().empty();
            const bool can_open = !m_uri->GetRemoteHostString().empty() &&
                                  !m_uri->GetRemoteUserString().empty() &&
                                  has_remote_path && can_authenticate;

            const std::string host = m_uri->GetRemoteHostString();
            const std::string user = m_uri->GetRemoteUserString();
            const std::string host_chip =
                host.empty()
                    ? std::string("Configure")
                    : (user.empty() ? std::string("?") : user) + "@" + host + ":" +
                          m_uri->GetRemotePortString();

            constexpr float LABEL_WIDTH  = 132.0f;
            constexpr float BUTTON_WIDTH = 118.0f;

            // Tighten the vertical gaps between the stacked panels.
            const ImVec2 default_item_spacing = style.ItemSpacing;
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                                ImVec2(default_item_spacing.x, 4.0f));

            ImGui::PushStyleColor(ImGuiCol_ChildBg, settings.GetColor(Colors::kBgFrame));
            ImGui::PushStyleColor(ImGuiCol_Border, settings.GetColor(Colors::kPanelBorderSubtle));
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 10.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(18.0f, 14.0f));
            ImGui::BeginChild("##remote_trace_header", ImVec2(0.0f, 0.0f),
                              ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY,
                              ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
            {
                if(ImGui::BeginTable("##remote_trace_header_table", 3,
                                      ImGuiTableFlags_SizingStretchProp))
                {
                    ImGui::TableSetupColumn("Title", ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableSetupColumn("Connection", ImGuiTableColumnFlags_WidthFixed,
                                            248.0f);
                    ImGui::TableSetupColumn("Close", ImGuiTableColumnFlags_WidthFixed, 24.0f);
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    render_icon(ICON_COMPASS, accent);
                    ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
                    ImGui::BeginGroup();
                    ImGui::PushFont(nullptr,
                                    settings.GetFontManager().GetFontSize(FontSize::kMedLarge));
                    ImGui::TextUnformatted("Remote Trace");
                    ImGui::PopFont();
                    ImGui::PushStyleColor(ImGuiCol_Text, text_dim);
                    ImGui::TextUnformatted("Launch or fetch a profiler trace over SSH.");
                    ImGui::PopStyleColor();
                    ImGui::EndGroup();

                    ImGui::TableSetColumnIndex(1);
                    ImGui::PushID("header_connection");
                    if(running)
                    {
                        ImGui::BeginDisabled();
                    }
                    ImGui::PushStyleColor(ImGuiCol_Button, settings.GetColor(Colors::kButton));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                                          settings.GetColor(Colors::kButtonHovered));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                                          settings.GetColor(Colors::kButtonActive));
                    ImGui::PushStyleColor(ImGuiCol_Text, host.empty() ? text_dim : accent);
                    if(ImGui::Button((host_chip + "##button").c_str(),
                                     ImVec2(-FLT_MIN, 0.0f)))
                    {
                        open_connection_settings();
                    }
                    ImGui::PopStyleColor(4);
                    if(running)
                    {
                        ImGui::EndDisabled();
                    }
                    ImGui::PopID();

                    ImGui::TableSetColumnIndex(2);
                    if(XButton("##remote_trace_close", "Close", &settings))
                    {
                        m_show_window = false;
                    }
                    ImGui::EndTable();
                }
            }
            ImGui::EndChild();
            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor(2);

            ImGui::PushStyleColor(ImGuiCol_ChildBg, settings.GetColor(Colors::kBgMain));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.0f, 10.0f));
            ImGui::BeginChild("##remote_trace_body", ImVec2(0.0f, 0.0f),
                              ImGuiChildFlags_AutoResizeY,
                              ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
            {
                ImGui::PushStyleColor(ImGuiCol_ChildBg, settings.GetColor(Colors::kBgPanel));
                ImGui::PushStyleColor(ImGuiCol_Border,
                                      settings.GetColor(Colors::kPanelBorderSubtle));
                ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 10.0f);
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 12.0f));
                ImGui::BeginChild("##remote_trace_target", ImVec2(0.0f, 0.0f),
                                  ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY,
                                  ImGuiWindowFlags_NoScrollbar |
                                      ImGuiWindowFlags_NoScrollWithMouse);
                {
                    if(ImGui::BeginTable("##remote_trace_target_table", 3,
                                          ImGuiTableFlags_SizingStretchProp))
                    {
                        ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed,
                                                LABEL_WIDTH);
                        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
                        ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed,
                                                94.0f);
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::AlignTextToFramePadding();
                        field_label("Result database");

                        ImGui::TableSetColumnIndex(1);
                        ImGui::SetNextItemWidth(-FLT_MIN);
                        InputTextStringWithHint("##rpath", "/path/to/file.db",
                                                m_uri->GetRemoteResultPath());

                        ImGui::TableSetColumnIndex(2);
                        if(ImGui::Button("Browse", ImVec2(-FLT_MIN, 0.0f)))
                        {
                            m_uri->InitRemoteBrowsingPathString(
                                m_uri->GetRemoteResultPathString().c_str());
                            // Start a fresh browsing session; subsequent folder
                            // navigation reuses this session.
                            m_orchestrator.reset();
                            BrowseRemotePath();
                        }
                        ImGui::EndTable();
                    }
                }
                ImGui::EndChild();
                ImGui::PopStyleVar(2);
                ImGui::PopStyleColor(2);
            }
            ImGui::EndChild();
            ImGui::PopStyleVar();
            ImGui::PopStyleColor();

            const std::string& status_msg =
                m_orchestrator ? m_orchestrator->GetStatusMessage() : m_status_msg;
            bool open_clicked = false;
            ImGui::PushStyleColor(ImGuiCol_ChildBg, settings.GetColor(Colors::kBgFrame));
            ImGui::PushStyleColor(ImGuiCol_Border, settings.GetColor(Colors::kPanelBorderSubtle));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(18.0f, 14.0f));
            ImGui::BeginChild("##remote_trace_footer", ImVec2(0.0f, 0.0f),
                              ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY,
                              ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
            {
                if(ImGui::BeginTable("##remote_trace_footer_table", 2,
                                      ImGuiTableFlags_SizingStretchProp))
                {
                    const float total_width = BUTTON_WIDTH * 2.0f + style.ItemSpacing.x;
                    ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed,
                                            total_width);
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    if(status_msg.empty())
                    {
                        ImGui::AlignTextToFramePadding();
                        ImGui::PushStyleColor(ImGuiCol_Text, text_dim);
                        ImGui::TextUnformatted("Ready");
                        ImGui::PopStyleColor();
                    }
                    else
                    {
                        render_icon(running ? ICON_ARROWS_CYCLE : ICON_CHAIN,
                                    running ? accent : text_dim);
                        ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
                        ImGui::PushID("footer_status");
                        ImGui::PushStyleColor(ImGuiCol_Text, running ? accent : text_dim);
                        ElidedText(status_msg.c_str(), ImGui::GetContentRegionAvail().x, 420.0f,
                                   Alignment_Left, true);
                        ImGui::PopStyleColor();
                        ImGui::PopID();
                    }

                    ImGui::TableSetColumnIndex(1);
                    if(ImGui::Button("Close", ImVec2(BUTTON_WIDTH, 0.0f)))
                    {
                        m_show_window = false;
                    }
                    ImGui::SameLine();

                    const bool open_disabled = !can_open || running;
                    if(open_disabled)
                    {
                        ImGui::BeginDisabled();
                    }
                    ImGui::PushStyleColor(ImGuiCol_Button, accent);
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, accent_hover);
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, accent_active);
                    ImGui::PushStyleColor(ImGuiCol_Text, text_on_accent);
                    open_clicked =
                        ImGui::Button(running ? "Working..." : "Open",
                                      ImVec2(BUTTON_WIDTH, 0.0f));
                    ImGui::PopStyleColor(4);
                    if(open_disabled)
                    {
                        ImGui::EndDisabled();
                    }
                    ImGui::EndTable();
                }
            }
            ImGui::EndChild();
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(2);

            if(open_clicked)
            {
                // Connection config is persisted by the settings dialog; bind the
                // selected profile in case it changed since last apply.
                ApplySelectedConnection();
                m_show_stdout_popup   = false;
                m_show_progress_popup = false;

                // Drive the connect -> authenticate -> execute -> download chain
                // on the main thread via the AppMonitor; OpenFile is invoked when
                // the trace has been downloaded.
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

            ImGui::PopStyleVar();
        }
        ImGui::End();
        ImGui::PopStyleVar(2);
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
