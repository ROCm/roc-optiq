// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_controller_segment.h"
#include "rocprofvis_controller_array.h"
#include "rocprofvis_controller_event_lod.h"
#include "rocprofvis_controller_sample_lod.h"

#include <cassert>
#include <algorithm>

namespace RocProfVis
{
namespace Controller
{

Segment::LOD::LOD()
: m_valid(false)
{
}

Segment::LOD::LOD(LOD const& other)
{ 
    operator=(other);
}

Segment::LOD::LOD(LOD&& other)
{
    operator=(other);
}
        
Segment::LOD::~LOD()
{
    for (auto& pair : m_entries)
    {
        delete pair.second;
    }
}

Segment::LOD& Segment::LOD::operator=(LOD const& other)
{
    if (this != &other)
    {
        m_entries = other.m_entries;
        m_valid   = other.m_valid;
    }
    return *this;
}

Segment::LOD& Segment::LOD::operator=(LOD&& other)
{
    if(this != &other)
    {
        m_entries = other.m_entries;
        m_valid   = other.m_valid;
    }
    return *this;
}

std::multimap<double, Handle*>& Segment::LOD::GetEntries()
{
    return m_entries;
}

void Segment::LOD::SetValid(bool valid)
{
    m_valid = valid;
}

bool Segment::LOD::IsValid() const
{
    return m_valid;
}

void Segment::GenerateEventLOD(std::vector<Event*>& events, double event_start, double event_end, uint32_t lod_to_generate)
{
    Event* new_event = new EventLOD(0, event_start, event_end);
    if (new_event)
    {
        std::multimap<double, Handle*>& new_events = m_lods[lod_to_generate]->GetEntries();
        new_events.insert(std::make_pair(event_start, new_event));
    }
}

void Segment::GenerateSampleLOD(std::vector<Sample*>& samples, rocprofvis_controller_primitive_type_t type, double timestamp, uint32_t lod_to_generate)
{
    SampleLOD* new_sample = new SampleLOD(type, 0, timestamp, samples);
    if (new_sample)
    {
        std::multimap<double, Handle*>& new_samples = m_lods[lod_to_generate]->GetEntries();
        new_samples.insert(std::make_pair(timestamp, new_sample));
    }
}

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
    for (auto& pair : m_lods)
    {
        delete pair.second;
    }
}

void Segment::GenerateLOD(uint32_t lod_to_generate)
{
    if (lod_to_generate > 0)
    {
        uint32_t previous_lod = (uint32_t)(lod_to_generate - 1);
        LOD* lod = m_lods[previous_lod];
        std::multimap<double, Handle*>& values = lod->GetEntries();
        if(!lod->IsValid() && values.size() > 1)
        {
            double scale = 1.0;
            for(uint32_t i = 0; i < lod_to_generate; i++)
            {
                scale *= 1000.0;
            }
            double min_ts = m_min_timestamp;
            double max_ts = m_min_timestamp + scale;
            if (m_type = kRPVControllerTrackTypeEvents)
            {
                std::vector<Event*> events;
                for(auto& pair : values)
                {
                    Event* event = (Event*)pair.second;
                    if (event)
                    {
                        double event_start = pair.first;
                        double event_end = 0.0;
                        if (event->GetDouble(kRPVControllerEventEndTimestamp, 0, &event_end) == kRocProfVisResultSuccess)
                        {
                            if ((event_start >= min_ts && event_start <= max_ts)
                            && (event_end >= min_ts && event_end <= max_ts))
                            {
                                // Merge into the current event
                                events.push_back(event);
                            }
                            else
                            {
                                // We assume that the events are ordered by time, so this must at least end after the current event
                                assert(event_end > max_ts);

                                // Generate the stub event for any populated events.
                                if ((events.front()->GetDouble(kRPVControllerEventStartTimestamp, 0, &event_start) == kRocProfVisResultSuccess)
                                && (events.back()->GetDouble(kRPVControllerEventEndTimestamp, 0, &event_end) == kRocProfVisResultSuccess))
                                {
                                    GenerateEventLOD(events, event_start, event_end, lod_to_generate);
                                }

                                // Create a new event & increment the search
                                min_ts = std::min(min_ts + scale, m_max_timestamp);
                                max_ts = std::min(max_ts + scale, m_max_timestamp);
                                events.clear();
                                events.push_back(event);
                            }
                        }
                    }
                }
                
                if (events.size())
                {
                    double event_start = 0.0;
                    double event_end = 0.0;
                    if ((events.front()->GetDouble(kRPVControllerEventStartTimestamp, 0, &event_start) == kRocProfVisResultSuccess)
                    && (events.back()->GetDouble(kRPVControllerEventEndTimestamp, 0, &event_end) == kRocProfVisResultSuccess))
                    {
                        GenerateEventLOD(events, event_start, event_end, lod_to_generate);
                    }
                }
            }
            else
            {
                std::vector<Sample*> samples;
                for(auto& pair : values)
                {
                    Sample* sample = (Sample*)pair.second;
                    if (sample)
                    {
                        double sample_start = pair.first;
                        {
                            if (sample_start >= min_ts && sample_start <= max_ts)
                            {
                                // Merge into the current sample
                                samples.push_back(sample);
                            }
                            else
                            {
                                // We assume that the events are ordered by time, so this must at least end after the current sample
                                assert(sample_start > max_ts);

                                // Generate the stub event for any populated events.
                                uint64_t type = 0;
                                if (sample->GetUInt64(kRPVControllerSampleType, 0, &type) == kRocProfVisResultSuccess)
                                {
                                    GenerateSampleLOD(samples, (rocprofvis_controller_primitive_type_t)type, sample_start, lod_to_generate);
                                }

                                // Create a new sample & increment the search
                                min_ts = std::min(min_ts + scale, m_max_timestamp);
                                max_ts = std::min(max_ts + scale, m_max_timestamp);
                                samples.clear();
                                samples.push_back(sample);
                            }
                        }
                    }
                }
            }

            lod->SetValid(true);
        }
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
    if (m_lods.find(0) == m_lods.end())
    {
        // LOD0 is always valid or nothing will work.
        m_lods[0] = new LOD();
        m_lods[0]->SetValid(true);
    }
    m_lods[0]->GetEntries().insert(std::make_pair(timestamp, event));
}

rocprofvis_result_t Segment::Fetch(uint32_t lod, double start, double end, Array& array, uint64_t& index)
{
    rocprofvis_result_t result = kRocProfVisResultOutOfRange;
    if (m_lods.find(lod) != m_lods.end()
        && (start <= m_start_timestamp && end >= m_end_timestamp) || (start >= m_start_timestamp && start < m_end_timestamp) || (end > m_start_timestamp && end <= m_end_timestamp))
    {
        auto& lod_ref = m_lods[lod];
        auto& entries = lod_ref->GetEntries();
        auto lower = std::lower_bound(entries.begin(), entries.end(), start, [](std::pair<double, Handle*> const& pair, double const& start) -> bool
        {
            double max_ts = pair.first;
            pair.second->GetDouble(kRPVControllerEventStartTimestamp, 0, &max_ts);

            bool result = max_ts < start;
            return result;
        });
        auto upper = std::upper_bound(entries.begin(), entries.end(), end, [](double const& end, std::pair<double, Handle*> const& pair) -> bool
        {
            bool result = end <= pair.first;
            return result;
        });
        while (lower != upper)
        {
            result = array.SetUInt64(kRPVControllerArrayNumEntries, 0, index + 1);
            if(result == kRocProfVisResultSuccess)
            {
                result = array.SetObject(kRPVControllerArrayEntryIndexed, index++, (rocprofvis_handle_t*)lower->second);
                if(result == kRocProfVisResultSuccess)
                {
                    ++lower;
                }
                else
                {
                    break;
                }
            }
            else
            {
                break;
            }
        }
    }
    return result;
}

}
}
