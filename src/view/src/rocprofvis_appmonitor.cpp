// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_appmonitor.h"
#include "rocprofvis_event_manager.h"
#include "rocprofvis_controller.h"

namespace RocProfVis
{
namespace View
{

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

AppMonitor::~AppMonitor() {}

uint64_t
AppMonitor::AddOperation(MonitorOperationType            type,
                         MonitorOperation::StatusFn      status_fn,
                         MonitorOperation::EventFactory  event_factory,
                         MonitorOperation::IsTerminalFn  is_terminal_fn,
                         MonitorOperation::CancelFn      cancel_fn,
                         rocprofvis_controller_future_t* future)
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

    m_operations.emplace(id, std::move(op));
    return id;
}

bool
AppMonitor::TryReleaseFuture(MonitorOperation& op)
{
    if(op.future == nullptr)
    {
        return true;
    }

    // Non-blocking probe. rocprofvis_controller_future_free does not join the
    // worker, so we must not free until the future has actually resolved.
    if(rocprofvis_controller_future_wait(op.future, 0) == kRocProfVisResultTimeout)
    {
        return false;
    }

    rocprofvis_controller_future_free(op.future);
    op.future = nullptr;
    return true;
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
    if(TryReleaseFuture(op))
    {
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

void
AppMonitor::Update()
{
    for(auto it = m_operations.begin(); it != m_operations.end();)
    {
        MonitorOperation& op = it->second;

        // A cancelling operation no longer polls status or emits events; we just
        // wait (non-blocking) for its future to resolve so it can be freed.
        if(op.cancelling)
        {
            if(TryReleaseFuture(op))
            {
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
            // to). Try to free it now; if it has not quite resolved yet, defer
            // reaping to a later frame via the cancelling path (no blocking).
            if(TryReleaseFuture(op))
            {
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
