#include "rocprofvis_data_provider.h"
#include "rocprofvis_controller.h"
#include "rocprofvis_core_assert.h"
#include "rocprofvis_events.h"

#include "spdlog/spdlog.h"

#include <cfloat>
#include <iostream>
#include <sstream>
#include <future>

namespace RocProfVis
{
namespace View
{

const uint64_t DataProvider::EVENT_TABLE_REQUEST_ID  = MakeRequestId(RequestType::kFetchTrackEventTable);
const uint64_t DataProvider::SAMPLE_TABLE_REQUEST_ID = MakeRequestId(RequestType::kFetchTrackSampleTable);
const uint64_t DataProvider::EVENT_EXTENDED_DATA_REQUEST_ID = MakeRequestId(RequestType::kFetchEventExtendedData);
const uint64_t DataProvider::EVENT_FLOW_DATA_REQUEST_ID     = MakeRequestId(RequestType::kFetchEventFlowDetails);
const uint64_t DataProvider::EVENT_CALL_STACK_DATA_REQUEST_ID = MakeRequestId(RequestType::kFetchEventCallStack);
const uint64_t DataProvider::SAVE_TRIMMED_TRACE_REQUEST_ID = MakeRequestId(RequestType::kSaveTrimmedTrace);

DataProvider::DataProvider()
: m_state(ProviderState::kInit)
, m_trace_future(nullptr)
, m_trace_controller(nullptr)
, m_trace_timeline(nullptr)
, m_track_data_ready_callback(nullptr)
, m_trace_data_ready_callback(nullptr)
, m_track_metadata_changed_callback(nullptr)
, m_table_data_ready_callback(nullptr)
, m_save_trace_callback(nullptr)
, m_num_graphs(0)
, m_min_ts(0)
, m_max_ts(0)
, m_trace_file_path("")
, m_progress_percent(0)
, m_table_infos(static_cast<size_t>(TableType::__kTableTypeCount))
{}

DataProvider::~DataProvider() { CloseController(); }

const event_info_t*
DataProvider::GetEventInfo(uint64_t event_id) const
{
    if(m_event_data.count(event_id) > 0)
    {
        return &m_event_data.at(event_id);
    }
    return nullptr;
}

void
DataProvider::CloseController()
{
    if(m_trace_future)
    {
        rocprofvis_controller_future_free(m_trace_future);
        m_trace_future = nullptr;
    }
    if(m_trace_timeline)
    {
        m_trace_timeline = nullptr;
    }

    FreeAllTracks();
    FreeRequests();
    m_state      = ProviderState::kInit;
    m_num_graphs = 0;
    m_min_ts     = 0;
    m_max_ts     = 0;
    m_trace_file_path.clear();

    if(m_trace_controller)
    {
        rocprofvis_controller_free(m_trace_controller);
        m_trace_controller = nullptr;
    }
}

void
DataProvider::SetSelectedState(const std::string& id)
{
    if(id == m_trace_file_path)
    {
        rocprofvis_controller_set_uint64(m_trace_controller, kRPVControllerNotifySelected,
                                         0, true);
    }
}

void
DataProvider::FreeRequests()
{
    for(auto item : m_requests)
    {
        data_req_info_t& req = item.second;

        if(req.request_future) {
            spdlog::debug("FreeRequests: cancelling request {} of type {}", req.request_id,
                static_cast<int>(req.request_type));

            rocprofvis_result_t result =
                rocprofvis_controller_future_cancel(req.request_future);
            if(result != kRocProfVisResultSuccess)
            {
                spdlog::warn("Failed to cancel request {}: {}", req.request_id,
                            static_cast<int>(result));
            }

            result = rocprofvis_controller_future_wait(req.request_future, FLT_MAX);
            if(result != kRocProfVisResultSuccess)
            {
                spdlog::warn("Failed to wait for request {}: {}", req.request_id,
                            static_cast<int>(result));
            }
            
            rocprofvis_controller_future_free(req.request_future);
            req.request_future = nullptr;
        }
        // Free the request resources
        if(req.request_array)
        {
            rocprofvis_controller_array_free(req.request_array);
            req.request_array = nullptr;
        }
        if(req.request_args)
        {
            rocprofvis_controller_arguments_free(req.request_args);
            req.request_args = nullptr;
        }
        if(req.request_obj_handle)
        {
            req.request_obj_handle = nullptr;
        }
    }

    m_requests.clear();
}

void
DataProvider::FreeAllTracks()
{
    for(auto& it : m_raw_trackdata)
    {
        if(it.second)
        {
            delete it.second;
        }
    }
    m_raw_trackdata.clear();
}

double
DataProvider::GetStartTime()
{
    return m_min_ts;
}

double
DataProvider::GetEndTime()
{
    return m_max_ts;
}

uint64_t
DataProvider::GetTrackCount()
{
    return m_num_graphs;
}

std::vector<const node_info_t*>
DataProvider::GetNodeInfoList() const
{
    std::vector<const node_info_t*> nodes(m_node_infos.size());
    int                             count = 0;
    for(auto& it : m_node_infos)
    {
        nodes[count] = &it.second;
        count++;
    }
    return nodes;
}

const device_info_t*
DataProvider::GetDeviceInfo(uint64_t device_id) const
{
    if(m_device_infos.count(device_id) > 0)
    {
        return &m_device_infos.at(device_id);
    }
    return nullptr;
}

const process_info_t*
DataProvider::GetProcessInfo(uint64_t process_id) const
{
    if(m_process_infos.count(process_id) > 0)
    {
        return &m_process_infos.at(process_id);
    }
    return nullptr;
}

const thread_info_t*
DataProvider::GetThreadInfo(uint64_t thread_id) const
{
    if(m_thread_infos.count(thread_id) > 0)
    {
        return &m_thread_infos.at(thread_id);
    }
    return nullptr;
}

const queue_info_t*
DataProvider::GetQueueInfo(uint64_t queue_id) const
{
    if(m_queue_infos.count(queue_id) > 0)
    {
        return &m_queue_infos.at(queue_id);
    }
    return nullptr;
}

const stream_info_t*
DataProvider::GetStreamInfo(uint64_t stream_id) const
{
    if(m_stream_infos.count(stream_id) > 0)
    {
        return &m_stream_infos.at(stream_id);
    }
    return nullptr;
}

const counter_info_t*
DataProvider::GetCounterInfo(uint64_t counter_id) const
{
    if(m_counter_infos.count(counter_id) > 0)
    {
        return &m_counter_infos.at(counter_id);
    }
    return nullptr;
}

const std::string&
DataProvider::GetTraceFilePath()
{
    return m_trace_file_path;
}

ProviderState
DataProvider::GetState()
{
    return m_state;
}

const std::vector<std::string>&
DataProvider::GetTableHeader(TableType type)
{
    return m_table_infos[static_cast<size_t>(type)].table_header;
}

const std::vector<std::vector<std::string>>&
DataProvider::GetTableData(TableType type)
{
    return m_table_infos[static_cast<size_t>(type)].table_data;
}

const std::vector<formatted_column_info_t>&
DataProvider::GetFormattedTableData(TableType type)
{
    return m_table_infos[static_cast<size_t>(type)].formatted_column_data;
}

std::vector<formatted_column_info_t>&
DataProvider::GetMutableFormattedTableData(TableType type)
{
    return m_table_infos[static_cast<size_t>(type)].formatted_column_data;
}

std::shared_ptr<TableRequestParams>
DataProvider::GetTableParams(TableType type)
{
    return m_table_infos[static_cast<size_t>(type)].table_params;
}

uint64_t
DataProvider::GetTableTotalRowCount(TableType type)
{
    return m_table_infos[static_cast<size_t>(type)].total_row_count;
}

const char*
DataProvider::GetProgressMessage()
{
    return m_progress_mesage.c_str();
}

void
DataProvider::ClearTable(TableType type)
{
    m_table_infos[static_cast<size_t>(type)].table_header.clear();
    m_table_infos[static_cast<size_t>(type)].table_data.clear();
    m_table_infos[static_cast<size_t>(type)].total_row_count = 0;
    m_table_infos[static_cast<size_t>(type)].table_params.reset();
    m_table_infos[static_cast<size_t>(type)].formatted_column_data.clear();
}

void
DataProvider::SetTrackMetadataChangedCallback(
    const std::function<void(const std::string&)>& callback)
{
    m_track_metadata_changed_callback = callback;
}

void
DataProvider::SetTableDataReadyCallback(
    const std::function<void(const std::string&, uint64_t)>& callback)
{
    m_table_data_ready_callback = callback;
}

void
DataProvider::SetTrackDataReadyCallback(
    const std::function<void(uint64_t, const std::string&, const data_req_info_t&)>& callback)
{
    m_track_data_ready_callback = callback;
}

void
DataProvider::SetTraceLoadedCallback(
    const std::function<void(const std::string&, uint64_t)>& callback)
{
    m_trace_data_ready_callback = callback;
}

void
DataProvider::SetSaveTraceCallback(
    const std::function<void(bool)>& callback)
{
    m_save_trace_callback = callback;
}

bool
DataProvider::SetGraphIndex(uint64_t track_id, uint64_t index)
{
    rocprofvis_result_t result = kRocProfVisResultUnknownError;
    if(m_state == ProviderState::kReady)
    {
        ROCPROFVIS_ASSERT(index < m_num_graphs);
        const track_info_t* metadata = GetTrackInfo(track_id);
        ROCPROFVIS_ASSERT(metadata);
        result = rocprofvis_controller_set_object(m_trace_timeline,
                                                  kRPVControllerTimelineGraphIndexed,
                                                  index, metadata->graph_handle);
        if(result == kRocProfVisResultSuccess)
        {
            rocprofvis_handle_t* graph = nullptr;
            for(int i = 0; i < m_num_graphs; i++)
            {
                result = rocprofvis_controller_get_object(
                    m_trace_timeline, kRPVControllerTimelineGraphIndexed, i, &graph);
                if(result == kRocProfVisResultSuccess && graph)
                {
                    uint64_t id = 0;
                    result      = rocprofvis_controller_get_uint64(
                        graph, kRPVControllerGraphId, 0, &id);
                    if(result == kRocProfVisResultSuccess)
                    {
                        metadata = GetTrackInfo(id);
                        ROCPROFVIS_ASSERT(metadata && metadata->graph_handle == graph);
                        m_track_metadata[id].index = i;
                    }
                }
            }
            if(m_track_metadata_changed_callback)
            {
                m_track_metadata_changed_callback(m_trace_file_path);
            }
        }
    }
    return (result == kRocProfVisResultSuccess);
}

bool
DataProvider::FetchTrace(const std::string& file_path)
{
    if(m_state == ProviderState::kLoading || m_state == ProviderState::kError)
    {
        spdlog::debug("Cannot fetch, provider busy or error, state: {}",
                      static_cast<int>(m_state));
        return false;
    }

    // free any previously acquired resources
    CloseController();
    m_trace_file_path  = file_path;
    m_trace_controller = rocprofvis_controller_alloc();
    if(m_trace_controller)
    {
        rocprofvis_result_t result = kRocProfVisResultUnknownError;
        m_trace_future             = rocprofvis_controller_future_alloc();
        if(m_trace_future)
        {
            result = rocprofvis_controller_load_async(m_trace_controller,
                                                      file_path.c_str(), m_trace_future);
            ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);

            if(result != kRocProfVisResultSuccess)
            {
                rocprofvis_controller_future_free(m_trace_future);
                m_trace_future = nullptr;
            }
        }
        if(result != kRocProfVisResultSuccess)
        {
            rocprofvis_controller_free(m_trace_controller);
            m_trace_controller = nullptr;
            return false;
        }

        m_state = ProviderState::kLoading;
        spdlog::debug("Provider load started, state: {}, controller: {}",
                      static_cast<int>(m_state),
                      reinterpret_cast<unsigned long long>(m_trace_controller));
    }

