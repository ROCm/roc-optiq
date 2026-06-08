// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_profiler_session.h"
#include "rocprofvis_controller.h"

#include <spdlog/spdlog.h>

namespace RocProfVis
{
namespace View
{

bool
ProfilerSession::Launch(rocprofvis_profiler_type_t profiler_type,
                        const std::string&         profiler_path,
                        const std::string&         target_executable,
                        const std::string&         target_args,
                        const std::string&         output_directory,
                        const std::string&         profiler_args,
                        const std::vector<std::pair<std::string, std::string>>& env_vars)
{
    Close();

    if(!BuildConfig(profiler_type, profiler_path, target_executable, target_args,
                    output_directory, profiler_args, env_vars))
    {
        return false;
    }

    m_profiler = rocprofvis_profiler_alloc();
    if(m_profiler == nullptr)
    {
        spdlog::error("Failed to allocate profiler session");
        Close();
        return false;
    }

    m_future = rocprofvis_controller_future_alloc();
    if(m_future == nullptr)
    {
        spdlog::error("Failed to allocate profiler future");
        Close();
        return false;
    }

    rocprofvis_result_t result = rocprofvis_profiler_launch_async(m_profiler, m_config, m_future);
    if(result != kRocProfVisResultSuccess)
    {
        spdlog::error("Failed to launch profiler: error code {}", static_cast<int>(result));
        Close();
        return false;
    }

    RegisterProfilerMonitor();

    spdlog::info("Profiler launched successfully (monitor op {})", GetOperationId());
    return true;
}

}  // namespace View
}  // namespace RocProfVis
