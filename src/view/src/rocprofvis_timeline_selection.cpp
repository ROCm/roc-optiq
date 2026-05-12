// Copyright (C) 2025 Advanced Micro Devces, Inc. All rights reserved.

#include "rocprofvis_timeline_selection.h"
#include "rocprofvis_data_provider.h"
#include "rocprofvis_event_manager.h"
#include "rocprofvis_track_item.h"
#include "rocprofvis_utils.h"
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
, m_highlight_timer_active(false)
, m_last_highlighted_event_id(INVALID_SELECTION_ID)
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
        SendTimeRangeChanged(m_selected_range_start, m_selected_range_end);
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
    SendTimeRangeChanged(m_selected_range_start, m_selected_range_end);
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
    SendEventSelectionChanged(INVALID_SELECTION_ID, INVALID_SELECTION_ID, false, true);
}

bool
TimelineSelection::GetSelectedEventsTimeRange(double& start_ts_out, double& end_ts_out) const
{
    std::vector<uint64_t> event_ids(m_selected_event_ids.begin(), m_selected_event_ids.end());
    return m_data_provider.DataModel().GetEvents().GetEventTimeRange(event_ids, start_ts_out,
                                                                     end_ts_out);
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

void
TimelineSelection::SendTimeRangeChanged(double start_ts, double end_ts)
{
    EventManager::GetInstance()->AddEvent(
        std::make_shared<TimeRangeSelectionChangedEvent>(
            start_ts, end_ts, m_data_provider.GetTraceFilePath()));
}

bool
TimelineSelection::HasSelectedEvents() const
{
    return !m_selected_event_ids.empty();
}

void
TimelineSelection::NavigateToEvent(uint64_t track_id, uint64_t event_uuid,
                                   double start_ns, double duration_ns)
{
    ViewRangeNS view_range = calculate_adaptive_view_range(start_ns, duration_ns);

    EventManager::GetInstance()->AddEvent(std::make_shared<ScrollToTrackEvent>(
        static_cast<int>(RocEvents::kHandleUserGraphNavigationEvent), track_id,
        m_data_provider.GetTraceFilePath()));
    EventManager::GetInstance()->AddEvent(std::make_shared<RangeEvent>(
        static_cast<int>(RocEvents::kSetViewRange), view_range.start_ns, view_range.end_ns,
        m_data_provider.GetTraceFilePath()));

    if(event_uuid != INVALID_SELECTION_ID)
    {
        HighlightTrackEvent(track_id, event_uuid);
    }
}

void
TimelineSelection::HighlightTrackEvent(uint64_t track_id, uint64_t event_id)
{
    if(m_highlighted_event_ids.count(event_id) == 0)
    {
        m_highlighted_event_ids.insert(event_id);
        SendEventHighlightChanged(event_id, track_id, true);
    }
    m_last_highlighted_event_id = event_id;
    m_highlight_timer_start     = std::chrono::steady_clock::now();
    m_highlight_timer_active    = true;
}

void
TimelineSelection::UnhighlightTrackEvent(uint64_t track_id, uint64_t event_id)
{
    if(m_highlighted_event_ids.count(event_id) > 0)
    {
        m_highlighted_event_ids.erase(event_id);
        SendEventHighlightChanged(event_id, track_id, false);
    }
}

bool
TimelineSelection::EventHighlighted(uint64_t event_id) const
{
    return m_highlighted_event_ids.count(event_id) > 0;
}

void
TimelineSelection::UnhighlightAllEvents()
{
    m_highlighted_event_ids.clear();
    m_highlight_timer_active        = false;
    m_last_highlighted_event_id     = INVALID_SELECTION_ID;
    SendEventHighlightChanged(INVALID_SELECTION_ID, INVALID_SELECTION_ID, false, true);
}

bool
TimelineSelection::HasHighlightedEvents() const
{
    return !m_highlighted_event_ids.empty();
}

uint64_t
TimelineSelection::GetLastHighlightedEventId() const
{
    return m_last_highlighted_event_id;
}

double
TimelineSelection::GetHighlightElapsedSeconds() const
{
    if(!m_highlight_timer_active)
        return 0.0;
    auto elapsed = std::chrono::steady_clock::now() - m_highlight_timer_start;
    return std::chrono::duration<double>(elapsed).count();
}

void
TimelineSelection::UpdateHighlightTimer()
{
    if(m_highlight_timer_active)
    {
        auto elapsed = std::chrono::steady_clock::now() - m_highlight_timer_start;
        double elapsed_s =
            std::chrono::duration<double>(elapsed).count();
        if(elapsed_s >= HIGHLIGHT_TIMEOUT_S)
        {
            UnhighlightAllEvents();
        }
    }
}

void
TimelineSelection::SendEventHighlightChanged(uint64_t event_id, uint64_t track_id,
                                              bool highlighted, bool all)
{
    EventManager::GetInstance()->AddEvent(std::make_shared<EventHighlightChangedEvent>(
        event_id, track_id, highlighted, m_data_provider.GetTraceFilePath(), all));
}

}  // namespace View
}  // namespace RocProfVis
