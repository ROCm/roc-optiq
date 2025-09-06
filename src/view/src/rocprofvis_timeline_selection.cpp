// Copyright (C) 2025 Advanced Micro Devces, Inc. All rights reserved.

#include "rocprofvis_timeline_selection.h"
#include "rocprofvis_event_manager.h"
#include "rocprofvis_track_item.h"
#include "rocprofvis_view_structs.h"
#include "spdlog/spdlog.h"

namespace RocProfVis
{
namespace View
{


TimelineSelection::TimelineSelection(DataProvider& dp)
: m_selected_range_start(INVALID_SELECTION_TIME)
, m_selected_range_end(INVALID_SELECTION_TIME)
, m_tracks_changed(false)
, m_range_changed(false)
, m_data_provider(dp)
{}

TimelineSelection::~TimelineSelection() {}

void
TimelineSelection::Update()
{
    if(m_tracks_changed || m_range_changed)
    {
        EventManager::GetInstance()->AddEvent(
            std::make_shared<TrackSelectionChangedEvent>(
                std::vector(m_selected_track_ids.begin(), m_selected_track_ids.end()),
                m_selected_range_start, m_selected_range_end,
                m_data_provider.GetTraceFilePath()));
        m_tracks_changed = false;
        m_range_changed  = false;
    }
}

void
TimelineSelection::SelectTrack(rocprofvis_graph_t& graph)
{
    if(!graph.selected && graph.chart && !graph.chart->IsSelected() &&
       m_selected_track_ids.count(graph.chart->GetID()) == 0)
    {
        graph.selected = true;
        graph.chart->SetSelected(true);
        m_selected_track_ids.insert(graph.chart->GetID());
        m_tracks_changed = true;
    }
}

void
TimelineSelection::UnselectTrack(rocprofvis_graph_t& graph)
{
    if(graph.selected && graph.chart && graph.chart->IsSelected() &&
       m_selected_track_ids.count(graph.chart->GetID()) > 0)
    {
        graph.selected = false;
        graph.chart->SetSelected(false);
        m_selected_track_ids.erase(graph.chart->GetID());
        m_tracks_changed = true;
    }
}

void
TimelineSelection::UnselectAllTracks(std::vector<rocprofvis_graph_t>& graphs)
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
    m_tracks_changed = true;
}

void
TimelineSelection::ToggleSelectTrack(rocprofvis_graph_t& graph)
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

void
TimelineSelection::SelectTimeRange(double start_ts, double end_ts)
{
    if(start_ts != m_selected_range_start || end_ts != m_selected_range_end)
    {
        m_selected_range_start = start_ts;
        m_selected_range_end   = end_ts;
        m_range_changed        = true;
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
    m_range_changed        = true;
}

bool
TimelineSelection::HasValidTimeRangeSelection() const
{
    return m_selected_range_start != TimelineSelection::INVALID_SELECTION_TIME ||
           m_selected_range_end != TimelineSelection::INVALID_SELECTION_TIME;
}

void
TimelineSelection::SelectTrackEvent(uint64_t track_id, uint64_t event_id)
{
    if(m_selected_event_ids.count(event_id) == 0)
    {
        m_selected_event_ids.insert(event_id);
        SendEventSelectionChanged(event_id, track_id, true);
    }
}

void
TimelineSelection::UnselectTrackEvent(uint64_t track_id, uint64_t event_id)
{
    if(m_selected_event_ids.count(event_id) > 0)
    {
        m_selected_event_ids.erase(event_id);
        SendEventSelectionChanged(event_id, track_id, false);
    }
}

bool
TimelineSelection::HasSelectedTracks() const
{
    return !m_selected_track_ids.empty();
}

bool
TimelineSelection::GetSelectedEvents(std::vector<uint64_t>& event_ids)
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
    SendEventSelectionChanged(INVALID_SELECTION_ID, INVALID_SELECTION_ID, false, true);
}

void
TimelineSelection::SendEventSelectionChanged(uint64_t event_id, uint64_t track_id,
                                             bool selected, bool all)
{
    EventManager::GetInstance()->AddEvent(std::make_shared<EventSelectionChangedEvent>(
        event_id, track_id, selected, m_data_provider.GetTraceFilePath(), all));
}

bool
TimelineSelection::HasSelectedEvents() const
{
    return !m_selected_event_ids.empty();
}


}  // namespace View
}  // namespace RocProfVis