    return true;
}

void
DataProvider::Update()
{
    // process initial controller loading state
    if(m_state == ProviderState::kLoading)
    {
        HandleLoadTrace();
    }
    else if(m_state == ProviderState::kReady)
    {
        HandleRequests();
    }
}

void
DataProvider::HandleLoadTrace()
{
    if(m_trace_future)
    {
        rocprofvis_result_t result = rocprofvis_controller_future_wait(m_trace_future, 0);
        ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess ||
                          result == kRocProfVisResultTimeout);
        if(result == kRocProfVisResultSuccess)
        {
            uint64_t uint64_result = 0;
            result                 = rocprofvis_controller_get_uint64(
                m_trace_future, kRPVControllerFutureResult, 0, &uint64_result);
            ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);

            if(uint64_result != kRocProfVisResultSuccess) 
            {
                spdlog::error("Failed to load trace file: {}, error code: {}",
                              m_trace_file_path, uint64_result);
                m_state = ProviderState::kError;
                if(m_trace_data_ready_callback)
                {
                    m_trace_data_ready_callback(m_trace_file_path, uint64_result);
                }
                return;
            }

            // Load the system topology
            HandleLoadSystemTopology();

            result = rocprofvis_controller_get_object(
                m_trace_controller, kRPVControllerTimeline, 0, &m_trace_timeline);

            // store timeline meta data
            if(result == kRocProfVisResultSuccess && m_trace_timeline)
            {
                m_num_graphs = 0;
                result       = rocprofvis_controller_get_uint64(
                    m_trace_timeline, kRPVControllerTimelineNumGraphs, 0, &m_num_graphs);
                ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);

                m_min_ts = 0;
                result   = rocprofvis_controller_get_double(
                    m_trace_timeline, kRPVControllerTimelineMinTimestamp, 0, &m_min_ts);
                ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);

                m_max_ts = 0;
                result   = rocprofvis_controller_get_double(
                    m_trace_timeline, kRPVControllerTimelineMaxTimestamp, 0, &m_max_ts);
                ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);

                spdlog::debug("Timeline parameters: tracks {} min ts {} max ts {}",
                              m_num_graphs, m_min_ts, m_max_ts);

                // get the per track meta data
                HandleLoadTrackMetaData();
            } else {
                spdlog::error("Failed to get timeline object from controller");
                m_state = ProviderState::kError;
                if(m_trace_data_ready_callback)
                {
                    m_trace_data_ready_callback(m_trace_file_path, result == kRocProfVisResultSuccess
                                                ? kRocProfVisResultUnknownError
                                                : result);
                }
            }

            // trace loaded successfully, free the future pointer
            rocprofvis_controller_future_free(m_trace_future);
            m_trace_future = nullptr;

            m_state = ProviderState::kReady;
            // fire callback
            if(m_trace_data_ready_callback)
            {
                m_trace_data_ready_callback(m_trace_file_path, kRocProfVisResultSuccess);
            }
        }
        else
        {
            // timed out, do nothing and try again later
            uint64_t            progress_percent;
            rocprofvis_result_t result = rocprofvis_controller_get_uint64(
                m_trace_controller, kRPVControllerGetDmProgress, 0, &progress_percent);
            if(result == kRocProfVisResultSuccess)
            {
               if (progress_percent != m_progress_percent)
               {
                   uint32_t            length = 0;
                   rocprofvis_result_t result = rocprofvis_controller_get_string(
                       m_trace_controller, kRPVControllerGetDmMessage, 0, nullptr,
                       &length);
                   if(result == kRocProfVisResultSuccess)
                   {
                       length++;
                       char* str_buffer = new char[length];
                       result           = rocprofvis_controller_get_string(
                           m_trace_controller, kRPVControllerGetDmMessage, 0, str_buffer,
                           &length);
                       m_progress_mesage = std::string(str_buffer);
                       delete[] str_buffer;
                   }
               }
               m_progress_percent = progress_percent; 
            }
        }
    }
}

void
DataProvider::HandleLoadSystemTopology()
{
    uint64_t            num_nodes;
    rocprofvis_result_t result = rocprofvis_controller_get_uint64(
        m_trace_controller, kRPVControllerNumNodes, 0, &num_nodes);
    ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
    // Query nodes...
    for(int i = 0; i < num_nodes; i++)
    {
        rocprofvis_handle_t* node_handle;
        result = rocprofvis_controller_get_object(
            m_trace_controller, kRPVControllerNodeIndexed, i, &node_handle);
        ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess && node_handle);
        node_info_t node_info;
        result = rocprofvis_controller_get_uint64(node_handle, kRPVControllerNodeId, 0,
                                                  &node_info.id);
        ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
        node_info.host_name  = GetString(node_handle, kRPVControllerNodeHostName, 0);
        node_info.os_name    = GetString(node_handle, kRPVControllerNodeOSName, 0);
        node_info.os_release = GetString(node_handle, kRPVControllerNodeOSRelease, 0);
        node_info.os_version = GetString(node_handle, kRPVControllerNodeOSVersion, 0);
        uint64_t num_processors;
        result = rocprofvis_controller_get_uint64(
            node_handle, kRPVControllerNodeNumProcessors, 0, &num_processors);
        ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
        node_info.device_ids.resize(num_processors);
        // Query devices...
        for(int j = 0; j < num_processors; j++)
        {
            rocprofvis_handle_t* processor_handle;
            result = rocprofvis_controller_get_object(
                node_handle, kRPVControllerNodeProcessorIndexed, j, &processor_handle);
            ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess && processor_handle);
            device_info_t device_info;
            result = rocprofvis_controller_get_uint64(
                processor_handle, kRPVControllerProcessorId, 0, &device_info.id);
            ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
            device_info.product_name =
                GetString(processor_handle, kRPVControllerProcessorProductName, 0);
            device_info.type =
                GetString(processor_handle, kRPVControllerProcessorType, 0);
            result = rocprofvis_controller_get_uint64(processor_handle,
                                                      kRPVControllerProcessorTypeIndex, 0,
                                                      &device_info.type_index);
            ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
            m_device_infos[device_info.id] = std::move(device_info);
            node_info.device_ids[j]        = device_info.id;
        }
        uint64_t num_processes;
        result = rocprofvis_controller_get_uint64(
            node_handle, kRPVControllerNodeNumProcesses, 0, &num_processes);
        ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
        node_info.process_ids.resize(num_processes);
        // Query processes...
        for(int j = 0; j < num_processes; j++)
        {
            rocprofvis_handle_t* process_handle;
            result = rocprofvis_controller_get_object(
                node_handle, kRPVControllerNodeProcessIndexed, j, &process_handle);
            ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess && node_handle);
            process_info_t process_info;
            result = rocprofvis_controller_get_uint64(
                process_handle, kRPVControllerProcessId, 0, &process_info.id);
            ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
            result = rocprofvis_controller_get_double(process_handle,
                                                      kRPVControllerProcessStartTime, 0,
                                                      &process_info.start_time);
            ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
            result = rocprofvis_controller_get_double(
                process_handle, kRPVControllerProcessEndTime, 0, &process_info.end_time);
            process_info.command =
                GetString(process_handle, kRPVControllerProcessCommand, 0);
            process_info.environment =
                GetString(process_handle, kRPVControllerProcessEnvironment, 0);
            ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
            uint64_t num_threads;
            result = rocprofvis_controller_get_uint64(
                process_handle, kRPVControllerProcessNumThreads, 0, &num_threads);
            ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
            process_info.thread_ids.reserve(num_threads);
            // Query threads...
            for(int k = 0; k < num_threads; k++)
            {
                rocprofvis_handle_t* thread_handle;
                result = rocprofvis_controller_get_object(
                    process_handle, kRPVControllerProcessThreadIndexed, k,
                    &thread_handle);
                ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess && thread_handle);
                rocprofvis_handle_t* track;
                result = rocprofvis_controller_get_object(
                    thread_handle, kRPVControllerThreadTrack, 0, &track);
                ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
                if(track)
                {
                    thread_info_t thread_info;
                    result = rocprofvis_controller_get_uint64(
                        thread_handle, kRPVControllerThreadId, 0, &thread_info.id);
                    ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
                    thread_info.name = GetString(thread_handle, kRPVControllerThreadName, 0);
                    result           = rocprofvis_controller_get_double(thread_handle,
                                                                        kRPVControllerThreadStartTime,
                                                                        0, &thread_info.start_time);
                    ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
                    result = rocprofvis_controller_get_double(
                        thread_handle, kRPVControllerThreadEndTime, 0, &thread_info.end_time);
                    ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
                    process_info.thread_ids.push_back(thread_info.id);
                    m_thread_infos[thread_info.id] = std::move(thread_info);   
                }             
            }
            uint64_t num_queues;
            result = rocprofvis_controller_get_uint64(
                process_handle, kRPVControllerProcessNumQueues, 0, &num_queues);
            ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
            process_info.queue_ids.reserve(num_queues);
            // Query queues...
            for(int k = 0; k < num_queues; k++)
            {
                rocprofvis_handle_t* queue_handle;
                result = rocprofvis_controller_get_object(
                    process_handle, kRPVControllerProcessQueueIndexed, k, &queue_handle);
                ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess && queue_handle);
                rocprofvis_handle_t* track;
                result = rocprofvis_controller_get_object(
                    queue_handle, kRPVControllerQueueTrack, 0, &track);
                ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
                if(track)
                {
                    queue_info_t queue_info;
                    result = rocprofvis_controller_get_uint64(
                        queue_handle, kRPVControllerQueueId, 0, &queue_info.id);
                    ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
                    queue_info.name = GetString(queue_handle, kRPVControllerQueueName, 0);
                    rocprofvis_handle_t* processor_handle;
                    result = rocprofvis_controller_get_object(
                        queue_handle, kRPVControllerQueueProcessor, 0, &processor_handle);
                    ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
                    if(processor_handle)
                    {
                        result = rocprofvis_controller_get_uint64(processor_handle,
                                                                  kRPVControllerProcessorId,
                                                                  0, &queue_info.device_id);
                        ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
                    }
                    process_info.queue_ids.push_back(queue_info.id);
                    m_queue_infos[queue_info.id] = std::move(queue_info);
                }               
            }
            uint64_t num_streams;
            result = rocprofvis_controller_get_uint64(
                process_handle, kRPVControllerProcessNumStreams, 0, &num_streams);
            ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
            process_info.stream_ids.reserve(num_streams);
            // Query streams...
            for(int k = 0; k < num_streams; k++)
            {
                rocprofvis_handle_t* stream_handle;
                result = rocprofvis_controller_get_object(
                    process_handle, kRPVControllerProcessStreamIndexed, k, &stream_handle);
                ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess && stream_handle);
                rocprofvis_handle_t* track;
                result = rocprofvis_controller_get_object(
                    stream_handle, kRPVControllerStreamTrack, 0, &track);
                ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
                if(track)
                {
                    stream_info_t stream_info;
                    result = rocprofvis_controller_get_uint64(
                        stream_handle, kRPVControllerStreamId, 0, &stream_info.id);
                    ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
                    stream_info.name = GetString(stream_handle, kRPVControllerStreamName, 0);
                    rocprofvis_handle_t* processor_handle;
                    result = rocprofvis_controller_get_object(
                        stream_handle, kRPVControllerStreamProcessor, 0, &processor_handle);
                    ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
                    if(processor_handle)
                    {
                        result = rocprofvis_controller_get_uint64(processor_handle,
                                                                  kRPVControllerProcessorId,
                                                                  0, &stream_info.device_id);
                        ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
                    }
                    process_info.stream_ids.push_back(stream_info.id);
                    m_stream_infos[stream_info.id] = std::move(stream_info); 
                }
               
            }
            uint64_t num_counters;
            result = rocprofvis_controller_get_uint64(
                process_handle, kRPVControllerProcessNumCounters, 0, &num_counters);
            ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
            process_info.counter_ids.reserve(num_counters);
            // Query counters...
            for(int k = 0; k < num_counters; k++)
            {
                rocprofvis_handle_t* counter_handle;
                result = rocprofvis_controller_get_object(
                    process_handle, kRPVControllerProcessCounterIndexed, k,
                    &counter_handle);
                ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess && counter_handle);
                rocprofvis_handle_t* track;
                result = rocprofvis_controller_get_object(
                    counter_handle, kRPVControllerCounterTrack, 0, &track);
                ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
                if(track)
                {
                    counter_info_t counter_info;
                    result = rocprofvis_controller_get_uint64(
                        counter_handle, kRPVControllerCounterId, 0, &counter_info.id);
                    ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
                    counter_info.name =
                        GetString(counter_handle, kRPVControllerCounterName, 0);
                    rocprofvis_handle_t* processor_handle;
                    result = rocprofvis_controller_get_object(
                        counter_handle, kRPVControllerCounterProcessor, 0,
                        &processor_handle);
                    ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
                    if(processor_handle)
                    {
                        result = rocprofvis_controller_get_uint64(
                            processor_handle, kRPVControllerProcessorId, 0,
                            &counter_info.device_id);
                        ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
                    }
                    counter_info.description =
                        GetString(counter_handle, kRPVControllerCounterDescription, 0);
                    counter_info.units =
                        GetString(counter_handle, kRPVControllerCounterUnits, 0);
                    counter_info.value_type =
                        GetString(counter_handle, kRPVControllerCounterValueType, 0);
                    process_info.counter_ids.push_back(counter_info.id);
                    m_counter_infos[counter_info.id] = std::move(counter_info);                    
                }
            }
            m_process_infos[process_info.id] = std::move(process_info);
            node_info.process_ids[j]         = process_info.id;
        }
        m_node_infos[node_info.id] = std::move(node_info);
    }
}

