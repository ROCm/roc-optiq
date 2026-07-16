// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "json.h"
#include <string>
#include <vector>
#include <map>

namespace RocProfVis
{
namespace View
{

enum class ConnectionType
{
    kLocal,
    kSsh
};

// Note: only the connection MODE (local vs. SSH) is stored on a LaunchConfig.
// The actual SSH connection details (host/user/auth/output path) live in
// RemoteUri, edited via the SshSettingsDialog and persisted separately.

struct TargetSpec
{
    std::string executable;
    std::string arguments;
    std::string working_directory;
    std::string output_directory;
    bool        auto_load_trace = true;
};

struct LaunchConfig
{
    std::string              profiler_id;
    std::string              tool_id;
    ConnectionType           connection = ConnectionType::kLocal;
    // Id of the SSH connection profile (SshConnectionConfig::id) used when
    // connection == kSsh. Empty for local launches or when unset.
    std::string              ssh_connection_ref;
    TargetSpec               target;
    std::map<std::string, std::string> extra_env;
    std::vector<std::string> extra_argv;
    jt::Json                 backend_payload;

    LaunchConfig();

    jt::Json ToJson() const;
    static LaunchConfig FromJson(jt::Json const& json);
};

} // namespace View
} // namespace RocProfVis
