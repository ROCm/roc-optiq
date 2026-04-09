// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_controller_enums.h"
#include "rocprofvis_controller_types.h"
#include "rocprofvis_raw_track_data.h"
#include "rocprofvis_requests.h"
#include "model/rocprofvis_trace_data_model.h"
#ifdef COMPUTE_UI_SUPPORT
#include "model/compute/rocprofvis_compute_data_model.h"
#endif


#include <functional>
#include <memory>
#include <optional>
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

class DataProvider
{
public:
    static const uint64_t EVENT_TABLE_REQUEST_ID;
    static const uint64_t SAMPLE_TABLE_REQUEST_ID;
    static const uint64_t EVENT_SEARCH_REQUEST_ID;
    static const uint64_t EVENT_EXTENDED_DATA_REQUEST_ID;
    static const uint64_t EVENT_FLOW_DATA_REQUEST_ID;
    static const uint64_t EVENT_CALL_STACK_DATA_REQUEST_ID;
    static const uint64_t SAVE_TRIMMED_TRACE_REQUEST_ID;
    static const uint64_t TABLE_EXPORT_REQUEST_ID;
    static const uint64_t FETCH_SYSTEM_TRACE_REQUEST_ID;
    static const uint64_t SUMMARY_REQUEST_ID;
    static const uint64_t SUMMARY_KERNEL_INSTANCE_TABLE_REQUEST_ID;
#ifdef COMPUTE_UI_SUPPORT
    static const uint64_t FETCH_COMPUTE_TRACE_REQUEST_ID;
    static const uint64_t METRIC_PIVOT_TABLE_REQUEST_ID;
#endif

    DataProvider();
    ~DataProvider();

    /*
     * Fetches an event of a track from the controller. Stores the data in m_event_data.
     * @param track_id: ID of the track that owns the event to fetch.
     * @param event_id: ID of event to fetch
     */
    bool FetchEvent(uint64_t track_id, uint64_t event_id);
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
     * Loads the trace data into the controller.
     * Any previous data will be cleared.
     * @param controller: The handle of the controller.
     * @param file_path: The path to the trace file to passed to the trace data model.
     */
    bool FetchTrace(rocprofvis_controller_t* controller, const std::string& file_path);

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

    bool FetchSummary();

    bool IsRequestPending(uint64_t request_id) const;

    /* Cancels a pending request.
     * @param request_id: The id of the request to cancel.
     * @return: True if the cancel operation was accepted.
     */
    bool CancelRequest(uint64_t request_id);

    /*
     * Performs all data processing.  Call this from the "game loop".
     */
    void Update();

    const std::string& GetTraceFilePath();

    ProviderState GetState();

    const char* GetProgressMessage();

    void SetTrackMetadataChangedCallback(
        const std::function<void(const std::string&)>& callback);
    void SetTableDataReadyCallback(
        const std::function<void(const std::string&, uint64_t, uint64_t)>& callback);
    void SetTrackDataReadyCallback(
        const std::function<void(uint64_t, const std::string&, const RequestInfo&)>&
            callback);
    void SetSummaryDataReadyCallback(const std::function<void()>& callback);
    void SetTraceLoadedCallback(
        const std::function<void(const std::string&, uint64_t)>& callback);
    void SetSaveTraceCallback(const std::function<void(bool)>& callback);
    void SetExportTableCallback(
        const std::function<void(const std::string&, bool)>& callback);
    void SetEventDataReadyCallback(
        const std::function<void(uint64_t, const std::string&, bool)>& callback);
    void SetRequestProgressUpdateCallback(
        const std::function<void(const RequestInfo&, uint64_t, const std::string&)>&
            callback);

    /*
     * Moves a graph inside the controller's timeline to a specified index and updates the
     * indexes of m_track_metadata.
     * @param track_id: The id of the track to move
     * @param index: The desired index of the track.
     * @return: True if operation is successful.
     */
    bool SetGraphIndex(uint64_t track_id, uint64_t index);

    bool SaveTrimmedTrace(const std::string& path, double start_ns, double end_ns);

    const TraceDataModel& DataModel() const { return m_model; };
    TraceDataModel&       DataModel() { return m_model; };

    /*
     * ==================================================================================
     * Profiler Launcher Methods
     * ==================================================================================
     */

    /*
     * Launches a profiler process asynchronously.
     * @param profiler_type: Type of profiler to launch
     * @param profiler_path: Path to the profiler executable
     * @param target_executable: Path to the target application to profile
     * @param target_args: Arguments to pass to the target application
     * @param output_directory: Directory where profiler output should be saved
     * @param profiler_args: Arguments to pass to the profiler
     * @return: True if launch was initiated successfully
     */
    bool LaunchProfiler(rocprofvis_profiler_type_t profiler_type,
                        const std::string& profiler_path,
                        const std::string& target_executable,
                        const std::string& target_args,
                        const std::string& output_directory,
                        const std::string& profiler_args);

    /*
     * Gets the current state of the profiler execution.
     * @return: Current profiler state
     */
    rocprofvis_profiler_state_t GetProfilerState() const;

    /*
     * Gets the accumulated profiler output (stdout/stderr).
     * @return: Profiler output text
     */
    std::string GetProfilerOutput();

    /*
     * Gets the path to the generated trace file (only valid when state = Completed).
     * @return: Path to trace file, or empty string if not available
     */
    std::string GetProfilerTracePath();

