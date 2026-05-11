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
    std::string host;
    std::string user;
    std::string password;
    int         port = 22;
    std::string path;
    std::string identity_file;
    // Passphrase for an encrypted private key. Memory-only; never parsed
    // from or written to a URI string.
    std::string key_passphrase;
};

bool IsSshUri(const std::string& s);

// Strict ssh://[user[:password]@]host[:port]/abs/path[?key=/path/to/id]
std::optional<RemoteUri> ParseRemoteUri(const std::string& uri);

// URI suitable for Recent-files: omits password.
std::string SanitizeForRecent(const RemoteUri& u);

// Re-serialize a RemoteUri (includes password if present). Used by callers
// that want a single string to log or to display.
std::string SerializeRemoteUri(const RemoteUri& u, bool include_password);

}  // namespace DataModel
}  // namespace RocProfVis
