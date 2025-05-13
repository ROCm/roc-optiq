// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_controller_graph.h"
#include "rocprofvis_controller_track.h"
#include "rocprofvis_controller_event.h"
#include "rocprofvis_controller_sample.h"
#include "rocprofvis_controller_sample_lod.h"
#include "rocprofvis_controller_reference.h"
#include "rocprofvis_controller_array.h"
#include "rocprofvis_core.h"
#include "rocprofvis_core_assert.h"

namespace RocProfVis
{
namespace Controller
{

constexpr double kGraphScaleFactor = 10.0;

typedef Reference<rocprofvis_controller_track_t, Track, kRPVControllerObjectTypeTrack> TrackRef;
typedef Reference<rocprofvis_controller_event_t, Event, kRPVControllerObjectTypeEvent> EventRef;
typedef Reference<rocprofvis_controller_sample_t, Sample, kRPVControllerObjectTypeSample> SampleRef;

Graph::LOD::LOD()
: m_valid_range(std::make_pair(0, 0))
{
}

Graph::LOD::LOD(LOD&& other)
: m_segments(std::move(other.m_segments))
, m_valid_range(m_valid_range)
{
}

Graph::LOD::~LOD()
{
}

Graph::LOD& Graph::LOD::operator=(LOD&& other)
{
    if(this != &other)
    {
        m_segments = std::move(other.m_segments);
        m_valid_range = other.m_valid_range;
    }
    return *this;
}

SegmentTimeline& Graph::LOD::GetSegments()
{
    return m_segments;
}

std::pair<double, double> const& Graph::LOD::GetValidRange()
{
    return m_valid_range;
}


void Graph::LOD::SetValidRange(double start, double end)
{
    m_valid_range.first = start;
    m_valid_range.second = end;
}

void Graph::Insert(uint32_t lod, double timestamp, Handle* object)
{
    ROCPROFVIS_ASSERT(object);
    ROCPROFVIS_ASSERT(m_lods.find(lod) != m_lods.end());

    rocprofvis_result_t result = kRocProfVisResultOutOfRange;
    auto object_type = object->GetType();
    auto& segments = m_lods[lod].GetSegments();
    double start_timestamp = 0;
    double end_timestamp = 0;

    result = GetDouble(kRPVControllerGraphStartTimestamp, 0, &start_timestamp);
    result = (result == kRocProfVisResultSuccess) ? GetDouble(kRPVControllerGraphEndTimestamp, 0, &end_timestamp) : result;

    if(result == kRocProfVisResultSuccess && timestamp >= start_timestamp && timestamp <= end_timestamp)
    {
        double scale = 1.0;
        for(uint32_t i = 0; i < lod; i++)
        {
            scale *= 10.0;
        }

        double segment_duration = kSegmentDuration * scale;
        double relative         = (timestamp - start_timestamp);
        double num_segments     = floor(relative / segment_duration);
        double segment_start    = start_timestamp + (num_segments * segment_duration);

        if(segments.GetSegments().find(segment_start) == segments.GetSegments().end())
        {
            uint64_t track_type = 0;
            result = m_track->GetUInt64(kRPVControllerTrackType, 0, &track_type);
            ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);

            double                   segment_end = segment_start + segment_duration;
            std::unique_ptr<Segment> segment     = std::make_unique<Segment>((rocprofvis_controller_track_type_t)track_type);
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
            std::unique_ptr<Segment>& segment = segments.GetSegments()[segment_start];
            segment->SetMinTimestamp(std::min(segment->GetMinTimestamp(), timestamp));
            double max_timestamp = timestamp;
            if(object_type == kRPVControllerObjectTypeEvent)
            {
                object->GetDouble(kRPVControllerEventEndTimestamp, 0, &max_timestamp);
            }
            segment->SetMaxTimestamp(std::max(segment->GetMaxTimestamp(), max_timestamp));
            segment->Insert(timestamp, object);
        }
    }
    else
    {
        result = kRocProfVisResultOutOfRange;
    }
}

rocprofvis_result_t Graph::GenerateLOD(uint32_t lod_to_generate, double start_ts, double end_ts, std::vector<Data>& entries)
{
    rocprofvis_result_t result = kRocProfVisResultUnknownError; 
    double scale = 1.0;
    for(uint32_t i = 0; i < lod_to_generate; i++)
    {
        scale *= 10.0;
    }

    uint64_t track_type = 0;
    result = m_track->GetUInt64(kRPVControllerTrackType, 0, &track_type);
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
            
            std::vector<Event*>& events_at_level = pair.second;
            for(auto& event : events_at_level)
            {
                double event_start = 0.0;
                double event_end   = 0.0;
                uint64_t event_level = 0;

                result =
                    event->GetDouble(kRPVControllerEventStartTimestamp, 0, &event_start);
                result =
                    (result == kRocProfVisResultSuccess)
                        ? event->GetDouble(kRPVControllerEventEndTimestamp, 0, &event_end)
                        : result;
                result =
                    (result == kRocProfVisResultSuccess)
                        ? event->GetUInt64(kRPVControllerEventLevel, 0, &event_level)
                        : result;
                ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
                if(result == kRocProfVisResultSuccess)
                {
                    ROCPROFVIS_ASSERT(event_start >= min_ts);
                    ROCPROFVIS_ASSERT(level == event_level || level == UINT64_MAX);

                    if((event_start >= min_ts && event_start <= max_ts) &&
                       (event_end >= min_ts && event_end <= max_ts))
                    {
                        // Merge into current event
                        events.push_back(event);
                        level = level == UINT64_MAX ? event_level : level;
                    }
                    else
                    {
                        // Start a new event

                        // We assume that the events are ordered by time, so
                        // this must at least end after the current event or be a different level
                        ROCPROFVIS_ASSERT(event_end > max_ts || level != event_level);

                        double sample_start = event_start;

                        // Generate the stub event for any populated events.
                        if(events.size() &&
                           ((events.front()->GetDouble(kRPVControllerEventStartTimestamp,
                                                       0, &event_start) ==
                             kRocProfVisResultSuccess) &&
                            (events.back()->GetDouble(kRPVControllerEventEndTimestamp, 0,
                                                      &event_end) ==
                             kRocProfVisResultSuccess)))
                        {
                            std::string combined_name = "";
                            for(auto event : events)
                            {
                                std::string name;
                                uint32_t    len = 0;
                                result = event->GetString(kRPVControllerEventName, 0,
                                                               nullptr, &len);
                                if(result == kRocProfVisResultSuccess)
                                {
                                    name.resize(len);
                                    result = event->GetString(
                                        kRPVControllerEventName, 0,
                                        const_cast<char*>(name.c_str()), &len);
                                    if(result == kRocProfVisResultSuccess)
                                    {
                                        if(combined_name.size() > 0)
                                        {
                                            combined_name += "\n";
                                        }
                                        combined_name += name;
                                    }
                                }
                            }

                            Event* event = new Event(0, event_start, event_end);
                            ROCPROFVIS_ASSERT(level != UINT64_MAX);
                            event->SetUInt64(kRPVControllerEventLevel, 0, level);
                            event->SetString(kRPVControllerEventName, 0,
                                             combined_name.c_str(), combined_name.size());
                            Insert(lod_to_generate, event_start, event);
                        }

                        // Create a new event & increment the search
                        while(max_ts < sample_start)
                        {
                            min_ts = std::min(max_ts, end_ts);
                            max_ts = std::min(max_ts + scale, end_ts);
                        }

                        events.clear();
                        events.push_back(event);
                        level = event_level;
                    }
                }
            }

            if(events.size())
            {
                double event_start = 0.0;
                double event_end   = 0.0;
                if((events.front()->GetDouble(kRPVControllerEventStartTimestamp, 0,
                                              &event_start) == kRocProfVisResultSuccess) &&
                   (events.back()->GetDouble(kRPVControllerEventEndTimestamp, 0,
                                             &event_end) == kRocProfVisResultSuccess))
                {
                    std::string combined_name = "";
                    for (auto event : events)
                    {
                        std::string name;
                        uint32_t len = 0;
                        result = event->GetString(kRPVControllerEventName, 0, nullptr, &len);
                        if (result == kRocProfVisResultSuccess)
                        {
                            name.resize(len);
                            result = event->GetString(kRPVControllerEventName, 0,
                                                      const_cast<char*>(name.c_str()), &len);
                            if (result == kRocProfVisResultSuccess)
                            {
                                if(combined_name.size() > 0)
                                {
                                    combined_name += "\n";
                                }
                                combined_name += name;
                            }
                        }
                    }

                    Event* event = new Event(0, event_start, event_end);
                    ROCPROFVIS_ASSERT(level != UINT64_MAX);
                    event->SetUInt64(kRPVControllerEventLevel, 0, level);
                    event->SetString(kRPVControllerEventName, 0, combined_name.c_str(),
                                     combined_name.size());
                    Insert(lod_to_generate, event_start, event);
                }
            }
        }
    }
    else
    {
        double min_ts = start_ts;
        double max_ts = start_ts + scale;
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
                        ROCPROFVIS_ASSERT(sample_start > max_ts);

                        // Generate the stub event for any populated events.
                        uint64_t type = 0;
                        if((samples.size() > 0) &&
                           (sample->GetUInt64(kRPVControllerSampleType, 0, &type) ==
                            kRocProfVisResultSuccess))
                        {
                            SampleLOD* new_sample = new SampleLOD(
                                (rocprofvis_controller_primitive_type_t) type, 0,
                                sample_start, samples);
                            Insert(lod_to_generate, sample_start, new_sample);
                        }

                        // Create a new event & increment the search
                        while(max_ts < sample_start)
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

        if(samples.size())
        {
            uint64_t type         = 0;
            double   sample_start = 0.0;
            if((samples.front()->GetDouble(kRPVControllerSampleTimestamp, 0,
                                           &sample_start) == kRocProfVisResultSuccess) &&
               (samples.front()->GetUInt64(kRPVControllerSampleType, 0, &type) ==
                kRocProfVisResultSuccess))
            {
                SampleLOD* new_sample =
                    new SampleLOD((rocprofvis_controller_primitive_type_t) type, 0,
                                  sample_start, samples);
                Insert(lod_to_generate, sample_start, new_sample);
            }
        }
    }
    ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
    return result;
}