    /*
     * Gets the exit code of the profiler process (valid after completion/failure).
     * @return: Process exit code, or -1 if not available
     */
    int32_t GetProfilerExitCode() const;

    /*
     * Cancels the running profiler process.
     * @return: True if cancellation was successful
     */
    bool CancelProfiler();

    /*
     * Closes the profiler and frees associated resources.
     */
    void CloseProfiler();

private:
    struct ProcessChildCount
    {
        size_t thread_count;
        size_t stream_count;
    };

    struct ProcessorChildCount
    {
        size_t queue_count;
        size_t counter_count;
    };

    /* Helper called by FetchEvent()*/
    bool FetchEventExtData(uint64_t event_id);
    void HandleLoadSystemTopology();
    bool ParseNodeData(rocprofvis_handle_t* node_handle, NodeInfo& node_info);
    bool ParseDeviceData(rocprofvis_handle_t* processor_handle, DeviceInfo& device_info,
                          DataProvider::ProcessorChildCount& processor_child_count);
    bool ParseProcessData(rocprofvis_handle_t* process_handle, ProcessInfo& process_info,
                          ProcessChildCount& process_child_count);
    bool ParseQueueData(rocprofvis_handle_t* queue_handle, QueueInfo& queue_info);
    bool ParseThreadData(rocprofvis_handle_t* thread_handle, ThreadInfo& thread_info,
                         uint64_t& thread_type);
    bool ParseCounterData(rocprofvis_handle_t* counter_handle, CounterInfo& counter_info);
    bool ParseStreamData(rocprofvis_handle_t* stream_handle, StreamInfo& stream_info);

    void HandleLoadTrackMetaData();
    void HandleRequests();
    void UpdateRequestProgress(RequestInfo& req);

    void ProcessRequest(RequestInfo& req);
    void ProcessLoadSystemTrace(RequestInfo& req);
    void ProcessEventExtendedRequest(RequestInfo& req);
    void ProcessEventFlowDetailsRequest(RequestInfo& req);
    void ProcessEventCallStackRequest(RequestInfo& req);
    void ProcessGraphRequest(RequestInfo& req);
    void ProcessTrackRequest(RequestInfo& req);
    void ProcessTableRequest(RequestInfo& req);
    void ProcessTableExportRequest(RequestInfo& req);
    void ProcessSaveTrimmedTraceRequest(RequestInfo& req);
    void ProcessSummaryRequest(RequestInfo& req);

    bool SetupCommonTableArguments(rocprofvis_controller_arguments_t* args,
                                   const TableRequestParams&          table_params);

    void CreateRawEventData(const TrackRequestParams& params, const RequestInfo& req);
    void CreateRawSampleData(const TrackRequestParams& params,
                             const RequestInfo&    req);
    void CreateSummaryData(rocprofvis_handle_t* metrics_handle,
                           std::vector<size_t>& sub_metrics_idx);

    rocprofvis_result_t GetString(rocprofvis_handle_t*  handle,
                                  rocprofvis_property_t property, uint64_t index,
                                  std::string& out_string);

    std::string GetString(rocprofvis_handle_t* handle, rocprofvis_property_t property,
                          uint64_t index);

    rocprofvis_controller_t*          m_trace_controller;
    rocprofvis_controller_timeline_t* m_trace_timeline;

    ProviderState m_state;

    // Data model aggregating all trace data
    TraceDataModel m_model;

    std::unordered_map<int64_t, RequestInfo> m_requests;
    // Called when track metadata has changed
    std::function<void(const std::string&)> m_track_metadata_changed_callback;
    // Called when table data has changed
    std::function<void(const std::string&, uint64_t, uint64_t)> m_table_data_ready_callback;
    // Called when new track data is ready
    std::function<void(uint64_t, const std::string&, const RequestInfo&)>
        m_track_data_ready_callback;
    // Called when a new trace is loaded
    std::function<void(const std::string&, uint64_t)> m_trace_data_ready_callback;
    // called when event data is ready
    std::function<void(uint64_t, const std::string&, bool)>
        m_event_data_ready_callback;

    // Called when summary data has changed
    std::function<void()> m_summary_data_ready_callback;
    // Callback when trace is saved
    std::function<void(bool)> m_save_trace_callback;
    // Callback when table export has completed
    std::function<void(const std::string&, bool)> m_table_export_callback;
    // Callback to update request progress
    std::function<void(const RequestInfo&, uint64_t, const std::string&)>
        m_request_progress_callback;
    // Current loading status message retrieved form data model
    std::string m_progress_mesage;
    // Current loading status progress in percents
    uint64_t m_progress_percent;

    // Profiler launcher state
    rocprofvis_profiler_config_t* m_profiler_config;
    rocprofvis_controller_future_t* m_profiler_future;

#ifdef COMPUTE_UI_SUPPORT
public:
    ComputeDataModel& ComputeModel();
    
    bool FetchMetrics(const MetricsRequestParams& metrics_params);
    bool FetchMetricPivotTable(const ComputeTableRequestParams& params);

    void SetFetchMetricsCallback(
        const std::function<void(const std::string&, uint64_t, bool)>& callback);

private:
    void ProcessLoadComputeTrace(RequestInfo& req);
    void ProcessMetricsRequest(RequestInfo& req);
    void ProcessMetricPivotTable(RequestInfo& req);

    ComputeDataModel m_compute_model;

    std::function<void(const std::string&, uint64_t, bool)> m_metrics_fetch_callback;

#endif
};

}  // namespace View
}  // namespace RocProfVis
