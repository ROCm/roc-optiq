// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.
#pragma once

#include "rocprofvis_controller_enums.h"
#include "rocprofvis_controller_types.h"
#include "rocprofvis_raw_track_data.h"
#include "rocprofvis_structs.h"

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
    kFetchTrackSampleTable
};

enum class TableType
{
    kSampleTable,
    kEventTable,
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
    rocprofvis_handle_t* graph_handle;  // handle to the graph object owned by the track
} track_info_t;

typedef struct event_ext_data_t
{
    std::string category;
    std::string name;
    std::string value;
} event_ext_data_t;

typedef struct selected_event_t
{
    uint64_t event_id;     // id of the selected event
    double   position_ns;  // position of the event in the timeline
    uint64_t track_id;     // id of the track where the event is located
} selected_event_t;

typedef struct event_info_t
{
    uint64_t event_id;  // id of the event for which the extended info is stored
    std::vector<event_ext_data_t> ext_data;
} event_info_t;

typedef struct event_flow_data_t
{
    uint64_t id;
    uint64_t timestamp;
    uint32_t track_id;
    uint32_t direction;
} event_flow_data_t;

typedef struct flow_info_t
{
    uint64_t event_id;  // id of the event for which the flow info is stored
    std::vector<event_flow_data_t> flow_data;
} flow_info_t;

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

typedef struct call_stack_info_t
{
    uint64_t event_id;  // id of the event for which the call stack data is stored
    std::vector<call_stack_data_t> call_stack_data;  // vector of call stack entries
} call_stack_info_t;

class RequestParamsBase
{
public:
    virtual ~RequestParamsBase() = default;
};

// Track request parameters
class TrackRequestParams : public RequestParamsBase
{
public:
    uint64_t m_track_id;          // id of track that is being requested
    double   m_start_ts;          // start time stamp of data being requested
    double   m_end_ts;            // end time stamp of data being requested
    uint32_t m_horz_pixel_range;  // horizontal pixel range for the request
    uint64_t m_data_group_id;     // group id for the request, used for grouping requests

    TrackRequestParams(const TrackRequestParams& other)            = default;
    TrackRequestParams& operator=(const TrackRequestParams& other) = default;

    TrackRequestParams(uint64_t track_id, double start_ts, double end_ts,
                       uint32_t horz_pixel_range, uint64_t group_id)
    : m_track_id(track_id)
    , m_start_ts(start_ts)
    , m_end_ts(end_ts)
    , m_horz_pixel_range(horz_pixel_range)
    , m_data_group_id(group_id)
    {}
};

// Table request parameters
class TableRequestParams : public RequestParamsBase
{
public:
    rocprofvis_controller_table_type_t m_table_type;  // type of the table
    std::vector<uint64_t>              m_track_ids;   // ids of the tracks in the table
    double   m_start_ts;           // starting time stamp of the data in the table
    double   m_end_ts;             // ending time stamp of the data in the table
    uint64_t m_start_row;          // starting row of the data in the table
    uint64_t m_req_row_count;      // number of rows requested
    uint64_t m_sort_column_index;  // index of the column to sort by
    rocprofvis_controller_sort_order_t m_sort_order;  // sort order of the column
    std::string           m_filter;

    TableRequestParams(const TableRequestParams& table_params)            = default;
    TableRequestParams& operator=(const TableRequestParams& table_params) = default;

    TableRequestParams(
        rocprofvis_controller_table_type_t table_type,
        const std::vector<uint64_t>& track_ids, double start_ts, double end_ts, char const* filter,
        uint64_t start_row = -1, uint64_t req_row_count = -1,
        uint64_t                           sort_column_index = 0,
        rocprofvis_controller_sort_order_t sort_order = kRPVControllerSortOrderAscending)
    : m_table_type(table_type)
    , m_track_ids(track_ids)
    , m_start_ts(start_ts)
    , m_end_ts(end_ts)
    , m_start_row(start_row)
    , m_req_row_count(req_row_count)
    , m_sort_column_index(sort_column_index)
    , m_sort_order(sort_order)
    , m_filter(filter)
    {}
};

typedef struct data_req_info_t
{
    uint64_t                           request_id;      // unique id of the request
    rocprofvis_controller_future_t*    request_future;  // future for the request
    rocprofvis_controller_array_t*     request_array;   // array of data for the request
    rocprofvis_handle_t*               request_obj_handle;  // object for the request
    rocprofvis_controller_arguments_t* request_args;        // arguments for the request
    ProviderState                      loading_state;       // state of the request
    RequestType                        request_type;        // type of request
    std::shared_ptr<RequestParamsBase> custom_params;       // custom request parameters
} data_req_info_t;

typedef struct table_info_t
{
    std::shared_ptr<TableRequestParams>   table_params;
    std::vector<std::string>              table_header;
    std::vector<std::vector<std::string>> table_data;
    uint64_t                              total_row_count;

} table_info_t;

class DataProvider
{
public:
    static constexpr uint64_t EVENT_TABLE_REQUEST_ID  = static_cast<uint64_t>(-1);
    static constexpr uint64_t SAMPLE_TABLE_REQUEST_ID = static_cast<uint64_t>(-2);

    DataProvider();
    ~DataProvider();

    const event_info_t&      GetEventInfoStruct() const;
    const flow_info_t&       GetFlowInfo() const;
    const call_stack_info_t& GetCallStackInfo() const;

    bool FetchEventExtData(uint64_t event_id);
    bool FetchEventFlowDetails(uint64_t event_id);
    bool FetchEventCallStackData(uint64_t event_id);

    // Get user selected event.
    uint64_t GetSelectedEventId();

