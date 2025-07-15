// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_controller_trace.h"
#include "rocprofvis_controller_arguments.h"
#include "rocprofvis_controller_array.h"
#include "rocprofvis_controller_event.h"
#include "rocprofvis_controller_ext_data.h"
#include "rocprofvis_controller_future.h"
#include "rocprofvis_controller_graph.h"
#include "rocprofvis_controller_id.h"
#include "rocprofvis_controller_json_trace.h"
#include "rocprofvis_controller_arguments.h"
#include "rocprofvis_controller_queue.h"
#include "rocprofvis_controller_stream.h"
#include "rocprofvis_controller_thread.h"
#include "rocprofvis_controller_node.h"
#include "rocprofvis_controller_process.h"
#include "rocprofvis_controller_processor.h"
#include "rocprofvis_controller_reference.h"
#include "rocprofvis_controller_sample.h"
#include "rocprofvis_controller_table_system.h"
#include "rocprofvis_controller_timeline.h"
#include "rocprofvis_controller_track.h"
#include "rocprofvis_core.h"
#include "rocprofvis_core_assert.h"
#ifdef COMPUTE_UI_SUPPORT
#include "rocprofvis_controller_trace_compute.h"
#include "rocprofvis_controller_plot.h"
#endif

#include <cfloat>
#include <cstdint>
#include <cstring>
#include <set>
 
