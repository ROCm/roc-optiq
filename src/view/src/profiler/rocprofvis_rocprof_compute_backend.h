// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_profiler_backend.h"
#include <string>

namespace RocProfVis
{
namespace View
{

struct RocprofComputeSettings
{
    enum PcSamplingMethod
    {
        kPcSamplingDisabled,
        kPcSamplingStochastic,
        kPcSamplingHostTrap,
    };
    enum Roofline
    {
        kRooflineEnabledInclusive,
        kRooflineEnabledExclusive,
        kRooflineDisabled,
    };

    // Profile
    bool             profile_overwrite   = false;
    Roofline         profile_roofline    = kRooflineEnabledInclusive;
    PcSamplingMethod profile_pc_sampling = kPcSamplingDisabled;
    TargetSpec       profile_target;
    ExtraArguments   profile_extra_arguments;

    // Analyze
    bool           analyze_output_format_db = true;
    TargetSpec     analyze_target;
    ExtraArguments analyze_extra_arguments;

    jt::Json                      ToJson() const;
    static RocprofComputeSettings FromJson(jt::Json const& json);
};

class RocprofComputeBackend : public IProfilerBackend
{
public:
    RocprofComputeBackend();
    ~RocprofComputeBackend() override = default;

    const char* Id() const override;
    const char* DisplayName() const override;

    std::vector<ToolOption> GetTools() const override;
    TabDescriptor GetOptions(rocprofvis_profiler_tool_t      tool,
                             rocprofvis_profiler_operation_t operation) const override;
    std::vector<TabDescriptor> GetAdvancedOptions(
        rocprofvis_profiler_tool_t      tool,
        rocprofvis_profiler_operation_t operation) const override;

    std::string Validate(LaunchConfig const& config) const override;

    TargetSpec const& GetTarget(rocprofvis_profiler_operation_t operation) const override;
    ExtraArguments&   GetExtraArguments(rocprofvis_profiler_operation_t operation) override;
    std::vector<std::string>& GetRecentTargets(
        rocprofvis_profiler_tool_t      tool,
        rocprofvis_profiler_operation_t operation) override;
    bool RenderTargetSection(LaunchConfig const&   config,
                             TargetBrowseFn const& on_browse) override;

    void FlattenToExecution(LaunchConfig const&                               config,
                            std::vector<std::pair<std::string, std::string>>& env_out,
                            std::vector<std::string>& argv_out) const override;

    jt::Json    SaveSettings() const override;
    void        LoadSettings(jt::Json const& payload) override;
    std::string ExportCfg() const override;

    std::vector<WarningMessage> GetWarnings(LaunchConfig const& config) const override;

    bool ProducesTrace(LaunchConfig const& config) const override;

private:
    bool RenderProfilingOptions(rocprofvis_profiler_operation_t operation);

    RocprofComputeSettings m_settings;
};

}  // namespace View
}  // namespace RocProfVis
