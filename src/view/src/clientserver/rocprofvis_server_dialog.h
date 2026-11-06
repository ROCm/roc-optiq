// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_server_connection.h"

#include <cstdint>
#include <future>
#include <memory>
#include <string>
#include <vector>

namespace RocProfVis
{
namespace View
{

/**
 * @brief File > Open from Server: connects to a roc-optiq-server, browses its
 * file system for a trace to open or two to compare, and uploads local traces.
 *
 * Chosen traces open through AppWindow as optiq:// URIs, so they land in recent
 * files and project files like any other trace. Connecting and uploading run off
 * the UI thread.
 */
class ServerDialog
{
public:
    ServerDialog();

    void Show();
    void Render();

private:
    struct ConnectResult
    {
        std::shared_ptr<ServerConnection> connection;
        std::string                       error;
    };

    void RenderConnect();
    void RenderBrowser();
    void RenderEntries();
    void RenderUploadStatus();
    void StartConnect();
    void PollConnect();
    void RequestListing(const std::string& path);
    void PollListing();
    void BrowseForUpload();
    void StartUpload(const std::string& local_path);
    void PollUpload();
    void Select(const std::string& path, bool add);
    bool IsSelected(const std::string& path) const;

    bool m_should_open;
    char m_host[256];
    int  m_port;
    bool m_tls;
    char m_token[256];
    char m_ca_file[1024];
    bool m_verify_certificate;
    char m_path[1024];

    std::future<ConnectResult>        m_connect_future;
    std::shared_ptr<ServerConnection> m_connection;
    std::string                       m_error_message;
    uint64_t                          m_pending_listing;
    bool                              m_has_listing;
    ServerDirectoryListing            m_listing;
    // One to open, or two to compare, in the order picked.
    std::vector<std::string>          m_selected_files;

    std::shared_ptr<ServerConnection> m_upload_connection;
    uint64_t                          m_upload_id;
    std::string                       m_upload_name;
    ServerUploadStatus                m_upload_status;

    // Opened after the popup ends, so a failure can raise its own message dialog.
    std::vector<std::string> m_open_uris;
};

}  // namespace View
}  // namespace RocProfVis
