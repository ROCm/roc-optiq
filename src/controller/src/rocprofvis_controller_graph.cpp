// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_controller_graph.h"
#include "rocprofvis_controller_array.h"
#include "rocprofvis_controller_event.h"
#include "rocprofvis_controller_reference.h"
#include "rocprofvis_controller_sample.h"
#include "rocprofvis_controller_sample_lod.h"
#include "rocprofvis_controller_track.h"
#include "rocprofvis_controller_trace.h"
#include "rocprofvis_controller_future.h"
#include "rocprofvis_core.h"
#include "rocprofvis_core_assert.h"
#include <cfloat>

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

    result = GetDouble(kRPVControllerGraphStartTimestamp, 0, &start_timestamp);
    result = (result == kRocProfVisResultSuccess)
                 ? GetDouble(kRPVControllerGraphEndTimestamp, 0, &end_timestamp)
                 : result;

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
            uint64_t track_type = 0;
            result = m_track->GetUInt64(kRPVControllerTrackType, 0, &track_type);
            ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);

            double                   segment_end = segment_start + segment_duration;
            std::unique_ptr<Segment> segment     = std::make_unique<Segment>(
                (rocprofvis_controller_track_type_t) track_type, &segments);
            segment->SetStartEndTimestamps(segment_start, segment_end);
            segment->SetMinTimestamp(timestamp);
            double max_timestamp = timestamp;
            if(object_type == kRPVControllerObjectTypeEvent)
            {               
                object->GetDouble(kRPVControllerEventEndTimestamp, 0, &max_timestamp);
            }
            else
            {
                object->GetDouble(kRPVControllerSampleEndTimestamp, 0, &max_timestamp);
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
                object->GetDouble(kRPVControllerEventEndTimestamp, 0, &max_timestamp);
            }
            else
            {
                object->GetDouble(kRPVControllerSampleEndTimestamp, 0, &max_timestamp);
            }
            segment->SetMaxTimestamp(std::max(segment->GetMaxTimestamp(), max_timestamp));
            segment->Insert(timestamp, level, object);
        }
    }
    else
    {
        result = kRocProfVisResultOutOfRange;
    }
}

