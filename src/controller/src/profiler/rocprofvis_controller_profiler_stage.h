// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_controller_handle.h"
#include "rocprofvis_controller_profiler_scrape.h"

namespace RocProfVis
{
namespace Controller
{

/*
 * Authoring handle for one pipeline stage. Copied into ProfilerConfig on
 * add_stage; the caller keeps this object.
 *
 * A caller says what to run - tool, operation, version, argv - and never how
 * to read the output. The scrape rules are chosen from those three by
 * ProfilerScrapeRules at launch, so there is deliberately no way to set a
 * pattern here.
 */
class ProfilerStage : public Handle
{
public:
    ProfilerStage();
    ~ProfilerStage() override;

    rocprofvis_controller_object_type_t GetType(void) final;

    rocprofvis_result_t SetLabel(char const* label);
    rocprofvis_result_t SetTool(rocprofvis_profiler_tool_t tool);
    rocprofvis_result_t SetOperation(rocprofvis_profiler_operation_t operation);
    rocprofvis_result_t SetToolVersion(char const* version);
    rocprofvis_result_t SetToolDirectory(char const* directory);
    rocprofvis_result_t AddArg(char const* arg);
    rocprofvis_result_t SetWorkingDirectory(char const* dir);
    rocprofvis_result_t AddEnvVar(char const* name, char const* value);
    rocprofvis_result_t AddExpectedValue(char const* key, char const* value);
    rocprofvis_result_t SetArtifactDestination(char const* directory);

    ProfilerStageSpec const& Spec() const { return m_spec; }

private:
    ProfilerStageSpec m_spec;
};

} // namespace Controller
} // namespace RocProfVis
