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

struct SshConnectionSpec
{
    std::string host;
    std::string user;
    int         port = 22;
    std::string identity_file;
    std::string remote_stage_dir;
};

struct ConnectionSpec
{
    ConnectionType  type = ConnectionType::kLocal;
    SshConnectionSpec ssh;
};

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
    ConnectionSpec           connection;
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
