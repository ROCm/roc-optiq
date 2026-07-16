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
#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

namespace RocProfVis
{
namespace View
{

namespace
{
    // Wraps s in single quotes for safe embedding in a POSIX shell command,
    // escaping any embedded single quotes.
    std::string shell_single_quote(const std::string& s)
    {
        std::string out = "'";
        for (char c : s)
        {
            if (c == '\'')
            {
                out += "'\\''";
            }
            else
            {
                out += c;
            }
        }
        out += "'";
        return out;
    }

    // Builds a `find` command listing (type, size, mtime, path) for every entry
    // whose name contains query (case-insensitive) under dir. GNU find's
    // `-printf` is assumed, which is safe for the Linux hosts that remote
    // profiling targets.
    // Caps the number of matches so a search under a huge tree cannot flood the
    // UI with output.
    constexpr int SEARCH_RESULT_LIMIT = 5000;

    std::string build_find_command(const std::string& query, const std::string& dir)
    {
        std::string pattern = "*" + query + "*";
        std::string cmd = "find " + shell_single_quote(dir.empty() ? "." : dir) +
            " -iname " + shell_single_quote(pattern) +
            " -printf '%y\\t%s\\t%T@\\t%p\\n' 2>/dev/null | head -n " +
            std::to_string(SEARCH_RESULT_LIMIT);
        return cmd;
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
, m_file_browser()
, m_search_in_progress(false)
{
    m_connection_store.Load();
    if(!m_connection_store.Empty())
    {
        m_selected_connection_id = m_connection_store.List().front().id;
    }
    ApplySelectedConnection();
    SetupFileBrowserCallbacks();
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
                m_search_in_progress = false;
                m_file_browser.Open(m_uri->GetRemoteBrowsingPathString());
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

    // Auth prompts / host-key requests, and download/output popups. The
    // download/output popups are suppressed while the file browser is open so
    // they do not fight with it for the modal stack (the browser's own
    // recursive search reuses the execute path and would otherwise pop the
    // stdout window).
    RenderSshAuthModal(m_orchestrator ? m_orchestrator->GetSession() : nullptr);
    if(!m_file_browser.IsOpen())
    {
        RenderProgressPopup();
        RenderOutputPopup();
    }
    PollFileBrowser();
    m_file_browser.Render();
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
SshTestDialog::SetupFileBrowserCallbacks()
{
    RemoteFileBrowser::Callbacks callbacks;

    // Navigate / refresh: point the browsing path at the requested directory and
    // browse it over the (reused) SSH session.
    callbacks.request_listing = [this](const std::string& path)
    {
        m_uri->SetRemoteBrowsingPathString(path.c_str());
        BrowseRemotePath();
    };

    // Recursive search: run a remote `find` whose stdout is parsed into results.
    callbacks.request_search = [this](const std::string& query, const std::string& dir)
    {
        EnsureOrchestrator();
        m_search_in_progress = true;
        m_orchestrator->SearchPath(build_find_command(query, dir));
    };

    // File chosen: commit the path and drop the browse session.
    callbacks.on_file_chosen = [this](const std::string& file_path)
    {
        m_uri->SetRemoteResultPathString(file_path.c_str());
        m_orchestrator.reset();
        m_search_in_progress = false;
    };

    // Cancelled: drop the browse session.
    callbacks.on_cancel = [this]()
    {
        m_orchestrator.reset();
        m_search_in_progress = false;
    };

    m_file_browser.SetCallbacks(std::move(callbacks));
}

void
SshTestDialog::EnsureOrchestrator()
{
    // Reuse one orchestrator (and its live SshSession) across folder navigation
    // and searches so the connection stays authenticated; the directory-path
    // callback mirrors the browsed directory back into m_uri, and the
    // search-results callback receives raw `find` stdout.
    if (!m_orchestrator)
    {
        m_orchestrator = std::make_unique<RemoteTraceOrchestrator>(
            m_uri,
            [this](const std::string& path)
            {
                m_uri->SetCurrentDirectoryPath(path.c_str());
            });
        m_orchestrator->SetSearchResultsCallback(
            [this](const std::string& output)
            {
                HandleSearchResults(output);
            });
    }
}

void
SshTestDialog::BrowseRemotePath()
{
    EnsureOrchestrator();
    m_orchestrator->BrowsePath();
}

void
SshTestDialog::PollFileBrowser()
{
    if (!m_file_browser.IsOpen())
    {
        return;
    }

    SshSession* ssh_session = m_orchestrator ? m_orchestrator->GetSession() : nullptr;
    if (ssh_session)
    {
        // A completed directory listing arrives on the RemoteDir snapshot.
        if (auto fetch = ssh_session->GetRemoteDir()->ConsumeIfUpdated())
        {
            m_file_browser.SetListing(m_uri->GetRemoteBrowsingPathString(), *fetch);
            m_search_in_progress = false;
        }
    }

    // Surface a browse/search failure into the browser. Fail() leaves a
    // descriptive status and clears IsRunning(); success leaves "Done.".
    if (m_orchestrator && !m_orchestrator->IsRunning())
    {
        const std::string& status = m_orchestrator->GetStatusMessage();
        if (!status.empty() && status != "Done.")
        {
            m_file_browser.SetError(status);
            m_file_browser.SetBusy(false);
        }
    }
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
        if (t1 == std::string::npos) { continue; }
        std::string::size_type t2 = line.find('\t', t1 + 1);
        if (t2 == std::string::npos) { continue; }
        std::string::size_type t3 = line.find('\t', t2 + 1);
        if (t3 == std::string::npos) { continue; }

        std::string type_field = line.substr(0, t1);
        std::string size_field = line.substr(t1 + 1, t2 - t1 - 1);
        std::string time_field = line.substr(t2 + 1, t3 - t2 - 1);
        std::string path_field = line.substr(t3 + 1);

        RemoteDir::FileEntry entry;
        entry.name   = path_field;
        entry.is_dir = (!type_field.empty() && type_field[0] == 'd');
        entry.size   = static_cast<uint64_t>(std::strtoull(size_field.c_str(), nullptr, 10));
        entry.time   = static_cast<uint64_t>(std::strtod(time_field.c_str(), nullptr));
        results.push_back(std::move(entry));
    }

    m_file_browser.SetSearchResults(std::move(results));
    m_search_in_progress = false;
}


}  // namespace View
}  // namespace RocProfVis