void
DataProvider::HandleLoadTrackMetaData()
{
    m_track_metadata.clear();
    FreeAllTracks();

    size_t str_buffer_length = 128;
    char*  str_buffer        = new char[str_buffer_length];

    for(uint64_t i = 0; i < m_num_graphs; i++)
    {
        rocprofvis_handle_t* graph  = nullptr;
        rocprofvis_result_t  result = rocprofvis_controller_get_object(
            m_trace_timeline, kRPVControllerTimelineGraphIndexed, i, &graph);
        ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess && graph);

        rocprofvis_handle_t* track = nullptr;
        result =
            rocprofvis_controller_get_object(graph, kRPVControllerGraphTrack, 0, &track);
        ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess && track);

        if(result == kRocProfVisResultSuccess)
        {
            track_info_t track_info;

            track_info.graph_handle = graph;

            track_info.index = i;
            result = rocprofvis_controller_get_uint64(track, kRPVControllerTrackId, 0,
                                                      &track_info.id);
            ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);

            uint64_t track_type = 0;
            result = rocprofvis_controller_get_uint64(track, kRPVControllerTrackType, 0,
                                                      &track_type);
            ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess &&
                              (track_type == kRPVControllerTrackTypeEvents ||
                               track_type == kRPVControllerTrackTypeSamples));
            track_info.track_type = track_type == kRPVControllerTrackTypeSamples
                                        ? kRPVControllerTrackTypeSamples
                                        : kRPVControllerTrackTypeEvents;

            // get track name
            uint32_t length = 0;
            result = rocprofvis_controller_get_string(track, kRPVControllerTrackName, 0,
                                                      nullptr, &length);
            ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);

            if(length >= str_buffer_length)
            {
                delete[] str_buffer;
                str_buffer_length = length + 1;
                str_buffer        = new char[str_buffer_length];
            }
            length += 1;
            result = rocprofvis_controller_get_string(track, kRPVControllerTrackName, 0,
                                                      str_buffer, &length);
            ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
            track_info.name = std::string(str_buffer);

            result = rocprofvis_controller_get_double(
                track, kRPVControllerTrackMinTimestamp, 0, &track_info.min_ts);
            ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);

            result = rocprofvis_controller_get_double(
                track, kRPVControllerTrackMaxTimestamp, 0, &track_info.max_ts);
            ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);

            result = rocprofvis_controller_get_uint64(
                track, kRPVControllerTrackNumberOfEntries, 0, &track_info.num_entries);
            ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);

            result = rocprofvis_controller_get_double(track, kRPVControllerTrackMinValue,
                                                      0, &track_info.min_value);
            ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);

            result = rocprofvis_controller_get_double(track, kRPVControllerTrackMaxValue,
                                                      0, &track_info.max_value);
            ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);

            // Determine queue/thread/counter that this track represents.
            rocprofvis_handle_t* queue = nullptr;
            result = rocprofvis_controller_get_object(track, kRPVControllerTrackQueue, 0,
                                                      &queue);
            ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
            rocprofvis_handle_t* stream = nullptr;
            result = rocprofvis_controller_get_object(track, kRPVControllerTrackStream, 0,
                                                      &stream);
            ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
            rocprofvis_handle_t* thread = nullptr;
            result = rocprofvis_controller_get_object(track, kRPVControllerTrackThread, 0,
                                                      &thread);
            ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
            rocprofvis_handle_t* counter = nullptr;
            result = rocprofvis_controller_get_object(track, kRPVControllerTrackCounter,
                                                      0, &counter);
            ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
            rocprofvis_handle_t* process   = nullptr;
            rocprofvis_handle_t* node      = nullptr;
            rocprofvis_handle_t* processor = nullptr;
            if(queue)
            {
                result = rocprofvis_controller_get_uint64(queue, kRPVControllerQueueId, 0,
                                                          &track_info.topology.id);
                ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
                result = rocprofvis_controller_get_object(
                    queue, kRPVControllerQueueProcess, 0, &process);
                ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
                result = rocprofvis_controller_get_object(queue, kRPVControllerQueueNode,
                                                          0, &node);
                ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
                result = rocprofvis_controller_get_object(
                    queue, kRPVControllerQueueProcessor, 0, &processor);
                ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
                track_info.topology.type = track_info_t::Topology::Queue;
            }
            else if(stream)
            {
                result = rocprofvis_controller_get_uint64(stream, kRPVControllerStreamId, 0,
                                                          &track_info.topology.id);
                ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
                result = rocprofvis_controller_get_object(
                    stream, kRPVControllerStreamProcess, 0, &process);
                ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
                result = rocprofvis_controller_get_object(stream, kRPVControllerStreamNode,
                                                          0, &node);
                ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
                result = rocprofvis_controller_get_object(
                    stream, kRPVControllerStreamProcessor, 0, &processor);
                ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
                track_info.topology.type = track_info_t::Topology::Stream;
            }
            else if(thread)
            {
                result = rocprofvis_controller_get_uint64(thread, kRPVControllerThreadId,
                                                          0, &track_info.topology.id);
                ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
                result = rocprofvis_controller_get_object(
                    thread, kRPVControllerThreadProcess, 0, &process);
                ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
                result = rocprofvis_controller_get_object(
                    thread, kRPVControllerThreadNode, 0, &node);
                ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
                track_info.topology.type = track_info_t::Topology::Thread;
            }
            else if(counter)
            {
                result = rocprofvis_controller_get_uint64(
                    counter, kRPVControllerCounterId, 0, &track_info.topology.id);
                ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
                result = rocprofvis_controller_get_object(
                    counter, kRPVControllerCounterProcess, 0, &process);
                ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
                result = rocprofvis_controller_get_object(
                    counter, kRPVControllerCounterNode, 0, &node);
                ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
                result = rocprofvis_controller_get_object(
                    queue, kRPVControllerCounterProcessor, 0, &processor);
                track_info.topology.type = track_info_t::Topology::Counter;
            }
            else
            {
                track_info.topology.type = track_info_t::Topology::Unknown;
                spdlog::warn("No thread/queue/counter binding found for track {}",
                             track_info.id);
            }
            // Determine process, device, node this track belongs to.
            if(process)
            {
                result = rocprofvis_controller_get_uint64(
                    process, kRPVControllerProcessId, 0, &track_info.topology.process_id);
                ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
            }
            if(node)
            {
                result = rocprofvis_controller_get_uint64(node, kRPVControllerNodeId, 0,
                                                          &track_info.topology.node_id);
                ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
            }
            if(processor)
            {
                result =
                    rocprofvis_controller_get_uint64(processor, kRPVControllerProcessorId,
                                                     0, &track_info.topology.device_id);
                ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
            }

            // Todo:
            // kRPVControllerTrackDescription = 0x30000007,
            // kRPVControllerTrackNode = 0x3000000A,
            // kRPVControllerTrackProcessor = 0x3000000B,

            m_raw_trackdata[track_info.id]  = nullptr;
            m_track_metadata[track_info.id] = std::move(track_info);
        }
        else
        {
            spdlog::debug("Error getting track meta data for track at index: {}", i);
        }
    }

    delete[] str_buffer;

    spdlog::info("Track meta data loaded");
}

bool
DataProvider::SaveTrimmedTrace(const std::string& path, double start_ns, double end_ns)
{
    spdlog::debug("Saving trimmed trace to path: {}, start: {}, end: {}",
                  path, start_ns, end_ns);

    if(m_state != ProviderState::kReady)
    {
        spdlog::debug("Cannot save, provider not ready or error, state: {}",
                      static_cast<int>(m_state));
        return false;
    }

    if(m_requests.find(SAVE_TRIMMED_TRACE_REQUEST_ID) != m_requests.end())
    {
        spdlog::debug("Save request already pending");
        return false;
    }

    rocprofvis_handle_t* trim_future = rocprofvis_controller_future_alloc();
    ROCPROFVIS_ASSERT(trim_future);
    
    rocprofvis_result_t result = rocprofvis_controller_save_trimmed_trace(m_trace_controller, start_ns,
    end_ns, path.c_str(), trim_future);

    if(result == kRocProfVisResultSuccess && trim_future)
    {
        spdlog::debug("Trimmed trace save request submitted successfully");
        data_req_info_t request_info;
        request_info.request_array      = nullptr;
        request_info.request_future     = trim_future;
        request_info.request_obj_handle = nullptr;
        request_info.request_args       = nullptr;
        request_info.request_id         = SAVE_TRIMMED_TRACE_REQUEST_ID;
        request_info.loading_state      = ProviderState::kLoading;
        request_info.request_type       = RequestType::kSaveTrimmedTrace;
        m_requests.emplace(request_info.request_id, request_info);
        return true;
    }
    else
    {
        spdlog::error("Failed to submit trimmed trace save request, result: {}",
                      static_cast<int>(result));
        rocprofvis_controller_future_free(trim_future);
    }

    return false;
}

bool
DataProvider::FetchWholeTrack(uint32_t track_id, double start_ts, double end_ts,
                              uint32_t horz_pixel_range, uint8_t group_id,
                              uint16_t chunk_index, size_t chunk_count)
{
    if(m_state != ProviderState::kReady)
    {
        spdlog::debug("Cannot fetch, provider not ready or error, state: {}",
                      static_cast<int>(m_state));
        return false;
    }

    uint64_t request_id =
        MakeTrackDataRequestId(track_id, chunk_index, group_id, RequestType::kFetchTrack);

    const track_info_t* metadata = GetTrackInfo(track_id);
    if(metadata)
    {

        auto it = m_requests.find(request_id);
        // only allow load if a request for this id (track) is not pending
        if(it == m_requests.end())
        {
            rocprofvis_handle_t* track_future = rocprofvis_controller_future_alloc();
            rocprofvis_controller_array_t* track_array =
                rocprofvis_controller_array_alloc(metadata->num_entries);
            rocprofvis_handle_t* track_handle = nullptr;
            rocprofvis_result_t  result       = rocprofvis_controller_get_object(
                m_trace_controller, kRPVControllerTrackById, track_id, &track_handle);

            if(result == kRocProfVisResultSuccess && track_handle && track_future &&
               track_array)
            {
                result = rocprofvis_controller_track_fetch_async(
                    m_trace_controller, (rocprofvis_controller_track_t*) track_handle,
                    start_ts, end_ts, track_future, track_array);
            
                ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);

                data_req_info_t request_info;
                request_info.request_array      = track_array;
                request_info.request_future     = track_future;
                request_info.request_obj_handle = track_handle;
                request_info.request_args       = nullptr;
                request_info.request_id         = request_id; //track_id;
                request_info.loading_state      = ProviderState::kLoading;
                request_info.request_type       = RequestType::kFetchTrack;

                auto params = std::make_shared<TrackRequestParams>(track_id, start_ts, end_ts,
                                                                horz_pixel_range, group_id,
                                                                chunk_index, chunk_count);
                request_info.custom_params = params;

                request_info.request_time = std::chrono::steady_clock::now();
                m_requests.emplace(request_info.request_id, request_info);

                spdlog::debug("Fetching track graph data {}", track_id);
                return true;
            } 
            else 
            {
                rocprofvis_controller_array_free(track_array);
                rocprofvis_controller_future_free(track_future);
                spdlog::debug("Failed to fetch track graph data {}, result: {}",
                              track_id, static_cast<int>(result));
            }
        }
        else
        {
            // request for item already exists
            spdlog::debug("Request for this track, index {}, is already pending",
                          track_id);
        }
    }
    else
    {
        spdlog::debug("Cannot fetch Track index {} is out of range", track_id);
    }
    return false;
}

