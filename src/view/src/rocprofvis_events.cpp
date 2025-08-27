// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_events.h"
#include "spdlog/spdlog.h"

using namespace RocProfVis::View;

// RocEvent Implementation

RocEvent::RocEvent()
: m_event_id(-1)
, m_event_type(RocEventType::kRocEvent)
, m_allow_propagate(true)
{}

RocEvent::RocEvent(int event_id)
: m_event_id(event_id)
, m_event_type(RocEventType::kRocEvent)
, m_allow_propagate(true)
{}

RocEvent::~RocEvent() {}

int
RocEvent::GetId()
{
    return m_event_id;
}

void
RocEvent::SetId(int id)
{
    m_event_id = id;
}

RocEventType
RocEvent::GetType()
{
    return m_event_type;
}

void
RocEvent::SetType(RocEventType type)
{
    m_event_type = type;
}

void
RocEvent::SetAllowPropagate(bool allow)
{
    m_allow_propagate = allow;
}

void
RocEvent::StopPropagation()
{
    m_allow_propagate = false;
}

bool
RocEvent::CanPropagate() const
{
    return m_allow_propagate;
}

// TrackDataEvent Implementation
TrackDataEvent::TrackDataEvent(int event_id, uint64_t track_id,
                               const std::string& trace_path, uint64_t request_id,
                               uint64_t response_code)
: RocEvent(event_id)
, m_track_id(track_id)
, m_trace_path(trace_path)
, m_request_id(request_id)
, m_response_code(response_code)
{
    m_event_type = RocEventType::kTrackDataEvent;
}

uint64_t
TrackDataEvent::GetTrackID() const
{
    return m_track_id;
}

const std::string&
TrackDataEvent::GetTracePath() const
{
    return m_trace_path;
}

uint64_t
TrackDataEvent::GetRequestID() const
{
    return m_request_id;
}

uint64_t
TrackDataEvent::GetResponseCode() const
{
    return m_response_code;
}

#ifdef COMPUTE_UI_SUPPORT
ComputeTableSearchEvent::ComputeTableSearchEvent(int event_id, std::string& term)
: RocEvent(event_id)
, m_search_term(term)
{
    m_event_type = RocEventType::kComputeTableSearchEvent;
}

const std::string
RocProfVis::View::ComputeTableSearchEvent::GetSearchTerm()
{
    return m_search_term;
}
#endif

TabEvent::TabEvent(int event_id, const std::string& tab_id, const std::string& tab_source)
: RocEvent(event_id)
, m_tab_id(tab_id)
, m_tab_source(tab_source)
{
    m_event_type = RocEventType::kTabEvent;
}

const std::string&
TabEvent::GetTabId() const
{
    return m_tab_id;
}

const std::string&
TabEvent::GetTabSource() const
{
    return m_tab_source;
}

TrackSelectionChangedEvent::TrackSelectionChangedEvent(
    std::vector<uint64_t> selected_tracks, double start_ns, double end_ns,
    const std::string& trace_path)
: RocEvent(static_cast<int>(RocEvents::kTimelineTrackSelectionChanged))
, m_selected_tracks(std::move(selected_tracks))
, m_start_ns(start_ns)
, m_end_ns(end_ns)
, m_trace_path(trace_path)
{
    m_event_type = RocEventType::kTimelineTrackSelectionChangedEvent;
}

const std::vector<uint64_t>&
TrackSelectionChangedEvent::GetSelectedTracks() const
{
    return m_selected_tracks;
}

double
TrackSelectionChangedEvent::GetStartNs() const
{
    return m_start_ns;
}

double
TrackSelectionChangedEvent::GetEndNs() const
{
    return m_end_ns;
}

const std::string&
TrackSelectionChangedEvent::GetTracePath()
{
    return m_trace_path;
}

ScrollToTrackEvent::ScrollToTrackEvent(int event_id, const uint64_t& track_id)
: RocEvent(event_id)
, m_track_id(track_id)
{
    m_event_type = RocEventType::kScrollToTrackEvent;
}

const uint64_t
ScrollToTrackEvent::GetTrackID() const
{
    return m_track_id;
}

EventSelectionChangedEvent::EventSelectionChangedEvent(uint64_t event_id,
                                                       uint64_t track_id, bool selected,
                                                       const std::string& trace_path)
: RocEvent(static_cast<int>(RocEvents::kTimelineEventSelectionChanged))
, m_event_id(event_id)
, m_event_track_id(track_id)
, m_selected(selected)
, m_trace_path(trace_path)
{
    m_event_type = RocEventType::kTimelineEventSelectionChangedEvent;
}

uint64_t
EventSelectionChangedEvent::GetEventID() const
{
    return m_event_id;
}

uint64_t
EventSelectionChangedEvent::GetEventTrackID() const
{
    return m_event_track_id;
}

bool
EventSelectionChangedEvent::EventSelected() const
{
    return m_selected;
}

const std::string&
EventSelectionChangedEvent::GetTracePath()
{
    return m_trace_path;
}
const int
StickyNoteEvent::GetID()
{
    return m_id;
}