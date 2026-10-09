// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_controller_profiler_stage.h"
#include "rocprofvis_controller_profiler_cmdline.h"
#include "rocprofvis_controller_profiler_tool.h"

namespace RocProfVis
{
namespace Controller
{

ProfilerStage::ProfilerStage()
    : Handle(0, 0)
    , m_spec()
{
}

ProfilerStage::~ProfilerStage()
{
}

rocprofvis_controller_object_type_t ProfilerStage::GetType(void)
{
    return kRPVProfilerStage;
}

rocprofvis_result_t ProfilerStage::SetLabel(char const* label)
{
    if (label == nullptr)
    {
        return kRocProfVisResultInvalidArgument;
    }
    m_spec.label = label;
    return kRocProfVisResultSuccess;
}

rocprofvis_result_t ProfilerStage::SetTool(rocprofvis_profiler_tool_t tool)
{
    if (ProfilerTool::GetBinaryName(tool) == nullptr)
    {
        return kRocProfVisResultInvalidEnum;
    }
    m_spec.tool = tool;
    return kRocProfVisResultSuccess;
}

rocprofvis_result_t ProfilerStage::SetOperation(rocprofvis_profiler_operation_t operation)
{
    if (operation >= __kRPVProfilerOperationLast)
    {
        return kRocProfVisResultInvalidEnum;
    }
    m_spec.operation = operation;
    return kRocProfVisResultSuccess;
}

rocprofvis_result_t ProfilerStage::SetToolVersion(char const* version)
{
    if (version == nullptr)
    {
        return kRocProfVisResultInvalidArgument;
    }
    // An unparsable version is not rejected: it selects the base rules, which
    // is what should happen when a tool reports something we have not seen.
    m_spec.tool_version = version;
    return kRocProfVisResultSuccess;
}

rocprofvis_result_t ProfilerStage::SetToolDirectory(char const* directory)
{
    if (directory == nullptr)
    {
        return kRocProfVisResultInvalidArgument;
    }
    m_spec.tool_directory = directory;
    return kRocProfVisResultSuccess;
}

rocprofvis_result_t ProfilerStage::AddArg(char const* arg)
{
    if (arg == nullptr)
    {
        return kRocProfVisResultInvalidArgument;
    }
    m_spec.argv.push_back(arg);
    return kRocProfVisResultSuccess;
}

rocprofvis_result_t ProfilerStage::SetWorkingDirectory(char const* dir)
{
    if (dir == nullptr)
    {
        return kRocProfVisResultInvalidArgument;
    }
    m_spec.working_directory = dir;
    return kRocProfVisResultSuccess;
}

rocprofvis_result_t ProfilerStage::AddEnvVar(char const* name, char const* value)
{
    if (name == nullptr || value == nullptr)
    {
        return kRocProfVisResultInvalidArgument;
    }
    if (!Cmdline::IsValidEnvName(name))
    {
        return kRocProfVisResultInvalidArgument;
    }
    m_spec.env.emplace_back(name, value);
    return kRocProfVisResultSuccess;
}

rocprofvis_result_t ProfilerStage::AddExpectedValue(char const* key, char const* value)
{
    if (key == nullptr || value == nullptr || key[0] == '\0')
    {
        return kRocProfVisResultInvalidArgument;
    }
    m_spec.expected.emplace_back(key, value);
    return kRocProfVisResultSuccess;
}

rocprofvis_result_t ProfilerStage::SetArtifactDestination(char const* directory)
{
    if (directory == nullptr)
    {
        return kRocProfVisResultInvalidArgument;
    }
    m_spec.relocate_to = directory;
    return kRocProfVisResultSuccess;
}

} // namespace Controller
} // namespace RocProfVis
