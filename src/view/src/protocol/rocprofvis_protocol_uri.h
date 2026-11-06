// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include <string>

namespace RocProfVis
{
namespace View
{

/*
 * A trace on a roc-optiq-server, written optiq://host:port/path/on/server, or
 * optiqs:// for a server that speaks TLS. The URI is what the UI keys the trace
 * by (project id, recent files, the -f argument), so it has to round-trip
 * exactly. It may carry ?token=<access token> when typed or passed on the
 * command line; make_server_uri never writes one, so tokens do not end up in
 * recent files or projects.
 */
struct ServerUri
{
    std::string host;
    int         port = 0;
    bool        tls  = false;
    std::string path;
    std::string token;
};

constexpr const char* SERVER_URI_SCHEME     = "optiq://";
constexpr const char* SERVER_TLS_URI_SCHEME = "optiqs://";

bool        is_server_uri(const std::string& text);
// False when text is not a well formed server URI; out is then untouched.
bool        parse_server_uri(const std::string& text, ServerUri& out);
std::string make_server_uri(const std::string& host, int port, bool tls,
                            const std::string& path);
// "host:port", the key a connection is shared by.
std::string make_server_address(const std::string& host, int port);
// localhost, 127.x.x.x or ::1: a server on this machine.
bool        is_loopback_host(const std::string& host);

}  // namespace View
}  // namespace RocProfVis