struct GraphLODArgs
{
    uint64_t m_index;
    std::pair<double, double> m_valid_range;
    std::vector<Data> m_entries;
};

rocprofvis_result_t Graph::GenerateLOD(uint32_t lod_to_generate, double start, double end)
{
    rocprofvis_result_t result = kRocProfVisResultOutOfRange;
    if (lod_to_generate > 0)
    {
        auto it = m_lods.find(lod_to_generate);
        if(it == m_lods.end())
        {
            m_lods[lod_to_generate] = LOD();
            it = m_lods.find(lod_to_generate);
        }

        auto range = it->second.GetValidRange();

        if(start < range.first || end > range.second)
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
                    scale *= 10.0;
                }
                double segment_duration = kSegmentDuration * scale;

                start = std::max(start, min_ts);
                end = std::min(end, max_ts);
                if(end > start)
                {
                    double fetch_start =
                        min_ts +
                        (floor((start - min_ts) / segment_duration) * segment_duration);
                    double fetch_end =
                        std::min(min_ts + (ceil((end - min_ts) / segment_duration) *
                                           segment_duration),
                                 max_ts);

                    GraphLODArgs args;
                    args.m_valid_range = range;
                    args.m_index       = 0;
                    result             = m_track->FetchSegments(
                        fetch_start, fetch_end, &args,
                        [](double start, double end, Segment& segment,
                           void* user_ptr) -> rocprofvis_result_t {
                            GraphLODArgs*       pair   = (GraphLODArgs*) user_ptr;
                            rocprofvis_result_t result = kRocProfVisResultSuccess;

                            if(pair->m_valid_range.first >= segment.GetStartTimestamp() ||
                               pair->m_valid_range.second <= segment.GetMaxTimestamp())
                            {
                                result = segment.Fetch(start, end, pair->m_entries,
                                                                   pair->m_index);
                                ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
                            }
                            return result;
                        });

                    if(result == kRocProfVisResultSuccess)
                    {
                        result = GenerateLOD(lod_to_generate, start, end, args.m_entries);
                        ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
                        it->second.SetValidRange(start, end);
                    }
                }
            }
        }
        else
        {
            result = kRocProfVisResultSuccess;
        }
    }
    return result;
}

