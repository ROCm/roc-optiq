// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_controller_segment.h"
#include "rocprofvis_controller_array.h"
#include "rocprofvis_controller_event.h"
#include "rocprofvis_controller_sample_lod.h"
#include "rocprofvis_core_assert.h"
#include "rocprofvis_controller_trace.h"

#include <algorithm>
#include <execution>

namespace RocProfVis
{
namespace Controller
{

Segment::Segment(rocprofvis_controller_track_type_t type, Handle* ctx)
: m_start_timestamp(0.0)
, m_end_timestamp(0.0)
, m_min_timestamp(0.0)
, m_max_timestamp(0.0)
, m_type(type)
, m_ctx(ctx)
{
}



Segment::~Segment()
{
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    Trace* trace = (Trace*) m_ctx->GetContext();
    if(trace->GetMemoryManager())
    {
        for(auto& level : m_entries)
        {
            for(auto& pair : level.second)
            {
                ((Trace*) m_ctx->GetContext())->GetMemoryManager()->Delete(pair.second);
            }
        }
    }
    else
    {
        for(auto& level : m_entries)
        {
            level.second.clear();
        }
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

void Segment::Insert(double timestamp, uint8_t level, Handle* event)
{
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    event->IncreaseRetainCounter();
    m_entries[level].insert(std::make_pair(timestamp, event));
}

rocprofvis_result_t Segment::Fetch(double start, double end, std::vector<Data>& array, uint64_t& index, std::unordered_set<uint64_t>* event_id_set, SegmentLRUParams* lru_params)
{
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    rocprofvis_result_t result = kRocProfVisResultOutOfRange;
    double last_timestamp = std::max(m_end_timestamp, m_max_timestamp);
    if(m_start_timestamp <= end && last_timestamp >= start)
    {
        if(lru_params)
        {
            SetLRUIterator(lru_params->m_ctx->GetMemoryManager()->AddLRUReference(
                lru_params->m_owner, this, lru_params->m_lod, &array));
        }
        result = kRocProfVisResultSuccess;
        for(auto& level : m_entries)
        {
            auto&                              entries = level.second;
            rocprofvis_controller_properties_t property =
                (rocprofvis_controller_properties_t) ((m_type ==
                                                        kRPVControllerTrackTypeEvents)
                                                            ? kRPVControllerEventEndTimestamp
                                                            : kRPVControllerSampleTimestamp);

            std::map<double, Handle*>::iterator lower = entries.end();
            for(auto it = entries.begin(); it != entries.end(); ++it)
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

            std::map<double, Handle*>::iterator upper = entries.end();
            for(auto it = entries.begin(); it != entries.end(); ++it)
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

                if(event_id_set && m_type == kRPVControllerTrackTypeEvents)
                {
                    uint64_t event_id;
                    lower->second->GetUInt64(kRPVControllerEventId, 0, &event_id);
                    auto it = event_id_set->find(event_id);
                    if(it != event_id_set->end())
                    {
                        // spdlog::debug("Remove duplicate with id = {}", event_id);
                        ++lower;
                        continue;
                    }
                    event_id_set->insert(event_id);
                }

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

                for(auto& level : m_entries)
                {
                    for(auto& entry : level.second)
                    {
                        *value += sizeof(entry);
                        uint64_t entry_size = 0;
                        result = entry.second->GetUInt64(property, 0, &entry_size);
                        if(result == kRocProfVisResultSuccess)
                        {
                            *value += entry_size;
                        }
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


void
Segment::SetTimelineIterator(rocprofvis_timeline_iterator_t timeline_iterator)
{
    m_timeline_iterator = timeline_iterator;
}

void
Segment::SetLRUIterator(rocprofvis_lru_iterator_t lru_iterator)
{
    m_lru_iterator = lru_iterator;
}

Segment::rocprofvis_timeline_iterator_t&
Segment::GetTimelineIterator(void)
{
    return m_timeline_iterator;
}

Segment::rocprofvis_lru_iterator_t&
Segment::GetLRUIterator(void) {
    return m_lru_iterator;
}

size_t Segment::GetNumEntries()
{
    size_t num_entries = 0;
    for (auto level : m_entries)
    {
        num_entries += level.second.size();
    }
    return num_entries;
};


SegmentTimeline::SegmentTimeline()
: m_segment_duration(0)
, m_num_segments(0)
, m_segment_start_time(0)
{
}

SegmentTimeline::~SegmentTimeline()
{
}

SegmentTimeline::SegmentTimeline(SegmentTimeline&& other)
: m_segments(std::move(other.m_segments))
, m_valid_segments(std::move(other.m_valid_segments))
, m_processed_segments(std::move(other.m_processed_segments))
, m_segment_duration(other.m_segment_duration)
, m_num_segments(other.m_num_segments)
, m_segment_start_time(other.m_segment_start_time)
{

}

SegmentTimeline& SegmentTimeline::operator=(SegmentTimeline&& other)
{
    m_segments = std::move(other.m_segments);
    m_segment_duration = other.m_segment_duration;
    m_num_segments     = other.m_num_segments;
    m_valid_segments = std::move(other.m_valid_segments);
    m_processed_segments     = std::move(other.m_processed_segments);
    m_segment_start_time = other.m_segment_start_time;
    return *this;
}

void SegmentTimeline::Init(double segment_start_time, double segment_duration, uint32_t num_segments)
{
    m_segment_duration = segment_duration;
    m_num_segments = num_segments;
    m_segment_start_time = segment_start_time;
    m_valid_segments.Init(num_segments);
    m_processed_segments.Init(num_segments);
}

void SegmentTimeline::SetContext(Handle* ctx) {
    m_ctx = ctx;
}

rocprofvis_result_t SegmentTimeline::FetchSegments(double start, double end, void* user_ptr, FetchSegmentsFunc func)
{
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    rocprofvis_result_t result = kRocProfVisResultOutOfRange;
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
        result = func(start, end, *lower->second.get(), user_ptr, this);
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

rocprofvis_result_t
SegmentTimeline::Insert(double segment_start, std::unique_ptr<Segment>&& segment)
{
    rocprofvis_result_t                 result = kRocProfVisResultMemoryAllocError;
    auto pair = m_segments.insert(std::make_pair(segment_start, std::move(segment)));
    if(pair.second)
    {
        pair.first->second.get()->SetTimelineIterator(pair.first);
        pair.first->second.get()->SetLRUIterator(((Trace*) m_ctx)->GetMemoryManager()->GetDefaultLRUIterator());
        result = kRocProfVisResultSuccess;
    }
    else
    {
        result = kRocProfVisResultDuplicate;
    }
    return result;
}

std::map<double, std::unique_ptr<Segment>>& SegmentTimeline::GetSegments()
{
    return m_segments;
}

bool
SegmentTimeline::IsValid(uint32_t segment_index) const
{
    bool is_set = false;
    if(segment_index < m_num_segments)
    {
        is_set = m_valid_segments.Test(segment_index);
    }
    return is_set;
}

void
SegmentTimeline::SetValid(uint32_t segment_index, bool state)
{
    if(segment_index < m_num_segments)
    {
        if(state)
        {
            m_valid_segments.Set(segment_index);
        }
        else
        {
            m_valid_segments.Clear(segment_index);
        }
    }
}


bool SegmentTimeline::IsProcessed(uint32_t segment_index) const 
{
    bool is_set = false;
    if(segment_index < m_num_segments)
    {
        is_set = m_processed_segments.Test(segment_index);
    }
    return is_set;
}

void SegmentTimeline::SetProcessed(uint32_t segment_index, bool state) 
{
    if(segment_index < m_num_segments)
    {
        if(state)
        {
            m_processed_segments.Set(segment_index);
        }
        else
        {
            m_processed_segments.Clear(segment_index);
        }
    }
}

double SegmentTimeline::GetSegmentDuration() const
{
    return m_segment_duration;
}

std::shared_mutex* SegmentTimeline::GetMutex() {
    return &m_mutex;
}

rocprofvis_result_t SegmentTimeline::Remove(Segment* target)
{ 
    // do not lock segment deletion. It's been locked by memory manager

    int segment_index =
        (target->GetStartTimestamp() - m_segment_start_time) / m_segment_duration;
    SetValid(segment_index, false);
    m_segments.erase(target->GetTimelineIterator());

    return kRocProfVisResultSuccess;
}

}
}
