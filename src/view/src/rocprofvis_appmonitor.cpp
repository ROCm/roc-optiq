// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_appmonitor.h"
#include "rocprofvis_event_manager.h"
#include "rocprofvis_controller.h"

#include <spdlog/spdlog.h>

#include <cfloat>

namespace RocProfVis
{
namespace View
{

namespace
{
// Bounded wait used only during process teardown to drain any operations that
// have not resolved yet. Normal cancellation never blocks.
constexpr float kShutdownDrainTimeoutSeconds = 5.0f;
}  // namespace

AppMonitor* AppMonitor::s_instance = nullptr;

AppMonitor*
AppMonitor::GetInstance()
{
    if(!s_instance)
    {
        s_instance = new AppMonitor();
    }
    return s_instance;
}

void
AppMonitor::DestroyInstance()
{
    if(s_instance)
    {
        delete s_instance;
        s_instance = nullptr;
    }
}

AppMonitor::AppMonitor()
: m_next_operation_id(1)
{
}

AppMonitor::~AppMonitor()
{
    // Process teardown: any operation still tracked here means its session was
    // already destroyed (having transferred its resources to us). Drain each
    // with a bounded blocking wait - acceptable only at true process exit - so
    // no background job is left holding freed resources.
    for(auto& entry : m_operations)
    {
        MonitorOperation& op = entry.second;
        if(op.future != nullptr)
        {
            if(op.cancel_fn)
            {
                op.cancel_fn();
            }
            rocprofvis_controller_future_cancel(op.future);
            if(rocprofvis_controller_future_wait(op.future, kShutdownDrainTimeoutSeconds) ==
               kRocProfVisResultTimeout)
            {
                spdlog::warn("AppMonitor: operation {} did not resolve before shutdown drain "
                             "timeout; freeing anyway",
                             op.operation_id);
            }
        }
        ReapOperation(op);
    }
    m_operations.clear();
}

uint64_t
AppMonitor::AddOperation(MonitorOperationType            type,
                         MonitorOperation::StatusFn      status_fn,
                         MonitorOperation::EventFactory  event_factory,
                         MonitorOperation::IsTerminalFn  is_terminal_fn,
                         MonitorOperation::CancelFn      cancel_fn,
                         rocprofvis_controller_future_t* future,
                         MonitorOperation::TeardownFn    teardown_fn)
{
    ROCPROFVIS_ASSERT(status_fn != nullptr);

    uint64_t id = m_next_operation_id++;

    MonitorOperation op;
    op.operation_id   = id;
    op.operation_type = type;
    op.status_fn      = std::move(status_fn);
    op.event_factory  = std::move(event_factory);
    op.is_terminal_fn = std::move(is_terminal_fn);
    op.cancel_fn      = std::move(cancel_fn);
    op.future         = future;
    op.last_status    = 0;
    op.has_status     = false;
    op.cancelling     = false;
    if(teardown_fn)
    {
        op.teardowns.push_back(std::move(teardown_fn));
    }

    m_operations.emplace(id, std::move(op));
    return id;
}

bool
AppMonitor::AppendTeardown(uint64_t operation_id, MonitorOperation::TeardownFn teardown_fn)
{
    auto it = m_operations.find(operation_id);
    if(it == m_operations.end())
    {
        return false;
    }
    if(teardown_fn)
    {
        it->second.teardowns.push_back(std::move(teardown_fn));
    }
    return true;
}

void
AppMonitor::AddTeardownOp(MonitorOperationType             type,
                          rocprofvis_controller_future_t*  future,
                          MonitorOperation::CancelFn       cancel_fn,
                          MonitorOperation::TeardownFn     teardown_fn)
{
    // Create a bare operation with no status/event callbacks: it exists solely
    // so Update() can reap it (running teardown_fn) once the future resolves.
    uint64_t id = m_next_operation_id++;

    MonitorOperation op;
    op.operation_id   = id;
    op.operation_type = type;
    op.status_fn      = nullptr;
    op.event_factory  = nullptr;
    op.is_terminal_fn = nullptr;
    op.cancel_fn      = std::move(cancel_fn);
    op.future         = future;
    op.last_status    = 0;
    op.has_status     = false;
    op.cancelling     = true;  // never polled; only drained
    if(teardown_fn)
    {
        op.teardowns.push_back(std::move(teardown_fn));
    }

    // Signal cancel immediately so the worker unblocks; reaping happens in
    // Update() once the future resolves (or right away if already resolved).
    if(op.cancel_fn)
    {
        op.cancel_fn();
    }
    if(op.future != nullptr)
    {
        rocprofvis_controller_future_cancel(op.future);
    }

    m_operations.emplace(id, std::move(op));
}

bool
AppMonitor::FutureResolved(MonitorOperation& op)
{
    if(op.future == nullptr)
    {
        return true;
    }
    // Non-blocking probe. The future is freed by a teardown closure, never here.
    return rocprofvis_controller_future_wait(op.future, 0) != kRocProfVisResultTimeout;
}

void
AppMonitor::ReapOperation(MonitorOperation& op)
{
    // Run teardowns in registration order (future first, then transferred
    // resources such as an SSH connection).
    for(auto& teardown : op.teardowns)
    {
        if(teardown)
        {
            teardown();
        }
    }
    op.teardowns.clear();
    op.future = nullptr;
}

void
AppMonitor::RemoveOperation(uint64_t operation_id)
{
    auto it = m_operations.find(operation_id);
    if(it == m_operations.end())
    {
        return;
    }

    MonitorOperation& op = it->second;

    // If the future is already resolved (or absent), reap immediately.
    if(FutureResolved(op))
    {
        ReapOperation(op);
        m_operations.erase(it);
        return;
    }

    // Still in flight: signal the work to stop and defer reaping to Update().
    // We never block the main thread waiting on the future.
    if(!op.cancelling)
    {
        op.cancelling = true;
        if(op.cancel_fn)
        {
            op.cancel_fn();
        }
        // Best-effort cancel of a still-queued job; harmless if already running.
        rocprofvis_controller_future_cancel(op.future);
    }
}

bool
AppMonitor::HasOperation(uint64_t operation_id) const
{
    return m_operations.find(operation_id) != m_operations.end();
}

bool
AppMonitor::HasPendingOperations() const
{
    return !m_operations.empty();
}

void
AppMonitor::Update()
{
    for(auto it = m_operations.begin(); it != m_operations.end();)
    {
        MonitorOperation& op = it->second;

        // A cancelling operation no longer polls status or emits events; we just
        // wait (non-blocking) for its future to resolve so it can be reaped.
        if(op.cancelling)
        {
            if(FutureResolved(op))
            {
                ReapOperation(op);
                it = m_operations.erase(it);
            }
            else
            {
                ++it;
            }
            continue;
        }

        uint64_t status = op.status_fn();

        bool changed = !op.has_status || status != op.last_status;
        if(changed)
        {
            op.last_status = status;
            op.has_status  = true;

            if(op.event_factory)
            {
                if(std::shared_ptr<RocEvent> event = op.event_factory(status))
                {
                    EventManager::GetInstance()->AddEvent(std::move(event));
                }
            }
        }

        bool terminal = op.is_terminal_fn && op.is_terminal_fn(status);
        if(terminal)
        {
            // Terminal status means the bound future has resolved (or is about
            // to). Reap now if resolved; otherwise defer to a later frame via
            // the cancelling path (no blocking).
            if(FutureResolved(op))
            {
                ReapOperation(op);
                it = m_operations.erase(it);
            }
            else
            {
                op.cancelling = true;
                ++it;
            }
        }
        else
        {
            ++it;
        }
    }
}

}  // namespace View
}  // namespace RocProfVis