Graph::Graph(rocprofvis_controller_graph_type_t type, uint64_t id)
: m_id(id)
, m_track(nullptr)
, m_type(type)
{
}

Graph::~Graph()
{
}

struct GraphFetchLODArgs
{
    Array* m_array;
    uint64_t* m_index;
};

rocprofvis_result_t Graph::Fetch(uint32_t pixels, double start, double end, Array& array, uint64_t& index)
{
    rocprofvis_result_t result = kRocProfVisResultUnknownError;
    // Zero out the array - we don't know how many entries we will add and we don't want to report a non-zero value to the caller.
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

            result = it->second.GetSegments().FetchSegments(start, end, &args, [](double start, double end, Segment& segment, void* user_ptr) -> rocprofvis_result_t{
                rocprofvis_result_t result = kRocProfVisResultSuccess;
                GraphFetchLODArgs* args = (GraphFetchLODArgs*)user_ptr;
                return segment.Fetch(start, end, args->m_array->GetVector(), *(args->m_index));
            });
        }
    }
    ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess || result == kRocProfVisResultOutOfRange);
    return result;
}

rocprofvis_controller_object_type_t Graph::GetType(void)
{
    return kRPVControllerObjectTypeGraph;
}

rocprofvis_result_t Graph::GetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t* value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if (value)
    {
        switch (property)
        {
            case kRPVControllerCommonMemoryUsageInclusive:
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
                result = m_track ? m_track->GetUInt64(kRPVControllerTrackNumberOfEntries, index, value) : kRocProfVisResultUnknownError;
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
rocprofvis_result_t Graph::GetDouble(rocprofvis_property_t property, uint64_t index, double* value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            case kRPVControllerGraphStartTimestamp:
            {
                *value = 0;
                result = m_track ? m_track->GetDouble(kRPVControllerTrackMinTimestamp, index, value) : kRocProfVisResultUnknownError;
                break;
            }
            case kRPVControllerGraphEndTimestamp:
            {
                *value = 0;
                result = m_track ? m_track->GetDouble(kRPVControllerTrackMaxTimestamp, index, value) : kRocProfVisResultUnknownError;
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
rocprofvis_result_t Graph::GetObject(rocprofvis_property_t property, uint64_t index, rocprofvis_handle_t** value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            case kRPVControllerGraphTrack:
            {
                *value = (rocprofvis_handle_t*)m_track;
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
rocprofvis_result_t Graph::GetString(rocprofvis_property_t property, uint64_t index, char* value, uint32_t* length)
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

rocprofvis_result_t Graph::SetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerGraphType:
        {
            m_type = (rocprofvis_controller_graph_type_t)value;
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
rocprofvis_result_t Graph::SetDouble(rocprofvis_property_t property, uint64_t index, double value)
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
rocprofvis_result_t Graph::SetObject(rocprofvis_property_t property, uint64_t index, rocprofvis_handle_t* value)
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
rocprofvis_result_t Graph::SetString(rocprofvis_property_t property, uint64_t index, char const* value, uint32_t length)
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

}
}
