// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_launch_config.h"
#include "rocprofvis_profiler_backend.h"
#include <string>
#include <vector>

namespace RocProfVis
{
namespace View
{

struct PresetInfo
{
    std::string name;
    std::string file_path;
};

class LaunchPresetManager
{
public:
    LaunchPresetManager();
    ~LaunchPresetManager() = default;

    std::vector<PresetInfo> ListPresets(std::string const& profiler_id) const;

    bool SavePreset(std::string const& name, LaunchConfig const& config,
                    IProfilerBackend const* backend);

    bool LoadPreset(std::string const& name, std::string const& profiler_id,
                    LaunchConfig& config_out) const;

    bool DeletePreset(std::string const& name, std::string const& profiler_id);

    bool ExportCfg(std::string const& file_path, LaunchConfig const& config,
                   IProfilerBackend const* backend) const;

    bool ImportPreset(std::string const& file_path, std::string const& profiler_id,
                      LaunchConfig& config_out) const;

private:
    std::string GetPresetsDirectory(std::string const& profiler_id) const;
    std::string GetPresetFilePath(std::string const& name, std::string const& profiler_id) const;
};

} // namespace View
} // namespace RocProfVis
