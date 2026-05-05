// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include <optional>
#include <string>

namespace RocProfVis
{
namespace DataModel
{

struct RemoteUri
{
    std::string user_at_host;   // "user@host" form for ssh CLI
    std::string remote_path;    // absolute path on remote
    std::string password;       // empty if not provided in URI
    std::string identity_file;  // optional private key path (?key=...)
};

// True iff `path` starts with the literal prefix "ssh://".
bool IsSshUri(const std::string& path);

// Parse "ssh://[user[:password]@]host[:port]/path/to.db[?key=/path/to/id_rsa]".
// Port is stripped from the authority. Optional ?key= query selects the
// private-key file. Returns std::nullopt on malformed input.
std::optional<RemoteUri> ParseRemoteUri(const std::string& uri);

// Resolve (and lazily materialize) the on-disk SQLite cache for `uri`.
// On success, writes the cache file path into `cache_path` and returns true.
// Materialization is via per-table `.dump` over an SSH ControlMaster channel.
bool MaterializeRemoteCache(const std::string& uri, std::string& cache_path);

enum class SshAuthResult
{
    Ok,
    NeedsPassword,
    ConnectionError
};

// Probe the ssh authentication path for `uri`. Uses BatchMode=yes so the ssh
// client never prompts; failure with "Permission denied" maps to NeedsPassword.
// Network failures (host unreachable, DNS) map to ConnectionError. A password
// in the URI is honored via sshpass.
SshAuthResult TestSshConnection(const std::string& uri);

}  // namespace DataModel
}  // namespace RocProfVis
