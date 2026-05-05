// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_remote_uri.h"

#include <spdlog/spdlog.h>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <sstream>
#include <string>
#include <vector>

namespace RocProfVis
{
namespace DataModel
{

namespace
{

constexpr const char* kSshPrefix = "ssh://";
constexpr const char* kCacheDir  = "/tmp";

std::string FnvHash(const std::string& key)
{
    uint64_t h = 14695981039346656037ULL;
    for(unsigned char c : key)
    {
        h = (h ^ c) * 1099511628211ULL;
    }
    char buf[17];
    std::snprintf(buf, sizeof(buf), "%016llx", static_cast<unsigned long long>(h));
    return buf;
}

// Bourne-shell single-quote quoting. Wraps `s` in '...' and escapes any
// embedded ' as '\''. Safe for arbitrary input.
std::string ShellQuote(const std::string& s)
{
    std::string out;
    out.reserve(s.size() + 2);
    out.push_back('\'');
    for(char c : s)
    {
        if(c == '\'')
        {
            out += "'\\''";
        }
        else
        {
            out.push_back(c);
        }
    }
    out.push_back('\'');
    return out;
}

std::string ControlPathFor(const std::string& cache_hash)
{
    return std::string(kCacheDir) + "/roc-optiq-cm-" + cache_hash;
}

// Build the SSH command prefix shared by all remote invocations:
//   sshpass -p PWD ssh -o ... user@host
// or, when password is empty:
//   ssh -o ... user@host
std::string BuildSshPrefix(const RemoteUri& uri, const std::string& cache_hash)
{
    std::ostringstream cmd;
    if(!uri.password.empty())
    {
        cmd << "sshpass -p " << ShellQuote(uri.password) << " ";
    }
    cmd << "ssh -o StrictHostKeyChecking=no -o ControlMaster=auto"
        << " -o ControlPath=" << ShellQuote(ControlPathFor(cache_hash))
        << " -o ControlPersist=10m";
    if(!uri.identity_file.empty())
    {
        cmd << " -o IdentitiesOnly=yes -i " << ShellQuote(uri.identity_file);
    }
    cmd << " " << ShellQuote(uri.user_at_host);
    return cmd.str();
}

// Run `cmd` via popen and append stdout lines to `out`. Returns the exit code.
int RunCapture(const std::string& cmd, std::string& out)
{
    FILE* pipe = ::popen(cmd.c_str(), "r");
    if(!pipe) return -1;
    char buf[4096];
    while(::fgets(buf, sizeof(buf), pipe))
    {
        out.append(buf);
    }
    return ::pclose(pipe);
}

int RunSilent(const std::string& cmd)
{
    FILE* pipe = ::popen(cmd.c_str(), "r");
    if(!pipe) return -1;
    char buf[4096];
    while(::fgets(buf, sizeof(buf), pipe))
    {
        // discard
    }
    return ::pclose(pipe);
}

bool ListRemoteObjects(const std::string&                                ssh_prefix,
                       const std::string&                                remote_path,
                       std::vector<std::pair<std::string, std::string>>& objects)
{
    // Use char(9) for tab so the SELECT survives nested shell quoting.
    const std::string remote_sql =
        "SELECT type || char(9) || name FROM sqlite_master "
        "WHERE type IN ('table','view') AND name NOT LIKE 'sqlite_%' "
        "ORDER BY CASE type WHEN 'table' THEN 0 ELSE 1 END, name;";
    std::string remote_cmd = "sqlite3 " + ShellQuote(remote_path) + " " + ShellQuote(remote_sql);
    std::string full_cmd   = ssh_prefix + " " + ShellQuote(remote_cmd);

    std::string out;
    int         rc = RunCapture(full_cmd, out);
    if(rc != 0)
    {
        spdlog::error("[remote-cache] failed to list remote objects (rc={}): {}", rc, out);
        return false;
    }
    std::istringstream iss(out);
    std::string        line;
    while(std::getline(iss, line))
    {
        if(line.empty()) continue;
        auto tab = line.find('\t');
        if(tab == std::string::npos) continue;
        objects.emplace_back(line.substr(0, tab), line.substr(tab + 1));
    }
    return true;
}

bool DumpOneObject(const std::string& ssh_prefix,
                   const std::string& remote_path,
                   const std::string& object_name,
                   const std::string& cache_path)
{
    std::string remote_cmd = "sqlite3 " + ShellQuote(remote_path) + " " +
                             ShellQuote(std::string(".dump ") + object_name);
    std::string full_cmd = "(" + ssh_prefix + " " + ShellQuote(remote_cmd) + ") | sqlite3 " +
                           ShellQuote(cache_path);
    return RunSilent(full_cmd) == 0;
}

}  // namespace

bool IsSshUri(const std::string& path)
{
    return path.compare(0, std::strlen(kSshPrefix), kSshPrefix) == 0;
}

std::optional<RemoteUri> ParseRemoteUri(const std::string& uri)
{
    if(!IsSshUri(uri)) return std::nullopt;
    // After "ssh://" we expect [authority][/path][?query]
    std::string rest    = uri.substr(std::strlen(kSshPrefix));
    auto        slashp  = rest.find('/');
    if(slashp == std::string::npos || slashp == 0) return std::nullopt;
    std::string authority = rest.substr(0, slashp);
    std::string path      = rest.substr(slashp);  // keep leading '/'

    std::string identity_file;
    auto        qmark = path.find('?');
    if(qmark != std::string::npos)
    {
        std::string query = path.substr(qmark + 1);
        path              = path.substr(0, qmark);
        // Parse minimal &-separated key=value query. Only "key" is honored.
        size_t pos = 0;
        while(pos < query.size())
        {
            auto amp = query.find('&', pos);
            std::string token =
                query.substr(pos, amp == std::string::npos ? std::string::npos : amp - pos);
            auto eq = token.find('=');
            if(eq != std::string::npos)
            {
                std::string k = token.substr(0, eq);
                std::string v = token.substr(eq + 1);
                if(k == "key" || k == "identity") identity_file = v;
            }
            if(amp == std::string::npos) break;
            pos = amp + 1;
        }
    }

    std::string userpass;
    std::string hostport;
    auto        atp = authority.rfind('@');
    if(atp == std::string::npos)
    {
        hostport = authority;
    }
    else
    {
        userpass = authority.substr(0, atp);
        hostport = authority.substr(atp + 1);
    }
    if(hostport.empty()) return std::nullopt;

    // Strip :port from hostport.
    auto colonp = hostport.find(':');
    std::string host = (colonp == std::string::npos) ? hostport : hostport.substr(0, colonp);
    if(host.empty()) return std::nullopt;

    std::string user;
    std::string password;
    if(!userpass.empty())
    {
        auto pcolon = userpass.find(':');
        if(pcolon == std::string::npos)
        {
            user = userpass;
        }
        else
        {
            user     = userpass.substr(0, pcolon);
            password = userpass.substr(pcolon + 1);
        }
    }

    RemoteUri out;
    out.user_at_host  = user.empty() ? host : (user + "@" + host);
    out.remote_path   = path;
    out.password      = password;
    out.identity_file = identity_file;
    return out;
}

SshAuthResult TestSshConnection(const std::string& uri)
{
    auto parsed = ParseRemoteUri(uri);
    if(!parsed) return SshAuthResult::ConnectionError;

    const std::string hash_key = parsed->user_at_host + "|" + parsed->remote_path;
    const std::string hash     = FnvHash(hash_key);

    std::ostringstream cmd;
    if(!parsed->password.empty())
    {
        cmd << "sshpass -p " << ShellQuote(parsed->password) << " ";
    }
    cmd << "ssh -o BatchMode="
        << (parsed->password.empty() ? "yes" : "no")
        << " -o StrictHostKeyChecking=no -o ConnectTimeout=5"
        << " -o ControlMaster=auto -o ControlPath=" << ShellQuote(ControlPathFor(hash))
        << " -o ControlPersist=10m";
    if(!parsed->identity_file.empty())
    {
        cmd << " -o IdentitiesOnly=yes -i " << ShellQuote(parsed->identity_file);
    }
    cmd << " " << ShellQuote(parsed->user_at_host) << " true 2>&1";

    std::string out;
    int         rc = RunCapture(cmd.str(), out);
    if(rc == 0) return SshAuthResult::Ok;

    // ssh exit 255 = SSH-level error. Distinguish auth vs network by stderr.
    if(out.find("Permission denied") != std::string::npos ||
       out.find("Authentication failed") != std::string::npos ||
       out.find("publickey") != std::string::npos ||
       out.find("Host key verification failed") == std::string::npos)
    {
        // Heuristic: any non-network failure is treated as needing a password.
        // Network errors are caught below.
        if(out.find("Could not resolve") != std::string::npos ||
           out.find("Connection refused") != std::string::npos ||
           out.find("Connection timed out") != std::string::npos ||
           out.find("No route to host") != std::string::npos)
        {
            return SshAuthResult::ConnectionError;
        }
        return SshAuthResult::NeedsPassword;
    }
    return SshAuthResult::ConnectionError;
}

bool MaterializeRemoteCache(const std::string& uri, std::string& cache_path)
{
    auto parsed = ParseRemoteUri(uri);
    if(!parsed)
    {
        spdlog::error("[remote-cache] malformed URI: {}", uri);
        return false;
    }

    const std::string hash_key = parsed->user_at_host + "|" + parsed->remote_path;
    const std::string hash     = FnvHash(hash_key);
    cache_path = std::string(kCacheDir) + "/roc-optiq-cache-" + hash + ".db";

    if(std::filesystem::exists(cache_path))
    {
        std::error_code ec;
        auto sz = std::filesystem::file_size(cache_path, ec);
        double mb = (!ec) ? (sz / (1024.0 * 1024.0)) : 0.0;
        spdlog::info("[remote-cache] cache hit: {} ({:.1f} MB)", cache_path, mb);
        return true;
    }

    spdlog::info("[remote-cache] cache miss; materializing (raw cat) into {}", cache_path);
    const std::string ssh_prefix = BuildSshPrefix(*parsed, hash);

    // Single binary stream: ssh remote "cat <file>" > cache_path
    std::string remote_cmd = "cat " + ShellQuote(parsed->remote_path);
    std::string full_cmd   = ssh_prefix + " " + ShellQuote(remote_cmd) +
                           " > " + ShellQuote(cache_path);

    using clock_t  = std::chrono::steady_clock;
    auto t0        = clock_t::now();
    int  rc        = RunSilent(full_cmd);
    auto total_ms  = std::chrono::duration_cast<std::chrono::milliseconds>(clock_t::now() - t0).count();

    if(rc != 0)
    {
        spdlog::error("[remote-cache] raw cat failed (rc={}); removing partial cache", rc);
        std::error_code ec;
        std::filesystem::remove(cache_path, ec);
        return false;
    }

    int64_t bytes = 0;
    {
        std::error_code ec;
        auto sz = std::filesystem::file_size(cache_path, ec);
        if(!ec) bytes = static_cast<int64_t>(sz);
    }
    double mb         = bytes / (1024.0 * 1024.0);
    double throughput = (total_ms > 0) ? (mb * 1000.0 / total_ms) : 0.0;
    spdlog::info("[remote-cache] complete (raw cat): {:.1f} MB in {} ms ({:.1f} MB/s)",
                 mb, total_ms, throughput);
    return true;
}

}  // namespace DataModel
}  // namespace RocProfVis
