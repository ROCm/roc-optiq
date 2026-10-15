// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_controller_graph.h"
#include "rocprofvis_controller_array.h"
#include "rocprofvis_controller_event.h"
#include "rocprofvis_controller_reference.h"
#include "rocprofvis_controller_sample.h"
#include "rocprofvis_controller_sample_lod.h"
#include "rocprofvis_controller_track.h"
#include "rocprofvis_controller_trace_system.h"
#include "rocprofvis_controller_future.h"
#include "rocprofvis_core.h"
#include "rocprofvis_core_assert.h"
#include <cfloat>
#include <utility>

namespace RocProfVis
{
namespace Controller
{

constexpr double kGraphScaleFactor   = 10.0;
constexpr double kMaxSegmentDuration = 60000000000.0;

typedef Reference<rocprofvis_controller_track_t, Track, kRPVControllerObjectTypeTrack>
    TrackRef;
typedef Reference<rocprofvis_controller_event_t, Event, kRPVControllerObjectTypeEvent>
    EventRef;
typedef Reference<rocprofvis_controller_sample_t, Sample, kRPVControllerObjectTypeSample>
    SampleRef;

struct CombinedEventInfo
{
    uint32_t total_count;
    double   total_duration;
};

void
Graph::Insert(uint32_t lod, double timestamp, uint8_t level, Handle* object)
{
    ROCPROFVIS_ASSERT(object);
    ROCPROFVIS_ASSERT(m_lods.find(lod) != m_lods.end());

    rocprofvis_result_t result          = kRocProfVisResultOutOfRange;
    auto                object_type     = object->GetType();
    auto&               segments        = m_lods[lod];
    double              start_timestamp = 0;
    double              end_timestamp   = 0;
    bool                stored_in_segment = false;

    result = GetStartTimestamp(&start_timestamp);
    result =
        (result == kRocProfVisResultSuccess) ? GetEndTimestamp(&end_timestamp) : result;

    if(result == kRocProfVisResultSuccess && timestamp >= start_timestamp &&
       timestamp <= end_timestamp)
    {
        double scale = 1.0;
        for(uint32_t i = 0; i < lod; i++)
        {
            scale *= kGraphScaleFactor;
        }

        double segment_duration =
            std::min(kScalableSegmentDuration * scale, kMaxSegmentDuration);
        double relative         = (timestamp - start_timestamp);
        double num_segments     = floor(relative / segment_duration);
        double segment_start    = start_timestamp + (num_segments * segment_duration);

        std::unique_lock lock(*segments.GetMutex());

        if(segments.GetSegments().find(segment_start) == segments.GetSegments().end())
        {
            rocprofvis_controller_track_type_t track_type =
                m_track->GetTrackType();

            double                   segment_end = segment_start + segment_duration;
            std::unique_ptr<Segment> segment     = std::make_unique<Segment>(
                track_type, &segments);
            segment->SetStartEndTimestamps(segment_start, segment_end);
            segment->SetMinTimestamp(timestamp);
            double max_timestamp = timestamp;
            if(object_type == kRPVControllerObjectTypeEvent)
            {
                max_timestamp = static_cast<Event*>(object)->GetEndTimestamp();
            }
            else
            {
                max_timestamp = static_cast<Sample*>(object)->GetEndTimestamp();
            }
            segment->SetMaxTimestamp(max_timestamp);

            segments.Insert(segment_start, std::move(segment));
            result = (segments.GetSegments().find(segment_start) !=
                      segments.GetSegments().end())
                         ? kRocProfVisResultSuccess
                         : kRocProfVisResultMemoryAllocError;
        }

        if(result == kRocProfVisResultSuccess)
        {
            std::unique_ptr<Segment>& segment = segments.GetSegments()[segment_start];
            segment->SetMinTimestamp(std::min(segment->GetMinTimestamp(), timestamp));
            double max_timestamp = timestamp;
            if(object_type == kRPVControllerObjectTypeEvent)
            {
                max_timestamp = static_cast<Event*>(object)->GetEndTimestamp();
            }
            else
            {
                max_timestamp = static_cast<Sample*>(object)->GetEndTimestamp();
            }
            segment->SetMaxTimestamp(std::max(segment->GetMaxTimestamp(), max_timestamp));
            stored_in_segment = segment->Insert(timestamp, level, object);
        }
    }
    else
    {
        result = kRocProfVisResultOutOfRange;
    }

    if(!stored_in_segment && m_ctx && m_ctx->GetMemoryManager() &&
       !m_ctx->GetMemoryManager()->IsShuttingDown())
    {
        // No segment kept the object...
        m_ctx->GetMemoryManager()->Delete(object, &segments);
    }
}

rocprofvis_result_t
Graph::CombineEventInfo(std::vector<Event*>& events, std::string& combined_name,
                        uint64_t& max_duration_str_index)
{
    if(events.empty())
    {
        spdlog::warn("No events provided to CombineEventInfo.");
        return kRocProfVisResultInvalidArgument;
    }
    if(events.size() == 1)
    {
        combined_name          = events[0]->GetName();
        max_duration_str_index = events[0]->GetNameStringIndex();
        return kRocProfVisResultSuccess;
    }

    std::unordered_map<uint64_t, CombinedEventInfo> info_accumulator;
    std::unordered_map<uint64_t, std::string>       string_cache;
    for(Event* event : events)
    {
        const uint64_t name_index = event->GetNameStringIndex();
        const double   duration   = event->GetDuration();
        auto [it, inserted] =
            info_accumulator.emplace(name_index, CombinedEventInfo{ 1, duration });
        if(inserted)
        {
            string_cache[name_index] = event->GetName();
        }
        else
        {
            ++it->second.total_count;
            it->second.total_duration += duration;
        }
    }

    // Build the combined name string and find max duration.
    // Reset combined_name so callers cannot accidentally pass in a non-empty string.
    combined_name.clear();
    double   max_duration       = 0.0;
    uint64_t max_duration_index = UINT64_MAX;
    bool     first_entry        = true;
    for(const auto& [name_index, info] : info_accumulator)
    {
        if(!std::exchange(first_entry, false))
        {
            combined_name += "\n";
        }
        std::string count_string = std::to_string(info.total_count);
        count_string += "|";
        count_string += std::to_string(static_cast<uint64_t>(info.total_duration));
        count_string += "|";
        combined_name += count_string;
        auto it = string_cache.find(name_index);
        if(it != string_cache.end())
        {
            combined_name += it->second;
        }
        else
        {
            combined_name += "Unknown Name";
            spdlog::warn("String for index {} not found when combining event info.",
                         name_index);
        }

        // Update max values
        if(info.total_duration > max_duration)
        {
            max_duration       = info.total_duration;
            max_duration_index = name_index;
        }
    }

    max_duration_str_index = max_duration_index;
    return kRocProfVisResultSuccess;
}

rocprofvis_result_t
Graph::GenerateLODEvent(std::vector<Event*>& events, uint32_t lod_to_generate,
                        uint32_t level, double event_min, double event_max)
{
    if(events.empty())
        return kRocProfVisResultSuccess;

    if(level > UINT8_MAX)
        return kRocProfVisResultOutOfRange;

    if(auto* mgr = m_ctx ? m_ctx->GetMemoryManager() : nullptr; !mgr)
        return kRocProfVisResultInvalidArgument;
    else
    {
        std::string combined_name;
        uint64_t    max_duration_str_index = UINT64_MAX;
        rocprofvis_result_t result =
            CombineEventInfo(events, combined_name, max_duration_str_index);
        ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);

        const uint64_t event_id = events[0]->GetId();
        Event*         event    = mgr->NewEvent(event_id, event_min, event_max,
                                                &m_lods[lod_to_generate]);
        if(!event)
            return kRocProfVisResultMemoryAllocError;

        result = event->SetLevel(level);
        if(result == kRocProfVisResultSuccess)
            result = event->SetName(combined_name.c_str());
        if(result == kRocProfVisResultSuccess)
        {
            event->SetTopCombinedNameStringIndex(
                static_cast<size_t>(max_duration_str_index));
            result = event->SetNumberOfChildren(events.size());
        }
        for(uint64_t e_idx = 0;
            result == kRocProfVisResultSuccess && e_idx < events.size(); e_idx++)
        {
            result = event->SetChildId(e_idx, events[e_idx]->GetId());
        }
        if(result != kRocProfVisResultSuccess)
        {
            mgr->Delete(event, &m_lods[lod_to_generate]);
            return result;
        }

        Insert(lod_to_generate, event_min, static_cast<uint8_t>(level), event);
    }
    return kRocProfVisResultSuccess;
}

rocprofvis_result_t
Graph::GenerateLOD(uint32_t lod_to_generate, double start_ts, double end_ts,
                   std::vector<Data>& entries, Future* future)
{
    (void) future;
    double scale = 1.0;
    for(uint32_t i = 0; i < lod_to_generate; i++)
    {
        scale *= kGraphScaleFactor;
    }

    const rocprofvis_controller_track_type_t track_type =
        m_track->GetTrackType();

    if(track_type == kRPVControllerTrackTypeEvents)
    {
        double   min_ts = start_ts;
        double   max_ts = start_ts + scale;
        uint64_t level  = 0;
        double event_min = DBL_MAX;
        double event_max = DBL_MIN;
        std::vector<Event*> events;

        for(auto& data : entries)
        {
            rocprofvis_handle_t* handle = nullptr;
            rocprofvis_result_t result = data.GetObject(&handle);
            ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
            if(result != kRocProfVisResultSuccess)
            {
                return result;
            }
            EventRef eventRef((rocprofvis_controller_event_t*) handle);
            if(eventRef.IsValid())
            {
                Event* event = eventRef.Get();
                ROCPROFVIS_ASSERT(event);
                const uint64_t event_level = event->GetLevel();
                if(event_level != level)
                {
                    result = GenerateLODEvent(events, lod_to_generate,
                                              static_cast<uint32_t>(level), event_min,
                                              event_max);
                    if(result != kRocProfVisResultSuccess)
                    {
                        return result;
                    }
                    events.clear();
                    min_ts = start_ts;
                    max_ts = start_ts + scale;
                    level  = event_level;
                    event_min = DBL_MAX;
                    event_max = DBL_MIN;
                }

                const double event_start = event->GetStartTimestamp();
                const double event_end   = event->GetEndTimestamp();

                ROCPROFVIS_ASSERT(level == event_level || level == UINT64_MAX);

                if(event_start < end_ts && event_end >= start_ts)
                {
                    if((event_start >= min_ts && event_start <= max_ts) &&
                       (event_end >= min_ts && event_end <= max_ts))
                    {
                        // Merge into current event
                        events.push_back(event);
                        level = level == UINT64_MAX ? event_level : level;
                        event_min = std::min(event_min, event_start);
                        event_max = std::max(event_max, event_end);
                    }
                    else
                    {
                        // Start a new event

                        double sample_start = event_start;

                        // Generate the stub event for any populated events.
                        result = GenerateLODEvent(events, lod_to_generate,
                                                  static_cast<uint32_t>(level), event_min,
                                                  event_max);
                        if(result != kRocProfVisResultSuccess)
                        {
                            return result;
                        }

                        // Create a new event & increment the search
                        while(max_ts < sample_start && min_ts < end_ts)
                        {
                            min_ts = std::min(max_ts, end_ts);
                            max_ts = std::min(max_ts + scale, end_ts);
                        }

                        events.clear();
                        events.push_back(event);

                        event_min = event_start;
                        event_max = event_end;

                        level = event_level;
                    }
                }
            }
        }
        const rocprofvis_result_t result = GenerateLODEvent(
            events, lod_to_generate, static_cast<uint32_t>(level), event_min, event_max);
        if(result != kRocProfVisResultSuccess)
        {
            return result;
        }
    }
    else
    {
        double               min_ts = start_ts;
        double               max_ts = start_ts + scale;
        std::vector<Sample*> samples;

        for(auto& data : entries)
        {
            rocprofvis_handle_t* handle = nullptr;
            rocprofvis_result_t result = data.GetObject(&handle);
            ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
            if(result != kRocProfVisResultSuccess)
            {
                return result;
            }
            SampleRef sampleRef((rocprofvis_controller_sample_t*) handle);
            if(sampleRef.IsValid())
            {
                Sample* sample = sampleRef.Get();
                ROCPROFVIS_ASSERT(sample);

                const double sample_start = sample->GetTimestamp();
                const double sample_next  = sample->GetEndTimestamp();
                if(sample_start < end_ts && sample_next > start_ts)
                {
                    if(sample_start >= min_ts && sample_start <= max_ts)
                    {
                        // Merge into the current sample
                        samples.push_back(sample);
                    }
                    else
                    {
                        // Generate the stub event for any populated events.

                        if(!samples.empty())
                        {
                            const double sample_insert_ts =
                                samples.front()->GetTimestamp();
                            const double sample_last_ts =
                                samples.back()->GetEndTimestamp();
                            SampleLOD* new_sample =
                                m_ctx->GetMemoryManager()->NewSampleLOD(
                                    0, sample_insert_ts, samples, &m_lods[lod_to_generate]);
                            if(!new_sample)
                            {
                                return kRocProfVisResultMemoryAllocError;
                            }
                            new_sample->SetEndTimestamp(sample_last_ts);
                            Insert(lod_to_generate, sample_insert_ts, 0, new_sample);
                        }

                        // Create a new event & increment the search
                        while(max_ts < sample_start && min_ts < end_ts)
                        {
                            min_ts = std::min(max_ts, end_ts);
                            max_ts = std::min(max_ts + scale, end_ts);
                        }

                        samples.clear();
                        samples.push_back(sample);
                    }
                }
            }
        }

        if(!samples.empty())
        {
            const double sample_insert_ts = samples.front()->GetTimestamp();
            const double sample_last_ts   = samples.back()->GetEndTimestamp();
            SampleLOD*   new_sample       = m_ctx->GetMemoryManager()->NewSampleLOD(
                0, sample_insert_ts, samples, &m_lods[lod_to_generate]);
            if(!new_sample)
            {
                return kRocProfVisResultMemoryAllocError;
            }
            new_sample->SetEndTimestamp(sample_last_ts);
            Insert(lod_to_generate, sample_insert_ts, 0, new_sample);
        }
    }
    return kRocProfVisResultSuccess;
}

struct FetchTrackSegmentArgs
{
    std::vector<Data>         m_entries;
    uint64_t                  m_index;
	SegmentLRUParams          m_lru_params;
};

rocprofvis_result_t
Graph::GenerateLOD(uint32_t lod_to_generate, double start, double end, Future* future,
                   uint64_t array_id)
{
    rocprofvis_result_t result = kRocProfVisResultOutOfRange;
    if(lod_to_generate > 0)
    {
        double min_ts = start;
        double max_ts = end;

        if(m_track)
        {
            min_ts = m_track->GetStartTimestamp();
            max_ts = m_track->GetEndTimestamp();
            double scale = 1.0;
            for(uint32_t i = 0; i < lod_to_generate; i++)
            {
                scale *= kGraphScaleFactor;
            }
            double segment_duration = std::min(std::min(kScalableSegmentDuration * scale, max_ts - min_ts),
                         kMaxSegmentDuration);

            std::map<uint32_t, SegmentTimeline>::iterator it = m_lods.end();
            {
                std::lock_guard lock(m_mutex);

                it = m_lods.find(lod_to_generate);
                if(it == m_lods.end())
                {
                    uint32_t num_segments = static_cast<uint32_t>(ceil((max_ts - min_ts) / segment_duration));
                    uint64_t num_items = m_track->GetNumberOfEntries();
                    SegmentTimeline& segments = m_lods[lod_to_generate];
                    segments.SetContext(this);
                    segments.Init(min_ts, segment_duration, num_segments, num_items);
                    it = m_lods.find(lod_to_generate);
                }
            }

            it->second.AddActiveArray(array_id);

            start = std::max(start, min_ts);
            end   = std::min(end, max_ts);
            if (end > start)
            {
                std::vector<std::pair<uint32_t, uint32_t>> fetch_ranges;
                uint32_t start_index = (uint32_t)floor((start - min_ts) / segment_duration);
                uint32_t end_index = (uint32_t)ceil((end - min_ts) / segment_duration);
                {
                    std::unique_lock lock(*it->second.GetMutex());
                    m_cv.wait(lock, [&] {
                        for(uint32_t i = start_index; i < end_index; i++)
                        {
                            if(it->second.IsProcessed(i))
                            {
                                return false;
                            }
                        }
                        return true;
                    });
                    for(uint32_t i = start_index; i < end_index; i++)
                    {
                        if(!it->second.IsValid(i))
                        {
                            it->second.SetProcessed(i, true);
                            if(fetch_ranges.size())
                            {
                                auto& last_range = fetch_ranges.back();
                                if(last_range.second == i - 1)
                                {
                                    last_range.second = i;
                                }
                                else
                                {
                                    fetch_ranges.push_back(std::make_pair(i, i));
                                }
                            }
                            else
                            {
                                fetch_ranges.push_back(std::make_pair(i, i));
                            }
                        }
                    }
                }

                if(fetch_ranges.size())
                {
                    result = kRocProfVisResultSuccess;
                    for(auto& range : fetch_ranges)
                    {
                        double fetch_start = min_ts + (range.first * segment_duration);
                        double fetch_end   = min_ts + ((range.second + 1) * segment_duration);
                        FetchTrackSegmentArgs args;
                        args.m_index       = 0;
                    	args.m_lru_params.m_ctx      = (SystemTrace*)m_track->GetContext();
                        args.m_lru_params.m_array_id = array_id;

                        m_track->GetSegments()->AddActiveArray(array_id);

                        if(result == kRocProfVisResultSuccess)
                        {
                            result = m_track->FetchSegments(
                                fetch_start, fetch_end, &args, future,
                                [](double start, double end, Segment& segment,
                                   void* user_ptr,
                                   SegmentTimeline* owner) -> rocprofvis_result_t {
                                    FetchTrackSegmentArgs* args =
                                        (FetchTrackSegmentArgs*) user_ptr;
                                    rocprofvis_result_t result = kRocProfVisResultSuccess;
                                    args->m_lru_params.m_owner = owner;
                                    result = segment.Fetch(start, end, args->m_entries,
                                                           args->m_index, nullptr,
                                                           &args->m_lru_params);
                                    ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
                                    return result;
                                });
                        }

                        if(result == kRocProfVisResultSuccess)
                        {
                            result = GenerateLOD(lod_to_generate, fetch_start, fetch_end,
                                                 args.m_entries, future);                            
                        }
                        {
                            std::unique_lock lock(*it->second.GetMutex());
                            for(uint32_t i = range.first; i <= range.second; i++)
                            {
                                it->second.SetProcessed(i, false);
                                it->second.SetValid(i,
                                                    result == kRocProfVisResultSuccess);
                            }
                            
                        }
                        m_cv.notify_all();

                        m_track->GetSegments()->RemoveActiveArray(array_id);
                    }
                }
                else
                {
                    result = kRocProfVisResultSuccess;
                }

                 {
                    std::unique_lock lock(*it->second.GetMutex());
                    m_cv.wait(lock, [&] {
                        for(uint32_t i = start_index; i < end_index; i++)
                        {
                            if(it->second.IsProcessed(i))
                            {
                                return false;
                            }
                        }
                        return true;
                    });
                }
            }

            it->second.RemoveActiveArray(array_id);
        }
    }
    return result;
}

Graph::Graph(Handle* ctx, rocprofvis_controller_graph_type_t type, uint64_t id)
: Handle(__kRPVControllerGraphPropertiesFirst, __kRPVControllerGraphPropertiesLast)
, m_id(id)
, m_track(nullptr)
, m_type(type)
, m_ctx((SystemTrace*)ctx)
{
}

Handle* Graph::GetContext() {
    return m_ctx;
}

void
Graph::UnregisterFromMemoryManager()
{
    if(auto* mgr = m_ctx ? m_ctx->GetMemoryManager() : nullptr; mgr)
    {
        for(auto& [lod, segment_timeline] : m_lods)
        {
            mgr->RemoveLRUOwner(&segment_timeline);
        }
    }
}

struct GraphFetchLODArgs
{
    Array*    m_array;
    uint64_t* m_index;
    SegmentLRUParams m_lru_params;
};

rocprofvis_result_t
Graph::Fetch(uint32_t pixels, double start, double end, Array& array, uint64_t& index, Future* future)
{
    rocprofvis_result_t result = kRocProfVisResultUnknownError;
    MemoryManager* mgr = m_ctx ? m_ctx->GetMemoryManager() : nullptr;

    // Zero out the array - we don't know how many entries we will add and we don't want
    // to report a non-zero value to the caller.
    array.GetVector().clear();

    if(m_track)
    {
        uint32_t lod      = 0;
        double   duration = (end - start);
        while(duration > (pixels * kGraphScaleFactor))
        {
            duration /= kGraphScaleFactor;
            lod++;
        }

        // calculate data for LOD 1 even LOD 0 is requested
        lod = std::max(lod, (uint32_t)1);

        if(mgr)
        {
            mgr->EnterArrayOwnership(array.GetArrayId(), kRocProfVisOwnerTypeGraph);
        }

        array.SetContext(m_ctx);

        result = GenerateLOD(lod, start, end, future, array.GetArrayId());

        auto it = m_lods.find(lod);
        if((it != m_lods.end()) && (result == kRocProfVisResultSuccess))
        {
            GraphFetchLODArgs args;
            args.m_array = &array;
            args.m_index = &index;
            args.m_lru_params.m_ctx = m_ctx;
            args.m_lru_params.m_array_id = array.GetArrayId();

            result = it->second.FetchSegments(
                start, end, &args, future,
                [](double start, double end, Segment& segment,
                   void* user_ptr, SegmentTimeline* owner) -> rocprofvis_result_t {
                    GraphFetchLODArgs*  args   = (GraphFetchLODArgs*) user_ptr;
                    args->m_lru_params.m_owner = owner;
                    return segment.Fetch(start, end, args->m_array->GetVector(),
                                         *(args->m_index),nullptr, &args->m_lru_params);
                });
        }
    }
    ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess ||
                      result == kRocProfVisResultOutOfRange ||
                      result == kRocProfVisResultCancelled);
    return result;
}

