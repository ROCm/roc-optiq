// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_server_open.h"
#include "protocol/rocprofvis_data_channel.h"
#include "protocol/rocprofvis_protocol_uri.h"
#include "rocprofvis_server_connection.h"

#include <filesystem>

namespace RocProfVis
{
namespace View
{

std::string
canonical_server_uri(const std::string& uri)
{
    ServerUri parsed;
    if(!parse_server_uri(uri, parsed))
    {
        return uri;
    }
    if(!parsed.token.empty())
    {
        ServerConnection::SetToken(parsed.host, parsed.port, parsed.tls, parsed.token);
    }
    return make_server_uri(parsed.host, parsed.port, parsed.tls, parsed.path);
}

ServerOpenResult
open_on_server(const std::vector<std::string>& uris)
{
    ServerOpenResult         result;
    ServerUri                server;
    std::vector<std::string> paths;
    for(const std::string& uri : uris)
    {
        ServerUri parsed;
        if(!parse_server_uri(uri, parsed))
        {
            result.error = "Not a valid server trace address:\n\n" + uri;
            return result;
        }
        if(paths.empty())
        {
            server = parsed;
        }
        else if(parsed.host != server.host || parsed.port != server.port ||
                parsed.tls != server.tls)
        {
            result.error = "Traces to compare must be on the same server.";
            return result;
        }
        if(!parsed.token.empty())
        {
            ServerConnection::SetToken(parsed.host, parsed.port, parsed.tls, parsed.token);
        }
        paths.push_back(parsed.path);
    }
    if(paths.empty())
    {
        result.error = "No trace to open.";
        return result;
    }

    ServerConnectionOptions options;
    options.tls = server.tls;
    std::string                       error;
    std::shared_ptr<ServerConnection> connection =
        ServerConnection::Get(server.host, server.port, options, error);
    if(connection)
    {
        result.channel = connection->OpenTrace(paths, result.is_compute, error);
    }
    if(!result.channel)
    {
        result.error = "Could not open " + paths.front() + " on " +
                       make_server_address(server.host, server.port) + ":\n\n" + error;
    }
    return result;
}

std::string
server_uri_path(const std::string& uri)
{
    ServerUri parsed;
    return parse_server_uri(uri, parsed) ? parsed.path : uri;
}

std::string
server_trace_label(const std::string& uri)
{
    ServerUri parsed;
    if(!parse_server_uri(uri, parsed))
    {
        return std::filesystem::path(uri).filename().string();
    }
    return std::filesystem::path(parsed.path).filename().string() + " @ " + parsed.host;
}

}  // namespace View
}  // namespace RocProfVis
