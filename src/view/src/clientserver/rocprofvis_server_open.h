// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include <memory>
#include <string>
#include <vector>

namespace RocProfVis
{
namespace View
{

class IDataChannel;

struct ServerOpenResult
{
    std::shared_ptr<IDataChannel> channel;
    bool                          is_compute = false;
    std::string                   error;
};

/*
 * Returns uri without its ?token=, remembering the token for that server, or uri
 * unchanged when it is not a server URI. Everything the UI keys a trace by
 * (project id, recent files) is the canonical form.
 */
std::string canonical_server_uri(const std::string& uri);

/*
 * Connects to the server in uris (all on one server) and opens them as one
 * trace: a single trace, or two or more system traces to compare. Blocks for
 * as long as connecting and the server's open take, so the UI runs it on a
 * worker thread.
 */
ServerOpenResult open_on_server(const std::vector<std::string>& uris);

// The server path in uri, or uri when it is not a server URI.
std::string server_uri_path(const std::string& uri);
// "name @ host" for a tab label.
std::string server_trace_label(const std::string& uri);

}  // namespace View
}  // namespace RocProfVis
