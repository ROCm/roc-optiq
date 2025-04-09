#pragma once

#include "rocprofvis_controller_enums.h"
#include "rocprofvis_controller_types.h"
#include "rocprofvis_structs.h"

#include <vector>

namespace RocProfVis
{
namespace View
{

class RawTrackData
{
public:
    RawTrackData(rocprofvis_controller_track_type_t track_type, uint64_t index,
                 double start_ts, double end_ts);
    virtual ~RawTrackData() = 0;
    rocprofvis_controller_track_type_t GetType();
    double                             GetStartTs();
    double                             GetEndTs();
    uint64_t                           GetIndex();

protected:
    rocprofvis_controller_track_type_t m_track_type;

    double m_start_ts;  // starting time stamp of track data
    double m_end_ts;    // ending time stamp of track data

    uint64_t m_index;  // index of the track
};

class RawTrackSampleData : public RawTrackData
{
public:
    RawTrackSampleData(uint64_t index, double start_ts, double end_ts);
    virtual ~RawTrackSampleData();
    const std::vector<rocprofvis_trace_counter_t>& GetData();
    void SetData(std::vector<rocprofvis_trace_counter_t>&& data);

private:
    std::vector<rocprofvis_trace_counter_t> m_data;
};

class RawTrackEventData : public RawTrackData
{
public:
    RawTrackEventData(uint64_t index, double start_ts, double end_ts);
    virtual ~RawTrackEventData();
    const std::vector<rocprofvis_trace_event_t>& GetData();
    void SetData(std::vector<rocprofvis_trace_event_t>&& data);

private:
    std::vector<rocprofvis_trace_event_t> m_data;
};

}  // namespace View
}  // namespace RocProfVis
