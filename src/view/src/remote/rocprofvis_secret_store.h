// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include <string>

namespace RocProfVis
{
namespace View
{

// Access to the OS credential store, used to keep SSH passwords and key
// passphrases out of profiles.json. The backend is picked at build time:
// Windows Credential Manager, macOS Keychain, or libsecret on Linux (when it
// is available). Secrets are keyed by the SshConnectionConfig id.
//
// IsAvailable() is false when there is no usable store (a Linux build without
// libsecret, or no running Secret Service). Callers should then leave the
// secret unset and let the SSH layer prompt for it at connect time instead of
// writing it to disk.
class SecretStore
{
public:
    static bool IsAvailable();
    static bool Set(const std::string& key, const std::string& secret);
    // Returns false and clears out_secret when the key is absent.
    static bool Get(const std::string& key, std::string& out_secret);
    static bool Erase(const std::string& key);
};

}  // namespace View
}  // namespace RocProfVis
