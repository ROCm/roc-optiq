// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_launch_preset_manager.h"
#include "rocprofvis_utils.h"
#include "rocprofvis_json_utils.h"
#include "rocprofvis_profiles_document.h"
#include "spdlog/spdlog.h"
#include <filesystem>
#include <fstream>
#include <sstream>

namespace RocProfVis
{
namespace View
{

LaunchPresetManager::LaunchPresetManager()
{
}

std::vector<PresetInfo> LaunchPresetManager::ListPresets(std::string const& profiler_id) const
{
    std::vector<PresetInfo> presets;

    // Profiles are grouped by profiler, so each profiler has its own names.
    jt::Json& profiles = ProfilesDocument::Get().LaunchProfiles();
    if (!profiles.contains(profiler_id) || !profiles[profiler_id].isObject())
    {
        return presets;
    }

    for (std::pair<std::string const, jt::Json>& entry : profiles[profiler_id].getObject())
    {
        PresetInfo info;
        info.name = entry.first;
        info.file_path = entry.first;  // logical name; no longer a filesystem path
        presets.push_back(info);
    }

    return presets;
}

bool LaunchPresetManager::SavePreset(std::string const& name, LaunchConfig const& config,
                                      IProfilerBackend const* /*backend*/)
{
    if (name.empty())
    {
        spdlog::error("Cannot save launch profile with empty name");
        return false;
    }

    jt::Json& group = ProfilesDocument::Get().LaunchProfiles()[config.profiler_id];
    if (!group.isObject())
    {
        group.setObject();
    }
    group[name] = config.ToJson();

    if (!ProfilesDocument::Get().Persist())
    {
        spdlog::error("Failed to persist launch profile '{}'", name);
        return false;
    }

    spdlog::info("Saved launch profile '{}'", name);
    return true;
}

bool LaunchPresetManager::LoadPreset(std::string const& name, std::string const& profiler_id,
                                      LaunchConfig& config_out) const
{
    jt::Json& profiles = ProfilesDocument::Get().LaunchProfiles();
    if (!profiles.contains(profiler_id) || !profiles[profiler_id].isObject() ||
        !profiles[profiler_id].contains(name))
    {
        spdlog::error("Launch profile not found: {}", name);
        return false;
    }

    config_out = LaunchConfig::FromJson(profiles[profiler_id][name]);
    spdlog::info("Loaded launch profile '{}'", name);
    return true;
}

bool LaunchPresetManager::DeletePreset(std::string const& name, std::string const& profiler_id)
{
    jt::Json& profiles = ProfilesDocument::Get().LaunchProfiles();
    if (!profiles.contains(profiler_id) || !profiles[profiler_id].isObject() ||
        !profiles[profiler_id].contains(name))
    {
        spdlog::error("Launch profile not found for delete: {}", name);
        return false;
    }

    profiles[profiler_id].getObject().erase(name);

    if (!ProfilesDocument::Get().Persist())
    {
        spdlog::error("Failed to persist after deleting launch profile '{}'", name);
        return false;
    }

    spdlog::info("Deleted launch profile '{}'", name);
    return true;
}

bool LaunchPresetManager::ExportCfg(std::string const& file_path, LaunchConfig const& config,
                                     IProfilerBackend const* backend) const
{
    if (backend == nullptr)
    {
        return false;
    }

    (void)config;
    std::string cfg_content = backend->ExportCfg();
    if (cfg_content.empty())
    {
        spdlog::warn("Backend '{}' does not support .cfg export", backend->Id());
        return false;
    }

    std::ofstream ofs(file_path);
    if (!ofs.is_open())
    {
        spdlog::error("Failed to open file for .cfg export: {}", file_path);
        return false;
    }

    ofs << cfg_content;
    ofs.close();

    spdlog::info("Exported .cfg to {}", file_path);
    return true;
}

bool LaunchPresetManager::ImportPreset(std::string const& file_path,
                                        std::string const& profiler_id,
                                        LaunchConfig& config_out) const
{
    if (!std::filesystem::exists(file_path))
    {
        spdlog::error("Import file not found: {}", file_path);
        return false;
    }

    // Try JSON preset first.
    jt::Json json;
    if (JsonUtils::ReadFile(file_path, json))
    {
        config_out = LaunchConfig::FromJson(json);
        return true;
    }

    // Fall back to .cfg format: KEY = VALUE lines -> the backend's extra
    // environment variables
    std::ifstream ifs(file_path);
    if (!ifs.is_open())
    {
        spdlog::error("Failed to open import file: {}", file_path);
        return false;
    }

    std::ostringstream ss;
    ss << ifs.rdbuf();
    std::string content = ss.str();
    ifs.close();

    config_out = LaunchConfig();
    config_out.profiler_id = profiler_id;
    ExtraArguments extra;

    std::istringstream lines(content);
    std::string line;
    while (std::getline(lines, line))
    {
        if (line.empty() || line[0] == '#')
        {
            continue;
        }
        auto eq_pos = line.find('=');
        if (eq_pos == std::string::npos)
        {
            continue;
        }

        std::string key = line.substr(0, eq_pos);
        std::string val = line.substr(eq_pos + 1);

        // Trim whitespace
        auto trim = [](std::string& s)
        {
            size_t start = s.find_first_not_of(" \t");
            size_t end = s.find_last_not_of(" \t");
            if (start == std::string::npos)
            {
                s.clear();
            }
            else
            {
                s = s.substr(start, end - start + 1);
            }
        };
        trim(key);
        trim(val);

        if (!key.empty())
        {
            extra.env[key] = val;
        }
    }
    config_out.backend_payload["extra_arguments"] = extra.ToJson();

    spdlog::info("Imported .cfg file as raw env vars: {}", file_path);
    return true;
}

} // namespace View
} // namespace RocProfVis
