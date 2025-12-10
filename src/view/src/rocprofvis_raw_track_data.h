// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_controller_enums.h"
#include "rocprofvis_controller_types.h"

#include <chrono>
#include <map>
#include <string>
#include <type_traits>
#include <unordered_set>
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
    uint32_t    m_child_count;
    std::string m_top_combined_name;
} rocprofvis_trace_event_t;

typedef struct rocprofvis_trace_counter_t
{
    double m_start_ts;
    double m_value;
    double m_end_ts;
} rocprofvis_trace_counter_t;

typedef union
{
    struct
    {
        // Event ID from database
        uint64_t db_event_id : 60;
        // Event operation type
        uint64_t event_op : 4;
    } bitfield;
    uint64_t id;
} rocprofvis_trace_event_t_id_t;

class RawTrackData
{
public:
    RawTrackData(rocprofvis_controller_track_type_t track_type, uint64_t track_id,
                 double start_ts, double end_ts, uint64_t data_group_id, size_t chunk_count);
    virtual ~RawTrackData() = default;
    rocprofvis_controller_track_type_t GetType() const;
    double                             GetStartTs() const;
    double                             GetEndTs() const;
    uint64_t                           GetTrackID() const;
    uint64_t                           GetDataGroupID() const;
    size_t                             GetChunkCount() const;
    bool                               AllDataReady() const;

    void SetDataRequestTimePoint(
        const std::chrono::steady_clock::time_point& request_time);

    const std::chrono::steady_clock::time_point& GetDataRequestTimePoint() const;

protected:
    rocprofvis_controller_track_type_t m_track_type;

    double   m_start_ts;       // starting time stamp of track data
    double   m_end_ts;         // ending time stamp of track data
    uint64_t m_track_id;       // id of the track
    uint64_t m_data_group_id;  // group id for the data, used for grouping requests
    uint64_t m_expected_chunk_count; // expected number of chunks
    std::map<size_t, size_t> m_chunk_info; //map containing chunk sizes

    std::chrono::steady_clock::time_point m_request_time;
};

// Trait to get data properties from the data type
template <typename T>
struct data_traits;

template <>
struct data_traits<rocprofvis_trace_event_t>
{
    using id_type = uint64_t;
    static constexpr rocprofvis_controller_track_type_t track_type = kRPVControllerTrackTypeEvents;
};

template <>
struct data_traits<rocprofvis_trace_counter_t>
{
    using id_type = double;
    static constexpr rocprofvis_controller_track_type_t track_type = kRPVControllerTrackTypeSamples;
};

template <typename T>
class TemplatedRawTrackData : public RawTrackData
{
public:
    using id_type = typename data_traits<T>::id_type;

    TemplatedRawTrackData(uint64_t track_id, double start_ts, double end_ts,
                          uint64_t data_group_id, size_t chunk_count);
    virtual ~TemplatedRawTrackData();

    const std::vector<T>& GetData() const;
    void SetData(std::vector<T>&& data);

    std::unordered_set<id_type>& GetWritableIdSet();

    bool AddChunk(size_t chunk_index, const std::vector<T> &&chunk_data);

private:
    std::vector<T> m_data;
    std::unordered_set<id_type> m_ids;
};

using RawTrackSampleData = TemplatedRawTrackData<rocprofvis_trace_counter_t>;
using RawTrackEventData = TemplatedRawTrackData<rocprofvis_trace_event_t>;

}  // namespace View
}  // namespace RocProfVis
