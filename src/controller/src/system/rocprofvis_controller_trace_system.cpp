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
#include "rocprofvis_controller_table_system_search.h"
#include "rocprofvis_controller_timeline.h"
#include "rocprofvis_controller_track.h"
#include "rocprofvis_controller_topology.h"
#include "rocprofvis_core.h"
#include "rocprofvis_core_assert.h"
#include "rocprofvis_core_string_utils.h"
#include <cfloat>
#include <cstdint>
#include <cstring>
#include <memory>
#include <set>

namespace RocProfVis
{
namespace Controller
{

typedef Reference<rocprofvis_controller_table_t, SystemTable, kRPVControllerObjectTypeTable> SystemTableRef;
typedef Reference<rocprofvis_controller_track_t, Track, kRPVControllerObjectTypeTrack> TrackRef;
typedef Reference<rocprofvis_controller_timeline_t, Timeline, kRPVControllerObjectTypeTimeline> TimelineRef;

namespace
{
struct DataModelFutureDeleter
{
    void operator()(rocprofvis_db_future_t future) const
    {
        rocprofvis_db_future_free(future);
    }
};

using DataModelFuturePtr = std::unique_ptr<void, DataModelFutureDeleter>;
}

SystemTrace::SystemTrace(const std::string& filename, const std::string& config_path)
: Trace(__kRPVControllerSystemPropertiesFirst, __kRPVControllerSystemPropertiesLast, filename)
, m_timeline(nullptr)
, m_event_table(nullptr)
, m_sample_table(nullptr)
, m_search_table(nullptr)
, m_summary(nullptr)
, m_mem_mgmt(nullptr)
, m_topology_root(nullptr)
, m_config_path(config_path)
{
}

SystemTrace::SystemTrace(const std::vector<std::string>& filenames)
: Trace(__kRPVControllerSystemPropertiesFirst, __kRPVControllerSystemPropertiesLast,
        filenames.empty() ? std::string() : filenames.front())
, m_files(filenames)
, m_timeline(nullptr)
, m_event_table(nullptr)
, m_sample_table(nullptr)
, m_search_table(nullptr)
, m_summary(nullptr)
, m_mem_mgmt(nullptr)
, m_topology_root(nullptr)
{

}

SystemTrace::~SystemTrace()
{
    delete GetMemoryManager();
    SetMemoryManager(nullptr);
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
}

rocprofvis_result_t SystemTrace::Init()
{
    rocprofvis_result_t result = kRocProfVisResultUnknownError;
    try
    {
        m_event_table = new SystemTable(0);
        m_sample_table = new SystemTable(1);
        m_search_table = new EventSearchTable(2);
        m_summary = new Summary(this);
        SetMemoryManager(new MemoryManager(m_id));

        result = kRocProfVisResultSuccess;
    }
    catch(const std::exception&)
    {
        spdlog::error("Failed to allocate trace tables & memory manager");
        delete m_event_table;  m_event_table  = nullptr;
        delete m_sample_table; m_sample_table = nullptr;
        delete m_search_table; m_search_table = nullptr;
        delete m_summary;      m_summary      = nullptr;
        delete GetMemoryManager();
        SetMemoryManager(nullptr);
        result = kRocProfVisResultMemoryAllocError;
    }
    return result;
}

MemoryManager* SystemTrace::GetMemoryManager(){
    return m_mem_mgmt;
}

void
SystemTrace::SetMemoryManager(MemoryManager* memory_manager)
{
    m_mem_mgmt = memory_manager;
}

std::mutex& SystemTrace::GetTableMutex(rocprofvis_dm_table_use_case_enum_t use_case)
{
    return m_table_mutex[use_case];
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

    spdlog::debug(line);

    for (int i = 0; i < num_children; i++)
    {
        rocprofvis_dm_topology_node child_node =
            rocprofvis_dm_get_property_as_handle(
                node, kRPVControllerTopologyNodeChildHandleIndexed, i);
        DbgPrintTopologyNodeData(child_node, level + 1);
    }
}

rocprofvis_result_t SystemTrace::LoadRocpd(Future* future)
{
    if(!future)
    {
        return kRocProfVisResultInvalidArgument;
    }

    try
    {
        size_t trace_size = 0;
        SetDMHandle(rocprofvis_dm_create_trace());
        if(!GetDMHandle())
        {
            return kRocProfVisResultMemoryAllocError;
        }
        m_timeline = new Timeline(0);

        rocprofvis_dm_database_t db = nullptr;
        rocprofvis_result_t result = OpenRocpdDatabase(db);
        if(result != kRocProfVisResultSuccess)
        {
            return result;
        }

        result = ReadRocpdMetadata(db, future);
        if(result != kRocProfVisResultSuccess)
        {
            return result;
        }

        result = LoadRocpdTracks(trace_size);
        if(result != kRocProfVisResultSuccess)
        {
            return result;
        }

        GetMemoryManager()->Init(trace_size);
        return LoadRocpdTopology();
    }
    catch(const std::exception&)
    {
        return kRocProfVisResultMemoryAllocError;
    }
}

rocprofvis_result_t
SystemTrace::OpenRocpdDatabase(rocprofvis_dm_database_t& database)
{
    database = nullptr;
    if(m_files.size() > 1)
    {
        std::vector<const char*> file_ptrs;
        file_ptrs.reserve(m_files.size());
        for(const std::string& file : m_files)
        {
            file_ptrs.push_back(file.c_str());
        }
        database = rocprofvis_db_open_database_multi(file_ptrs.data(), file_ptrs.size());
    }
    else
    {
        database = rocprofvis_db_open_database(m_trace_file.c_str(), kAutodetect);
    }

    if(!database)
    {
        return kRocProfVisResultUnknownError;
    }

    rocprofvis_dm_result_t dm_result = rocprofvis_dm_bind_trace_to_database(
        GetDMHandle(), database, m_config_path.c_str());
    return dm_result == kRocProfVisDmResultSuccess
               ? kRocProfVisResultSuccess
               : kRocProfVisResultUnknownError;
}

rocprofvis_result_t
SystemTrace::ReadRocpdMetadata(rocprofvis_dm_database_t database, Future* future)
{
    DataModelFuturePtr object2wait(
        rocprofvis_db_future_alloc(&Future::ProgressCallback, future));
    if(!object2wait)
    {
        return kRocProfVisResultMemoryAllocError;
    }

    rocprofvis_dm_result_t dm_result =
        rocprofvis_db_read_metadata_async(database, object2wait.get());
    if(dm_result != kRocProfVisDmResultSuccess)
    {
        return kRocProfVisResultUnknownError;
    }

    future->AddDependentFuture(object2wait.get());
    dm_result = rocprofvis_db_future_wait(object2wait.get(), UINT64_MAX);
    future->RemoveDependentFuture(object2wait.get());
    return dm_result == kRocProfVisDmResultSuccess
               ? kRocProfVisResultSuccess
               : kRocProfVisResultTimeout;
}

rocprofvis_result_t
SystemTrace::AddRocpdGraph(Track* track, uint64_t dm_track_type, uint64_t track_id,
                           uint64_t& graph_index)
{
    std::unique_ptr<Graph> graph = std::make_unique<Graph>(
        this,
        dm_track_type == kRocProfVisDmPmcTrack ? kRPVControllerGraphTypeLine
                                               : kRPVControllerGraphTypeFlame,
        track_id);
    rocprofvis_result_t result = graph->SetObject(
        kRPVControllerGraphTrack, 0,
        reinterpret_cast<rocprofvis_handle_t*>(track));
    if(result != kRocProfVisResultSuccess)
    {
        return result;
    }

    result = m_timeline->SetUInt64(
        kRPVControllerTimelineNumGraphs, 0, ++graph_index);
    if(result != kRocProfVisResultSuccess)
    {
        return result;
    }

    result = m_timeline->SetObject(
        kRPVControllerTimelineGraphIndexed, graph_index - 1,
        reinterpret_cast<rocprofvis_handle_t*>(graph.get()));
    if(result != kRocProfVisResultSuccess)
    {
        return result;
    }

    graph.release();
    return kRocProfVisResultSuccess;
}

rocprofvis_result_t
SystemTrace::LoadRocpdTrack(rocprofvis_dm_track_t dm_track_handle,
                            uint64_t dm_track_type, uint64_t track_id,
                            size_t& trace_size, uint64_t& graph_index)
{
    rocprofvis_controller_track_type_t type =
        dm_track_type == kRocProfVisDmPmcTrack ? kRPVControllerTrackTypeSamples
                                               : kRPVControllerTrackTypeEvents;
    std::unique_ptr<Track> track =
        std::make_unique<Track>(type, track_id, dm_track_handle, this);

    rocprofvis_result_t result = track->FillBounds();
    if(result != kRocProfVisResultSuccess)
    {
        return result;
    }

    result = track->FillMetadata();
    if(result != kRocProfVisResultSuccess)
    {
        return result;
    }

    result = track->FillTopologyIds();
    if(result != kRocProfVisResultSuccess)
    {
        return result;
    }

    uint64_t num_records = track->GetNumberOfEntries();
    trace_size += num_records * (type == kRPVControllerTrackTypeEvents
                                     ? sizeof(Event)
                                     : sizeof(Sample));

    Track* track_ptr = track.release();
    m_tracks.push_back(track_ptr);
    return AddRocpdGraph(track_ptr, dm_track_type, track_id, graph_index);
}

rocprofvis_result_t
SystemTrace::LoadRocpdTracks(size_t& trace_size)
{
    rocprofvis_db_num_of_tracks_t num_tracks =
        static_cast<rocprofvis_db_num_of_tracks_t>(
            rocprofvis_dm_get_property_as_uint64(
                GetDMHandle(), kRPVDMNumberOfTracksUInt64, 0));

    uint64_t graph_index = 0;
    for(rocprofvis_db_num_of_tracks_t i = 0; i < num_tracks; i++)
    {
        rocprofvis_dm_track_t dm_track_handle =
            rocprofvis_dm_get_property_as_handle(
                GetDMHandle(), kRPVDMTrackHandleIndexed, i);
        if(!dm_track_handle)
        {
            return kRocProfVisResultUnknownError;
        }

        uint64_t track_id = rocprofvis_dm_get_property_as_uint64(
            dm_track_handle, kRPVDMTrackIdUInt64, 0);
        uint64_t dm_track_type = rocprofvis_dm_get_property_as_uint64(
            dm_track_handle, kRPVDMTrackCategoryEnumUInt64, 0);
        bool supported_track =
            dm_track_type == kRocProfVisDmRegionTrack ||
            dm_track_type == kRocProfVisDmRegionMainTrack ||
            dm_track_type == kRocProfVisDmRegionSampleTrack ||
            dm_track_type == kRocProfVisDmKernelDispatchTrack ||
            dm_track_type == kRocProfVisDmMemoryAllocationTrack ||
            dm_track_type == kRocProfVisDmMemoryCopyTrack ||
            dm_track_type == kRocProfVisDmStreamTrack ||
            dm_track_type == kRocProfVisDmPmcTrack;
        if(!supported_track)
        {
            continue;
        }

        rocprofvis_result_t result =
            LoadRocpdTrack(dm_track_handle, dm_track_type, track_id, trace_size,
                           graph_index);
        if(result != kRocProfVisResultSuccess)
        {
            return result;
        }
    }
    return kRocProfVisResultSuccess;
}

rocprofvis_result_t
SystemTrace::LoadRocpdTopology()
{
    rocprofvis_dm_topology_node dm_topology_root =
        rocprofvis_dm_get_property_as_handle(
            GetDMHandle(), kRPVDMTopologyHandle, 0);
    if(!dm_topology_root)
    {
        return kRocProfVisResultUnknownError;
    }

    DbgPrintTopologyNodeData(dm_topology_root, 1);
    m_topology_root = new TopologyRoot(dm_topology_root, this);
    if(!ValidateRocpdTrackTopology())
    {
        spdlog::warn("Trace loaded with incomplete track topology");
    }
    return kRocProfVisResultSuccess;
}

bool
SystemTrace::ValidateRocpdTrackTopology() const
{
    bool valid = true;
    for(const Track* track : m_tracks)
    {
        const auto track_type = static_cast<rocprofvis_dm_track_category_t>(
            rocprofvis_dm_get_property_as_uint64(
                track->GetDmHandle(), kRPVDMTrackCategoryEnumUInt64, 0));
        bool        linked        = false;
        const char* expected_link = "unknown";
        switch(track_type)
        {
            case kRocProfVisDmRegionTrack:
            case kRocProfVisDmRegionMainTrack:
            case kRocProfVisDmRegionSampleTrack:
            {
                linked        = track->GetThread() != nullptr;
                expected_link = "thread";
                break;
            }
            case kRocProfVisDmKernelDispatchTrack:
            case kRocProfVisDmMemoryAllocationTrack:
            case kRocProfVisDmMemoryCopyTrack:
            {
                linked        = track->GetQueue() != nullptr;
                expected_link = "queue";
                break;
            }
            case kRocProfVisDmStreamTrack:
            {
                linked        = track->GetStream() != nullptr;
                expected_link = "stream";
                break;
            }
            case kRocProfVisDmPmcTrack:
            {
                linked        = track->GetCounter() != nullptr;
                expected_link = "counter";
                break;
            }
            default:
            {
                spdlog::warn("Track {} has unsupported data-model type {}",
                             track->GetId(), static_cast<uint64_t>(track_type));
                valid = false;
                continue;
            }
        }

        if(!linked)
        {
            spdlog::warn("Track {} (type {}) is missing its {} topology link",
                         track->GetId(), static_cast<uint64_t>(track_type),
                         expected_link);
            valid = false;
        }
    }
    return valid;
}

rocprofvis_result_t SystemTrace::Load(RocProfVis::Controller::Future& future)
{    
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    future.Set(JobSystem::Get().IssueJob([this](Future* future) -> rocprofvis_result_t
        {
            rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
            using RocProfVis::Core::String::ends_with;
            if(ends_with(m_trace_file, ".rpd") ||
                ends_with(m_trace_file, ".db") ||
                ends_with(m_trace_file, ".yaml")
#ifdef ROCPROFVIS_PERFETTO_ENABLED
                ||
                ends_with(m_trace_file, ".json") ||
                ends_with(m_trace_file, ".proto") ||
                ends_with(m_trace_file, ".pftrace")
#endif
                )
            {
                result = LoadRocpd(future);
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

    rocprofvis_dm_trace_t dm_handle = GetDMHandle();
    std::string path_str = path;
    future.Set(JobSystem::Get().IssueJob([start, end, path_str, dm_handle](Future* future) -> rocprofvis_result_t {
                              (void) future;
                              rocprofvis_result_t result = kRocProfVisResultUnknownError;
                              rocprofvis_dm_database_t db = rocprofvis_dm_get_property_as_handle(dm_handle, kRPVDMDatabaseHandle, 0);
                              if (db)
                              {
                                  DataModelFuturePtr object2wait(
                                      rocprofvis_db_future_alloc(
                                          &Future::ProgressCallback, future));
                                  if (object2wait)
                                  {
                                    auto error = rocprofvis_db_trim_save_async(db, static_cast<rocprofvis_dm_timestamp_t>(start), static_cast<rocprofvis_dm_timestamp_t>(end), path_str.c_str(), object2wait.get());
                                      result = (error == kRocProfVisDmResultSuccess)
                                                   ? kRocProfVisResultSuccess
                                                   : kRocProfVisResultUnknownError;

                                    if (error == kRocProfVisDmResultSuccess)
                                    {
                                        future->AddDependentFuture(object2wait.get());
                                        error = rocprofvis_db_future_wait(object2wait.get(),
                                                                          UINT64_MAX);
                                        future->RemoveDependentFuture(object2wait.get());
                                        result = (error == kRocProfVisDmResultSuccess)
                                                     ? kRocProfVisResultSuccess
                                                     : kRocProfVisResultUnknownError;
                                    }
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

rocprofvis_result_t SystemTrace::CleanupTraceDatabase(Future& future, bool rebuild)
{
    rocprofvis_result_t error = kRocProfVisResultUnknownError;

    rocprofvis_dm_trace_t dm_handle = GetDMHandle();
    future.Set(JobSystem::Get().IssueJob([rebuild, dm_handle](Future* future) -> rocprofvis_result_t {
        (void) future;
        rocprofvis_result_t result = kRocProfVisResultUnknownError;
        rocprofvis_dm_database_t db = rocprofvis_dm_get_property_as_handle(dm_handle, kRPVDMDatabaseHandle, 0);
        if (db)
        {
            DataModelFuturePtr object2wait(rocprofvis_db_future_alloc(nullptr));
            if (object2wait)
            {
                auto error = rocprofvis_db_cleanup_async(db, object2wait.get(), rebuild);
                result = (error == kRocProfVisDmResultSuccess)
                    ? kRocProfVisResultSuccess
                    : kRocProfVisResultUnknownError;

                if (error == kRocProfVisDmResultSuccess)
                {
                    error = rocprofvis_db_future_wait(object2wait.get(),
                        UINT64_MAX);
                    result = (error == kRocProfVisDmResultSuccess)
                        ? kRocProfVisResultSuccess
                        : kRocProfVisResultUnknownError;
                }
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
    rocprofvis_dm_trace_t dm_handle = GetDMHandle();
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
                                                                       GetDMHandle());
                    break;
                }
                case kRPVControllerSystemEventDataCallStackIndexed:
                {
                    const uint64_t& event_id = index;
                    result = Event::FetchDataModelStackTraceProperty(event_id, array,
                                                                     GetDMHandle());
                    break;
                }
                case kRPVControllerSystemEventDataFlowControlIndexed:
                {
                    const uint64_t& event_id = index;
                    result = Event::FetchDataModelFlowTraceProperty(event_id, array,
                                                                    GetDMHandle());
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

rocprofvis_result_t SystemTrace::AsyncFetch(Table& table, Arguments& args, Future& future, Array& array)
{
    rocprofvis_result_t   error     = kRocProfVisResultUnknownError;

    future.Set(JobSystem::Get().IssueJob([this, &table, &args, &array](Future* future) -> rocprofvis_result_t {
            return table.SetupAndFetch(*this, args, array, future);
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
    rocprofvis_dm_trace_t dm_handle = GetDMHandle();
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
    rocprofvis_dm_trace_t dm_handle = GetDMHandle();

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
                    result = track->GetInclusiveMemoryUsage(&entry_size);
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
            case kRPVControllerSystemGetHistogramBucketsNumber:
            {
                *value = rocprofvis_dm_get_property_as_uint64(
                    GetDMHandle(), kRPVDMHistogramNumBuckets, 0);
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerSystemGetHistogramBucketSize:
            {
                *value = rocprofvis_dm_get_property_as_uint64(
                    GetDMHandle(), kRPVDMHistogramBucketSize, 0);
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
                    if(track != nullptr && track->GetId() == index)
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
            if(value > 0 && GetMemoryManager() != nullptr)
            {
                GetMemoryManager()->Configure(2.0);
            }
            break;
        }
        case kRPVControllerSystemNumAnalysisView:
        {
            ROCPROFVIS_UNIMPLEMENTED;
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
                    if(m_timeline != timeline.Get())
                    {
                        delete m_timeline;
                        m_timeline = timeline.Get();
                    }
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVControllerSystemEventTable:
            {
                SystemTableRef table(value);
                if(table.IsValid())
                {
                    if(m_event_table != table.Get())
                    {
                        delete m_event_table;
                        m_event_table = table.Get();
                    }
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVControllerSystemSampleTable:
            {
                SystemTableRef table(value);
                if(table.IsValid())
                {
                    if(m_sample_table != table.Get())
                    {
                        delete m_sample_table;
                        m_sample_table = table.Get();
                    }
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVControllerSystemSearchResultsTable:
            {
                SystemTableRef table(value);
                if(table.IsValid())
                {
                    EventSearchTable* search_table =
                        static_cast<EventSearchTable*>(table.Get());
                    if(m_search_table != search_table)
                    {
                        delete m_search_table;
                        m_search_table = search_table;
                    }
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
