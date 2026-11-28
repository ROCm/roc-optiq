// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_profiler_backend.h"
#include <bitset>
#include <string>

namespace RocProfVis
{
namespace View
{

struct RocprofV3Settings
{
    enum Tracing : uint32_t
    {
        kAggregateRuntimeTrace,
        kAggregateSysTrace,
        kAggregateTraceLast,
        kBasicHipTrace = kAggregateTraceLast,
        kBasicMarkerTrace,
        kBasicKernelTrace,
        kBasicMemoryCopyTrace,
        kBasicMemoryAllocationTrace,
        kBasicKFDTrace,
        kBasicScratchMemoryTrace,
        kBasicHSATrace,
        kBasicRCCLTrace,
        kBasicKokkosTrace,
        kBasicRocDecodeTrace,
        kBasicHIPEventTrace,
        kBasicHIPFILETrace,
        kTracingLast,
    };
    enum WindowUnit : uint32_t
    {
        kNanoseconds,
        kMicroseconds,
        kMilliseconds,
        kSeconds,
        kMinutes,
        kHours,
        kWindowUnitLast,
    };

    std::bitset<kTracingLast> collection_tracing;
    std::string               collection_counters;

    bool       window_enabled;
    uint32_t   window_delay;
    uint32_t   window_duration;
    uint32_t   window_repeat;
    WindowUnit window_unit;

    TargetSpec     target;
    ExtraArguments extra_arguments;

    jt::Json                 ToJson() const;
    static RocprofV3Settings FromJson(jt::Json const& json);
};

class RocprofV3Backend : public IProfilerBackend
{
public:
    RocprofV3Backend();
    ~RocprofV3Backend() override = default;

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
    ExtraArguments& GetExtraArguments(rocprofvis_profiler_operation_t operation) override;
    std::vector<std::string>& GetProfilerRecentTargets(
        LaunchConfig const& config) override;
    bool  RenderTargetSection(LaunchConfig const&   config,
                              TargetBrowseFn const& on_browse) override;
    float MinWidth() const override;

    void FlattenToExecution(LaunchConfig const&                               config,
                            std::vector<std::pair<std::string, std::string>>& env_out,
                            std::vector<std::string>& argv_out) const override;

    jt::Json    SaveSettings() const override;
    void        LoadSettings(jt::Json const& payload) override;
    std::string ExportCfg() const override;

    std::vector<WarningMessage> GetWarnings(LaunchConfig const& config) const override;

private:
    bool RenderProfilingOptions(rocprofvis_profiler_operation_t operation);

    RocprofV3Settings m_settings;
};

}  // namespace View
}  // namespace RocProfVis
