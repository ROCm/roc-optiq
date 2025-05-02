// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include "rocprofvis_controller.h"
#include "rocprofvis_controller_data.h"
#include "rocprofvis_controller_handle.h"
#include <map>
#include <vector>
#include <memory>

namespace RocProfVis
{
namespace Controller
{

class Array;
class Event;
class Sample;

constexpr double kSegmentDuration = 1000000000.0;

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

    void Insert(double timestamp, Handle* event);

    rocprofvis_result_t Fetch(double start, double end, std::vector<Data>& array, uint64_t& index);

    rocprofvis_result_t GetMemoryUsage(uint64_t* value, rocprofvis_common_property_t property);

private:
    std::multimap<double, Handle*>           m_entries;
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

    rocprofvis_result_t FetchSegments(double start, double end, void* user_ptr, FetchSegmentsFunc func);
    void Insert(double segment_start, std::unique_ptr<Segment>&& segment);
    std::map<double, std::unique_ptr<Segment>>& GetSegments();

private:
    std::map<double, std::unique_ptr<Segment>> m_segments;
};

}
}
