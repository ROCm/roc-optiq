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
ProfilerSession::Launch(const ProfilerLaunchSpec& spec)
{
    Close();

    if(!BuildConfig(spec))
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
