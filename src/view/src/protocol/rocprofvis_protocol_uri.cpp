// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_protocol_uri.h"

#include <cctype>
#include <cstdlib>
#include <cstring>
#include <utility>

namespace RocProfVis
{
namespace View
{

namespace
{

constexpr int         MAX_PORT     = 65535;
constexpr const char* TOKEN_PARAM  = "token=";

// A Windows server path keeps its drive letter: optiq://host:9750/C:/traces/a.db.
bool
is_drive_path(const std::string& path)
{
    return path.size() >= 3 && path[0] == '/' && std::isalpha(static_cast<unsigned char>(path[1])) &&
           path[2] == ':';
}

bool
starts_with(const std::string& text, const char* prefix)
{
    return text.compare(0, std::strlen(prefix), prefix) == 0;
}

}  // namespace

bool
is_server_uri(const std::string& text)
{
    return starts_with(text, SERVER_URI_SCHEME) || starts_with(text, SERVER_TLS_URI_SCHEME);
}

bool
parse_server_uri(const std::string& text, ServerUri& out)
{
    bool result = false;
    if(is_server_uri(text))
    {
        const bool   tls             = starts_with(text, SERVER_TLS_URI_SCHEME);
        const size_t authority_start = std::strlen(tls ? SERVER_TLS_URI_SCHEME : SERVER_URI_SCHEME);
        const size_t path_start      = text.find('/', authority_start);
        const size_t colon           = text.rfind(':', path_start);
        if(path_start != std::string::npos && colon != std::string::npos &&
           colon > authority_start && colon < path_start)
        {
            std::string port_text = text.substr(colon + 1, path_start - colon - 1);
            char*       end       = nullptr;
            long        port      = std::strtol(port_text.c_str(), &end, 10);
            if(!port_text.empty() && end && *end == '\0' && port > 0 && port <= MAX_PORT)
            {
                ServerUri parsed;
                parsed.host = text.substr(authority_start, colon - authority_start);
                parsed.port = static_cast<int>(port);
                parsed.tls  = tls;
                parsed.path = text.substr(path_start);
                const size_t query = parsed.path.find('?');
                if(query != std::string::npos)
                {
                    std::string params = parsed.path.substr(query + 1);
                    parsed.path.erase(query);
                    if(starts_with(params, TOKEN_PARAM))
                    {
                        parsed.token = params.substr(std::strlen(TOKEN_PARAM));
                    }
                }
                if(is_drive_path(parsed.path))
                {
                    parsed.path.erase(0, 1);
                }
                if(!parsed.host.empty())
                {
                    out    = std::move(parsed);
                    result = true;
                }
            }
        }
    }
    return result;
}

std::string
make_server_uri(const std::string& host, int port, bool tls, const std::string& path)
{
    std::string uri =
        (tls ? SERVER_TLS_URI_SCHEME : SERVER_URI_SCHEME) + make_server_address(host, port);
    if(path.empty() || path[0] != '/')
    {
        uri += '/';
    }
    return uri + path;
}

std::string
make_server_address(const std::string& host, int port)
{
    return host + ":" + std::to_string(port);
}

bool
is_loopback_host(const std::string& host)
{
    return host == "localhost" || host == "::1" || host == "[::1]" ||
           starts_with(host, "127.");
}

}  // namespace View
}  // namespace RocProfVis
