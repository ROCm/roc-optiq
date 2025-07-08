// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_controller_graph.h"
#include "rocprofvis_controller_array.h"
#include "rocprofvis_controller_event.h"
#include "rocprofvis_controller_reference.h"
#include "rocprofvis_controller_sample.h"
#include "rocprofvis_controller_sample_lod.h"
#include "rocprofvis_controller_track.h"
#include "rocprofvis_controller_trace.h"
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

        if(segments.GetSegments().find(segment_start) == segments.GetSegments().end())
        {
            uint64_t track_type = 0;
            result = m_track->GetUInt64(kRPVControllerTrackType, 0, &track_type);
            ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);

            double                   segment_end = segment_start + segment_duration;
            std::unique_ptr<Segment> segment     = std::make_unique<Segment>(
                (rocprofvis_controller_track_type_t) track_type, this);
            segment->SetStartEndTimestamps(segment_start, segment_end);
            segment->SetMinTimestamp(timestamp);
            if(object_type == kRPVControllerObjectTypeEvent)
            {
                double max_timestamp = timestamp;
                object->GetDouble(kRPVControllerEventEndTimestamp, 0, &max_timestamp);
                segment->SetMaxTimestamp(max_timestamp);
            }
            else
            {
                segment->SetMaxTimestamp(timestamp);
            }
            segments.Insert(segment_start, std::move(segment));
            result = (segments.GetSegments().find(segment_start) !=
                      segments.GetSegments().end())
                         ? kRocProfVisResultSuccess
                         : kRocProfVisResultMemoryAllocError;
        }

        if(result == kRocProfVisResultSuccess)
        {
            std::shared_ptr<Segment>& segment = segments.GetSegments()[segment_start];
            segment->SetMinTimestamp(std::min(segment->GetMinTimestamp(), timestamp));
            double max_timestamp = timestamp;
            if(object_type == kRPVControllerObjectTypeEvent)
            {
                object->GetDouble(kRPVControllerEventEndTimestamp, 0, &max_timestamp);
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
Graph::CombineEventNames(std::vector<Event*>& events, std::string & combined_name)
{
    rocprofvis_result_t  result = kRocProfVisResultUnknownError;
    std::unordered_map<std::string, int> string_counter;
    for(auto event : events)
    {
        std::string name;
        uint32_t    len = 0;
        result          = event->GetString(kRPVControllerEventName, 0, nullptr, &len);
        if(result == kRocProfVisResultSuccess)
        {
            name.resize(len);
            result = event->GetString(kRPVControllerEventName, 0,
                                      const_cast<char*>(name.c_str()), &len);
            if(result == kRocProfVisResultSuccess)
            {
                string_counter[name]++;
            }
        }
    }
    int max_count = 0;
    for(const auto& [key, count] : string_counter)
    {
        max_count = std::max(max_count, count);
    }
    int width = std::to_string(max_count).length();

    for(const auto& [name, count] : string_counter)
    {
        if(string_counter.size() > 1)
        {
            if(combined_name.size() > 0)
            {
                combined_name += "\n";
            }
            std::string count_string          = std::to_string(count);
            count_string += std::string(
                (width > count_string.size() ? width - count_string.size()
                                             : 0),' ');
            count_string += " of ";
            combined_name += count_string;
        }
        combined_name += name;
    }
    return result;
}

rocprofvis_result_t
Graph::GenerateLOD(uint32_t lod_to_generate, double start_ts, double end_ts,
                   std::vector<Data>& entries)
{
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
        std::map<uint64_t, std::vector<Event*>> event_stack;
        for(auto& data : entries)
        {
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

                event_stack[event_level].push_back(event);
            }
        }
        std::vector<Event*> events;
        for(auto& pair : event_stack)
        {
            double   min_ts = start_ts;
            double   max_ts = start_ts + scale;
            uint64_t level  = pair.first;

            double event_min = DBL_MAX;
            double event_max = DBL_MIN;

            std::vector<Event*>& events_at_level = pair.second;
            for(auto& event : events_at_level)
            {
                double   event_start = 0.0;
                double   event_end   = 0.0;
                uint64_t event_level = 0;

                result =
                    event->GetDouble(kRPVControllerEventStartTimestamp, 0, &event_start);
                result =
                    (result == kRocProfVisResultSuccess)
                        ? event->GetDouble(kRPVControllerEventEndTimestamp, 0, &event_end)
                        : result;
                result = (result == kRocProfVisResultSuccess)
                             ? event->GetUInt64(kRPVControllerEventLevel, 0, &event_level)
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
                            if(events.size())
                            {
                                std::string combined_name = "";
                                CombineEventNames(events, combined_name);

                                uint64_t event_id=0;
                                events[0]->GetUInt64(kRPVControllerEventId, 0, &event_id);
                                Event* event = m_ctx->GetMemoryManager()->NewEvent(event_id, event_min, event_max, this);
                                ROCPROFVIS_ASSERT(level != UINT64_MAX);
                                event->SetUInt64(kRPVControllerEventLevel, 0, level);
                                event->SetString(kRPVControllerEventName, 0,
                                                    combined_name.c_str(),
                                                    combined_name.size());
                                Insert(lod_to_generate, event_min, level, event);
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

            if(events.size())
            {
                std::string combined_name = "";
                CombineEventNames(events, combined_name);

                uint64_t event_id = 0;
                events[0]->GetUInt64(kRPVControllerEventId, 0, &event_id);
                Event* event = m_ctx->GetMemoryManager()->NewEvent(event_id, event_min, event_max, this);
                ROCPROFVIS_ASSERT(level != UINT64_MAX);
                event->SetUInt64(kRPVControllerEventLevel, 0, level);
                event->SetString(kRPVControllerEventName, 0, combined_name.c_str(),
                                    combined_name.size());
                Insert(lod_to_generate, event_min, level, event);

                events.clear();
            }
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
            rocprofvis_result_t  result = data.GetObject(&handle);
            ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
            SampleRef sampleRef((rocprofvis_controller_sample_t*) handle);
            if((result == kRocProfVisResultSuccess) && sampleRef.IsValid())
            {
                Sample* sample = sampleRef.Get();
                ROCPROFVIS_ASSERT(sample);

                double sample_start = 0.0;
                result =
                    sample->GetDouble(kRPVControllerSampleTimestamp, 0, &sample_start);
                ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
                if(result == kRocProfVisResultSuccess)
                {
                    if(sample_start >= min_ts && sample_start <= max_ts)
                    {
                        // Merge into the current sample
                        samples.push_back(sample);
                    }
                    else
                    {
                        // We assume that the events are ordered by time, so
                        // this must at least end after the current sample
                        if(sample_start > max_ts)
                        {

                            // Generate the stub event for any populated events.
                            uint64_t type = 0;
                            if((samples.size() > 0) &&
                               (sample->GetUInt64(kRPVControllerSampleType, 0, &type) ==
                                kRocProfVisResultSuccess))
                            {
                                SampleLOD* new_sample = m_ctx->GetMemoryManager()->NewSampleLOD(
                                    (rocprofvis_controller_primitive_type_t) type, 0,
                                    sample_start, samples, this);
                                Insert(lod_to_generate, sample_start, 0, new_sample);
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
        }

        if(samples.size())
        {
            uint64_t type         = 0;
            double   sample_start = 0.0;
            if((samples.front()->GetDouble(kRPVControllerSampleTimestamp, 0,
                                           &sample_start) == kRocProfVisResultSuccess) &&
               (samples.front()->GetUInt64(kRPVControllerSampleType, 0, &type) ==
                kRocProfVisResultSuccess))
            {
                SampleLOD* new_sample = m_ctx->GetMemoryManager()->NewSampleLOD(
                    (rocprofvis_controller_primitive_type_t) type, 0,
                                  sample_start, samples, this);
                Insert(lod_to_generate, sample_start, 0, new_sample);
            }
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
Graph::GenerateLOD(uint32_t lod_to_generate, double start, double end)
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
                uint32_t num_segments = ceil((max_ts - min_ts) / segment_duration);
                SegmentTimeline& segments = m_lods[lod_to_generate];
                segments.Init(min_ts,segment_duration, num_segments);
                segments.SetContext(m_ctx);
                it = m_lods.find(lod_to_generate);
            }

            start = std::max(start, min_ts);
            end   = std::min(end, max_ts);
            if (end > start)
            {
                std::vector<std::pair<uint32_t, uint32_t>> fetch_ranges;
                uint32_t start_index = (uint32_t)floor((start - min_ts) / segment_duration);
                uint32_t end_index = (uint32_t)ceil((end - min_ts) / segment_duration);
                for (uint32_t i = start_index; i < end_index; i++)
                {
                    if (!it->second.IsValid(i))
                    {
                        if (fetch_ranges.size())
                        {
                            auto& last_range = fetch_ranges.back();
                            if (last_range.second == i - 1)
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
                        m_ctx->GetMemoryManager()->EnterArrayOwnersip(&args.m_entries);
                        m_track->LockSegments(
                            fetch_start, fetch_end, &args,
                            [](double start, double end, Segment& segment, void* user_ptr,
                               SegmentTimeline* owner) -> rocprofvis_result_t {
                                rocprofvis_result_t    result = kRocProfVisResultSuccess;
                                FetchTrackSegmentArgs* args = (FetchTrackSegmentArgs*) user_ptr;
                                auto it = segment.GetLRUIterator();
                                if(it != args->m_lru_params.m_ctx->GetMemoryManager()
                                             ->GetDefaultLRUIterator())
                                {
                                    it->second->m_array_ptr = &args->m_entries;
                                }
                                return result;
                            });

                        result = m_track->FetchSegments(
                            fetch_start, fetch_end, &args,
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
                                                 args.m_entries);
                            if (result == kRocProfVisResultSuccess)
                            {
                                for (uint32_t i = range.first; i <= range.second; i++)
                                {
                                    it->second.SetValid(i);
                                }
                            }
                        }
                        ((Trace*)m_track->GetContext())->GetMemoryManager()->CancelArrayOwnersip(&args.m_entries);
                    }
                }
                else
                {
                    result = kRocProfVisResultSuccess;
                }
            }
        }
    }
    return result;
}

Graph::Graph(Handle* ctx, rocprofvis_controller_graph_type_t type, uint64_t id)
: m_id(id)
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
Graph::Fetch(uint32_t pixels, double start, double end, Array& array, uint64_t& index)
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

        result = GenerateLOD(lod, start, end);

        auto it = m_lods.find(lod);
        if((it != m_lods.end()) && (result == kRocProfVisResultSuccess))
        {
            GraphFetchLODArgs args;
            args.m_array = &array;
            args.m_index = &index;
            args.m_lru_params.m_ctx = m_ctx;
            args.m_lru_params.m_lod   = lod;
            m_ctx->GetMemoryManager()->EnterArrayOwnersip(&args.m_array->GetVector());
            array.SetContext(m_ctx);

            result = it->second.FetchSegments(
                start, end, &args,
                [](double start, double end, Segment& segment, void* user_ptr,
                   SegmentTimeline* owner) -> rocprofvis_result_t {
                    rocprofvis_result_t result = kRocProfVisResultSuccess;
                    GraphFetchLODArgs*  args   = (GraphFetchLODArgs*) user_ptr;
                    auto                it     = segment.GetLRUIterator();
                    if(it != args->m_lru_params.m_ctx->GetMemoryManager()
                                 ->GetDefaultLRUIterator())
                    {
                        it->second->m_array_ptr = &args->m_array->GetVector();
                    }
                    return result;
                });

            result = it->second.FetchSegments(
                start, end, &args,
                [](double start, double end, Segment& segment,
                   void* user_ptr, SegmentTimeline* owner) -> rocprofvis_result_t {
                    rocprofvis_result_t result = kRocProfVisResultSuccess;
                    GraphFetchLODArgs*  args   = (GraphFetchLODArgs*) user_ptr;
                    args->m_lru_params.m_owner = owner;
                    return segment.Fetch(start, end, args->m_array->GetVector(),
                                         *(args->m_index),nullptr, &args->m_lru_params);
                });
        }
    }
    ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess ||
                      result == kRocProfVisResultOutOfRange);
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
            case kRPVControllerGraphTrack:
            case kRPVControllerGraphStartTimestamp:
            case kRPVControllerGraphEndTimestamp:
            case kRPVControllerGraphEntryIndexed:
            {
                result = kRocProfVisResultInvalidType;
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
            case kRPVControllerGraphId:
            case kRPVControllerGraphType:
            case kRPVControllerGraphTrack:
            case kRPVControllerGraphNumEntries:
            case kRPVControllerGraphEntryIndexed:
            {
                result = kRocProfVisResultInvalidType;
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
rocprofvis_result_t
Graph::GetObject(rocprofvis_property_t property, uint64_t index,
                 rocprofvis_handle_t** value)
{
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
            case kRPVControllerGraphId:
            case kRPVControllerGraphType:
            case kRPVControllerGraphStartTimestamp:
            case kRPVControllerGraphEndTimestamp:
            case kRPVControllerGraphNumEntries:
            case kRPVControllerGraphEntryIndexed:
            {
                result = kRocProfVisResultInvalidType;
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
rocprofvis_result_t
Graph::GetString(rocprofvis_property_t property, uint64_t index, char* value,
                 uint32_t* length)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(length)
    {
        switch(property)
        {
            case kRPVControllerGraphId:
            case kRPVControllerGraphType:
            case kRPVControllerGraphTrack:
            case kRPVControllerGraphStartTimestamp:
            case kRPVControllerGraphEndTimestamp:
            case kRPVControllerGraphNumEntries:
            case kRPVControllerGraphEntryIndexed:
            {
                result = kRocProfVisResultInvalidType;
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

rocprofvis_result_t
Graph::SetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t value)
{
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
        case kRPVControllerGraphTrack:
        case kRPVControllerGraphStartTimestamp:
        case kRPVControllerGraphEndTimestamp:
        case kRPVControllerGraphEntryIndexed:
        {
            result = kRocProfVisResultInvalidType;
            break;
        }
        default:
        {
            result = kRocProfVisResultInvalidEnum;
            break;
        }
    }
    return result;
}
rocprofvis_result_t
Graph::SetDouble(rocprofvis_property_t property, uint64_t index, double value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerGraphStartTimestamp:
        case kRPVControllerGraphEndTimestamp:
        {
            result = kRocProfVisResultReadOnlyError;
            break;
        }
        case kRPVControllerGraphId:
        case kRPVControllerGraphType:
        case kRPVControllerGraphTrack:
        case kRPVControllerGraphNumEntries:
        case kRPVControllerGraphEntryIndexed:
        {
            result = kRocProfVisResultInvalidType;
            break;
        }
        default:
        {
            result = kRocProfVisResultInvalidEnum;
            break;
        }
    }
    return result;
}
rocprofvis_result_t
Graph::SetObject(rocprofvis_property_t property, uint64_t index,
                 rocprofvis_handle_t* value)
{
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
            case kRPVControllerGraphId:
            case kRPVControllerGraphType:
            case kRPVControllerGraphStartTimestamp:
            case kRPVControllerGraphEndTimestamp:
            case kRPVControllerGraphNumEntries:
            case kRPVControllerGraphEntryIndexed:
            {
                result = kRocProfVisResultInvalidType;
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
rocprofvis_result_t
Graph::SetString(rocprofvis_property_t property, uint64_t index, char const* value,
                 uint32_t length)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            case kRPVControllerGraphId:
            case kRPVControllerGraphType:
            case kRPVControllerGraphTrack:
            case kRPVControllerGraphStartTimestamp:
            case kRPVControllerGraphEndTimestamp:
            case kRPVControllerGraphNumEntries:
            case kRPVControllerGraphEntryIndexed:
            {
                result = kRocProfVisResultInvalidType;
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

}  // namespace Controller
}  // namespace RocProfVis
