// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_server_dialog.h"
#include "protocol/rocprofvis_protocol.h"
#include "protocol/rocprofvis_protocol_uri.h"
#include "rocprofvis_appwindow.h"
#include "rocprofvis_render_scheduler.h"
#include "rocprofvis_settings_manager.h"
#include "widgets/rocprofvis_gui_helpers.h"
#include "widgets/rocprofvis_notification_manager.h"
#include "widgets/rocprofvis_widget.h"

#include "imgui.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>

namespace RocProfVis
{
namespace View
{

namespace
{

constexpr const char* SERVER_DIALOG_NAME     = "Open from Server##_server_dialog";
constexpr const char* DEFAULT_SERVER_HOST    = "localhost";
constexpr const char* UPLOAD_NOTIFICATION_ID = "server_upload";
constexpr float       BROWSER_LIST_HEIGHT    = 320.0f;
constexpr double      BYTES_PER_KILOBYTE     = 1024.0;
constexpr size_t      MAX_SELECTED_FILES     = 2;

std::string
format_size(uint64_t bytes)
{
    const char* units[] = { "B", "KB", "MB", "GB", "TB" };
    double      size    = static_cast<double>(bytes);
    size_t      unit    = 0;
    while(size >= BYTES_PER_KILOBYTE && unit + 1 < sizeof(units) / sizeof(units[0]))
    {
        size /= BYTES_PER_KILOBYTE;
        unit++;
    }
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), unit == 0 ? "%.0f %s" : "%.1f %s", size,
                  units[unit]);
    return buffer;
}

std::string
join_server_path(const std::string& directory, const std::string& name)
{
    if(directory.empty() || directory.back() == '/' || directory.back() == '\\')
    {
        return directory + name;
    }
    return directory + "/" + name;
}

void
copy_to_buffer(char* buffer, size_t size, const std::string& text)
{
    std::snprintf(buffer, size, "%s", text.c_str());
}

std::string
upload_progress_text(const std::string& name, const ServerUploadStatus& status)
{
    const int percent =
        status.total == 0 ? 100 : static_cast<int>(status.sent * 100 / status.total);
    return "Uploading " + name + ": " + std::to_string(percent) + "% (" +
           format_size(status.sent) + " of " + format_size(status.total) + ")";
}

}  // namespace

ServerDialog::ServerDialog()
: m_should_open(false)
, m_port(DEFAULT_SERVER_PORT)
, m_tls(false)
, m_verify_certificate(true)
, m_pending_listing(0)
, m_has_listing(false)
, m_upload_id(0)
{
    copy_to_buffer(m_host, sizeof(m_host), DEFAULT_SERVER_HOST);
    m_token[0]   = '\0';
    m_ca_file[0] = '\0';
    m_path[0]    = '\0';
}

void
ServerDialog::Show()
{
    m_should_open = true;
    m_error_message.clear();
    m_selected_files.clear();
    if(!m_connection || m_connection->GetState() == ServerConnectionState::kLost)
    {
        std::vector<std::shared_ptr<ServerConnection>> live = ServerConnection::GetAll();
        m_connection  = live.empty() ? nullptr : live.front();
        m_has_listing = false;
    }
    if(m_connection && !m_has_listing)
    {
        RequestListing(m_path[0] ? m_path : m_connection->GetHomeDirectory());
    }
}

