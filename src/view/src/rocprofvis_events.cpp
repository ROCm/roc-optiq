// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

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

RocEvent::RocEvent(int event_id, const std::string& src_id)
: m_event_id(event_id)
, m_event_type(RocEventType::kRocEvent)
, m_allow_propagate(true)
, m_source_id(src_id)
{}

RocEvent::~RocEvent() {}

int
RocEvent::GetId() const
{
    return m_event_id;
}

void
RocEvent::SetId(int id)
{
    m_event_id = id;
}

RocEventType
RocEvent::GetType() const
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

void
RocEvent::SetSourceId(const std::string& source_id)
{
    m_source_id = source_id;
}

const std::string
RocEvent::GetSourceId() const
{
    return m_source_id;
}

// TrackDataEvent Implementation

TrackDataEvent::TrackDataEvent(int event_id, uint64_t track_id,
                               const std::string& source_id, uint64_t request_id,
                               uint64_t response_code)
: RocEvent(event_id, source_id)
, m_track_id(track_id)
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

TableDataEvent::TableDataEvent(const std::string& source_id, uint64_t request_id)
: RocEvent(static_cast<int>(RocEvents::kNewTableData), source_id)
, m_request_id(request_id)
{
    m_event_type = RocEventType::kTableDataEvent;
}

uint64_t
TableDataEvent::GetRequestID() const
{
    return m_request_id;
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

TabEvent::TabEvent(int event_id, const std::string& tab_id, const std::string& source_id)
: RocEvent(event_id, source_id)
, m_tab_id(tab_id)
{
    m_event_type = RocEventType::kTabEvent;
}

const std::string&
TabEvent::GetTabId() const
{
    return m_tab_id;
}

TrackSelectionChangedEvent::TrackSelectionChangedEvent(uint64_t track_id, bool selected,
                                                       const std::string& source_id)
: RocEvent(static_cast<int>(RocEvents::kTimelineTrackSelectionChanged), source_id)
, m_track_id(track_id)
, m_selected(selected)
{
    m_event_type = RocEventType::kTimelineTrackSelectionChangedEvent;
}

uint64_t
TrackSelectionChangedEvent::GetTrackID() const
{
    return m_track_id;
}

bool
TrackSelectionChangedEvent::TrackSelected() const
{
    return m_selected;
}


ScrollToTrackEvent::ScrollToTrackEvent(int event_id, const uint64_t& track_id,
                                       const std::string& source_id)
: RocEvent(event_id, source_id)
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
                                                       const std::string& source_id,
                                                       bool               batch)
: RocEvent(static_cast<int>(RocEvents::kTimelineEventSelectionChanged), source_id)
, m_event_id(event_id)
, m_event_track_id(track_id)
, m_selected(selected)
, m_is_batch(batch)
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

bool
EventSelectionChangedEvent::IsBatch() const
{
    return m_is_batch;
}

StickyNoteEvent::StickyNoteEvent(int id, const std::string& source_id)
: RocEvent(static_cast<int>(RocEvents::kStickyNoteEdited), source_id)
, m_id(id)
{
    SetType(RocEventType::kStickyNoteEvent);
}

const int
StickyNoteEvent::GetNoteId() const
{
    return m_id;
}

RangeEvent::RangeEvent(int event_id, double start_ns, double end_ns,
                       const std::string& source_id)
: RocEvent(event_id, source_id)
, m_start_ns(start_ns)
, m_end_ns(end_ns)
{
    m_event_type = RocEventType::kRangeEvent;
}

double
RangeEvent::GetStartNs() const
{
    return m_start_ns;
}

double
RangeEvent::GetEndNs() const
{
    return m_end_ns;
}
NavigationEvent::NavigationEvent(double v_min, double v_max, double y_position, bool center )
: RocEvent(static_cast<int>(RocEvents::kGoToTimelineSpot))
, m_v_min(v_min)
, m_v_max(v_max)
, m_y_position(y_position)
, m_center(center)
{
    SetType(RocEventType::kNavigationEvent);
}

double
NavigationEvent::GetVMin() const
{
    return m_v_min;
}

double
NavigationEvent::GetVMax() const
{
    return m_v_max;
}

double
NavigationEvent::GetYPosition() const
{
    return m_y_position;
}
bool
NavigationEvent::GetCenter() const
{
    return m_center;
}