std::pair<bool, uint64_t>
DataProvider::FetchTrack(uint32_t track_id, double start_ts, double end_ts,
                         uint32_t horz_pixel_range, uint8_t group_id,
                         uint16_t chunk_index, size_t chunk_count)
{
    TrackRequestParams request_params(track_id, start_ts, end_ts, horz_pixel_range,
                                      group_id, chunk_index, chunk_count);
    return FetchTrack(request_params);
}

std::pair<bool, uint64_t>
DataProvider::FetchTrack(const TrackRequestParams& request_params)
{
    if(m_state != ProviderState::kReady)
    {
        spdlog::debug("Cannot fetch, provider not ready or error, state: {}",
                      static_cast<int>(m_state));
        return {false,0};
    }

    uint64_t request_id =
        MakeTrackDataRequestId(request_params.m_track_id, request_params.m_chunk_index,
                               request_params.m_data_group_id, RequestType::kFetchGraph);

    if(GetTrackInfo(request_params.m_track_id))
    {
        auto it = m_requests.find(request_id);
        // only allow load if a request for this id (track chunk) is not pending
        if(it == m_requests.end())
        {
            rocprofvis_handle_t* graph_future = rocprofvis_controller_future_alloc();
            rocprofvis_controller_array_t* graph_array =
                rocprofvis_controller_array_alloc(32);
            rocprofvis_handle_t* graph_obj = nullptr;

            rocprofvis_result_t result = rocprofvis_controller_get_object(
                m_trace_timeline, kRPVControllerTimelineGraphById,
                request_params.m_track_id, &graph_obj);

            if(result == kRocProfVisResultSuccess && graph_obj && graph_future &&
               graph_array)
            {
                result = rocprofvis_controller_graph_fetch_async(
                    m_trace_controller, graph_obj, request_params.m_start_ts,
                    request_params.m_end_ts, request_params.m_horz_pixel_range,
                    graph_future, graph_array);

                ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
            
                data_req_info_t request_info;
                request_info.request_array      = graph_array;
                request_info.request_future     = graph_future;
                request_info.request_obj_handle = graph_obj;
                request_info.request_args       = nullptr;
                request_info.request_id         = request_id;
                request_info.loading_state      = ProviderState::kLoading;
                request_info.request_type       = RequestType::kFetchGraph;

                auto params = std::make_shared<TrackRequestParams>(request_params);
                request_info.custom_params = params;

                request_info.request_time = std::chrono::steady_clock::now();
                m_requests.emplace(request_info.request_id, request_info);

                return {true, request_id};
            }
            else 
            {
                rocprofvis_controller_array_free(graph_array);
                rocprofvis_controller_future_free(graph_future);
                spdlog::debug("Failed to fetch track graph data {}, result: {}",
                              request_params.m_track_id, static_cast<int>(result));
            }            
        }
        else
        {
            // request for item already exists
            spdlog::debug("Request for this track, id {}, is already pending",
                          request_params.m_track_id);
        }
    }
    else
    {
        spdlog::debug("Cannot fetch Track id {} is invalid", request_params.m_track_id);
    }
    return {false, 0};
}

bool
DataProvider::SetupCommonTableArguments(rocprofvis_controller_arguments_t* args,
                                        const TableRequestParams&          table_params)
{
    ROCPROFVIS_ASSERT(args != nullptr);

    rocprofvis_result_t result = rocprofvis_controller_set_uint64(
        args, kRPVControllerTableArgsType, 0, table_params.m_table_type);
    ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);

    result = rocprofvis_controller_set_double(args, kRPVControllerTableArgsStartTime, 0,
                                              table_params.m_start_ts);
    ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);

    result = rocprofvis_controller_set_double(args, kRPVControllerTableArgsEndTime, 0,
                                              table_params.m_end_ts);
    ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);

    result = rocprofvis_controller_set_uint64(args, kRPVControllerTableArgsSortColumn, 0,
                                              table_params.m_sort_column_index);
    ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);

    result = rocprofvis_controller_set_uint64(args, kRPVControllerTableArgsSortOrder, 0,
                                              table_params.m_sort_order);
    ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);

    result = rocprofvis_controller_set_string(args, kRPVControllerTableArgsFilter, 0,
                                              table_params.m_filter.data(),
                                              table_params.m_filter.length());
    ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);

    result = rocprofvis_controller_set_string(args, kRPVControllerTableArgsGroup, 0,
                                              table_params.m_group.data(),
                                              table_params.m_group.length());
    ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);

    result = rocprofvis_controller_set_string(args, kRPVControllerTableArgsGroupColumns,
                                              0, table_params.m_group_columns.data(),
                                              table_params.m_group_columns.length());
    ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);

    if(table_params.m_start_row != -1)
    {
        result = rocprofvis_controller_set_uint64(args, kRPVControllerTableArgsStartIndex,
                                                  0, table_params.m_start_row);
        ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
    }

    if(table_params.m_req_row_count != -1)
    {
        result = rocprofvis_controller_set_uint64(args, kRPVControllerTableArgsStartCount,
                                                  0, table_params.m_req_row_count);
        ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
    }
    return true;
}

bool
DataProvider::FetchSingleTrackSampleTable(uint64_t track_id, double start_ts,
                                          double end_ts, char const* filter,
                                          uint64_t start_row, uint64_t req_row_count,
                                          uint64_t sort_column_index,
                                          rocprofvis_controller_sort_order_t sort_order)
{
    return FetchSingleTrackTable(TableRequestParams(
        kRPVControllerTableTypeSamples, { track_id }, start_ts, end_ts, filter, "", "",
        start_row, req_row_count, sort_column_index, sort_order));
}

bool
DataProvider::FetchSingleTrackEventTable(uint64_t track_id, double start_ts,
                                         double end_ts, char const* filter,
                                         char const* group, char const* group_cols,
                                         uint64_t start_row, uint64_t req_row_count,
                                         uint64_t sort_column_index,
                                         rocprofvis_controller_sort_order_t sort_order)
{
    return FetchSingleTrackTable(TableRequestParams(
        kRPVControllerTableTypeEvents, { track_id }, start_ts, end_ts, filter, group,
        group_cols, start_row, req_row_count, sort_column_index, sort_order));
}

bool
DataProvider::FetchSingleTrackTable(const TableRequestParams& table_params)
{
    if(m_state != ProviderState::kReady)
    {
        spdlog::warn("Cannot fetch, provider not ready or error, state: {}",
                     static_cast<int>(m_state));
        return false;
    }

    if(table_params.m_track_ids.empty())
    {
        spdlog::debug("Cannot fetch table, no track id provided");
        return false;
    }

    uint64_t            track_id = table_params.m_track_ids[0];
    const track_info_t* metadata = GetTrackInfo(track_id);

    if(metadata)
    {
        uint64_t request_id = table_params.m_table_type == kRPVControllerTableTypeEvents
                                  ? EVENT_TABLE_REQUEST_ID
                                  : SAMPLE_TABLE_REQUEST_ID;

        auto it = m_requests.find(request_id);

        // check if track is an event track
        if(table_params.m_table_type == kRPVControllerTableTypeEvents &&
           metadata->track_type != kRPVControllerTrackTypeEvents)
        {
            spdlog::warn("Cannot fetch event table, track {} is not an event track",
                         track_id);
            return false;
        }
        // check if track is a sample track
        if(table_params.m_table_type == kRPVControllerTableTypeSamples &&
           metadata->track_type != kRPVControllerTrackTypeSamples)
        {
            spdlog::warn("Cannot fetch sample table, track {} is not a sample track",
                         track_id);
            return false;
        }

        // only allow load if a request for this table type is not pending
        if(it == m_requests.end())
        {
            // get the table handle
            rocprofvis_handle_t* table_handle = nullptr;
            rocprofvis_result_t  result       = kRocProfVisResultUnknownError;

            if(table_params.m_table_type == kRPVControllerTableTypeEvents)
            {
                result = rocprofvis_controller_get_object(
                    m_trace_controller, kRPVControllerEventTable, 0, &table_handle);
            }
            else if(table_params.m_table_type == kRPVControllerTableTypeSamples)
            {
                result = rocprofvis_controller_get_object(
                    m_trace_controller, kRPVControllerSampleTable, 0, &table_handle);
            }
            else
            {
                spdlog::error("Unsupported table type: {}",
                              static_cast<int>(table_params.m_table_type));
                return false;
            }
            ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
            ROCPROFVIS_ASSERT(table_handle);

            // get the track handle
            rocprofvis_handle_t* track_handle = nullptr;
            result                            = rocprofvis_controller_get_object(
                metadata->graph_handle, kRPVControllerGraphTrack, 0, &track_handle);

            // setup arguments for event table request
            rocprofvis_controller_arguments_t* args =
                rocprofvis_controller_arguments_alloc();
            ROCPROFVIS_ASSERT(args != nullptr);

            if(SetupCommonTableArguments(args, table_params))
            {
                result = rocprofvis_controller_set_uint64(
                    args, kRPVControllerTableArgsNumTracks, 0, 1);
                ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);

                result = rocprofvis_controller_set_object(
                    args, kRPVControllerTableArgsTracksIndexed, 0, track_handle);
                ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
            }
            else
            {
                spdlog::error("Failed to setup event table common arguments");
                // free the args
                rocprofvis_controller_arguments_free(args);
                return false;
            }

            // prepare to fetch the table
            rocprofvis_controller_array_t* array = rocprofvis_controller_array_alloc(0);
            ROCPROFVIS_ASSERT(array != nullptr);

            rocprofvis_controller_future_t* future = rocprofvis_controller_future_alloc();
            ROCPROFVIS_ASSERT(future != nullptr);

            result = rocprofvis_controller_table_fetch_async(
                m_trace_controller, table_handle, args, future, array);
            ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);

            // create the request info
            data_req_info_t request_info;
            request_info.request_array      = array;
            request_info.request_future     = future;
            request_info.request_obj_handle = nullptr;
            request_info.request_args       = args;
            request_info.request_id         = request_id;
            request_info.loading_state      = ProviderState::kLoading;
            request_info.request_type =
                (table_params.m_table_type == kRPVControllerTableTypeEvents)
                    ? RequestType::kFetchTrackEventTable
                    : RequestType::kFetchTrackSampleTable;

            std::vector<uint64_t> track_ids;
            track_ids.push_back(track_id);

            auto params         = std::make_shared<TableRequestParams>(table_params);
            params->m_track_ids = std::move(track_ids);
            request_info.custom_params = params;

            m_requests.emplace(request_id, request_info);
            spdlog::debug("Fetching {} table data",
                          (table_params.m_table_type == kRPVControllerTableTypeEvents)
                              ? "event"
                              : "sample");

            return true;
        }
        else
        {
            // request for item already exists
            spdlog::debug("Request for this table, type {}, is already pending",
                          static_cast<uint64_t>(table_params.m_table_type));
            return false;
        }
    }
    else
    {
        spdlog::debug("Cannot fetch track, index {} is out of range", track_id);
        return false;
    }
}

