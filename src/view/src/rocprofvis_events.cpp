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

TrackDataEvent::TrackDataEvent(int event_id, uint64_t track_index)
: RocEvent(event_id)
, m_track_index(track_index)
{
    m_event_type = RocEventType::kTrackDataEvent;
}

uint64_t
TrackDataEvent::GetTrackIndex()
{
    return m_track_index;
}