    // Set user selected event.
    void SetSelectedEvent(selected_event_t event);

    // Get user selected event.
    selected_event_t GetSelectedEvent();

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
    bool FetchTrack(uint64_t track_id, double start_ts, double end_ts,
                    uint32_t horz_pixel_range, uint64_t group_id);

    bool FetchTrack(const TrackRequestParams& request_params);

    bool FetchWholeTrack(uint64_t track_id, double start_ts, double end_ts,
                         uint32_t horz_pixel_range, uint64_t group_id);

    /*
     * Fetches an event table from the controller for a single track.
     * @param id: The id of the track to select
     * @param start_ts: The start timestamp of the event table
     * @param end_ts: The end timestamp of the event table
     * @param start_row: The starting row of the sample table
     * @param req_row_count: The number of rows to request
     * @param sort_column_index: The index of the column to sort by
     * @param sort_order: The sort order of the column
     */
    bool FetchSingleTrackEventTable(uint64_t track_id, double start_ts, double end_ts, char const* filter, uint64_t start_row = -1,
        uint64_t req_row_count = -1, uint64_t sort_column_index = 0,
        rocprofvis_controller_sort_order_t sort_order = kRPVControllerSortOrderAscending);

    /*
     * Fetches a sample table from the controller for a single track.
     * @param id: The id of the track to select
     * @param start_ts: The start timestamp of the sample table
     * @param end_ts: The end timestamp of the sample table
     * @param start_row: The starting row of the sample table
     * @param req_row_count: The number of rows to request
     * @param sort_column_index: The index of the column to sort by
     * @param sort_order: The sort order of the column
     */
    bool FetchSingleTrackSampleTable(uint64_t track_id, double start_ts, double end_ts, char const* filter, uint64_t start_row = -1,
        uint64_t req_row_count = -1, uint64_t sort_column_index = 0,
        rocprofvis_controller_sort_order_t sort_order = kRPVControllerSortOrderAscending);

    /*
     * Fetches a table from the controller for a single track.
     * @param table_params: The parameters for the table request
     */
    bool FetchSingleTrackTable(const TableRequestParams& table_params);

    bool FetchMultiTrackSampleTable(const std::vector<uint64_t>& track_ids, double start_ts, double end_ts, char const* filter,
        uint64_t start_row = -1, uint64_t req_row_count = -1,
        uint64_t                           sort_column_index = 0,
        rocprofvis_controller_sort_order_t sort_order = kRPVControllerSortOrderAscending);

    bool FetchMultiTrackEventTable(const std::vector<uint64_t>& track_ids, double start_ts, double end_ts, char const* filter,
        uint64_t start_row = -1, uint64_t req_row_count = -1,
        uint64_t                           sort_column_index = 0,
        rocprofvis_controller_sort_order_t sort_order = kRPVControllerSortOrderAscending);

    bool FetchMultiTrackTable(const TableRequestParams& table_params);

    bool IsRequestPending(uint64_t request_id);

    /*
     * Release memory buffer holding raw data for selected track
     * @param id: The id of the track to select
     */
    bool FreeTrack(uint64_t track_id);

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

    uint64_t GetTrackCount();

    const std::string& GetTraceFilePath();

    ProviderState GetState();

    const std::vector<std::string>&              GetTableHeader(TableType type);
    const std::vector<std::vector<std::string>>& GetTableData(TableType type);
    std::shared_ptr<TableRequestParams>          GetTableParams(TableType type);
    uint64_t                                     GetTableTotalRowCount(TableType type);
    void                                         ClearTable(TableType type);

    void SetTrackDataReadyCallback(
        const std::function<void(uint64_t, const std::string&)>& callback);
    void SetTraceLoadedCallback(const std::function<void(const std::string&)>& callback);

    /*
     * Moves a graph inside the controller's timeline to a specified index and updates the
     * indexes of m_track_metadata.
     * @param track_id: The id of the track to move
     * @param index: The desired index of the track.
     * @return: True if operation is successful.
     */
    bool SetGraphIndex(uint64_t track_id, uint64_t index);

private:
    void HandleLoadTrace();
    void HandleLoadTrackMetaData();
    void HandleRequests();

    void ProcessRequest(data_req_info_t& req);
    void ProcessGraphRequest(data_req_info_t& req);
    void ProcessTrackRequest(data_req_info_t& req);
    void ProcessTableRequest(data_req_info_t& req);

    bool SetupCommonTableArguments(rocprofvis_controller_arguments_t* args,
                                   const TableRequestParams&          table_params);


    void CreateRawEventData(const TrackRequestParams &params, rocprofvis_controller_array_t* track_data);                           
    void CreateRawSampleData(const TrackRequestParams &params, rocprofvis_controller_array_t* track_data);

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

    selected_event_t  m_selected_event;   // Store selected event info
    event_info_t      m_event_info;       // Store event info for selected event
    flow_info_t       m_flow_info;        // Store flow info for selected event
    call_stack_info_t m_call_stack_info;  // Store call stack info for selected event

    std::unordered_map<uint64_t, track_info_t>  m_track_metadata;
    std::unordered_map<uint64_t, RawTrackData*> m_raw_trackdata;

    // Store table_info_t for each TableType in a vector
    std::vector<table_info_t> m_table_infos;

    std::unordered_map<int64_t, data_req_info_t> m_requests;

    // Called when new track data is ready
    std::function<void(uint64_t, const std::string&)> m_track_data_ready_callback;
    // Called when a new trace is loaded
    std::function<void(const std::string&)> m_trace_data_ready_callback;
};

}  // namespace View
}  // namespace RocProfVis
