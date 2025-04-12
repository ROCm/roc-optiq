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
    Event* new_event = new EventLOD(0, event_start, event_end, events);
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

void Segment::GenerateLOD(uint32_t lod_to_generate)
{
    if (lod_to_generate > 0)
    {
        {
            std::unique_ptr<LOD>& lod = m_lods[lod_to_generate];
            if(!lod)
            {
                m_lods[lod_to_generate] = std::make_unique<LOD>();
            }
        }
        std::unique_ptr<LOD>& lod = m_lods[lod_to_generate];
        ROCPROFVIS_ASSERT(lod);
        if(!lod->IsValid())
        {
            uint32_t previous_lod                  = (uint32_t) (lod_to_generate - 1);
            std::unique_ptr<LOD>& prev_lod         = m_lods[previous_lod];
            std::multimap<double, Handle*>& values = prev_lod->GetEntries();
            if(prev_lod->IsValid() && values.size() > 1)
            {
                double scale = 1.0;
                for(uint32_t i = 0; i < lod_to_generate; i++)
                {
                    scale *= 10.0;
                }
                double min_ts = m_min_timestamp;
                double max_ts = m_min_timestamp + scale;
                if(m_type == kRPVControllerTrackTypeEvents)
                {
                    std::vector<Event*> events;
                    for(auto& pair : values)
                    {
                        Event* event = (Event*) pair.second;
                        if(event)
                        {
                            double event_start = pair.first;
                            double event_end   = 0.0;
                            if(event->GetDouble(kRPVControllerEventEndTimestamp, 0,
                                                &event_end) == kRocProfVisResultSuccess)
                            {
                                if((event_start >= min_ts && event_start <= max_ts) &&
                                   (event_end >= min_ts && event_end <= max_ts))
                                {
                                    // Merge into the current event
                                    AddEvents(events, event, previous_lod);
                                }
                                else
                                {
                                    // We assume that the events are ordered by time, so
                                    // this must at least end after the current event
                                    ROCPROFVIS_ASSERT(event_end > max_ts);

                                    // Generate the stub event for any populated events.
                                    if(events.size() && ((events.front()->GetDouble(
                                            kRPVControllerEventStartTimestamp, 0,
                                            &event_start) == kRocProfVisResultSuccess) &&
                                       (events.back()->GetDouble(
                                            kRPVControllerEventEndTimestamp, 0,
                                            &event_end) == kRocProfVisResultSuccess)))
                                    {
                                        GenerateEventLOD(events, event_start, event_end,
                                                         lod_to_generate);
                                    }

                                    // Create a new event & increment the search
                                    min_ts = pair.first;
                                    max_ts = std::min(pair.first + scale, m_max_timestamp);

                                    events.clear();
                                    AddEvents(events, event, previous_lod);
                                }
                            }
                        }
                    }

                    if(events.size())
                    {
                        double event_start = 0.0;
                        double event_end   = 0.0;
                        if((events.front()->GetDouble(kRPVControllerEventStartTimestamp,
                                                      0, &event_start) ==
                            kRocProfVisResultSuccess) &&
                           (events.back()->GetDouble(kRPVControllerEventEndTimestamp, 0,
                                                     &event_end) ==
                            kRocProfVisResultSuccess))
                        {
                            GenerateEventLOD(events, event_start, event_end,
                                             lod_to_generate);
                        }
                    }
                }
                else
                {
                    std::vector<Sample*> samples;
                    for(auto& pair : values)
                    {
                        Sample* sample = (Sample*) pair.second;
                        if(sample)
                        {
                            double sample_start = pair.first;
                            {
                                if(sample_start >= min_ts && sample_start <= max_ts)
                                {
                                    // Merge into the current sample
                                    AddSamples(samples, sample, previous_lod);
                                }
                                else
                                {
                                    // We assume that the events are ordered by time, so
                                    // this must at least end after the current sample
                                    ROCPROFVIS_ASSERT(sample_start > max_ts);

                                    // Generate the stub event for any populated events.
                                    uint64_t type = 0;
                                    if((samples.size() > 0) && (sample->GetUInt64(kRPVControllerSampleType, 0, &type) == kRocProfVisResultSuccess))
                                    {
                                        GenerateSampleLOD(
                                            samples,
                                            (rocprofvis_controller_primitive_type_t) type,
                                            sample_start, lod_to_generate);
                                    }

                                    // Create a new event & increment the search
                                    do
                                    {
                                        min_ts = std::min(min_ts + scale, m_max_timestamp);
                                        max_ts = std::min(max_ts + scale, m_max_timestamp);
                                    } while(sample_start < min_ts);

                                    samples.clear();
                                    AddSamples(samples, sample, previous_lod);
                                }
                            }
                        }
                    }

                    if(samples.size())
                    {
                        uint64_t type         = 0;
                        double   sample_start = 0.0;
                        if((samples.front()->GetDouble(kRPVControllerSampleTimestamp,
                                                       0, &sample_start) ==
                            kRocProfVisResultSuccess) &&
                           (samples.front()->GetUInt64(kRPVControllerSampleType, 0,
                                                       &type) ==
                            kRocProfVisResultSuccess))
                        {
                            GenerateSampleLOD(
                                samples, (rocprofvis_controller_primitive_type_t) type,
                                sample_start, lod_to_generate);
                        }
                    }
                }

                lod->SetValid(true);
            }
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
        m_lods[0] = std::make_unique<LOD>();
        ROCPROFVIS_ASSERT(m_lods[0]);
        m_lods[0]->SetValid(true);
    }
    m_lods[0]->GetEntries().insert(std::make_pair(timestamp, event));
}

rocprofvis_result_t Segment::Fetch(uint32_t lod, double start, double end, Array& array, uint64_t& index)
{
    rocprofvis_result_t result = kRocProfVisResultOutOfRange;
    if ((start <= m_start_timestamp && end >= m_end_timestamp) || (start >= m_start_timestamp && start < m_end_timestamp) || (end > m_start_timestamp && end <= m_end_timestamp))
    {
        if(m_lods.find(0) != m_lods.end())
        {
            for(uint32_t i = 1; i <= lod; i++)
            {
                if(m_lods.find(i) == m_lods.end() || m_lods[i] == nullptr ||
                    m_lods[i]->IsValid())
                {
                    GenerateLOD(i);
                }
            }

            uint32_t i = lod;
            while(i)
            {
                if(m_lods.find(i) != m_lods.end() && m_lods[i] != nullptr &&
                    m_lods[i]->IsValid())
                {
                    break;
                }
                i--;
            }
    
            auto& lod_ref = m_lods[i];
            auto& entries = lod_ref->GetEntries();
            rocprofvis_controller_properties_t property = (rocprofvis_controller_properties_t)((m_type = kRPVControllerTrackTypeEvents) ? kRPVControllerEventStartTimestamp : kRPVControllerSampleTimestamp);
            auto lower = std::lower_bound(entries.begin(), entries.end(), start, [property](std::pair<double, Handle*> const& pair, double const& start) -> bool
            {
                double max_ts = pair.first;
                pair.second->GetDouble(property, 0, &max_ts);

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

                for (auto& pair : m_lods)
                {
                    *value += sizeof(pair);
                    *value += sizeof(LOD);
                    for (auto& entry : pair.second->GetEntries())
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
                }
                break;
            }
            case kRPVControllerCommonMemoryUsageExclusive:
            {
                *value = 0;
                result = kRocProfVisResultSuccess;

                for(auto& pair : m_lods)
                {
                    *value += sizeof(pair);
                    *value += sizeof(LOD);
                    for(auto& entry : pair.second->GetEntries())
                    {
                        *value += sizeof(entry);
                    }
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

}
}