rocprofvis_result_t
Graph::GetNumberOfEntries(uint64_t* value) const
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        *value = m_track ? m_track->GetNumberOfEntries() : 0;
        result = m_track ? kRocProfVisResultSuccess : kRocProfVisResultUnknownError;
    }
    return result;
}

rocprofvis_result_t
Graph::GetStartTimestamp(double* value) const
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        result = kRocProfVisResultUnknownError;
        if(m_track)
        {
            *value = m_track->GetStartTimestamp();
            result = kRocProfVisResultSuccess;
        }
    }
    return result;
}

rocprofvis_result_t
Graph::GetEndTimestamp(double* value) const
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        result = kRocProfVisResultUnknownError;
        if(m_track)
        {
            *value = m_track->GetEndTimestamp();
            result = kRocProfVisResultSuccess;
        }
    }
    return result;
}

rocprofvis_result_t
Graph::GetInclusiveMemoryUsage(uint64_t* value) const
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        *value = 0;
        for(const auto& lod : m_lods)
        {
            for(const auto& segment : lod.second.GetSegments())
            {
                uint64_t memory_usage = 0;
                segment.second->GetMemoryUsage(&memory_usage,
                                               kRPVControllerCommonMemoryUsageInclusive);
                *value += memory_usage;
            }
        }
        result = kRocProfVisResultSuccess;
    }
    return result;
}

