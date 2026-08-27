// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_controller_enums.h"
#include "rocprofvis_controller_types.h"
#include "rocprofvis_raw_track_data.h"
#include "rocprofvis_requests.h"
#include "model/rocprofvis_trace_data_model.h"
#include "model/compute/rocprofvis_compute_data_model.h"


#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
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

struct DataProviderCleanupWork
{
    std::string                           trace_file_path;
    std::unordered_map<int64_t, RequestInfo> requests;
    rocprofvis_controller_t*              controller = nullptr;
    // Privately allocated tables travel with the controller they were read
    // through, because they must be freed before it is.
    std::vector<rocprofvis_handle_t*>     client_tables;
};

struct DataProviderCleanupResult
{
    std::string trace_file_path;
    size_t      request_count = 0;
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
    static const uint64_t CLEANUP_DATABASE_REQUEST_ID;
    static const uint64_t TABLE_EXPORT_REQUEST_ID;
    static const uint64_t FETCH_SYSTEM_TRACE_REQUEST_ID;
    static const uint64_t SUMMARY_REQUEST_ID;
    static const uint64_t SUMMARY_KERNEL_INSTANCE_TABLE_REQUEST_ID;
    static const uint64_t ANALYSIS_TOP_INSTRUMENTED_EVENTS_TABLE_REQUEST_ID;
    static const uint64_t ANALYSIS_TOP_DISPATCH_EVENTS_TABLE_REQUEST_ID;
    static const uint64_t ANALYSIS_TOP_MEMORY_ALLOCATION_EVENTS_TABLE_REQUEST_ID;
    static const uint64_t ANALYSIS_TOP_MEMORY_COPY_EVENTS_TABLE_REQUEST_ID;
    static const uint64_t ANALYSIS_TOP_LAUNCH_SAMPLED_TABLE_REQUEST_ID;
    static const uint64_t FETCH_COMPUTE_TRACE_REQUEST_ID;
    static const uint64_t METRIC_PIVOT_TABLE_REQUEST_ID;

    // Ask Optiq's table reads. A background reader sharing the ids above would
    // be refused whenever a tab happened to be loading, and - worse - would
    // overwrite the rows that tab is showing once it was not. Its own client id
    // gives it its own request ids, its own controller tables, and its own
    // model slots, so the two never meet. Anything else that reads tables
    // without being a tab wants the same treatment and a client id of its own.
    static constexpr uint64_t ASSISTANT_CLIENT_ID = 1;
    static const uint64_t ASSISTANT_EVENT_TABLE_REQUEST_ID;
    static const uint64_t ASSISTANT_SAMPLE_TABLE_REQUEST_ID;
    static const uint64_t ASSISTANT_EVENT_SEARCH_REQUEST_ID;
    static const uint64_t ASSISTANT_SUMMARY_KERNEL_INSTANCE_TABLE_REQUEST_ID;
    static const uint64_t ASSISTANT_TOP_EVENTS_TABLE_REQUEST_ID;
#ifdef ROCPROFVIS_ENABLE_SCRIPTING
    static const uint64_t EXECUTE_SCRIPT_REQUEST_ID;
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
     *   Cancel, wait for, and free outstanding requests on the calling thread.
     *   Use DetachCleanupWork() when the caller needs asynchronous cleanup.
     */
    void FreeRequests();

    /*
     *   Get the number of pending requests.
     */
    size_t GetPendingRequestCount() const;

    DataProviderCleanupWork DetachCleanupWork();
    static DataProviderCleanupResult CleanupDetachedResources(
        DataProviderCleanupWork cleanup_work);

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

    bool FetchAnalysisTrackStatistics(const AnalysisTrackStatisticsRequestParams& params);

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

    // Sets each track's TrackInfo::index from its position in ordered_track_ids (a
    // full permutation) and fires the metadata-changed callback. Does not touch the
    // controller's graph order.
    bool SetTrackIndex(const std::vector<uint64_t>& ordered_track_ids);

