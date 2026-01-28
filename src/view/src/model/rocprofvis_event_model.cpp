// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_event_model.h"
#include "spdlog/spdlog.h"
#include <algorithm>
#include <limits>

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

bool
EventModel::GetEventTimeRange(const std::vector<uint64_t>& event_ids, double& start_ts_out,
                              double& end_ts_out) const
{
    if(event_ids.empty())
    {
        return false;
    }

    start_ts_out = std::numeric_limits<double>::max();
    end_ts_out   = std::numeric_limits<double>::lowest();

    bool found = false;
    for(const auto& event_id : event_ids)
    {
        const auto* event = GetEvent(event_id);
        if(event)
        {
            start_ts_out = std::min(start_ts_out, event->basic_info.start_ts);
            end_ts_out =
                std::max(end_ts_out, event->basic_info.start_ts + event->basic_info.duration);
            found = true;
        }
    }
    return found;
}

}  // namespace View
}  // namespace RocProfVis
