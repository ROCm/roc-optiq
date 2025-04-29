// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_controller_segment.h"
#include "rocprofvis_controller_array.h"
#include "rocprofvis_controller_event_lod.h"
#include "rocprofvis_controller_sample_lod.h"
#include "rocprofvis_core_assert.h"

#include <algorithm>

namespace RocProfVis
{
namespace Controller
{

Segment::Segment()
: m_start_timestamp(0.0)
, m_end_timestamp(0.0)
, m_min_timestamp(0.0)
, m_max_timestamp(0.0)
, m_type(kRPVControllerTrackTypeSamples)
{
}

Segment::Segment(rocprofvis_controller_track_type_t type)
: m_start_timestamp(0.0)
, m_end_timestamp(0.0)
, m_min_timestamp(0.0)
, m_max_timestamp(0.0)
, m_type(type)
{
}

Segment::~Segment()
{ 
    for(auto& pair : m_entries)
    {
        delete pair.second;
    }
}

static void AddSamples(std::vector<Sample*>& samples, Sample* sample, uint64_t lod)
{
    if (lod != 0)
    {
        uint64_t children = 0;
        rocprofvis_result_t result = sample->GetUInt64(kRPVControllerSampleNumChildren, 0, &children);
        ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
        ROCPROFVIS_ASSERT(children > 0);
        for(uint64_t i = 0; i < children; i++)
        {
            rocprofvis_handle_t* child = nullptr;
            result = sample->GetObject(kRPVControllerSampleChildIndex, i, &child);
            ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
            ROCPROFVIS_ASSERT(child);
            samples.push_back((Sample*)child);
        }
    }
    else
    {
        samples.push_back(sample);
    }
}

static void AddEvents(std::vector<Event*>& events, Event* event, uint64_t lod)
{
    if (lod != 0)
    {
        uint64_t children = 0;
        rocprofvis_result_t result = event->GetUInt64(kRPVControllerEventNumChildren, 0, &children);
        ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
        ROCPROFVIS_ASSERT(children > 0);
        for(uint64_t i = 0; i < children; i++)
        {
            rocprofvis_handle_t* child = nullptr;
            result = event->GetObject(kRPVControllerEventChildIndexed, i, &child);
            ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
            ROCPROFVIS_ASSERT(child);
            events.push_back((Event*)child);
        }
    }
    else
    {
        events.push_back(event);
    }
}

double Segment::GetStartTimestamp()
{
    return m_start_timestamp;
}
double Segment::GetEndTimestamp()
{
    return m_end_timestamp;
}
double Segment::GetMinTimestamp()
{
    return m_min_timestamp;
}
double Segment::GetMaxTimestamp()
{
    return m_max_timestamp;
}

void Segment::SetStartEndTimestamps(double start, double end)
{
    m_start_timestamp = start;
    m_end_timestamp = end;
}

void Segment::SetMinTimestamp(double value)
{
    m_min_timestamp = value;
}

void Segment::SetMaxTimestamp(double value)
{
    m_max_timestamp = value;
}

void Segment::Insert(double timestamp, Handle* event)
{
    m_entries.insert(std::make_pair(timestamp, event));
}

rocprofvis_result_t Segment::Fetch(double start, double end, std::vector<Data>& array, uint64_t& index)
{
    rocprofvis_result_t result = kRocProfVisResultOutOfRange;
    double last_timestamp = std::max(m_end_timestamp, m_max_timestamp);
    if(m_start_timestamp <= end && last_timestamp >= start)
    {
        result = kRocProfVisResultSuccess;
        {
            auto& entries = m_entries; 
            rocprofvis_controller_properties_t property = (rocprofvis_controller_properties_t)((m_type = kRPVControllerTrackTypeEvents) ? kRPVControllerEventEndTimestamp : kRPVControllerSampleTimestamp);
            
            std::multimap<double, Handle*>::iterator lower = entries.end();
            for (auto it = entries.begin(); it != entries.end(); ++it)
            {
                double min_ts = it->first;
                double max_ts = it->first;
                it->second->GetDouble(property, 0, &max_ts);
                if(min_ts <= end && max_ts >= start)
                {
                    lower = it;
                    break;
                }
            }

            std::multimap<double, Handle*>::iterator upper = entries.end();
            for (auto it = entries.begin(); it != entries.end(); ++it)
            {
                double min_ts = it->first;
                if(min_ts > end)
                {
                    upper = it;
                    break;
                }
            }

            while(lower != upper && lower != entries.end())
            {
                double min_ts = lower->first;
                double max_ts = lower->first;
                lower->second->GetDouble(property, 0, &max_ts);

                if(min_ts <= end && max_ts >= start)
                {
                    if(array.size() < index + 1)
                    {
                        array.resize(index + 1);
                    }
                    array[index].SetType(kRPVControllerPrimitiveTypeObject);
                    array[index++] = Data((rocprofvis_handle_t*) lower->second);
                }
                ++lower;
            }
        }
    }
    return result;
}

rocprofvis_result_t Segment::GetMemoryUsage(uint64_t* value, rocprofvis_common_property_t property)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            case kRPVControllerCommonMemoryUsageInclusive:
            {
                *value = 0;
                result = kRocProfVisResultSuccess;

                for(auto& entry : m_entries)
                {
                    *value += sizeof(entry);
                    uint64_t entry_size = 0;
                    result = entry.second->GetUInt64(property, 0, &entry_size);
                    if (result == kRocProfVisResultSuccess)
                    {
                        *value += entry_size;
                    }
                    else
                    {
                        break;
                    }
                }
                break;
            }
            case kRPVControllerCommonMemoryUsageExclusive:
            {
                *value = 0;
                result = kRocProfVisResultSuccess;

                for(auto& entry : m_entries)
                {
                    *value += sizeof(entry);
                }
                break;
            }
            default:
            {
                result = kRocProfVisResultInvalidEnum;
                break;
            }
        }
    }
    return result;
}