namespace RocProfVis
{
namespace Controller
{

static IdGenerator<Trace> s_trace_id;

typedef Reference<rocprofvis_controller_table_t, SystemTable, kRPVControllerObjectTypeTable> SystemTableRef;
typedef Reference<rocprofvis_controller_track_t, Track, kRPVControllerObjectTypeTrack> TrackRef;
typedef Reference<rocprofvis_controller_timeline_t, Timeline, kRPVControllerObjectTypeTimeline> TimelineRef;
typedef Reference<rocprofvis_controller_processor_t, Processor, kRPVControllerObjectTypeProcessor> ProcessorRef;
typedef Reference<rocprofvis_controller_process_t, Process, kRPVControllerObjectTypeProcess> ProcessRef;
typedef Reference<rocprofvis_controller_thread_t, Thread, kRPVControllerObjectTypeThread> ThreadRef;
typedef Reference<rocprofvis_controller_queue_t, Queue, kRPVControllerObjectTypeQueue> QueueRef;
typedef Reference<rocprofvis_controller_stream_t, Stream, kRPVControllerObjectTypeStream> StreamRef;
typedef Reference<rocprofvis_controller_node_t, Node, kRPVControllerObjectTypeNode> NodeRef;

Trace::Trace()
: m_id(s_trace_id.GetNextId())
, m_timeline(nullptr)
, m_event_table(nullptr)
, m_sample_table(nullptr)
, m_dm_handle(nullptr)
#ifdef COMPUTE_UI_SUPPORT
, m_compute_trace(nullptr)
#endif
{
    m_event_table = new SystemTable(0);
    ROCPROFVIS_ASSERT(m_event_table);

    m_sample_table = new SystemTable(1);
    ROCPROFVIS_ASSERT(m_sample_table);

    m_mem_mgmt = new MemoryManager(m_id);
}

Trace::~Trace()
{
    delete m_mem_mgmt; 
    m_mem_mgmt = nullptr;
    delete m_timeline;
    delete m_event_table;
    delete m_sample_table;
    for (Track* track : m_tracks)
    {
        delete track;
    }
    if(m_dm_handle)
        rocprofvis_dm_delete_trace(m_dm_handle);
#ifdef COMPUTE_UI_SUPPORT
    if(m_compute_trace)
        delete m_compute_trace;
#endif
    for (auto* node : m_nodes)
    {
        delete node;
    }
}

MemoryManager*
Trace::GetMemoryManager(){
    return m_mem_mgmt;
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
                                            Event* new_event = GetMemoryManager()->NewEvent(
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
                                                graph = new Graph(this,
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
                                            Sample* new_sample =
                                                GetMemoryManager()->NewSample(
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
                                                    new Graph(this, kRPVControllerGraphTypeLine,
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
    size_t trace_size          = 0;
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
                    std::map<uint64_t, Track*> queue_to_track;
                    std::map<uint64_t, Track*> thread_to_track;
                    
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

                            uint64_t  graph_index = 0;
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

                                        uint64_t num_records =
                                            rocprofvis_dm_get_property_as_uint64(
                                                track->GetDmHandle(),
                                                kRPVDMTrackNumRecordsUInt64, 0);

                                        trace_size +=
                                            num_records *
                                            (type == kRPVControllerTrackTypeEvents
                                                 ? sizeof(Event)
                                                 : sizeof(Sample));

                                        track->SetUInt64(
                                            kRPVControllerTrackNumberOfEntries, 0,
                                            num_records);
                                        double min_ts =
                                            rocprofvis_dm_get_property_as_uint64(
                                                track->GetDmHandle(),
                                                kRPVDMTrackMinimumTimestampUInt64, 0);
                                        double max_ts =
                                            rocprofvis_dm_get_property_as_uint64(
                                                track->GetDmHandle(),
                                                kRPVDMTrackMaximumTimestampUInt64, 0);
                                        track->SetDouble(kRPVControllerTrackMinTimestamp,
                                                         0, min_ts);
                                        track->SetDouble(kRPVControllerTrackMaxTimestamp,
                                                         0, max_ts);

                                        uint64_t num_ext_data = 0;
                                        track->GetUInt64(
                                            kRPVControllerTrackExtDataNumberOfEntries, 0,
                                            &num_ext_data);
                                        for (uint32_t idx = 0; idx < num_ext_data; idx++)
                                        {
                                            std::string category;
                                            std::string name;
                                            std::string value;
                                            uint32_t    length = 0;
                                            track->GetString(
                                                kRPVControllerTrackExtDataCategoryIndexed,
                                                idx, nullptr, &length);
                                            category.resize(length);
                                            track->GetString(
                                                kRPVControllerTrackExtDataCategoryIndexed,
                                                idx, category.data(), &length);

                                            length = 0;
                                            track->GetString(
                                                kRPVControllerTrackExtDataNameIndexed,
                                                idx, nullptr, &length);
                                            name.resize(length);
                                            track->GetString(
                                                kRPVControllerTrackExtDataNameIndexed,
                                                idx, name.data(), &length);

                                            length = 0;
                                            track->GetString(
                                                kRPVControllerTrackExtDataValueIndexed,
                                                idx, nullptr, &length);
                                            value.resize(length);
                                            track->GetString(
                                                kRPVControllerTrackExtDataValueIndexed,
                                                idx, value.data(), &length);

                                            if (category == "Queue" && name == "id")
                                            {
                                                char*    end = nullptr;
                                                uint64_t val = std::strtoull(
                                                    value.c_str(), &end, 10);
                                                queue_to_track[val] = track;
                                            }
                                            else if(category == "Thread" && name == "id")
                                            {
                                                char*    end = nullptr;
                                                uint64_t val = std::strtoull(
                                                    value.c_str(), &end, 10);
                                                thread_to_track[val] = track;
                                            }

                                            spdlog::info("{} {} {}", category.c_str(),
                                                         name.c_str(), value.c_str());
                                        }

                                        uint32_t index = m_tracks.size();
                                        m_tracks.push_back(track);
                                        if(m_tracks.size() != (index + 1))
                                        {
                                            delete track;
                                            track  = nullptr;
                                            result = kRocProfVisResultMemoryAllocError;
                                            break;
                                        }

                                        Graph* graph = new Graph(this,
                                            (dm_track_type == kRocProfVisDmPmcTrack)
                                                ? kRPVControllerGraphTypeLine
                                                : kRPVControllerGraphTypeFlame,
                                            track_id);
                                        if(graph)
                                        {
                                            result = graph->SetObject(
                                                kRPVControllerGraphTrack, 0,
                                                (rocprofvis_handle_t*) track);
                                            if(result == kRocProfVisResultSuccess)
                                            {
                                                result = m_timeline->SetUInt64(
                                                    kRPVControllerTimelineNumGraphs, 0,
                                                    ++graph_index);
                                                if(result == kRocProfVisResultSuccess)
                                                {
                                                    result = m_timeline->SetObject(
                                                        kRPVControllerTimelineGraphIndexed,
                                                        graph_index - 1,
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
                            }

                            GetMemoryManager()->Init(trace_size);

                            // This block is asynchronously loading full trace
                            // todo : remove following block after  UI implemented segmented loading
                            // or : use this code for preloading some segments at the load time. start and end has to be calculated considering preloaded segment boundaries  
 /*                           
                            std::vector<RocProfVis::Controller::Array*> arrays;
                            std::vector<RocProfVis::Controller::Future*> futures;
                            arrays.resize(num_tracks);
                            futures.resize(num_tracks);
                            for(int i = 0; i < num_tracks; i++)
                            {                               
                                arrays[i] = (RocProfVis::Controller::Array*)
                                    rocprofvis_controller_array_alloc(32);
                                futures[i] = (RocProfVis::Controller::Future*)
                                    rocprofvis_controller_future_alloc();
                                double start, end;
                                m_tracks[i]->GetDouble(kRPVControllerTrackMinTimestamp, 0, &start);
                                m_tracks[i]->GetDouble(kRPVControllerTrackMaxTimestamp, 0, &end);
                                result = AsyncFetch(*m_tracks[i], *futures[i], *arrays[i],
                                                    start, end);
                            }

                            for (int i = 0; i < num_tracks; i++)
                            {
                                result = rocprofvis_controller_future_wait(
                                    (rocprofvis_controller_future_t*) futures[i], FLT_MAX);
                                rocprofvis_controller_future_free(
                                    (rocprofvis_controller_future_t*) futures[i]);
                                rocprofvis_controller_array_free(
                                    (rocprofvis_controller_array_t*) arrays[i]);
                            }
                            */
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

                    std::map<uint64_t, Node*> nodes;
                    if(result == kRocProfVisDmResultSuccess)
                    {
                        rocprofvis_db_future_t future =
                            rocprofvis_db_future_alloc(nullptr);
                        if (future)
                        {
                            std::string node_query = "select * from rocpd_info_node";
                            rocprofvis_dm_table_id_t table_id = 0;
                            auto dm_result = rocprofvis_db_execute_query_async(
                                db, node_query.c_str(), "Fetch node info",
                                    future, &table_id);
                            if(dm_result == kRocProfVisDmResultSuccess)
                            {
                                dm_result = rocprofvis_db_future_wait(future,
                                                                        UINT64_MAX);
                            }

                            if (dm_result == kRocProfVisDmResultSuccess)
                            {
                                uint64_t num_tables =
                                    rocprofvis_dm_get_property_as_uint64(
                                        m_dm_handle, kRPVDMNumberOfTablesUInt64, 0);
                                if(num_tables > 0)
                                {
                                    rocprofvis_dm_table_t table =
                                        rocprofvis_dm_get_property_as_handle(
                                            m_dm_handle, kRPVDMTableHandleByID,
                                            table_id);
                                    if(nullptr != table)
                                    {
                                        char* table_query =
                                            rocprofvis_dm_get_property_as_charptr(
                                                table, kRPVDMExtTableQueryCharPtr,
                                                0);
                                        uint64_t num_columns =
                                            rocprofvis_dm_get_property_as_uint64(
                                                table,
                                                kRPVDMNumberOfTableColumnsUInt64,
                                                0);
                                        uint64_t num_rows =
                                            rocprofvis_dm_get_property_as_uint64(
                                                table,
                                                kRPVDMNumberOfTableRowsUInt64, 0);
                                        if(strcmp(table_query,
                                                    node_query.c_str()) == 0)
                                        {
                                            std::vector<rocprofvis_controller_node_properties_t> columns;
                                            for(int i = 0; i < num_columns; i++)
                                            {
                                                char const* column_name = rocprofvis_dm_get_property_as_charptr(
                                                        table,
                                                        kRPVDMExtTableColumnNameCharPtrIndexed,
                                                        i);

                                                if (strcmp(column_name, "id") == 0)
                                                {
                                                    columns.push_back(kRPVControllerNodeId);
                                                }
                                                else if(strcmp(column_name, "hostname") ==
                                                        0)
                                                {
                                                    columns.push_back(kRPVControllerNodeHostName);
                                                }
                                                else if(strcmp(column_name,
                                                               "domain_name") == 0)
                                                {
                                                    columns.push_back(kRPVControllerNodeDomainName);
                                                }
                                                else if(strcmp(column_name,
                                                               "system_name") == 0)
                                                {
                                                    columns.push_back(kRPVControllerNodeOSName);
                                                }
                                                else if(strcmp(column_name, "release") ==
                                                        0)
                                                {
                                                    columns.push_back(kRPVControllerNodeOSRelease);
                                                }
                                                else if(strcmp(column_name, "version") ==
                                                        0)
                                                {
                                                    columns.push_back(kRPVControllerNodeOSVersion);
                                                }
                                                else if(strcmp(column_name,
                                                               "hardware_name") == 0)
                                                {
                                                    columns.push_back(kRPVControllerNodeHardwareName);
                                                }
                                                else if (strcmp(column_name, "machine_id") == 0)
                                                {
                                                    columns.push_back(kRPVControllerNodeMachineId);
                                                }
                                                else if (strcmp(column_name, "guid") == 0)
                                                {
                                                    columns.push_back(kRPVControllerNodeMachineGuid);
                                                }
                                                else if (strcmp(column_name, "hash") == 0)
                                                {
                                                    columns.push_back(kRPVControllerNodeHash);
                                                }
                                                else
                                                {
                                                    columns.push_back((rocprofvis_controller_node_properties_t)0);
                                                }
                                            }
                                            for (int i = 0; i < num_rows; i++)
                                            {
                                                rocprofvis_dm_table_row_t table_row =
                                                    rocprofvis_dm_get_property_as_handle(
                                                        table,
                                                        kRPVDMExtTableRowHandleIndexed,
                                                        i);
                                                if(table_row != nullptr)
                                                {
                                                    uint64_t num_cells =
                                                        rocprofvis_dm_get_property_as_uint64(
                                                            table_row,
                                                            kRPVDMNumberOfTableRowCellsUInt64,
                                                            0);
                                                    if(num_cells == num_columns)
                                                    {
                                                        Node* node = new Node;

                                                        std::vector<std::string>
                                                            row;
                                                        for(int j = 0;
                                                            j < num_cells; j++)
                                                        {
                                                            char const* prop_string = rocprofvis_dm_get_property_as_charptr(
                                                                    table_row,
                                                                    kRPVDMExtTableRowCellValueCharPtrIndexed,
                                                                    j);
                                                            if (columns[j] == (rocprofvis_controller_node_properties_t)0)
                                                            {
                                                                continue;
                                                            }
                                                            else if (columns[j] == kRPVControllerNodeId || columns[j] == kRPVControllerNodeHash)
                                                            {
                                                                char* end = nullptr;
                                                                uint64_t val = std::strtoull(prop_string, &end, 10);
                                                                node->SetUInt64(
                                                                    columns[j],
                                                                    0, val);
                                                                if (columns[j] ==
                                                                    kRPVControllerNodeId)
                                                                {
                                                                    nodes[val] = node;
                                                                }
                                                            }
                                                            else
                                                            {
                                                                node->SetString(
                                                                    columns[j], 0, prop_string, strlen(prop_string));

                                                            }
                                                        }

                                                        m_nodes.push_back(node);
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }

                                rocprofvis_dm_delete_table_at(m_dm_handle, table_id);
                            }
                            rocprofvis_db_future_free(future);
                        }
                    }

                    std::map<uint64_t, Processor*> processors;
                    if (result == kRocProfVisDmResultSuccess)
                    {
                        rocprofvis_db_future_t future =
                            rocprofvis_db_future_alloc(nullptr);
                        if(future)
                        {
                            std::string processor_query = "select * from rocpd_info_agent";
                            rocprofvis_dm_table_id_t table_id = 0;
                            auto dm_result = rocprofvis_db_execute_query_async(
                                db, processor_query.c_str(), "Fetch processor info",
                                future,
                                &table_id);
                            if(dm_result == kRocProfVisDmResultSuccess)
                            {
                                dm_result = rocprofvis_db_future_wait(future, UINT64_MAX);
                            }

                            if(dm_result == kRocProfVisDmResultSuccess)
                            {
                                uint64_t num_tables =
                                    rocprofvis_dm_get_property_as_uint64(
                                        m_dm_handle, kRPVDMNumberOfTablesUInt64, 0);
                                if(num_tables > 0)
                                {
                                    rocprofvis_dm_table_t table =
                                        rocprofvis_dm_get_property_as_handle(
                                            m_dm_handle, kRPVDMTableHandleByID, table_id);
                                    if(nullptr != table)
                                    {
                                        char* table_query =
                                            rocprofvis_dm_get_property_as_charptr(
                                                table, kRPVDMExtTableQueryCharPtr, 0);
                                        uint64_t num_columns =
                                            rocprofvis_dm_get_property_as_uint64(
                                                table, kRPVDMNumberOfTableColumnsUInt64,
                                                0);
                                        uint64_t num_rows =
                                            rocprofvis_dm_get_property_as_uint64(
                                                table, kRPVDMNumberOfTableRowsUInt64, 0);
                                        if(strcmp(table_query, processor_query.c_str()) == 0)
                                        {
                                            std::vector<
                                                rocprofvis_controller_processor_properties_t>
                                                columns;
                                            for(int i = 0; i < num_columns; i++)
                                            {
                                                char const* column_name =
                                                    rocprofvis_dm_get_property_as_charptr(
                                                        table,
                                                        kRPVDMExtTableColumnNameCharPtrIndexed,
                                                        i);

                                                if(strcmp(column_name, "id") == 0)
                                                {
                                                    columns.push_back(
                                                        kRPVControllerProcessorId);
                                                }
                                                else if(strcmp(column_name, "nid") ==
                                                        0)
                                                {
                                                    columns.push_back(
                                                        kRPVControllerProcessorNodeId);
                                                }
                                                else if(strcmp(column_name,
                                                               "type") == 0)
                                                {
                                                    columns.push_back(
                                                        kRPVControllerProcessorType);
                                                }
                                                else if(strcmp(column_name,
                                                               "type_index") == 0)
                                                {
                                                    columns.push_back(
                                                        kRPVControllerProcessorTypeIndex);
                                                }
                                                else if(strcmp(column_name,
                                                               "absolute_index") == 0)
                                                {
                                                    columns.push_back(
                                                        kRPVControllerProcessorIndex);
                                                }
                                                else if(strcmp(column_name,
                                                               "logical_index") == 0)
                                                {
                                                    columns.push_back(
                                                        kRPVControllerProcessorLogicalIndex);
                                                }
                                                else if(strcmp(column_name, "uuid") ==
                                                        0)
                                                {
                                                    columns.push_back(
                                                        kRPVControllerProcessorUUID);
                                                }
                                                else if(strcmp(column_name, "name") ==
                                                        0)
                                                {
                                                    columns.push_back(
                                                        kRPVControllerProcessorName);
                                                }
                                                else if(strcmp(column_name,
                                                               "model_name") == 0)
                                                {
                                                    columns.push_back(
                                                        kRPVControllerProcessorModelName);
                                                }
                                                else if(strcmp(column_name,
                                                               "vendor_name") == 0)
                                                {
                                                    columns.push_back(
                                                        kRPVControllerProcessorVendorName);
                                                }
                                                else if(strcmp(column_name,
                                                               "product_name") == 0)
                                                {
                                                    columns.push_back(
                                                        kRPVControllerProcessorProductName);
                                                }
                                                else if(strcmp(column_name,
                                                               "user_name") == 0)
                                                {
                                                    columns.push_back(
                                                        kRPVControllerProcessorUserName);
                                                }
                                                else if(strcmp(column_name,
                                                               "extdata") == 0)
                                                {
                                                    columns.push_back(
                                                        kRPVControllerProcessorExtData);
                                                }
                                                else
                                                {
                                                    columns.push_back((rocprofvis_controller_processor_properties_t)0);
                                                }
                                            }
                                            for(int i = 0; i < num_rows; i++)
                                            {
                                                rocprofvis_dm_table_row_t table_row =
                                                    rocprofvis_dm_get_property_as_handle(
                                                        table,
                                                        kRPVDMExtTableRowHandleIndexed,
                                                        i);
                                                if(table_row != nullptr)
                                                {
                                                    uint64_t num_cells =
                                                        rocprofvis_dm_get_property_as_uint64(
                                                            table_row,
                                                            kRPVDMNumberOfTableRowCellsUInt64,
                                                            0);
                                                    if(num_cells == num_columns)
                                                    {
                                                        Processor* proc = new Processor;

                                                        std::vector<std::string> row;
                                                        for(int j = 0; j < num_cells; j++)
                                                        {
                                                            char const* prop_string =
                                                                rocprofvis_dm_get_property_as_charptr(
                                                                    table_row,
                                                                    kRPVDMExtTableRowCellValueCharPtrIndexed,
                                                                    j);
                                                            if(columns[j] ==
                                                               (rocprofvis_controller_processor_properties_t) 0)
                                                            {
                                                                continue;
                                                            }
                                                            else if(columns[j] ==
                                                                    kRPVControllerProcessorId ||
                                                                columns[j] ==
                                                                    kRPVControllerProcessorNodeId ||
                                                                columns[j] ==
                                                                    kRPVControllerProcessorIndex ||
                                                                columns[j] ==
                                                                    kRPVControllerProcessorLogicalIndex ||
                                                                columns[j] ==
                                                                    kRPVControllerProcessorTypeIndex)
                                                            {
                                                                char* end = nullptr;
                                                                uint64_t val =
                                                                    std::strtoull(
                                                                        prop_string, &end,
                                                                        10);
                                                                proc->SetUInt64(
                                                                    columns[j],
                                                                    0,
                                                                    val);
                                                                if(columns[j] ==
                                                                   kRPVControllerProcessorId)
                                                                {
                                                                    processors[val] = proc;
                                                                }
                                                            }
                                                            else
                                                            {
                                                                proc->SetString(
                                                                    columns[j], 0,
                                                                    prop_string,
                                                                    strlen(prop_string));
                                                            }
                                                        }

                                                        uint64_t node_id = 0;
                                                        proc->GetUInt64(kRPVControllerProcessorNodeId, 0, &node_id);
                                                        for (auto* node : m_nodes)
                                                        {
                                                            uint64_t id = 0;
                                                            node->GetUInt64(kRPVControllerNodeId, 0, &id);
                                                            if (id == node_id)
                                                            {
                                                                uint64_t num_procs = 0;
                                                                node->GetUInt64(kRPVControllerNodeNumProcessors, 0, &num_procs);
                                                                node->SetUInt64(kRPVControllerNodeNumProcessors, 0, num_procs+1);
                                                                node->SetObject(kRPVControllerNodeProcessorIndexed, num_procs, (rocprofvis_handle_t*)proc);
                                                                proc = nullptr;
                                                                break;
                                                            }
                                                        }
                                                        ROCPROFVIS_ASSERT(!proc);
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }

                                rocprofvis_dm_delete_table_at(m_dm_handle, table_id);
                            }

                            rocprofvis_db_future_free(future);
                        }
                    }

                    std::map<uint64_t, Process*> processes;
                    if(result == kRocProfVisDmResultSuccess)
                    {
                        rocprofvis_db_future_t future =
                            rocprofvis_db_future_alloc(nullptr);
                        if(future)
                        {
                            std::string processor_query =
                                "select * from rocpd_info_process";
                            rocprofvis_dm_table_id_t table_id = 0;
                            auto dm_result = rocprofvis_db_execute_query_async(
                                db, processor_query.c_str(), "Fetch processor info",
                                future, &table_id);
                            if(dm_result == kRocProfVisDmResultSuccess)
                            {
                                dm_result = rocprofvis_db_future_wait(future, UINT64_MAX);
                            }

                            if(dm_result == kRocProfVisDmResultSuccess)
                            {
                                uint64_t num_tables =
                                    rocprofvis_dm_get_property_as_uint64(
                                        m_dm_handle, kRPVDMNumberOfTablesUInt64, 0);
                                if(num_tables > 0)
                                {
                                    rocprofvis_dm_table_t table =
                                        rocprofvis_dm_get_property_as_handle(
                                            m_dm_handle, kRPVDMTableHandleByID, table_id);
                                    if(nullptr != table)
                                    {
                                        char* table_query =
                                            rocprofvis_dm_get_property_as_charptr(
                                                table, kRPVDMExtTableQueryCharPtr, 0);
                                        uint64_t num_columns =
                                            rocprofvis_dm_get_property_as_uint64(
                                                table, kRPVDMNumberOfTableColumnsUInt64,
                                                0);
                                        uint64_t num_rows =
                                            rocprofvis_dm_get_property_as_uint64(
                                                table, kRPVDMNumberOfTableRowsUInt64, 0);
                                        if(strcmp(table_query, processor_query.c_str()) ==
                                           0)
                                        {
                                            std::vector<
                                                rocprofvis_controller_process_properties_t>
                                                columns;
                                            for(int i = 0; i < num_columns; i++)
                                            {
                                                char const* column_name =
                                                    rocprofvis_dm_get_property_as_charptr(
                                                        table,
                                                        kRPVDMExtTableColumnNameCharPtrIndexed,
                                                        i);

                                                if(strcmp(column_name, "id") == 0)
                                                {
                                                    columns.push_back(
                                                        kRPVControllerProcessId);
                                                }
                                                else if(strcmp(column_name, "nid") == 0)
                                                {
                                                    columns.push_back(
                                                        kRPVControllerProcessNodeId);
                                                }
                                                else if(strcmp(column_name, "init") == 0)
                                                {
                                                    columns.push_back(
                                                        kRPVControllerProcessInitTime);
                                                }
                                                else if(strcmp(column_name,
                                                               "fini") == 0)
                                                {
                                                    columns.push_back(
                                                        kRPVControllerProcessFinishTime);
                                                }
                                                else if(strcmp(column_name, "start") == 0)
                                                {
                                                    columns.push_back(
                                                        kRPVControllerProcessStartTime);
                                                }
                                                else if(strcmp(column_name, "end") == 0)
                                                {
                                                    columns.push_back(
                                                        kRPVControllerProcessEndTime);
                                                }
                                                else if(strcmp(column_name,
                                                               "command") == 0)
                                                {
                                                    columns.push_back(
                                                        kRPVControllerProcessCommand);
                                                }
                                                else if(strcmp(column_name,
                                                               "environment") == 0)
                                                {
                                                    columns.push_back(
                                                        kRPVControllerProcessEnvironment);
                                                }
                                                else if(strcmp(column_name,
                                                               "extdata") == 0)
                                                {
                                                    columns.push_back(
                                                        kRPVControllerProcessExtData);
                                                }
                                                else
                                                {
                                                    columns.push_back((
                                                        rocprofvis_controller_process_properties_t) 0);
                                                }
                                            }
                                            for(int i = 0; i < num_rows; i++)
                                            {
                                                rocprofvis_dm_table_row_t table_row =
                                                    rocprofvis_dm_get_property_as_handle(
                                                        table,
                                                        kRPVDMExtTableRowHandleIndexed,
                                                        i);
                                                if(table_row != nullptr)
                                                {
                                                    uint64_t num_cells =
                                                        rocprofvis_dm_get_property_as_uint64(
                                                            table_row,
                                                            kRPVDMNumberOfTableRowCellsUInt64,
                                                            0);
                                                    if(num_cells == num_columns)
                                                    {
                                                        Process* proc = new Process;

                                                        std::vector<std::string> row;
                                                        for(int j = 0; j < num_cells; j++)
                                                        {
                                                            char const* prop_string =
                                                                rocprofvis_dm_get_property_as_charptr(
                                                                    table_row,
                                                                    kRPVDMExtTableRowCellValueCharPtrIndexed,
                                                                    j);
                                                            if(columns[j] ==
                                                               (rocprofvis_controller_process_properties_t) 0)
                                                            {
                                                                continue;
                                                            }
                                                            else if(
                                                                columns[j] ==
                                                                    kRPVControllerProcessId ||
                                                                columns[j] ==
                                                                    kRPVControllerProcessNodeId)
                                                            {
                                                                char* end = nullptr;
                                                                uint64_t val =
                                                                    std::strtoull(
                                                                        prop_string, &end,
                                                                        10);
                                                                proc->SetUInt64(
                                                                    columns[j], 0,
                                                                    val);
                                                                if (columns[j] ==
                                                                    kRPVControllerProcessId)
                                                                {
                                                                    processes[val] = proc;
                                                                }
                                                            }
                                                            else if(
                                                                columns[j] ==
                                                                    kRPVControllerProcessInitTime ||
                                                                columns[j] ==
                                                                    kRPVControllerProcessFinishTime ||
                                                                columns[j] ==
                                                                    kRPVControllerProcessStartTime ||
                                                                columns[j] ==
                                                                    kRPVControllerProcessEndTime)
                                                            {
                                                                char* end = nullptr;
                                                                proc->SetDouble(
                                                                    columns[j], 0,
                                                                    strtod(
                                                                        prop_string, &end));
                                                            }
                                                            else
                                                            {
                                                                proc->SetString(
                                                                    columns[j], 0,
                                                                    prop_string,
                                                                    strlen(prop_string));
                                                            }
                                                        }

                                                        uint64_t node_id = 0;
                                                        proc->GetUInt64(
                                                            kRPVControllerProcessNodeId,
                                                            0, &node_id);
                                                        for(auto* node : m_nodes)
                                                        {
                                                            uint64_t id = 0;
                                                            node->GetUInt64(
                                                                kRPVControllerNodeId, 0,
                                                                &id);
                                                            if(id == node_id)
                                                            {
                                                                uint64_t num_procs = 0;
                                                                node->GetUInt64(
                                                                    kRPVControllerNodeNumProcesses,
                                                                    0, &num_procs);
                                                                node->SetUInt64(
                                                                    kRPVControllerNodeNumProcesses,
                                                                    0, num_procs + 1);
                                                                node->SetObject(
                                                                    kRPVControllerNodeProcessIndexed,
                                                                    num_procs,
                                                                    (rocprofvis_handle_t*)
                                                                        proc);
                                                                proc = nullptr;
                                                                break;
                                                            }
                                                        }
                                                        ROCPROFVIS_ASSERT(!proc);
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }

                                rocprofvis_dm_delete_table_at(m_dm_handle, table_id);
                            }

                            rocprofvis_db_future_free(future);
                        }
                    }

                    rocprofvis_dm_delete_all_tables(m_dm_handle);

                    std::map<uint64_t, uint64_t> queue_to_device;
                    if(result == kRocProfVisDmResultSuccess)
                    {
                        rocprofvis_db_future_t future =
                            rocprofvis_db_future_alloc(nullptr);
                        if(future)
                        {
                            std::string stream_query =
                                "SELECT DISTINCT "
                                "concat(nid,\"-\",pid,\"-\",agent_id,\"-\", queue_id) as "
                                "tuple, agent_id, queue_id FROM rocpd_kernel_dispatch;";
                            rocprofvis_dm_table_id_t table_id = 0;
                            auto dm_result = rocprofvis_db_execute_query_async(
                                db, stream_query.c_str(),
                                "Fetch agent-queue mapping info", future, &table_id);
                            if(dm_result == kRocProfVisDmResultSuccess)
                            {
                                dm_result = rocprofvis_db_future_wait(future, UINT64_MAX);
                            }

                            if(dm_result == kRocProfVisDmResultSuccess)
                            {
                                uint64_t num_tables =
                                    rocprofvis_dm_get_property_as_uint64(
                                        m_dm_handle, kRPVDMNumberOfTablesUInt64, 0);
                                if(num_tables > 0)
                                {
                                    rocprofvis_dm_table_t table =
                                        rocprofvis_dm_get_property_as_handle(
                                            m_dm_handle, kRPVDMTableHandleByID, table_id);
                                    if(nullptr != table)
                                    {
                                        char* table_query =
                                            rocprofvis_dm_get_property_as_charptr(
                                                table, kRPVDMExtTableQueryCharPtr, 0);
                                        uint64_t num_columns =
                                            rocprofvis_dm_get_property_as_uint64(
                                                table, kRPVDMNumberOfTableColumnsUInt64,
                                                0);
                                        uint64_t num_rows =
                                            rocprofvis_dm_get_property_as_uint64(
                                                table, kRPVDMNumberOfTableRowsUInt64, 0);
                                        if(strcmp(table_query, stream_query.c_str()) ==
                                               0 &&
                                           num_columns == 3)
                                        {
                                            for(int i = 0; i < num_rows; i++)
                                            {
                                                rocprofvis_dm_table_row_t table_row =
                                                    rocprofvis_dm_get_property_as_handle(
                                                        table,
                                                        kRPVDMExtTableRowHandleIndexed,
                                                        i);
                                                if(table_row != nullptr)
                                                {
                                                    uint64_t num_cells =
                                                        rocprofvis_dm_get_property_as_uint64(
                                                            table_row,
                                                            kRPVDMNumberOfTableRowCellsUInt64,
                                                            0);
                                                    if(num_cells == num_columns)
                                                    {
                                                        char const* agent_string =
                                                            rocprofvis_dm_get_property_as_charptr(
                                                                table_row,
                                                                kRPVDMExtTableRowCellValueCharPtrIndexed,
                                                                1);
                                                        char*    end = nullptr;
                                                        uint64_t device_id =
                                                            std::strtoull(agent_string,
                                                                          &end, 10);

                                                        char const* queue_string =
                                                            rocprofvis_dm_get_property_as_charptr(
                                                                table_row,
                                                                kRPVDMExtTableRowCellValueCharPtrIndexed,
                                                                2);
                                                        end               = nullptr;
                                                        uint64_t queue_id = std::strtoull(
                                                            queue_string, &end, 10);

                                                        queue_to_device[queue_id] =
                                                            device_id;
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }

                                rocprofvis_dm_delete_table_at(m_dm_handle, table_id);
                            }

                            rocprofvis_db_future_free(future);
                        }
                    }

                    std::map<uint64_t, uint64_t> stream_to_device;
                    if(result == kRocProfVisDmResultSuccess)
                    {
                        rocprofvis_db_future_t future =
                            rocprofvis_db_future_alloc(nullptr);
                        if(future)
                        {
                            std::string stream_query =
                                "SELECT DISTINCT "
                                "concat(nid,\"-\",pid,\"-\",agent_id,\"-\",stream_id) as "
                                "tuple, agent_id, stream_id FROM rocpd_kernel_dispatch;";
                            rocprofvis_dm_table_id_t table_id = 0;
                            auto dm_result = rocprofvis_db_execute_query_async(
                                db, stream_query.c_str(),
                                "Fetch agent-stream mapping info", future, &table_id);
                            if(dm_result == kRocProfVisDmResultSuccess)
                            {
                                dm_result = rocprofvis_db_future_wait(future, UINT64_MAX);
                            }

                            if(dm_result == kRocProfVisDmResultSuccess)
                            {
                                uint64_t num_tables =
                                    rocprofvis_dm_get_property_as_uint64(
                                        m_dm_handle, kRPVDMNumberOfTablesUInt64, 0);
                                if(num_tables > 0)
                                {
                                    rocprofvis_dm_table_t table =
                                        rocprofvis_dm_get_property_as_handle(
                                            m_dm_handle, kRPVDMTableHandleByID, table_id);
                                    if(nullptr != table)
                                    {
                                        char* table_query =
                                            rocprofvis_dm_get_property_as_charptr(
                                                table, kRPVDMExtTableQueryCharPtr, 0);
                                        uint64_t num_columns =
                                            rocprofvis_dm_get_property_as_uint64(
                                                table, kRPVDMNumberOfTableColumnsUInt64,
                                                0);
                                        uint64_t num_rows =
                                            rocprofvis_dm_get_property_as_uint64(
                                                table, kRPVDMNumberOfTableRowsUInt64, 0);
                                        if(strcmp(table_query, stream_query.c_str()) ==
                                               0 &&
                                           num_columns == 3)
                                        {
                                            for(int i = 0; i < num_rows; i++)
                                            {
                                                rocprofvis_dm_table_row_t table_row =
                                                    rocprofvis_dm_get_property_as_handle(
                                                        table,
                                                        kRPVDMExtTableRowHandleIndexed,
                                                        i);
                                                if(table_row != nullptr)
                                                {
                                                    uint64_t num_cells =
                                                        rocprofvis_dm_get_property_as_uint64(
                                                            table_row,
                                                            kRPVDMNumberOfTableRowCellsUInt64,
                                                            0);
                                                    if(num_cells == num_columns)
                                                    {
                                                        char const* agent_string =
                                                            rocprofvis_dm_get_property_as_charptr(
                                                                table_row,
                                                                kRPVDMExtTableRowCellValueCharPtrIndexed,
                                                                1);
                                                        char*    end = nullptr;
                                                        uint64_t device_id =
                                                            std::strtoull(agent_string,
                                                                          &end, 10);

                                                        char const* stream_string =
                                                            rocprofvis_dm_get_property_as_charptr(
                                                                table_row,
                                                                kRPVDMExtTableRowCellValueCharPtrIndexed,
                                                                2);
                                                        end               = nullptr;
                                                        uint64_t stream_id = std::strtoull(
                                                            stream_string, &end, 10);

                                                        stream_to_device[stream_id] =
                                                            device_id;
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }

                                rocprofvis_dm_delete_table_at(m_dm_handle, table_id);
                            }

                            rocprofvis_db_future_free(future);
                        }
                    }

                    std::map<uint64_t, std::set<uint64_t>> stream_to_queue;
                    if(result == kRocProfVisDmResultSuccess)
                    {
                        rocprofvis_db_future_t future =
                            rocprofvis_db_future_alloc(nullptr);
                        if(future)
                        {
                            std::string stream_query =
                                "SELECT DISTINCT "
                                "concat(nid,\"-\",pid,\"-\",stream_id,\"-\",queue_id) as "
                                "tuple, stream_id, queue_id FROM rocpd_kernel_dispatch;";
                            rocprofvis_dm_table_id_t table_id = 0;
                            auto dm_result = rocprofvis_db_execute_query_async(
                                db, stream_query.c_str(),
                                "Fetch stream-queue mapping info", future, &table_id);
                            if(dm_result == kRocProfVisDmResultSuccess)
                            {
                                dm_result = rocprofvis_db_future_wait(future, UINT64_MAX);
                            }

                            if(dm_result == kRocProfVisDmResultSuccess)
                            {
                                uint64_t num_tables =
                                    rocprofvis_dm_get_property_as_uint64(
                                        m_dm_handle, kRPVDMNumberOfTablesUInt64, 0);
                                if(num_tables > 0)
                                {
                                    rocprofvis_dm_table_t table =
                                        rocprofvis_dm_get_property_as_handle(
                                            m_dm_handle, kRPVDMTableHandleByID, table_id);
                                    if(nullptr != table)
                                    {
                                        char* table_query =
                                            rocprofvis_dm_get_property_as_charptr(
                                                table, kRPVDMExtTableQueryCharPtr, 0);
                                        uint64_t num_columns =
                                            rocprofvis_dm_get_property_as_uint64(
                                                table, kRPVDMNumberOfTableColumnsUInt64,
                                                0);
                                        uint64_t num_rows =
                                            rocprofvis_dm_get_property_as_uint64(
                                                table, kRPVDMNumberOfTableRowsUInt64, 0);
                                        if(strcmp(table_query, stream_query.c_str()) ==
                                               0 &&
                                           num_columns == 3)
                                        {
                                            for(int i = 0; i < num_rows; i++)
                                            {
                                                rocprofvis_dm_table_row_t table_row =
                                                    rocprofvis_dm_get_property_as_handle(
                                                        table,
                                                        kRPVDMExtTableRowHandleIndexed,
                                                        i);
                                                if(table_row != nullptr)
                                                {
                                                    uint64_t num_cells =
                                                        rocprofvis_dm_get_property_as_uint64(
                                                            table_row,
                                                            kRPVDMNumberOfTableRowCellsUInt64,
                                                            0);
                                                    if(num_cells == num_columns)
                                                    {
                                                        char const* stream_string =
                                                            rocprofvis_dm_get_property_as_charptr(
                                                                table_row,
                                                                kRPVDMExtTableRowCellValueCharPtrIndexed,
                                                                1);
                                                        char*    end = nullptr;
                                                        uint64_t stream_id =
                                                            std::strtoull(stream_string,
                                                                          &end, 10);

                                                        char const* queue_string =
                                                            rocprofvis_dm_get_property_as_charptr(
                                                                table_row,
                                                                kRPVDMExtTableRowCellValueCharPtrIndexed,
                                                                2);
                                                        end = nullptr;
                                                        uint64_t queue_id =
                                                            std::strtoull(queue_string,
                                                                          &end, 10);

                                                        std::set<uint64_t>& set = stream_to_queue[stream_id];
                                                        set.insert(queue_id);
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }

                                rocprofvis_dm_delete_table_at(m_dm_handle, table_id);
                            }

                            rocprofvis_db_future_free(future);
                        }
                    }

                    if(result == kRocProfVisDmResultSuccess)
                    {
                        rocprofvis_db_future_t future =
                            rocprofvis_db_future_alloc(nullptr);
                        if(future)
                        {
                            std::string thread_query =
                                "select * from rocpd_info_thread;";
                            rocprofvis_dm_table_id_t table_id = 0;
                            auto dm_result = rocprofvis_db_execute_query_async(
                                db, thread_query.c_str(), "Fetch thread info",
                                future, &table_id);
                            if(dm_result == kRocProfVisDmResultSuccess)
                            {
                                dm_result = rocprofvis_db_future_wait(future, UINT64_MAX);
                            }

                            if(dm_result == kRocProfVisDmResultSuccess)
                            {
                                uint64_t num_tables =
                                    rocprofvis_dm_get_property_as_uint64(
                                        m_dm_handle, kRPVDMNumberOfTablesUInt64, 0);
                                if(num_tables > 0)
                                {
                                    rocprofvis_dm_table_t table =
                                        rocprofvis_dm_get_property_as_handle(
                                            m_dm_handle, kRPVDMTableHandleByID, table_id);
                                    if(nullptr != table)
                                    {
                                        char* table_query =
                                            rocprofvis_dm_get_property_as_charptr(
                                                table, kRPVDMExtTableQueryCharPtr, 0);
                                        uint64_t num_columns =
                                            rocprofvis_dm_get_property_as_uint64(
                                                table, kRPVDMNumberOfTableColumnsUInt64,
                                                0);
                                        uint64_t num_rows =
                                            rocprofvis_dm_get_property_as_uint64(
                                                table, kRPVDMNumberOfTableRowsUInt64, 0);
                                        if(strcmp(table_query, thread_query.c_str()) ==
                                           0)
                                        {
                                            std::vector<
                                                rocprofvis_controller_thread_properties_t>
                                                columns;
                                            for(int i = 0; i < num_columns; i++)
                                            {
                                                char const* column_name =
                                                    rocprofvis_dm_get_property_as_charptr(
                                                        table,
                                                        kRPVDMExtTableColumnNameCharPtrIndexed,
                                                        i);

                                                if(strcmp(column_name, "id") == 0)
                                                {
                                                    columns.push_back(
                                                        kRPVControllerThreadId);
                                                }
                                                else if(strcmp(column_name, "nid") == 0)
                                                {
                                                    columns.push_back(
                                                        kRPVControllerThreadNodeId);
                                                }
                                                else if(strcmp(column_name, "pid") == 0)
                                                {
                                                    columns.push_back(
                                                        kRPVControllerThreadProcessId);
                                                }
                                                else if(strcmp(column_name,
                                                               "ppid") == 0)
                                                {
                                                    columns.push_back(
                                                        kRPVControllerThreadParentId);
                                                }
                                                else if(strcmp(column_name, "tid") == 0)
                                                {
                                                    columns.push_back(
                                                        kRPVControllerThreadTid);
                                                }
                                                else if(strcmp(column_name, "name") == 0)
                                                {
                                                    columns.push_back(
                                                        kRPVControllerThreadName);
                                                }
                                                else if(strcmp(column_name,
                                                               "extdata") == 0)
                                                {
                                                    columns.push_back(
                                                        kRPVControllerThreadExtData);
                                                }
                                                else if(strcmp(column_name,
                                                               "start") == 0)
                                                {
                                                    columns.push_back(
                                                        kRPVControllerThreadStartTime);
                                                }
                                                else if(strcmp(column_name,
                                                               "end") == 0)
                                                {
                                                    columns.push_back(
                                                        kRPVControllerThreadEndTime);
                                                }
                                                else
                                                {
                                                    columns.push_back((
                                                        rocprofvis_controller_thread_properties_t) 0);
                                                }
                                            }
                                            for(int i = 0; i < num_rows; i++)
                                            {
                                                rocprofvis_dm_table_row_t table_row =
                                                    rocprofvis_dm_get_property_as_handle(
                                                        table,
                                                        kRPVDMExtTableRowHandleIndexed,
                                                        i);
                                                if(table_row != nullptr)
                                                {
                                                    uint64_t num_cells =
                                                        rocprofvis_dm_get_property_as_uint64(
                                                            table_row,
                                                            kRPVDMNumberOfTableRowCellsUInt64,
                                                            0);
                                                    if(num_cells == num_columns)
                                                    {
                                                        Thread* thread = new Thread;

                                                        std::vector<std::string> row;
                                                        for(int j = 0; j < num_cells; j++)
                                                        {
                                                            char const* prop_string =
                                                                rocprofvis_dm_get_property_as_charptr(
                                                                    table_row,
                                                                    kRPVDMExtTableRowCellValueCharPtrIndexed,
                                                                    j);
                                                            if(columns[j] ==
                                                               (rocprofvis_controller_thread_properties_t) 0)
                                                            {
                                                                continue;
                                                            }
                                                            else if(
                                                                columns[j] ==
                                                                    kRPVControllerThreadId ||
                                                                columns[j] ==
                                                                    kRPVControllerThreadNodeId ||
                                                                columns[j] ==
                                                                    kRPVControllerThreadProcessId ||
                                                                columns[j] ==
                                                                    kRPVControllerThreadParentId ||
                                                                columns[j] ==
                                                                    kRPVControllerThreadTid)
                                                            {
                                                                char* end = nullptr;
                                                                uint64_t val =
                                                                    std::strtoull(
                                                                        prop_string, &end,
                                                                        10);
                                                                thread->SetUInt64(
                                                                    columns[j], 0,
                                                                    val);

                                                                if (columns[j] == kRPVControllerThreadId)
                                                                {
                                                                    Track* t =
                                                                        thread_to_track
                                                                            [val];
                                                                    t->SetObject(
                                                                        kRPVControllerTrackThread,
                                                                        0,
                                                                        (rocprofvis_handle_t*)
                                                                            thread);

                                                                    thread->SetObject(kRPVControllerThreadTrack, 0, (rocprofvis_handle_t*)t);
                                                                }
                                                            }
                                                            else if(
                                                                columns[j] ==
                                                                    kRPVControllerThreadStartTime ||
                                                                columns[j] ==
                                                                    kRPVControllerThreadEndTime)
                                                            {
                                                                char* end = nullptr;
                                                                thread->SetDouble(
                                                                    columns[j], 0,
                                                                    strtod(
                                                                        prop_string, &end));
                                                            }
                                                            else
                                                            {
                                                                thread->SetString(
                                                                    columns[j], 0,
                                                                    prop_string,
                                                                    strlen(prop_string));
                                                            }
                                                        }

                                                        uint64_t node_id = 0;
                                                        thread->GetUInt64(
                                                            kRPVControllerThreadNodeId,
                                                            0, &node_id);
                                                        uint64_t process_id = 0;
                                                        thread->GetUInt64(
                                                            kRPVControllerThreadProcessId, 0,
                                                            &process_id);
                                                        for(auto* node : m_nodes)
                                                        {
                                                            uint64_t id = 0;
                                                            node->GetUInt64(
                                                                kRPVControllerNodeId, 0,
                                                                &id);
                                                            if(id == node_id)
                                                            {
                                                                uint64_t num_procs = 0;
                                                                node->GetUInt64(
                                                                    kRPVControllerNodeNumProcesses,
                                                                    0, &num_procs);

                                                                for (uint32_t i = 0; i < num_procs; i++)
                                                                {
                                                                    ProcessRef ref;
                                                                    if (node->GetObject(
                                                                        kRPVControllerNodeProcessIndexed,
                                                                        i,
                                                                        ref.GetHandleAddress()) == kRocProfVisResultSuccess && ref.IsValid())
                                                                    {
                                                                        uint64_t proc_id =
                                                                            0;
                                                                        ref->GetUInt64(
                                                                            kRPVControllerProcessId,
                                                                            0, &proc_id);

                                                                        if (proc_id == process_id)
                                                                        {
                                                                            uint64_t num_threads = 0;
                                                                            ref->GetUInt64(
                                                                                kRPVControllerProcessNumThreads,
                                                                                0,
                                                                                &num_threads);
                                                                            ref->SetUInt64(
                                                                                kRPVControllerProcessNumThreads,
                                                                                0,
                                                                                num_threads +
                                                                                    1);
                                                                            ref->SetObject(
                                                                                kRPVControllerProcessThreadIndexed,
                                                                                num_threads,
                                                                                (rocprofvis_handle_t*)
                                                                                    thread);
                                                                            thread =
                                                                                nullptr;
                                                                            break;
                                                                        }
                                                                    }
                                                                }

                                                                break;
                                                            }
                                                        }
                                                        ROCPROFVIS_ASSERT(!thread);
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }

                                rocprofvis_dm_delete_table_at(m_dm_handle, table_id);
                            }

                            rocprofvis_db_future_free(future);
                        }
                    }

                    std::map<uint64_t, Queue*> queues;
                    if(result == kRocProfVisDmResultSuccess)
                    {
                        rocprofvis_db_future_t future =
                            rocprofvis_db_future_alloc(nullptr);
                        if(future)
                        {
                            std::string queue_query = "select * from rocpd_info_queue;";
                            rocprofvis_dm_table_id_t table_id = 0;
                            auto dm_result = rocprofvis_db_execute_query_async(
                                db, queue_query.c_str(), "Fetch queue info", future,
                                &table_id);
                            if(dm_result == kRocProfVisDmResultSuccess)
                            {
                                dm_result = rocprofvis_db_future_wait(future, UINT64_MAX);
                            }

                            if(dm_result == kRocProfVisDmResultSuccess)
                            {
                                uint64_t num_tables =
                                    rocprofvis_dm_get_property_as_uint64(
                                        m_dm_handle, kRPVDMNumberOfTablesUInt64, 0);
                                if(num_tables > 0)
                                {
                                    rocprofvis_dm_table_t table =
                                        rocprofvis_dm_get_property_as_handle(
                                            m_dm_handle, kRPVDMTableHandleByID, table_id);
                                    if(nullptr != table)
                                    {
                                        char* table_query =
                                            rocprofvis_dm_get_property_as_charptr(
                                                table, kRPVDMExtTableQueryCharPtr, 0);
                                        uint64_t num_columns =
                                            rocprofvis_dm_get_property_as_uint64(
                                                table, kRPVDMNumberOfTableColumnsUInt64,
                                                0);
                                        uint64_t num_rows =
                                            rocprofvis_dm_get_property_as_uint64(
                                                table, kRPVDMNumberOfTableRowsUInt64, 0);
                                        if(strcmp(table_query, queue_query.c_str()) == 0)
                                        {
                                            std::vector<
                                                rocprofvis_controller_queue_properties_t>
                                                columns;
                                            for(int i = 0; i < num_columns; i++)
                                            {
                                                char const* column_name =
                                                    rocprofvis_dm_get_property_as_charptr(
                                                        table,
                                                        kRPVDMExtTableColumnNameCharPtrIndexed,
                                                        i);

                                                if(strcmp(column_name, "id") == 0)
                                                {
                                                    columns.push_back(
                                                        kRPVControllerQueueId);
                                                }
                                                else if(strcmp(column_name, "nid") == 0)
                                                {
                                                    columns.push_back(
                                                        kRPVControllerQueueNodeId);
                                                }
                                                else if(strcmp(column_name, "pid") == 0)
                                                {
                                                    columns.push_back(
                                                        kRPVControllerQueueProcessId);
                                                }
                                                else if(strcmp(column_name, "name") == 0)
                                                {
                                                    columns.push_back(
                                                        kRPVControllerQueueName);
                                                }
                                                else if(strcmp(column_name, "extdata") ==
                                                        0)
                                                {
                                                    columns.push_back(
                                                        kRPVControllerQueueExtData);
                                                }
                                                else
                                                {
                                                    columns.push_back((
                                                        rocprofvis_controller_queue_properties_t) 0);
                                                }
                                            }
                                            for(int i = 0; i < num_rows; i++)
                                            {
                                                rocprofvis_dm_table_row_t table_row =
                                                    rocprofvis_dm_get_property_as_handle(
                                                        table,
                                                        kRPVDMExtTableRowHandleIndexed,
                                                        i);
                                                if(table_row != nullptr)
                                                {
                                                    uint64_t num_cells =
                                                        rocprofvis_dm_get_property_as_uint64(
                                                            table_row,
                                                            kRPVDMNumberOfTableRowCellsUInt64,
                                                            0);
                                                    if(num_cells == num_columns)
                                                    {
                                                        Queue* queue = new Queue;

                                                        std::vector<std::string> row;
                                                        for(int j = 0; j < num_cells; j++)
                                                        {
                                                            char const* prop_string =
                                                                rocprofvis_dm_get_property_as_charptr(
                                                                    table_row,
                                                                    kRPVDMExtTableRowCellValueCharPtrIndexed,
                                                                    j);
                                                            if(columns[j] ==
                                                               (rocprofvis_controller_queue_properties_t) 0)
                                                            {
                                                                continue;
                                                            }
                                                            else if(
                                                                columns[j] ==
                                                                    kRPVControllerQueueId ||
                                                                columns[j] ==
                                                                    kRPVControllerQueueNodeId ||
                                                                columns[j] ==
                                                                    kRPVControllerQueueProcessId)
                                                            {
                                                                char* end = nullptr;
                                                                uint64_t val =
                                                                    std::strtoull(
                                                                        prop_string, &end,
                                                                        10);
                                                                queue->SetUInt64(
                                                                    columns[j], 0,
                                                                    val);
                                                                if (columns[j] ==
                                                                    kRPVControllerQueueId)
                                                                {
                                                                    auto it =
                                                                        processors.find(
                                                                            queue_to_device
                                                                                [val]);
                                                                    if(it !=
                                                                       processors.end())
                                                                    {
                                                                        Processor* p =
                                                                            it->second;
                                                                        ROCPROFVIS_ASSERT(
                                                                            p);

                                                                        queue->SetObject(
                                                                            kRPVControllerQueueProcessor,
                                                                            0,
                                                                            (rocprofvis_handle_t*)
                                                                                p);
                                                                        queues[val] =
                                                                            queue;

                                                                        uint64_t
                                                                            count = 0;
                                                                        p->GetUInt64(
                                                                            kRPVControllerProcessorNumQueues,
                                                                            0,
                                                                            &count);
                                                                        p->SetUInt64(
                                                                            kRPVControllerProcessorNumQueues,
                                                                            0,
                                                                            count +
                                                                                1);
                                                                        p->SetObject(
                                                                            kRPVControllerProcessorQueueIndexed,
                                                                            count,
                                                                            (rocprofvis_handle_t*)
                                                                                queue);
                                                                    }

                                                                    Track* t =
                                                                        queue_to_track
                                                                            [val];
                                                                    t->SetObject(
                                                                        kRPVControllerTrackQueue,
                                                                        0, (rocprofvis_handle_t*)queue);
                                                                    queue->SetObject(
                                                                        kRPVControllerQueueTrack,
                                                                        0, (rocprofvis_handle_t*)t);
                                                                }
                                                            }
                                                            else
                                                            {
                                                                queue->SetString(
                                                                    columns[j], 0,
                                                                    prop_string,
                                                                    strlen(prop_string));
                                                            }
                                                        }

                                                        uint64_t node_id = 0;
                                                        queue->GetUInt64(
                                                            kRPVControllerQueueNodeId, 0,
                                                            &node_id);
                                                        uint64_t process_id = 0;
                                                        queue->GetUInt64(
                                                            kRPVControllerQueueProcessId,
                                                            0, &process_id);
                                                        for(auto* node : m_nodes)
                                                        {
                                                            uint64_t id = 0;
                                                            node->GetUInt64(
                                                                kRPVControllerNodeId, 0,
                                                                &id);
                                                            if(id == node_id)
                                                            {
                                                                uint64_t num_procs = 0;
                                                                node->GetUInt64(
                                                                    kRPVControllerNodeNumProcesses,
                                                                    0, &num_procs);

                                                                for(uint32_t i = 0;
                                                                    i < num_procs; i++)
                                                                {
                                                                    ProcessRef ref;
                                                                    if(node->GetObject(
                                                                           kRPVControllerNodeProcessIndexed,
                                                                           i,
                                                                           ref.GetHandleAddress()) ==
                                                                           kRocProfVisResultSuccess &&
                                                                       ref.IsValid())
                                                                    {
                                                                        uint64_t proc_id =
                                                                            0;
                                                                        ref->GetUInt64(
                                                                            kRPVControllerProcessId,
                                                                            0, &proc_id);

                                                                        if(proc_id ==
                                                                           process_id)
                                                                        {
                                                                            uint64_t
                                                                                num_queues =
                                                                                    0;
                                                                            ref->GetUInt64(
                                                                                kRPVControllerProcessNumQueues,
                                                                                0,
                                                                                &num_queues);
                                                                            ref->SetUInt64(
                                                                                kRPVControllerProcessNumQueues,
                                                                                0,
                                                                                num_queues +
                                                                                    1);
                                                                            ref->SetObject(
                                                                                kRPVControllerProcessQueueIndexed,
                                                                                num_queues,
                                                                                (rocprofvis_handle_t*)
                                                                                    queue);
                                                                            queue =
                                                                                nullptr;
                                                                            break;
                                                                        }
                                                                    }
                                                                }

                                                                break;
                                                            }
                                                        }
                                                        ROCPROFVIS_ASSERT(!queue);
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }

                                rocprofvis_dm_delete_table_at(m_dm_handle, table_id);
                            }

                            rocprofvis_db_future_free(future);
                        }
                    }

                    if(result == kRocProfVisDmResultSuccess)
                    {
                        rocprofvis_db_future_t future =
                            rocprofvis_db_future_alloc(nullptr);
                        if(future)
                        {
                            std::string stream_query = "select * from rocpd_info_stream;";
                            rocprofvis_dm_table_id_t table_id = 0;
                            auto dm_result = rocprofvis_db_execute_query_async(
                                db, stream_query.c_str(), "Fetch stream info", future,
                                &table_id);
                            if(dm_result == kRocProfVisDmResultSuccess)
                            {
                                dm_result = rocprofvis_db_future_wait(future, UINT64_MAX);
                            }

                            if(dm_result == kRocProfVisDmResultSuccess)
                            {
                                uint64_t num_tables =
                                    rocprofvis_dm_get_property_as_uint64(
                                        m_dm_handle, kRPVDMNumberOfTablesUInt64, 0);
                                if(num_tables > 0)
                                {
                                    rocprofvis_dm_table_t table =
                                        rocprofvis_dm_get_property_as_handle(
                                            m_dm_handle, kRPVDMTableHandleByID, table_id);
                                    if(nullptr != table)
                                    {
                                        char* table_query =
                                            rocprofvis_dm_get_property_as_charptr(
                                                table, kRPVDMExtTableQueryCharPtr, 0);
                                        uint64_t num_columns =
                                            rocprofvis_dm_get_property_as_uint64(
                                                table, kRPVDMNumberOfTableColumnsUInt64,
                                                0);
                                        uint64_t num_rows =
                                            rocprofvis_dm_get_property_as_uint64(
                                                table, kRPVDMNumberOfTableRowsUInt64, 0);
                                        if(strcmp(table_query, stream_query.c_str()) == 0)
                                        {
                                            std::vector<
                                                rocprofvis_controller_stream_properties_t>
                                                columns;
                                            for(int i = 0; i < num_columns; i++)
                                            {
                                                char const* column_name =
                                                    rocprofvis_dm_get_property_as_charptr(
                                                        table,
                                                        kRPVDMExtTableColumnNameCharPtrIndexed,
                                                        i);

                                                if(strcmp(column_name, "id") == 0)
                                                {
                                                    columns.push_back(
                                                        kRPVControllerStreamId);
                                                }
                                                else if(strcmp(column_name, "nid") == 0)
                                                {
                                                    columns.push_back(
                                                        kRPVControllerStreamNodeId);
                                                }
                                                else if(strcmp(column_name, "pid") == 0)
                                                {
                                                    columns.push_back(
                                                        kRPVControllerStreamProcessId);
                                                }
                                                else if(strcmp(column_name, "name") == 0)
                                                {
                                                    columns.push_back(
                                                        kRPVControllerStreamName);
                                                }
                                                else if(strcmp(column_name, "extdata") ==
                                                        0)
                                                {
                                                    columns.push_back(
                                                        kRPVControllerStreamExtData);
                                                }
                                                else
                                                {
                                                    columns.push_back((
                                                        rocprofvis_controller_stream_properties_t) 0);
                                                }
                                            }
                                            for(int i = 0; i < num_rows; i++)
                                            {
                                                rocprofvis_dm_table_row_t table_row =
                                                    rocprofvis_dm_get_property_as_handle(
                                                        table,
                                                        kRPVDMExtTableRowHandleIndexed,
                                                        i);
                                                if(table_row != nullptr)
                                                {
                                                    uint64_t num_cells =
                                                        rocprofvis_dm_get_property_as_uint64(
                                                            table_row,
                                                            kRPVDMNumberOfTableRowCellsUInt64,
                                                            0);
                                                    if(num_cells == num_columns)
                                                    {
                                                        Stream* stream = new Stream;

                                                        std::vector<std::string> row;
                                                        for(int j = 0; j < num_cells; j++)
                                                        {
                                                            char const* prop_string =
                                                                rocprofvis_dm_get_property_as_charptr(
                                                                    table_row,
                                                                    kRPVDMExtTableRowCellValueCharPtrIndexed,
                                                                    j);
                                                            if(columns[j] ==
                                                               (rocprofvis_controller_stream_properties_t) 0)
                                                            {
                                                                continue;
                                                            }
                                                            else if(
                                                                columns[j] ==
                                                                    kRPVControllerStreamId ||
                                                                columns[j] ==
                                                                    kRPVControllerStreamNodeId ||
                                                                columns[j] ==
                                                                    kRPVControllerStreamProcessId)
                                                            {
                                                                char*    end = nullptr;
                                                                uint64_t val =
                                                                    std::strtoull(
                                                                        prop_string, &end,
                                                                        10);
                                                                stream->SetUInt64(
                                                                    columns[j], 0, val);
                                                                if(columns[j] ==
                                                                   kRPVControllerStreamId)
                                                                {
                                                                    auto it = processors.find(stream_to_device[val]);
                                                                    if(it !=
                                                                       processors.end())
                                                                    {
                                                                        Processor* p =
                                                                            it->second;
                                                                        stream->SetObject(
                                                                            kRPVControllerStreamProcessor,
                                                                            0,
                                                                            (rocprofvis_handle_t*)
                                                                                p);

                                                                        uint64_t count =
                                                                            0;
                                                                        p->GetUInt64(
                                                                            kRPVControllerProcessorNumStreams,
                                                                            0, &count);
                                                                        p->SetUInt64(
                                                                            kRPVControllerProcessorNumStreams,
                                                                            0, count + 1);
                                                                        p->SetObject(
                                                                            kRPVControllerProcessorStreamIndexed,
                                                                            count,
                                                                            (rocprofvis_handle_t*)
                                                                                stream);
                                                                    }
                                                                    auto& set = stream_to_queue[val];
                                                                    for (uint64_t queue_id : set)
                                                                    {
                                                                        Queue* q = queues[queue_id];
                                                                        uint64_t
                                                                            num_queues =
                                                                                0;
                                                                        stream->GetUInt64(
                                                                            kRPVControllerStreamNumQueues,
                                                                            0,
                                                                            &num_queues);
                                                                        stream->SetUInt64(
                                                                            kRPVControllerStreamNumQueues,
                                                                            0,
                                                                            num_queues +
                                                                                1);
                                                                        stream->SetObject(
                                                                            kRPVControllerStreamQueueIndexed,
                                                                            num_queues,
                                                                            (rocprofvis_handle_t*)
                                                                                q);
                                                                    }
                                                                }
                                                            }
                                                            else
                                                            {
                                                                stream->SetString(
                                                                    columns[j], 0,
                                                                    prop_string,
                                                                    strlen(prop_string));
                                                            }
                                                        }

                                                        uint64_t node_id = 0;
                                                        stream->GetUInt64(
                                                            kRPVControllerStreamNodeId, 0,
                                                            &node_id);
                                                        uint64_t process_id = 0;
                                                        stream->GetUInt64(
                                                            kRPVControllerStreamProcessId,
                                                            0, &process_id);
                                                        for(auto* node : m_nodes)
                                                        {
                                                            uint64_t id = 0;
                                                            node->GetUInt64(
                                                                kRPVControllerNodeId, 0,
                                                                &id);
                                                            if(id == node_id)
                                                            {
                                                                uint64_t num_procs = 0;
                                                                node->GetUInt64(
                                                                    kRPVControllerNodeNumProcesses,
                                                                    0, &num_procs);

                                                                for(uint32_t i = 0;
                                                                    i < num_procs; i++)
                                                                {
                                                                    ProcessRef ref;
                                                                    if(node->GetObject(
                                                                           kRPVControllerNodeProcessIndexed,
                                                                           i,
                                                                           ref.GetHandleAddress()) ==
                                                                           kRocProfVisResultSuccess &&
                                                                       ref.IsValid())
                                                                    {
                                                                        uint64_t proc_id =
                                                                            0;
                                                                        ref->GetUInt64(
                                                                            kRPVControllerProcessId,
                                                                            0, &proc_id);

                                                                        if(proc_id ==
                                                                           process_id)
                                                                        {
                                                                            uint64_t
                                                                                num_streams =
                                                                                    0;
                                                                            ref->GetUInt64(
                                                                                kRPVControllerProcessNumStreams,
                                                                                0,
                                                                                &num_streams);
                                                                            ref->SetUInt64(
                                                                                kRPVControllerProcessNumStreams,
                                                                                0,
                                                                                num_streams +
                                                                                    1);
                                                                            ref->SetObject(
                                                                                kRPVControllerProcessStreamIndexed,
                                                                                num_streams,
                                                                                (rocprofvis_handle_t*)
                                                                                    stream);
                                                                            stream =
                                                                                nullptr;
                                                                            break;
                                                                        }
                                                                    }
                                                                }

                                                                break;
                                                            }
                                                        }
                                                        ROCPROFVIS_ASSERT(!stream);
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }

                                rocprofvis_dm_delete_table_at(m_dm_handle, table_id);
                            }

                            rocprofvis_db_future_free(future);
                        }
                    }
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
    future.Set(JobSystem::Get().IssueJob([this, filepath]() -> rocprofvis_result_t
        {
            rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
            if(filepath.find(".rpd", filepath.size() - 4) != std::string::npos || 
                filepath.find(".db", filepath.size() - 3) != std::string::npos)
            {
                result = LoadRocpd(filepath.c_str());
            }
#ifdef COMPUTE_UI_SUPPORT
            else if(filepath.find(".csv", filepath.size() - 4 != std::string::npos))
            {
                m_compute_trace = new ComputeTrace();
                ROCPROFVIS_ASSERT(m_compute_trace);
                result = m_compute_trace->Load(filepath.c_str());
            }
#endif
#ifdef JSON_SUPPORT
            else if(filepath.find(".json", filepath.size() - 5) != std::string::npos)
            {
                result = LoadJson(filepath.c_str());
            }
#endif
            else
            {
                result = kRocProfVisResultInvalidArgument;
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
    future.Set(JobSystem::Get().IssueJob([&event, &array, property, dm_handle]() -> rocprofvis_result_t {
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


rocprofvis_result_t
Trace::AsyncFetch(rocprofvis_property_t property, Future& future, Array& array,
                  uint64_t index, uint64_t count)
{
    rocprofvis_result_t error = kRocProfVisResultUnknownError;

    future.Set(JobSystem::Get().IssueJob(
        [this, property, &array, index]() -> rocprofvis_result_t {
            rocprofvis_result_t result = kRocProfVisResultUnknownError;

            switch(property)
            {
                case kRPVControllerEventIndexed:
                {
                    // Todo: implement this function 
                    // result = Event::FetchSingleEvent(event_id, array, m_dm_handle);
                    break;
                }
                case kRPVControllerEventDataExtDataIndexed:
                {
                    const uint64_t& event_id = index;
                    result = Event::FetchDataModelExtendedDataProperty(event_id, array,
                                                                       m_dm_handle);
                    break;
                }
                case kRPVControllerEventDataCallStackIndexed:
                {
                    const uint64_t& event_id = index;
                    result = Event::FetchDataModelStackTraceProperty(event_id, array,
                                                                     m_dm_handle);
                    break;
                }
                case kRPVControllerEventDataFlowControlIndexed:
                {
                    const uint64_t& event_id = index;
                    result = Event::FetchDataModelFlowTraceProperty(event_id, array,
                                                                    m_dm_handle);
                    break;
                }
                default:
                {
                    result = kRocProfVisResultInvalidArgument;
                    ROCPROFVIS_ASSERT_MSG(false,
                                          "Invalid property for Trace::AsyncFetch");
                    break;
                }
            }

            return result;
        }));

    if(future.IsValid())
    {
        error = kRocProfVisResultSuccess;
    }

    return error;
}

rocprofvis_result_t
Trace::AsyncFetch(Table& table, Future& future, Array& array,
    uint64_t index, uint64_t count)
{
    rocprofvis_result_t error = kRocProfVisResultUnknownError;
    rocprofvis_dm_trace_t dm_handle = m_dm_handle;

    future.Set(JobSystem::Get().IssueJob([&table, &array, index, count, dm_handle]() -> rocprofvis_result_t {
                              rocprofvis_result_t result = kRocProfVisResultUnknownError;
                              result = table.Fetch(dm_handle, index, count, array);
                              return result;
                          }));

    if(future.IsValid())
    {
        error = kRocProfVisResultSuccess;
    }

    return error;
}

rocprofvis_result_t
Trace::AsyncFetch(Table& table, Arguments& args, Future& future, Array& array)
{
    rocprofvis_result_t   error     = kRocProfVisResultUnknownError;
    rocprofvis_dm_trace_t dm_handle = m_dm_handle;

    future.Set(JobSystem::Get().IssueJob([&table, dm_handle, &args, &array]() -> rocprofvis_result_t {
            rocprofvis_result_t result = kRocProfVisResultUnknownError;
            result = table.Setup(dm_handle, args);
            if (result == kRocProfVisResultSuccess)
            {
                uint64_t start_index = 0;
                uint64_t start_count = 0;
                if(result == kRocProfVisResultSuccess)
                {
                    result = args.GetUInt64(kRPVControllerTableArgsStartIndex, 0,
                                            &start_index);
                }
                if(result == kRocProfVisResultSuccess)
                {
                    result = args.GetUInt64(kRPVControllerTableArgsStartCount, 0,
                                            &start_count);
                }
                result = table.Fetch(dm_handle, start_index, start_count, array);
            }
            return result;
        }));

    if(future.IsValid())
    {
        error = kRocProfVisResultSuccess;
    }

    return error;
}

#ifdef COMPUTE_UI_SUPPORT
rocprofvis_result_t
Trace::AsyncFetch(Plot& plot, Arguments& args, Future& future, Array& array)
{
    rocprofvis_result_t error = kRocProfVisResultUnknownError;
    rocprofvis_dm_trace_t dm_handle = m_dm_handle;

    future.Set(JobSystem::Get().IssueJob([&plot, dm_handle, &args, &array]() -> rocprofvis_result_t {
            rocprofvis_result_t result = kRocProfVisResultUnknownError;
            result = plot.Setup(dm_handle, args);
            if (result == kRocProfVisResultSuccess)
            {
                result = plot.Fetch(dm_handle, 0, 0, array);
            }
            return result;
        }));

    if(future.IsValid())
    {
        error = kRocProfVisResultSuccess;
    }

    return error;
}
#endif

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
            {
                *value = m_nodes.size();
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerNodeIndexed:
            case kRPVControllerTimeline:
            case kRPVControllerTrackIndexed:
            case kRPVControllerTrackById:
            case kRPVControllerEventTable:
            case kRPVControllerSampleTable:
            case kRPVControllerAnalysisViewIndexed:
            case kRPVControllerNotifySelected:
#ifdef COMPUTE_UI_SUPPORT
            case kRPVControllerComputeTrace:
#endif
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
        case kRPVControllerTrackById:
        case kRPVControllerEventTable:
        case kRPVControllerSampleTable:
        case kRPVControllerAnalysisViewIndexed:
        case kRPVControllerNotifySelected:
#ifdef COMPUTE_UI_SUPPORT
        case kRPVControllerComputeTrace:
#endif
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
                *value = (rocprofvis_handle_t*)m_event_table;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerSampleTable:
            {
                *value = (rocprofvis_handle_t*)m_sample_table;
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
            case kRPVControllerTrackById:
            {
                result = kRocProfVisResultOutOfRange;
                for (auto* track : m_tracks)
                {
                    uint64_t track_id = 0;
                    if (track->GetUInt64(kRPVControllerTrackId, 0, &track_id) == kRocProfVisResultSuccess && track_id == index)
                    {
                        *value = (rocprofvis_handle_t*)track;
                        result = kRocProfVisResultSuccess;
                        break;
                    }
                }
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
#ifdef COMPUTE_UI_SUPPORT
            case kRPVControllerComputeTrace:
            {
                *value = (rocprofvis_handle_t*)m_compute_trace;
                result = kRocProfVisResultSuccess;
                break;
            }
#endif
            case kRPVControllerNumNodes:
            case kRPVControllerNodeIndexed:
            {
                if(index < m_nodes.size())
                {
                    *value = (rocprofvis_handle_t*)m_nodes[index];
                    result = kRocProfVisResultSuccess;
                }
                else
                {
                    result = kRocProfVisResultOutOfRange;
                }
                break;
            }
            case kRPVControllerNumTracks:
            case kRPVControllerId:
            case kRPVControllerNumAnalysisView:
            case kRPVControllerNotifySelected:
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
        case kRPVControllerTrackById:
        case kRPVControllerEventTable:
        case kRPVControllerSampleTable:
        case kRPVControllerAnalysisViewIndexed:
        case kRPVControllerNotifySelected:
#ifdef COMPUTE_UI_SUPPORT
        case kRPVControllerComputeTrace:
#endif
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
        case kRPVControllerNotifySelected:
        {
            if(value > 0 && m_mem_mgmt != nullptr)
            {
                m_mem_mgmt->Configure(2.0);
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
            if (m_nodes.size() != value)
            {
                for (uint32_t i = value; i < m_nodes.size(); i++)
                {
                    delete m_nodes[i];
                    m_nodes[i] = nullptr;
                }
                m_nodes.resize(value);
                result = m_nodes.size() == value ? kRocProfVisResultSuccess : kRocProfVisResultMemoryAllocError;
            }
            else
            {
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerTimeline:
        case kRPVControllerEventTable:
        case kRPVControllerSampleTable:
        case kRPVControllerAnalysisViewIndexed:
        case kRPVControllerTrackIndexed:
        case kRPVControllerTrackById:
        case kRPVControllerNodeIndexed:
#ifdef COMPUTE_UI_SUPPORT
        case kRPVControllerComputeTrace:
#endif
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
        case kRPVControllerTrackById:
        case kRPVControllerEventTable:
        case kRPVControllerSampleTable:
        case kRPVControllerAnalysisViewIndexed:
        case kRPVControllerNotifySelected:
#ifdef COMPUTE_UI_SUPPORT
        case kRPVControllerComputeTrace:
#endif
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
                SystemTableRef table(value);
                if(table.IsValid())
                {
                    m_event_table = table.Get();
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVControllerSampleTable:
            {
                SystemTableRef table(value);
                if(table.IsValid())
                {
                    m_sample_table = table.Get();
                    result = kRocProfVisResultSuccess;
                }
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
            case kRPVControllerNodeIndexed:
            {
                NodeRef node(value);
                if(node.IsValid())
                {
                    if(index < m_nodes.size())
                    {
                        m_nodes[index] = node.Get();
                        result = kRocProfVisResultSuccess;
                    }
                    else
                    {
                        result = kRocProfVisResultOutOfRange;
                    }
                }
                break;
            }
#ifdef COMPUTE_UI_SUPPORT
            case kRPVControllerComputeTrace:
#endif
            case kRPVControllerNumNodes:
            case kRPVControllerNumTracks:
            case kRPVControllerId:
            case kRPVControllerNumAnalysisView:
            case kRPVControllerTrackById:
            case kRPVControllerNotifySelected:
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
        case kRPVControllerTrackById:
        case kRPVControllerEventTable:
        case kRPVControllerSampleTable:
        case kRPVControllerAnalysisViewIndexed:
        case kRPVControllerNotifySelected:
#ifdef COMPUTE_UI_SUPPORT
        case kRPVControllerComputeTrace:
#endif
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
