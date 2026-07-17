// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_ssh_test_dialog.h"
#include "rocprofvis_ssh_auth_modal.h"
#include "rocprofvis_appwindow.h"
#include "rocprofvis_core_string_utils.h"
#include "rocprofvis_settings_manager.h"
#include "rocprofvis_utils.h"
#include "widgets/rocprofvis_widget.h"
#include "widgets/rocprofvis_gui_helpers.h"
#include "rocprofvis_font_manager.h"
#include "icons/rocprovfis_icon_defines.h"

#include "imgui.h"

#include <algorithm>
#include <cfloat>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <string>
#include <vector>

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

    // Human-readable "Type" column label.
    std::string type_label(const RemoteDir::FileEntry& entry)
    {
        if (entry.is_dir)
        {
            return "Folder";
        }
        std::string ext = posix_file_extension(entry.name);
        return ext.empty() ? std::string("File") : Core::String::to_lower_copy(ext) + " file";
    }

    // Extension presets for the "type" filter dropdown. Directories are always
    // shown regardless of the active preset.
    struct TypeFilterPreset
    {
        const char*              label;
        std::vector<std::string> extensions;  // empty => match every file
    };

    const std::vector<TypeFilterPreset>& type_filter_presets()
    {
        static const std::vector<TypeFilterPreset> presets = {
            { "All files", {} },
            { "Trace databases (*.db, *.rpd)", { "db", "rpd" } },
            { "Project files (*.rpv)", { "rpv" } },
            { "Traces & projects (*.db, *.rpd, *.rpv)", { "db", "rpd", "rpv" } },
        };
        return presets;
    }

    // Wraps s in single quotes for safe embedding in a POSIX shell command.
    std::string shell_single_quote(const std::string& s)
    {
        std::string out = "'";
        for (char c : s)
        {
            out += (c == '\'') ? "'\\''" : std::string(1, c);
        }
        out += "'";
        return out;
    }

    // Builds a `find` command emitting "type\tsize\tmtime\tpath" per match whose
    // name contains query (case-insensitive) under dir. Capped so a huge tree
    // cannot flood the UI. GNU find's -printf is assumed (Linux remote hosts).
    constexpr int SEARCH_RESULT_LIMIT = 5000;

    std::string build_find_command(const std::string& query, const std::string& dir)
    {
        std::string pattern = "*" + query + "*";
        return "find " + shell_single_quote(dir.empty() ? "." : dir) + " -iname " +
               shell_single_quote(pattern) +
               " -printf '%y\\t%s\\t%T@\\t%p\\n' 2>/dev/null | head -n " +
               std::to_string(SEARCH_RESULT_LIMIT);
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
, m_should_open_browser_popup(false)
, m_should_close_browser_popup(false)
, m_browser_busy(false)
, m_browser_error()
, m_browser_dir()
, m_last_directory_state()
, m_search_results()
, m_in_search_mode(false)
, m_search_root()
, m_history_back()
, m_history_forward()
, m_remote_file_filter()
, m_address_edit()
, m_address_editing(false)
, m_show_hidden(false)
, m_type_filter(0)
, m_selected_name()
, m_scroll_to_selected(false)
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
                OpenRemoteFileBrowser();
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

    // Auth prompts / host-key requests, and download/output popups. The download
    // and output popups are suppressed while the browser is open so the recursive
    // search (which reuses the execute path) does not open the stdout popup.
    RenderSshAuthModal(m_orchestrator ? m_orchestrator->GetSession() : nullptr);
    if (!m_show_remote_filesystem_popup)
    {
        RenderProgressPopup();
        RenderOutputPopup();
    }
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
SshTestDialog::EnsureBrowseOrchestrator()
{
    // Create the orchestrator on first use, bound to the callback that mirrors
    // the browsed directory back into m_uri. Reusing the same orchestrator (and
    // its SshSession) across folder navigation keeps the connection open and
    // authenticated; BrowsePath() only reconnects when there is no live session.
    // The search-results callback receives the raw `find` stdout.
    if (!m_orchestrator)
    {
        m_orchestrator = std::make_unique<RemoteTraceOrchestrator>(
            m_uri,
            [this](const std::string& path)
            {
                m_uri->SetCurrentDirectoryPath(path.c_str());
            });
        m_orchestrator->SetSearchResultsCallback(
            [this](const std::string& output) { HandleSearchResults(output); });
    }
}

void
SshTestDialog::BrowseRemotePath()
{
    EnsureBrowseOrchestrator();
    m_orchestrator->BrowsePath();
}

void
SshTestDialog::OpenRemoteFileBrowser()
{
    // Fresh browsing session: reset navigation, search, filter and selection
    // state, then seed the initial directory from the current result path's
    // parent (or the remote home when empty).
    m_orchestrator.reset();
    m_in_search_mode = false;
    m_search_results.clear();
    m_history_back.clear();
    m_history_forward.clear();
    m_remote_file_filter.clear();
    m_selected_name.clear();
    m_browser_error.clear();
    m_last_directory_state = RemoteDir::Snapshot();

    m_uri->InitRemoteBrowsingPathString(m_uri->GetRemoteResultPathString().c_str());
    const std::string seed = m_uri->GetRemoteBrowsingPathString();

    m_show_remote_filesystem_popup = true;
    m_should_open_browser_popup    = true;  // opened at render scope (matches BeginPopupModal)

    NavigateBrowserTo(seed.empty() ? std::string(".") : seed, false);
}

void
SshTestDialog::NavigateBrowserTo(const std::string& path, bool record_history)
{
    const std::string target = normalize_posix_path(path);

    if (record_history && !m_browser_dir.empty() && target != m_browser_dir)
    {
        m_history_back.push_back(m_browser_dir);
        m_history_forward.clear();
    }

    m_browser_dir       = target;  // replaced by the server-resolved path on arrival
    m_address_edit      = target;
    m_in_search_mode    = false;
    m_search_root.clear();
    m_search_results.clear();
    m_selected_name.clear();
    m_browser_error.clear();
    m_browser_busy      = true;

    m_uri->SetRemoteBrowsingPath(target.c_str());
    BrowseRemotePath();
}

void
SshTestDialog::RunRemoteSearch()
{
    if (m_remote_file_filter.empty())
    {
        return;
    }
    EnsureBrowseOrchestrator();
    m_search_root  = m_browser_dir;
    m_browser_busy = true;
    m_browser_error.clear();
    m_selected_name.clear();
    m_orchestrator->SearchPath(build_find_command(m_remote_file_filter, m_browser_dir));
}

void
SshTestDialog::HandleSearchResults(const std::string& output)
{
    // Each line is "%y\t%s\t%T@\t%p": type letter, size, mtime, and full path.
    std::vector<RemoteDir::FileEntry> results;

    std::string::size_type pos = 0;
    while (pos < output.size())
    {
        std::string::size_type eol = output.find('\n', pos);
        std::string line = (eol == std::string::npos) ? output.substr(pos)
                                                       : output.substr(pos, eol - pos);
        pos = (eol == std::string::npos) ? output.size() : eol + 1;
        if (line.empty())
        {
            continue;
        }

        std::string::size_type t1 = line.find('\t');
        std::string::size_type t2 = (t1 == std::string::npos) ? t1 : line.find('\t', t1 + 1);
        std::string::size_type t3 = (t2 == std::string::npos) ? t2 : line.find('\t', t2 + 1);
        if (t3 == std::string::npos)
        {
            continue;
        }

        std::string type_field = line.substr(0, t1);
        std::string size_field = line.substr(t1 + 1, t2 - t1 - 1);
        std::string time_field = line.substr(t2 + 1, t3 - t2 - 1);

        RemoteDir::FileEntry entry;
        entry.name   = line.substr(t3 + 1);
        entry.is_dir = (!type_field.empty() && type_field[0] == 'd');
        entry.size   = static_cast<uint64_t>(std::strtoull(size_field.c_str(), nullptr, 10));
        entry.time   = static_cast<uint64_t>(std::strtod(time_field.c_str(), nullptr));
        results.push_back(std::move(entry));
    }

    m_search_results = std::move(results);
    m_in_search_mode = true;
    m_browser_busy   = false;
    m_selected_name.clear();
}

void
SshTestDialog::ActivateBrowserEntry(const RemoteDir::FileEntry& entry)
{
    // In search mode the entry name is a path (absolute or relative to the
    // search root); in directory mode it is a plain name under m_browser_dir.
    std::string full_path;
    if (m_in_search_mode)
    {
        full_path = (!entry.name.empty() && entry.name[0] == '/')
                        ? normalize_posix_path(entry.name)
                        : join_posix_path(m_search_root, entry.name);
    }
    else
    {
        full_path = join_posix_path(m_browser_dir, entry.name);
    }

    if (entry.is_dir)
    {
        NavigateBrowserTo(full_path, true);
    }
    else
    {
        m_uri->SetRemoteResultPathString(full_path.c_str());
        m_orchestrator.reset();
        m_browser_busy = false;
        m_should_close_browser_popup = true;
        m_show_remote_filesystem_popup = false;
    }
}

void SshTestDialog::RenderRemoteFilePopup()
{
    // Pull a completed directory listing into browser state (pushed by the reused
    // SSH session). Recursive-search results arrive separately via the
    // orchestrator's search callback (HandleSearchResults).
    if (m_orchestrator)
    {
        if (SshSession* ssh_session = m_orchestrator->GetSession())
        {
            if (auto fetch = ssh_session->GetRemoteDir()->ConsumeIfUpdated())
            {
                m_last_directory_state = *fetch;
                m_in_search_mode       = false;
                m_browser_busy         = false;
                m_selected_name.clear();
                if (!m_last_directory_state.path.empty())
                {
                    m_browser_dir  = m_last_directory_state.path;
                    m_address_edit = m_browser_dir;
                    m_uri->SetCurrentDirectoryPath(m_browser_dir.c_str());
                }
            }
        }

        // Surface a browse/search failure. Fail() clears IsRunning() and leaves a
        // descriptive status (not "Done." and not a transient progress message).
        if (m_browser_busy && !m_orchestrator->IsRunning())
        {
            const std::string& status = m_orchestrator->GetStatusMessage();
            const bool transient = status.empty() || status == "Done." ||
                                   status.rfind("Connecting", 0) == 0 ||
                                   status.rfind("Authenticating", 0) == 0 ||
                                   status.rfind("Browsing", 0) == 0 ||
                                   status.rfind("Searching", 0) == 0;
            if (!transient)
            {
                m_browser_error = status;
                m_browser_busy  = false;
            }
        }
    }

    if (!m_show_remote_filesystem_popup)
    {
        return;
    }

    SettingsManager&  settings   = SettingsManager::GetInstance();
    ImFont*           icon_font  = settings.GetFontManager().GetFont(FontType::kIcon);
    const ImGuiStyle& style      = ImGui::GetStyle();

    const ImU32 accent         = settings.GetColor(Colors::kAccent);
    const ImU32 accent_hover   = settings.GetColor(Colors::kAccentHover);
    const ImU32 accent_active  = settings.GetColor(Colors::kAccentActive);
    const ImU32 text_on_accent = settings.GetColor(Colors::kTextOnAccent);
    const ImU32 text_dim       = settings.GetColor(Colors::kTextDim);
    const ImU32 text_main      = settings.GetColor(Colors::kTextMain);
    const ImU32 transparent    = settings.GetColor(Colors::kTransparent);
    const ImU32 btn_col        = settings.GetColor(Colors::kButton);
    const ImU32 btn_hover      = settings.GetColor(Colors::kButtonHovered);
    const ImU32 btn_active     = settings.GetColor(Colors::kButtonActive);

    // The rows come from the recursive-search hits in search mode, otherwise the
    // current directory listing. Entry names are full paths in search mode.
    const std::vector<RemoteDir::FileEntry>& source =
        m_in_search_mode ? m_search_results : m_last_directory_state.list_dir;

    // A file passes only when its extension matches the active preset; folders are
    // always shown so the user can navigate anywhere.
    auto passes_type = [&](const RemoteDir::FileEntry& e) -> bool {
        if (e.is_dir)
        {
            return true;
        }
        const auto& presets = type_filter_presets();
        if (m_type_filter <= 0 || m_type_filter >= static_cast<int>(presets.size()))
        {
            return true;
        }
        const auto& exts = presets[m_type_filter].extensions;
        if (exts.empty())
        {
            return true;
        }
        std::string ext = Core::String::to_lower_copy(posix_file_extension(e.name));
        for (const auto& a : exts)
        {
            if (ext == a)
            {
                return true;
            }
        }
        return false;
    };

    if (m_should_open_browser_popup)
    {
        ImGui::OpenPopup("Remote File System");
        m_should_open_browser_popup = false;
    }

    PopUpStyle popup_style;
    popup_style.PushPopupStyles();

    ImGui::SetNextWindowSize(ImVec2(900, 600), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(640, 440), ImVec2(FLT_MAX, FLT_MAX));

    if (ImGui::BeginPopupModal("Remote File System", nullptr,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoTitleBar))
    {
        const bool busy     = m_browser_busy || (m_orchestrator && m_orchestrator->IsRunning());
        const bool can_back = !m_history_back.empty();
        const bool can_fwd  = !m_history_forward.empty();
        const bool can_up   = !m_in_search_mode && !is_posix_root_path(m_browser_dir);

        // Header card: title, host chip, navigation, address bar and filters.
        ImGui::PushStyleColor(ImGuiCol_ChildBg, settings.GetColor(Colors::kBgFrame));
        ImGui::PushStyleColor(ImGuiCol_Border, settings.GetColor(Colors::kPanelBorderSubtle));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 10.0f));

        const float header_h = 10.0f * 2.0f + ImGui::GetTextLineHeightWithSpacing() +
                               ImGui::GetFrameHeightWithSpacing() +
                               ImGui::GetFrameHeightWithSpacing();
        ImGui::BeginChild("RemoteExplorerHeader", ImVec2(0.0f, header_h), true,
                          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        {
            // Title and host chip.
            ImGui::PushFont(icon_font, ImGui::GetFontSize());
            ImGui::PushStyleColor(ImGuiCol_Text, accent);
            ImGui::TextUnformatted(ICON_COMPASS);
            ImGui::PopStyleColor();
            ImGui::PopFont();
            ImGui::SameLine();
            ImGui::TextUnformatted("Remote File System");

            const std::string host_chip =
                m_uri->GetRemoteUserString() + "@" + m_uri->GetRemoteHostString() + ":" +
                m_uri->GetRemotePortString();
            ImGui::SameLine();
            ImGui::SetCursorPosX(ImGui::GetWindowContentRegionMax().x -
                                 ImGui::CalcTextSize(host_chip.c_str()).x - 4.0f);
            ImGui::PushStyleColor(ImGuiCol_Text, text_dim);
            ImGui::TextUnformatted(host_chip.c_str());
            ImGui::PopStyleColor();

            // Navigation buttons and address bar.
            auto nav_button = [&](const char* glyph, const char* tip, bool enabled) -> bool {
                if (!enabled)
                {
                    ImGui::BeginDisabled();
                }
                bool pressed = IconButton(glyph, icon_font, ImVec2(0, 0), tip, false,
                                          style.FramePadding, btn_col, btn_hover, btn_active);
                if (!enabled)
                {
                    ImGui::EndDisabled();
                }
                ImGui::SameLine();
                return pressed && enabled;
            };

            if (nav_button(ICON_CHEVRON_LEFT, "Back", can_back && !busy))
            {
                m_history_forward.push_back(m_browser_dir);
                std::string target = m_history_back.back();
                m_history_back.pop_back();
                NavigateBrowserTo(target, false);
            }
            if (nav_button(ICON_CHEVRON_RIGHT, "Forward", can_fwd && !busy))
            {
                m_history_back.push_back(m_browser_dir);
                std::string target = m_history_forward.back();
                m_history_forward.pop_back();
                NavigateBrowserTo(target, false);
            }
            if (nav_button(ICON_ARROW_UP, "Up one level", can_up && !busy))
            {
                NavigateBrowserTo(posix_parent_path(m_browser_dir), true);
            }
            if (nav_button(ICON_ARROWS_CYCLE, "Refresh", !busy))
            {
                if (m_in_search_mode)
                {
                    RunRemoteSearch();
                }
                else
                {
                    NavigateBrowserTo(m_browser_dir, false);
                }
            }
            if (nav_button(ICON_HOME, "Home", !busy))
            {
                NavigateBrowserTo(".", true);
            }
            if (IconButton(ICON_EDIT, icon_font, ImVec2(0, 0),
                           m_address_editing ? "Show breadcrumbs" : "Edit path", false,
                           style.FramePadding, btn_col, btn_hover, btn_active))
            {
                m_address_editing = !m_address_editing;
                m_address_edit    = m_browser_dir;
            }
            ImGui::SameLine();

            // Address pill: editable text field, or clickable breadcrumb segments.
            ImGui::PushStyleColor(ImGuiCol_ChildBg, settings.GetColor(Colors::kBgPanel));
            ImGui::PushStyleColor(ImGuiCol_Border, settings.GetColor(Colors::kBorderColor));
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 2.0f));
            ImGui::BeginChild("RemoteExplorerAddress",
                              ImVec2(0.0f, ImGui::GetFrameHeight() + 4.0f), true,
                              ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
            {
                if (m_address_editing)
                {
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    if (InputTextStringWithHint("##addr_edit", "/path/to/directory",
                            m_address_edit, ImGuiInputTextFlags_EnterReturnsTrue))
                    {
                        NavigateBrowserTo(m_address_edit, true);
                        m_address_editing = false;
                    }
                    if (ImGui::IsKeyPressed(ImGuiKey_Escape))
                    {
                        m_address_editing = false;
                    }
                }
                else
                {
                    ImGui::PushStyleColor(ImGuiCol_Button, transparent);
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, accent_hover);
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, accent_active);
                    ImGui::PushStyleColor(ImGuiCol_Text, accent);
                    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, style.FramePadding.y));

                    if (ImGui::Button("/##crumb_root"))
                    {
                        NavigateBrowserTo("/", true);
                    }

                    std::string accum;
                    size_t seg = 0;
                    while (seg < m_browser_dir.size())
                    {
                        if (m_browser_dir[seg] == '/')
                        {
                            ++seg;
                            continue;
                        }
                        size_t start = seg;
                        while (seg < m_browser_dir.size() && m_browser_dir[seg] != '/')
                        {
                            ++seg;
                        }
                        std::string segment = m_browser_dir.substr(start, seg - start);
                        accum += "/";
                        accum += segment;

                        ImGui::SameLine(0, 2.0f);
                        ImGui::PushFont(icon_font, ImGui::GetFontSize());
                        ImGui::PushStyleColor(ImGuiCol_Text, text_dim);
                        ImGui::AlignTextToFramePadding();
                        ImGui::TextUnformatted(ICON_CHEVRON_RIGHT);
                        ImGui::PopStyleColor();
                        ImGui::PopFont();
                        ImGui::SameLine(0, 2.0f);

                        std::string crumb = segment + "##crumb" + accum;
                        if (ImGui::Button(crumb.c_str()))
                        {
                            NavigateBrowserTo(accum, true);
                        }
                    }

                    ImGui::PopStyleVar();
                    ImGui::PopStyleColor(4);
                }
            }
            ImGui::EndChild();
            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor(2);

            // Name filter, recursive search, type filter and hidden toggle.
            ImGui::SetNextItemWidth(240.0f);
            const bool filter_enter = InputTextStringWithHint("##remote_file_filter",
                "Filter name (Enter to search subfolders)", m_remote_file_filter,
                ImGuiInputTextFlags_EnterReturnsTrue);
            ImGui::SameLine();
            if (!m_remote_file_filter.empty())
            {
                if (XButton("##remote_filter_clear", "Clear filter", &settings))
                {
                    m_remote_file_filter.clear();
                }
            }
            else
            {
                ImGui::Dummy(ImVec2(ImGui::GetFrameHeight(), ImGui::GetFrameHeight()));
            }

            ImGui::SameLine();
            const bool can_search = !m_remote_file_filter.empty() && !busy;
            if (!can_search)
            {
                ImGui::BeginDisabled();
            }
            const bool search_clicked = ImGui::Button("Search subfolders");
            if (!can_search)
            {
                ImGui::EndDisabled();
            }
            if ((search_clicked || (filter_enter && !m_remote_file_filter.empty())) && !busy)
            {
                RunRemoteSearch();
            }

            ImGui::SameLine();
            const auto& presets = type_filter_presets();
            if (m_type_filter < 0 || m_type_filter >= static_cast<int>(presets.size()))
            {
                m_type_filter = 0;
            }
            ImGui::SetNextItemWidth(220.0f);
            PushComboStyles();
            if (ImGui::BeginCombo("##remote_type_filter", presets[m_type_filter].label))
            {
                for (int i = 0; i < static_cast<int>(presets.size()); ++i)
                {
                    bool sel = (m_type_filter == i);
                    if (ImGui::Selectable(presets[i].label, sel))
                    {
                        m_type_filter = i;
                    }
                    if (sel)
                    {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }
            PopComboStyles();

            ImGui::SameLine();
            ImGui::Checkbox("Show hidden", &m_show_hidden);
        }
        ImGui::EndChild();
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(2);

        // Status line: busy, search context or error.
        if (busy)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, accent);
            ImGui::TextUnformatted(m_in_search_mode || !m_search_root.empty() ? "Searching..."
                                                                             : "Loading...");
            ImGui::PopStyleColor();
        }
        else if (!m_browser_error.empty())
        {
            ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.35f, 1.0f), "%s", m_browser_error.c_str());
        }
        else if (m_in_search_mode)
        {
            ImGui::TextDisabled("%zu match%s for \"%s\" in %s", m_search_results.size(),
                m_search_results.size() == 1 ? "" : "es", m_remote_file_filter.c_str(),
                m_search_root.c_str());
            ImGui::SameLine();
            if (ImGui::SmallButton("Clear search"))
            {
                m_in_search_mode = false;
                m_search_results.clear();
                m_remote_file_filter.clear();
                m_selected_name.clear();
            }
        }
        else
        {
            ImGui::NewLine();
        }

        // File listing table.
        const float footer_card_height = 48.0f;
        const float footer_reserve     = footer_card_height + style.ItemSpacing.y;

        // Filtered index list over the active source (hidden, type and name
        // filter). Sorted below once the sort spec is known, and reused by the
        // footer and keyboard handling.
        const std::string needle = Core::String::to_lower_copy(m_remote_file_filter);
        std::vector<size_t> visible;
        for (size_t i = 0; i < source.size(); ++i)
        {
            const std::string bn = posix_base_name(source[i].name);
            if (!m_show_hidden && !bn.empty() && bn[0] == '.')
            {
                continue;
            }
            if (!passes_type(source[i]))
            {
                continue;
            }
            if (!needle.empty())
            {
                const std::string& hay = m_in_search_mode ? source[i].name : bn;
                if (Core::String::to_lower_copy(hay).find(needle) == std::string::npos)
                {
                    continue;
                }
            }
            visible.push_back(i);
        }

        const ImGuiTableFlags table_flags =
            ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg |
            ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_Resizable |
            ImGuiTableFlags_Sortable;

        ImGui::PushStyleVar(ImGuiStyleVar_CellPadding,
                            ImVec2(style.CellPadding.x, style.CellPadding.y + 3.0f));
        if (ImGui::BeginTable("RemoteFiles", 4, table_flags, ImVec2(0, -footer_reserve)))
        {
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableSetupColumn("Name",
                ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_DefaultSort);
            ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, 90.0f);
            ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 96.0f);
            ImGui::TableSetupColumn("Modified", ImGuiTableColumnFlags_WidthFixed, 150.0f);
            ImGui::TableHeadersRow();

            // Selected and hovered rows use the accent color.
            ImGui::PushStyleColor(ImGuiCol_Header, accent);
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, accent_hover);
            ImGui::PushStyleColor(ImGuiCol_HeaderActive, accent_active);

            // Sort the visible list; directories always sort before files.
            if (ImGuiTableSortSpecs* specs = ImGui::TableGetSortSpecs())
            {
                const int  col = specs->SpecsCount > 0 ? specs->Specs[0].ColumnIndex : 0;
                const bool asc = specs->SpecsCount == 0 ||
                                 specs->Specs[0].SortDirection != ImGuiSortDirection_Descending;
                std::sort(visible.begin(), visible.end(), [&](size_t a, size_t b) {
                    const auto& fa = source[a];
                    const auto& fb = source[b];
                    if (fa.is_dir != fb.is_dir)
                    {
                        return fa.is_dir;
                    }
                    int cmp = 0;
                    if (col == 1)
                    {
                        cmp = (fa.size < fb.size) ? -1 : (fa.size > fb.size ? 1 : 0);
                    }
                    else if (col == 2)
                    {
                        cmp = Core::String::to_lower_copy(posix_file_extension(fa.name))
                                  .compare(Core::String::to_lower_copy(
                                      posix_file_extension(fb.name)));
                    }
                    else if (col == 3)
                    {
                        cmp = (fa.time < fb.time) ? -1 : (fa.time > fb.time ? 1 : 0);
                    }
                    else
                    {
                        cmp = 0;
                    }
                    if (cmp == 0)
                    {
                        cmp = Core::String::to_lower_copy(posix_base_name(fa.name))
                                  .compare(Core::String::to_lower_copy(posix_base_name(fb.name)));
                    }
                    return asc ? cmp < 0 : cmp > 0;
                });
            }

            // Resolve which visible row the remembered selection maps to.
            const bool parent_selected = (m_selected_name == "..");
            int        selected_visible = -1;
            for (size_t vi = 0; vi < visible.size(); ++vi)
            {
                if (source[visible[vi]].name == m_selected_name)
                {
                    selected_visible = static_cast<int>(vi);
                    break;
                }
            }

            auto row_icon = [&](const char* glyph, ImU32 color) {
                ImGui::PushFont(icon_font, ImGui::GetFontSize());
                ImGui::PushStyleColor(ImGuiCol_Text, color);
                ImGui::TextUnformatted(glyph);
                ImGui::PopStyleColor();
                ImGui::PopFont();
                ImGui::SameLine(0, style.ItemInnerSpacing.x);
            };

            // Synthetic ".." parent row (directory mode, not at the root).
            if (!m_in_search_mode && !is_posix_root_path(m_browser_dir))
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                if (ImGui::Selectable("##parent", parent_selected,
                        ImGuiSelectableFlags_SpanAllColumns |
                        ImGuiSelectableFlags_AllowDoubleClick))
                {
                    m_selected_name = "..";
                    if (ImGui::IsMouseDoubleClicked(0))
                        NavigateBrowserTo(posix_parent_path(m_browser_dir), true);
                }
                const ImU32 c = parent_selected ? text_on_accent : accent;
                ImGui::SameLine(0, 0);
                row_icon(ICON_FOLDER, c);
                ImGui::PushStyleColor(ImGuiCol_Text, c);
                ImGui::TextUnformatted("..");
                ImGui::PopStyleColor();
                ImGui::TableSetColumnIndex(1); ImGui::TextDisabled("-");
                ImGui::TableSetColumnIndex(2); ImGui::TextDisabled("Parent");
                ImGui::TableSetColumnIndex(3); ImGui::TextDisabled("-");
            }

            if (visible.empty() && !busy)
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::PushStyleColor(ImGuiCol_Text, text_dim);
                ImGui::TextUnformatted(
                    m_in_search_mode ? "No matches for the current search."
                    : (!m_remote_file_filter.empty() || m_type_filter > 0)
                          ? "No items match the current filter."
                          : "This remote folder is empty.");
                ImGui::PopStyleColor();
            }

            for (size_t vi = 0; vi < visible.size(); ++vi)
            {
                const auto& f = source[visible[vi]];
                const bool  row_selected = (selected_visible == static_cast<int>(vi));

                // Full remote path of this entry (for copy-path + hover tooltip).
                const std::string full_path =
                    m_in_search_mode
                        ? ((!f.name.empty() && f.name[0] == '/') ? f.name
                                                                 : join_posix_path(m_search_root, f.name))
                        : join_posix_path(m_browser_dir, f.name);

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);

                std::string sel_id = "##sel" + std::to_string(vi);
                if (ImGui::Selectable(sel_id.c_str(), row_selected,
                        ImGuiSelectableFlags_SpanAllColumns |
                        ImGuiSelectableFlags_AllowDoubleClick))
                {
                    m_selected_name = f.name;
                    if (ImGui::IsMouseDoubleClicked(0))
                    {
                        ActivateBrowserEntry(f);
                    }
                }
                const bool row_hovered = ImGui::IsItemHovered();

                // Right-click menu: Open / Copy path.
                if (ImGui::BeginPopupContextItem((std::string("##ctx") + std::to_string(vi)).c_str()))
                {
                    m_selected_name = f.name;
                    if (IconMenuItem(ICON_OPEN, f.is_dir ? "Open folder" : "Open"))
                        ActivateBrowserEntry(f);
                    if (IconMenuItem(ICON_COPY, "Copy path"))
                        ImGui::SetClipboardText(full_path.c_str());
                    ImGui::EndPopup();
                }

                if (m_scroll_to_selected && row_selected)
                {
                    ImGui::SetScrollHereY();
                }

                // Selected rows draw on the accent color; otherwise folders are
                // accented and files use the default text with a dimmed icon.
                const ImU32 icon_color =
                    row_selected ? text_on_accent : (f.is_dir ? accent : text_dim);
                const ImU32 name_color =
                    row_selected ? text_on_accent : (f.is_dir ? accent : text_main);
                ImGui::SameLine(0, 0);
                row_icon(f.is_dir ? ICON_FOLDER : ICON_DOCUMENT, icon_color);
                // Search hits show the full path; directory rows show the name.
                const std::string display =
                    m_in_search_mode ? f.name : (f.is_dir ? f.name + "/" : f.name);
                ImGui::PushStyleColor(ImGuiCol_Text, name_color);
                ElidedText(display.c_str(), ImGui::GetContentRegionAvail().x, 0.0f,
                           Alignment_Left, false);
                ImGui::PopStyleColor();
                if (row_hovered)
                {
                    SetTooltipStyled("%s", full_path.c_str());
                }

                ImGui::TableSetColumnIndex(1);
                if (f.is_dir)
                {
                    ImGui::TextDisabled("-");
                }
                else
                {
                    ImGui::TextUnformatted(format_file_size(f.size).c_str());
                }
                ImGui::TableSetColumnIndex(2);
                ImGui::TextUnformatted(type_label(f).c_str());
                ImGui::TableSetColumnIndex(3);
                ImGui::TextUnformatted(format_file_time(f.time).c_str());
            }

            ImGui::PopStyleColor(3);
            ImGui::EndTable();
        }
        ImGui::PopStyleVar();
        m_scroll_to_selected = false;

        // Footer: item count, selection summary, Cancel and Open.
        std::string selection_label;
        bool        selection_is_dir = true;
        if (m_selected_name == "..")
        {
            selection_label = "..";
        }
        else if (!m_selected_name.empty())
        {
            for (const auto& e : source)
            {
                if (e.name == m_selected_name)
                {
                    selection_label  = posix_base_name(e.name);
                    selection_is_dir = e.is_dir;
                    break;
                }
            }
        }

        // Opens the current selection: parent goes up, a directory is browsed
        // into, a file is committed and the dialog closes.
        auto commit_selection = [&]() {
            if (m_selected_name == "..")
            {
                NavigateBrowserTo(posix_parent_path(m_browser_dir), true);
                return;
            }
            for (const auto& e : source)
            {
                if (e.name == m_selected_name)
                {
                    ActivateBrowserEntry(e);
                    return;
                }
            }
        };

        bool open_pressed = false;
        ImGui::PushStyleColor(ImGuiCol_ChildBg, settings.GetColor(Colors::kBgFrame));
        ImGui::PushStyleColor(ImGuiCol_Border, settings.GetColor(Colors::kPanelBorderSubtle));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 8.0f));
        ImGui::BeginChild("RemoteExplorerFooter", ImVec2(0.0f, footer_card_height), true,
                          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        {
            ImGui::AlignTextToFramePadding();
            ImGui::TextDisabled("%zu item%s", visible.size(), visible.size() == 1 ? "" : "s");

            ImGui::SameLine(0, style.ItemSpacing.x * 2.0f);
            ImGui::TextDisabled("Selected:");
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Text, selection_label.empty() ? text_dim : accent);
            const float selected_width = ImGui::GetContentRegionAvail().x - 260.0f;
            ElidedText(selection_label.empty() ? "(none)" : selection_label.c_str(),
                       selected_width > 80.0f ? selected_width : 80.0f, 0.0f,
                       Alignment_Left, true);
            ImGui::PopStyleColor();

            const float button_width = 120.0f;
            const float total_width  = button_width * 2.0f + style.ItemSpacing.x;
            ImGui::SameLine();
            ImGui::SetCursorPosX(ImGui::GetWindowContentRegionMax().x - total_width);

            if (ImGui::Button("Cancel", ImVec2(button_width, 0)))
            {
                m_orchestrator.reset();
                m_browser_busy = false;
                ImGui::CloseCurrentPopup();
                m_show_remote_filesystem_popup = false;
            }
            ImGui::SameLine();

            const bool can_open = !m_selected_name.empty();
            if (!can_open)
            {
                ImGui::BeginDisabled();
            }
            ImGui::PushStyleColor(ImGuiCol_Button, accent);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, accent_hover);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, accent_active);
            ImGui::PushStyleColor(ImGuiCol_Text, text_on_accent);
            open_pressed =
                ImGui::Button(selection_is_dir ? "Open" : "Select", ImVec2(button_width, 0));
            ImGui::PopStyleColor(4);
            if (!can_open)
            {
                ImGui::EndDisabled();
            }
        }
        ImGui::EndChild();
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(2);

        // Keyboard: arrows move the selection, Enter opens, Backspace goes up.
        // Suppressed while a text field (address or filter) is being edited.
        const bool shortcuts_active =
            ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
            !ImGui::GetIO().WantTextInput;

        if (shortcuts_active && !visible.empty())
        {
            int cur = -1;
            for (size_t vi = 0; vi < visible.size(); ++vi)
            {
                if (source[visible[vi]].name == m_selected_name)
                {
                    cur = static_cast<int>(vi);
                    break;
                }
            }
            const int last = static_cast<int>(visible.size()) - 1;
            constexpr int PAGE = 10;
            int next = cur;
            if (ImGui::IsKeyPressed(ImGuiKey_DownArrow))
            {
                next = (cur < 0) ? 0 : std::min(cur + 1, last);
            }
            else if (ImGui::IsKeyPressed(ImGuiKey_UpArrow))
            {
                next = (cur <= 0) ? 0 : cur - 1;
            }
            else if (ImGui::IsKeyPressed(ImGuiKey_PageDown))
            {
                next = (cur < 0) ? 0 : std::min(cur + PAGE, last);
            }
            else if (ImGui::IsKeyPressed(ImGuiKey_PageUp))
            {
                next = (cur <= 0) ? 0 : std::max(cur - PAGE, 0);
            }
            else if (ImGui::IsKeyPressed(ImGuiKey_Home))
            {
                next = 0;
            }
            else if (ImGui::IsKeyPressed(ImGuiKey_End))
            {
                next = last;
            }

            if (next != cur && next >= 0 && next <= last)
            {
                m_selected_name      = source[visible[next]].name;
                m_scroll_to_selected = true;
            }
        }

        if (shortcuts_active && (ImGui::IsKeyPressed(ImGuiKey_Enter, false) ||
                                 ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false)))
        {
            commit_selection();
        }
        else if (shortcuts_active && !busy && ImGui::IsKeyPressed(ImGuiKey_Backspace, false) &&
                 !m_in_search_mode && !is_posix_root_path(m_browser_dir))
        {
            NavigateBrowserTo(posix_parent_path(m_browser_dir), true);
        }
        else if (open_pressed)
        {
            commit_selection();
        }

        if (m_should_close_browser_popup)
        {
            ImGui::CloseCurrentPopup();
            m_should_close_browser_popup = false;
        }

        ImGui::EndPopup();
    }

    popup_style.PopStyles();
}


}  // namespace View
}  // namespace RocProfVis