bool
DataProvider::FetchMultiTrackSampleTable(const std::vector<uint64_t>& track_ids,
                                         double start_ts, double end_ts,
                                         char const* filter, uint64_t start_row,
                                         uint64_t req_row_count,
                                         uint64_t sort_column_index,
                                         rocprofvis_controller_sort_order_t sort_order)
{
    return FetchMultiTrackTable(TableRequestParams(
        kRPVControllerTableTypeSamples, track_ids, start_ts, end_ts, filter, "", "",
        start_row, req_row_count, sort_column_index, sort_order));
}

bool
DataProvider::FetchMultiTrackEventTable(const std::vector<uint64_t>& track_ids,
                                        double start_ts, double end_ts,
                                        char const* filter, char const* group,
                                        char const* group_cols, uint64_t start_row,
                                        uint64_t req_row_count,
                                        uint64_t sort_column_index,
                                        rocprofvis_controller_sort_order_t sort_order)

{
    return FetchMultiTrackTable(TableRequestParams(
        kRPVControllerTableTypeEvents, track_ids, start_ts, end_ts, filter, group,
        group_cols, start_row, req_row_count, sort_column_index, sort_order));
}

bool
DataProvider::FetchMultiTrackTable(const TableRequestParams& table_params)
{
    if(m_state != ProviderState::kReady)
    {
        spdlog::warn("Cannot fetch, provider not ready or error, state: {}",
                     static_cast<int>(m_state));
        return false;
    }

    std::vector<uint64_t> filtered_track_ids;
    if(!table_params.m_track_ids.empty())
    {
        uint64_t request_id = table_params.m_table_type == kRPVControllerTableTypeEvents
                                  ? EVENT_TABLE_REQUEST_ID
                                  : SAMPLE_TABLE_REQUEST_ID;

        auto it = m_requests.find(request_id);
        // only allow load if a request for a table of this type is not pending
        if(it == m_requests.end())
        {
            // get the table handle
            rocprofvis_handle_t* table_handle = nullptr;
            rocprofvis_result_t  result       = kRocProfVisResultUnknownError;

            if(table_params.m_table_type == kRPVControllerTableTypeEvents)
            {
                result = rocprofvis_controller_get_object(
                    m_trace_controller, kRPVControllerEventTable, 0, &table_handle);
            }
            else if(table_params.m_table_type == kRPVControllerTableTypeSamples)
            {
                result = rocprofvis_controller_get_object(
                    m_trace_controller, kRPVControllerSampleTable, 0, &table_handle);
            }
            else
            {
                spdlog::error("Unsupported table type: {}",
                              static_cast<int>(table_params.m_table_type));
                return false;
            }
            ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
            ROCPROFVIS_ASSERT(table_handle);

            // setup arguments for event table request
            rocprofvis_controller_arguments_t* args =
                rocprofvis_controller_arguments_alloc();
            ROCPROFVIS_ASSERT(args != nullptr);

            uint32_t num_table_tracks = 0;
            if(SetupCommonTableArguments(args, table_params))
            {
                for(const auto& track_id : table_params.m_track_ids)
                {
                    const track_info_t* metadata = GetTrackInfo(track_id);
                    // skip track if id is invalid
                    if(!metadata)
                    {
                        spdlog::warn("Cannot fetch table, track id {} is invalid",
                                     track_id);
                        continue;
                    }

                    // check if track is an event track
                    if(table_params.m_table_type == kRPVControllerTableTypeEvents &&
                       metadata->track_type != kRPVControllerTrackTypeEvents)
                    {
                        spdlog::warn(
                            "Cannot fetch event table, track id {} is not a event track",
                            track_id);
                        continue;
                    }

                    // check if track is a sample track
                    if(table_params.m_table_type == kRPVControllerTableTypeSamples &&
                       metadata->track_type != kRPVControllerTrackTypeSamples)
                    {
                        spdlog::warn("Cannot fetch sample table, track id {} is not an "
                                     "sample track",
                                     track_id);
                        continue;
                    }

                    // get the track handle
                    rocprofvis_handle_t* track_handle = nullptr;
                    result = rocprofvis_controller_get_object(metadata->graph_handle,
                                                              kRPVControllerGraphTrack, 0,
                                                              &track_handle);
                    ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
                    ROCPROFVIS_ASSERT(track_handle != nullptr);

                    result = rocprofvis_controller_set_object(
                        args, kRPVControllerTableArgsTracksIndexed, num_table_tracks,
                        track_handle);
                    ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);

                    filtered_track_ids.push_back(track_id);
                    num_table_tracks++;
                    spdlog::debug("Adding track id {} to table request", track_id);
                }
            }
            else
            {
                spdlog::error("Failed to setup table common arguments");
                // free the args
                rocprofvis_controller_arguments_free(args);
                return false;
            }

            if(num_table_tracks == 0)
            {
                spdlog::debug("No valid tracks found for table request, aborting");
                // free the args
                rocprofvis_controller_arguments_free(args);
                return false;
            }

            // set the number of tracks in the request
            result = rocprofvis_controller_set_uint64(
                args, kRPVControllerTableArgsNumTracks, 0, num_table_tracks);
            ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);

            // prepare to fetch the table
            rocprofvis_controller_array_t* array = rocprofvis_controller_array_alloc(0);
            ROCPROFVIS_ASSERT(array != nullptr);

            rocprofvis_controller_future_t* future = rocprofvis_controller_future_alloc();
            ROCPROFVIS_ASSERT(future != nullptr);

            result = rocprofvis_controller_table_fetch_async(
                m_trace_controller, table_handle, args, future, array);
            ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);

            // create the request info
            data_req_info_t request_info;
            request_info.request_array      = array;
            request_info.request_future     = future;
            request_info.request_obj_handle = nullptr;
            request_info.request_args       = args;
            request_info.request_id         = request_id;
            request_info.loading_state      = ProviderState::kLoading;
            request_info.request_type =
                (table_params.m_table_type == kRPVControllerTableTypeEvents)
                    ? RequestType::kFetchTrackEventTable
                    : RequestType::kFetchTrackSampleTable;

            auto params         = std::make_shared<TableRequestParams>(table_params);
            params->m_track_ids = std::move(filtered_track_ids);
            request_info.custom_params = params;

            m_requests.emplace(request_id, request_info);
            spdlog::debug("Fetching {} table data",
                          (table_params.m_table_type == kRPVControllerTableTypeEvents)
                              ? "event"
                              : "sample");
            return true;
        }
        else
        {
            // request for item already exists
            spdlog::debug("Request for this table, type {}, is already pending",
                          static_cast<uint64_t>(table_params.m_table_type));
            return false;
        }
    }
    else
    {
        spdlog::debug("Cannot fetch table, no track id provided");
        return false;
    }
}

bool
DataProvider::IsRequestPending(uint64_t request_id) const
{
    auto it = m_requests.find(request_id);
    if(it != m_requests.end())
    {
        return true;
    }
    return false;
}

const RawTrackData*
DataProvider::GetRawTrackData(uint64_t track_id)
{
    if(m_raw_trackdata.count(track_id) > 0)
    {
        return m_raw_trackdata[track_id];
    }
    return nullptr;
}

const track_info_t*
DataProvider::GetTrackInfo(uint64_t track_id)
{
    if(m_track_metadata.count(track_id) > 0)
    {
        return &m_track_metadata[track_id];
    }
    return nullptr;
}

std::vector<const track_info_t*>
RocProfVis::View::DataProvider::GetTrackInfoList()
{
    std::vector<const track_info_t*> list(m_track_metadata.size(), nullptr);
    for(auto& it : m_track_metadata)
    {
        list[it.second.index] = &it.second;
    }
    return list;
}

bool
DataProvider::FreeTrack(uint64_t track_id, bool force /* = false */)
{
    if(m_raw_trackdata.count(track_id) > 0)
    {
        if(m_raw_trackdata[track_id])
        {
            if(!m_raw_trackdata[track_id]->AllDataReady() && !force) {
                spdlog::debug("Cannot delete track data, not all data is ready for id: {}",
                              track_id);
                return false;
            }

            delete m_raw_trackdata[track_id];
            m_raw_trackdata[track_id] = nullptr;
            spdlog::debug("Deleted track id: {}", track_id);
            return true;
        }
        else
        {
            spdlog::debug("No track data to delete for id: {}", track_id);
        }
    }
    else
    {
        spdlog::debug("Cannot delete track data, invalid id: {}", track_id);
    }
    return false;
}

bool 
DataProvider::CancelRequest(uint64_t request_id) 
{
    auto it = m_requests.find(request_id);
    if(it != m_requests.end())  
    {
        spdlog::debug("Cancelling request id: {}", request_id);
        data_req_info_t& request_info = it->second;
        rocprofvis_result_t result = rocprofvis_controller_future_cancel(request_info.request_future);
        if(result == kRocProfVisResultSuccess)
        {
            return true;
        }
        else
        {
            spdlog::debug("Failed to cancel request id: {}, result: {}", request_id, static_cast<int>(result));
        }
    }
    else
    {
        spdlog::debug("Cannot cancel request, invalid id: {}", request_id);
    }
    return false;
}

bool
DataProvider::FreeEvent(uint64_t event_id)
{
    if(m_event_data.count(event_id) > 0)
    {
        m_event_data.erase(event_id);
        spdlog::debug("Deleted event id: {}", event_id);
        return true;
    }
    else
    {
        spdlog::debug("Cannot delete event, invalid id: {}", event_id);
    }
    return false;
}

bool 
DataProvider::FreeAllEvents()
{
    if(!m_event_data.empty())
    {
        m_event_data.clear();
        spdlog::debug("Deleted all events");
        return true;
    }
    else
    {
        spdlog::debug("No events to delete");
    }
    return false;
}

void
DataProvider::DumpMetaData()
{
    for(const track_info_t* track_info : GetTrackInfoList())
    {
        spdlog::debug("Track index {}, id {}, name {}, min ts {}, max ts {}, type {}, "
                      "num entries {}, min value {}, max value {}",
                      track_info->index, track_info->id, track_info->name,
                      track_info->min_ts, track_info->max_ts,
                      track_info->track_type == kRPVControllerTrackTypeSamples ? "Samples"
                                                                               : "Events",
                      track_info->num_entries, track_info->min_value,
                      track_info->max_value);
    }
}

bool
DataProvider::DumpTrack(uint64_t track_id)
{
    if(m_raw_trackdata.count(track_id) > 0)
    {
        if(m_raw_trackdata[track_id])
        {
            bool result = false;
            switch(m_raw_trackdata[track_id]->GetType())
            {
                case kRPVControllerTrackTypeSamples:
                {
                    RawTrackSampleData* track =
                        dynamic_cast<RawTrackSampleData*>(m_raw_trackdata[track_id]);
                    if(track)
                    {
                        const std::vector<rocprofvis_trace_counter_t>& buffer =
                            track->GetData();
                        int64_t i = 0;
                        for(const auto item : buffer)
                        {
                            spdlog::debug("{}, start_ts {}, value {}", i, item.m_start_ts,
                                          item.m_value);
                            ++i;
                        }
                        result = true;
                    }
                    else
                    {
                        spdlog::debug("Cannot dump track data, Type Error");
                    }
                    break;
                }
                case kRPVControllerTrackTypeEvents:
                {
                    RawTrackEventData* track =
                        dynamic_cast<RawTrackEventData*>(m_raw_trackdata[track_id]);
                    if(track)
                    {
                        const std::vector<rocprofvis_trace_event_t>& buffer =
                            track->GetData();
                        int64_t i = 0;
                        for(const auto item : buffer)
                        {
                            spdlog::debug(
                                "{}, name: {}, start_ts {}, duration {}, end_ts {}", i,
                                item.m_name, item.m_start_ts, item.m_duration,
                                item.m_duration + item.m_start_ts);
                            ++i;
                        }
                        result = true;
                    }
                    else
                    {
                        spdlog::debug("Cannot dump track data, Type Error");
                    }
                    break;
                }
                default:
                {
                    spdlog::debug("Cannot dump track data, unknown type");
                    break;
                }
            }
            return result;
        }
        else
        {
            spdlog::debug("No track data with id: {}", track_id);
        }
    }
    else
    {
        spdlog::debug("Cannot show track data, invalid id: {}", track_id);
    }

    return false;
}

