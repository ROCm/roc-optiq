// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_profiler_session.h"
#include "rocprofvis_appmonitor.h"
#include "rocprofvis_controller.h"
#include "rocprofvis_events.h"

#include <spdlog/spdlog.h>

namespace RocProfVis
{
namespace View
{

namespace
{
bool is_terminal_state(rocprofvis_profiler_state_t state)
{
    return state == kRPVProfilerStateCompleted || state == kRPVProfilerStateFailed ||
           state == kRPVProfilerStateCancelled;
}
}  // namespace

ProfilerSession::ProfilerSession()
: m_config(nullptr)
, m_profiler(nullptr)
, m_future(nullptr)
, m_operation_id(0)
{
}

ProfilerSession::~ProfilerSession()
{
    Close();
}

void
ProfilerSession::StopMonitoring()
{
    if(m_operation_id != 0)
    {
        AppMonitor::GetInstance()->RemoveOperation(m_operation_id);
        m_operation_id = 0;
    }
}

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

    m_config = rocprofvis_profiler_config_alloc();
    if(m_config == nullptr)
    {
        spdlog::error("Failed to allocate profiler config");
        return false;
    }

    rocprofvis_profiler_config_set_type(m_config, profiler_type);
    rocprofvis_profiler_config_set_profiler_path(m_config, profiler_path.c_str());
    rocprofvis_profiler_config_set_target_executable(m_config, target_executable.c_str());
    rocprofvis_profiler_config_set_target_args(m_config, target_args.c_str());
    rocprofvis_profiler_config_set_output_directory(m_config, output_directory.c_str());
    rocprofvis_profiler_config_set_profiler_args(m_config, profiler_args.c_str());

    for(auto const& kv : env_vars)
    {
        rocprofvis_profiler_config_add_env_var(m_config, kv.first.c_str(), kv.second.c_str());
    }

    m_profiler = rocprofvis_profiler_alloc();
    if(m_profiler == nullptr)
    {
        spdlog::error("Failed to allocate profiler session");
        rocprofvis_profiler_config_free(m_config);
        m_config = nullptr;
        return false;
    }

    m_future = rocprofvis_controller_future_alloc();
    if(m_future == nullptr)
    {
        spdlog::error("Failed to allocate profiler future");
        rocprofvis_profiler_free(m_profiler);
        m_profiler = nullptr;
        rocprofvis_profiler_config_free(m_config);
        m_config = nullptr;
        return false;
    }

    rocprofvis_result_t result = rocprofvis_profiler_launch_async(m_profiler, m_config, m_future);
    if(result != kRocProfVisResultSuccess)
    {
        spdlog::error("Failed to launch profiler: error code {}", static_cast<int>(result));
        rocprofvis_controller_future_free(m_future);
        m_future = nullptr;
        rocprofvis_profiler_free(m_profiler);
        m_profiler = nullptr;
        rocprofvis_profiler_config_free(m_config);
        m_config = nullptr;
        return false;
    }

    // Register with the monitor so state transitions are surfaced as events.
    // The callbacks capture `this`; the session always outlives its monitor
    // registration because Close()/dtor unregister before freeing state. The
    // event factory reads m_operation_id at emit time so each event carries the
    // owning session's id (concurrent profiler sessions are distinguishable).
    m_operation_id = AppMonitor::GetInstance()->AddOperation(
        MonitorOperationType::ProfilerSession,
        [this]() -> uint64_t { return static_cast<uint64_t>(GetState()); },
        [this](uint64_t status) -> std::shared_ptr<RocEvent>
        {
            return std::make_shared<ProfilerStatusEvent>(
                m_operation_id, static_cast<rocprofvis_profiler_state_t>(status));
        },
        [](uint64_t status) -> bool
        {
            return is_terminal_state(static_cast<rocprofvis_profiler_state_t>(status));
        });

    spdlog::info("Profiler launched successfully (monitor op {})", m_operation_id);
    return true;
}

rocprofvis_profiler_state_t
ProfilerSession::GetState() const
{
    if(m_profiler == nullptr)
    {
        return kRPVProfilerStateIdle;
    }

    rocprofvis_profiler_state_t state = kRPVProfilerStateIdle;
    rocprofvis_profiler_get_state(m_profiler, &state);
    return state;
}

std::string
ProfilerSession::GetOutput()
{
    if(m_profiler == nullptr)
    {
        return "";
    }

    uint32_t length = 0;
    rocprofvis_profiler_get_output(m_profiler, nullptr, &length);
    if(length == 0)
    {
        return "";
    }

    std::vector<char>   buffer(length + 1);
    rocprofvis_result_t result = rocprofvis_profiler_get_output(m_profiler, buffer.data(), &length);
    if(result != kRocProfVisResultSuccess)
    {
        return "";
    }

    buffer[length] = '\0';
    return std::string(buffer.data());
}

void
ProfilerSession::ClearOutput()
{
    if(m_profiler != nullptr)
    {
        rocprofvis_profiler_clear_output(m_profiler);
    }
}

std::string
ProfilerSession::GetTracePath()
{
    if(m_profiler == nullptr)
    {
        return "";
    }

    uint32_t length = 0;
    rocprofvis_profiler_get_trace_path(m_profiler, nullptr, &length);
    if(length == 0)
    {
        return "";
    }

    std::vector<char>   buffer(length + 1);
    rocprofvis_result_t result =
        rocprofvis_profiler_get_trace_path(m_profiler, buffer.data(), &length);
    if(result != kRocProfVisResultSuccess)
    {
        return "";
    }

    buffer[length] = '\0';
    return std::string(buffer.data());
}

int32_t
ProfilerSession::GetExitCode() const
{
    if(m_profiler == nullptr)
    {
        return -1;
    }

    int32_t exit_code = -1;
    rocprofvis_profiler_get_exit_code(m_profiler, &exit_code);
    return exit_code;
}

bool
ProfilerSession::Cancel()
{
    if(m_profiler == nullptr)
    {
        return false;
    }

    rocprofvis_result_t result = rocprofvis_profiler_cancel(m_profiler);
    return (result == kRocProfVisResultSuccess);
}

void
ProfilerSession::Close()
{
    // Unregister from the monitor before tearing down state so no event factory
    // observes freed objects.
    StopMonitoring();

    // Cancel via the session handle: terminates the child process AND forwards
    // Cancel() to the bound future so any waiter unblocks.
    if(m_profiler != nullptr)
    {
        rocprofvis_profiler_cancel(m_profiler);
    }

    if(m_future != nullptr)
    {
        rocprofvis_controller_future_wait(m_future, 5.0f);
        rocprofvis_controller_future_free(m_future);
        m_future = nullptr;
    }

    if(m_profiler != nullptr)
    {
        rocprofvis_profiler_free(m_profiler);
        m_profiler = nullptr;
    }

    if(m_config != nullptr)
    {
        rocprofvis_profiler_config_free(m_config);
        m_config = nullptr;
    }
}

}  // namespace View
}  // namespace RocProfVis
