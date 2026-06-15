// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_profiles_document.h"
#include "rocprofvis_json_utils.h"
#include "rocprofvis_utils.h"

#include <filesystem>

namespace RocProfVis
{
namespace View
{

namespace
{
    constexpr int kProfilesVersion = 1;
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
    EnsureSections();
}

void ProfilesDocument::EnsureSections()
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
}

bool ProfilesDocument::Persist()
{
    EnsureSections();
    return JsonUtils::WriteFile(GetFilePath(), m_root, true);
}

jt::Json& ProfilesDocument::SshConnections()
{
    EnsureSections();
    return m_root["ssh_connections"];
}

jt::Json& ProfilesDocument::LaunchProfiles()
{
    EnsureSections();
    return m_root["launch_profiles"];
}

}  // namespace View
}  // namespace RocProfVis
