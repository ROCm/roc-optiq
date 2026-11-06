// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_data_provider.h"

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace jt
{
class Json;
}

namespace RocProfVis
{
namespace Server
{

/**
 * @brief One trace a client opened: the controller plus a DataProvider that
 * drives it exactly as the UI's would.
 *
 * Requests arrive as protocol messages and become DataProvider calls with the
 * same parameters, so they occupy the same request ids the client assigned.
 * When the provider finishes one, the result is read back out of the provider
 * (or its models) and sent as that request's response. Owned and driven by a
 * single session worker thread.
 */
class ServerTrace
{
public:
    // Sends one JSON message for this trace; the session adds the trace id.
    using SendMessageFunction = std::function<bool(jt::Json& message)>;
    // Sends one file chunk for request_id.
    using SendChunkFunction =
        std::function<bool(uint64_t request_id, const char* data, size_t size)>;

    ServerTrace(SendMessageFunction send_message, SendChunkFunction send_chunk);
    ~ServerTrace();

    ServerTrace(const ServerTrace&)            = delete;
    ServerTrace& operator=(const ServerTrace&) = delete;

    /*
     * Opens the trace and starts loading it. The load's progress and result
     * follow as messages for the load request id.
     * @param paths: One trace, or two or more system traces to compare.
     * @param is_compute: Set to the kind of trace on success.
     * @param error: Set to a message for the user on failure.
     */
    bool Open(const std::vector<std::string>& paths, const std::string& config_path,
              bool& is_compute, std::string& error);

    void HandleRequest(uint64_t request_id, const std::string& operation, jt::Json& params);
    void Cancel(uint64_t request_id);
    void SetSelected();
    // Python scripts run as the server's user, so they are off unless allowed.
    void SetAllowScripts(bool allow);

    // Pumps the provider. True while it has requests in flight.
    bool Update();

private:
    void OnRequestCompleted(const View::RequestInfo& req);
    void OnRequestProgress(const View::RequestInfo& req, uint64_t percent,
                           const std::string& message);
    void SendProgress(uint64_t request_id, uint64_t percent, const std::string& message);
    void SendResponse(uint64_t request_id, View::RequestType request_type, uint64_t code,
                      const std::string& error, jt::Json& data);
    void Reject(uint64_t request_id, const std::string& operation, const std::string& error);
    // Streams the file a request produced, then deletes it. False if it could not
    // be read in full.
    bool SendFile(uint64_t request_id, const std::string& path);
    std::string MakeTemporaryPath(const std::string& extension);
    std::vector<std::pair<uint32_t, std::string>> ReadMemoryChartLayouts();

    SendMessageFunction      m_send_message;
    SendChunkFunction        m_send_chunk;
    View::DataProvider       m_data_provider;
    rocprofvis_controller_t* m_controller;
    std::string              m_path;
    uint64_t                 m_load_request_id;
    uint64_t                 m_load_progress_percent;
    std::string              m_load_progress_message;
    bool                     m_allow_scripts;
    // Server-side files a request writes before they are streamed to the client.
    std::map<uint64_t, std::string> m_output_files;
};

}  // namespace Server
}  // namespace RocProfVis
