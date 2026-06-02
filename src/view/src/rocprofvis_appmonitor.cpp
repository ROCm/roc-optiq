// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_appmonitor.h"
#include "rocprofvis_event_manager.h"

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
                         MonitorOperation::IsTerminalFn  is_terminal_fn)
{
    ROCPROFVIS_ASSERT(status_fn != nullptr);

    uint64_t id = m_next_operation_id++;

    MonitorOperation op;
    op.operation_id   = id;
    op.operation_type = type;
    op.status_fn      = std::move(status_fn);
    op.event_factory  = std::move(event_factory);
    op.is_terminal_fn = std::move(is_terminal_fn);
    op.last_status    = 0;
    op.has_status     = false;

    m_operations.emplace(id, std::move(op));
    return id;
}

void
AppMonitor::RemoveOperation(uint64_t operation_id)
{
    m_operations.erase(operation_id);
}

void
AppMonitor::Update()
{
    for(auto it = m_operations.begin(); it != m_operations.end();)
    {
        MonitorOperation& op = it->second;

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
            it = m_operations.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

}  // namespace View
}  // namespace RocProfVis
