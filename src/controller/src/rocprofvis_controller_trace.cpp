// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_controller_trace.h"
#include "rocprofvis_controller_timeline.h"
#include "rocprofvis_controller_track.h"
#include "rocprofvis_controller_reference.h"
#include "rocprofvis_controller_future.h"
#include "rocprofvis_controller_event.h"
#include "rocprofvis_controller_sample.h"
#include "rocprofvis_controller_graph.h"
#include "rocprofvis_controller_id.h"

#include "rocprofvis_trace.h"

#include <cassert>

namespace RocProfVis
{
namespace Controller
{

static IdGenerator<Trace> s_trace_id;

typedef Reference<rocprofvis_controller_track_t, Track, kRPVControllerObjectTypeTrack> TrackRef;
typedef Reference<rocprofvis_controller_timeline_t, Timeline, kRPVControllerObjectTypeTimeline> TimelineRef;

Trace::Trace()
: m_id(s_trace_id.GetNextId())
, m_timeline(nullptr)
{
}

Trace::~Trace()
{
    delete m_timeline;
    for (Track* track : m_tracks)
    {
        delete track;
    }
}

rocprofvis_result_t Trace::Load(char const* const filename, RocProfVis::Controller::Future& future)
{
    assert (filename && strlen(filename));
        
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    std::string filepath = filename;
    future.Set(
        std::async(std::launch::async, [this, filepath]() -> rocprofvis_result_t
        {
            rocprofvis_result_t result = kRocProfVisResultUnknownError;
            m_timeline = new Timeline(0);
            if(m_timeline)
            {
                rocprofvis_trace_data_t trace_object;
                std::future<bool> future = rocprofvis_trace_async_load_json_trace( filepath.c_str(), trace_object);
                if(future.valid())
                {
                    future.wait();
                    result = future.get() ? kRocProfVisResultSuccess
                                          : kRocProfVisResultUnknownError;

                    if(result == kRocProfVisResultSuccess)
                    {
                        uint64_t event_id  = 0;
                        uint64_t sample_id = 0;
                        uint64_t track_id  = 0;
                        uint64_t graph_id  = 0;
                        for(auto& pair : trace_object.m_trace_data)
                        {
                            std::string const& name = pair.first;
                            auto const&        data = pair.second;

                            for(auto& thread : data.m_threads)
                            {
                                auto   type  = thread.second.m_events.size()
                                                   ? kRPVControllerTrackTypeEvents
                                                   : kRPVControllerTrackTypeSamples;
                                Graph* graph = nullptr;
                                Track* track = new Track(type, track_id++);
                                if(track)
                                {
                                    track->SetString(kRPVControllerTrackName, 0,
                                                     thread.first.c_str(),
                                                     thread.first.length());
                                    switch(type)
                                    {
                                        case kRPVControllerTrackTypeEvents:
                                        {
                                            track->SetUInt64(
                                                kRPVControllerTrackNumberOfEntries, 0,
                                                thread.second.m_events.size());

                                            double min_ts = DBL_MAX;
                                            double max_ts = DBL_MIN;
                                            for(auto& event : thread.second.m_events)
                                            {
                                                min_ts =
                                                    std::min(event.m_start_ts, min_ts);
                                                max_ts = std::max(event.m_start_ts +
                                                                      event.m_duration,
                                                                  max_ts);
                                            }
                                            track->SetDouble(
                                                kRPVControllerTrackMinTimestamp, 0,
                                                min_ts);
                                            track->SetDouble(
                                                kRPVControllerTrackMaxTimestamp, 0,
                                                max_ts);

                                            uint64_t index = 0;
                                            for(auto& event : thread.second.m_events)
                                            {
                                                Event* new_event = new Event(
                                                    event_id++, event.m_start_ts,
                                                    event.m_start_ts + event.m_duration);
                                                if(new_event)
                                                {
                                                    result = new_event->SetObject(
                                                        kRPVControllerEventTrack, 0,
                                                        (rocprofvis_handle_t*) track);
                                                    assert(result ==
                                                           kRocProfVisResultSuccess);

                                                    result = new_event->SetString(
                                                        kRPVControllerEventName, 0,
                                                        event.m_name.c_str(),
                                                        event.m_name.length());
                                                    assert(result ==
                                                           kRocProfVisResultSuccess);

                                                    result = track->SetObject(
                                                        kRPVControllerTrackEntry, index++,
                                                        (rocprofvis_handle_t*) new_event);
                                                    assert(result ==
                                                           kRocProfVisResultSuccess);
                                                }
                                                else
                                                {
                                                    result =
                                                        kRocProfVisResultMemoryAllocError;
                                                    break;
                                                }
                                            }

                                            if(result == kRocProfVisResultSuccess)
                                            {
                                                uint32_t index = m_tracks.size();
                                                m_tracks.push_back(track);
                                                if(m_tracks.size() == (index + 1))
                                                {
                                                    graph = new Graph(
                                                        kRPVControllerGraphTypeFlame,
                                                        graph_id++);
                                                    if(graph)
                                                    {
                                                        result = graph->SetObject(
                                                            kRPVControllerGraphTrack, 0,
                                                            (rocprofvis_handle_t*) track);
                                                        if(result ==
                                                           kRocProfVisResultSuccess)
                                                        {
                                                            result = m_timeline->SetUInt64(
                                                                kRPVControllerTimelineNumGraphs,
                                                                0, index + 1);
                                                            if(result ==
                                                               kRocProfVisResultSuccess)
                                                            {
                                                                result = m_timeline->SetObject(
                                                                    kRPVControllerTimelineGraphIndexed,
                                                                    index,
                                                                    (rocprofvis_handle_t*)
                                                                        graph);
                                                            }
                                                            if(result !=
                                                               kRocProfVisResultSuccess)
                                                            {
                                                                delete graph;
                                                            }
                                                        }
                                                    }
                                                    else
                                                    {
                                                        result =
                                                            kRocProfVisResultMemoryAllocError;
                                                    }
                                                }
                                                else
                                                {
                                                    delete track;
                                                    track = nullptr;
                                                    result =
                                                        kRocProfVisResultMemoryAllocError;
                                                }
                                            }
                                            break;
                                        }
                                        case kRPVControllerTrackTypeSamples:
                                        {
                                            track->SetUInt64(
                                                kRPVControllerTrackNumberOfEntries, 0,
                                                thread.second.m_events.size());

                                            double min_ts = DBL_MAX;
                                            double max_ts = DBL_MIN;
                                            for(auto& sample : thread.second.m_counters)
                                            {
                                                min_ts =
                                                    std::min(min_ts, sample.m_start_ts);
                                                max_ts =
                                                    std::max(max_ts, sample.m_start_ts);
                                            }
                                            track->SetDouble(
                                                kRPVControllerTrackMinTimestamp, 0,
                                                min_ts);
                                            track->SetDouble(
                                                kRPVControllerTrackMaxTimestamp, 0,
                                                max_ts);

                                            uint64_t index = 0;
                                            for(auto& sample : thread.second.m_counters)
                                            {
                                                Sample* new_sample = new Sample(
                                                    kRPVControllerPrimitiveTypeDouble,
                                                    sample_id++, sample.m_start_ts);
                                                if(new_sample)
                                                {
                                                    new_sample->SetObject(
                                                        kRPVControllerSampleTrack, 0,
                                                        (rocprofvis_handle_t*) track);
                                                    new_sample->SetDouble(
                                                        kRPVControllerSampleValue, 0,
                                                        sample.m_value);
                                                    track->SetObject(
                                                        kRPVControllerTrackEntry, index++,
                                                        (rocprofvis_handle_t*)
                                                            new_sample);
                                                }
                                                else
                                                {
                                                    result =
                                                        kRocProfVisResultMemoryAllocError;
                                                    break;
                                                }
                                            }

                                            if(result == kRocProfVisResultSuccess)
                                            {
                                                uint32_t index = m_tracks.size();
                                                m_tracks.push_back(track);
                                                if(m_tracks.size() == (index + 1))
                                                {
                                                    graph = new Graph(
                                                        kRPVControllerGraphTypeLine,
                                                        graph_id++);
                                                    if(graph)
                                                    {
                                                        result = graph->SetObject(
                                                            kRPVControllerGraphTrack, 0,
                                                            (rocprofvis_handle_t*) track);
                                                        if(result ==
                                                           kRocProfVisResultSuccess)
                                                        {
                                                            result = m_timeline->SetUInt64(
                                                                kRPVControllerTimelineNumGraphs,
                                                                0, index + 1);
                                                            if(result ==
                                                               kRocProfVisResultSuccess)
                                                            {
                                                                result = m_timeline->SetObject(
                                                                    kRPVControllerTimelineGraphIndexed,
                                                                    index,
                                                                    (rocprofvis_handle_t*)
                                                                        graph);
                                                            }
                                                            if(result !=
                                                               kRocProfVisResultSuccess)
                                                            {
                                                                delete graph;
                                                            }
                                                        }
                                                    }
                                                    else
                                                    {
                                                        result =
                                                            kRocProfVisResultMemoryAllocError;
                                                    }
                                                }
                                                else
                                                {
                                                    delete track;
                                                    track = nullptr;
                                                    result =
                                                        kRocProfVisResultMemoryAllocError;
                                                }
                                            }
                                            break;
                                        }
                                        default:
                                        {
                                            break;
                                        }
                                    }
                                }
                                else
                                {
                                    result = kRocProfVisResultMemoryAllocError;
                                    break;
                                }
                            }
                        }
                    }
                }
            }
            else
            {
                result = kRocProfVisResultMemoryAllocError;
            }
            if(result != kRocProfVisResultSuccess)
            {
                for (Track* track : m_tracks)
                {
                    delete track;
                }
                m_tracks.clear();
                delete m_timeline;
            }
            return result;
        }));

    if(future.IsValid())
    {
        result = kRocProfVisResultSuccess;
    }

    return result;
}

rocprofvis_result_t Trace::AsyncFetch(Track& track, Future& future, Array& array,
                                double start, double end)
{
    rocprofvis_result_t error = kRocProfVisResultUnknownError;
    if(m_timeline)
    {
        error = m_timeline->AsyncFetch(track, future, array, start, end);
    }
    return error;
}

rocprofvis_result_t Trace::AsyncFetch(Graph& graph, Future& future, Array& array,
                                double start, double end, uint32_t pixels)
{
    rocprofvis_result_t error = kRocProfVisResultUnknownError;
    if(m_timeline)
    {
        error = m_timeline->AsyncFetch(graph, future, array, start, end, pixels);
    }
    return error;
}

rocprofvis_controller_object_type_t Trace::GetType(void) 
{
    return kRPVControllerObjectTypeController;
}

rocprofvis_result_t Trace::GetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t* value) 
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if (value)
    {
        switch (property)
        {
            case kRPVControllerId:
            {
                *value = m_id;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerNumAnalysisView:
            {
                assert(0);
                *value = 0;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerNumTracks:
            {
                *value = m_tracks.size();
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerNumNodes:
            case kRPVControllerNodeIndexed:
            case kRPVControllerTimeline:
            case kRPVControllerTrackIndexed:
            case kRPVControllerEventTable:
            case kRPVControllerAnalysisViewIndexed:
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
rocprofvis_result_t Trace::GetDouble(rocprofvis_property_t property, uint64_t index, double* value) 
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerId:
        case kRPVControllerNumAnalysisView:
        case kRPVControllerNumTracks:
        case kRPVControllerNumNodes:
        case kRPVControllerNodeIndexed:
        case kRPVControllerTimeline:
        case kRPVControllerTrackIndexed:
        case kRPVControllerEventTable:
        case kRPVControllerAnalysisViewIndexed:
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
rocprofvis_result_t Trace::GetObject(rocprofvis_property_t property, uint64_t index, rocprofvis_handle_t** value) 
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if (value)
    {
        switch (property)
        {
            case kRPVControllerTimeline:
            {
                *value = (rocprofvis_handle_t*)m_timeline;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerEventTable:
            {
                assert(0);
                *value = nullptr;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerAnalysisViewIndexed:
            {
                assert(0);
                *value = nullptr;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerTrackIndexed:
            {
                if(index < m_tracks.size())
                {
                    *value = (rocprofvis_handle_t*)m_tracks[index];
                    result = kRocProfVisResultSuccess;
                }
                else
                {
                    result = kRocProfVisResultOutOfRange;
                }
                break;
            }
            case kRPVControllerNumNodes:
            case kRPVControllerNodeIndexed:
            case kRPVControllerNumTracks:
            case kRPVControllerId:
            case kRPVControllerNumAnalysisView:
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
rocprofvis_result_t Trace::GetString(rocprofvis_property_t property, uint64_t index, char* value, uint32_t* length) 
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerId:
        case kRPVControllerNumAnalysisView:
        case kRPVControllerNumTracks:
        case kRPVControllerNumNodes:
        case kRPVControllerNodeIndexed:
        case kRPVControllerTimeline:
        case kRPVControllerTrackIndexed:
        case kRPVControllerEventTable:
        case kRPVControllerAnalysisViewIndexed:
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

rocprofvis_result_t Trace::SetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t value) 
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerId:
        {
            result = kRocProfVisResultReadOnlyError;
            break;
        }
        case kRPVControllerNumTracks:
        {
            if (m_tracks.size() != value)
            {
                for (uint32_t i = value; i < m_tracks.size(); i++)
                {
                    delete m_tracks[i];
                    m_tracks[i] = nullptr;
                }
                m_tracks.resize(value);
                result = m_tracks.size() == value ? kRocProfVisResultSuccess : kRocProfVisResultMemoryAllocError;
            }
            else
            {
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerNumAnalysisView:
        {
            assert(0);
            break;
        }
        case kRPVControllerNumNodes:
        {
            assert(0);
            break;
        }
        case kRPVControllerTimeline:
        case kRPVControllerEventTable:
        case kRPVControllerAnalysisViewIndexed:
        case kRPVControllerTrackIndexed:
        case kRPVControllerNodeIndexed:
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
rocprofvis_result_t Trace::SetDouble(rocprofvis_property_t property, uint64_t index, double value) 
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerId:
        case kRPVControllerNumAnalysisView:
        case kRPVControllerNumTracks:
        case kRPVControllerNumNodes:
        case kRPVControllerNodeIndexed:
        case kRPVControllerTimeline:
        case kRPVControllerTrackIndexed:
        case kRPVControllerEventTable:
        case kRPVControllerAnalysisViewIndexed:
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

rocprofvis_result_t Trace::SetObject(rocprofvis_property_t property, uint64_t index, rocprofvis_handle_t* value) 
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            case kRPVControllerTimeline:
            {
                TimelineRef timeline(value);
                if(timeline.IsValid())
                {
                    m_timeline = timeline.Get();
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVControllerEventTable:
            {
                assert(0);
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerAnalysisViewIndexed:
            {
                assert(0);
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerTrackIndexed:
            {
                TrackRef track(value);
                if(track.IsValid())
                {
                    if(index < m_tracks.size())
                    {
                        m_tracks[index] = track.Get();
                        result = kRocProfVisResultSuccess;
                    }
                    else
                    {
                        result = kRocProfVisResultOutOfRange;
                    }
                }
                break;
            }
            case kRPVControllerNumNodes:
            case kRPVControllerNodeIndexed:
            case kRPVControllerNumTracks:
            case kRPVControllerId:
            case kRPVControllerNumAnalysisView:
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
rocprofvis_result_t Trace::SetString(rocprofvis_property_t property, uint64_t index, char const* value, uint32_t length) 
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerId:
        case kRPVControllerNumAnalysisView:
        case kRPVControllerNumTracks:
        case kRPVControllerNumNodes:
        case kRPVControllerNodeIndexed:
        case kRPVControllerTimeline:
        case kRPVControllerTrackIndexed:
        case kRPVControllerEventTable:
        case kRPVControllerAnalysisViewIndexed:
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

}
}
