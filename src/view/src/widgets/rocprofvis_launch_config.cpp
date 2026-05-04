// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_launch_config.h"

namespace RocProfVis
{
namespace View
{

LaunchConfig::LaunchConfig()
    : profiler_id("rocprof-sys")
    , tool_id("run")
    , connection()
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
    conn["type"] = (connection.type == ConnectionType::kLocal) ? "local" : "ssh";
    if (connection.type == ConnectionType::kSsh)
    {
        conn["host"] = connection.ssh.host;
        conn["user"] = connection.ssh.user;
        conn["port"] = static_cast<long>(connection.ssh.port);
        conn["identity_file"] = connection.ssh.identity_file;
        conn["remote_stage_dir"] = connection.ssh.remote_stage_dir;
    }

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

    if (j.contains("profiler_id"))
    {
        cfg.profiler_id = j["profiler_id"].getString();
    }
    if (j.contains("tool_id"))
    {
        cfg.tool_id = j["tool_id"].getString();
    }

    if (j.contains("connection"))
    {
        jt::Json& conn = j["connection"];
        if (conn.contains("type"))
        {
            std::string conn_type = conn["type"].getString();
            if (conn_type == "ssh")
            {
                cfg.connection.type = ConnectionType::kSsh;
                if (conn.contains("host"))
                    cfg.connection.ssh.host = conn["host"].getString();
                if (conn.contains("user"))
                    cfg.connection.ssh.user = conn["user"].getString();
                if (conn.contains("port"))
                    cfg.connection.ssh.port = static_cast<int>(conn["port"].getLong());
                if (conn.contains("identity_file"))
                    cfg.connection.ssh.identity_file = conn["identity_file"].getString();
                if (conn.contains("remote_stage_dir"))
                    cfg.connection.ssh.remote_stage_dir = conn["remote_stage_dir"].getString();
            }
        }
    }

    if (j.contains("target"))
    {
        jt::Json& tgt = j["target"];
        if (tgt.contains("executable"))
            cfg.target.executable = tgt["executable"].getString();
        if (tgt.contains("arguments"))
            cfg.target.arguments = tgt["arguments"].getString();
        if (tgt.contains("working_directory"))
            cfg.target.working_directory = tgt["working_directory"].getString();
        if (tgt.contains("output_directory"))
            cfg.target.output_directory = tgt["output_directory"].getString();
        if (tgt.contains("auto_load_trace"))
            cfg.target.auto_load_trace = tgt["auto_load_trace"].getBool();
    }

    if (json.contains("extra_env"))
    {
        jt::Json& env = const_cast<jt::Json&>(json)["extra_env"];
        if (env.isObject())
        {
            for (auto& kv : env.getObject())
            {
                cfg.extra_env[kv.first] = kv.second.getString();
            }
        }
    }

    if (json.contains("extra_argv"))
    {
        jt::Json& argv_json = const_cast<jt::Json&>(json)["extra_argv"];
        if (argv_json.isArray())
        {
            for (jt::Json& item : argv_json.getArray())
            {
                if (item.isString())
                {
                    cfg.extra_argv.push_back(item.getString());
                }
            }
        }
    }

    if (j.contains("backend_payload"))
    {
        cfg.backend_payload = j["backend_payload"];
    }

    return cfg;
}

} // namespace View
} // namespace RocProfVis
