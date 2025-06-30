// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include "rocprofvis_controller.h"
#include "rocprofvis_controller_data.h"
#include "rocprofvis_controller_handle.h"
#include <bitset>
#include <map>
#include <vector>
#include <memory>
#include <unordered_set>

namespace RocProfVis
{
namespace Controller
{

class Array;
class Event;
class Sample;
class Trace;

constexpr double kSegmentDuration = 1000000000.0;
constexpr double kScalableSegmentDuration   = 10000.0;
constexpr uint32_t kSegmentBitSetSize = 64;
struct SegmentLRUParams
{
    Trace* m_ctx;
    Handle* m_owner;
    uint32_t m_lod;
};

struct SegmentItemKey
{
    double  m_timestamp;
    uint8_t m_level;

    SegmentItemKey(double timestamp, uint8_t level)
    : m_timestamp(timestamp)
    , m_level(level) {};

    bool operator<(const SegmentItemKey& other) const
    {
        if(m_timestamp != other.m_timestamp) return m_timestamp < other.m_timestamp;
        return m_level < other.m_level;
    }
};

class Segment
{
public:
    Segment() = delete;

    Segment(rocprofvis_controller_track_type_t type, Trace* ctx);

    ~Segment();

    double GetStartTimestamp();
    double GetEndTimestamp();
    double GetMinTimestamp();
    double GetMaxTimestamp();

    void SetStartEndTimestamps(double start, double end);

    void SetMinTimestamp(double value);

    void SetMaxTimestamp(double value);

    void Insert(double timestamp, uint8_t level, Handle* event);

    rocprofvis_result_t Fetch(double start, double end, std::vector<Data>& array, uint64_t& index, std::unordered_set<uint64_t>* event_id_set, SegmentLRUParams* lru_params);

    rocprofvis_result_t GetMemoryUsage(uint64_t* value, rocprofvis_common_property_t property);

    size_t              GetNumEntries();
    void                Clear();

private:
    Trace* m_ctx;
    std::map<uint8_t, std::map<double, Handle*>>  m_entries;
    double m_start_timestamp;
    double m_end_timestamp;
    double m_min_timestamp;
    double m_max_timestamp;
    uint64_t m_memory_usage;
    rocprofvis_controller_track_type_t m_type;
};

typedef rocprofvis_result_t (*FetchSegmentsFunc)(double start, double end, Segment& segment, void* user_ptr); 

class SegmentTimeline
{
    SegmentTimeline(SegmentTimeline const& other) = delete;
    SegmentTimeline& operator=(SegmentTimeline const& other) = delete;

public:
    SegmentTimeline();
    ~SegmentTimeline();
    SegmentTimeline(SegmentTimeline&& other);
    SegmentTimeline& operator=(SegmentTimeline&& other);

    void Init(double start_time, double segment_duration, uint32_t num_segments);

    rocprofvis_result_t FetchSegments(double start, double end, void* user_ptr, FetchSegmentsFunc func);
    rocprofvis_result_t Remove(Segment* segment);
    void Insert(double segment_start, std::unique_ptr<Segment>&& segment);
    std::map<double, std::shared_ptr<Segment>>& GetSegments();
    bool IsValid(uint32_t segment_index) const;
    void SetValid(uint32_t segment_index);
    void SetInvalid(uint32_t segment_index);

    double GetSegmentDuration() const;

private:
    std::map<double, std::shared_ptr<Segment>> m_segments;
    std::vector<std::bitset<kSegmentBitSetSize>> m_valid_segments;
    double                                     m_segment_start_time;
    double                                     m_segment_duration;
    uint32_t                                   m_num_segments;
};

}
}