rocprofvis_result_t
Graph::GetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t* value)
{
    (void) index;
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            case kRPVControllerCommonMemoryUsageInclusive:
            {
                result = GetInclusiveMemoryUsage(value);
                break;
            }
            case kRPVControllerCommonMemoryUsageExclusive:
            {
                *value = GetExclusiveMemoryUsage();
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerGraphId:
            {
                *value = GetId();
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerGraphType:
            {
                *value = GetGraphType();
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerGraphNumEntries:
            {
                result = GetNumberOfEntries(value);
                break;
            }
            default:
            {
                result = UnhandledProperty(property);
                break;
            }
        }
    }
    return result;
}
rocprofvis_result_t
Graph::GetDouble(rocprofvis_property_t property, uint64_t index, double* value)
{
    if(!value)
        return kRocProfVisResultInvalidArgument;
    if(!m_track)
        return kRocProfVisResultUnknownError;

    switch(property)
    {
        case kRPVControllerGraphStartTimestamp:
        {
            return GetStartTimestamp(value);
        }
        case kRPVControllerGraphEndTimestamp:
        {
            return GetEndTimestamp(value);
        }
        default:
        {
            return UnhandledProperty(property);
        }
    }
}
rocprofvis_result_t
Graph::GetObject(rocprofvis_property_t property, uint64_t index,
                 rocprofvis_handle_t** value)
{
    (void) index;
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            case kRPVControllerGraphTrack:
            {
                *value = reinterpret_cast<rocprofvis_handle_t*>(GetTrack());
                result = kRocProfVisResultSuccess;
                break;
            }
            default:
            {
                result = UnhandledProperty(property);
                break;
            }
        }
    }
    return result;
}

