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
, m_selected_event(std::numeric_limits<uint64_t>::max())
{}

DataProvider::~DataProvider() { CloseController(); }

void
DataProvider::SetSelectedEvent(uint64_t id)
{
    m_selected_event = id;
}

uint64_t
DataProvider::GetSelectedEvent()
{
    return m_selected_event;
}

void
DataProvider::CloseController()
{
    if(m_trace_controller)
    {
        rocprofvis_controller_free(m_trace_controller);
        m_trace_controller = nullptr;
    }
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
    for(RawTrackData* item : m_raw_trackdata)
    {
        if(item)
        {
            delete item;
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
DataProvider::FetchEvent(uint64_t event_id)
{
    if(m_state != ProviderState::kReady)
    {
        spdlog::debug("Cannot fetch, provider not ready or error, state: {}",
                      static_cast<int>(m_state));
        return false;
    }


    {
        rocprofvis_handle_t*           future = rocprofvis_controller_future_alloc();
        rocprofvis_controller_array_t* array  = rocprofvis_controller_array_alloc(0);

        auto result = rocprofvis_controller_get_indexed_property_async(
            m_trace_controller, m_trace_controller, kRPVControllerEventIndexed, event_id,
            1, future, array);

        if(result == kRocProfVisResultSuccess)
        {
            result = rocprofvis_controller_future_wait(future, FLT_MAX);
            if(result == kRocProfVisResultSuccess)
            {
                rocprofvis_handle_t* event_handle = nullptr;
                result                            = rocprofvis_controller_get_object(
                    array, kRPVControllerEventIndexed, 0, &event_handle);
            
                if(result == kRocProfVisResultSuccess && event_handle) {
                    spdlog::debug("Event fetched successfully, id: {}", event_id);
                }
            }
        }

        rocprofvis_controller_array_free(array);
        rocprofvis_controller_future_free(future);
    }

    return true;
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

            m_track_metadata.push_back(track_info);
            m_raw_trackdata.push_back(nullptr);
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
DataProvider::FetchWholeTrack(uint64_t index, double start_ts, double end_ts,
                              uint32_t horz_pixel_range)
{
    if(m_state != ProviderState::kReady)
    {
        spdlog::debug("Cannot fetch, provider not ready or error, state: {}",
                      static_cast<int>(m_state));
        return false;
    }

    if(index < m_track_metadata.size())
    {
        auto it = m_requests.find(index);
        // only allow load if a request for this index (track) is not pending
        if(it == m_requests.end())
        {
            rocprofvis_handle_t* track_future = rocprofvis_controller_future_alloc();
            rocprofvis_controller_array_t* track_array =
                rocprofvis_controller_array_alloc(m_track_metadata[index].num_entries);
            rocprofvis_handle_t* track_handle = nullptr;
            rocprofvis_result_t  result       = rocprofvis_controller_get_object(
                m_trace_controller, kRPVControllerTrackIndexed, index, &track_handle);

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
            request_info.request_id         = index;
            request_info.loading_state      = ProviderState::kLoading;
            request_info.request_type       = RequestType::kFetchTrack;

            auto params = std::make_shared<TrackRequestParams>(index, start_ts, end_ts,
                                                               horz_pixel_range);
            request_info.custom_params = params;

            m_requests.emplace(request_info.request_id, request_info);

            spdlog::debug("Fetching track graph data {}", index);
            return true;
        }
        else
        {
            // request for item already exists
            spdlog::debug("Request for this track, index {}, is already pending", index);
            return false;
        }
    }
    else
    {
        spdlog::debug("Cannot fetch Track index {} is out of range", index);
        return false;
    }
}

bool
DataProvider::FetchTrack(uint64_t index, double start_ts, double end_ts,
                         uint32_t horz_pixel_range)
{
    if(m_state != ProviderState::kReady)
    {
        spdlog::debug("Cannot fetch, provider not ready or error, state: {}",
                      static_cast<int>(m_state));
        return false;
    }

    if(index < m_track_metadata.size())
    {
        auto it = m_requests.find(index);
        // only allow load if a request for this index (track) is not pending
        if(it == m_requests.end())
        {
            rocprofvis_handle_t* graph_future = rocprofvis_controller_future_alloc();
            rocprofvis_controller_array_t* graph_array =
                rocprofvis_controller_array_alloc(32);
            rocprofvis_handle_t* graph_obj = nullptr;

            rocprofvis_result_t result = rocprofvis_controller_get_object(
                m_trace_timeline, kRPVControllerTimelineGraphIndexed, index, &graph_obj);

            if(result == kRocProfVisResultSuccess && graph_obj && graph_future &&
               graph_array)
            {
                result = rocprofvis_controller_graph_fetch_async(
                    m_trace_controller, graph_obj, start_ts, end_ts, horz_pixel_range,
                    graph_future, graph_array);

                ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
            }

            data_req_info_t request_info;
            request_info.request_array      = graph_array;
            request_info.request_future     = graph_future;
            request_info.request_obj_handle = graph_obj;
            request_info.request_args       = nullptr;
            request_info.request_id         = index;
            request_info.loading_state      = ProviderState::kLoading;
            request_info.request_type       = RequestType::kFetchGraph;

            auto params = std::make_shared<TrackRequestParams>(index, start_ts, end_ts,
                                                               horz_pixel_range);
            request_info.custom_params = params;

            m_requests.emplace(request_info.request_id, request_info);

            spdlog::debug("Fetching track data {} from controller {}", index,
                          reinterpret_cast<unsigned long long>(m_trace_controller));
            return true;
        }
        else
        {
            // request for item already exists
            spdlog::debug("Request for this track, index {}, is already pending", index);
            return false;
        }
    }
    else
    {
        spdlog::debug("Cannot fetch Track index {} is out of range", index);
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
DataProvider::FetchSingleTrackSampleTable(uint64_t index, double start_ts, double end_ts,
                                          uint64_t start_row, uint64_t req_row_count,
                                          uint64_t sort_column_index,
                                          rocprofvis_controller_sort_order_t sort_order)
{
    return FetchSingleTrackTable(
        TableRequestParams(kRPVControllerTableTypeSamples, { index }, start_ts, end_ts,
                           start_row, req_row_count, sort_column_index, sort_order));
}

bool
DataProvider::FetchSingleTrackEventTable(uint64_t index, double start_ts, double end_ts,
                                         uint64_t start_row, uint64_t req_row_count,
                                         uint64_t sort_column_index,
                                         rocprofvis_controller_sort_order_t sort_order)
{
    return FetchSingleTrackTable(
        TableRequestParams(kRPVControllerTableTypeEvents, { index }, start_ts, end_ts,
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

    if(table_params.m_track_indices.empty())
    {
        spdlog::debug("Cannot fetch table, no track indices provided");
        return false;
    }

    uint64_t index = table_params.m_track_indices[0];
    if(index < m_track_metadata.size())
    {
        uint64_t request_id = table_params.m_table_type == kRPVControllerTableTypeEvents
                                  ? EVENT_TABLE_REQUEST_ID
                                  : SAMPLE_TABLE_REQUEST_ID;

        auto it = m_requests.find(request_id);

        // check if track is an event track
        if(table_params.m_table_type == kRPVControllerTableTypeEvents &&
           m_track_metadata[index].track_type != kRPVControllerTrackTypeEvents)
        {
            spdlog::warn("Cannot fetch event table, track {} is not an event track",
                         index);
            return false;
        }
        // check if track is a sample track
        if(table_params.m_table_type == kRPVControllerTableTypeSamples &&
           m_track_metadata[index].track_type != kRPVControllerTrackTypeSamples)
        {
            spdlog::warn("Cannot fetch sample table, track {} is not a sample track",
                         index);
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
                m_trace_controller, kRPVControllerTrackIndexed, index, &track_handle);
            ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
            ROCPROFVIS_ASSERT(track_handle != nullptr);

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

            std::vector<uint64_t> track_indices;
            track_indices.push_back(index);

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
        spdlog::debug("Cannot fetch track, index {} is out of range", index);
        return false;
    }
}

bool
DataProvider::FetchMultiTrackSampleTable(const std::vector<uint64_t>& track_indices,
                                         double start_ts, double end_ts,
                                         uint64_t start_row, uint64_t req_row_count,
                                         uint64_t sort_column_index,
                                         rocprofvis_controller_sort_order_t sort_order)
{
    return FetchMultiTrackTable(TableRequestParams(
        kRPVControllerTableTypeSamples, track_indices, start_ts, end_ts, start_row,
        req_row_count, sort_column_index, sort_order));
}

bool
DataProvider::FetchMultiTrackEventTable(const std::vector<uint64_t>& track_indices,
                                        double start_ts, double end_ts,
                                        uint64_t start_row, uint64_t req_row_count,
                                        uint64_t sort_column_index,
                                        rocprofvis_controller_sort_order_t sort_order)

{
    return FetchMultiTrackTable(
        TableRequestParams(kRPVControllerTableTypeEvents, track_indices, start_ts, end_ts,
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

    std::vector<uint64_t> filtered_track_indices;
    if(!table_params.m_track_indices.empty())
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
                for(const auto& index : table_params.m_track_indices)
                {
                    // skip track if index is out of range
                    if(index >= m_track_metadata.size())
                    {
                        spdlog::warn("Cannot fetch table, track index {} is out of range",
                                     index);
                        continue;
                    }

                    // check if track is an event track
                    if(table_params.m_table_type == kRPVControllerTableTypeEvents &&
                       m_track_metadata[index].track_type !=
                           kRPVControllerTrackTypeEvents)
                    {
                        spdlog::warn(
                            "Cannot fetch event table, track {} is not a event track",
                            index);
                        continue;
                    }

                    // check if track is a sample track
                    if(table_params.m_table_type == kRPVControllerTableTypeSamples &&
                       m_track_metadata[index].track_type !=
                           kRPVControllerTrackTypeSamples)
                    {
                        spdlog::warn(
                            "Cannot fetch sample table, track {} is not an sample track",
                            index);
                        continue;
                    }

                    // get the track handle
                    rocprofvis_handle_t* track_handle = nullptr;
                    result = rocprofvis_controller_get_object(m_trace_controller,
                                                              kRPVControllerTrackIndexed,
                                                              index, &track_handle);
                    ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
                    ROCPROFVIS_ASSERT(track_handle != nullptr);

                    result = rocprofvis_controller_set_object(
                        args, kRPVControllerTableArgsTracksIndexed, num_table_tracks,
                        track_handle);
                    ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);

                    filtered_track_indices.push_back(index);
                    num_table_tracks++;
                    spdlog::debug("Adding track {} to table request", index);
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
            params->m_track_indices = std::move(filtered_track_indices);
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
            spdlog::debug("Request for this table, tyoe {}, is already pending",
                          static_cast<uint64_t>(table_params.m_table_type));
            return false;
        }
    }
    else
    {
        spdlog::debug("Cannot fetch table, no track indices provided");
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
DataProvider::GetRawTrackData(uint64_t index)
{
    return m_raw_trackdata[index];
}

const track_info_t*
DataProvider::GetTrackInfo(uint64_t index)
{
    if(index < m_track_metadata.size())
    {
        return &m_track_metadata[index];
    }
    return nullptr;
}

bool
DataProvider::FreeTrack(uint64_t index)
{
    if(index < m_raw_trackdata.size())
    {
        if(m_raw_trackdata[index])
        {
            delete m_raw_trackdata[index];
            m_raw_trackdata[index] = nullptr;
            spdlog::debug("Deleted track data at index: {}", index);
            return true;
        }
        else
        {
            spdlog::debug("No track data to delete at index: {}", index);
        }
    }
    else
    {
        spdlog::debug("Cannot delete track data, index out range at index: {}", index);
    }
    return false;
}

void
DataProvider::DumpMetaData()
{
    for(track_info_t& track_info : m_track_metadata)
    {
        spdlog::debug("Track index {}, id {}, name {}, min ts {}, max ts {}, type {}, "
                      "num entries {}",
                      track_info.index, track_info.id, track_info.name, track_info.min_ts,
                      track_info.max_ts,
                      track_info.track_type == kRPVControllerTrackTypeSamples ? "Samples"
                                                                              : "Events",
                      track_info.num_entries);
    }
}

bool
DataProvider::DumpTrack(uint64_t index)
{
    if(index < m_raw_trackdata.size())
    {
        if(m_raw_trackdata[index])
        {
            bool result = false;
            switch(m_raw_trackdata[index]->GetType())
            {
                case kRPVControllerTrackTypeSamples:
                {
                    RawTrackSampleData* track =
                        dynamic_cast<RawTrackSampleData*>(m_raw_trackdata[index]);
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
                        dynamic_cast<RawTrackEventData*>(m_raw_trackdata[index]);
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
            spdlog::debug("No track data at index: {}", index);
        }
    }
    else
    {
        spdlog::debug("Cannot show track data, index out range at index: {}", index);
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

    for(int i = 0; i < num_columns; i++)
    {
        uint32_t len = 0;
        result       = rocprofvis_controller_get_string(
            table_handle, kRPVControllerTableColumnHeaderIndexed, i, nullptr, &len);
        ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);

        std::string name;
        name.resize(len);

        result = rocprofvis_controller_get_string(
            table_handle, kRPVControllerTableColumnHeaderIndexed, i,
            const_cast<char*>(name.c_str()), &len);
        ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);

        column_names.push_back(std::move(name));
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
                    uint32_t len = 0;
                    result       = rocprofvis_controller_get_string(
                        row_array, kRPVControllerArrayEntryIndexed, j, nullptr, &len);
                    ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);

                    column_value.resize(len);

                    result = rocprofvis_controller_get_string(
                        row_array, kRPVControllerArrayEntryIndexed, j,
                        const_cast<char*>(column_value.c_str()), &len);
                    ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);

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
    ROCPROFVIS_ASSERT(track_params->m_index < m_track_metadata.size());
    switch(m_track_metadata[track_params->m_index].track_type)
    {
        case kRPVControllerTrackTypeEvents:
        {
            CreateRawEventData(track_params->m_index, req.request_array,
                               track_params->m_start_ts, track_params->m_end_ts);
            break;
        }
        case kRPVControllerTrackTypeSamples:
        {
            CreateRawSampleData(track_params->m_index, req.request_array,
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
        m_track_data_ready_callback(track_params->m_index, m_trace_file_path);
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

    spdlog::debug("{} Graph item count {} min ts {} max ts {}", track_params->m_index,
                  item_count, min_ts, max_ts);

    // use the track type to determine what type of data is present in the graph array
    ROCPROFVIS_ASSERT(track_params->m_index < m_track_metadata.size());
    switch(m_track_metadata[track_params->m_index].track_type)
    {
        case kRPVControllerTrackTypeEvents:
        {
            CreateRawEventData(track_params->m_index, req.request_array, min_ts, max_ts);
            break;
        }
        case kRPVControllerTrackTypeSamples:
        {
            CreateRawSampleData(track_params->m_index, req.request_array, min_ts, max_ts);
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
        m_track_data_ready_callback(track_params->m_index, m_trace_file_path);
    }
}

void
DataProvider::CreateRawSampleData(uint64_t                       index,
                                  rocprofvis_controller_array_t* track_data,
                                  double min_ts, double max_ts)
{
    uint64_t            count  = 0;
    rocprofvis_result_t result = rocprofvis_controller_get_uint64(
        track_data, kRPVControllerArrayNumEntries, 0, &count);
    ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);

    RawTrackSampleData* raw_sample_data = new RawTrackSampleData(index, min_ts, max_ts);
    spdlog::debug("Create sample track data at index {} with {} entries", index, count);

    if(m_raw_trackdata[index])
    {
        delete m_raw_trackdata[index];
        m_raw_trackdata[index] = nullptr;
        spdlog::debug("replacing existing track data at index {}", index);
    }
    m_raw_trackdata[index] = raw_sample_data;

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
    m_raw_trackdata[index] = raw_sample_data;
}

void
DataProvider::CreateRawEventData(uint64_t                       index,
                                 rocprofvis_controller_array_t* track_data, double min_ts,
                                 double max_ts)
{
    uint64_t            count  = 0;
    rocprofvis_result_t result = rocprofvis_controller_get_uint64(
        track_data, kRPVControllerArrayNumEntries, 0, &count);
    ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);

    RawTrackEventData* raw_event_data = new RawTrackEventData(index, min_ts, max_ts);
    spdlog::debug("Create event track data at index {} with {} entries", index, count);

    if(m_raw_trackdata[index])
    {
        delete m_raw_trackdata[index];
        m_raw_trackdata[index] = nullptr;
        spdlog::debug("replacing existing track data at index {}", index);
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
    m_raw_trackdata[index] = raw_event_data;
}