rocprofvis_result_t
Graph::CombineEventInfo(std::vector<Event*>& events, std::string& combined_name,
                        uint64_t& max_duration_str_index)
{
    rocprofvis_result_t result = kRocProfVisResultUnknownError;
    if(events.size() == 0)
    {
        spdlog::warn("No events provided to CombineEventInfo.");
        return kRocProfVisResultInvalidArgument;
    }
    if(events.size() == 1)
    {
        // If only one event, return its name directly
        uint32_t len = 0;
        result       = events[0]->GetString(kRPVControllerEventName, 0, nullptr, &len);
        if(result == kRocProfVisResultSuccess)
        {
            combined_name.clear();
            combined_name.resize(len);
            result = events[0]->GetString(kRPVControllerEventName, 0,
                                          const_cast<char*>(combined_name.c_str()), &len);
            if(result == kRocProfVisResultSuccess)
            {
                result = events[0]->GetUInt64(kRPVControllerEventNameStrIndex, 0,
                                              &max_duration_str_index);
            }
        }
        return result;
    }

    std::unordered_map<uint64_t, CombinedEventInfo> info_accumulator;
    std::unordered_map<uint64_t, std::string>       string_cache;
    std::string                                     name;
    for(auto event : events)
    {
        uint64_t name_index = 0;
        uint32_t len        = 0;
        double   duration   = 0.0;
        result              = event->GetDouble(kRPVControllerEventDuration, 0, &duration);
        if(result == kRocProfVisResultSuccess)
        {
            result = event->GetUInt64(kRPVControllerEventNameStrIndex, 0, &name_index);
            if(result == kRocProfVisResultSuccess)
            {
                // Try to insert or update the count and duration
                auto [it, inserted] = info_accumulator.emplace(
                    name_index, CombinedEventInfo{ 1, duration });
                if(!inserted)  // Key already exists
                {
                    // Increment the existing values
                    ++it->second.total_count;
                    it->second.total_duration += duration;
                }
                else
                {
                    // cache the name of this string index for later
                    result = event->GetString(kRPVControllerEventName, 0, nullptr, &len);
                    if(result == kRocProfVisResultSuccess)
                    {
                        name.clear();
                        name.resize(len);
                        result = event->GetString(kRPVControllerEventName, 0,
                                                  const_cast<char*>(name.c_str()), &len);
                        if(result == kRocProfVisResultSuccess)
                        {
                            string_cache[name_index] = name;
                        }
                    }
                }
            }
        }
    }

    // Build the combined name string and find max duration
    double   max_duration       = 0.0;
    uint64_t max_duration_index = UINT64_MAX;
    for(const auto& [name_index, info] : info_accumulator)
    {
        if(combined_name.size() > 0)
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
Graph::GenerateLODEvent(std::vector<Event*> & events, uint32_t lod_to_generate, uint32_t level, double event_min, double event_max)
{
    if(events.size())
    {
        std::string combined_name = "";
        uint64_t max_duration_str_index = UINT64_MAX;
        rocprofvis_result_t result = CombineEventInfo(events, combined_name, max_duration_str_index);
        ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);

        uint64_t event_id = 0;
        events[0]->GetUInt64(kRPVControllerEventId, 0, &event_id);
        Event* event = m_ctx->GetMemoryManager()->NewEvent(
            event_id, event_min, event_max,
            &m_lods[lod_to_generate]);
        ROCPROFVIS_ASSERT(level != UINT64_MAX);
        event->SetUInt64(kRPVControllerEventLevel, 0, level);
        event->SetString(kRPVControllerEventName, 0, combined_name.c_str());
        event->SetUInt64(kRPVControllerEventTopCombinedNameStrIndex, 0,
                         max_duration_str_index);
        event->SetUInt64(kRPVControllerEventNumChildren, 0, events.size());
        for(uint32_t e_idx = 0; e_idx < events.size(); e_idx++)
        {
            auto*    e    = events[e_idx];
            uint64_t e_id = 0;
            if(e->GetUInt64(kRPVControllerEventId, 0, &e_id) == kRocProfVisResultSuccess)
            {
                event->SetUInt64(kRPVControllerEventChildIndexed, e_idx, e_id);
            }
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
    rocprofvis_result_t result = kRocProfVisResultUnknownError;
    double              scale  = 1.0;
    for(uint32_t i = 0; i < lod_to_generate; i++)
    {
        scale *= kGraphScaleFactor;
    }

    uint64_t track_type = 0;
    result              = m_track->GetUInt64(kRPVControllerTrackType, 0, &track_type);
    ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);

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
            //if(future->IsCancelled())
            //{
            //    break;
            //}
            rocprofvis_handle_t* handle = nullptr;
            result                      = data.GetObject(&handle);
            ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
            EventRef eventRef((rocprofvis_controller_event_t*) handle);
            if((result == kRocProfVisResultSuccess) && eventRef.IsValid())
            {
                Event* event = eventRef.Get();
                ROCPROFVIS_ASSERT(event);
                uint64_t event_level = 0;
                result = event->GetUInt64(kRPVControllerEventLevel, 0, &event_level);
                ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
                if(event_level != level)
                {
                    GenerateLODEvent(events,lod_to_generate,static_cast<uint32_t>(level),event_min,event_max);
                    events.clear();
                    min_ts = start_ts;
                    max_ts = start_ts + scale;
                    level  = event_level;
                    event_min = DBL_MAX;
                    event_max = DBL_MIN;
                }

                double   event_start = 0.0;
                double   event_end   = 0.0;

                result =
                    event->GetDouble(kRPVControllerEventStartTimestamp, 0, &event_start);
                result =
                    (result == kRocProfVisResultSuccess)
                        ? event->GetDouble(kRPVControllerEventEndTimestamp, 0, &event_end)
                        : result;

                ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);

                if(result == kRocProfVisResultSuccess)
                {
                    ROCPROFVIS_ASSERT(level == event_level || level == UINT64_MAX);

                    if (event_start < end_ts && event_end > start_ts)
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
                            GenerateLODEvent(events,lod_to_generate,static_cast<uint32_t>(level),event_min,event_max);

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
        }
        GenerateLODEvent(events, lod_to_generate, static_cast<uint32_t>(level), event_min, event_max);
    }
    else
    {
        double               min_ts = start_ts;
        double               max_ts = start_ts + scale;
        std::vector<Sample*> samples;
        double sample_insert_ts = 0.0;
        double sample_last_ts = 0.0;
        double sample_last_value = 0.0;
        uint64_t type = 0;

        for(auto& data : entries)
        {
            //if(future->IsCancelled())
            //{
            //    break;
            //}
            rocprofvis_handle_t* handle = nullptr;
            result                      = data.GetObject(&handle);
            ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
            SampleRef sampleRef((rocprofvis_controller_sample_t*) handle);
            if((result == kRocProfVisResultSuccess) && sampleRef.IsValid())
            {
                Sample* sample = sampleRef.Get();
                ROCPROFVIS_ASSERT(sample);

                double sample_start = 0.0;
                double sample_next = 0.0;
                result =
                    sample->GetDouble(kRPVControllerSampleTimestamp, 0, &sample_start);
                result =
                    (result == kRocProfVisResultSuccess)
                    ? sample->GetDouble(kRPVControllerSampleEndTimestamp, 0, &sample_next)
                    : result;
                ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
                if(result == kRocProfVisResultSuccess)
                {
                    if (sample_start < end_ts && sample_next > start_ts)
                    {
                        if (sample_start >= min_ts && sample_start <= max_ts) 
                        {
                            // Merge into the current sample
                            samples.push_back(sample);
                        }
                        else
                        {
                            // Generate the stub event for any populated events.
                            
                            if ((samples.size() > 0) &&
                                (samples.front()->GetUInt64(kRPVControllerSampleType, 0, &type) == kRocProfVisResultSuccess) &&
                                (samples.front()->GetDouble(kRPVControllerSampleTimestamp, 0,&sample_insert_ts) == kRocProfVisResultSuccess) &&
                                (samples.back()->GetDouble(kRPVControllerSampleEndTimestamp, 0,&sample_last_ts) == kRocProfVisResultSuccess))
                            {
                                SampleLOD* new_sample = m_ctx->GetMemoryManager()->NewSampleLOD(
                                    (rocprofvis_controller_primitive_type_t)type, 0,
                                    sample_insert_ts, samples, &m_lods[lod_to_generate]);
                                new_sample->SetDouble(
                                    kRPVControllerSampleEndTimestamp, 0, sample_last_ts);
                                Insert(lod_to_generate, sample_insert_ts, 0, new_sample);
                            }

                            // Create a new event & increment the search
                            while (max_ts < sample_start && min_ts < end_ts)
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
        }

        if ((samples.size() > 0) &&
            (samples.front()->GetUInt64(kRPVControllerSampleType, 0, &type) == kRocProfVisResultSuccess) &&
            (samples.front()->GetDouble(kRPVControllerSampleTimestamp, 0,&sample_insert_ts) == kRocProfVisResultSuccess) &&
            (samples.back()->GetDouble(kRPVControllerSampleEndTimestamp, 0,&sample_last_ts) == kRocProfVisResultSuccess))
        {
            SampleLOD* new_sample = m_ctx->GetMemoryManager()->NewSampleLOD(
                (rocprofvis_controller_primitive_type_t)type, 0,
                sample_insert_ts, samples, &m_lods[lod_to_generate]);
            new_sample->SetDouble(
                kRPVControllerSampleEndTimestamp, 0, sample_last_ts);
            Insert(lod_to_generate, sample_insert_ts, 0, new_sample);
            
        }
    }
    ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
    return result;
}

struct FetchTrackSegmentArgs
{
    std::vector<Data>         m_entries;
    uint64_t                  m_index;
	SegmentLRUParams          m_lru_params;
};

rocprofvis_result_t
Graph::GenerateLOD(uint32_t lod_to_generate, double start, double end, Future* future)
{
    rocprofvis_result_t result = kRocProfVisResultOutOfRange;
    if(lod_to_generate > 0)
    {
        double min_ts = start;
        double max_ts = end;

        if((m_track->GetDouble(kRPVControllerTrackMinTimestamp, 0, &min_ts) ==
            kRocProfVisResultSuccess) &&
           (m_track->GetDouble(kRPVControllerTrackMaxTimestamp, 0, &max_ts) ==
            kRocProfVisResultSuccess))
        {
            double scale = 1.0;
            for(uint32_t i = 0; i < lod_to_generate; i++)
            {
                scale *= kGraphScaleFactor;
            }
            double segment_duration = std::min(std::min(kScalableSegmentDuration * scale, max_ts - min_ts),
                         kMaxSegmentDuration);

            auto it = m_lods.find(lod_to_generate);
            if(it == m_lods.end())
            {
                uint32_t num_segments = static_cast<uint32_t>(ceil((max_ts - min_ts) / segment_duration));
                SegmentTimeline& segments = m_lods[lod_to_generate];
                uint64_t         num_items    = 0;
                m_track->GetUInt64(kRPVControllerTrackNumberOfEntries, 0, &num_items);
                segments.SetContext(this);
                segments.Init(min_ts, segment_duration, num_segments, num_items);    
                it = m_lods.find(lod_to_generate);
            }

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
                    for(auto& range : fetch_ranges)
                    {
                        double fetch_start = min_ts + (range.first * segment_duration);
                        double fetch_end   = min_ts + ((range.second + 1) * segment_duration);
                        FetchTrackSegmentArgs args;
                        args.m_index       = 0;
                    	args.m_lru_params.m_ctx      = (Trace*)m_track->GetContext();
                    	args.m_lru_params.m_lod      = 0;
                        m_ctx->GetMemoryManager()->EnterArrayOwnership(&args.m_entries, kRocProfVisOwnerTypeTrack);

                        result = m_track->FetchSegments(
                            fetch_start, fetch_end, &args, future,
                            [](double start, double end, Segment& segment,
                                void*            user_ptr,
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
                        ((Trace*)m_track->GetContext())->GetMemoryManager()->CancelArrayOwnership(&args.m_entries, kRocProfVisOwnerTypeTrack);
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
        }
    }
    return result;
}

Graph::Graph(Handle* ctx, rocprofvis_controller_graph_type_t type, uint64_t id)
: Handle(__kRPVControllerGraphPropertiesFirst, __kRPVControllerGraphPropertiesLast)
, m_id(id)
, m_track(nullptr)
, m_type(type)
, m_ctx((Trace*)ctx)
{
}

Graph::~Graph() {}

Handle* Graph::GetContext() {
    return m_ctx;
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

        result = GenerateLOD(lod, start, end, future);

        auto it = m_lods.find(lod);
        if((it != m_lods.end()) && (result == kRocProfVisResultSuccess))
        {
            GraphFetchLODArgs args;
            args.m_array = &array;
            args.m_index = &index;
            args.m_lru_params.m_ctx = m_ctx;
            args.m_lru_params.m_lod   = lod;
            m_ctx->GetMemoryManager()->EnterArrayOwnership(&args.m_array->GetVector(), kRocProfVisOwnerTypeGraph);
            array.SetContext(m_ctx);

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


rocprofvis_controller_object_type_t
Graph::GetType(void)
{
    return kRPVControllerObjectTypeGraph;
}

rocprofvis_result_t
Graph::GetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t* value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            case kRPVControllerCommonMemoryUsageInclusive:
            {
                *value = 0;
                for(auto it_lods = m_lods.begin(); it_lods != m_lods.end(); ++it_lods)
                {
                    for(auto it = it_lods->second.GetSegments().begin();
                        it != it_lods->second.GetSegments().end(); ++it)
                    {
                        uint64_t mem_usage;
                        it->second.get()->GetMemoryUsage(
                            &mem_usage, kRPVControllerCommonMemoryUsageInclusive);
                        *value += mem_usage;
                    }
                }
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerCommonMemoryUsageExclusive:
            {
                *value = sizeof(Graph);
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerGraphId:
            {
                *value = m_id;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerGraphType:
            {
                *value = m_type;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerGraphNumEntries:
            {
                *value = 0;
                result = m_track ? m_track->GetUInt64(kRPVControllerTrackNumberOfEntries,
                                                      index, value)
                                 : kRocProfVisResultUnknownError;
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
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            case kRPVControllerGraphStartTimestamp:
            {
                *value = 0;
                result = m_track ? m_track->GetDouble(kRPVControllerTrackMinTimestamp,
                                                      index, value)
                                 : kRocProfVisResultUnknownError;
                break;
            }
            case kRPVControllerGraphEndTimestamp:
            {
                *value = 0;
                result = m_track ? m_track->GetDouble(kRPVControllerTrackMaxTimestamp,
                                                      index, value)
                                 : kRocProfVisResultUnknownError;
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
                *value = (rocprofvis_handle_t*) m_track;
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
            m_type = (rocprofvis_controller_graph_type_t) value;
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
                    m_track = track_ref.Get();
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
