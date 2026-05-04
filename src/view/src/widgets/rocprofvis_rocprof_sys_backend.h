// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_profiler_backend.h"

namespace RocProfVis
{
namespace View
{

class RocprofSysBackend : public IProfilerBackend
{
public:
    RocprofSysBackend();
    ~RocprofSysBackend() override = default;

    const char* Id() const override;
    const char* DisplayName() const override;

    std::vector<ToolOption> GetTools() const override;
    std::string GetDefaultBinary(std::string const& tool_id) const override;
    std::vector<TabDescriptor> GetTabs(std::string const& tool_id) const override;

    std::string Validate(LaunchConfig const& config) const override;

    void FlattenToExecution(
        LaunchConfig const& config,
        std::vector<std::pair<std::string, std::string>>& env_out,
        std::vector<std::string>& argv_out) const override;

    jt::Json DefaultPayload() const override;
    std::string ExportCfg(jt::Json const& payload) const override;

private:
    void RenderGeneralTab(jt::Json& payload);
    void RenderBackendsTab(jt::Json& payload);
    void RenderSamplingTab(jt::Json& payload);
    void RenderRocmTab(jt::Json& payload);
    void RenderPerfettoTab(jt::Json& payload);
    void RenderProcessSamplingTab(jt::Json& payload);
    void RenderParallelismTab(jt::Json& payload);
    void RenderInstrumentTab(jt::Json& payload);
    void RenderAdvancedTab(jt::Json& payload);
};

} // namespace View
} // namespace RocProfVis