SegmentTimeline::SegmentTimeline()
{
}

SegmentTimeline::~SegmentTimeline()
{
}

SegmentTimeline::SegmentTimeline(SegmentTimeline&& other)
: m_segments(std::move(other.m_segments))
{

}

SegmentTimeline& SegmentTimeline::operator=(SegmentTimeline&& other)
{
    m_segments = std::move(other.m_segments);
    return *this;
}

rocprofvis_result_t SegmentTimeline::FetchSegments(double start, double end, void* user_ptr, FetchSegmentsFunc func)
{
    rocprofvis_result_t result = kRocProfVisResultSuccess;
    std::map<double, std::unique_ptr<Segment>>::iterator lower = m_segments.end();
    for(auto it = m_segments.begin(); it != m_segments.end(); ++it)
    {
        double min_ts = it->first;
        double max_ts = it->second->GetMaxTimestamp();
        if(min_ts <= end && max_ts >= start)
        {
            lower = it;
            break;
        }
    }

    std::map<double, std::unique_ptr<Segment>>::iterator upper = m_segments.end();
    for(auto it = m_segments.begin(); it != m_segments.end(); ++it)
    {
        double min_ts = it->first;
        if(min_ts > end)
        {
            upper = it;
            break;
        }
    }

    while(lower != upper && lower != m_segments.end())
    {
        result = func(start, end, *lower->second.get(), user_ptr);
        if(result == kRocProfVisResultSuccess)
        {
            ++lower;
        }
        else if(result == kRocProfVisResultOutOfRange)
        {
            result = kRocProfVisResultSuccess;
            ++lower;
        }
        else
        {
            break;
        }
    }
    return result;
}

void SegmentTimeline::Insert(double segment_start, std::unique_ptr<Segment>&& segment)
{
    m_segments.insert(std::make_pair(segment_start, std::move(segment)));
}

std::map<double, std::unique_ptr<Segment>>& SegmentTimeline::GetSegments()
{
    return m_segments;
}

}
}
