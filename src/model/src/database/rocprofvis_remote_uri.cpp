// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_remote_uri.h"

#include <cctype>
#include <cstdlib>

namespace RocProfVis
{
namespace DataModel
{

namespace
{
constexpr const char* kSchemePrefix = "ssh://";
constexpr size_t      kSchemeLen    = 6;

bool IsValidPort(int p) { return p > 0 && p < 65536; }
}  // namespace

bool IsSshUri(const std::string& s)
{
    return s.size() >= kSchemeLen &&
           std::equal(s.begin(), s.begin() + kSchemeLen, kSchemePrefix);
}

std::optional<RemoteUri> ParseRemoteUri(const std::string& uri)
{
    if(!IsSshUri(uri)) return std::nullopt;

    std::string rest = uri.substr(kSchemeLen);

    // Split off ?key=... query suffix first; key path may contain '/' but not '?' or '#'.
    std::string identity;
    auto        qpos = rest.find('?');
    if(qpos != std::string::npos)
    {
        std::string query = rest.substr(qpos + 1);
        rest.resize(qpos);
        // Single supported key: key=...
        constexpr const char* kKeyEq = "key=";
        if(query.compare(0, 4, kKeyEq) == 0)
        {
            identity = query.substr(4);
        }
        else
        {
            return std::nullopt;
        }
    }

    // Split userinfo from authority+path on first '@' (if any).
    std::string userinfo;
    std::string authority_path = rest;
    auto        atp            = rest.find('@');
    if(atp != std::string::npos)
    {
        userinfo       = rest.substr(0, atp);
        authority_path = rest.substr(atp + 1);
    }

    // Split authority (host[:port]) from path on first '/'.
    // The first '/' is the URI separator and is consumed; what remains is
    // the path. A leading '/' in the *remaining* characters means the
    // original URI had a double slash and the path is absolute. Otherwise
    // it's relative to the user's home directory.
    auto slash = authority_path.find('/');
    if(slash == std::string::npos) return std::nullopt;  // path required

    std::string authority = authority_path.substr(0, slash);
    std::string path      = authority_path.substr(slash + 1);  // strip URI separator

    if(authority.empty() || path.empty()) return std::nullopt;

    // userinfo := user[:password]
    std::string user;
    std::string password;
    if(!userinfo.empty())
    {
        auto colon = userinfo.find(':');
        if(colon == std::string::npos)
        {
            user = userinfo;
        }
        else
        {
            user     = userinfo.substr(0, colon);
            password = userinfo.substr(colon + 1);
        }
    }

    // authority := host[:port]
    std::string host;
    int         port  = 22;
    auto        pcol  = authority.rfind(':');
    if(pcol == std::string::npos)
    {
        host = authority;
    }
    else
    {
        host                     = authority.substr(0, pcol);
        const std::string portstr = authority.substr(pcol + 1);
        if(portstr.empty()) return std::nullopt;
        char* end = nullptr;
        long  p   = std::strtol(portstr.c_str(), &end, 10);
        if(end != portstr.c_str() + portstr.size() || !IsValidPort(static_cast<int>(p)))
        {
            return std::nullopt;
        }
        port = static_cast<int>(p);
    }
    if(host.empty()) return std::nullopt;

    RemoteUri out;
    out.host          = std::move(host);
    out.user          = std::move(user);
    out.password      = std::move(password);
    out.port          = port;
    out.path          = std::move(path);
    out.identity_file = std::move(identity);
    return out;
}

std::string SanitizeForRecent(const RemoteUri& u)
{
    return SerializeRemoteUri(u, /*include_password=*/false);
}

std::string SerializeRemoteUri(const RemoteUri& u, bool include_password)
{
    std::string out = "ssh://";
    if(!u.user.empty())
    {
        out += u.user;
        if(include_password && !u.password.empty())
        {
            out += ":";
            out += u.password;
        }
        out += "@";
    }
    out += u.host;
    if(u.port != 22)
    {
        out += ":";
        out += std::to_string(u.port);
    }
    // Always emit the URI separator '/' between authority and path. Absolute
    // paths (already starting with '/') therefore appear as '//path' — the
    // standard SCP-style convention for distinguishing absolute from
    // home-relative SSH paths.
    out += "/";
    out += u.path;
    if(!u.identity_file.empty())
    {
        out += "?key=";
        out += u.identity_file;
    }
    return out;
}

}  // namespace DataModel
}  // namespace RocProfVis
