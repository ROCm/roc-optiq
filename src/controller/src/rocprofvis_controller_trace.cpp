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
#include "rocprofvis_controller_json_trace.h"
#include "rocprofvis_core_assert.h"

#include <cfloat>
#include <cstdint>
#include <cstring>

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
, m_dm_handle(nullptr)
{
}

Trace::~Trace()
{
    delete m_timeline;
    for (Track* track : m_tracks)
    {
        delete track;
    }
    if(m_dm_handle)
        rocprofvis_dm_delete_trace(m_dm_handle);
}

#ifdef JSON_SUPPORT
rocprofvis_result_t Trace::LoadJson(char const* const filename) {
    rocprofvis_result_t result = kRocProfVisResultUnknownError;
    m_timeline                 = new Timeline(0);
    if(m_timeline)
    {
        rocprofvis_controller_json_trace_data_t trace_object;
        std::future<bool>                       future =
            rocprofvis_controller_json_trace_async_load(filename, trace_object);
        if(future.valid())
        {
            future.wait();
            result =
                future.get() ? kRocProfVisResultSuccess : kRocProfVisResultUnknownError;

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
                        if(thread.second.m_events.size() ||
                           thread.second.m_counters.size())
                        {
                            auto   type  = thread.second.m_events.size()
                                               ? kRPVControllerTrackTypeEvents
                                               : kRPVControllerTrackTypeSamples;
                            Graph* graph = nullptr;
                            Track* track = new Track(type, track_id++, nullptr, this);
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
                                            min_ts = std::min(event.m_start_ts, min_ts);
                                            max_ts = std::max(event.m_start_ts +
                                                                  event.m_duration,
                                                              max_ts);
                                        }
                                        track->SetDouble(kRPVControllerTrackMinTimestamp,
                                                         0, min_ts);
                                        track->SetDouble(kRPVControllerTrackMaxTimestamp,
                                                         0, max_ts);

                                        uint64_t index = 0;
                                        for(auto& event : thread.second.m_events)
                                        {
                                            Event* new_event = new Event(
                                                event_id++, event.m_start_ts,
                                                event.m_start_ts + event.m_duration);
                                            if(new_event)
                                            {
                                                result = new_event->SetString(
                                                    kRPVControllerEventName, 0,
                                                    event.m_name.c_str(),
                                                    event.m_name.length());
                                                ROCPROFVIS_ASSERT(result ==
                                                       kRocProfVisResultSuccess);

                                                result = track->SetObject(
                                                    kRPVControllerTrackEntry, index++,
                                                    (rocprofvis_handle_t*) new_event);
                                                ROCPROFVIS_ASSERT(result ==
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
                                                    if(result == kRocProfVisResultSuccess)
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
                                            min_ts = std::min(min_ts, sample.m_start_ts);
                                            max_ts = std::max(max_ts, sample.m_start_ts);
                                        }
                                        track->SetDouble(kRPVControllerTrackMinTimestamp,
                                                         0, min_ts);
                                        track->SetDouble(kRPVControllerTrackMaxTimestamp,
                                                         0, max_ts);

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
                                                    (rocprofvis_handle_t*) new_sample);
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
                                                graph =
                                                    new Graph(kRPVControllerGraphTypeLine,
                                                              graph_id++);
                                                if(graph)
                                                {
                                                    result = graph->SetObject(
                                                        kRPVControllerGraphTrack, 0,
                                                        (rocprofvis_handle_t*) track);
                                                    if(result == kRocProfVisResultSuccess)
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
    }
    else
    {
        result = kRocProfVisResultMemoryAllocError;
    }
    if(result != kRocProfVisResultSuccess)
    {
        for(Track* track : m_tracks)
        {
            delete track;
        }
        m_tracks.clear();
        delete m_timeline;
    }
    return result;
}
#endif

rocprofvis_result_t Trace::LoadRocpd(char const* const filename) {
    rocprofvis_result_t result = kRocProfVisResultUnknownError;
    m_timeline                 = new Timeline(0);
    if(m_timeline)
    {
        m_dm_handle = rocprofvis_dm_create_trace();
        if(nullptr != m_dm_handle)
        {
            rocprofvis_dm_database_t db =
                rocprofvis_db_open_database(filename, kAutodetect);
            if(nullptr != db && kRocProfVisDmResultSuccess ==
                                    rocprofvis_dm_bind_trace_to_database(m_dm_handle, db))
            {
                rocprofvis_db_future_t object2wait = rocprofvis_db_future_alloc(nullptr);
                if(nullptr != object2wait)
                {
                    if(kRocProfVisDmResultSuccess ==
                       rocprofvis_db_read_metadata_async(db, object2wait))
                    {
                        if(kRocProfVisDmResultSuccess ==
                           rocprofvis_db_future_wait(object2wait, UINT64_MAX))
                        {
                            rocprofvis_dm_timestamp_t     start_time;
                            rocprofvis_dm_timestamp_t     end_time;
                            rocprofvis_db_num_of_tracks_t num_tracks;

                            start_time = rocprofvis_dm_get_property_as_uint64(
                                m_dm_handle, kRPVDMStartTimeUInt64, 0);
                            end_time = rocprofvis_dm_get_property_as_uint64(
                                m_dm_handle, kRPVDMEndTimeUInt64, 0);
                            num_tracks = (rocprofvis_db_num_of_tracks_t)
                                rocprofvis_dm_get_property_as_uint64(
                                    m_dm_handle, kRPVDMNumberOfTracksUInt64, 0);
                            std::vector<uint32_t> tracks_selection;

                            for(int i = 0; i < num_tracks; i++)
                            {
                                rocprofvis_dm_track_t dm_track_handle =
                                    rocprofvis_dm_get_property_as_handle(
                                        m_dm_handle, kRPVDMTrackHandleIndexed, i);
                                uint64_t track_id = rocprofvis_dm_get_property_as_uint64(
                                    dm_track_handle, kRPVDMTrackIdUInt64, 0);
                                uint64_t dm_track_type =
                                    rocprofvis_dm_get_property_as_uint64(
                                        dm_track_handle, kRPVDMTrackCategoryEnumUInt64,
                                        0);
                                if(dm_track_type == kRocProfVisDmRegionTrack ||
                                   dm_track_type == kRocProfVisDmKernelTrack ||
                                   dm_track_type == kRocProfVisDmPmcTrack)
                                {
                                    auto   type = (dm_track_type == kRocProfVisDmPmcTrack)
                                                      ? kRPVControllerTrackTypeSamples
                                                      : kRPVControllerTrackTypeEvents;
                                    Track* track =
                                        new Track(type, track_id, dm_track_handle, this);
                                    if(track)
                                    {
                                        tracks_selection.push_back((uint32_t) track_id);
                                        std::string dm_track_name =
                                            rocprofvis_dm_get_property_as_charptr(
                                                dm_track_handle,
                                                kRPVDMTrackCategoryEnumCharPtr, 0);
                                        dm_track_name += ":";
                                        dm_track_name +=
                                            rocprofvis_dm_get_property_as_charptr(
                                                dm_track_handle,
                                                kRPVDMTrackMainProcessNameCharPtr, 0);
                                        dm_track_name += ":";
                                        dm_track_name +=
                                            rocprofvis_dm_get_property_as_charptr(
                                                dm_track_handle,
                                                kRPVDMTrackSubProcessNameCharPtr, 0);
                                        track->SetString(kRPVControllerTrackName, 0,
                                                         dm_track_name.c_str(),
                                                         dm_track_name.length());

                                        uint32_t index = m_tracks.size();
                                        m_tracks.push_back(track);
                                        if(m_tracks.size() != (index + 1))
                                        {
                                            delete track;
                                            track  = nullptr;
                                            result = kRocProfVisResultMemoryAllocError;
                                        }
                                    }
                                }
                            }
                            if(tracks_selection.size() > 0)
                            {
                                if(kRocProfVisDmResultSuccess ==
                                   rocprofvis_db_read_trace_slice_async(
                                       db, start_time, end_time, tracks_selection.size(),
                                       &tracks_selection[0], object2wait))
                                {
                                    if(kRocProfVisDmResultSuccess ==
                                       rocprofvis_db_future_wait(object2wait, UINT64_MAX))
                                    {
                                        uint64_t graph_id = 0;
                                        for(Track* track : m_tracks)
                                        {
                                            uint64_t              sample_id = 0;
                                            rocprofvis_dm_slice_t slice =
                                                rocprofvis_dm_get_property_as_handle(
                                                    track->GetDmHandle(),
                                                    kRPVDMSliceHandleTimed, start_time);
                                            uint64_t dm_track_type =
                                                rocprofvis_dm_get_property_as_uint64(
                                                    track->GetDmHandle(),
                                                    kRPVDMTrackCategoryEnumUInt64, 0);
                                            uint64_t num_records =
                                                rocprofvis_dm_get_property_as_uint64(
                                                    slice, kRPVDMNumberOfRecordsUInt64,
                                                    0);
                                            if(num_records == 0) continue;
                                            track->SetUInt64(
                                                kRPVControllerTrackNumberOfEntries, 0,
                                                num_records);
                                            switch(dm_track_type)
                                            {
                                                case kRocProfVisDmRegionTrack:
                                                case kRocProfVisDmKernelTrack:
                                                {
                                                    uint64_t index  = 0;
                                                    double   min_ts = (double)
                                                        rocprofvis_dm_get_property_as_uint64(
                                                            slice,
                                                            kRPVDMTimestampUInt64Indexed,
                                                            0);
                                                    double max_ts =
                                                        (double)
                                                            rocprofvis_dm_get_property_as_uint64(
                                                                slice,
                                                                kRPVDMTimestampUInt64Indexed,
                                                                num_records - 1) +
                                                        (double)
                                                            rocprofvis_dm_get_property_as_int64(
                                                                slice,
                                                                kRPVDMEventDurationInt64Indexed,
                                                                num_records - 1);
                                                    track->SetDouble(
                                                        kRPVControllerTrackMinTimestamp,
                                                        0, min_ts);

                                                    for(int i = 0; i < num_records; i++)
                                                    {
                                                        double timestamp = (double)
                                                            rocprofvis_dm_get_property_as_uint64(
                                                                slice,
                                                                kRPVDMTimestampUInt64Indexed,
                                                                i);
                                                        double duration = (double)
                                                            rocprofvis_dm_get_property_as_int64(
                                                                slice,
                                                                kRPVDMEventDurationInt64Indexed,
                                                                i);
                                                        if(duration < 0) continue;

                                                        max_ts = std::max(max_ts, (timestamp + duration));

                                                        uint64_t event_id =
                                                            rocprofvis_dm_get_property_as_uint64(
                                                                slice,
                                                                kRPVDMEventIdUInt64Indexed,
                                                                i);
                                                        Event* new_event = new Event(
                                                            event_id, timestamp,
                                                            timestamp + duration);
                                                        if(new_event)
                                                        {
                                                            result = new_event->SetUInt64(
                                                                kRPVControllerEventLevel,
                                                                0,
                                                                rocprofvis_dm_get_property_as_uint64(
                                                                    slice,
                                                                    kRPVDMEventLevelUInt64Indexed,
                                                                    i));
                                                            result = new_event->SetString(
                                                                kRPVControllerEventCategory,
                                                                0,
                                                                rocprofvis_dm_get_property_as_charptr(
                                                                    slice,
                                                                    kRPVDMEventTypeStringCharPtrIndexed,
                                                                    i),
                                                                0);
                                                            ROCPROFVIS_ASSERT(
                                                                result ==
                                                                kRocProfVisResultSuccess);

                                                            result = new_event->SetString(
                                                                kRPVControllerEventName,
                                                                0,
                                                                rocprofvis_dm_get_property_as_charptr(
                                                                    slice,
                                                                    kRPVDMEventSymbolStringCharPtrIndexed,
                                                                    i),
                                                                0);
                                                            ROCPROFVIS_ASSERT(
                                                                result ==
                                                                kRocProfVisResultSuccess);

                                                            result = track->SetObject(
                                                                kRPVControllerTrackEntry,
                                                                index++,
                                                                (rocprofvis_handle_t*)
                                                                    new_event);
                                                            ROCPROFVIS_ASSERT(
                                                                result ==
                                                                kRocProfVisResultSuccess);
                                                        }
                                                        else
                                                        {
                                                            result =
                                                                kRocProfVisResultMemoryAllocError;
                                                            break;
                                                        }
                                                    }

                                                    track->SetDouble(
                                                        kRPVControllerTrackMaxTimestamp,
                                                        0, max_ts);
                                                    break;
                                                }
                                                case kRocProfVisDmPmcTrack:
                                                {
                                                    uint64_t index  = 0;
                                                    double   min_ts = (double)
                                                        rocprofvis_dm_get_property_as_uint64(
                                                            slice,
                                                            kRPVDMTimestampUInt64Indexed,
                                                            0);
                                                    double max_ts = (double)
                                                        rocprofvis_dm_get_property_as_uint64(
                                                            slice,
                                                            kRPVDMTimestampUInt64Indexed,
                                                            num_records - 1);
                                                    track->SetDouble(
                                                        kRPVControllerTrackMinTimestamp,
                                                        0, min_ts);
                                                    track->SetDouble(
                                                        kRPVControllerTrackMaxTimestamp,
                                                        0, max_ts);

                                                    for(int i = 0; i < num_records; i++)
                                                    {
                                                        double timestamp = (double)
                                                            rocprofvis_dm_get_property_as_uint64(
                                                                slice,
                                                                kRPVDMTimestampUInt64Indexed,
                                                                i);

                                                        Sample* new_sample = new Sample(
                                                            kRPVControllerPrimitiveTypeDouble,
                                                            sample_id++, timestamp);
                                                        if(new_sample)
                                                        {
                                                            new_sample->SetObject(
                                                                kRPVControllerSampleTrack,
                                                                0,
                                                                (rocprofvis_handle_t*)
                                                                    track);
                                                            new_sample->SetDouble(
                                                                kRPVControllerSampleValue,
                                                                0,
                                                                rocprofvis_dm_get_property_as_double(
                                                                    slice,
                                                                    kRPVDMPmcValueDoubleIndexed,
                                                                    i));
                                                            track->SetObject(
                                                                kRPVControllerTrackEntry,
                                                                index++,
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

                                                    break;
                                                }
                                                default:
                                                {
                                                    break;
                                                }
                                            }

                                            Graph* graph = new Graph(
                                                (dm_track_type == kRocProfVisDmPmcTrack)
                                                    ? kRPVControllerGraphTypeLine
                                                    : kRPVControllerGraphTypeFlame,
                                                graph_id++);
                                            if(graph)
                                            {
                                                result = graph->SetObject(
                                                    kRPVControllerGraphTrack, 0,
                                                    (rocprofvis_handle_t*) track);
                                                if(result == kRocProfVisResultSuccess)
                                                {
                                                    result = m_timeline->SetUInt64(
                                                        kRPVControllerTimelineNumGraphs,
                                                        0, graph_id);
                                                    if(result == kRocProfVisResultSuccess)
                                                    {
                                                        result = m_timeline->SetObject(
                                                            kRPVControllerTimelineGraphIndexed,
                                                            graph_id - 1,
                                                            (rocprofvis_handle_t*) graph);
                                                    }
                                                    if(result != kRocProfVisResultSuccess)
                                                    {
                                                        delete graph;
                                                    }
                                                }
                                            }
                                        }
                                    }
                                    else
                                    {
                                        result = kRocProfVisResultTimeout;
                                    }
                                }
                                else
                                {
                                    result = kRocProfVisResultUnknownError;
                                }
                                rocprofvis_dm_delete_all_time_slices(m_dm_handle);
                            }
                            else
                            {
                                result = kRocProfVisResultNotLoaded;
                            }
                        }
                        else
                        {
                            result = kRocProfVisResultTimeout;
                        }
                    }
                    else
                    {
                        result = kRocProfVisResultUnknownError;
                    }
                    rocprofvis_db_future_free(object2wait);
                }
                else
                {
                    result = kRocProfVisResultMemoryAllocError;
                }
            }
            else
            {
                result = kRocProfVisResultUnknownError;
            }
        }
        else
        {
            result = kRocProfVisResultMemoryAllocError;
        }
    }
    else
    {
        result = kRocProfVisResultMemoryAllocError;
    }
    return result;
}

rocprofvis_result_t Trace::Load(char const* const filename, RocProfVis::Controller::Future& future)
{
    assert (filename && strlen(filename));
        
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    std::string filepath = filename;
    future.Set(
        std::async(std::launch::async, [this, filepath]() -> rocprofvis_result_t
        {
            rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
#ifdef JSON_SUPPORT
            std::size_t index = filepath.find(".json", filepath.size() - 5);
            if(index != std::string::npos)
            {

                result = LoadJson(filepath.c_str());
            }
            else
#endif
            {
                result = LoadRocpd(filepath.c_str());
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

rocprofvis_result_t
Trace::AsyncFetch(Event& event, Future& future, Array& array,
                  rocprofvis_property_t property)
{
    rocprofvis_result_t error = kRocProfVisResultUnknownError;
    rocprofvis_dm_trace_t dm_handle = m_dm_handle;
    future.Set(std::async(std::launch::async,
                   [&event, &array, property, dm_handle]() -> rocprofvis_result_t {
                              rocprofvis_result_t result = kRocProfVisResultUnknownError;
                              result = event.Fetch(property, array, dm_handle);
                              return result;
                          }));

    if(future.IsValid())
    {
        error = kRocProfVisResultSuccess;
    }

    return error;
}

rocprofvis_controller_object_type_t Trace::GetType(void) 
{
    return kRPVControllerObjectTypeController;
}

rocprofvis_result_t Trace::AddLRUReference(Track* owner, Segment* reference)
{ 
    auto now          = std::chrono::system_clock::now();
    uint64_t ts = std::chrono::time_point_cast<std::chrono::nanoseconds>(now)
                            .time_since_epoch()
                            .count();
    auto it = m_lru_lookup.find(reference);
    if(it != m_lru_lookup.end())
    {
        owner = it->second->track;
        m_lru_array.erase(it->second);
    }
    size_t size = m_lru_array.size();
    auto pair = m_lru_array.insert({ts, owner, reference});
    if(pair.second)
    {
        m_lru_lookup[reference] = pair.first;
        return kRocProfVisResultSuccess;
    }

    return kRocProfVisResultMemoryAllocError;
}

rocprofvis_result_t Trace::ManageLRU(Track* requestor)
{ 
    std::lock_guard<std::mutex> lock(m_lru_mutex);
    uint64_t memory_used = 0;
    rocprofvis_result_t result =
        GetUInt64(kRPVControllerCommonMemoryUsageInclusive, 0, &memory_used);

    if (result == kRocProfVisResultSuccess)
    {
        while (memory_used > TRACE_MEMORY_USAGE_LIMIT)
        {
            size_t lru_size = m_lru_array.size();
            auto it = std::prev(m_lru_array.end());            
            result = it->track->DeleteSegment(requestor, it->reference);
            m_lru_array.erase(it);
            m_lru_lookup.erase(it->reference);
            if (result != kRocProfVisResultSuccess)
            {
                break;
            }
            result =
                GetUInt64(kRPVControllerCommonMemoryUsageInclusive, 0, &memory_used);
            if(result != kRocProfVisResultSuccess)
            {
                break;
            }
        }
    }
    return result;
}


rocprofvis_result_t Trace::GetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t* value) 
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if (value)
    {
        switch (property)
        {
            case kRPVControllerCommonMemoryUsageInclusive:
            {
                *value = sizeof(Trace);
                *value += m_tracks.size() * sizeof(Track*);
                result = kRocProfVisResultSuccess;
                for(auto& track : m_tracks)
                {
                    uint64_t entry_size = 0;
                    result = track->GetUInt64(property, 0, &entry_size);
                    if (result == kRocProfVisResultSuccess)
                    {
                        *value += entry_size;
                    }
                    else
                    {
                        break;
                    }
                }
                if (result == kRocProfVisResultSuccess)
                {
                    uint64_t timeline_size = 0;
                    result = m_timeline->GetUInt64(property, 0, &timeline_size);
                    *value += timeline_size;
                }
                break;
            }
            case kRPVControllerCommonMemoryUsageExclusive:
            {
                *value = sizeof(Trace);
                *value += m_tracks.size() * sizeof(Track*);
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerId:
            {
                *value = m_id;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerNumAnalysisView:
            {
                ROCPROFVIS_UNIMPLEMENTED;
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
                ROCPROFVIS_UNIMPLEMENTED;
                *value = nullptr;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerAnalysisViewIndexed:
            {
                ROCPROFVIS_UNIMPLEMENTED;
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
            ROCPROFVIS_UNIMPLEMENTED;
            break;
        }
        case kRPVControllerNumNodes:
        {
            ROCPROFVIS_UNIMPLEMENTED;
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
                ROCPROFVIS_UNIMPLEMENTED;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerAnalysisViewIndexed:
            {
                ROCPROFVIS_UNIMPLEMENTED;
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
