// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_controller_trace_system.h"
#include "rocprofvis_controller_arguments.h"
#include "rocprofvis_controller_array.h"
#include "rocprofvis_controller_event.h"
#include "rocprofvis_controller_future.h"
#include "rocprofvis_controller_graph.h"
#include "rocprofvis_controller_reference.h"
#include "rocprofvis_controller_sample.h"
#include "rocprofvis_controller_summary.h"
#include "rocprofvis_controller_summary_metrics.h"
#include "rocprofvis_controller_table_system.h"
#include "rocprofvis_controller_timeline.h"
#include "rocprofvis_controller_track.h"
#include "rocprofvis_controller_topology.h"
#include "rocprofvis_core.h"
#include "rocprofvis_core_assert.h"
#ifdef COMPUTE_UI_SUPPORT
#include "compute/rocprofvis_controller_trace_compute.h"
#include "compute/rocprofvis_controller_plot.h"
#include <filesystem>
#endif

#include <cfloat>
#include <cstdint>
#include <cstring>
#include <set>

namespace RocProfVis
{
namespace Controller
{

typedef Reference<rocprofvis_controller_table_t, SystemTable, kRPVControllerObjectTypeTable> SystemTableRef;
typedef Reference<rocprofvis_controller_track_t, Track, kRPVControllerObjectTypeTrack> TrackRef;
typedef Reference<rocprofvis_controller_timeline_t, Timeline, kRPVControllerObjectTypeTimeline> TimelineRef;

SystemTrace::SystemTrace(const std::string& filename)
: Trace(__kRPVControllerSystemPropertiesFirst, __kRPVControllerSystemPropertiesLast, filename)
, m_timeline(nullptr)
, m_event_table(nullptr)
, m_sample_table(nullptr)
, m_search_table(nullptr)
, m_summary(nullptr)
, m_mem_mgmt(nullptr)
{
    
}

rocprofvis_result_t SystemTrace::Init()
{
    rocprofvis_result_t result = kRocProfVisResultUnknownError;
    try
    {
        m_event_table = new SystemTable(0);

        m_sample_table = new SystemTable(1);

        m_search_table = new SystemTable(2);
        
        m_summary = new Summary(this);

        m_mem_mgmt = new MemoryManager(m_id);

        result = kRocProfVisResultSuccess;
    }
    catch(const std::exception&)
    {
        spdlog::error("Failed to allocate trace tables & memory manager");
        result = kRocProfVisResultMemoryAllocError;
    }
    return result;
}

SystemTrace::~SystemTrace()
{
    delete m_mem_mgmt; 
    m_mem_mgmt = nullptr;
    delete m_timeline;
    delete m_event_table;
    delete m_sample_table;
    delete m_search_table;
    delete m_summary;
    delete m_topology_root;
    for (Track* track : m_tracks)
    {
        delete track;
    }
    for (auto* node : m_nodes)
    {
        delete node;
    }
}

MemoryManager* SystemTrace::GetMemoryManager(){
    return m_mem_mgmt;
}

void SystemTrace::DbgPrintTopologyNodeData(rocprofvis_dm_topology_node node, int level)
{
    std::string line;
    for (int i = 0; i < level; i++)
    {
        line += " |";
    }
    line += "-->";
    const char* name =
        rocprofvis_dm_get_property_as_charptr(
            node, kRPVControllerTopologyNodeName, 0);
    line += name;

    uint64_t num_children =
        rocprofvis_dm_get_property_as_uint64(
            node, kRPVControllerTopologyNodeNumChildren, 0);

    spdlog::info(line);

    for (int i = 0; i < num_children; i++)
    {
        rocprofvis_dm_topology_node child_node =
            rocprofvis_dm_get_property_as_handle(
                node, kRPVControllerTopologyNodeChildHandleIndexed, i);
        DbgPrintTopologyNodeData(child_node, level + 1);
    }
}

rocprofvis_result_t SystemTrace::LoadRocpd() {
    rocprofvis_result_t result = kRocProfVisResultUnknownError;
    try
    {
        m_timeline                 = new Timeline(0);
        size_t trace_size          = 0;
        m_dm_handle = rocprofvis_dm_create_trace();
        if(nullptr != m_dm_handle)
        {
            rocprofvis_dm_database_t db =
                rocprofvis_db_open_database(m_trace_file.c_str(), kAutodetect);
            if(nullptr != db && kRocProfVisDmResultSuccess ==
                                    rocprofvis_dm_bind_trace_to_database(m_dm_handle, db))
            {
                rocprofvis_db_future_t object2wait = rocprofvis_db_future_alloc(&Trace::ProgressCallback, this);
                if(nullptr != object2wait)
                {
                    std::multimap<uint64_t, Track*> queue_to_track;
                    std::multimap<uint64_t, Track*> stream_to_track;
                    std::multimap<uint64_t, Track*> thread_to_track;
                    std::multimap<uint64_t, Track*> sample_thread_to_track;
                    std::multimap<uint64_t, Track*> counter_to_track;
                    
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
                                   dm_track_type == kRocProfVisDmRegionMainTrack ||
                                   dm_track_type == kRocProfVisDmRegionSampleTrack ||
                                   dm_track_type == kRocProfVisDmKernelDispatchTrack ||
                                   dm_track_type == kRocProfVisDmMemoryAllocationTrack ||
                                   dm_track_type == kRocProfVisDmMemoryCopyTrack ||
                                   dm_track_type == kRocProfVisDmStreamTrack ||
                                   dm_track_type == kRocProfVisDmPmcTrack)
                                {
                                    auto   type = (dm_track_type == kRocProfVisDmPmcTrack)
                                                      ? kRPVControllerTrackTypeSamples
                                                      : kRPVControllerTrackTypeEvents;
                                    Track* track =
                                        new Track(type, track_id, dm_track_handle, this);
                                    {
                                        std::string category =
                                            rocprofvis_dm_get_property_as_charptr(
                                                dm_track_handle,
                                                kRPVDMTrackCategoryEnumCharPtr, 0);

                                        track->SetString(kRPVControllerCategory, 0,
                                                         category.c_str());

                                        std::string main_name =
                                            rocprofvis_dm_get_property_as_charptr(
                                                dm_track_handle,
                                                kRPVDMTrackMainProcessNameCharPtr, 0);
                                        track->SetString(kRPVControllerMainName, 0,
                                                         main_name.c_str());

                                        std::string sub_name =
                                            rocprofvis_dm_get_property_as_charptr(
                                                dm_track_handle,
                                                kRPVDMTrackSubProcessNameCharPtr, 0);
                                        track->SetString(kRPVControllerSubName, 0,
                                                         sub_name.c_str());

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
                                        double min_ts = static_cast<double>(
                                            rocprofvis_dm_get_property_as_uint64(
                                                track->GetDmHandle(),
                                                kRPVDMTrackMinimumTimestampUInt64, 0));
                                        double max_ts = static_cast<double>(
                                            rocprofvis_dm_get_property_as_uint64(
                                                track->GetDmHandle(),
                                                kRPVDMTrackMaximumTimestampUInt64, 0));
                                        double min_value =
                                            rocprofvis_dm_get_property_as_double(
                                                track->GetDmHandle(),
                                                kRPVDMTrackMinimumValueDouble, 0);
                                        double max_value =
                                            rocprofvis_dm_get_property_as_double(
                                                track->GetDmHandle(),
                                                kRPVDMTrackMaximumValueDouble, 0);
                                        uint64_t instance_id =
                                            rocprofvis_dm_get_property_as_uint64(
                                                track->GetDmHandle(),
                                                kRPVDMTrackInstanceIdUInt64, 0) << 48;
                                        uint64_t node =
                                            rocprofvis_dm_get_property_as_uint64(
                                                track->GetDmHandle(),
                                                kRPVDMTrackNodeIdUInt64, 0);
                                        uint64_t agent_id_or_pid =
                                            rocprofvis_dm_get_property_as_uint64(
                                                track->GetDmHandle(),
                                                kRPVDMTrackProcessIdUInt64, 0);
                                        uint64_t queue_id_or_tid =
                                            rocprofvis_dm_get_property_as_uint64(
                                                track->GetDmHandle(),
                                                kRPVDMTrackSubProcessIdUInt64, 0);
                                        if (type == kRPVControllerTrackTypeSamples)
                                        {
                                            max_ts = end_time;
                                        }
                                        track->SetDouble(kRPVControllerTrackMinTimestamp,
                                                         0, min_ts);
                                        track->SetDouble(kRPVControllerTrackMaxTimestamp,
                                                         0, max_ts);
                                        track->SetDouble(kRPVControllerTrackMinValue,
                                                         0, min_value);
                                        track->SetDouble(kRPVControllerTrackMaxValue,
                                                         0, max_value);
                                        track->SetUInt64(kRPVControllerTrackNode, 0,
                                                         node);
                                        track->SetUInt64(kRPVControllerTrackAgentIdOrPid, 0,
                                            agent_id_or_pid);
                                        track->SetUInt64(kRPVControllerTrackQueueIdOrTid, 0,
                                            queue_id_or_tid);

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
                                                queue_to_track.insert({ val | instance_id, track });
                                            }
                                            else if(category == "Stream" && name == "id")
                                            {
                                                char*    end = nullptr;
                                                uint64_t val = std::strtoull(
                                                    value.c_str(), &end, 10);
                                                stream_to_track.insert({ val | instance_id, track });
                                            }
                                            else if(category == "Thread" && name == "id")
                                            {
                                                char*    end = nullptr;
                                                uint64_t val = std::strtoull(
                                                    value.c_str(), &end, 10);
                                                if(dm_track_type == kRocProfVisDmRegionSampleTrack)
                                                {
                                                    sample_thread_to_track.insert({ val | instance_id, track });
                                                }
                                                else
                                                {
                                                    thread_to_track.insert({ val | instance_id, track });
                                                }                                                
                                            }
                                            else if(category == "PMC")
                                            {
                                                if(name == "id")
                                                {
                                                    char*    end = nullptr;
                                                    uint64_t val = std::strtoull(
                                                        value.c_str(), &end, 10);
                                                    counter_to_track.insert({ val | instance_id, track });
                                                }
                                            }

                                            spdlog::debug("{} {} {}", category.c_str(),
                                                         name.c_str(), value.c_str());
                                        }

                                        uint32_t index = static_cast<uint32_t>(m_tracks.size());
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


                    rocprofvis_dm_topology_node dm_topology_root =
                        rocprofvis_dm_get_property_as_handle(
                            m_dm_handle, kRPVDMTopologyHandle, 0);

                    DbgPrintTopologyNodeData(dm_topology_root, 1);

                    m_topology_root = new TopologyRoot(dm_topology_root, this);

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
    catch(const std::exception&)
    {
        result = kRocProfVisResultMemoryAllocError;
    }
    return result;
}

rocprofvis_result_t SystemTrace::Load(RocProfVis::Controller::Future& future)
{    
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    future.Set(JobSystem::Get().IssueJob([this](Future* future) -> rocprofvis_result_t
        {
            (void) future;
            rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
            if(m_trace_file.find(".rpd", m_trace_file.size() - 4) != std::string::npos || 
                m_trace_file.find(".db", m_trace_file.size() - 3) != std::string::npos ||
                m_trace_file.find(".yaml", m_trace_file.size() - 5) != std::string::npos)
            {
                result = LoadRocpd();
            }
            else
            {
                result = kRocProfVisResultInvalidArgument;
            }
        return result;
        },&future));

    if(future.IsValid())
    {
        result = kRocProfVisResultSuccess;
    }

    return result;
}

rocprofvis_result_t SystemTrace::SaveTrimmedTrace(Future& future, double start, double end, char const* path)
{
    rocprofvis_result_t error = kRocProfVisResultUnknownError;

    rocprofvis_dm_trace_t dm_handle = m_dm_handle;
    std::string path_str = path;
    future.Set(JobSystem::Get().IssueJob([start, end, path_str, dm_handle](Future* future) -> rocprofvis_result_t {
                              (void) future;
                              rocprofvis_result_t result = kRocProfVisResultUnknownError;
                              rocprofvis_dm_database_t db = rocprofvis_dm_get_property_as_handle(dm_handle, kRPVDMDatabaseHandle, 0);
                              if (db)
                              {
                                  rocprofvis_db_future_t object2wait = rocprofvis_db_future_alloc(nullptr);
                                  if (object2wait)
                                  {
                                    auto error = rocprofvis_db_trim_save_async(db, start, end, path_str.c_str(), object2wait);
                                      result = (error == kRocProfVisDmResultSuccess)
                                                   ? kRocProfVisResultSuccess
                                                   : kRocProfVisResultUnknownError;

                                    if (error == kRocProfVisDmResultSuccess)
                                    {
                                        error = rocprofvis_db_future_wait(object2wait,
                                                                          UINT64_MAX);
                                        result = (error == kRocProfVisDmResultSuccess)
                                                     ? kRocProfVisResultSuccess
                                                     : kRocProfVisResultUnknownError;
                                    }

                                    rocprofvis_db_future_free(object2wait);
                                  }
                              }
                              return result;
                          }, &future));

    if(future.IsValid())
    {
        error = kRocProfVisResultSuccess;
    }

    return error;
}

rocprofvis_result_t SystemTrace::AsyncFetch(Track& track, Future& future, Array& array,
                                double start, double end)
{
    rocprofvis_result_t error = kRocProfVisResultUnknownError;
    if(m_timeline)
    {
        error = m_timeline->AsyncFetch(track, future, array, start, end);
    }
    return error;
}

rocprofvis_result_t SystemTrace::AsyncFetch(Graph& graph, Future& future, Array& array,
                                double start, double end, uint32_t pixels)
{
    rocprofvis_result_t error = kRocProfVisResultUnknownError;
    if(m_timeline)
    {
        error = m_timeline->AsyncFetch(graph, future, array, start, end, pixels);
    }
    return error;
}

rocprofvis_result_t SystemTrace::AsyncFetch(Event& event, Future& future, Array& array,
                  rocprofvis_property_t property)
{
    rocprofvis_result_t error = kRocProfVisResultUnknownError;
    rocprofvis_dm_trace_t dm_handle = m_dm_handle;
    future.Set(JobSystem::Get().IssueJob([&event, &array, property, dm_handle](Future* future) -> rocprofvis_result_t {
                              (void) future;
                              rocprofvis_result_t result = kRocProfVisResultUnknownError;
                              result = event.Fetch(property, array, dm_handle);
                              return result;
                          },&future));

    if(future.IsValid())
    {
        error = kRocProfVisResultSuccess;
    }

    return error;
}

rocprofvis_result_t SystemTrace::AsyncFetch(rocprofvis_property_t property, Future& future, Array& array,
                  uint64_t index, uint64_t count)
{
    (void) count;
    rocprofvis_result_t error = kRocProfVisResultUnknownError;

    future.Set(JobSystem::Get().IssueJob(
        [this, property, &array, index](Future* future) -> rocprofvis_result_t {
            (void) future;
            rocprofvis_result_t result = kRocProfVisResultUnknownError;

            switch(property)
            {
                case kRPVControllerSystemEventIndexed:
                {
                    // Todo: implement this function 
                    // result = Event::FetchSingleEvent(event_id, array, m_dm_handle);
                    break;
                }
                case kRPVControllerSystemEventDataExtDataIndexed:
                {
                    const uint64_t& event_id = index;
                    result = Event::FetchDataModelExtendedDataProperty(event_id, array,
                                                                       m_dm_handle);
                    break;
                }
                case kRPVControllerSystemEventDataCallStackIndexed:
                {
                    const uint64_t& event_id = index;
                    result = Event::FetchDataModelStackTraceProperty(event_id, array,
                                                                     m_dm_handle);
                    break;
                }
                case kRPVControllerSystemEventDataFlowControlIndexed:
                {
                    const uint64_t& event_id = index;
                    result = Event::FetchDataModelFlowTraceProperty(event_id, array,
                                                                    m_dm_handle);
                    break;
                }
                case kRPVControllerSystemBucketDataValueIndexed:
                {
                    uint64_t buckets_num = 0;
                    result = GetUInt64(kRPVControllerSystemGetHistogramBucketsNumber, 0, &buckets_num);
                    if (result == kRocProfVisResultSuccess)
                    {
                        result = array.SetUInt64(kRPVControllerArrayNumEntries, 0, buckets_num);
                    }
                    result = m_tracks[index]->GetBucketValues(buckets_num, array);
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
        }, &future));

    if(future.IsValid())
    {
        error = kRocProfVisResultSuccess;
    }

    return error;
}

rocprofvis_result_t SystemTrace::AsyncFetch(Table& table, Future& future, Array& array,
    uint64_t index, uint64_t count)
{
    rocprofvis_result_t error = kRocProfVisResultUnknownError;
    rocprofvis_dm_trace_t dm_handle = m_dm_handle;

    future.Set(JobSystem::Get().IssueJob([&table, &array, index, count, dm_handle](Future* future) -> rocprofvis_result_t {
                              rocprofvis_result_t result = kRocProfVisResultUnknownError;
                              result = table.Fetch(dm_handle, index, count, array, future);
                              return result;
                          },&future));

    if(future.IsValid())
    {
        error = kRocProfVisResultSuccess;
    }

    return error;
}

rocprofvis_result_t SystemTrace::AsyncFetch(Table& table, Arguments& args, Future& future, Array& array)
{
    rocprofvis_result_t   error     = kRocProfVisResultUnknownError;
    rocprofvis_dm_trace_t dm_handle = m_dm_handle;

    future.Set(JobSystem::Get().IssueJob([&table, dm_handle, &args, &array](Future* future) -> rocprofvis_result_t {
            rocprofvis_result_t result = kRocProfVisResultUnknownError;
            result = table.Setup(dm_handle, args, future);
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
                result = table.Fetch(dm_handle, start_index, start_count, array, future);
            }
            return result;
        }, &future));

    if(future.IsValid())
    {
        error = kRocProfVisResultSuccess;
    }

    return error;
}

rocprofvis_result_t SystemTrace::TableExportCSV(Table& table, Arguments& args, Future& future, const char* path)
{
    rocprofvis_result_t   error     = kRocProfVisResultUnknownError;
    rocprofvis_dm_trace_t dm_handle = m_dm_handle;
    std::string path_str = path; 

    future.Set(JobSystem::Get().IssueJob([&table, dm_handle, &args, path_str](Future* future) -> rocprofvis_result_t {
            return table.ExportCSV(dm_handle, args, future, path_str.c_str());
        }, &future));

    if(future.IsValid())
    {
        error = kRocProfVisResultSuccess;
    }

    return error;
}

rocprofvis_result_t SystemTrace::AsyncFetch(Summary& summary, Arguments& args, Future& future, SummaryMetrics& output)
{
    rocprofvis_result_t   error     = kRocProfVisResultUnknownError;
    rocprofvis_dm_trace_t dm_handle = m_dm_handle;

    future.Set(JobSystem::Get().IssueJob([&summary, dm_handle, &args, &output](Future* future) -> rocprofvis_result_t {
            rocprofvis_result_t result = kRocProfVisResultUnknownError;
            result = summary.Fetch(dm_handle, args, output, future);
            return result;
        }, &future));

    if(future.IsValid())
    {
        error = kRocProfVisResultSuccess;
    }

    return error;
}

rocprofvis_controller_object_type_t SystemTrace::GetType(void) 
{
    return kRPVControllerObjectTypeControllerSystem;
}

rocprofvis_result_t SystemTrace::GetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t* value) 
{
    (void) index;
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
            case kRPVControllerSystemId:
            {
                *value = m_id;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerSystemNumAnalysisView:
            {
                ROCPROFVIS_UNIMPLEMENTED;
                *value = 0;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerSystemNumTracks:
            {
                *value = m_tracks.size();
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerSystemNumNodes:
            {
                result = m_topology_root->GetUInt64(kRPVControllerSystemNumNodes, 0, value);
                break;
            }
            case kRPVControllerSystemGetDmProgress:
            {
                std::lock_guard lock(m_mutex);
                *value = m_dm_progress_percent;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerSystemGetHistogramBucketsNumber:
            {
                *value = rocprofvis_dm_get_property_as_uint64(
                    m_dm_handle, kRPVDMHistogramNumBuckets, 0);
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerSystemGetHistogramBucketSize:
            {
                *value = rocprofvis_dm_get_property_as_uint64(
                    m_dm_handle, kRPVDMHistogramBucketSize, 0);
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

rocprofvis_result_t SystemTrace::GetObject(rocprofvis_property_t property, uint64_t index, rocprofvis_handle_t** value) 
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if (value)
    {
        switch (property)
        {
            case kRPVControllerSystemTimeline:
            {
                *value = (rocprofvis_handle_t*)m_timeline;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerSystemEventTable:
            {
                *value = (rocprofvis_handle_t*)m_event_table;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerSystemSampleTable:
            {
                *value = (rocprofvis_handle_t*)m_sample_table;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerSystemSearchResultsTable:
            {
                *value = (rocprofvis_handle_t*)m_search_table;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerSystemAnalysisViewIndexed:
            {
                ROCPROFVIS_UNIMPLEMENTED;
                *value = nullptr;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerSystemTrackById:
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
            case kRPVControllerSystemTrackIndexed:
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
            case kRPVControllerSystemNumNodes:
            case kRPVControllerSystemNodeIndexed:         
            {
                result = m_topology_root->GetObject(kRPVControllerSystemNodeIndexed, index, value);
                break;
            }
            case kRPVControllerSystemSummary:
            {
                *value = (rocprofvis_handle_t*)m_summary;
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
rocprofvis_result_t SystemTrace::GetString(rocprofvis_property_t property, uint64_t index, char* value, uint32_t* length) 
{
    (void) index;
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerSystemGetDmMessage:
        {
            std::lock_guard lock(m_mutex);
            result = m_dm_message.GetString(value, length);
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

rocprofvis_result_t SystemTrace::SetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t value) 
{
    (void) index;
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerSystemId:
        {
            result = kRocProfVisResultReadOnlyError;
            break;
        }
        case kRPVControllerSystemNumTracks:
        {
            if (m_tracks.size() != value)
            {
                for (uint64_t i = value; i < m_tracks.size(); i++)
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
        case kRPVControllerSystemNotifySelected:
        {
            if(value > 0 && m_mem_mgmt != nullptr)
            {
                m_mem_mgmt->Configure(2.0);
            }
            break;
        }
        case kRPVControllerSystemNumAnalysisView:
        {
            ROCPROFVIS_UNIMPLEMENTED;
            break;
        }
        case kRPVControllerSystemNumNodes:
        {
            if (m_nodes.size() != value)
            {
                for (uint64_t i = value; i < m_nodes.size(); i++)
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
        default:
        {
            result = UnhandledProperty(property);
            break;
        }
    }
    return result;
}

rocprofvis_result_t SystemTrace::SetObject(rocprofvis_property_t property, uint64_t index, rocprofvis_handle_t* value) 
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            case kRPVControllerSystemTimeline:
            {
                TimelineRef timeline(value);
                if(timeline.IsValid())
                {
                    m_timeline = timeline.Get();
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVControllerSystemEventTable:
            {
                SystemTableRef table(value);
                if(table.IsValid())
                {
                    m_event_table = table.Get();
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVControllerSystemSampleTable:
            {
                SystemTableRef table(value);
                if(table.IsValid())
                {
                    m_sample_table = table.Get();
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVControllerSystemSearchResultsTable:
            {
                SystemTableRef table(value);
                if(table.IsValid())
                {
                    m_search_table = table.Get();
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVControllerSystemAnalysisViewIndexed:
            {
                ROCPROFVIS_UNIMPLEMENTED;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerSystemTrackIndexed:
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
            default:
            {
                result = UnhandledProperty(property);
                break;
            }
        }
    }
    return result;
}

}
}