// Copyright (C) 2025 Advanced Micro Devces, Inc. All rights reserved.

#include "rocprofvis_timeline_selection.h"
#include "rocprofvis_event_manager.h"
#include "rocprofvis_track_item.h"
#include "rocprofvis_view_structs.h"

namespace RocProfVis
{
namespace View
{

TimelineSelection::TimelineSelection()
: m_selected_range_start(-1)
, m_selected_range_end(-1)
, m_tracks_changed(false)
, m_range_changed(false)
{}

TimelineSelection::~TimelineSelection() {}

void
TimelineSelection::Init(double min_ts, double max_ts)
{
    m_selected_range_start = min_ts;
    m_selected_range_end = max_ts;
}

void
TimelineSelection::Update()
{
    if(m_tracks_changed || m_range_changed)
    {
        EventManager::GetInstance()->AddEvent(
            std::make_shared<TrackSelectionChangedEvent>(
                static_cast<int>(RocEvents::kTimelineSelectionChanged),
                std::vector(m_selected_track_ids.begin(), m_selected_track_ids.end()),
                m_selected_range_start, m_selected_range_end));
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

}  // namespace View
}  // namespace RocProfVis