#include "rocprofvis_data_provider.h"
#include "rocprofvis_controller.h"
#include "rocprofvis_core_assert.h"

#include "spdlog/spdlog.h"

#include <iostream>
#include <sstream>

using namespace RocProfVis::View;

DataProvider::DataProvider()
: m_state(ProviderState::kInit)
, m_trace_future(nullptr)
, m_trace_controller(nullptr)
, m_trace_timeline(nullptr)
, m_track_data_ready_callback(nullptr)
, m_trace_data_ready_callback(nullptr)
, m_num_graphs(0)
, m_min_ts(0)
, m_max_ts(0)
, m_trace_file_path("")
, m_table_infos(static_cast<size_t>(TableType::__kTableTypeCount))
, m_selected_event_id(std::numeric_limits<uint64_t>::max())
{}

DataProvider::~DataProvider() { CloseController(); }

void
DataProvider::SetSelectedEventId(uint64_t id)
{
    m_selected_event_id = id;
}

const event_info_t&
DataProvider::GetEventInfoStruct() const
{
    return m_event_info;
}

const flow_info_t&
DataProvider::GetFlowInfo() const
{
    return m_flow_info;
}

const call_stack_info_t&
DataProvider::GetCallStackInfo() const
{
    return m_call_stack_info;
}

