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

RocEvent::~RocEvent()
{
    spdlog::debug("RocEvent destructor called for event id: {}", m_event_id);
}

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
TrackDataEvent::TrackDataEvent(int event_id, uint64_t track_index,
                               const std::string& trace_path)
: RocEvent(event_id)
, m_track_index(track_index)
, m_trace_path(trace_path)
{
    m_event_type = RocEventType::kTrackDataEvent;
}

uint64_t
TrackDataEvent::GetTrackIndex()
{
    return m_track_index;
}

const std::string&
TrackDataEvent::GetTracePath()
{
    return m_trace_path;
}

ComputeBlockNavigationEvent::ComputeBlockNavigationEvent(int event_id, int level, int block)
: RocEvent(event_id)
, m_level(level)
, m_block(block)
{
    m_event_type = RocEventType::kComputeBlockNavigationEvent;
}

const int
ComputeBlockNavigationEvent::GetLevel()
{
    return m_level;
}

const int
ComputeBlockNavigationEvent::GetBlock()
{
    return m_block;
}

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

TabClosedEvent::TabClosedEvent(int event_id, const std::string& tab_id)
: RocEvent(event_id)
, m_tab_id(tab_id)
{
    m_event_type = RocEventType::kTabEvent;
}

const std::string&
TabClosedEvent::GetTabId() {
    return m_tab_id;
}
