// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.
#pragma once

#include "rocprofvis_controller_enums.h"
#include "rocprofvis_controller_types.h"
#include "rocprofvis_interface_types.h"
#include "rocprofvis_raw_track_data.h"

#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace RocProfVis
{
namespace View
{

enum class ProviderState
{
    kInit,
    kLoading,
    kReady,
    kError
};

enum class RequestType
{
    kFetchTrack,
    kFetchGraph,
    kFetchTrackEventTable,
    kFetchTrackSampleTable,
    kFetchEventSearchTable,
    kFetchEventExtendedData,
    kFetchEventFlowDetails,
    kFetchEventCallStack,
    kSaveTrimmedTrace
};

enum class TableType
{
    kSampleTable,
    kEventTable,
    kEventSearchTable,
    __kTableTypeCount
};

typedef struct track_info_t
{
    uint64_t                           index;  // index of the track in the controller
    uint64_t                           id;     // id of the track in the controller
    std::string                        name;   // name of the track
    rocprofvis_controller_track_type_t track_type;   // the type of track
    double                             min_ts;       // starting time stamp of track
    double                             max_ts;       // ending time stamp of track
    uint64_t                           num_entries;  // number of entries in the track
    double min_value;  // minimum value in the track (for samples) or level (for events)
    double max_value;  // maximum value in the track (for samples) or level (for events)
    rocprofvis_handle_t* graph_handle;  // handle to the graph object owned by the track
    struct Topology
    {
        uint64_t node_id;     // ID of track's parent node
        uint64_t process_id;  // ID of track's parent process
        uint64_t device_id;   // ID of track's parent device
        enum
        {
            Unknown,
            Queue,
            Stream,
            Thread,
            Counter
        } type;
        uint64_t id;  // ID of queue/stream/thread/counter
    } topology;
} track_info_t;

typedef struct event_ext_data_t
{
    std::string category;
    std::string name;
    std::string value;
} event_ext_data_t;

typedef struct event_flow_data_t
{
    uint64_t    id;
    uint64_t    timestamp;
    uint64_t    track_id;
    uint64_t    level;
    uint64_t    direction;
    std::string name;
} event_flow_data_t;

typedef struct call_stack_data_t
{
    std::string function;   // Source code function name
    std::string arguments;  // Source code function arguments
    std::string file;       // Source code file path
    std::string line;       // Source code line number

    std::string isa_function;  // ISA/ASM function name
    std::string isa_file;      // ISA/ASM file path
    std::string isa_line;      // ISA/ASM line number
} call_stack_data_t;

typedef struct event_info_t
{
    uint64_t                       track_id;  // ID of owning track.
    rocprofvis_trace_event_t       basic_info;
    std::vector<event_ext_data_t>  ext_info;
    std::vector<event_flow_data_t> flow_info;
    std::vector<call_stack_data_t> call_stack_info;
} event_info_t;

typedef struct node_info_t
{
    uint64_t              id;
    std::string           host_name;
    std::string           os_name;
    std::string           os_release;
    std::string           os_version;
    std::vector<uint64_t> device_ids;   // IDs of this node's devices
    std::vector<uint64_t> process_ids;  // IDs of this node's processes
} node_info_t;

typedef struct device_info_t
{
    uint64_t    id;
    std::string product_name;
    std::string type;        // GPU/CPU
    uint64_t    type_index;  // GPU0, GPU1...etc
} device_info_t;

typedef struct process_info_t
{
    uint64_t              id;
    double                start_time;
    double                end_time;
    std::string           command;
    std::string           environment;
    std::vector<uint64_t> thread_ids;   // IDs of this process' threads
    std::vector<uint64_t> queue_ids;    // IDs of this process' queues
    std::vector<uint64_t> stream_ids;   // IDs of this process' streams
    std::vector<uint64_t> counter_ids;  // IDs of this process' counters
} process_info_t;

typedef struct thread_info_t
{
    uint64_t    id;
    std::string name;
    double      start_time;
    double      end_time;
} thread_info_t;

typedef struct queue_info_t
{
    uint64_t    id;
    std::string name;
    uint64_t    device_id;  // ID of owning device.
} queue_info_t;

typedef struct stream_info_t
{
    uint64_t    id;
    std::string name;
    uint64_t    device_id;  // ID of owning device.
} stream_info_t;

typedef struct counter_info_t
{
    uint64_t    id;
    std::string name;
    uint64_t    device_id;  // ID of owning device.
    std::string description;
    std::string units;
    std::string value_type;
} counter_info_t;

class RequestParamsBase
{
public:
    virtual ~RequestParamsBase() = default;
};

// Track request parameters
class TrackRequestParams : public RequestParamsBase
{
public:
    uint32_t m_track_id;          // id of track that is being requested
    double   m_start_ts;          // start time stamp of data being requested
    double   m_end_ts;            // end time stamp of data being requested
    uint32_t m_horz_pixel_range;  // horizontal pixel range for the request
    uint8_t  m_data_group_id;     // group id for the request, used for grouping requests
    uint16_t m_chunk_index;       // index of the chunk being requested
    size_t   m_chunk_count;       // total number of chunks for the track

    TrackRequestParams(const TrackRequestParams& other)            = default;
    TrackRequestParams& operator=(const TrackRequestParams& other) = default;

    TrackRequestParams(uint32_t track_id, double start_ts, double end_ts,
                       uint32_t horz_pixel_range, uint8_t group_id, uint16_t chunk_index,
                       size_t chunk_count)
    : m_track_id(track_id)
    , m_start_ts(start_ts)
    , m_end_ts(end_ts)
    , m_horz_pixel_range(horz_pixel_range)
    , m_data_group_id(group_id)
    , m_chunk_index(chunk_index)
    , m_chunk_count(chunk_count)
    {}
};

// Table request parameters
class TableRequestParams : public RequestParamsBase
{
public:
    rocprofvis_controller_table_type_t m_table_type;  // type of the table
    std::vector<uint64_t>              m_track_ids;   // ids of the tracks in the table
    std::vector<rocprofvis_dm_event_operation_t> m_op_types;  // op types in the table
    double   m_start_ts;           // starting time stamp of the data in the table
    double   m_end_ts;             // ending time stamp of the data in the table
    uint64_t m_start_row;          // starting row of the data in the table
    uint64_t m_req_row_count;      // number of rows requested
    uint64_t m_sort_column_index;  // index of the column to sort by
    rocprofvis_controller_sort_order_t m_sort_order;  // sort order of the column
    std::string                        m_filter;
    std::vector<std::string>           m_string_table_filters; // strings to use for string table filtering.
    std::string                        m_group;
    std::string                        m_group_columns;

    TableRequestParams(const TableRequestParams& table_params)            = default;
    TableRequestParams& operator=(const TableRequestParams& table_params) = default;

    TableRequestParams(
        rocprofvis_controller_table_type_t                  table_type,
        const std::vector<uint64_t>&                        track_ids,
        const std::vector<rocprofvis_dm_event_operation_t>& op_types, double start_ts,
        double end_ts, char const* filter, char const* group, char const* group_cols,
        const std::vector<std::string> string_table_filters = {}, uint64_t start_row = -1,
        uint64_t req_row_count = -1, uint64_t sort_column_index = 0,
        rocprofvis_controller_sort_order_t sort_order = kRPVControllerSortOrderAscending)
    : m_table_type(table_type)
    , m_track_ids(track_ids)
    , m_op_types(op_types)
    , m_start_ts(start_ts)
    , m_end_ts(end_ts)
    , m_start_row(start_row)
    , m_req_row_count(req_row_count)
    , m_sort_column_index(sort_column_index)
    , m_sort_order(sort_order)
    , m_filter(filter)
    , m_group(group)
    , m_group_columns(group_cols)
    , m_string_table_filters(string_table_filters)
    {}
};

// Event request parameters
class EventRequestParams : public RequestParamsBase
{
public:
    uint64_t m_event_id;

    EventRequestParams(const EventRequestParams& table_params)            = default;
    EventRequestParams& operator=(const EventRequestParams& table_params) = default;

    EventRequestParams(uint64_t event_id)
    : m_event_id(event_id)
    {}
};

typedef struct data_req_info_t
{
    uint64_t                              request_id;      // unique id of the request
    rocprofvis_controller_future_t*       request_future;  // future for the request
    rocprofvis_controller_array_t*        request_array;  // array of data for the request
    rocprofvis_handle_t*                  request_obj_handle;  // object for the request
    rocprofvis_controller_arguments_t*    request_args;   // arguments for the request
    ProviderState                         loading_state;  // state of the request
    RequestType                           request_type;   // type of request
    std::shared_ptr<RequestParamsBase>    custom_params;  // custom request parameters
    std::chrono::steady_clock::time_point request_time;  // time when the request was made
    uint64_t                              response_code;  // response code for the request
} data_req_info_t;

typedef struct formatted_column_data_t
{
    // this column needs formatting
    bool needs_formatting;
    //only populated if needs_formatting is true
    std::vector<std::string> formatted_row_value;
} formatted_column_info_t;

typedef struct table_info_t
{
    std::shared_ptr<TableRequestParams>   table_params;
    std::vector<std::string>              table_header;
    std::vector<std::vector<std::string>> table_data;
    // formatted version of the table data but only for columns that need formatting
    std::vector<formatted_column_info_t>  formatted_column_data;
    uint64_t                              total_row_count;
} table_info_t;

class DataProvider
{
public:
    static constexpr uint8_t TRACK_CHUNK_OFFSET_BITS = sizeof(uint32_t) * 8;
    static constexpr uint8_t TRACK_GROUP_OFFSET_BITS =
        sizeof(uint16_t) * 8 + TRACK_CHUNK_OFFSET_BITS;
    static constexpr uint8_t REQUEST_TYPE_OFFSET_BITS =
        sizeof(uint8_t) * 8 + TRACK_GROUP_OFFSET_BITS;

    static uint64_t MakeTrackDataRequestId(uint32_t track_id, uint16_t chunk_index,
                                           uint8_t group_id, RequestType request_type)
    {
        return (static_cast<uint64_t>(request_type) << REQUEST_TYPE_OFFSET_BITS) |
               (static_cast<uint64_t>(group_id) << TRACK_GROUP_OFFSET_BITS) |
               (static_cast<uint64_t>(chunk_index) << TRACK_CHUNK_OFFSET_BITS) |
               (static_cast<uint64_t>(track_id));
    }

    static uint64_t MakeRequestId(RequestType request_type)
    {
        return (static_cast<uint64_t>(request_type) << REQUEST_TYPE_OFFSET_BITS) |
               static_cast<uint64_t>(0);
    }

    static const uint64_t EVENT_TABLE_REQUEST_ID;
    static const uint64_t SAMPLE_TABLE_REQUEST_ID;
    static const uint64_t EVENT_SEARCH_REQUEST_ID;
    static const uint64_t EVENT_EXTENDED_DATA_REQUEST_ID;
    static const uint64_t EVENT_FLOW_DATA_REQUEST_ID;
    static const uint64_t EVENT_CALL_STACK_DATA_REQUEST_ID;
    static const uint64_t SAVE_TRIMMED_TRACE_REQUEST_ID;

    DataProvider();
    ~DataProvider();

    const event_info_t* GetEventInfo(uint64_t event_id) const;

    /*
     * Fetches an event of a track from the controller. Stores the data in m_event_data.
     * @param track_id: ID of the track that owns the event to fetch.
     * @param event_id: ID of event to fetch
     */
    bool FetchEvent(uint64_t track_id, uint64_t event_id);
    bool FetchEventExtData(uint64_t event_id);
    bool FetchEventFlowDetails(uint64_t event_id);
    bool FetchEventCallStackData(uint64_t event_id);

    /*
     *   Close the controller.
     */
    void CloseController();

    /*
     *   Notify controller it can consume more resources.
     */
    void SetSelectedState(const std::string& id);

    /*
     *   Free all requests. This does not cancel the requests on the controller end.
     */
    void FreeRequests();

    /*
     *   Free all track data
     */
    void FreeAllTracks();

    /*
     * Opens a trace file and loads the data into the controller.
     * Any previous data will be cleared.
     * @param file_path: The path to the trace file to load.
     *
     */
    bool FetchTrace(const std::string& file_path);

    /*
     * Fetches a track from the controller. Stores the data in a raw track buffer.
     * @param id: The id of the track to select
     * @param start_ts: The start timestamp of the track
     * @param end_ts: The end timestamp of the track
     * @param horz_pixel_range: The horizontal pixel range of the view
     * @param group_id: The group id for the request, used for grouping requests
     */
    std::pair<bool, uint64_t> FetchTrack(uint32_t track_id, double start_ts,
                                         double end_ts, uint32_t horz_pixel_range,
                                         uint8_t group_id, uint16_t chunk_index = 0,
                                         size_t chunk_count = 1);

    std::pair<bool, uint64_t> FetchTrack(const TrackRequestParams& request_params);

    bool FetchWholeTrack(uint32_t track_id, double start_ts, double end_ts,
                         uint32_t horz_pixel_range, uint8_t group_id,
                         uint16_t chunk_index = 0, size_t chunk_count = 1);

    /*
     * Fetches an event table from the controller for a single track.
     * @param id: The id of the track to select
     * @param start_ts: The start timestamp of the event table
     * @param end_ts: The end timestamp of the event table
     * @param filter: The SQL filter for the sample table
     * @param group: The SQL column name to group by for the event table
     * @param group_cols: The SQL column definition when grouping the event table
     * @param start_row: The starting row of the sample table
     * @param req_row_count: The number of rows to request
     * @param sort_column_index: The index of the column to sort by
     * @param sort_order: The sort order of the column
     */
    bool FetchSingleTrackEventTable(
        uint64_t track_id, double start_ts, double end_ts, char const* filter,
        char const* group, char const* group_cols, uint64_t start_row = -1,
        uint64_t req_row_count = -1, uint64_t sort_column_index = 0,
        rocprofvis_controller_sort_order_t sort_order = kRPVControllerSortOrderAscending);

    /*
     * Fetches a sample table from the controller for a single track.
     * @param id: The id of the track to select
     * @param start_ts: The start timestamp of the sample table
     * @param end_ts: The end timestamp of the sample table
     * @param filter: The SQL filter for the sample table
     * @param start_row: The starting row of the sample table
     * @param req_row_count: The number of rows to request
     * @param sort_column_index: The index of the column to sort by
     * @param sort_order: The sort order of the column
     */
    bool FetchSingleTrackSampleTable(
        uint64_t track_id, double start_ts, double end_ts, char const* filter,
        uint64_t start_row = -1, uint64_t req_row_count = -1,
        uint64_t                           sort_column_index = 0,
        rocprofvis_controller_sort_order_t sort_order = kRPVControllerSortOrderAscending);

    /*
     * Fetches a table from the controller for a single track.
     * @param table_params: The parameters for the table request
     */
    bool FetchSingleTrackTable(const TableRequestParams& table_params);

    bool FetchMultiTrackSampleTable(
        const std::vector<uint64_t>& track_ids, double start_ts, double end_ts,
        char const* filter, uint64_t start_row = -1, uint64_t req_row_count = -1,
        uint64_t                           sort_column_index = 0,
        rocprofvis_controller_sort_order_t sort_order = kRPVControllerSortOrderAscending);

    bool FetchMultiTrackEventTable(
        const std::vector<uint64_t>& track_ids, double start_ts, double end_ts,
        char const* filter, char const* group, char const* group_cols,
        uint64_t start_row = -1, uint64_t req_row_count = -1,
        uint64_t                           sort_column_index = 0,
        rocprofvis_controller_sort_order_t sort_order = kRPVControllerSortOrderAscending);

    bool FetchTable(const TableRequestParams& table_params);

    bool IsRequestPending(uint64_t request_id) const;

    /*
     * Release memory buffer holding raw data for selected track
     * @param id: The id of the track to select
     * @param force: If true, the track will be freed even if not all data is ready.
     * @return: True if the track was freed, false if not.
     */
    bool FreeTrack(uint64_t track_id, bool force = false);

    /* Cancels a pending request.
     * @param request_id: The id of the request to cancel.
     * @return: True if the cancel operation was accepted.
     */
    bool CancelRequest(uint64_t request_id);

    /*
     * Erases an event's data from m_event_data.
     * @param id: ID of the event to erase
     */
    bool FreeEvent(uint64_t event_id);

    /* Removes all events from m_event_data. */
    bool FreeAllEvents();

    /*
     * Output track list meta data.
     */
    void DumpMetaData();

    /*
     * Output raw data of a track to the log.
     * @param id: The id of the track to dump.
     */
    bool DumpTrack(uint64_t track_id);

    void DumpTable(TableType type);

    void DumpTable(const std::vector<std::string>&              header,
                   const std::vector<std::vector<std::string>>& data);
    /*
    Gets a histogram of all tracks in the controller.
    */
    const std::vector<double>& GetHistogram();
    /*
     Gets a Minimap of all tracks individually in the controller.
     */
    const std::map<int, std::vector<double>>& GetMiniMap();

    /*
     * Performs all data processing.  Call this from the "game loop".
     */
    void Update();

    /*
     * Gets the start timestamp of the trace
     */
    double GetStartTime();

    /*
     * Gets the end timestamp of the trace
     */
    double GetEndTime();

    /*
     * Get access to the raw track data.  The returned pointer should only be
     * considered valid until the next call to Update() or any of the memory
     * freeing functions and FetchTrace() or CloseController().
     * @param id: The id of the track data to fetch.
     * @return: Pointer to the raw track data
     * or null if data for this track has not been fetched.
     */
    const RawTrackData* GetRawTrackData(uint64_t track_id);

    const track_info_t*              GetTrackInfo(uint64_t track_id);
    std::vector<const track_info_t*> GetTrackInfoList();
    uint64_t                         GetTrackCount();

    std::vector<const node_info_t*> GetNodeInfoList() const;
    const device_info_t*            GetDeviceInfo(uint64_t device_id) const;
    const process_info_t*           GetProcessInfo(uint64_t process_id) const;
    const thread_info_t*            GetThreadInfo(uint64_t thread_id) const;
    const queue_info_t*             GetQueueInfo(uint64_t queue_id) const;
    const stream_info_t*            GetStreamInfo(uint64_t stream_id) const;
    const counter_info_t*           GetCounterInfo(uint64_t counter_id) const;

    const std::string& GetTraceFilePath();

    ProviderState GetState();

    const std::vector<std::string>&              GetTableHeader(TableType type);
    const std::vector<std::vector<std::string>>& GetTableData(TableType type);
    std::shared_ptr<TableRequestParams>          GetTableParams(TableType type);
    uint64_t                                     GetTableTotalRowCount(TableType type);
    void                                         ClearTable(TableType type);
    const std::vector<formatted_column_info_t>&  GetFormattedTableData(TableType type);
    std::vector<formatted_column_info_t>&        GetMutableFormattedTableData(TableType type);


    const char* GetProgressMessage();

    void SetTrackMetadataChangedCallback(
        const std::function<void(const std::string&)>& callback);
    void SetTableDataReadyCallback(
        const std::function<void(const std::string&, uint64_t)>& callback);
    void SetTrackDataReadyCallback(
        const std::function<void(uint64_t, const std::string&, const data_req_info_t&)>&
            callback);
    void SetTraceLoadedCallback(
        const std::function<void(const std::string&, uint64_t)>& callback);
    void SetSaveTraceCallback(const std::function<void(bool)>& callback);

    /*
     * Moves a graph inside the controller's timeline to a specified index and updates the
     * indexes of m_track_metadata.
     * @param track_id: The id of the track to move
     * @param index: The desired index of the track.
     * @return: True if operation is successful.
     */
    bool SetGraphIndex(uint64_t track_id, uint64_t index);

    bool SaveTrimmedTrace(const std::string& path, double start_ns, double end_ns);

private:
    void HandleLoadTrace();
    void HandleLoadSystemTopology();
    void HandleLoadTrackMetaData();
    void HandleRequests();

    void ProcessRequest(data_req_info_t& req);
    void ProcessEventExtendedRequest(data_req_info_t& req);
    void ProcessEventFlowDetailsRequest(data_req_info_t& req);
    void ProcessEventCallStackRequest(data_req_info_t& req);
    void ProcessGraphRequest(data_req_info_t& req);
    void ProcessTrackRequest(data_req_info_t& req);
    void ProcessTableRequest(data_req_info_t& req);
    void ProcessSaveTrimmedTraceRequest(data_req_info_t& req);

    bool SetupCommonTableArguments(rocprofvis_controller_arguments_t* args,
                                   const TableRequestParams&          table_params);

    void CreateRawEventData(const TrackRequestParams& params, const data_req_info_t& req);
    void CreateRawSampleData(const TrackRequestParams& params,
                             const data_req_info_t&    req);

    std::string GetString(rocprofvis_handle_t* handle, rocprofvis_property_t property,
                          uint64_t index);

    rocprofvis_controller_future_t*   m_trace_future;
    rocprofvis_controller_t*          m_trace_controller;
    rocprofvis_controller_timeline_t* m_trace_timeline;

    ProviderState m_state;

    uint64_t    m_num_graphs;       // number of graphs contained in the trace
    double      m_min_ts;           // timeline start point
    double      m_max_ts;           // timeline end point
    std::string m_trace_file_path;  // path to the trace file

    std::unordered_map<uint64_t, track_info_t>  m_track_metadata;
    std::unordered_map<uint64_t, RawTrackData*> m_raw_trackdata;
    std::unordered_map<uint64_t, event_info_t>  m_event_data;

    std::unordered_map<uint64_t, node_info_t>    m_node_infos;
    std::unordered_map<uint64_t, device_info_t>  m_device_infos;
    std::unordered_map<uint64_t, process_info_t> m_process_infos;
    std::unordered_map<uint64_t, thread_info_t>  m_thread_infos;
    std::unordered_map<uint64_t, queue_info_t>   m_queue_infos;
    std::unordered_map<uint64_t, stream_info_t>  m_stream_infos;
    std::unordered_map<uint64_t, counter_info_t> m_counter_infos;

    // Store table_info_t for each TableType in a vector
    std::vector<table_info_t> m_table_infos;

    std::unordered_map<int64_t, data_req_info_t> m_requests;

    // Global Histogram Vector
    std::vector<double> m_histogram;
    // Minimap per track.
    std::map<int, std::vector<double>> m_mini_map;

    // Called when track metadata has changed
    std::function<void(const std::string&)> m_track_metadata_changed_callback;
    // Called when table data has changed
    std::function<void(const std::string&, uint64_t)> m_table_data_ready_callback;
    // Called when new track data is ready
    std::function<void(uint64_t, const std::string&, const data_req_info_t&)>
        m_track_data_ready_callback;
    // Called when a new trace is loaded
    std::function<void(const std::string&, uint64_t)> m_trace_data_ready_callback;
    // Callback when trace is saved
    std::function<void(bool)> m_save_trace_callback;
    // Current loading status message retrieved form data model
    std::string m_progress_mesage;
    // Current loading status progress in percents
    uint64_t m_progress_percent;
};

}  // namespace View
}  // namespace RocProfVis
