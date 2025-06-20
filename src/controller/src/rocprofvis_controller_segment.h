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

constexpr double kSegmentDuration = 1000000000.0;
constexpr uint32_t kSegmentBitSetSize = 64;

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
    Segment();

    Segment(rocprofvis_controller_track_type_t type);

    ~Segment();

    double GetStartTimestamp();
    double GetEndTimestamp();
    double GetMinTimestamp();
    double GetMaxTimestamp();

    void SetStartEndTimestamps(double start, double end);

    void SetMinTimestamp(double value);

    void SetMaxTimestamp(double value);

    void Insert(double timestamp, uint8_t level, Handle* event);

    rocprofvis_result_t Fetch(double start, double end, std::vector<Data>& array, uint64_t& index, std::unordered_set<uint64_t>* event_id_set);

    rocprofvis_result_t GetMemoryUsage(uint64_t* value, rocprofvis_common_property_t property);

private:
    std::map<SegmentItemKey, Handle*>  m_entries;
    double m_start_timestamp;
    double m_end_timestamp;
    double m_min_timestamp;
    double m_max_timestamp;
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

    void Init(double segment_duration, uint32_t num_segments);

    rocprofvis_result_t FetchSegments(double start, double end, void* user_ptr, FetchSegmentsFunc func);
    void Insert(double segment_start, std::unique_ptr<Segment>&& segment);
    std::map<double, std::unique_ptr<Segment>>& GetSegments();

    bool IsValid(uint32_t segment_index) const;
    void SetValid(uint32_t segment_index);

    double GetSegmentDuration() const;

private:
    std::map<double, std::unique_ptr<Segment>> m_segments;
    std::vector<std::bitset<kSegmentBitSetSize>> m_valid_segments;
    double                                     m_segment_duration;
    uint32_t                                   m_num_segments;
};

}
}
