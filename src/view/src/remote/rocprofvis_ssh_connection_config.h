// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include <string>

#include "json.h"

namespace RocProfVis
{
namespace View
{

// Pure SSH connection configuration: connection identity plus credentials.
// This is the persisted, named "connection profile" half of what used to live
// inside RemoteUri. Each profile carries a stable id (used as the on-disk key
// and the reference target for launch profiles) and a user-facing display name.
//
// NOTE: password and passphrase are stored in plaintext for now. Secure storage
// (e.g. an OS keyring) is intended future work and is explicitly out of scope.
struct SshConnectionConfig
{
    std::string id;            // stable, auto-generated identifier
    std::string display_name;  // user-facing, editable label
    std::string host;
    std::string port = "22";
    std::string user;
    std::string password;
    std::string identity_file;
    std::string passphrase;

    // Trimmed accessors used by the SSH/session layers.
    std::string HostTrimmed() const;
    std::string PortTrimmed() const;
    int         PortInt() const;
    std::string UserTrimmed() const;
    std::string PasswordTrimmed() const;
    std::string IdentityFileTrimmed() const;
    std::string PassphraseTrimmed() const;

    jt::Json            ToJson() const;
    static SshConnectionConfig FromJson(jt::Json const& j);

    // Generates a new stable id (16 hex chars). Used when a brand-new profile is
    // created so launch profiles can reference it.
    static std::string GenerateId();
};

}  // namespace View
}  // namespace RocProfVis