    bool SaveTrimmedTrace(const std::string& path, double start_ns, double end_ns);

#ifdef ROCPROFVIS_ENABLE_SCRIPTING
    // Runs source on the interpreter thread. track_ids empty means all
    // tracks; start/end are the visible or selected time range.
    bool ExecuteScript(const std::string& source, const std::vector<uint64_t>& track_ids,
                       double start_ts, double end_ts);
    bool CancelScript();

    // What the last script produced, kept after its request is gone: a caller
    // that polls the request id rather than listening for the completion event
    // has nothing left to read by the time it notices the request finished.
    // False when no script has run, or the script failed.
    bool GetLastScriptResult(std::string& text_out, std::string& error_out) const;
#endif

    bool CleanupDatabase(bool rebuild);

    void SetCleanupDatabaseCallback(const std::function<void(bool)>& callback);

    const TraceDataModel& DataModel() const { return m_model; };
    TraceDataModel&       DataModel() { return m_model; };

    ComputeDataModel& ComputeModel();

    bool FetchMetrics(const MetricsRequestParams& metrics_params);
    bool FetchMetricPivotTable(const ComputeTableRequestParams& params);
    bool FetchPcSampling(const PcSamplingRequestParams& params);

    void SetFetchMetricsCallback(
        const std::function<void(const std::string&, uint64_t, bool)>& callback);
    void SetFetchPcSamplingCallback(
        const std::function<void(const std::string&, uint32_t, uint32_t, uint32_t, bool)>& callback);

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

    bool FetchTrackTable(const TrackTableRequestParams& table_params);
    bool FetchEventSearch(const EventSearchRequestParams& table_params);

    /*
     * The request id, table handle and model slot a table fetch should use.
     * For the UI (client 0) these are the shared ones; for any other client
     * they are private, so the two can be in flight at once without either
     * reading the other's rows.
     */
    uint64_t             ClientTableRequestId(const TableRequestParams& table_params) const;
    rocprofvis_handle_t* ClientTableHandle(const TableRequestParams& table_params);
    static TableType     ClientTableSlot(rocprofvis_controller_table_type_t table_type,
                                         uint64_t                          client_id,
                                         bool&                             is_analysis_model);
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
    // Reorders the timeline so compared traces' counterpart tracks (A, B, ...) sit
    // adjacent, using the data model's track order ranking. No-op for non-compare traces.
    void ApplyTrackOrderRanking();
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
#ifdef ROCPROFVIS_ENABLE_SCRIPTING
    void ProcessExecuteScriptRequest(RequestInfo& req);
    rocprofvis_controller_arguments_t* BuildScriptContext(
        const std::vector<uint64_t>& track_ids, double start_ts, double end_ts);
#endif
    void ProcessCleanupDatabaseRequest(RequestInfo& req);
    void ProcessSummaryRequest(RequestInfo& req);
    void ProcessAnalysisTrackStatisticsRequest(RequestInfo& req);

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
    // Callback when database cleanup has completed
    std::function<void(bool)> m_cleanup_database_callback;
    // Callback when table export has completed
    std::function<void(const std::string&, bool)> m_table_export_callback;
    // Callback to update request progress
    std::function<void(const RequestInfo&, uint64_t, const std::string&)>
        m_request_progress_callback;
    // Current loading status message retrieved form data model
    std::string m_progress_mesage;
    // Current loading status progress in percents
    uint64_t m_progress_percent;
    // Tables allocated for a non-UI client, keyed by (client id, controller
    // table type, operation). Built on first use and kept for the life of the
    // controller, since a client asks the same few questions repeatedly and
    // reallocating per fetch would throw away the row cache each time.
    std::map<std::tuple<uint64_t, uint64_t, uint64_t>, rocprofvis_handle_t*>
        m_client_tables;
#ifdef ROCPROFVIS_ENABLE_SCRIPTING
    // Outlives the request it came from. See GetLastScriptResult.
    std::string m_script_result_text;
    std::string m_script_result_error;
    bool        m_script_result_ok = false;
#endif

