// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_profiles_document.h"
#include "rocprofvis_json_utils.h"
#include "rocprofvis_utils.h"

#include <filesystem>
#include <map>
#include <utility>

namespace RocProfVis
{
namespace View
{

namespace
{
    constexpr int kProfilesVersion = 1;

    // Writes a launch_environments entry, which carries its own id like an
    // ssh_connections entry.
    void
    WriteLaunchEnvironment(jt::Json& entry, const std::string& id,
                           const LaunchEnvironment& environment)
    {
        entry.setObject();
        entry["id"]                 = id;
        entry["tool_directory"]     = environment.tool_directory;
        entry["python_environment"] = environment.python_environment;
    }
}  // namespace

ProfilesDocument& ProfilesDocument::Get()
{
    static ProfilesDocument instance;
    return instance;
}

ProfilesDocument::ProfilesDocument()
{
    Reload();
}

std::string ProfilesDocument::GetFilePath() const
{
    std::filesystem::path config_root = get_application_config_path(true);
    return (config_root / "profiles.json").string();
}

void ProfilesDocument::Reload()
{
    jt::Json loaded;
    if(JsonUtils::ReadFile(GetFilePath(), loaded) && loaded.isObject())
    {
        m_root = std::move(loaded);
    }
    else
    {
        m_root.setObject();
    }
    EnsureSections(false);
}

void ProfilesDocument::EnsureSections(bool clear)
{
    if(!m_root.isObject())
    {
        m_root.setObject();
    }
    m_root["version"] = kProfilesVersion;

    if(!m_root.contains("ssh_connections") || !m_root["ssh_connections"].isObject())
    {
        m_root["ssh_connections"].setObject();
    }
    if(!m_root.contains("launch_profiles") || !m_root["launch_profiles"].isObject())
    {
        m_root["launch_profiles"].setObject();
    }
    if(!m_root.contains("launch_environments") || !m_root["launch_environments"].isObject())
    {
        m_root["launch_environments"].setObject();
    }
    if(!m_root["launch_environments"].contains(LOCAL_LAUNCH_ENVIRONMENT_ID))
    {
        WriteLaunchEnvironment(m_root["launch_environments"][LOCAL_LAUNCH_ENVIRONMENT_ID],
                               LOCAL_LAUNCH_ENVIRONMENT_ID, LaunchEnvironment());
    }
    if(clear)
    {
        std::map<std::string, jt::Json>&          root = m_root.getObject();
        std::map<std::string, jt::Json>::iterator key  = root.begin();
        while(key != root.end())
        {
            if(key->first != "version" && key->first != "ssh_connections" &&
               key->first != "launch_profiles" && key->first != "launch_environments")
            {
                key = root.erase(key);
            }
            else
            {
                ++key;
            }
        }

        std::map<std::string, jt::Json>& launch_profiles = m_root["launch_profiles"].getObject();
        std::map<std::string, jt::Json>::iterator it = launch_profiles.begin();
        while(it != launch_profiles.end())
        {
            if(it->second.isObject() && it->second.contains("profiler_id") &&
               it->second["profiler_id"].isString())
            {
                it = launch_profiles.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }
}

bool ProfilesDocument::Persist()
{
    // Stale content is dropped here, so it is gone the next time the document
    // is written.
    EnsureSections(true);
    return JsonUtils::WriteFile(GetFilePath(), m_root, true);
}

jt::Json& ProfilesDocument::SshConnections()
{
    EnsureSections(false);
    return m_root["ssh_connections"];
}

jt::Json& ProfilesDocument::LaunchProfiles()
{
    EnsureSections(false);
    return m_root["launch_profiles"];
}

jt::Json& ProfilesDocument::LaunchEnvironments()
{
    EnsureSections(false);
    return m_root["launch_environments"];
}

LaunchEnvironment
ProfilesDocument::GetLaunchEnvironment(const std::string& id)
{
    LaunchEnvironment environment;
    jt::Json&         environments = LaunchEnvironments();
    if(environments.contains(id) && environments[id].isObject())
    {
        environment.tool_directory =
            JsonUtils::GetString(environments[id], "tool_directory", "");
        environment.python_environment =
            JsonUtils::GetString(environments[id], "python_environment", "");
    }
    return environment;
}

bool
ProfilesDocument::SetLaunchEnvironment(const std::string&       id,
                                       const LaunchEnvironment& environment)
{
    WriteLaunchEnvironment(LaunchEnvironments()[id], id, environment);
    return Persist();
}

void
ProfilesDocument::AddLaunchEnvironment(const std::string& id)
{
    jt::Json& environments = LaunchEnvironments();
    if(!environments.contains(id) || !environments[id].isObject())
    {
        WriteLaunchEnvironment(environments[id], id, LaunchEnvironment());
    }
}

void
ProfilesDocument::RemoveLaunchEnvironment(const std::string& id)
{
    LaunchEnvironments().getObject().erase(id);
}

}  // namespace View
}  // namespace RocProfVis
