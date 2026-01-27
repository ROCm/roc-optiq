// Copyright (C) 2025 Advanced Micro Devces, Inc. All rights reserved.

#include "rocprofvis_timeline_selection.h"
#include "rocprofvis_event_manager.h"
#include "rocprofvis_track_item.h"
#include "spdlog/spdlog.h"
#include <algorithm>

namespace RocProfVis
{
namespace View
{

TimelineSelection::TimelineSelection(DataProvider& dp)
: m_selected_range_start(INVALID_SELECTION_TIME)
, m_selected_range_end(INVALID_SELECTION_TIME)
, m_data_provider(dp)
{}

TimelineSelection::~TimelineSelection() {}

void
TimelineSelection::SelectTrack(TrackGraph& graph)
{
    if(!graph.selected && graph.chart && !graph.chart->IsSelected() &&
       m_selected_track_ids.count(graph.chart->GetID()) == 0)
    {
        graph.selected = true;
        graph.chart->SetSelected(true);
        m_selected_track_ids.insert(graph.chart->GetID());
        SendTrackSelectionChanged(graph.chart->GetID(), true);
    }
}

void
TimelineSelection::UnselectTrack(TrackGraph& graph)
{
    if(graph.selected && graph.chart && graph.chart->IsSelected() &&
       m_selected_track_ids.count(graph.chart->GetID()) > 0)
    {
        graph.selected = false;
        graph.chart->SetSelected(false);
        m_selected_track_ids.erase(graph.chart->GetID());
        SendTrackSelectionChanged(graph.chart->GetID(), false);
    }
}

void
TimelineSelection::UnselectAllTracks(std::vector<TrackGraph>& graphs)
{
    for(auto& graph : graphs)
    {
        graph.selected = false;
        if(graph.chart)
        {
            graph.chart->SetSelected(false);
        }
    }
    m_selected_track_ids.clear();
    SendTrackSelectionChanged(INVALID_SELECTION_ID, false);
}

void
TimelineSelection::ToggleSelectTrack(TrackGraph& graph)
{
    if(graph.selected)
    {
        UnselectTrack(graph);
    }
    else
    {
        SelectTrack(graph);
    }
}

bool
TimelineSelection::GetSelectedTracks(std::vector<uint64_t>& track_ids) const
{
    if(m_selected_track_ids.empty())
    {
        return false;
    }

    track_ids.insert(track_ids.end(), m_selected_track_ids.begin(),
                     m_selected_track_ids.end());
    return true;
}

bool
TimelineSelection::HasSelectedTracks() const
{
    return !m_selected_track_ids.empty();
}

void
TimelineSelection::SelectTimeRange(double start_ts, double end_ts)
{
    if(start_ts != m_selected_range_start || end_ts != m_selected_range_end)
    {
        m_selected_range_start = start_ts;
        m_selected_range_end   = end_ts;
        SendTrackSelectionChanged(INVALID_SELECTION_ID, false);
    }
}

bool
TimelineSelection::GetSelectedTimeRange(double& start_ts_out, double& end_ts_out) const
{
    if(!HasValidTimeRangeSelection())
    {
        return false;  // No valid selection
    }

    start_ts_out = m_selected_range_start;
    end_ts_out   = m_selected_range_end;
    return true;
}

void
TimelineSelection::ClearTimeRange()
{
    m_selected_range_start = TimelineSelection::INVALID_SELECTION_TIME;
    m_selected_range_end   = TimelineSelection::INVALID_SELECTION_TIME;
    SendTrackSelectionChanged(INVALID_SELECTION_ID, false);
}

bool
TimelineSelection::HasValidTimeRangeSelection() const
{
    return m_selected_range_start != TimelineSelection::INVALID_SELECTION_TIME ||
           m_selected_range_end != TimelineSelection::INVALID_SELECTION_TIME;
}

void
TimelineSelection::SelectTrackEvent(uint64_t track_id, uint64_t event_id, double start_ts, double end_ts)
{
    if(m_selected_event_ids.count(event_id) == 0)
    {
        m_selected_event_ids.insert(event_id);
        m_selected_event_times[event_id] = {start_ts, end_ts};
        SendEventSelectionChanged(event_id, track_id, true);
    }
}

void
TimelineSelection::UnselectTrackEvent(uint64_t track_id, uint64_t event_id)
{
    if(m_selected_event_ids.count(event_id) > 0)
    {
        m_selected_event_ids.erase(event_id);
        m_selected_event_times.erase(event_id);
        SendEventSelectionChanged(event_id, track_id, false);
    }
}

bool
TimelineSelection::GetSelectedEvents(std::vector<uint64_t>& event_ids) const
{
    if(m_selected_event_ids.empty())
    {
        return false;
    }

    event_ids.insert(event_ids.end(), m_selected_event_ids.begin(),
                     m_selected_event_ids.end());
    return true;
}

bool
TimelineSelection::EventSelected(uint64_t event_id) const
{
    return m_selected_event_ids.count(event_id) > 0;
}

void
TimelineSelection::UnselectAllEvents()
{
    m_selected_event_ids.clear();
    m_selected_event_times.clear();
    SendEventSelectionChanged(INVALID_SELECTION_ID, INVALID_SELECTION_ID, false, true);
}

bool
TimelineSelection::GetSelectedEventsTimeRange(double& start_ts_out, double& end_ts_out) const
{
    if(m_selected_event_times.empty())
    {
        return false;
    }

    start_ts_out = std::numeric_limits<double>::max();
    end_ts_out   = std::numeric_limits<double>::lowest();

    for(const auto& [event_id, times] : m_selected_event_times)
    {
        start_ts_out = std::min(start_ts_out, times.first);
        end_ts_out   = std::max(end_ts_out, times.second);
    }
    return true;
}

void
TimelineSelection::SendEventSelectionChanged(uint64_t event_id, uint64_t track_id,
                                             bool selected, bool all)
{
    EventManager::GetInstance()->AddEvent(std::make_shared<EventSelectionChangedEvent>(
        event_id, track_id, selected, m_data_provider.GetTraceFilePath(), all));
}

void
TimelineSelection::SendTrackSelectionChanged(uint64_t track_id, bool selected)
{
    EventManager::GetInstance()->AddEvent(std::make_shared<TrackSelectionChangedEvent>(
        track_id, selected, m_data_provider.GetTraceFilePath()));
}

bool
TimelineSelection::HasSelectedEvents() const
{
    return !m_selected_event_ids.empty();
}

}  // namespace View
}  // namespace RocProfVis
