// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_launch_preset_manager.h"
#include "rocprofvis_utils.h"
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

std::string LaunchPresetManager::GetPresetsDirectory(std::string const& profiler_id) const
{
    std::string config_path = get_application_config_path(true);
    std::filesystem::path dir = std::filesystem::path(config_path) / "launch_presets" / profiler_id;
    return dir.string();
}

std::string LaunchPresetManager::GetPresetFilePath(std::string const& name,
                                                    std::string const& profiler_id) const
{
    std::filesystem::path dir(GetPresetsDirectory(profiler_id));
    return (dir / (name + ".json")).string();
}

std::vector<PresetInfo> LaunchPresetManager::ListPresets(std::string const& profiler_id) const
{
    std::vector<PresetInfo> presets;
    std::string dir_path = GetPresetsDirectory(profiler_id);

    if (!std::filesystem::exists(dir_path))
    {
        return presets;
    }

    for (auto const& entry : std::filesystem::directory_iterator(dir_path))
    {
        if (!entry.is_regular_file())
        {
            continue;
        }
        if (entry.path().extension() != ".json")
        {
            continue;
        }

        PresetInfo info;
        info.name = entry.path().stem().string();
        info.file_path = entry.path().string();
        presets.push_back(info);
    }

    return presets;
}

bool LaunchPresetManager::SavePreset(std::string const& name, LaunchConfig const& config,
                                      IProfilerBackend const* /*backend*/)
{
    std::string dir_path = GetPresetsDirectory(config.profiler_id);

    std::error_code ec;
    std::filesystem::create_directories(dir_path, ec);
    if (ec)
    {
        spdlog::error("Failed to create presets directory '{}': {}", dir_path, ec.message());
        return false;
    }

    std::string file_path = GetPresetFilePath(name, config.profiler_id);

    jt::Json json = config.ToJson();
    std::string json_str = json.toString();

    std::ofstream ofs(file_path);
    if (!ofs.is_open())
    {
        spdlog::error("Failed to open preset file for writing: {}", file_path);
        return false;
    }

    ofs << json_str;
    ofs.close();

    spdlog::info("Saved preset '{}' to {}", name, file_path);
    return true;
}

bool LaunchPresetManager::LoadPreset(std::string const& name, std::string const& profiler_id,
                                      LaunchConfig& config_out) const
{
    std::string file_path = GetPresetFilePath(name, profiler_id);

    if (!std::filesystem::exists(file_path))
    {
        spdlog::error("Preset file not found: {}", file_path);
        return false;
    }

    std::ifstream ifs(file_path);
    if (!ifs.is_open())
    {
        spdlog::error("Failed to open preset file: {}", file_path);
        return false;
    }

    std::ostringstream ss;
    ss << ifs.rdbuf();
    std::string json_str = ss.str();
    ifs.close();

    auto [status, json] = jt::Json::parse(json_str);
    if (status != jt::Json::success)
    {
        spdlog::error("Failed to parse preset file '{}': parse error", file_path);
        return false;
    }

    config_out = LaunchConfig::FromJson(json);
    spdlog::info("Loaded preset '{}' from {}", name, file_path);
    return true;
}

bool LaunchPresetManager::DeletePreset(std::string const& name, std::string const& profiler_id)
{
    std::string file_path = GetPresetFilePath(name, profiler_id);

    std::error_code ec;
    if (std::filesystem::remove(file_path, ec))
    {
        spdlog::info("Deleted preset '{}' ({})", name, file_path);
        return true;
    }

    spdlog::error("Failed to delete preset '{}': {}", file_path, ec.message());
    return false;
}

bool LaunchPresetManager::ExportCfg(std::string const& file_path, LaunchConfig const& config,
                                     IProfilerBackend const* backend) const
{
    if (backend == nullptr)
    {
        return false;
    }

    std::string cfg_content = backend->ExportCfg(config.backend_payload);
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

    // Try JSON preset first
    auto [status, json] = jt::Json::parse(content);
    if (status == jt::Json::success)
    {
        config_out = LaunchConfig::FromJson(json);
        return true;
    }

    // Fall back to .cfg format: KEY = VALUE lines -> extra_env
    config_out = LaunchConfig();
    config_out.profiler_id = profiler_id;

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
            config_out.extra_env[key] = val;
        }
    }

    spdlog::info("Imported .cfg file as raw env vars: {}", file_path);
    return true;
}

} // namespace View
} // namespace RocProfVis
