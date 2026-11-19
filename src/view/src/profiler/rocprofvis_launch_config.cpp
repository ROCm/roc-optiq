// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_launch_config.h"
#include "rocprofvis_json_utils.h"
#include "rocprofvis_profiler.h"
#include "spdlog/spdlog.h"

#include <filesystem>

namespace RocProfVis
{
namespace View
{

LaunchConfig::LaunchConfig()
    : profiler_id("rocprof-sys")
    , connection(ConnectionType::kLocal)
    , backend_payload()
{
}

jt::Json LaunchConfig::ToJson() const
{
    jt::Json json;
    json["profiler_id"] = profiler_id;
    json["tool"] = static_cast<int32_t>(tool);
    json["operation"] = static_cast<int32_t>(operation);

    jt::Json& conn = json["connection"];
    conn["type"] = (connection == ConnectionType::kLocal) ? "local" : "ssh";
    conn["ssh_connection_ref"] = ssh_connection_ref;

    json["backend_payload"] = backend_payload;

    return json;
}

LaunchConfig LaunchConfig::FromJson(jt::Json const& json)
{
    LaunchConfig cfg;
    jt::Json& j = const_cast<jt::Json&>(json);

    cfg.profiler_id    = JsonUtils::GetString(j, "profiler_id", cfg.profiler_id);
    cfg.tool           = ToolFromInt(JsonUtils::GetInt(j, "tool", kRPVProfilerToolNone));
    cfg.operation      = OperationFromInt(
        JsonUtils::GetInt(j, "operation", kRPVProfilerOperationDefault));

    if (j.contains("connection"))
    {
        jt::Json& conn = j["connection"];
        if (JsonUtils::GetString(conn, "type", "local") == "ssh")
        {
            cfg.connection = ConnectionType::kSsh;
        }
        cfg.ssh_connection_ref = JsonUtils::GetString(conn, "ssh_connection_ref", "");
    }

    if (j.contains("backend_payload"))
    {
        cfg.backend_payload = j["backend_payload"];
    }

    return cfg;
}

jt::Json
ExtraArguments::ToJson() const
{
    jt::Json json;

    jt::Json& env_json = json["extra_env"];
    env_json.setObject();
    for (std::pair<std::string const, std::string> const& kv : env)
    {
        env_json[kv.first] = kv.second;
    }

    jt::Json& argv_json = json["extra_argv"];
    argv_json.setArray();
    for (std::string const& entry : argv)
    {
        argv_json.getArray().push_back(entry);
    }

    return json;
}

ExtraArguments
ExtraArguments::FromJson(jt::Json const& json)
{
    ExtraArguments extra;
    jt::Json&      j = const_cast<jt::Json&>(json);

    if (j.contains("extra_env") && j["extra_env"].isObject())
    {
        for (std::pair<std::string const, jt::Json>& kv : j["extra_env"].getObject())
        {
            if (kv.second.isString())
            {
                extra.env[kv.first] = kv.second.getString();
            }
        }
    }
    extra.argv = JsonUtils::GetStringArray(j, "extra_argv");

    return extra;
}

void
ExtraArguments::Argv(std::vector<std::string>& argv_out) const
{
    for(std::string const& entry : argv)
    {
        argv_out.push_back(entry);
    }
}

jt::Json
TargetSpec::ToJson() const
{
    jt::Json json;
    json["executable"]        = path;
    json["arguments"]         = arguments;
    json["working_directory"] = working_directory;
    json["output_directory"]  = output_directory;
    json["auto_load_trace"]   = auto_load_trace;
    return json;
}

TargetSpec
TargetSpec::FromJson(jt::Json const& json)
{
    TargetSpec target;
    jt::Json&  j             = const_cast<jt::Json&>(json);
    target.path              = JsonUtils::GetString(j, "executable", target.path);
    target.arguments         = JsonUtils::GetString(j, "arguments", target.arguments);
    target.working_directory = JsonUtils::GetString(j, "working_directory", target.working_directory);
    target.output_directory  = JsonUtils::GetString(j, "output_directory", target.output_directory);
    target.auto_load_trace   = JsonUtils::GetBool(j, "auto_load_trace", target.auto_load_trace);
    return target;
}

void
TargetSpec::Argv(bool executable, std::vector<std::string>& argv_out) const
{
    if(!path.empty())
    {
        if(executable)
        {
            argv_out.push_back("--");
        }
        argv_out.push_back(path);

        if(executable)
        {
            for(std::string const& arg : SplitArguments(arguments))
            {
                argv_out.push_back(arg);
            }
        }
    }
}

std::vector<std::string> SplitArguments(std::string const& arguments)
{
    std::vector<std::string> argv;
    std::string              current;
    // Tracks whether `current` holds a token at all, so that an explicitly
    // empty argument ("" on the command line) survives as an empty entry
    // instead of being dropped as if it were whitespace.
    bool                     have_token = false;

    for (size_t i = 0; i < arguments.size(); i++)
    {
        char c = arguments[i];

        if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f')
        {
            if (have_token)
            {
                argv.push_back(current);
                current.clear();
                have_token = false;
            }
            continue;
        }

        have_token = true;

        if (c == '\'')
        {
            // Single quotes are fully literal: everything up to the closing
            // quote, backslashes included.
            for (i++; i < arguments.size() && arguments[i] != '\''; i++)
            {
                current.push_back(arguments[i]);
            }
        }
        else if (c == '"')
        {
            for (i++; i < arguments.size() && arguments[i] != '"'; i++)
            {
                bool is_escape = arguments[i] == '\\' && (i + 1) < arguments.size() &&
                                 (arguments[i + 1] == '"' || arguments[i + 1] == '\\');
                if (is_escape)
                {
                    i++;
                }
                current.push_back(arguments[i]);
            }
        }
        else if (c == '\\' && (i + 1) < arguments.size())
        {
            i++;
            current.push_back(arguments[i]);
        }
        else
        {
            current.push_back(c);
        }
    }

    if (have_token)
    {
        argv.push_back(current);
    }

    return argv;
}

std::string GetToolBinaryName(rocprofvis_profiler_tool_t tool)
{
    uint32_t length = 0;
    rocprofvis_profiler_tool_get_binary_name(tool, nullptr, &length);
    if (length == 0)
    {
        return std::string();
    }

    std::vector<char>   buffer(length + 1, '\0');
    rocprofvis_result_t result =
        rocprofvis_profiler_tool_get_binary_name(tool, buffer.data(), &length);
    if (result != kRocProfVisResultSuccess)
    {
        return std::string();
    }

    buffer[length] = '\0';
    return std::string(buffer.data());
}

rocprofvis_profiler_tool_t ToolFromInt(int32_t value)
{
    // The signed test comes first and stands alone: comparing a negative int32
    // against the enum's unsigned values would convert it to a huge positive one.
    bool const is_tool =
        (value > 0) && (static_cast<uint32_t>(value) < __kRPVProfilerToolLast);
    if (!is_tool)
    {
        // 0 is kRPVProfilerToolNone, which is how an absent key arrives - not
        // worth a warning. Anything else is a value this build has no tool for.
        if (value != 0)
        {
            spdlog::warn("Ignoring unknown profiler tool {} in a saved launch profile", value);
        }
        return kRPVProfilerToolNone;
    }
    return static_cast<rocprofvis_profiler_tool_t>(value);
}

rocprofvis_profiler_operation_t OperationFromInt(int32_t value)
{
    // Signed test first, for the same reason as ToolFromInt. 0 is valid here:
    // it is kRPVProfilerOperationDefault.
    bool const is_operation =
        (value >= 0) && (static_cast<uint32_t>(value) < __kRPVProfilerOperationLast);
    if (!is_operation)
    {
        spdlog::warn("Ignoring unknown profiler operation {} in a saved launch profile",
                     value);
        return kRPVProfilerOperationDefault;
    }
    return static_cast<rocprofvis_profiler_operation_t>(value);
}

std::string ResolveToolPath(rocprofvis_profiler_tool_t tool,
                            std::string const&         tool_directory,
                            std::string const&         python_environment,
                            std::string&               out_error)
{
    out_error.clear();

    char const* directory = tool_directory.empty() ? nullptr : tool_directory.c_str();
    char const* environment =
        python_environment.empty() ? nullptr : python_environment.c_str();

    // The search runs on the length query, so that first call is the one that
    // reports why a tool could not be resolved.
    uint32_t            length = 0;
    rocprofvis_result_t result =
        rocprofvis_profiler_tool_resolve_path(tool, directory, environment, nullptr, &length);
    if (result == kRocProfVisResultSuccess && length > 0)
    {
        std::vector<char> buffer(length + 1, '\0');
        result = rocprofvis_profiler_tool_resolve_path(tool, directory, environment,
                                                       buffer.data(), &length);
        if (result == kRocProfVisResultSuccess)
        {
            buffer[length] = '\0';
            return std::string(buffer.data());
        }
    }

    std::string const name = GetToolBinaryName(tool);
    switch (result)
    {
        case kRocProfVisResultToolNotFound:
            // Naming the configured directory matters: the reason this fails
            // rather than falling back is that the user asked for that directory
            // specifically, so the fix is to correct it or clear it.
            out_error = !tool_directory.empty()
                            ? (name + " was not found in " + tool_directory)
                            : (name + " was not found. Check that ROCm is installed and that "
                                      "$ROCM_PATH or $PATH points at it.");
            break;
        case kRocProfVisResultInvalidArgument:
            if (!tool_directory.empty() && !std::filesystem::path(tool_directory).is_absolute())
            {
                out_error = "The tool directory must be an absolute path: " + tool_directory;
            }
#ifndef _WIN32
            else if (!python_environment.empty() &&
                     !std::filesystem::path(python_environment).is_absolute())
            {
                out_error = "The Python environment must be an absolute path: " +
                            python_environment;
            }
#endif
            else
            {
                out_error = "No profiler tool selected";
            }
            break;
        default:
            out_error = "Could not determine which " +
                        (name.empty() ? std::string("profiler") : name) + " binary to run";
            break;
    }
    return std::string();
}

} // namespace View
} // namespace RocProfVis