rocprofvis_result_t
Graph::SetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t value)
{
    (void) index;
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerGraphType:
        {
            SetGraphType(static_cast<rocprofvis_controller_graph_type_t>(value));
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerGraphId:
        case kRPVControllerGraphNumEntries:
        {
            result = kRocProfVisResultReadOnlyError;
            break;
        }
        default:
        {
            result = UnhandledProperty(property);
            break;
        }
    }
    return result;
}
rocprofvis_result_t
Graph::SetDouble(rocprofvis_property_t property, uint64_t index, double value)
{
    (void) index;
    (void) value;
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerGraphStartTimestamp:
        case kRPVControllerGraphEndTimestamp:
        {
            result = kRocProfVisResultReadOnlyError;
            break;
        }
        default:
        {
            result = UnhandledProperty(property);
            break;
        }
    }
    return result;
}
rocprofvis_result_t
Graph::SetObject(rocprofvis_property_t property, uint64_t index,
                 rocprofvis_handle_t* value)
{
    (void) index;
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            case kRPVControllerGraphTrack:
            {
                TrackRef track_ref(value);
                if(track_ref.IsValid())
                {
                    SetTrack(track_ref.Get());
                    result  = kRocProfVisResultSuccess;
                }
                break;
            }
            default:
            {
                result = UnhandledProperty(property);
                break;
            }
        }
    }
    return result;
}

}  // namespace Controller
}  // namespace RocProfVis