void
ServerDialog::Render()
{
    PollConnect();
    PollListing();
    PollUpload();
    if(m_should_open)
    {
        ImGui::OpenPopup(SERVER_DIALOG_NAME);
        m_should_open = false;
    }

    if(ImGui::IsPopupOpen(SERVER_DIALOG_NAME, ImGuiPopupFlags_None))
    {
        PopUpStyle popup_style;
        popup_style.PushPopupStyles();
        popup_style.PushTitlebarColors();
        popup_style.CenterPopup();
        ImGui::SetNextWindowSize(
            GetResponsiveWindowSize(ImVec2(760.0f, 0.0f), ImVec2(520.0f, 0.0f)));

        if(ImGui::BeginPopupModal(SERVER_DIALOG_NAME, nullptr,
                                  ImGuiWindowFlags_AlwaysAutoResize |
                                      ImGuiWindowFlags_NoSavedSettings))
        {
            if(m_connection && m_connection->GetState() != ServerConnectionState::kLost)
            {
                RenderBrowser();
            }
            else
            {
                RenderConnect();
            }

            if(!m_error_message.empty())
            {
                ImGui::Spacing();
                ImGui::PushStyleColor(
                    ImGuiCol_Text, SettingsManager::GetInstance().GetColor(Colors::kTextError));
                ImGui::PushTextWrapPos(ImGui::GetCursorPosX() +
                                       ImGui::GetContentRegionAvail().x);
                ImGui::TextUnformatted(m_error_message.c_str());
                ImGui::PopTextWrapPos();
                ImGui::PopStyleColor();
            }
            if(!m_open_uris.empty())
            {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        popup_style.PopStyles();
    }

    if(!m_open_uris.empty())
    {
        std::vector<std::string> uris = std::move(m_open_uris);
        m_open_uris.clear();
#ifdef ROCPROFVIS_ENABLE_TRACE_COMPARE
        if(uris.size() > 1)
        {
            AppWindow::GetInstance()->OpenCompare(uris[0], uris[1]);
            return;
        }
#endif
        AppWindow::GetInstance()->OpenFile(uris.front());
    }
}

void
ServerDialog::RenderConnect()
{
    SectionTitle("Connect to a roc-optiq-server", false);
    ImGui::TextWrapped(
        "Start roc-optiq-server on the machine that has the traces. For another "
        "machine, forward its port over SSH and connect to localhost, or start the "
        "server with --tls-cert and --tls-key and connect to it directly:");
    ImGui::Spacing();
    char tunnel[128];
    std::snprintf(tunnel, sizeof(tunnel), "ssh -L %d:localhost:%d user@host", m_port,
                  m_port);
    ImGui::TextDisabled("%s", tunnel);
    ImGui::Spacing();

    const bool connecting = m_connect_future.valid();
    ImGui::BeginDisabled(connecting);
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.6f);
    bool submit = ImGui::InputText("Host", m_host, sizeof(m_host),
                                   ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.3f);
    submit = ImGui::InputInt("Port", &m_port, 0, 0, ImGuiInputTextFlags_EnterReturnsTrue) ||
             submit;
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.6f);
    submit = ImGui::InputText("Access token", m_token, sizeof(m_token),
                              ImGuiInputTextFlags_Password |
                                  ImGuiInputTextFlags_EnterReturnsTrue) ||
             submit;
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if(ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Printed by roc-optiq-server when it starts. Leave empty to use "
                          "ROCPROFVIS_SERVER_TOKEN, or, for a server on this machine, "
                          "the token it saved.");
    }
    ImGui::Checkbox("TLS", &m_tls);
    if(m_tls)
    {
        ImGui::SameLine();
        ImGui::Checkbox("Verify the certificate", &m_verify_certificate);
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.6f);
        ImGui::InputText("CA certificate", m_ca_file, sizeof(m_ca_file));
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if(ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("A PEM file to trust, such as the server's own self-signed "
                              "certificate. Empty uses ROCPROFVIS_SERVER_CA or the "
                              "system's certificate authorities.");
        }
    }
    ImGui::EndDisabled();

    ImGui::Separator();
    ImGui::BeginDisabled(connecting);
    if(ImGui::Button("Connect") || (submit && !connecting))
    {
        StartConnect();
    }
    ImGui::EndDisabled();
    ImGui::SetItemDefaultFocus();
    ImGui::SameLine();
    if(ImGui::Button("Cancel"))
    {
        ImGui::CloseCurrentPopup();
    }
    if(connecting)
    {
        ImGui::SameLine();
        ImGui::TextDisabled("Connecting...");
    }
}