void
DataProvider::DumpTable(TableType type)
{
    DumpTable(m_table_infos[static_cast<size_t>(type)].table_header,
              m_table_infos[static_cast<size_t>(type)].table_data);
}

void
DataProvider::DumpTable(const std::vector<std::string>&              header,
                        const std::vector<std::vector<std::string>>& data)
{
    std::ostringstream str_collector;
    for(const auto& col_header : header)
    {
        str_collector << col_header;
        str_collector << " ";
    }
    spdlog::debug("{}", str_collector.str());
    str_collector.str("");
    str_collector.clear();

    // print the event table data
    int skip_count = 0;
    for(const auto& row : data)
    {
        int col_count = 0;
        for(const auto& column : row)
        {
            str_collector << column;
            str_collector << " ";
            col_count++;
        }
        if(col_count == 0)
        {
            skip_count++;
            continue;
        }
        if(skip_count > 0)
        {
            spdlog::debug("Skipped {} empty rows", skip_count);
            skip_count = 0;
        }

        spdlog::debug("{}", str_collector.str());
        str_collector.str("");
        str_collector.clear();
    }
}

void
DataProvider::HandleRequests()
{
    if(m_requests.size() > 0)
    {
        for(auto it = m_requests.begin(); it != m_requests.end();)
        {
            data_req_info_t& req = it->second;
            rocprofvis_result_t result =
                rocprofvis_controller_future_wait(req.request_future, 0);
            ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess ||
                                result == kRocProfVisResultTimeout);

            // the response is ready
            if(result == kRocProfVisResultSuccess)
            {
                req.loading_state  = ProviderState::kReady;
                req.response_code = kRocProfVisResultSuccess;
                uint64_t future_result = 0;
                result = rocprofvis_controller_get_uint64(req.request_future, kRPVControllerFutureResult,
                                    0, &req.response_code);
                ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);

                rocprofvis_controller_future_free(req.request_future);
                req.request_future = nullptr;
                    ProcessRequest(req);
                    // remove request from processing container
                    it = m_requests.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }
}
void
DataProvider::ProcessEventCallStackRequest(data_req_info_t& req)
{
    if(!req.request_array)
    {
        spdlog::error("Call stack request array is null");
        return;
    }

    uint64_t            prop_count = 0;
    rocprofvis_result_t result     = rocprofvis_controller_get_uint64(
        req.request_array, kRPVControllerArrayNumEntries, 0, &prop_count);
    if(result != kRocProfVisResultSuccess)
    {
        spdlog::error("Failed to get call stack array entry count, result: {}",
                      static_cast<int>(result));
        rocprofvis_controller_array_free(req.request_array);
        return;
    }

    spdlog::debug("Event {} has {} call stack properties", req.request_id, prop_count);

    std::shared_ptr<EventRequestParams> event_params =
        std::dynamic_pointer_cast<EventRequestParams>(req.custom_params);
    if(event_params && m_event_data.count(event_params->m_event_id) > 0)
    {
        event_info_t& event_info = m_event_data[event_params->m_event_id];
        event_info.call_stack_info.clear();
        event_info.call_stack_info.resize(prop_count);

        for(uint64_t i = 0; i < prop_count; i++)
        {
            rocprofvis_handle_t* callstack_handle = nullptr;
            result                                = rocprofvis_controller_get_object(
                req.request_array, kRPVControllerArrayEntryIndexed, i, &callstack_handle);
            if(result != kRocProfVisResultSuccess || !callstack_handle)
            {
                spdlog::warn("Failed to get call stack handle for entry {}", i);
                continue;
            }

            event_info.call_stack_info[i].function =
                GetString(callstack_handle, kRPVControllerCallstackFunction, i);
            event_info.call_stack_info[i].arguments =
                GetString(callstack_handle, kRPVControllerCallstackArguments, i);
            event_info.call_stack_info[i].line =
                GetString(callstack_handle, kRPVControllerCallstackLine, i);

            spdlog::debug("Call stack entry {}: function: {}, line: {}, arguments: {}", i,
                          event_info.call_stack_info[i].function,
                          event_info.call_stack_info[i].line,
                          event_info.call_stack_info[i].arguments);
        }
    }

    rocprofvis_controller_array_free(req.request_array);
    req.request_array = nullptr;
}

void
DataProvider::ProcessEventExtendedRequest(data_req_info_t& req)
{
    if(!req.request_array)
    {
        spdlog::error("Event extended data request array is null");
        return;
    }

    uint64_t            prop_count = 0;
    rocprofvis_result_t result     = rocprofvis_controller_get_uint64(
        req.request_array, kRPVControllerArrayNumEntries, 0, &prop_count);
    if(result != kRocProfVisResultSuccess)
    {
        spdlog::error("Failed to get extended data array entry count, result: {}",
                      static_cast<int>(result));
        rocprofvis_controller_array_free(req.request_array);
        return;
    }

    std::shared_ptr<EventRequestParams> event_params =
        std::dynamic_pointer_cast<EventRequestParams>(req.custom_params);
    if(event_params && m_event_data.count(event_params->m_event_id) > 0)
    {
        event_info_t& event_info = m_event_data[event_params->m_event_id];
        event_info.ext_info.clear();
        event_info.ext_info.resize(prop_count);

        for(uint64_t j = 0; j < prop_count; j++)
        {
            rocprofvis_handle_t* ext_data_handle = nullptr;
            result                               = rocprofvis_controller_get_object(
                req.request_array, kRPVControllerArrayEntryIndexed, j, &ext_data_handle);
            if(result != kRocProfVisResultSuccess || !ext_data_handle)
            {
                spdlog::warn("Failed to get extended data handle for entry {}", j);
                continue;
            }

            event_info.ext_info[j].category =
                GetString(ext_data_handle, kRPVControllerExtDataCategory, 0);
            event_info.ext_info[j].name =
                GetString(ext_data_handle, kRPVControllerExtDataName, 0);
            event_info.ext_info[j].value =
                GetString(ext_data_handle, kRPVControllerExtDataValue, 0);

//this block is for testing purposes. Use kRPVControllerExtDataType and kRPVControllerExtDataCategoryEnum as needed
            {
                //uint64_t data_type; //rocprofvis_controller_primitive_type_t
                //uint64_t data_enum; //rocprofvis_event_data_category_enum_t
                //if(kRocProfVisResultSuccess ==
                //       rocprofvis_controller_get_uint64(ext_data_handle,
                //                                        kRPVControllerExtDataType, 0,
                //                                        (uint64_t*) &data_type) &&
                //   kRocProfVisResultSuccess ==
                //       rocprofvis_controller_get_uint64(ext_data_handle,
                //                                        kRPVControllerExtDataCategoryEnum,
                //                                        0, (uint64_t*) &data_enum))
                //{
                //    event_info.ext_info[j].value += " (" + std::to_string(data_type) +
                //                                    "," + std::to_string(data_enum) + ")";
                //}
            }
//end of test block
        }
    }

    rocprofvis_controller_array_free(req.request_array);
    req.request_array = nullptr;
}
void
DataProvider::ProcessEventFlowDetailsRequest(data_req_info_t& req)
{
    if(!req.request_array)
    {
        spdlog::error("Event flow details request array is null");
        return;
    }

    uint64_t            prop_count = 0;
    rocprofvis_result_t result     = rocprofvis_controller_get_uint64(
        req.request_array, kRPVControllerArrayNumEntries, 0, &prop_count);
    if(result != kRocProfVisResultSuccess)
    {
        spdlog::error("Failed to get flow details array entry count, result: {}",
                      static_cast<int>(result));
        rocprofvis_controller_array_free(req.request_array);
        return;
    }

    std::shared_ptr<EventRequestParams> event_params =
        std::dynamic_pointer_cast<EventRequestParams>(req.custom_params);
    if(event_params && m_event_data.count(event_params->m_event_id) > 0)
    {
        event_info_t& event_info = m_event_data[event_params->m_event_id];
        event_info.flow_info.clear();
        event_info.flow_info.resize(prop_count);

        for(uint64_t j = 0; j < prop_count; j++)
        {
            rocprofvis_handle_t* flow_control_handle = nullptr;
            result = rocprofvis_controller_get_object(req.request_array,
                                                      kRPVControllerArrayEntryIndexed, j,
                                                      &flow_control_handle);
            if(result != kRocProfVisResultSuccess || !flow_control_handle)
            {
                spdlog::warn("Failed to get flow control handle for entry {}", j);
                continue;
            }

            uint64_t data = 0;
            result        = rocprofvis_controller_get_uint64(
                flow_control_handle, kRPVControllerFlowControltId, 0, &data);
            if(result == kRocProfVisResultSuccess)
            {
                event_info.flow_info[j].id  = data;
            }

            data   = 0;
            result = rocprofvis_controller_get_uint64(
                flow_control_handle, kRPVControllerFlowControlTimestamp, 0, &data);
            if(result == kRocProfVisResultSuccess)
            {
                event_info.flow_info[j].timestamp  = data;
            }

            data   = 0;
            result = rocprofvis_controller_get_uint64(
                flow_control_handle, kRPVControllerFlowControlTrackId, 0, &data);
            if(result == kRocProfVisResultSuccess)
            {
                event_info.flow_info[j].track_id  = data;
            }

            data   = 0;
            result = rocprofvis_controller_get_uint64(
                flow_control_handle, kRPVControllerFlowControlDirection, 0, &data);
            if(result == kRocProfVisResultSuccess)
            {
                event_info.flow_info[j].direction  = data;
            }

            data   = 0;
            result = rocprofvis_controller_get_uint64(
                flow_control_handle, kRPVControllerFlowControlLevel, 0, &data);
            if(result == kRocProfVisResultSuccess)
            {
                event_info.flow_info[j].level  = data;
            }

            event_info.flow_info[j].name = GetString(flow_control_handle, 
                                                kRPVControllerFlowControlName, 0);
        }
    }

    rocprofvis_controller_array_free(req.request_array);
    req.request_array = nullptr;
}

void
DataProvider::ProcessRequest(data_req_info_t& req)
{
    switch(req.request_type)
    {
        case RequestType::kFetchGraph:
        {
            ProcessGraphRequest(req);
            break;
        }
        case RequestType::kFetchTrack:
        {
            ProcessTrackRequest(req);
            break;
        }
        case RequestType::kFetchEventExtendedData:
        {
            ProcessEventExtendedRequest(req);
            break;
        }
        case RequestType::kFetchEventCallStack:
        {
            ProcessEventCallStackRequest(req);
            break;
        }
        case RequestType::kFetchEventFlowDetails:
        {
            ProcessEventFlowDetailsRequest(req);
            break;
        }
        case RequestType::kFetchTrackEventTable:
        case RequestType::kFetchTrackSampleTable:
        {
            spdlog::debug("Processing table data {}", req.request_id);
            ProcessTableRequest(req);
            break;
        }
        case RequestType::kSaveTrimmedTrace:
        {
            ProcessSaveTrimmedTraceRequest(req);
            break;
        }        
        default:
        {
            spdlog::debug("Unknown request type {}", static_cast<int>(req.request_type));
            return;
        }
    }
}

