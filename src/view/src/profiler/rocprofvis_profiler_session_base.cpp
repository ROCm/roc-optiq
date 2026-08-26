// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_profiler_session_base.h"
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

ProfilerSessionBase::ProfilerSessionBase()
: m_config(nullptr)
, m_profiler(nullptr)
, m_future(nullptr)
, m_profiler_op_id(0)
{
}

ProfilerSessionBase::~ProfilerSessionBase()
{
    Close();
}

bool
ProfilerSessionBase::BuildConfig(const ProfilerLaunchSpec& spec)
{
    m_config = rocprofvis_profiler_config_alloc();
    if(m_config == nullptr)
    {
        spdlog::error("Failed to allocate profiler config");
        return false;
    }

    if(rocprofvis_profiler_config_set_tool(m_config, spec.tool) != kRocProfVisResultSuccess)
    {
        spdlog::error("Failed to set profiler tool ({})", static_cast<int>(spec.tool));
        return false;
    }
    if(!spec.tool_directory.empty())
    {
        rocprofvis_profiler_config_set_tool_directory(m_config, spec.tool_directory.c_str());
    }
    rocprofvis_profiler_config_set_output_directory(m_config, spec.output_directory.c_str());
    rocprofvis_profiler_config_set_working_directory(m_config, spec.working_directory.c_str());

    // One call per argument: the controller passes each entry through to the
    // process untouched, so an argument containing spaces stays one argument.
    for(auto const& arg : spec.profiler_argv)
    {
        rocprofvis_profiler_config_add_profiler_arg(m_config, arg.c_str());
    }

    for(auto const& kv : spec.env_vars)
    {
        rocprofvis_profiler_config_add_env_var(m_config, kv.first.c_str(), kv.second.c_str());
    }

    return true;
}

uint64_t
ProfilerSessionBase::RegisterProfilerMonitor()
{
    // Register with the monitor as a pure status poller. The status callback
    // must read the LIVE controller state (not a cached value, which would be
    // circular and never change). The event factory reads m_profiler_op_id at
    // emit time so each event carries the owning session's id.
    m_profiler_op_id = AppMonitor::GetInstance()->AddOperation(
        MonitorOperationType::ProfilerSession,
        [this]() -> uint64_t { return ReadProfilerState(); },
        [this](uint64_t status) -> std::shared_ptr<RocEvent>
        {
            return std::make_shared<ProfilerStatusEvent>(
                m_profiler_op_id, static_cast<rocprofvis_profiler_state_t>(status));
        },
        [](uint64_t status) -> bool { return IsTerminalProfilerState(status); });

    return m_profiler_op_id;
}

uint64_t
ProfilerSessionBase::ReadProfilerState() const
{
    rocprofvis_profiler_state_t state = kRPVProfilerStateIdle;
    if(m_profiler != nullptr)
    {
        rocprofvis_profiler_get_state(m_profiler, &state);
    }
    return static_cast<uint64_t>(state);
}

bool
ProfilerSessionBase::IsTerminalProfilerState(uint64_t state)
{
    return is_terminal_state(static_cast<rocprofvis_profiler_state_t>(state));
}

rocprofvis_profiler_state_t
ProfilerSessionBase::GetState() const
{
    return static_cast<rocprofvis_profiler_state_t>(ReadProfilerState());
}

std::string
ProfilerSessionBase::GetOutput()
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

    std::vector<char>   buffer(length + 1, '\0');
    rocprofvis_result_t result = rocprofvis_profiler_get_output(m_profiler, buffer.data(), &length);
    if(result != kRocProfVisResultSuccess)
    {
        return "";
    }

    buffer[length] = '\0';
    return std::string(buffer.data());
}

void
ProfilerSessionBase::ClearOutput()
{
    if(m_profiler != nullptr)
    {
        rocprofvis_profiler_clear_output(m_profiler);
    }
}

int32_t
ProfilerSessionBase::GetExitCode() const
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
ProfilerSessionBase::Cancel()
{
    if(m_profiler == nullptr)
    {
        return false;
    }

    rocprofvis_result_t result = rocprofvis_profiler_cancel(m_profiler);
    return (result == kRocProfVisResultSuccess);
}

void
ProfilerSessionBase::Close()
{
    FreeProfilerObjects();
}

void
ProfilerSessionBase::FreeProfilerObjects()
{
    // Stop the status poller (it captures `this`, which may be about to go
    // away).
    if(m_profiler_op_id != 0)
    {
        AppMonitor::GetInstance()->RemoveOperation(m_profiler_op_id);
        m_profiler_op_id = 0;
    }

    if(m_profiler == nullptr && m_future == nullptr && m_config == nullptr)
    {
        // No profiler objects, but a subclass may still have handed us an extra
        // teardown (e.g. an SSH connection); run it once.
        if(m_extra_teardown)
        {
            m_extra_teardown();
            m_extra_teardown = nullptr;
        }
        return;
    }

    // If the job is still running, the worker is touching the profiler handle.
    // Transfer the resources to the monitor for deferred, non-blocking teardown
    // (cancel signalled, freed once the future resolves). Capture BY VALUE so
    // the closures remain valid after this session is destroyed. The extra
    // teardown (subclass-owned resource, e.g. the borrowed SSH connection) runs
    // LAST, after the profiler is freed and its worker joined, so the resource
    // outlives the worker.
    if(m_future != nullptr &&
       rocprofvis_controller_future_wait(m_future, 0) == kRocProfVisResultTimeout)
    {
        rocprofvis_profiler_config_t*   config   = m_config;
        rocprofvis_profiler_t*          profiler = m_profiler;
        rocprofvis_controller_future_t* future   = m_future;
        std::function<void()>           extra    = std::move(m_extra_teardown);
        AppMonitor::GetInstance()->AddTeardownOp(
            MonitorOperationType::ProfilerSession,
            future,
            [profiler]() { rocprofvis_profiler_cancel(profiler); },
            [config, profiler, future, extra]()
            {
                rocprofvis_controller_future_free(future);
                rocprofvis_profiler_free(profiler);
                rocprofvis_profiler_config_free(config);
                if(extra)
                {
                    extra();
                }
            });
        m_future         = nullptr;
        m_profiler       = nullptr;
        m_config         = nullptr;
        m_extra_teardown = nullptr;
        return;
    }

    // Future resolved (or none): no worker is using the resources; free now.
    if(m_future != nullptr)
    {
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
    // Free the subclass-owned resource last, after the profiler (and its worker)
    // is gone.
    if(m_extra_teardown)
    {
        m_extra_teardown();
        m_extra_teardown = nullptr;
    }
}

}  // namespace View
}  // namespace RocProfVis
