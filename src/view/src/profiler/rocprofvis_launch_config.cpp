// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_launch_config.h"
#include "rocprofvis_json_utils.h"

namespace RocProfVis
{
namespace View
{

LaunchConfig::LaunchConfig()
    : profiler_id("rocprof-sys")
    , tool_id("run")
    , connection(ConnectionType::kLocal)
    , target()
    , extra_env()
    , extra_argv()
    , backend_payload()
{
}

jt::Json LaunchConfig::ToJson() const
{
    jt::Json json;
    json["profiler_id"] = profiler_id;
    json["tool_id"] = tool_id;

    jt::Json& conn = json["connection"];
    conn["type"] = (connection == ConnectionType::kLocal) ? "local" : "ssh";
    conn["ssh_connection_ref"] = ssh_connection_ref;

    jt::Json& tgt = json["target"];
    tgt["executable"] = target.executable;
    tgt["arguments"] = target.arguments;
    tgt["working_directory"] = target.working_directory;
    tgt["output_directory"] = target.output_directory;
    tgt["auto_load_trace"] = target.auto_load_trace;

    jt::Json& env = json["extra_env"];
    for (auto const& kv : extra_env)
    {
        env[kv.first] = kv.second;
    }

    int argv_idx = 0;
    for (auto const& arg : extra_argv)
    {
        json["extra_argv"][argv_idx++] = arg;
    }

    json["backend_payload"] = backend_payload;

    return json;
}

LaunchConfig LaunchConfig::FromJson(jt::Json const& json)
{
    LaunchConfig cfg;
    jt::Json& j = const_cast<jt::Json&>(json);

    cfg.profiler_id = JsonUtils::GetString(j, "profiler_id", cfg.profiler_id);
    cfg.tool_id     = JsonUtils::GetString(j, "tool_id", cfg.tool_id);

    if (j.contains("connection"))
    {
        jt::Json& conn = j["connection"];
        if (JsonUtils::GetString(conn, "type", "local") == "ssh")
        {
            cfg.connection = ConnectionType::kSsh;
        }
        cfg.ssh_connection_ref = JsonUtils::GetString(conn, "ssh_connection_ref", "");
    }

    if (j.contains("target"))
    {
        jt::Json& tgt = j["target"];
        cfg.target.executable        = JsonUtils::GetString(tgt, "executable", cfg.target.executable);
        cfg.target.arguments         = JsonUtils::GetString(tgt, "arguments", cfg.target.arguments);
        cfg.target.working_directory = JsonUtils::GetString(tgt, "working_directory", cfg.target.working_directory);
        cfg.target.output_directory  = JsonUtils::GetString(tgt, "output_directory", cfg.target.output_directory);
        cfg.target.auto_load_trace   = JsonUtils::GetBool(tgt, "auto_load_trace", cfg.target.auto_load_trace);
    }

    if (j.contains("extra_env"))
    {
        jt::Json& env = j["extra_env"];
        if (env.isObject())
        {
            for (auto& kv : env.getObject())
            {
                if (kv.second.isString())
                {
                    cfg.extra_env[kv.first] = kv.second.getString();
                }
            }
        }
    }

    cfg.extra_argv = JsonUtils::GetStringArray(j, "extra_argv");

    if (j.contains("backend_payload"))
    {
        cfg.backend_payload = j["backend_payload"];
    }

    return cfg;
}

} // namespace View
} // namespace RocProfVis