void 
DataProvider::ProcessSaveTrimmedTraceRequest(data_req_info_t& req)
{
    spdlog::debug("Save trimmed trace request complete with result: {}", req.response_code);

    // call the trim complete callback
    if(m_save_trace_callback)
    {
        m_save_trace_callback(req.response_code == kRocProfVisResultSuccess);
    }
}

void
DataProvider::ProcessTableRequest(data_req_info_t& req)
{
    // free arguments
    if(req.request_args)
    {
        rocprofvis_controller_arguments_free(req.request_args);
        req.request_args = nullptr;
    }

    if(!req.request_array)
    {
        spdlog::debug("Cannot process table data, request array is null");
        return;
    }

    if(req.response_code == kRocProfVisResultSuccess)
    {   
        rocprofvis_controller_table_type_t table_type =
            (req.request_type == RequestType::kFetchTrackEventTable)
                ? kRPVControllerTableTypeEvents
                : kRPVControllerTableTypeSamples;

        rocprofvis_handle_t* table_handle = nullptr;
        rocprofvis_result_t  result       = kRocProfVisResultUnknownError;

        if(table_type == kRPVControllerTableTypeEvents)
        {
            result = rocprofvis_controller_get_object(
                m_trace_controller, kRPVControllerEventTable, 0, &table_handle);
        }
        else if(table_type == kRPVControllerTableTypeSamples)
        {
            result = rocprofvis_controller_get_object(
                m_trace_controller, kRPVControllerSampleTable, 0, &table_handle);
        }
        else
        {
            spdlog::error("Unsupported table type: {}", static_cast<int>(table_type));
            return;
        }

        ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
        ROCPROFVIS_ASSERT(table_handle);

        uint64_t num_columns    = 0;
        uint64_t total_num_rows = 0;

        // get the number of columns and rows in the table
        result = rocprofvis_controller_get_uint64(table_handle, kRPVControllerTableNumColumns,
                                                  0, &num_columns);
        ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);

        result = rocprofvis_controller_get_uint64(table_handle, kRPVControllerTableNumRows, 0,
                                                  &total_num_rows);
        ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);

        // get the column names
        std::vector<std::string> column_names;
        column_names.reserve(num_columns);
        for(int i = 0; i < num_columns; i++)
        {
            column_names.emplace_back(
                GetString(table_handle, kRPVControllerTableColumnHeaderIndexed, i));
        }

        uint64_t num_rows = 0;
        ROCPROFVIS_ASSERT(req.request_array);

        result = rocprofvis_controller_get_uint64(
            req.request_array, kRPVControllerArrayNumEntries, 0, &num_rows);
        spdlog::debug("Table request returned {0} rows", num_rows);
        ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);

        // get row data
        std::vector<std::vector<std::string>> table_data;
        table_data.reserve(num_rows);
        for(uint32_t i = 0; i < num_rows; i++)
        {
            // get the row data, for now all data will be stored as strings
            std::vector<std::string> row_data;
            row_data.reserve(num_columns);

            rocprofvis_handle_t* row_array = nullptr;
            result                         = rocprofvis_controller_get_object(
                req.request_array, kRPVControllerArrayEntryIndexed, i, &row_array);
            ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
            ROCPROFVIS_ASSERT(row_array);
            for(uint32_t j = 0; j < num_columns; j++)
            {
                uint64_t column_type = 0;
                result               = rocprofvis_controller_get_uint64(
                    table_handle, kRPVControllerTableColumnTypeIndexed, j, &column_type);
                ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
                std::string column_value = "";
                switch(column_type)
                {
                    case kRPVControllerPrimitiveTypeUInt64:
                    {
                        uint64_t value = 0;
                        result         = rocprofvis_controller_get_uint64(
                            row_array, kRPVControllerArrayEntryIndexed, j, &value);
                        ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);

                        column_value = std::to_string(value);
                        break;
                    }
                    case kRPVControllerPrimitiveTypeDouble:
                    {
                        double value = 0;
                        result       = rocprofvis_controller_get_double(
                            row_array, kRPVControllerArrayEntryIndexed, j, &value);
                        ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);

                        column_value = std::to_string(value);
                        break;
                    }
                    case kRPVControllerPrimitiveTypeString:
                    {
                        column_value =
                            GetString(row_array, kRPVControllerArrayEntryIndexed, j);
                        break;
                    }
                    case kRPVControllerPrimitiveTypeObject:
                    default:
                    {
                        // skip columns with object types for now
                        spdlog::debug("Skipping column {} with object type", j);
                        break;
                    }
                }
                row_data.push_back(std::move(column_value));
            }
            table_data.push_back(std::move(row_data));
        }

        
        std::shared_ptr<TableRequestParams> table_params =
            std::dynamic_pointer_cast<TableRequestParams>(req.custom_params);
        if(!table_params)
        {
            spdlog::warn("Table request params are not set or invalid");
            table_params = nullptr;
        }

        if(table_type == kRPVControllerTableTypeEvents ||
           table_type == kRPVControllerTableTypeSamples)
        {
            table_info_t& table_info =
                table_type == kRPVControllerTableTypeEvents
                    ? m_table_infos[static_cast<size_t>(TableType::kEventTable)]
                    : m_table_infos[static_cast<size_t>(TableType::kSampleTable)];
            // store the event table data
            table_info.table_header    = std::move(column_names);
            table_info.table_data      = std::move(table_data);
            table_info.table_params    = table_params;
            table_info.total_row_count = total_num_rows;
            
            // create a place holder for the formatted data, higher level consumers
            // will format the data as needed
            table_info.formatted_column_data.clear();
            table_info.formatted_column_data.resize(table_info.table_header.size());
            for(auto& col : table_info.formatted_column_data)
            {
                col.needs_formatting = false;
                col.formatted_row_value = std::vector<std::string>();
            }
        }
        else
        {
            spdlog::error("Unsupported table type: {}", static_cast<int>(table_type));
            return;
        }
    }
    else
    {
        spdlog::debug("Table request failed with code {}", req.response_code);
    }

    // free the array
    if(req.request_array)
    {
        rocprofvis_controller_array_free(req.request_array);
        req.request_array = nullptr;
    }

    if(m_table_data_ready_callback)
    {
        m_table_data_ready_callback(m_trace_file_path, req.request_id);
    }
}

void
DataProvider::ProcessTrackRequest(data_req_info_t& req)
{
    spdlog::debug("Processing track data {}", req.request_id);

    auto track_params = std::dynamic_pointer_cast<TrackRequestParams>(req.custom_params);
    if(!track_params)
    {
        spdlog::error("Track request params are not set or invalid");
        return;
    }

    // use the track type to determine what type of data is present in the graph array
    const track_info_t* metadata = GetTrackInfo(track_params->m_track_id);
    ROCPROFVIS_ASSERT(metadata);
    switch(metadata->track_type)
    {
        case kRPVControllerTrackTypeEvents:
        {
            CreateRawEventData(*track_params, req);
            break;
        }
        case kRPVControllerTrackTypeSamples:
        {
            CreateRawSampleData(*track_params, req);
            break;
        }
        default:
        {
            spdlog::debug("Unknown track type for track id {}, skipping", track_params->m_track_id);
            break;
        }
    }

    // free the array
    if(req.request_array)
    {
        rocprofvis_controller_array_free(req.request_array);
        req.request_array = nullptr;
    }

    // call the new data ready callback
    if(m_track_data_ready_callback)
    {
        m_track_data_ready_callback(track_params->m_track_id, m_trace_file_path, req);
    }
}

void
DataProvider::ProcessGraphRequest(data_req_info_t& req)
{
    spdlog::debug("Processing graph data {}", req.request_id);

    auto track_params = std::dynamic_pointer_cast<TrackRequestParams>(req.custom_params);
    if(!track_params)
    {
        spdlog::error("Track request params are not set or invalid");
        return;
    }

    if(req.response_code == kRocProfVisResultSuccess) {
        uint64_t graph_type = 0;
        auto     graph      = req.request_obj_handle;

        rocprofvis_result_t result =
            rocprofvis_controller_get_uint64(graph, kRPVControllerGraphType, 0, &graph_type);
        ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);

        double min_ts = 0;
        result = rocprofvis_controller_get_double(graph, kRPVControllerGraphStartTimestamp, 0,
                                                &min_ts);
        ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);

        double max_ts = 0;
        result = rocprofvis_controller_get_double(graph, kRPVControllerGraphEndTimestamp, 0,
                                                &max_ts);
        ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);

        uint64_t item_count = 0;
        result = rocprofvis_controller_get_uint64(graph, kRPVControllerGraphNumEntries, 0,
                                                &item_count);
        ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);

        spdlog::debug("{} Graph item count {} min ts {} max ts {}", track_params->m_track_id,
                    item_count, min_ts, max_ts);

    } else {
        spdlog::debug("Graph request failed with code {}", req.response_code);
    }

    // use the track type to determine what type of data is present in the graph array
    const track_info_t* metadata = GetTrackInfo(track_params->m_track_id);
    ROCPROFVIS_ASSERT(metadata);
    
    switch(metadata->track_type)
    {
        case kRPVControllerTrackTypeEvents:
        {
            CreateRawEventData(*track_params, req);
            break;
        }
        case kRPVControllerTrackTypeSamples:
        {
            CreateRawSampleData(*track_params, req);
            break;
        }
        default:
        {
            break;
        }
    }

    // free the array
    if(req.request_array)
    {
        rocprofvis_controller_array_free(req.request_array);
        req.request_array = nullptr;
    }

    // call the new data ready callback
    if(m_track_data_ready_callback)
    {
        m_track_data_ready_callback(track_params->m_track_id, m_trace_file_path, req);
    }
}

void
DataProvider::CreateRawSampleData(const TrackRequestParams& params, const data_req_info_t& req)
{
    rocprofvis_controller_array_t* track_data = req.request_array;
    const std::chrono::steady_clock::time_point& request_time = req.request_time;
    bool response_valid = (req.response_code == kRocProfVisResultSuccess);

    uint64_t            count  = 0;

    if(response_valid)
    {    
    rocprofvis_result_t result = rocprofvis_controller_get_uint64(
        track_data, kRPVControllerArrayNumEntries, 0, &count);
    ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
    } else {
        spdlog::warn("Sample track data request failed with code {}", req.response_code);
        count = 0;
    }

    RawTrackSampleData* raw_sample_data = nullptr;
    const RawTrackData* existing_raw_data = GetRawTrackData(params.m_track_id);
    
    // If group id matches, reuse existing raw data
    if(existing_raw_data && existing_raw_data->GetDataGroupID() == params.m_data_group_id)
    {
        raw_sample_data =
            dynamic_cast<RawTrackSampleData*>(m_raw_trackdata[params.m_track_id]);
    }
    // If group id does not match ...
    else if(existing_raw_data)
    {
        auto existing_timepoint =
            m_raw_trackdata[params.m_track_id]->GetDataRequestTimePoint();

        // check if the existing data is newer than the request time of this response
        if(existing_timepoint > request_time)
        {
            spdlog::debug("Skipping track data with id {} for group {}, existing data is "
                          "part of newer group ({})",
                          params.m_track_id, params.m_data_group_id,
                          existing_raw_data->GetDataGroupID());
            return;
        }
        delete m_raw_trackdata[params.m_track_id];
        m_raw_trackdata[params.m_track_id] = nullptr;
        spdlog::debug("Replacing existing track data with id {}", params.m_track_id);
    }

    if(!raw_sample_data)
    {
        spdlog::debug("Create sample track {} data with {} entries", params.m_track_id,
                      count);
        raw_sample_data = new RawTrackSampleData(params.m_track_id, params.m_start_ts,
                                                 params.m_end_ts, params.m_data_group_id, params.m_chunk_count);
    }

    std::vector<rocprofvis_trace_counter_t> buffer;
    buffer.reserve(count);

    std::unordered_set timepoint_set = raw_sample_data->GetWritableIdSet();

    for(uint64_t i = 0; i < count; i++)
    {
        rocprofvis_controller_sample_t* sample = nullptr;
        rocprofvis_result_t result             = rocprofvis_controller_get_object(
            track_data, kRPVControllerArrayEntryIndexed, i, &sample);
        ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess && sample);

        double start_ts = 0;
        result = rocprofvis_controller_get_double(sample, kRPVControllerSampleTimestamp,
                                                  0, &start_ts);
        ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);

        auto [_, inserted] = timepoint_set.insert(start_ts);
        if(!inserted)
        {
            spdlog::debug("Skipping duplicate sample timepoint {} on track {}", start_ts,
                          params.m_track_id);
            continue;  // skip duplicate samples
        }

        // Construct rocprofvis_trace_counter_t item in-place
        buffer.emplace_back();
        rocprofvis_trace_counter_t& trace_counter = buffer.back();

        double value = 0;
        result = rocprofvis_controller_get_double(sample, kRPVControllerSampleValue, 0,
                                                  &value);
        ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);

        trace_counter.m_start_ts = start_ts;
        trace_counter.m_value    = value;
    }

    raw_sample_data->AddChunk(params.m_chunk_index, std::move(buffer));

    m_raw_trackdata[params.m_track_id] = raw_sample_data;
}

