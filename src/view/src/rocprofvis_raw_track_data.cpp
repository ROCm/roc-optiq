// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_raw_track_data.h"

using namespace RocProfVis::View;

RawTrackData::RawTrackData(rocprofvis_controller_track_type_t track_type,
                           uint64_t track_id, double start_ts, double end_ts,
                           uint64_t data_group_id)
: m_track_type(track_type)
, m_track_id(track_id)
, m_start_ts(start_ts)
, m_end_ts(end_ts)
, m_data_group_id(data_group_id)
{}

RawTrackData::~RawTrackData() {}

rocprofvis_controller_track_type_t
RawTrackData::GetType() const
{
    return m_track_type;
}

double
RawTrackData::GetStartTs() const
{
    return m_start_ts;
}
double
RawTrackData::GetEndTs() const
{
    return m_end_ts;
}
uint64_t
RawTrackData::GetTrackID() const
{
    return m_track_id;
}
uint64_t
RawTrackData::GetDataGroupID() const
{
    return m_data_group_id;
}

RawTrackSampleData::RawTrackSampleData(uint64_t track_id, double start_ts, double end_ts,
                                       uint64_t data_group_id)
: RawTrackData(kRPVControllerTrackTypeSamples, track_id, start_ts, end_ts, data_group_id)
{}

RawTrackSampleData::~RawTrackSampleData() { m_data.clear(); }

const std::vector<rocprofvis_trace_counter_t>&
RawTrackSampleData::GetData() const
{
    return m_data;
}

void
RawTrackSampleData::SetData(std::vector<rocprofvis_trace_counter_t>&& data)
{
    m_data = std::move(data);
}

std::vector<rocprofvis_trace_counter_t>&
RawTrackSampleData::GetWritableData()
{
    return m_data;
}

std::unordered_set<double>&
RawTrackSampleData::GetWritableTimepoints()
{
    return m_timepoints;
}

RawTrackEventData::RawTrackEventData(uint64_t track_id, double start_ts, double end_ts,
                                     uint64_t data_group_id)
: RawTrackData(kRPVControllerTrackTypeEvents, track_id, start_ts, end_ts, data_group_id)
{}

RawTrackEventData::~RawTrackEventData() { m_data.clear(); }

const std::vector<rocprofvis_trace_event_t>&
RawTrackEventData::GetData() const
{
    return m_data;
}

void
RawTrackEventData::SetData(std::vector<rocprofvis_trace_event_t>&& data)
{
    m_data = std::move(data);
}

std::vector<rocprofvis_trace_event_t>&
RawTrackEventData::GetWritableData()
{
    return m_data;
}

std::unordered_set<uint64_t>&
RawTrackEventData::GetWritableEventSet()
{
    return m_event_ids;
}
