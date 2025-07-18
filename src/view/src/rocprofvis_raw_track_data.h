// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include "rocprofvis_controller_enums.h"
#include "rocprofvis_controller_types.h"

#include <unordered_set>
#include <string>
#include <vector>

namespace RocProfVis
{
namespace View
{

typedef struct rocprofvis_trace_event_t
{
    uint64_t    m_id;
    std::string m_name;
    double      m_start_ts;
    double      m_duration;
    uint32_t    m_level;
} rocprofvis_trace_event_t;

typedef struct rocprofvis_trace_counter_t
{
    double m_start_ts;
    double m_value;
} rocprofvis_trace_counter_t;

class RawTrackData
{
public:
    RawTrackData(rocprofvis_controller_track_type_t track_type, uint64_t track_id,
                 double start_ts, double end_ts, uint64_t data_group_id);
    virtual ~RawTrackData() = 0;
    rocprofvis_controller_track_type_t GetType() const;
    double                             GetStartTs() const;
    double                             GetEndTs() const;
    uint64_t                           GetTrackID() const;
    uint64_t                           GetDataGroupID() const;

protected:
    rocprofvis_controller_track_type_t m_track_type;

    double   m_start_ts;       // starting time stamp of track data
    double   m_end_ts;         // ending time stamp of track data
    uint64_t m_track_id;       // id of the track
    uint64_t m_data_group_id;  // group id for the data, used for grouping requests
};

class RawTrackSampleData : public RawTrackData
{
public:
    RawTrackSampleData(uint64_t track_id, double start_ts, double end_ts,
                       uint64_t data_group_id);
    virtual ~RawTrackSampleData();
    const std::vector<rocprofvis_trace_counter_t>& GetData() const;
    void SetData(std::vector<rocprofvis_trace_counter_t>&& data);

    std::vector<rocprofvis_trace_counter_t>& GetWritableData();
    std::unordered_set<double>& GetWritableTimepoints();
    
private:
    std::vector<rocprofvis_trace_counter_t> m_data;
    // Use start timestamps as unique identifiers for samples
    std::unordered_set<double> m_timepoints;

};

class RawTrackEventData : public RawTrackData
{
public:
    RawTrackEventData(uint64_t track_id, double start_ts, double end_ts,
                      uint64_t data_group_id);
    virtual ~RawTrackEventData();
    const std::vector<rocprofvis_trace_event_t>& GetData() const;
    void SetData(std::vector<rocprofvis_trace_event_t>&& data);

    std::vector<rocprofvis_trace_event_t>& GetWritableData();
    std::unordered_set<uint64_t>&          GetWritableEventSet();

private:
    std::vector<rocprofvis_trace_event_t> m_data;
    // Store unique event IDs to avoid duplicates
    std::unordered_set<uint64_t> m_event_ids;
};

}  // namespace View
}  // namespace RocProfVis
