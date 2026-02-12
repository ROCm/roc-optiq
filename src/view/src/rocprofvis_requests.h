// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "rocprofvis_controller_enums.h"
#include "rocprofvis_controller_types.h"
#include "rocprofvis_c_interface_types.h"

namespace RocProfVis
{
namespace View
{

enum class RequestType
{
    kFetchTrack,
    kFetchGraph,
    kFetchTrackEventTable,
    kFetchTrackSampleTable,
    kFetchEventSearchTable,
    kFetchSummaryKernelInstanceTable,
    kFetchEventExtendedData,
    kFetchEventFlowDetails,
    kFetchEventCallStack,
    kFetchSummary,
    kSaveTrimmedTrace,
    kTableExport,
    kFetchSystemTrace,
#ifdef COMPUTE_UI_SUPPORT
    kFetchComputeTrace,
    kFetchMetrics,
#endif
};

enum class RequestState
{
    kInit,
    kLoading,
    kReady,
    kError
};

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
    std::string                        m_where;       // SQL where clause
    std::string                        m_filter;      // CMD filter
    std::vector<std::string>
                m_string_table_filters;  // strings to use for string table filtering.
    std::string m_group;
    std::string m_group_columns;
    std::string m_export_to_file_path;
    bool        m_summary;

    TableRequestParams(const TableRequestParams& table_params)            = default;
    TableRequestParams& operator=(const TableRequestParams& table_params) = default;

    TableRequestParams(
        rocprofvis_controller_table_type_t                  table_type,
        const std::vector<uint64_t>&                        track_ids,
        const std::vector<rocprofvis_dm_event_operation_t>& op_types, double start_ts,
        double end_ts, const char* where, char const* filter, char const* group,
        char const* group_cols, const std::vector<std::string> string_table_filters = {},
        uint64_t start_row = -1, uint64_t req_row_count = -1,
        uint64_t                           sort_column_index = 0,
        rocprofvis_controller_sort_order_t sort_order = kRPVControllerSortOrderAscending,
        std::string export_to_file_path = "", bool summary = false)
    : m_table_type(table_type)
    , m_track_ids(track_ids)
    , m_op_types(op_types)
    , m_start_ts(start_ts)
    , m_end_ts(end_ts)
    , m_start_row(start_row)
    , m_req_row_count(req_row_count)
    , m_sort_column_index(sort_column_index)
    , m_sort_order(sort_order)
    , m_where(where)
    , m_filter(filter)
    , m_group(group)
    , m_group_columns(group_cols)
    , m_string_table_filters(string_table_filters)
    , m_export_to_file_path(export_to_file_path)
    , m_summary(summary)
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

#ifdef COMPUTE_UI_SUPPORT
class MetricsRequestParams : public RequestParamsBase
{
public:
    struct MetricID
    {
        uint32_t                category_id;
        std::optional<uint32_t> table_id;
        std::optional<uint32_t> entry_id;
    };
    uint32_t              m_workload_id;
    std::vector<uint32_t> m_kernel_ids;
    std::vector<MetricID> m_metric_ids;

    MetricsRequestParams(const MetricsRequestParams& metrics_params)            = default;
    MetricsRequestParams& operator=(const MetricsRequestParams& metrics_params) = default;

    MetricsRequestParams(uint32_t workload_id, const std::vector<uint32_t>& kernel_ids,
                         const std::vector<MetricID>& metric_ids)
    : m_workload_id(workload_id)
    , m_kernel_ids(kernel_ids)
    , m_metric_ids(metric_ids)
    {}
};
#endif

struct RequestInfo
{
    uint64_t                              request_id;      // unique id of the request
    rocprofvis_controller_future_t*       request_future;  // future for the request
    rocprofvis_controller_array_t*        request_array;  // array of data for the request
    rocprofvis_handle_t*                  request_obj_handle;  // object for the request
    rocprofvis_controller_arguments_t*    request_args;   // arguments for the request
    RequestState                          loading_state;  // state of the request
    RequestType                           request_type;   // type of request
    std::shared_ptr<RequestParamsBase>    custom_params;  // custom request parameters
    std::chrono::steady_clock::time_point request_time;  // time when the request was made
    uint64_t                              response_code;  // response code for the request
};

}  // namespace View
}  // namespace RocProfVis