uint64_t
DataProvider::GetSelectedEventId()
{
    return m_selected_event_id;
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
DataProvider::FreeRequests()
{
    for(auto item : m_requests)
    {
        data_req_info_t& req = item.second;
        if(req.request_array)
        {
            rocprofvis_controller_array_free(req.request_array);
            req.request_array = nullptr;
        }
        if(req.request_future)
        {
            rocprofvis_controller_future_free(req.request_future);
            req.request_future = nullptr;
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

void
DataProvider::ClearTable(TableType type)
{
    m_table_infos[static_cast<size_t>(type)].table_header.clear();
    m_table_infos[static_cast<size_t>(type)].table_data.clear();
    m_table_infos[static_cast<size_t>(type)].total_row_count = 0;
    m_table_infos[static_cast<size_t>(type)].table_params.reset();
}

void
DataProvider::SetTrackDataReadyCallback(
    const std::function<void(uint64_t, const std::string&)>& callback)
{
    m_track_data_ready_callback = callback;
}

void
DataProvider::SetTraceLoadedCallback(
    const std::function<void(const std::string&)>& callback)
{
    m_trace_data_ready_callback = callback;
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
            ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess &&
                              uint64_result == kRocProfVisResultSuccess);

            result = rocprofvis_controller_get_object(
                m_trace_controller, kRPVControllerTimeline, 0, &m_trace_timeline);

            // store timeline meta data
            if(result == kRocProfVisResultSuccess && m_trace_timeline)
            {
                m_num_graphs = 0;
                result       = rocprofvis_controller_get_uint64(
                    m_trace_timeline, kRPVControllerTimelineNumGraphs, 0, &m_num_graphs);

                m_min_ts = 0;
                result   = rocprofvis_controller_get_double(
                    m_trace_timeline, kRPVControllerTimelineMinTimestamp, 0, &m_min_ts);

                m_max_ts = 0;
                result   = rocprofvis_controller_get_double(
                    m_trace_timeline, kRPVControllerTimelineMaxTimestamp, 0, &m_max_ts);

                spdlog::debug("Timeline parameters: tracks {} min ts {} max ts {}",
                              m_num_graphs, m_min_ts, m_max_ts);

                // get the per track meta data
                HandleLoadTrackMetaData();
            }

            // trace loaded successfully, free the future pointer
            rocprofvis_controller_future_free(m_trace_future);
            m_trace_future = nullptr;

            m_state = ProviderState::kReady;
            // fire callback
            if(m_trace_data_ready_callback)
            {
                m_trace_data_ready_callback(m_trace_file_path);
            }
        }
        else
        {
            // timed out, do nothing and try again later
        }
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
                spdlog::debug("Resizing str buffer to {}", str_buffer_length);
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

            // Todo:
            // kRPVControllerTrackDescription = 0x30000007,
            // kRPVControllerTrackMinValue = 0x30000008,
            // kRPVControllerTrackMaxValue = 0x30000009,
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
DataProvider::FetchWholeTrack(uint64_t track_id, double start_ts, double end_ts,
                              uint32_t horz_pixel_range)
{
    if(m_state != ProviderState::kReady)
    {
        spdlog::debug("Cannot fetch, provider not ready or error, state: {}",
                      static_cast<int>(m_state));
        return false;
    }

    const track_info_t* metadata = GetTrackInfo(track_id);
    if(metadata)
    {
        auto it = m_requests.find(track_id);
        // only allow load if a request for this id (track) is not pending
        if(it == m_requests.end())
        {
            rocprofvis_handle_t* track_future = rocprofvis_controller_future_alloc();
            rocprofvis_controller_array_t* track_array =
                rocprofvis_controller_array_alloc(metadata->num_entries);
            rocprofvis_handle_t* track_handle = nullptr;
            rocprofvis_result_t  result       = rocprofvis_controller_get_object(
                m_trace_controller, kRPVControllerTrackIndexed, track_id, &track_handle);

            if(result == kRocProfVisResultSuccess && track_handle && track_future &&
               track_array)
            {
                rocprofvis_result_t result = rocprofvis_controller_track_fetch_async(
                    m_trace_controller, (rocprofvis_controller_track_t*) track_handle,
                    start_ts, end_ts, track_future, track_array);
            }

            data_req_info_t request_info;
            request_info.request_array      = track_array;
            request_info.request_future     = track_future;
            request_info.request_obj_handle = track_handle;
            request_info.request_args       = nullptr;
            request_info.request_id         = track_id;
            request_info.loading_state      = ProviderState::kLoading;
            request_info.request_type       = RequestType::kFetchTrack;

            auto params = std::make_shared<TrackRequestParams>(track_id, start_ts, end_ts,
                                                               horz_pixel_range);
            request_info.custom_params = params;

            m_requests.emplace(request_info.request_id, request_info);

            spdlog::debug("Fetching track graph data {}", track_id);
            return true;
        }
        else
        {
            // request for item already exists
            spdlog::debug("Request for this track, index {}, is already pending", track_id);
            return false;
        }
    }
    else
    {
        spdlog::debug("Cannot fetch Track index {} is out of range", track_id);
        return false;
    }
}

bool
DataProvider::FetchTrack(uint64_t track_id, double start_ts, double end_ts,
                         uint32_t horz_pixel_range)
{
    TrackRequestParams request_params(track_id, start_ts, end_ts, horz_pixel_range);
    return FetchTrack(request_params);
}

bool
DataProvider::FetchTrack(const TrackRequestParams& request_params)
{
    if(m_state != ProviderState::kReady)
    {
        spdlog::debug("Cannot fetch, provider not ready or error, state: {}",
                      static_cast<int>(m_state));
        return false;
    }

    const track_info_t* metadata = GetTrackInfo(request_params.m_track_id);
    if(metadata)
    {
        auto it = m_requests.find(request_params.m_track_id);
        // only allow load if a request for this id (track) is not pending
        if(it == m_requests.end())
        {
            rocprofvis_handle_t* graph_future = rocprofvis_controller_future_alloc();
            rocprofvis_controller_array_t* graph_array =
                rocprofvis_controller_array_alloc(32);
            rocprofvis_handle_t* graph_obj = nullptr;

            // TODO: Switch to ID API. For now we convert the ID to index and fetch via
            // index.
            rocprofvis_result_t result = rocprofvis_controller_get_object(
                m_trace_timeline, kRPVControllerTimelineGraphIndexed,
                metadata->index, &graph_obj);

            if(result == kRocProfVisResultSuccess && graph_obj && graph_future &&
               graph_array)
            {
                result = rocprofvis_controller_graph_fetch_async(
                    m_trace_controller, graph_obj, request_params.m_start_ts,
                    request_params.m_end_ts, request_params.m_horz_pixel_range,
                    graph_future, graph_array);

                ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
            }

            data_req_info_t request_info;
            request_info.request_array      = graph_array;
            request_info.request_future     = graph_future;
            request_info.request_obj_handle = graph_obj;
            request_info.request_args       = nullptr;
            request_info.request_id         = request_params.m_track_id;
            request_info.loading_state      = ProviderState::kLoading;
            request_info.request_type       = RequestType::kFetchGraph;

            auto params = std::make_shared<TrackRequestParams>(request_params);
            request_info.custom_params = params;

            m_requests.emplace(request_info.request_id, request_info);

            spdlog::debug("Fetching track data {} from controller {}",
                          request_params.m_track_id,
                          reinterpret_cast<unsigned long long>(m_trace_controller));
            return true;
        }
        else
        {
            // request for item already exists
            spdlog::debug("Request for this track, id {}, is already pending",
                          request_params.m_track_id);
            return false;
        }
    }
    else
    {
        spdlog::debug("Cannot fetch Track id {} is invalid", request_params.m_track_id);
        return false;
    }
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
                                          double end_ts, uint64_t start_row,
                                          uint64_t req_row_count,
                                          uint64_t sort_column_index,
                                          rocprofvis_controller_sort_order_t sort_order)
{
    return FetchSingleTrackTable(
        TableRequestParams(kRPVControllerTableTypeSamples, { track_id }, start_ts, end_ts,
                           start_row, req_row_count, sort_column_index, sort_order));
}

bool
DataProvider::FetchSingleTrackEventTable(uint64_t track_id, double start_ts,
                                         double end_ts, uint64_t start_row,
                                         uint64_t req_row_count,
                                         uint64_t sort_column_index,
                                         rocprofvis_controller_sort_order_t sort_order)
{
    return FetchSingleTrackTable(
        TableRequestParams(kRPVControllerTableTypeEvents, { track_id }, start_ts, end_ts,
                           start_row, req_row_count, sort_column_index, sort_order));
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

    uint64_t track_id = table_params.m_track_ids[0];
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
            spdlog::warn("Cannot fetch event table, track {} is not an event track", track_id);
            return false;
        }
        // check if track is a sample track
        if(table_params.m_table_type == kRPVControllerTableTypeSamples &&
           metadata->track_type != kRPVControllerTrackTypeSamples)
        {
            spdlog::warn("Cannot fetch sample table, track {} is not a sample track", track_id);
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
                        metadata->graph_handle, kRPVControllerGraphTrack, 0,
                        &track_handle);

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

            auto params = std::make_shared<TableRequestParams>(table_params);
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
            spdlog::debug("Request for this table, tyoe {}, is already pending",
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
                                         uint64_t start_row, uint64_t req_row_count,
                                         uint64_t sort_column_index,
                                         rocprofvis_controller_sort_order_t sort_order)
{
    return FetchMultiTrackTable(TableRequestParams(
        kRPVControllerTableTypeSamples, track_ids, start_ts, end_ts, start_row, req_row_count, sort_column_index, sort_order));
}

bool
DataProvider::FetchMultiTrackEventTable(const std::vector<uint64_t>& track_ids,
                                        double start_ts, double end_ts,
                                        uint64_t start_row, uint64_t req_row_count,
                                        uint64_t sort_column_index,
                                        rocprofvis_controller_sort_order_t sort_order)

{
    return FetchMultiTrackTable(
        TableRequestParams(kRPVControllerTableTypeEvents, track_ids, start_ts, end_ts,
                           start_row, req_row_count, sort_column_index, sort_order));
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
                        spdlog::warn("Cannot fetch table, track id {} is invalid", track_id);
                        continue;
                    }

                    // check if track is an event track
                    if(table_params.m_table_type == kRPVControllerTableTypeEvents &&
                       metadata->track_type !=
                           kRPVControllerTrackTypeEvents)
                    {
                        spdlog::warn(
                            "Cannot fetch event table, track id {} is not a event track",
                            track_id);
                        continue;
                    }

                    // check if track is a sample track
                    if(table_params.m_table_type == kRPVControllerTableTypeSamples &&
                       metadata->track_type !=
                           kRPVControllerTrackTypeSamples)
                    {
                        spdlog::warn("Cannot fetch sample table, track id {} is not an "
                                     "sample track",
                                     track_id);
                        continue;
                    }

                    // get the track handle
                    rocprofvis_handle_t* track_handle = nullptr;
                    result                            = rocprofvis_controller_get_object(
                        metadata->graph_handle, kRPVControllerGraphTrack, 0,
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

            auto params             = std::make_shared<TableRequestParams>(table_params);
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
DataProvider::IsRequestPending(uint64_t request_id)
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
DataProvider::FreeTrack(uint64_t track_id)
{
    if(m_raw_trackdata.count(track_id) > 0)
    {
        if(m_raw_trackdata[track_id])
        {
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

void
DataProvider::DumpMetaData()
{
    for(const track_info_t* track_info : GetTrackInfoList())
    {
        spdlog::debug("Track index {}, id {}, name {}, min ts {}, max ts {}, type {}, "
                      "num entries {}",
                      track_info->index, track_info->id, track_info->name, track_info->min_ts,
                      track_info->max_ts,
                      track_info->track_type == kRPVControllerTrackTypeSamples ? "Samples"
                                                                              : "Events",
                      track_info->num_entries);
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

            // this graph is ready
            if(result == kRocProfVisResultSuccess)
            {
                rocprofvis_controller_future_free(req.request_future);
                req.request_future = nullptr;
                req.loading_state  = ProviderState::kReady;
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
DataProvider::ProcessRequest(data_req_info_t& req)
{
    if(req.request_type == RequestType::kFetchGraph)
    {
        ProcessGraphRequest(req);
    }
    else if(req.request_type == RequestType::kFetchTrack)
    {
        ProcessTrackRequest(req);
    }
    else if(req.request_type == RequestType::kFetchTrackEventTable ||
            req.request_type == RequestType::kFetchTrackSampleTable)
    {
        spdlog::debug("Processing table data {}", req.request_id);
        ProcessTableRequest(req);
    }
    else
    {
        spdlog::debug("Unknown request type {}", static_cast<int>(req.request_type));
        return;
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
        column_names.emplace_back(GetString(
            table_handle, kRPVControllerTableColumnHeaderIndexed, i));
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
                    column_value = GetString(
                        row_array, kRPVControllerArrayEntryIndexed, j);
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

    if(table_type == kRPVControllerTableTypeEvents)
    {
        table_info_t& table_info =
            m_table_infos[static_cast<size_t>(TableType::kEventTable)];
        // store the event table data
        table_info.table_header    = std::move(column_names);
        table_info.table_data      = std::move(table_data);
        table_info.table_params    = table_params;
        table_info.total_row_count = total_num_rows;
    }
    else if(table_type == kRPVControllerTableTypeSamples)
    {
        // store the sample table data
        table_info_t& table_info =
            m_table_infos[static_cast<size_t>(TableType::kSampleTable)];
        table_info.table_header    = std::move(column_names);
        table_info.table_data      = std::move(table_data);
        table_info.table_params    = table_params;
        table_info.total_row_count = total_num_rows;
    }
    else
    {
        spdlog::error("Unsupported table type: {}", static_cast<int>(table_type));
        return;
    }

    // free the array
    if(req.request_array)
    {
        rocprofvis_controller_array_free(req.request_array);
        req.request_array = nullptr;
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
            CreateRawEventData(track_params->m_track_id, req.request_array,
                               track_params->m_start_ts, track_params->m_end_ts);
            break;
        }
        case kRPVControllerTrackTypeSamples:
        {
            CreateRawSampleData(track_params->m_track_id, req.request_array,
                                track_params->m_start_ts, track_params->m_end_ts);
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
        m_track_data_ready_callback(track_params->m_track_id, m_trace_file_path);
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

    // use the track type to determine what type of data is present in the graph array
    const track_info_t* metadata = GetTrackInfo(track_params->m_track_id);
    ROCPROFVIS_ASSERT(metadata);
    switch(metadata->track_type)
    {
        case kRPVControllerTrackTypeEvents:
        {
            CreateRawEventData(track_params->m_track_id, req.request_array, min_ts, max_ts);
            break;
        }
        case kRPVControllerTrackTypeSamples:
        {
            CreateRawSampleData(track_params->m_track_id, req.request_array, min_ts, max_ts);
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
        m_track_data_ready_callback(track_params->m_track_id, m_trace_file_path);
    }
}

void
DataProvider::CreateRawSampleData(uint64_t                       track_id,
                                  rocprofvis_controller_array_t* track_data,
                                  double min_ts, double max_ts)
{
    uint64_t            count  = 0;
    rocprofvis_result_t result = rocprofvis_controller_get_uint64(
        track_data, kRPVControllerArrayNumEntries, 0, &count);
    ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);

    RawTrackSampleData* raw_sample_data = new RawTrackSampleData(track_id, min_ts, max_ts);
    spdlog::debug("Create sample track {} data with {} entries", track_id, count);

    if(GetRawTrackData(track_id))
    {
        delete m_raw_trackdata[track_id];
        m_raw_trackdata[track_id] = nullptr;
        spdlog::debug("replacing existing track data with id {}", track_id);
    }
    m_raw_trackdata[track_id] = raw_sample_data;

    std::vector<rocprofvis_trace_counter_t> buffer;
    buffer.reserve(count);

    for(uint64_t i = 0; i < count; i++)
    {
        rocprofvis_controller_sample_t* sample = nullptr;
        result                                 = rocprofvis_controller_get_object(
            track_data, kRPVControllerArrayEntryIndexed, i, &sample);
        ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess && sample);

        // Construct rocprofvis_trace_counter_t item in-place
        buffer.emplace_back();
        rocprofvis_trace_counter_t& trace_counter = buffer.back();

        double start_ts = 0;
        result = rocprofvis_controller_get_double(sample, kRPVControllerSampleTimestamp,
                                                  0, &start_ts);
        ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);

        double value = 0;
        result = rocprofvis_controller_get_double(sample, kRPVControllerSampleValue, 0,
                                                  &value);
        ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);

        trace_counter.m_start_ts = start_ts;
        trace_counter.m_value    = value;
    }

    raw_sample_data->SetData(std::move(buffer));
    m_raw_trackdata[track_id] = raw_sample_data;
}

void
DataProvider::CreateRawEventData(uint64_t                       track_id,
                                 rocprofvis_controller_array_t* track_data, double min_ts,
                                 double max_ts)
{
    uint64_t            count  = 0;
    rocprofvis_result_t result = rocprofvis_controller_get_uint64(
        track_data, kRPVControllerArrayNumEntries, 0, &count);
    ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);

    RawTrackEventData* raw_event_data = new RawTrackEventData(track_id, min_ts, max_ts);
    spdlog::debug("Create event track {} data with {} entries", track_id, count);

    if(GetRawTrackData(track_id))
    {
        delete m_raw_trackdata[track_id];
        m_raw_trackdata[track_id] = nullptr;
        spdlog::debug("replacing existing track data with id {}", track_id);
    }

    std::vector<rocprofvis_trace_event_t> buffer;
    buffer.reserve(count);

    size_t str_buffer_length = 128;
    char*  str_buffer        = new char[str_buffer_length];

    for(uint64_t i = 0; i < count; i++)
    {
        rocprofvis_controller_event_t* event = nullptr;
        result                               = rocprofvis_controller_get_object(
            track_data, kRPVControllerArrayEntryIndexed, i, &event);
        ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess && event);

        // Construct rocprofvis_trace_event_t item in-place
        buffer.emplace_back();
        rocprofvis_trace_event_t& trace_event = buffer.back();

        uint64_t id = 0;
        result = rocprofvis_controller_get_uint64(event, kRPVControllerEventId, 0, &id);
        ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
        trace_event.m_id = id;

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
        trace_event.m_level = level;

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
            spdlog::debug("Resizing str buffer to {}", str_buffer_length);
        }
        length += 1;
        result = rocprofvis_controller_get_string(event, kRPVControllerEventName, 0,
                                                  str_buffer, &length);
        ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
        trace_event.m_name.assign(str_buffer);
    }

    delete[] str_buffer;

    raw_event_data->SetData(std::move(buffer));
    m_raw_trackdata[track_id] = raw_event_data;
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

    m_event_info   = {};
    m_event_info.event_id = event_id;
    bool ret_value = false;
    auto future    = rocprofvis_controller_future_alloc();
    ROCPROFVIS_ASSERT(future != nullptr);
    auto outArray = rocprofvis_controller_array_alloc(0);
    ROCPROFVIS_ASSERT(outArray != nullptr);

    rocprofvis_result_t result = rocprofvis_controller_get_indexed_property_async(
        m_trace_controller, m_trace_controller, kRPVControllerEventDataExtDataIndexed,
        event_id, 1, future, outArray);

    if(result == kRocProfVisResultSuccess)
    {
        result = rocprofvis_controller_future_wait(future, FLT_MAX);
        if(result == kRocProfVisResultSuccess)
        {
            uint64_t prop_count = 0;
            result              = rocprofvis_controller_get_uint64(
                outArray, kRPVControllerArrayNumEntries, 0, &prop_count);
            ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
            spdlog::debug("Event {} has {} ext data properties", event_id, prop_count);
            
            m_event_info.ext_data.resize(prop_count);
            for(auto j = 0; j < prop_count; j++)
            {
                rocprofvis_handle_t* ext_data_handle = nullptr;
                result                               = rocprofvis_controller_get_object(
                    outArray, kRPVControllerArrayEntryIndexed, j, &ext_data_handle);
                ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);

                m_event_info.ext_data[j].category = GetString(ext_data_handle, kRPVControllerExtDataCategory, 0);
                m_event_info.ext_data[j].name     = GetString(ext_data_handle, kRPVControllerExtDataName, 0);
                m_event_info.ext_data[j].value    = GetString(ext_data_handle, kRPVControllerExtDataValue, 0);
            }
            ret_value = true;
        }
        else
        {
            spdlog::error("Failed to fetch event details for event id {}, result: {}",
                          event_id, static_cast<int>(result));
        }
    }
    else
    {
        spdlog::error("Failed to fetch event details for event id {}, result: {}",
                      event_id, static_cast<int>(result));
    }

    rocprofvis_controller_future_free(future);
    rocprofvis_controller_array_free(outArray);

    return ret_value;
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
        
    bool ret_value = false;
    m_flow_info    = {};
    m_flow_info.event_id = event_id;

    auto future = rocprofvis_controller_future_alloc();
    ROCPROFVIS_ASSERT(future != nullptr);
    auto outArray = rocprofvis_controller_array_alloc(0);
    ROCPROFVIS_ASSERT(outArray != nullptr);

    rocprofvis_result_t result = rocprofvis_controller_get_indexed_property_async(
        m_trace_controller, m_trace_controller, kRPVControllerEventDataFlowControlIndexed,
        event_id, 1, future, outArray);

    if(result == kRocProfVisResultSuccess)
    {
        result = rocprofvis_controller_future_wait(future, FLT_MAX);
        if(result == kRocProfVisResultSuccess)
        {
            uint64_t prop_count = 0;
            result              = rocprofvis_controller_get_uint64(
                outArray, kRPVControllerArrayNumEntries, 0, &prop_count);
            ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
            spdlog::debug("Event {} has {} flow control properties", event_id,
                          prop_count);
            
            std::vector<event_flow_data_t>& flow_data = m_flow_info.flow_data;
            flow_data.resize(prop_count);

            for(auto j = 0; j < prop_count; j++)
            {
                rocprofvis_handle_t* flow_control_handle = nullptr;
                result = rocprofvis_controller_get_object(
                    outArray, kRPVControllerArrayEntryIndexed, j, &flow_control_handle);
                ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);

                uint64_t data = 0;
                result        = rocprofvis_controller_get_uint64(
                    flow_control_handle, kRPVControllerFlowControltId, 0, &data);
                if(result == kRocProfVisResultSuccess)
                {
                    flow_data[j].id = data;
                }

                data   = 0;
                result = rocprofvis_controller_get_uint64(
                    flow_control_handle, kRPVControllerFlowControlTimestamp, 0, &data);
                if(result == kRocProfVisResultSuccess)
                {
                    flow_data[j].timestamp = data;
                }

                data   = 0;
                result = rocprofvis_controller_get_uint64(
                    flow_control_handle, kRPVControllerFlowControlTrackId, 0, &data);
                if(result == kRocProfVisResultSuccess)
                {
                    flow_data[j].track_id = data;
                }

                data   = 0;
                result = rocprofvis_controller_get_uint64(
                    flow_control_handle, kRPVControllerFlowControlDirection, 0, &data);
                if(result == kRocProfVisResultSuccess)
                {
                    flow_data[j].direction = data;
                }
            }
            ret_value = true;
        }
        else
        {
            spdlog::error(
                "Failed to fetch event flow details for event id {}, result: {}",
                event_id, static_cast<int>(result));
        }
    }
    else
    {
        spdlog::error("Failed to fetch event flow details for event id {}, result: {}",
                      event_id, static_cast<int>(result));
    }

    rocprofvis_controller_future_free(future);
    rocprofvis_controller_array_free(outArray);

    return ret_value;
}

bool DataProvider::FetchEventCallStackData(uint64_t event_id)
{
    if(m_state != ProviderState::kReady)
    {
        spdlog::debug("Cannot fetch, provider not ready or error, state: {}",
                      static_cast<int>(m_state));
        return false;
    }

    bool ret_value = false;
    m_call_stack_info = {};
    m_call_stack_info.event_id = event_id;

    auto future = rocprofvis_controller_future_alloc();
    ROCPROFVIS_ASSERT(future != nullptr);
    auto outArray = rocprofvis_controller_array_alloc(0);
    ROCPROFVIS_ASSERT(outArray != nullptr);

    rocprofvis_result_t result = rocprofvis_controller_get_indexed_property_async(
        m_trace_controller, m_trace_controller,
        kRPVControllerEventDataCallStackIndexed, event_id, 1, future, outArray);

    if(result == kRocProfVisResultSuccess)
    {
        result = rocprofvis_controller_future_wait(future, FLT_MAX);
        if(result == kRocProfVisResultSuccess)
        {
            uint64_t prop_count = 0;
            result              = rocprofvis_controller_get_uint64(
                outArray, kRPVControllerArrayNumEntries, 0, &prop_count);
            ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
            spdlog::debug("Event {} has {} call stack properties", event_id, prop_count);
            std::vector<call_stack_data_t>& call_stack_data =
                m_call_stack_info.call_stack_data;

            call_stack_data.resize(prop_count);

            for(auto i = 0; i < prop_count; i++)
            {
                rocprofvis_handle_t* callstack_handle = nullptr;
                result = rocprofvis_controller_get_object(
                    outArray, kRPVControllerArrayEntryIndexed, i, &callstack_handle);

                call_stack_data[i].function = GetString(callstack_handle, kRPVControllerCallstackFunction, i);                
                call_stack_data[i].arguments = GetString(callstack_handle, kRPVControllerCallstackArguments, i);
                call_stack_data[i].line = GetString(callstack_handle, kRPVControllerCallstackLine, i);

                //TODO: Add unimplemented properties
                //  kRPVControllerCallstackFile
                //  kRPVControllerCallstackISAFunction
                //  kRPVControllerCallstackISAFile
                //  kRPVControllerCallstackISALine
                spdlog::debug(
                    "Call stack entry {}: function: {}, line: {}, arguments: {}", i,
                    call_stack_data[i].function, call_stack_data[i].line,
                    call_stack_data[i].arguments);
            }
            ret_value = true;
        }
        else
        {
            spdlog::error("Failed to fetch event call stack data for event id {}, result: {}",
                          event_id, static_cast<int>(result));
        }
    }
    else
    {
        spdlog::error("Failed to fetch event call stack data for event id {}, result: {}",
                      event_id, static_cast<int>(result));
    }

    rocprofvis_controller_future_free(future);
    rocprofvis_controller_array_free(outArray);

    return ret_value;
}
                
char * DataProvider::GetStringAsCharArray(rocprofvis_handle_t* handle,
                                            rocprofvis_property_t  property, uint64_t index)
{
    uint32_t length = 0;
    rocprofvis_result_t result = rocprofvis_controller_get_string(
        handle, property, index, nullptr, &length);
    ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);

    if(length == 0)
    {
        return nullptr;
    }

    char* data = new char[length + 1];
    data[length] = '\0';
    result = rocprofvis_controller_get_string(handle, property, index, data, &length);
    ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);

    return data;
}   

void DataProvider::FreeStringCharArray(char* str)
{
    if(str)
    {
        delete[] str;
    }
}

std::string
DataProvider::GetString(rocprofvis_handle_t* handle, rocprofvis_property_t property, uint64_t index)
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
