// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_controller_trace.h"
#include "rocprofvis_controller_trace_compute.h"
#include "rocprofvis_controller_timeline.h"
#include "rocprofvis_controller_track.h"
#include "rocprofvis_controller_reference.h"
#include "rocprofvis_controller_future.h"
#include "rocprofvis_controller_event.h"
#include "rocprofvis_controller_sample.h"
#include "rocprofvis_controller_graph.h"
#include "rocprofvis_controller_table_system.h"
#include "rocprofvis_controller_plot.h"
#include "rocprofvis_controller_id.h"
#include "rocprofvis_controller_json_trace.h"
#include "rocprofvis_controller_arguments.h"
#include "rocprofvis_core_assert.h"

#include <cfloat>
#include <cstdint>
#include <cstring>

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

Trace::Trace()
: m_id(s_trace_id.GetNextId())
, m_timeline(nullptr)
, m_event_table(nullptr)
, m_sample_table(nullptr)
, m_dm_handle(nullptr)
, m_compute_trace(nullptr)
{
    m_event_table = new SystemTable(0);
    ROCPROFVIS_ASSERT(m_event_table);

    m_sample_table = new SystemTable(1);
    ROCPROFVIS_ASSERT(m_sample_table);
}

Trace::~Trace()
{
    delete m_timeline;
    delete m_event_table;
    delete m_sample_table;
    for (Track* track : m_tracks)
    {
        delete track;
    }
    if(m_dm_handle)
        rocprofvis_dm_delete_trace(m_dm_handle);

    if(m_compute_trace)
        delete m_compute_trace;

    for (auto* node : m_nodes)
    {
        delete node;
    }
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
                            Track* track = new Track(type, track_id++, nullptr);
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

                            uint64_t  graph_id = 0;
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
                                        new Track(type, track_id, dm_track_handle);
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

                                        uint32_t index = m_tracks.size();
                                        m_tracks.push_back(track);
                                        if(m_tracks.size() != (index + 1))
                                        {
                                            delete track;
                                            track  = nullptr;
                                            result = kRocProfVisResultMemoryAllocError;
                                            break;
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
                                                    kRPVControllerTimelineNumGraphs, 0,
                                                    graph_id);
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
                            }

                            // This block is asynchronously loading full trace
                            // todo : remove following block after  UI implemented segmented loading
                            // or : use this code for preloading some segments at the load time. start and end has to be calculated considering preloaded segment boundaries  
                            /*
                            for(int i = 0; i < num_tracks; i++)
                            {                               
                                RocProfVis::Controller::Array* array = (RocProfVis::Controller::Array*)rocprofvis_controller_array_alloc(32);
                                RocProfVis::Controller::Future* future = (RocProfVis::Controller::Future*) rocprofvis_controller_future_alloc();
                                double start, end;
                                m_tracks[i]->GetDouble(kRPVControllerTrackMinTimestamp, 0, &start);
                                m_tracks[i]->GetDouble(kRPVControllerTrackMaxTimestamp, 0, &end);
                                result = AsyncFetch(*m_tracks[i], *future, *array, start, end);
                                result = rocprofvis_controller_future_wait(
                                    (rocprofvis_controller_future_t*) future, FLT_MAX);
                                rocprofvis_controller_future_free(
                                    (rocprofvis_controller_future_t*) future);
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
                                                            else if (columns[j] == kRPVControllerNodeId)
                                                            {
                                                                char* end = nullptr;
                                                                node->SetUInt64(
                                                                    kRPVControllerNodeId,
                                                                    0, std::strtoull(prop_string, &end, 10));
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
                                                                    kRPVControllerProcessorTypeIndex)
                                                            {
                                                                char* end = nullptr;
                                                                proc->SetUInt64(
                                                                    columns[j],
                                                                    0,
                                                                    std::strtoull(
                                                                        prop_string, &end,
                                                                        10));
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
                                                                proc->SetUInt64(
                                                                    columns[j], 0,
                                                                    std::strtoull(
                                                                        prop_string, &end,
                                                                        10));
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
            if(filepath.find(".rpd", filepath.size() - 4) != std::string::npos || 
                filepath.find(".db", filepath.size() - 3) != std::string::npos)
            {
                result = LoadRocpd(filepath.c_str());
            }
            else if(filepath.find(".csv", filepath.size() - 4 != std::string::npos))
            {
                m_compute_trace = new ComputeTrace();
                ROCPROFVIS_ASSERT(m_compute_trace);
                result = m_compute_trace->Load(filepath.c_str());
            }
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

rocprofvis_result_t
Trace::AsyncFetch(Table& table, Future& future, Array& array,
    uint64_t index, uint64_t count)
{
    rocprofvis_result_t error = kRocProfVisResultUnknownError;
    rocprofvis_dm_trace_t dm_handle = m_dm_handle;

    future.Set(std::async(std::launch::async,
                   [&table, &array, index, count, dm_handle]() -> rocprofvis_result_t {
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

    future.Set(std::async(
        std::launch::async, [&table, dm_handle, &args, &array]() -> rocprofvis_result_t {
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

rocprofvis_result_t
Trace::AsyncFetch(Plot& plot, Arguments& args, Future& future, Array& array)
{
    rocprofvis_result_t error = kRocProfVisResultUnknownError;
    rocprofvis_dm_trace_t dm_handle = m_dm_handle;

    future.Set(std::async(
        std::launch::async, [&plot, dm_handle, &args, &array]() -> rocprofvis_result_t {
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
                    result = m_timeline->GetUInt64(property, 0, value);
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
            case kRPVControllerSampleTable:
            case kRPVControllerAnalysisViewIndexed:
            case kRPVControllerComputeTrace:
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
        case kRPVControllerSampleTable:
        case kRPVControllerAnalysisViewIndexed:
        case kRPVControllerComputeTrace:
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
            case kRPVControllerComputeTrace:
            {
                *value = (rocprofvis_handle_t*)m_compute_trace;
                result = kRocProfVisResultSuccess;
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
        case kRPVControllerSampleTable:
        case kRPVControllerAnalysisViewIndexed:
        case kRPVControllerComputeTrace:
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
        case kRPVControllerSampleTable:
        case kRPVControllerAnalysisViewIndexed:
        case kRPVControllerTrackIndexed:
        case kRPVControllerNodeIndexed:
        case kRPVControllerComputeTrace:
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
        case kRPVControllerSampleTable:
        case kRPVControllerAnalysisViewIndexed:
        case kRPVControllerComputeTrace:
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
            case kRPVControllerComputeTrace:
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
        case kRPVControllerSampleTable:
        case kRPVControllerAnalysisViewIndexed:
        case kRPVControllerComputeTrace:
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

Node::Node()
{
}

Node::~Node()
{
    for (auto* proc : m_processors)
    {
        delete proc;
    }
}

rocprofvis_controller_object_type_t Node::GetType(void)
{
    return kRPVControllerObjectTypeNode;
}

rocprofvis_result_t Node::GetUInt64(rocprofvis_property_t property, uint64_t index,
                                uint64_t* value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            case kRPVControllerNodeId:
            {
                *value = m_id;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerNodeNumProcessors:
            {
                *value = m_processors.size();
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerNodeNumProcesses:
            {
                *value = m_processes.size();
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerNodeHostName:
            case kRPVControllerNodeDomainName:
            case kRPVControllerNodeOSName:
            case kRPVControllerNodeOSRelease:
            case kRPVControllerNodeOSVersion:
            case kRPVControllerNodeHardwareName:
            case kRPVControllerNodeMachineId:
            case kRPVControllerNodeProcessorIndexed:
            case kRPVControllerNodeProcessIndexed:
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

rocprofvis_result_t Node::GetDouble(rocprofvis_property_t property, uint64_t index,
                                double* value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerNodeId:
        case kRPVControllerNodeHostName:
        case kRPVControllerNodeDomainName:
        case kRPVControllerNodeOSName:
        case kRPVControllerNodeOSRelease:
        case kRPVControllerNodeOSVersion:
        case kRPVControllerNodeHardwareName:
        case kRPVControllerNodeMachineId:
        case kRPVControllerNodeNumProcessors:
        case kRPVControllerNodeProcessorIndexed:
        case kRPVControllerNodeNumProcesses:
        case kRPVControllerNodeProcessIndexed:
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

rocprofvis_result_t Node::GetObject(rocprofvis_property_t property, uint64_t index,
                                rocprofvis_handle_t** value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            case kRPVControllerNodeProcessorIndexed:
            {
                if(m_processors.size() > index)
                {
                    *value = (rocprofvis_handle_t*)m_processors[index];
                    result              = kRocProfVisResultSuccess;
                }
                else
                {
                    result = kRocProfVisResultOutOfRange;
                }
                break;
            }
            case kRPVControllerNodeProcessIndexed:
            {
                if(m_processes.size() > index)
                {
                    *value = (rocprofvis_handle_t*)m_processes[index];
                    result             = kRocProfVisResultSuccess;
                }
                else
                {
                    result = kRocProfVisResultOutOfRange;
                }
                break;
            }
            case kRPVControllerNodeId:
            case kRPVControllerNodeHostName:
            case kRPVControllerNodeDomainName:
            case kRPVControllerNodeOSName:
            case kRPVControllerNodeOSRelease:
            case kRPVControllerNodeOSVersion:
            case kRPVControllerNodeHardwareName:
            case kRPVControllerNodeMachineId:
            case kRPVControllerNodeNumProcessors:
            case kRPVControllerNodeNumProcesses:
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

rocprofvis_result_t Node::GetString(rocprofvis_property_t property, uint64_t index,
                                char* value, uint32_t* length)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerNodeHostName:
        {
            if (value && length && *length)
            {
                strncpy(value, m_host_name.c_str(), *length);
                result = kRocProfVisResultSuccess;
            }
            else if (length)
            {
                *length = m_host_name.length();
                result  = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerNodeDomainName:
        {
            if(value && length && *length)
            {
                strncpy(value, m_domain_name.c_str(), *length);
                result = kRocProfVisResultSuccess;
            }
            else if(length)
            {
                *length = m_domain_name.length();
                result  = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerNodeOSName:
        {
            if(value && length && *length)
            {
                strncpy(value, m_os_name.c_str(), *length);
                result = kRocProfVisResultSuccess;
            }
            else if(length)
            {
                *length = m_os_name.length();
                result  = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerNodeOSRelease:
        {
            if(value && length && *length)
            {
                strncpy(value, m_os_release.c_str(), *length);
                result = kRocProfVisResultSuccess;
            }
            else if(length)
            {
                *length = m_os_release.length();
                result  = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerNodeOSVersion:
        {
            if(value && length && *length)
            {
                strncpy(value, m_os_version.c_str(), *length);
                result = kRocProfVisResultSuccess;
            }
            else if(length)
            {
                *length = m_os_version.length();
                result  = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerNodeHardwareName:
        {
            if(value && length && *length)
            {
                strncpy(value, m_hardware_name.c_str(), *length);
                result = kRocProfVisResultSuccess;
            }
            else if(length)
            {
                *length = m_hardware_name.length();
                result  = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerNodeMachineId:
        {
            if(value && length && *length)
            {
                strncpy(value, m_machine_id.c_str(), *length);
                result = kRocProfVisResultSuccess;
            }
            else if(length)
            {
                *length = m_machine_id.length();
                result  = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerNodeId:
        case kRPVControllerNodeNumProcessors:
        case kRPVControllerNodeProcessorIndexed:
        case kRPVControllerNodeNumProcesses:
        case kRPVControllerNodeProcessIndexed:
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

rocprofvis_result_t Node::SetUInt64(rocprofvis_property_t property, uint64_t index,
                                uint64_t value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerNodeId:
        {
            m_id = value;
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerNodeNumProcessors:
        {
            if (m_processors.size() < value)
            {
                m_processors.resize(value);
            }
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerNodeNumProcesses:
        {
            if(m_processes.size() < value)
            {
                m_processes.resize(value);
            }
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerNodeHostName:
        case kRPVControllerNodeDomainName:
        case kRPVControllerNodeOSName:
        case kRPVControllerNodeOSRelease:
        case kRPVControllerNodeOSVersion:
        case kRPVControllerNodeHardwareName:
        case kRPVControllerNodeMachineId:
        case kRPVControllerNodeProcessorIndexed:
        case kRPVControllerNodeProcessIndexed:
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

rocprofvis_result_t Node::SetDouble(rocprofvis_property_t property, uint64_t index,
                                double value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerNodeId:
        case kRPVControllerNodeHostName:
        case kRPVControllerNodeDomainName:
        case kRPVControllerNodeOSName:
        case kRPVControllerNodeOSRelease:
        case kRPVControllerNodeOSVersion:
        case kRPVControllerNodeHardwareName:
        case kRPVControllerNodeMachineId:
        case kRPVControllerNodeNumProcessors:
        case kRPVControllerNodeProcessorIndexed:
        case kRPVControllerNodeNumProcesses:
        case kRPVControllerNodeProcessIndexed:
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

rocprofvis_result_t Node::SetObject(rocprofvis_property_t property, uint64_t index,
                                rocprofvis_handle_t* value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerNodeProcessorIndexed:
        {
            if(m_processors.size() > index)
            {
                ProcessorRef ref(value);
                if(ref.IsValid())
                {
                    m_processors[index] = ref.Get();
                    result              = kRocProfVisResultSuccess;
                }
            }
            else
            {
                result = kRocProfVisResultOutOfRange;
            }
            break;
        }
        case kRPVControllerNodeProcessIndexed:
        {
            if(m_processes.size() > index)
            {
                ProcessRef ref(value);
                if(ref.IsValid())
                {
                    m_processes[index] = ref.Get();
                    result              = kRocProfVisResultSuccess;
                }
            }
            else
            {
                result = kRocProfVisResultOutOfRange;
            }
            break;
        }
        case kRPVControllerNodeId:
        case kRPVControllerNodeHostName:
        case kRPVControllerNodeDomainName:
        case kRPVControllerNodeOSName:
        case kRPVControllerNodeOSRelease:
        case kRPVControllerNodeOSVersion:
        case kRPVControllerNodeHardwareName:
        case kRPVControllerNodeMachineId:
        case kRPVControllerNodeNumProcessors:
        case kRPVControllerNodeNumProcesses:
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

rocprofvis_result_t Node::SetString(rocprofvis_property_t property, uint64_t index,
                                char const* value, uint32_t length)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value && length)
    {
        switch(property)
        {
            case kRPVControllerNodeHostName:
            {
                m_host_name = value;
                result      = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerNodeDomainName:
            {
                m_domain_name = value;
                result        = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerNodeOSName:
            {
                m_os_name = value;
                result    = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerNodeOSRelease:
            {
                m_os_release = value;
                result       = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerNodeOSVersion:
            {
                m_os_version = value;
                result       = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerNodeHardwareName:
            {
                m_hardware_name = value;
                result          = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerNodeMachineId:
            {
                m_machine_id = value;
                result       = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerNodeId:
            case kRPVControllerNodeNumProcessors:
            case kRPVControllerNodeProcessorIndexed:
            case kRPVControllerNodeNumProcesses:
            case kRPVControllerNodeProcessIndexed:
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

Processor::Processor()
{

}

Processor::~Processor()
{

}

rocprofvis_controller_object_type_t Processor::GetType(void)
{
    return kRPVControllerObjectTypeProcessor;
}

rocprofvis_result_t Processor::GetUInt64(rocprofvis_property_t property, uint64_t index,
                                uint64_t* value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            case kRPVControllerProcessorId:
            {
                *value = m_id;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerProcessorTypeIndex:
            {
                *value = m_type_index;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerProcessorNodeId:
            {
                *value = m_node_id;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerProcessorName:
            case kRPVControllerProcessorModelName:
            case kRPVControllerProcessorUserName:
            case kRPVControllerProcessorVendorName:
            case kRPVControllerProcessorProductName:
            case kRPVControllerProcessorExtData:
            case kRPVControllerProcessorUUID:
            case kRPVControllerProcessorType:
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

rocprofvis_result_t Processor::GetDouble(rocprofvis_property_t property, uint64_t index,
                                double* value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerProcessorId:
        case kRPVControllerProcessorName:
        case kRPVControllerProcessorModelName:
        case kRPVControllerProcessorUserName:
        case kRPVControllerProcessorVendorName:
        case kRPVControllerProcessorProductName:
        case kRPVControllerProcessorExtData: 
        case kRPVControllerProcessorUUID:
        case kRPVControllerProcessorType:
        case kRPVControllerProcessorTypeIndex:
        case kRPVControllerProcessorNodeId:
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

rocprofvis_result_t Processor::GetObject(rocprofvis_property_t property, uint64_t index,
                                rocprofvis_handle_t** value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerProcessorId:
        case kRPVControllerProcessorName:
        case kRPVControllerProcessorModelName:
        case kRPVControllerProcessorUserName:
        case kRPVControllerProcessorVendorName:
        case kRPVControllerProcessorProductName:
        case kRPVControllerProcessorExtData: 
        case kRPVControllerProcessorUUID:
        case kRPVControllerProcessorType:
        case kRPVControllerProcessorTypeIndex:
        case kRPVControllerProcessorNodeId:
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

rocprofvis_result_t Processor::GetString(rocprofvis_property_t property, uint64_t index,
                                char* value, uint32_t* length)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerProcessorName:
        {
            if(value && length && *length)
            {
                strncpy(value, m_name.c_str(), *length);
                result = kRocProfVisResultSuccess;
            }
            else if(length)
            {
                *length = m_name.length();
                result  = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerProcessorModelName:
        {
            if(value && length && *length)
            {
                strncpy(value, m_model_name.c_str(), *length);
                result = kRocProfVisResultSuccess;
            }
            else if(length)
            {
                *length = m_model_name.length();
                result  = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerProcessorUserName:
        {
            if(value && length && *length)
            {
                strncpy(value, m_user_name.c_str(), *length);
                result = kRocProfVisResultSuccess;
            }
            else if(length)
            {
                *length = m_user_name.length();
                result  = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerProcessorVendorName:
        {
            if(value && length && *length)
            {
                strncpy(value, m_vendor_name.c_str(), *length);
                result = kRocProfVisResultSuccess;
            }
            else if(length)
            {
                *length = m_vendor_name.length();
                result  = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerProcessorProductName:
        {
            if(value && length && *length)
            {
                strncpy(value, m_product_name.c_str(), *length);
                result = kRocProfVisResultSuccess;
            }
            else if(length)
            {
                *length = m_product_name.length();
                result  = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerProcessorExtData:
        {
            if(value && length && *length)
            {
                strncpy(value, m_ext_data.c_str(), *length);
                result = kRocProfVisResultSuccess;
            }
            else if(length)
            {
                *length = m_ext_data.length();
                result  = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerProcessorUUID:
        {
            if(value && length && *length)
            {
                strncpy(value, m_uuid.c_str(), *length);
                result = kRocProfVisResultSuccess;
            }
            else if(length)
            {
                *length = m_uuid.length();
                result  = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerProcessorType:
        {
            if(value && length && *length)
            {
                strncpy(value, m_type.c_str(), *length);
                result = kRocProfVisResultSuccess;
            }
            else if(length)
            {
                *length = m_type.length();
                result  = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerProcessorId:
        case kRPVControllerProcessorTypeIndex:
        case kRPVControllerProcessorNodeId:
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

rocprofvis_result_t Processor::SetUInt64(rocprofvis_property_t property, uint64_t index,
                                uint64_t value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerProcessorId:
        {
            m_id = value;
            result    = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerProcessorTypeIndex:
        {
            m_type_index = value;
            result    = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerProcessorNodeId:
        {
            m_node_id = value;
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerProcessorName:
        case kRPVControllerProcessorModelName:
        case kRPVControllerProcessorUserName:
        case kRPVControllerProcessorVendorName:
        case kRPVControllerProcessorProductName:
        case kRPVControllerProcessorExtData:
        case kRPVControllerProcessorUUID:
        case kRPVControllerProcessorType:
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

rocprofvis_result_t Processor::SetDouble(rocprofvis_property_t property, uint64_t index,
                                double value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerProcessorId:
        case kRPVControllerProcessorName:
        case kRPVControllerProcessorModelName:
        case kRPVControllerProcessorUserName:
        case kRPVControllerProcessorVendorName:
        case kRPVControllerProcessorProductName:
        case kRPVControllerProcessorExtData: 
        case kRPVControllerProcessorUUID:
        case kRPVControllerProcessorType:
        case kRPVControllerProcessorTypeIndex:
        case kRPVControllerProcessorNodeId:
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

rocprofvis_result_t Processor::SetObject(rocprofvis_property_t property, uint64_t index,
                                rocprofvis_handle_t* value)
    
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerProcessorId:
        case kRPVControllerProcessorName:
        case kRPVControllerProcessorModelName:
        case kRPVControllerProcessorUserName:
        case kRPVControllerProcessorVendorName:
        case kRPVControllerProcessorProductName:
        case kRPVControllerProcessorExtData: 
        case kRPVControllerProcessorUUID:
        case kRPVControllerProcessorType:
        case kRPVControllerProcessorTypeIndex:
        case kRPVControllerProcessorNodeId:
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

rocprofvis_result_t Processor::SetString(rocprofvis_property_t property, uint64_t index,
                                char const* value, uint32_t length)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value && length)
    {
        switch(property)
        {
            case kRPVControllerProcessorName:
            {
                m_name = value;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerProcessorModelName:
            {
                m_model_name = value;
                result       = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerProcessorUserName:
            {
                m_user_name = value;
                result      = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerProcessorVendorName:
            {
                m_vendor_name = value;
                result        = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerProcessorProductName:
            {
                m_product_name = value;
                result         = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerProcessorExtData:
            {
                m_ext_data = value;
                result     = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerProcessorUUID:
            {
                m_uuid = value;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerProcessorType:
            {
                m_type = value;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerProcessorId:
            case kRPVControllerProcessorTypeIndex:
            case kRPVControllerProcessorNodeId:
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

Process::Process()
{

}

Process::~Process()
{

}

rocprofvis_controller_object_type_t Process::GetType(void)
{
    return kRPVControllerObjectTypeProcess;
}

rocprofvis_result_t Process::GetUInt64(rocprofvis_property_t property, uint64_t index,
                                uint64_t* value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            case kRPVControllerProcessId:
            {
                *value = m_id;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerProcessNodeId:
            {
                *value = m_node_id;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerProcessInitTime:
            case kRPVControllerProcessFinishTime:
            case kRPVControllerProcessStartTime:
            case kRPVControllerProcessEndTime:
            case kRPVControllerProcessCommand:
            case kRPVControllerProcessEnvironment:
            case kRPVControllerProcessExtData:
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

rocprofvis_result_t Process::GetDouble(rocprofvis_property_t property, uint64_t index,
                                double* value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            case kRPVControllerProcessInitTime:
            {
                *value = m_init_time;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerProcessFinishTime:
            {
                *value = m_finish_time;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerProcessStartTime:
            {
                *value = m_start_time;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerProcessEndTime:
            {
                *value = m_end_time;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerProcessId:
            case kRPVControllerProcessNodeId:
            case kRPVControllerProcessCommand:
            case kRPVControllerProcessEnvironment:
            case kRPVControllerProcessExtData:
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

rocprofvis_result_t Process::GetObject(rocprofvis_property_t property, uint64_t index,
                                rocprofvis_handle_t** value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerProcessId:
        case kRPVControllerProcessNodeId:
        case kRPVControllerProcessInitTime:
        case kRPVControllerProcessFinishTime:
        case kRPVControllerProcessStartTime:
        case kRPVControllerProcessEndTime:
        case kRPVControllerProcessCommand:
        case kRPVControllerProcessEnvironment:
        case kRPVControllerProcessExtData:
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

rocprofvis_result_t Process::GetString(rocprofvis_property_t property, uint64_t index,
                                char* value, uint32_t* length)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerProcessCommand:
        {
            if(value && length && *length)
            {
                strncpy(value, m_command.c_str(), *length);
                result = kRocProfVisResultSuccess;
            }
            else if(length)
            {
                *length = m_command.length();
                result  = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerProcessEnvironment:
        {
            if(value && length && *length)
            {
                strncpy(value, m_environment.c_str(), *length);
                result = kRocProfVisResultSuccess;
            }
            else if(length)
            {
                *length = m_environment.length();
                result  = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerProcessExtData:
        {
            if(value && length && *length)
            {
                strncpy(value, m_ext_data.c_str(), *length);
                result = kRocProfVisResultSuccess;
            }
            else if(length)
            {
                *length = m_ext_data.length();
                result  = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerProcessId:
        case kRPVControllerProcessNodeId:
        case kRPVControllerProcessInitTime:
        case kRPVControllerProcessFinishTime:
        case kRPVControllerProcessStartTime:
        case kRPVControllerProcessEndTime:
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

rocprofvis_result_t Process::SetUInt64(rocprofvis_property_t property, uint64_t index,
                                uint64_t value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerProcessId:
        {
            m_id   = value;
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerProcessNodeId:
        {
            m_node_id = value;
            result    = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerProcessInitTime:
        case kRPVControllerProcessFinishTime:
        case kRPVControllerProcessStartTime:
        case kRPVControllerProcessEndTime:
        case kRPVControllerProcessCommand:
        case kRPVControllerProcessEnvironment:
        case kRPVControllerProcessExtData:
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

rocprofvis_result_t Process::SetDouble(rocprofvis_property_t property, uint64_t index,
                                double value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerProcessInitTime:
        {
            m_init_time = value;
            result     = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerProcessFinishTime:
        {
            m_finish_time = value;
            result     = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerProcessStartTime:
        {
            m_start_time = value;
            result     = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerProcessEndTime:
        {
            m_end_time = value;
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerProcessId:
        case kRPVControllerProcessNodeId:
        case kRPVControllerProcessCommand:
        case kRPVControllerProcessEnvironment:
        case kRPVControllerProcessExtData:
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

rocprofvis_result_t Process::SetObject(rocprofvis_property_t property, uint64_t index,
                                rocprofvis_handle_t* value)
    
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerProcessId:
        case kRPVControllerProcessNodeId:
        case kRPVControllerProcessInitTime:
        case kRPVControllerProcessFinishTime:
        case kRPVControllerProcessStartTime:
        case kRPVControllerProcessEndTime:
        case kRPVControllerProcessCommand:
        case kRPVControllerProcessEnvironment:
        case kRPVControllerProcessExtData:
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

rocprofvis_result_t Process::SetString(rocprofvis_property_t property, uint64_t index,
                                char const* value, uint32_t length)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value && length)
    {
        switch(property)
        {
            case kRPVControllerProcessCommand:
            {
                m_command = value;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerProcessEnvironment:
            {
                m_environment = value;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerProcessExtData:
            {
                m_ext_data = value;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerProcessId:
            case kRPVControllerProcessNodeId:
            case kRPVControllerProcessInitTime:
            case kRPVControllerProcessFinishTime:
            case kRPVControllerProcessStartTime:
            case kRPVControllerProcessEndTime:
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

Thread::Thread()
{

}

Thread::~Thread()
{

}

rocprofvis_controller_object_type_t Thread::GetType(void)
{
    return kRPVControllerObjectTypeThread;
}

rocprofvis_result_t Thread::GetUInt64(rocprofvis_property_t property, uint64_t index,
                                uint64_t* value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            case kRPVControllerThreadId:
            {
                *value = m_id;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerThreadNodeId:
            {
                *value = m_node_id;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerThreadProcessId:
            {
                *value = m_process_id;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerThreadParentId:
            {
                *value = m_parent_id;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerThreadTid:
            {
                *value = m_tid;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerThreadName:
            case kRPVControllerThreadExtData:
            case kRPVControllerThreadStartTime:
            case kRPVControllerThreadEndTime:
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

rocprofvis_result_t Thread::GetDouble(rocprofvis_property_t property, uint64_t index, double* value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            case kRPVControllerThreadStartTime:
            {
                *value = m_start_time;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerThreadEndTime:
            {
                *value = m_end_time;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerThreadId:
            case kRPVControllerThreadNodeId:
            case kRPVControllerThreadProcessId:
            case kRPVControllerThreadParentId:
            case kRPVControllerThreadTid:
            case kRPVControllerThreadName:
            case kRPVControllerThreadExtData:
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

rocprofvis_result_t Thread::GetObject(rocprofvis_property_t property, uint64_t index,
                  rocprofvis_handle_t** value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerThreadId:
        case kRPVControllerThreadNodeId:
        case kRPVControllerThreadProcessId:
        case kRPVControllerThreadParentId:
        case kRPVControllerThreadTid:
        case kRPVControllerThreadName:
        case kRPVControllerThreadExtData:
        case kRPVControllerThreadStartTime:
        case kRPVControllerThreadEndTime:
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

rocprofvis_result_t Thread::GetString(rocprofvis_property_t property, uint64_t index, char* value,
                  uint32_t* length)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerThreadName:
        {
            if(value && length && *length)
            {
                strncpy(value, m_name.c_str(), *length);
                result = kRocProfVisResultSuccess;
            }
            else if(length)
            {
                *length = m_name.length();
                result  = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerThreadExtData:
        {
            if(value && length && *length)
            {
                strncpy(value, m_ext_data.c_str(), *length);
                result = kRocProfVisResultSuccess;
            }
            else if(length)
            {
                *length = m_ext_data.length();
                result  = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerThreadId:
        case kRPVControllerThreadNodeId:
        case kRPVControllerThreadProcessId:
        case kRPVControllerThreadParentId:
        case kRPVControllerThreadTid:
        case kRPVControllerThreadStartTime:
        case kRPVControllerThreadEndTime:
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

rocprofvis_result_t Thread::SetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerThreadId:
        {
            m_id  = value;
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerThreadNodeId:
        {
            m_node_id  = value;
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerThreadProcessId:
        {
            m_process_id  = value;
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerThreadParentId:
        {
            m_parent_id  = value;
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerThreadTid:
        {
            m_tid  = value;
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerThreadName:
        case kRPVControllerThreadExtData:
        case kRPVControllerThreadStartTime:
        case kRPVControllerThreadEndTime:
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

rocprofvis_result_t Thread::SetDouble(rocprofvis_property_t property, uint64_t index, double value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerThreadStartTime:
        {
            m_start_time  = value;
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerThreadEndTime:
        {
            m_end_time  = value;
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerThreadId:
        case kRPVControllerThreadNodeId:
        case kRPVControllerThreadProcessId:
        case kRPVControllerThreadParentId:
        case kRPVControllerThreadTid:
        case kRPVControllerThreadName:
        case kRPVControllerThreadExtData:
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

rocprofvis_result_t Thread::SetObject(rocprofvis_property_t property, uint64_t index,
                  rocprofvis_handle_t* value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerThreadId:
        case kRPVControllerThreadNodeId:
        case kRPVControllerThreadProcessId:
        case kRPVControllerThreadParentId:
        case kRPVControllerThreadTid:
        case kRPVControllerThreadName:
        case kRPVControllerThreadExtData:
        case kRPVControllerThreadStartTime:
        case kRPVControllerThreadEndTime:
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

rocprofvis_result_t Thread::SetString(rocprofvis_property_t property, uint64_t index, char const* value,
                  uint32_t length)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerThreadName:
        {
            m_name = value;
            result     = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerThreadExtData:
        {
            m_ext_data = value;
            result     = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerThreadId:
        case kRPVControllerThreadNodeId:
        case kRPVControllerThreadProcessId:
        case kRPVControllerThreadParentId:
        case kRPVControllerThreadTid:
        case kRPVControllerThreadStartTime:
        case kRPVControllerThreadEndTime:
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

Queue::Queue() {

}

Queue::~Queue() {

}

rocprofvis_controller_object_type_t Queue::GetType(void)
{
    return kRPVControllerObjectTypeQueue;
}

rocprofvis_result_t Queue::GetUInt64(rocprofvis_property_t property, uint64_t index,
                                uint64_t* value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            case kRPVControllerQueueId:
            {
                *value = m_id;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerQueueNodeId:
            {
                *value = m_node_id;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerQueueProcessId:
            {
                *value = m_process_id;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerQueueName:
            case kRPVControllerQueueExtData:
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

rocprofvis_result_t Queue::GetDouble(rocprofvis_property_t property, uint64_t index, double* value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            case kRPVControllerQueueId:
            case kRPVControllerQueueNodeId:
            case kRPVControllerQueueProcessId:
            case kRPVControllerQueueName:
            case kRPVControllerQueueExtData:
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

rocprofvis_result_t Queue::GetObject(rocprofvis_property_t property, uint64_t index,
                  rocprofvis_handle_t** value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerQueueId:
        case kRPVControllerQueueNodeId:
        case kRPVControllerQueueProcessId:
        case kRPVControllerQueueName:
        case kRPVControllerQueueExtData:
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

rocprofvis_result_t Queue::GetString(rocprofvis_property_t property, uint64_t index, char* value,
                  uint32_t* length)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerQueueName:
        {
            if(value && length && *length)
            {
                strncpy(value, m_name.c_str(), *length);
                result = kRocProfVisResultSuccess;
            }
            else if(length)
            {
                *length = m_name.length();
                result  = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerQueueExtData:
        {
            if(value && length && *length)
            {
                strncpy(value, m_ext_data.c_str(), *length);
                result = kRocProfVisResultSuccess;
            }
            else if(length)
            {
                *length = m_ext_data.length();
                result  = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerQueueId:
        case kRPVControllerQueueNodeId:
        case kRPVControllerQueueProcessId:
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

rocprofvis_result_t Queue::SetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerQueueId:
        {
            m_id  = value;
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerQueueNodeId:
        {
            m_node_id  = value;
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerQueueProcessId:
        {
            m_process_id  = value;
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerQueueName:
        case kRPVControllerQueueExtData:
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

rocprofvis_result_t Queue::SetDouble(rocprofvis_property_t property, uint64_t index, double value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerQueueId:
        case kRPVControllerQueueNodeId:
        case kRPVControllerQueueProcessId:
        case kRPVControllerQueueName:
        case kRPVControllerQueueExtData:
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

rocprofvis_result_t Queue::SetObject(rocprofvis_property_t property, uint64_t index,
                  rocprofvis_handle_t* value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerQueueId:
        case kRPVControllerQueueNodeId:
        case kRPVControllerQueueProcessId:
        case kRPVControllerQueueName:
        case kRPVControllerQueueExtData:
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

rocprofvis_result_t Queue::SetString(rocprofvis_property_t property, uint64_t index, char const* value,
                  uint32_t length)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerQueueName:
        {
            m_name = value;
            result     = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerQueueExtData:
        {
            m_ext_data = value;
            result     = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerQueueId:
        case kRPVControllerQueueNodeId:
        case kRPVControllerQueueProcessId:
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



Stream::Stream() {

}

Stream::~Stream() {

}

rocprofvis_controller_object_type_t Stream::GetType(void)
{
    return kRPVControllerObjectTypeStream;
}

rocprofvis_result_t Stream::GetUInt64(rocprofvis_property_t property, uint64_t index,
                                uint64_t* value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            case kRPVControllerStreamId:
            {
                *value = m_id;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerStreamNodeId:
            {
                *value = m_node_id;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerStreamProcessId:
            {
                *value = m_process_id;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerStreamName:
            case kRPVControllerStreamExtData:
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

rocprofvis_result_t Stream::GetDouble(rocprofvis_property_t property, uint64_t index, double* value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            case kRPVControllerStreamId:
            case kRPVControllerStreamNodeId:
            case kRPVControllerStreamProcessId:
            case kRPVControllerStreamName:
            case kRPVControllerStreamExtData:
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

rocprofvis_result_t Stream::GetObject(rocprofvis_property_t property, uint64_t index,
                  rocprofvis_handle_t** value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerStreamId:
        case kRPVControllerStreamNodeId:
        case kRPVControllerStreamProcessId:
        case kRPVControllerStreamName:
        case kRPVControllerStreamExtData:
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

rocprofvis_result_t Stream::GetString(rocprofvis_property_t property, uint64_t index, char* value,
                  uint32_t* length)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerStreamName:
        {
            if(value && length && *length)
            {
                strncpy(value, m_name.c_str(), *length);
                result = kRocProfVisResultSuccess;
            }
            else if(length)
            {
                *length = m_name.length();
                result  = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerStreamExtData:
        {
            if(value && length && *length)
            {
                strncpy(value, m_ext_data.c_str(), *length);
                result = kRocProfVisResultSuccess;
            }
            else if(length)
            {
                *length = m_ext_data.length();
                result  = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerStreamId:
        case kRPVControllerStreamNodeId:
        case kRPVControllerStreamProcessId:
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

rocprofvis_result_t Stream::SetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerStreamId:
        {
            m_id  = value;
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerStreamNodeId:
        {
            m_node_id  = value;
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerStreamProcessId:
        {
            m_process_id  = value;
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerStreamName:
        case kRPVControllerStreamExtData:
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

rocprofvis_result_t Stream::SetDouble(rocprofvis_property_t property, uint64_t index, double value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerStreamId:
        case kRPVControllerStreamNodeId:
        case kRPVControllerStreamProcessId:
        case kRPVControllerStreamName:
        case kRPVControllerStreamExtData:
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

rocprofvis_result_t Stream::SetObject(rocprofvis_property_t property, uint64_t index,
                  rocprofvis_handle_t* value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerStreamId:
        case kRPVControllerStreamNodeId:
        case kRPVControllerStreamProcessId:
        case kRPVControllerStreamName:
        case kRPVControllerStreamExtData:
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

rocprofvis_result_t Stream::SetString(rocprofvis_property_t property, uint64_t index, char const* value,
                  uint32_t length)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerStreamName:
        {
            m_name = value;
            result     = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerStreamExtData:
        {
            m_ext_data = value;
            result     = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerStreamId:
        case kRPVControllerStreamNodeId:
        case kRPVControllerStreamProcessId:
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