void
ServerDialog::RenderBrowser()
{
    const bool reconnecting = m_connection->GetState() == ServerConnectionState::kReconnecting;
    SectionTitle((m_connection->GetServerVersion() + " at " + m_connection->GetAddress() +
                  (m_connection->IsTls() ? " (TLS)" : "") +
                  (reconnecting ? " - reconnecting..." : ""))
                     .c_str(),
                 false);
    if(ImGui::Button("Disconnect"))
    {
        m_connection->Disconnect();
        m_connection.reset();
        m_has_listing = false;
        m_selected_files.clear();
        return;
    }
    ImGui::Spacing();

    if(ImGui::Button("Up") && m_has_listing && !m_listing.parent.empty())
    {
        RequestListing(m_listing.parent);
    }
    ImGui::SameLine();
    if(ImGui::Button("Home"))
    {
        RequestListing(m_connection->GetHomeDirectory());
    }
    ImGui::SameLine();
    if(ImGui::Button("Refresh") && m_has_listing)
    {
        RequestListing(m_listing.path);
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    if(ImGui::InputText("##server_path", m_path, sizeof(m_path),
                        ImGuiInputTextFlags_EnterReturnsTrue))
    {
        RequestListing(m_path);
    }

    RenderEntries();

    const ServerFeatures features = m_connection->GetFeatures();
    if(m_selected_files.empty())
    {
        ImGui::TextDisabled(features.compare
                                ? "Select a trace (.db, .rpd) on the server; Ctrl+click a "
                                  "second one to compare them."
                                : "Select a trace (.db, .rpd) on the server.");
    }
    else
    {
        for(const std::string& file : m_selected_files)
        {
            ImGui::TextDisabled("%s", file.c_str());
        }
    }
    RenderUploadStatus();

    ImGui::Separator();
    ImGui::BeginDisabled(m_selected_files.size() != 1);
    if(ImGui::Button("Open"))
    {
        m_open_uris = { make_server_uri(m_connection->GetHost(), m_connection->GetPort(),
                                        m_connection->IsTls(), m_selected_files.front()) };
    }
    ImGui::EndDisabled();
#ifdef ROCPROFVIS_ENABLE_TRACE_COMPARE
    ImGui::SameLine();
    ImGui::BeginDisabled(m_selected_files.size() != MAX_SELECTED_FILES || !features.compare);
    if(ImGui::Button("Compare"))
    {
        for(const std::string& file : m_selected_files)
        {
            m_open_uris.push_back(make_server_uri(m_connection->GetHost(),
                                                  m_connection->GetPort(),
                                                  m_connection->IsTls(), file));
        }
    }
    ImGui::EndDisabled();
#endif
    ImGui::SameLine();
    ImGui::BeginDisabled(!features.upload || m_upload_id != 0);
    if(ImGui::Button("Upload a Local Trace..."))
    {
        BrowseForUpload();
    }
    ImGui::EndDisabled();
    if(!features.upload && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
    {
        ImGui::SetTooltip("This server was started with --no-upload.");
    }
    ImGui::SameLine();
    if(ImGui::Button("Close"))
    {
        ImGui::CloseCurrentPopup();
    }
}

void
ServerDialog::RenderEntries()
{
    ImGui::BeginChild("##server_entries", ImVec2(0.0f, BROWSER_LIST_HEIGHT), true);
    if(m_pending_listing != 0 && !m_has_listing)
    {
        ImGui::TextDisabled("Listing...");
    }
    else if(m_has_listing && !m_listing.ok)
    {
        ImGui::TextDisabled("Cannot list this folder: %s", m_listing.error.c_str());
    }
    else if(m_has_listing && ImGui::BeginTable("##server_entries_table", 2,
                                               ImGuiTableFlags_RowBg |
                                                   ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 0.8f);
        ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthStretch, 0.2f);
        std::string navigate_to;
        for(const ServerDirectoryEntry& entry : m_listing.entries)
        {
            const std::string path = join_server_path(m_listing.path, entry.name);
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            std::string label = entry.is_directory ? entry.name + "/" : entry.name;
            if(ImGui::Selectable(label.c_str(), !entry.is_directory && IsSelected(path),
                                 ImGuiSelectableFlags_SpanAllColumns |
                                     ImGuiSelectableFlags_AllowDoubleClick))
            {
                if(entry.is_directory)
                {
                    if(ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                    {
                        navigate_to = path;
                    }
                }
                else
                {
                    Select(path, ImGui::GetIO().KeyCtrl);
                    if(ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                    {
                        m_open_uris = { make_server_uri(m_connection->GetHost(),
                                                        m_connection->GetPort(),
                                                        m_connection->IsTls(), path) };
                    }
                }
            }
            ImGui::TableSetColumnIndex(1);
            if(!entry.is_directory)
            {
                ImGui::TextDisabled("%s", format_size(entry.size).c_str());
            }
        }
        ImGui::EndTable();
        if(!navigate_to.empty())
        {
            RequestListing(navigate_to);
        }
    }
    ImGui::EndChild();
}

void
ServerDialog::RenderUploadStatus()
{
    if(m_upload_id == 0)
    {
        return;
    }
    const float fraction =
        m_upload_status.total == 0
            ? 1.0f
            : static_cast<float>(static_cast<double>(m_upload_status.sent) /
                                 static_cast<double>(m_upload_status.total));
    ImGui::ProgressBar(fraction, ImVec2(ImGui::GetContentRegionAvail().x * 0.7f, 0.0f),
                       upload_progress_text(m_upload_name, m_upload_status).c_str());
    ImGui::SameLine();
    if(ImGui::Button("Cancel Upload") && m_upload_connection)
    {
        m_upload_connection->CancelUpload(m_upload_id);
    }
}

void
ServerDialog::StartConnect()
{
    m_error_message.clear();
    const std::string       host = m_host;
    const int               port = m_port;
    ServerConnectionOptions options;
    options.tls                = m_tls;
    options.token              = m_token;
    options.ca_file            = m_tls ? m_ca_file : "";
    options.verify_certificate = m_verify_certificate;
    m_connect_future = std::async(std::launch::async, [host, port, options]() {
        ConnectResult result;
        result.connection = ServerConnection::Get(host, port, options, result.error);
        return result;
    });
}

void
ServerDialog::PollConnect()
{
    if(!m_connect_future.valid())
    {
        return;
    }
    if(m_connect_future.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
    {
        // Connecting happens on a worker; keep the lazy loop awake for it.
        RenderScheduler::GetInstance().RequestRender();
        return;
    }
    ConnectResult result = m_connect_future.get();
    if(!result.connection)
    {
        m_error_message = result.error;
        return;
    }
    m_connection = result.connection;
    if(m_token[0])
    {
        // Traces opened from here look the token up by server.
        ServerConnection::SetToken(m_connection->GetHost(), m_connection->GetPort(),
                                   m_connection->IsTls(), m_token);
    }
    m_has_listing = false;
    RequestListing(m_connection->GetHomeDirectory());
}

void
ServerDialog::RequestListing(const std::string& path)
{
    if(m_connection)
    {
        m_pending_listing = m_connection->RequestDirectoryListing(path);
        m_has_listing     = false;
        m_selected_files.clear();
    }
}

void
ServerDialog::PollListing()
{
    if(m_connection && m_pending_listing != 0)
    {
        if(m_connection->TakeDirectoryListing(m_pending_listing, m_listing))
        {
            m_pending_listing = 0;
            m_has_listing     = true;
            if(m_listing.ok)
            {
                copy_to_buffer(m_path, sizeof(m_path), m_listing.path);
            }
        }
        else
        {
            // The listing arrives on another thread; keep the lazy loop awake for it.
            RenderScheduler::GetInstance().RequestRender();
        }
    }
}

void
ServerDialog::BrowseForUpload()
{
    FileFilter trace_filter;
    trace_filter.m_name       = "Trace Files";
    trace_filter.m_extensions = { "db", "rpd", "yaml" };
    AppWindow::GetInstance()->ShowOpenFileDialog(
        "Choose a Trace to Upload", { trace_filter }, "",
        [this](std::string file_path) { StartUpload(file_path); });
}

void
ServerDialog::StartUpload(const std::string& local_path)
{
    if(!m_connection || m_upload_id != 0 || local_path.empty())
    {
        return;
    }
    m_error_message.clear();
    m_upload_connection = m_connection;
    m_upload_name       = std::filesystem::path(local_path).filename().string();
    m_upload_status     = ServerUploadStatus();
    m_upload_id         = m_upload_connection->StartUpload(local_path);
    NotificationManager::GetInstance().ShowPersistent(
        UPLOAD_NOTIFICATION_ID, "Uploading " + m_upload_name + "...", NotificationLevel::Info);
}

void
ServerDialog::PollUpload()
{
    if(m_upload_id == 0 || !m_upload_connection)
    {
        return;
    }
    if(!m_upload_connection->GetUploadStatus(m_upload_id, m_upload_status))
    {
        m_upload_id = 0;
        NotificationManager::GetInstance().Hide(UPLOAD_NOTIFICATION_ID);
        return;
    }
    if(m_upload_status.state == ServerUploadStatus::State::kRunning)
    {
        const uint64_t percent = m_upload_status.total == 0
                                     ? 100
                                     : m_upload_status.sent * 100 / m_upload_status.total;
        NotificationManager::GetInstance().UpdateProgress(
            UPLOAD_NOTIFICATION_ID, percent, upload_progress_text(m_upload_name, m_upload_status));
        RenderScheduler::GetInstance().RequestRender();
        return;
    }

    NotificationManager::GetInstance().Hide(UPLOAD_NOTIFICATION_ID);
    if(m_upload_status.state == ServerUploadStatus::State::kDone)
    {
        NotificationManager::GetInstance().Show("Uploaded " + m_upload_name + " to " +
                                                    m_upload_status.server_path + ".",
                                                NotificationLevel::Success);
        m_open_uris = { make_server_uri(m_upload_connection->GetHost(),
                                        m_upload_connection->GetPort(),
                                        m_upload_connection->IsTls(),
                                        m_upload_status.server_path) };
        if(m_connection == m_upload_connection && m_has_listing)
        {
            RequestListing(m_listing.path);
        }
    }
    else
    {
        m_error_message = "Upload of " + m_upload_name + " failed: " + m_upload_status.error;
        NotificationManager::GetInstance().Show(m_error_message, NotificationLevel::Error);
    }
    m_upload_id = 0;
    m_upload_connection.reset();
}

void
ServerDialog::Select(const std::string& path, bool add)
{
    auto it = std::find(m_selected_files.begin(), m_selected_files.end(), path);
    if(!add)
    {
        m_selected_files = { path };
    }
    else if(it != m_selected_files.end())
    {
        m_selected_files.erase(it);
    }
    else
    {
        if(m_selected_files.size() >= MAX_SELECTED_FILES)
        {
            m_selected_files.erase(m_selected_files.begin());
        }
        m_selected_files.push_back(path);
    }
}

bool
ServerDialog::IsSelected(const std::string& path) const
{
    return std::find(m_selected_files.begin(), m_selected_files.end(), path) !=
           m_selected_files.end();
}

}  // namespace View
}  // namespace RocProfVis
