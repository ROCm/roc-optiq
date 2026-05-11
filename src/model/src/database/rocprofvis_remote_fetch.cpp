// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_remote_fetch.h"

#include "rocprofvis_ssh_auth_bridge.h"

#include <spdlog/spdlog.h>

#include <cctype>
#include <cstdio>
#include <fstream>
#include <sstream>

namespace RocProfVis
{
namespace DataModel
{

namespace
{
// FNV-1a 64-bit. Adequate for naming local cache directories.
uint64_t Fnv1a64(const std::string& s)
{
    uint64_t h = 0xcbf29ce484222325ull;
    for(unsigned char c : s)
    {
        h ^= c;
        h *= 0x100000001b3ull;
    }
    return h;
}

std::string ToHex(uint64_t v)
{
    char buf[17];
    std::snprintf(buf, sizeof(buf), "%016llx", static_cast<unsigned long long>(v));
    return buf;
}

bool IsSafeChar(char c)
{
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-' ||
           c == '.';
}

std::string Sanitize(const std::string& s)
{
    std::string out;
    out.reserve(s.size());
    for(char c : s) out.push_back(IsSafeChar(c) ? c : '_');
    return out;
}

SshAuthResult FromConnect(ConnectResult r)
{
    switch(r)
    {
        case ConnectResult::Ok:               return SshAuthResult::Ok;
        case ConnectResult::NeedsPassword:    return SshAuthResult::NeedsPassword;
        case ConnectResult::AuthFailed:       return SshAuthResult::AuthFailed;
        case ConnectResult::HostKeyMismatch:  return SshAuthResult::HostKeyMismatch;
        case ConnectResult::HostKeyRejected:  return SshAuthResult::HostKeyRejected;
        case ConnectResult::ConnectionError:  return SshAuthResult::ConnectionError;
        case ConnectResult::Cancelled:        return SshAuthResult::Cancelled;
    }
    return SshAuthResult::ConnectionError;
}
}  // namespace

std::string RemoteCacheKey(const RemoteUri& u)
{
    std::string canon = u.user + "@" + u.host + ":" + std::to_string(u.port) + u.path;
    return ToHex(Fnv1a64(canon));
}

std::string RemoteBasenameForCache(const RemoteUri& u)
{
    auto pos = u.path.find_last_of('/');
    std::string base = (pos == std::string::npos) ? u.path : u.path.substr(pos + 1);
    if(base.empty()) base = "remote";
    base = Sanitize(base);
    if(base.size() < 3 || base.substr(base.size() - 3) != ".db") base += ".db";
    return base;
}

SshAuthResult TestSshConnection(const RemoteUri& u, AuthBridge& bridge,
                                std::string& err)
{
    SshSession   s;
    AuthOptions  opts;
    opts.password      = u.password;
    opts.identity_file = u.identity_file;
    return FromConnect(s.Connect(u.host, u.port, u.user, opts, bridge, err));
}

SshAuthResult FetchRemoteFile(const RemoteUri& u,
                              const std::filesystem::path& dest_path,
                              AuthBridge& bridge, ProgressSink& sink,
                              std::string& err)
{
    spdlog::info("[ssh] FetchRemoteFile uri=ssh://{}@{}:{}{} dest={}",
                 u.user, u.host, u.port, u.path, dest_path.string());

    SshSession  s;
    AuthOptions opts;
    opts.password      = u.password;
    opts.identity_file = u.identity_file;
    opts.passphrase    = u.key_passphrase;

    auto r = s.Connect(u.host, u.port, u.user, opts, bridge, err);
    if(r != ConnectResult::Ok)
    {
        spdlog::warn("[ssh] FetchRemoteFile aborting: Connect returned {}",
                     static_cast<int>(r));
        return FromConnect(r);
    }

    uint64_t mtime = 0;
    std::string stat_err;
    uint64_t    remote_size = s.StatFileSize(u.path, mtime, stat_err);
    if(remote_size == 0 && !stat_err.empty())
    {
        err = stat_err;
        return SshAuthResult::ConnectionError;
    }

    auto meta_path = std::filesystem::path(dest_path).concat(".meta");

    // Cache hit: dest exists and meta matches remote (size, mtime).
    {
        std::error_code ec;
        if(std::filesystem::exists(dest_path, ec) &&
           std::filesystem::exists(meta_path, ec))
        {
            std::ifstream m(meta_path);
            uint64_t      cached_size = 0, cached_mtime = 0;
            if(m >> cached_size >> cached_mtime &&
               cached_size == remote_size && cached_mtime == mtime)
            {
                sink.total      = remote_size;
                sink.bytes_read = remote_size;
                spdlog::info("Remote cache hit: {}", dest_path.string());
                return SshAuthResult::Ok;
            }
        }
    }

    std::error_code ec;
    std::filesystem::create_directories(dest_path.parent_path(), ec);

    if(!s.DownloadTo(u.path, dest_path.string(), sink, err))
    {
        return SshAuthResult::ConnectionError;
    }

    {
        std::ofstream m(meta_path, std::ios::trunc);
        if(m) m << remote_size << ' ' << mtime << '\n';
    }
    spdlog::info("Fetched remote file to {}", dest_path.string());
    return SshAuthResult::Ok;
}

}  // namespace DataModel
}  // namespace RocProfVis