    void ProcessLoadComputeTrace(RequestInfo& req);
    inline void LoadWorkload(uint64_t workload_index);
    inline void LoadSystemInfo(WorkloadInfo&        workload,
                               rocprofvis_handle_t* workload_handle);
    inline void LoadProfilingConfig(WorkloadInfo&        workload,
                                    rocprofvis_handle_t* workload_handle);
    inline void LoadMetricList(WorkloadInfo&        workload,
                                    rocprofvis_handle_t* workload_handle);
    inline void LoadValueNames(WorkloadInfo&        workload,
                               rocprofvis_handle_t* workload_handle);
    inline void LoadKernels(WorkloadInfo&        workload,
                               rocprofvis_handle_t* workload_handle);
    inline void LoadPcSamplingCodeObjects(KernelInfo&          kernel,
                                          rocprofvis_handle_t* pc_handle);
    inline void LoadPcSamplingSourceFiles(KernelInfo&          kernel,
                                          rocprofvis_handle_t* pc_handle);
    inline void LoadPcSamplingIsaLine(IsaLine&             isa_line,
                                      rocprofvis_handle_t* pc_handle,
                                      uint64_t             index);
    inline void LoadPcSamplingSourceLine(SourceLine&          source_line,
                                         rocprofvis_handle_t* pc_handle,
                                         uint64_t             index);
    inline void LoadPcSamplingJunctions(KernelInfo&          kernel,
                                        rocprofvis_handle_t* pc_handle);
    inline void LoadPcSamplingStates(KernelInfo&          kernel,
                                           rocprofvis_handle_t* pc_handle);
    inline void LoadPcSamplingStallReasonCounts(KernelInfo&          kernel,
                                                rocprofvis_handle_t* pc_handle);
    inline void LoadRoofLine(WorkloadInfo& workload, rocprofvis_handle_t* workload_handle);

    using compute_ridge_map = std::unordered_map<
        rocprofvis_controller_roofline_ceiling_compute_type_t,
        std::unordered_map<rocprofvis_controller_roofline_ceiling_bandwidth_type_t,
                           Point>>;

    using bandwidth_ridge_map = std::unordered_map<
        rocprofvis_controller_roofline_ceiling_bandwidth_type_t,
        std::unordered_map<rocprofvis_controller_roofline_ceiling_compute_type_t, Point>>;


    inline void LoadRoofLineCeilingsRidge(WorkloadInfo&        workload,
                                          rocprofvis_handle_t* roofline_handle,
                                          compute_ridge_map&   compute_ridge,
                                          bandwidth_ridge_map& bandwidth_ridge);

    inline void LoadRoofLineCeilingsCompute(WorkloadInfo&        workload,
                                            rocprofvis_handle_t* roofline_handle,
                                            compute_ridge_map&   compute_ridge);

    inline void LoadRoofLineCeilingsBandwidth(WorkloadInfo&        workload,
                                              rocprofvis_handle_t* roofline_handle,
                                              bandwidth_ridge_map& bandwidth_ridge);

    inline void LoadRoofLineKernels(WorkloadInfo&        workload,
                                    rocprofvis_handle_t* roofline_handle);

    void ProcessMetricsRequest(RequestInfo& req);
    void ProcessMetricPivotTable(RequestInfo& req);
    void ProcessPcSamplingRequest(RequestInfo& req);

    ComputeDataModel m_compute_model;

    // Code View permits one PC sampling request per trace. Completed data is
    // accepted only when it belongs to the latest submitted selection.
    uint32_t m_pc_sampling_generation = 0;

    std::function<void(const std::string&, uint64_t, bool)> m_metrics_fetch_callback;
    std::function<void(const std::string&, uint32_t, uint32_t, uint32_t, bool)>
        m_pc_sampling_fetch_callback;
};

}  // namespace View
}  // namespace RocProfVis
