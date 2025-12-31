// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_event_model.h"
#include "spdlog/spdlog.h"

namespace RocProfVis
{
namespace View
{

const EventInfo*
EventModel::GetEvent(uint64_t event_id) const
{
    auto it = m_event_data.find(event_id);
    return (it != m_event_data.end()) ? &it->second : nullptr;
}

EventInfo*
EventModel::GetEvent(uint64_t event_id)
{
    auto it = m_event_data.find(event_id);
    return (it != m_event_data.end()) ? &it->second : nullptr;
}

bool
EventModel::AddEvent(uint64_t event_id, EventInfo&& event)
{
    if(m_event_data.find(event_id) != m_event_data.end())
    {
        spdlog::debug("Event already exists: {}", event_id);
        return false;
    }
    m_event_data[event_id] = std::move(event);
    return true;
}

bool
EventModel::RemoveEvent(uint64_t event_id)
{
    if(m_event_data.erase(event_id) == 0)
    {
        spdlog::debug("Cannot delete event, invalid id: {}", event_id);
        return false;
    }
    return true;
}

void
EventModel::ClearEvents()
{
    m_event_data.clear();
}

size_t
EventModel::EventCount() const
{
    return m_event_data.size();
}

}  // namespace View
}  // namespace RocProfVis