void
DataProvider::CreateRawEventData(const TrackRequestParams& params,
                                 const data_req_info_t& req)
{
    rocprofvis_controller_array_t* track_data = req.request_array;
    const std::chrono::steady_clock::time_point& request_time = req.request_time;
    bool response_valid = (req.response_code == kRocProfVisResultSuccess);

    uint64_t            count  = 0;

    if(response_valid)
    {
        rocprofvis_result_t result = rocprofvis_controller_get_uint64(
            track_data, kRPVControllerArrayNumEntries, 0, &count);
        ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
    } else {
        spdlog::warn("Event track data request failed with code {}", req.response_code);
        count = 0;
    }

    RawTrackEventData* raw_event_data = nullptr;
    const RawTrackData* existing_raw_data = GetRawTrackData(params.m_track_id);

    // If group id matches, reuse existing raw data
    if(existing_raw_data && existing_raw_data->GetDataGroupID() == params.m_data_group_id)
    {
        raw_event_data =
            dynamic_cast<RawTrackEventData*>(m_raw_trackdata[params.m_track_id]);
    }
    // If group id does not match ...
    else if(existing_raw_data)
    {
        auto existing_timepoint =
            m_raw_trackdata[params.m_track_id]->GetDataRequestTimePoint();

        // check if the existing data is newer than the request time of this response
        if(existing_timepoint > request_time)
        {
            spdlog::debug("Skipping track data with id {} for group {}, existing data is "
                          "part of newer group ({})",
                          params.m_track_id, params.m_data_group_id,
                          existing_raw_data->GetDataGroupID());
            return;
        }

        spdlog::debug(
            "Replacing existing track data from group {} with group {} for id {}",
            existing_raw_data->GetDataGroupID(), params.m_data_group_id,
            params.m_track_id);
        delete m_raw_trackdata[params.m_track_id];
        m_raw_trackdata[params.m_track_id] = nullptr;
    }

    if(!raw_event_data)
    {
        spdlog::debug("Creating event track {} data with {} entries", params.m_track_id,
                      count);
        raw_event_data = new RawTrackEventData(params.m_track_id, params.m_start_ts,
                                               params.m_end_ts, params.m_data_group_id, params.m_chunk_count);
    }

    std::vector<rocprofvis_trace_event_t> buffer;
    buffer.reserve(count);

    size_t str_buffer_length = 128;
    char*  str_buffer        = new char[str_buffer_length];

    std::unordered_set event_set = raw_event_data->GetWritableIdSet();

    size_t real_count = 0;
    for(uint64_t i = 0; i < count; i++)
    {
        rocprofvis_controller_event_t* event = nullptr;
        rocprofvis_result_t result           = rocprofvis_controller_get_object(
            track_data, kRPVControllerArrayEntryIndexed, i, &event);
        ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess && event);

        uint64_t id = 0;
        result = rocprofvis_controller_get_uint64(event, kRPVControllerEventId, 0, &id);
        ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);

        auto [_, inserted] = event_set.insert(id);
        if(!inserted)
        {
            spdlog::debug("Skipping duplicate event id {} on track {}", id,
                          params.m_track_id);
            continue;  // skip duplicate events
        }

        // Construct rocprofvis_trace_event_t item in-place
        buffer.emplace_back();
        rocprofvis_trace_event_t& trace_event = buffer.back();
        trace_event.m_id                      = id;

        double start_ts = 0;
        result          = rocprofvis_controller_get_double(
            event, kRPVControllerEventStartTimestamp, 0, &start_ts);
        ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
        trace_event.m_start_ts = start_ts;

        double end_ts = 0;
        result = rocprofvis_controller_get_double(event, kRPVControllerEventEndTimestamp,
                                                  0, &end_ts);
        ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
        trace_event.m_duration = end_ts - start_ts;

        uint64_t level = 0;
        result =
            rocprofvis_controller_get_uint64(event, kRPVControllerEventLevel, 0, &level);
        ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
        trace_event.m_level = static_cast<uint32_t>(level);

        // get event name
        uint32_t length = 0;
        result = rocprofvis_controller_get_string(event, kRPVControllerEventName, 0,
                                                  nullptr, &length);
        ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);

        if(length >= str_buffer_length)
        {
            delete[] str_buffer;
            str_buffer_length = length + 1;
            str_buffer        = new char[str_buffer_length];
        }
        length += 1;
        result = rocprofvis_controller_get_string(event, kRPVControllerEventName, 0,
                                                  str_buffer, &length);
        ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
        trace_event.m_name.assign(str_buffer);
        real_count++;
    }

    // Add the buffer to the raw event data
    spdlog::debug("Adding {} event entries to track id {}", real_count, params.m_track_id);
    raw_event_data->AddChunk(params.m_chunk_index, std::move(buffer));
    delete[] str_buffer;

    m_raw_trackdata[params.m_track_id] = raw_event_data;
}

bool
DataProvider::FetchEvent(uint64_t track_id, uint64_t event_id)
{
    m_event_data[event_id].track_id   = track_id;
    const RawTrackEventData* event_track =
        dynamic_cast<const RawTrackEventData*>(GetRawTrackData(track_id));
    if(event_track)
    {
        for(const rocprofvis_trace_event_t& event : event_track->GetData())
        {
            if(event.m_id == event_id)
            {
                m_event_data[event_id].basic_info = event;
                break;
            }
        }
    }
    return FetchEventExtData(event_id) && FetchEventFlowDetails(event_id) &&
                       FetchEventCallStackData(event_id);;
}

bool
DataProvider::FetchEventExtData(uint64_t event_id)
{
    if(m_state != ProviderState::kReady)
    {
        spdlog::debug("Cannot fetch, provider not ready or error, state: {}",
                      static_cast<int>(m_state));
        return false;
    }

    auto future   = rocprofvis_controller_future_alloc();
    auto outArray = rocprofvis_controller_array_alloc(0);
    ROCPROFVIS_ASSERT(future != nullptr);
    ROCPROFVIS_ASSERT(outArray != nullptr);

    rocprofvis_result_t result = rocprofvis_controller_get_indexed_property_async(
        m_trace_controller, m_trace_controller, kRPVControllerEventDataExtDataIndexed,
        event_id, 1, future, outArray);

    if(result == kRocProfVisResultSuccess)
    {
        data_req_info_t request_info;
        request_info.request_array      = outArray;
        request_info.request_future     = future;
        request_info.request_obj_handle = nullptr;
        request_info.request_args       = nullptr;
        request_info.request_id         = EVENT_EXTENDED_DATA_REQUEST_ID;
        request_info.loading_state      = ProviderState::kLoading;
        request_info.request_type       = RequestType::kFetchEventExtendedData;
        request_info.custom_params      = std::make_shared<EventRequestParams>(event_id);

        m_requests.emplace(request_info.request_id, request_info);
        return true;
    }
    else
    {
        spdlog::error("Failed to fetch event details for event id {}, result: {}",
                      event_id, static_cast<int>(result));
        rocprofvis_controller_future_free(future);
        rocprofvis_controller_array_free(outArray);
        return false;
    }
}

bool
DataProvider::FetchEventFlowDetails(uint64_t event_id)
{
    if(m_state != ProviderState::kReady)
    {
        spdlog::debug("Cannot fetch, provider not ready or error, state: {}",
                      static_cast<int>(m_state));
        return false;
    }

    auto future   = rocprofvis_controller_future_alloc();
    auto outArray = rocprofvis_controller_array_alloc(0);
    ROCPROFVIS_ASSERT(future != nullptr);
    ROCPROFVIS_ASSERT(outArray != nullptr);

    rocprofvis_result_t result = rocprofvis_controller_get_indexed_property_async(
        m_trace_controller, m_trace_controller, kRPVControllerEventDataFlowControlIndexed,
        event_id, 1, future, outArray);

    if(result == kRocProfVisResultSuccess)
    {
        data_req_info_t request_info;
        request_info.request_array      = outArray;
        request_info.request_future     = future;
        request_info.request_obj_handle = nullptr;
        request_info.request_args       = nullptr;
        request_info.request_id         = EVENT_FLOW_DATA_REQUEST_ID;
        request_info.loading_state      = ProviderState::kLoading;
        request_info.request_type       = RequestType::kFetchEventFlowDetails;
        request_info.custom_params      = std::make_shared<EventRequestParams>(event_id);

        m_requests.emplace(request_info.request_id, request_info);
        return true;
    }
    else
    {
        spdlog::error("Failed to fetch event flow details for event id {}, result: {}",
                      event_id, static_cast<int>(result));
        rocprofvis_controller_future_free(future);
        rocprofvis_controller_array_free(outArray);
        return false;
    }
}

bool
DataProvider::FetchEventCallStackData(uint64_t event_id)
{
    if(m_state != ProviderState::kReady)
    {
        spdlog::debug("Cannot fetch, provider not ready or error, state: {}",
                      static_cast<int>(m_state));
        return false;
    }

    auto future   = rocprofvis_controller_future_alloc();
    auto outArray = rocprofvis_controller_array_alloc(0);
    ROCPROFVIS_ASSERT(future != nullptr);
    ROCPROFVIS_ASSERT(outArray != nullptr);

    rocprofvis_result_t result = rocprofvis_controller_get_indexed_property_async(
        m_trace_controller, m_trace_controller, kRPVControllerEventDataCallStackIndexed,
        event_id, 1, future, outArray);

    if(result == kRocProfVisResultSuccess)
    {
        data_req_info_t request_info;
        request_info.request_array      = outArray;
        request_info.request_future     = future;
        request_info.request_obj_handle = nullptr;
        request_info.request_args       = nullptr;
        request_info.request_id         = EVENT_CALL_STACK_DATA_REQUEST_ID;
        request_info.loading_state      = ProviderState::kLoading;
        request_info.request_type       = RequestType::kFetchEventCallStack;
        request_info.custom_params      = std::make_shared<EventRequestParams>(event_id);

        m_requests.emplace(request_info.request_id, request_info);
        return true;
    }
    else
    {
        spdlog::error("Failed to fetch event call stack data for event id {}, result: {}",
                      event_id, static_cast<int>(result));
        rocprofvis_controller_future_free(future);
        rocprofvis_controller_array_free(outArray);
        return false;
    }
}

std::string
DataProvider::GetString(rocprofvis_handle_t* handle, rocprofvis_property_t property,
                        uint64_t index)
{
    uint32_t            length = 0;
    rocprofvis_result_t result =
        rocprofvis_controller_get_string(handle, property, index, nullptr, &length);
    ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);

    if(length == 0)
    {
        return std::string();
    }

    std::string str;
    str.resize(length);
    result = rocprofvis_controller_get_string(handle, property, index,
                                              const_cast<char*>(str.c_str()), &length);
    ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);

    return str;
}

}  // namespace View
}  // namespace RocProfVis
