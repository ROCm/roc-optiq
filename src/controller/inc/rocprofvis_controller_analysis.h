// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_controller.h"

#ifdef __cplusplus
extern "C"
{
#endif

/*
 * Available summary areas.
 */
typedef enum rocprofvis_analysis_summary_category_t : uint32_t
{
    __kRPVAnalysisSummaryCategoryFirst,
    kRPVAnalysisSummaryCategoryKernel = __kRPVAnalysisSummaryCategoryFirst,
    kRPVAnalysisSummaryCategoryMemory,
    kRPVAnalysisSummaryCategoryThread,
    __kRPVAnalysisSummaryCategoryLast
} rocprofvis_analysis_summary_category_t;

/*
 * Available summary metrics for a family of events (ex. kernel dispatches, memory operations, or thread operations...etc).
 */
typedef enum rocprofvis_analysis_summary_top_events_metric_t : uint32_t
{
    __kRPVAnalysisSummaryTopEventsMetricFirst,
    kRPVAnalysisSummaryTopEventsMetricNumTopEvents = __kRPVAnalysisSummaryTopEventsMetricFirst, // Number of top events.
    kRPVAnalysisSummaryTopEventsMetricInvocations, // Total number of invocations for all events in the set.
    kRPVAnalysisSummaryTopEventsMetricDurationSum, // Total duration for all events in the set.
    __kRPVAnalysisSummaryTopEventsMetricLast
} rocprofvis_analysis_summary_top_events_metric_t;

/*
 * Available summary metrics for a single top event.
 */
typedef enum rocprofvis_analysis_summary_top_event_metric_t : uint32_t
{
    __kRPVAnalysisSummaryTopEventMetricFirst,
    kRPVAnalysisSummaryTopEventMetricInvocations = __kRPVAnalysisSummaryTopEventMetricFirst,
    kRPVAnalysisSummaryTopEventMetricDurationSum,
    kRPVAnalysisSummaryTopEventMetricDurationMin,
    kRPVAnalysisSummaryTopEventMetricDurationMax,
    kRPVAnalysisSummaryTopEventMetricDurationPct,
    kRPVAnalysisSummaryTopEventMetricDurationAvg,
    __kRPVAnalysisSummaryTopEventMetricLast
} rocprofvis_analysis_summary_top_event_metric_t;

/*
 * Available summary metrics for a counter.
 */
typedef enum rocprofvis_analysis_summary_counter_metric_t : uint32_t
{
    __kRPVAnalysisSummaryCounterMetricFirst,
    kRPVAnalysisSummaryCounterMetricMin = __kRPVAnalysisSummaryCounterMetricFirst,
    kRPVAnalysisSummaryCounterMetricMax,
    kRPVAnalysisSummaryCounterMetricMean,
    kRPVAnalysisSummaryCounterMetricStdDev,
    __kRPVAnalysisSummaryCounterMetricLast
} rocprofvis_analysis_summary_counter_metric_t;

/*
 * Available summary aggregation levels.
 */
typedef enum rocprofvis_analysis_summary_aggregation_level_t : uint32_t
{
    __kRPVAnalysisSummaryAggregationLevelFirst,
    kRPVAnalysisSummaryAggregationLevelTrace = __kRPVAnalysisSummaryAggregationLevelFirst,
    kRPVAnalysisSummaryAggregationLevelNode,
    kRPVAnalysisSummaryAggregationLevelProcessor,
    kRPVAnalysisSummaryAggregationLevelProcess,
    kRPVAnalysisSummaryAggregationLevelThread,
    __kRPVAnalysisSummaryAggregationLevelLast
} rocprofvis_analysis_summary_aggregation_level_t;

/*
 * Properties common to all summary aggregation levels.
 */
typedef enum rocprofvis_analysis_summary_metric_properties_t : uint32_t
{
    __kRPVAnalysisSummaryMetricPropertiesFirst = 0xF0000000,
    // Aggregation level (rocprofvis_analysis_summary_aggregation_level_t). Determines which of the property blocks below carry valid data for this container
    kRPVAnalysisSummaryMetricPropertyAggregationLevel = __kRPVAnalysisSummaryMetricPropertiesFirst,
    // Number of child/sub-metrics
    kRPVAnalysisSummaryMetricPropertyNumSubMetrics,
    // Indexed child/sub-metrics objects
    kRPVAnalysisSummaryMetricPropertySubMetricsIndexed,
    // Level specific topology object (trace, node, processor, process, thread)
    kRPVAnalysisSummaryMetricPropertyMetadata,
    __kRPVAnalysisSummaryMetricPropertiesLast
} rocprofvis_analysis_summary_metric_properties_t;

/*
 * Properties specific to the trace and node aggregation levels.
 */
typedef enum rocprofvis_analysis_summary_trace_node_property_t : uint32_t
{
    __kRPVAnalysisSummaryTraceNodePropertiesFirst = 0xF0100000,
    kRPVAnalysisSummaryTraceNodePropertyMemoryEventsInvocationsTotal = __kRPVAnalysisSummaryTraceNodePropertiesFirst,
    kRPVAnalysisSummaryTraceNodePropertyMemoryEventsDurationTotal,
    kRPVAnalysisSummaryTraceNodePropertyNumTopMemoryEvents,
    kRPVAnalysisSummaryTraceNodePropertyTopMemoryEventNameIndexed,
    kRPVAnalysisSummaryTraceNodePropertyTopMemoryEventInvocationsIndexed,
    kRPVAnalysisSummaryTraceNodePropertyTopMemoryEventDurationSumIndexed,
    kRPVAnalysisSummaryTraceNodePropertyTopMemoryEventDurationMinIndexed,
    kRPVAnalysisSummaryTraceNodePropertyTopMemoryEventDurationMaxIndexed,
    kRPVAnalysisSummaryTraceNodePropertyTopMemoryEventDurationPctIndexed,
    kRPVAnalysisSummaryTraceNodePropertyTopMemoryEventDurationAvgIndexed,
    __kRPVAnalysisSummaryTraceNodePropertiesLast
} rocprofvis_analysis_summary_trace_node_property_t;

/*
 * Properties for a GPU at the trace, node, and processor aggregation levels.
 */
typedef enum rocprofvis_analysis_summary_gpu_property_t : uint32_t
{
    __kRPVAnalysisSummaryGpuPropertiesFirst = 0xF0300000,
    kRPVAnalysisSummaryGpuPropertyDispatchEventsInvocationsTotal = __kRPVAnalysisSummaryGpuPropertiesFirst,
    kRPVAnalysisSummaryGpuPropertyDispatchEventsDurationTotal,
    kRPVAnalysisSummaryGpuPropertyNumTopDispatchEvents,
    kRPVAnalysisSummaryGpuPropertyTopDispatchEventNameIndexed,
    kRPVAnalysisSummaryGpuPropertyTopDispatchEventInvocationsIndexed,
    kRPVAnalysisSummaryGpuPropertyTopDispatchEventDurationSumIndexed,
    kRPVAnalysisSummaryGpuPropertyTopDispatchEventDurationMinIndexed,
    kRPVAnalysisSummaryGpuPropertyTopDispatchEventDurationMaxIndexed,
    kRPVAnalysisSummaryGpuPropertyTopDispatchEventDurationPctIndexed,
    kRPVAnalysisSummaryGpuPropertyTopDispatchEventDurationAvgIndexed,
    kRPVAnalysisSummaryGpuPropertyMemoryEventsInvocationsTotal,
    kRPVAnalysisSummaryGpuPropertyMemoryEventsDurationTotal,
    kRPVAnalysisSummaryGpuPropertyNumTopMemoryEvents,
    kRPVAnalysisSummaryGpuPropertyTopMemoryEventNameIndexed,
    kRPVAnalysisSummaryGpuPropertyTopMemoryEventInvocationsIndexed,
    kRPVAnalysisSummaryGpuPropertyTopMemoryEventDurationSumIndexed,
    kRPVAnalysisSummaryGpuPropertyTopMemoryEventDurationMinIndexed,
    kRPVAnalysisSummaryGpuPropertyTopMemoryEventDurationMaxIndexed,
    kRPVAnalysisSummaryGpuPropertyTopMemoryEventDurationPctIndexed,
    kRPVAnalysisSummaryGpuPropertyTopMemoryEventDurationAvgIndexed,
    kRPVAnalysisSummaryGpuPropertyNumCounters,
    kRPVAnalysisSummaryGpuPropertyCounterIndexed,
    kRPVAnalysisSummaryGpuPropertyCounterMinIndexed,
    kRPVAnalysisSummaryGpuPropertyCounterMaxIndexed,
    kRPVAnalysisSummaryGpuPropertyCounterMeanIndexed,
    kRPVAnalysisSummaryGpuPropertyCounterStdDevIndexed,
    __kRPVAnalysisSummaryGpuPropertiesLast
} rocprofvis_analysis_summary_gpu_property_t;

/*
 * Properties for a CPU at the trace, node, and processor aggregation levels.
 */
typedef enum rocprofvis_analysis_summary_cpu_property_t : uint32_t
{
    __kRPVAnalysisSummaryCpuPropertiesFirst = 0xF0400000,
    kRPVAnalysisSummaryCpuPropertyMemoryEventsInvocationsTotal = __kRPVAnalysisSummaryCpuPropertiesFirst,
    kRPVAnalysisSummaryCpuPropertyMemoryEventsDurationTotal,
    kRPVAnalysisSummaryCpuPropertyNumTopMemoryEvents,
    kRPVAnalysisSummaryCpuPropertyTopMemoryEventNameIndexed,
    kRPVAnalysisSummaryCpuPropertyTopMemoryEventInvocationsIndexed,
    kRPVAnalysisSummaryCpuPropertyTopMemoryEventDurationSumIndexed,
    kRPVAnalysisSummaryCpuPropertyTopMemoryEventDurationMinIndexed,
    kRPVAnalysisSummaryCpuPropertyTopMemoryEventDurationMaxIndexed,
    kRPVAnalysisSummaryCpuPropertyTopMemoryEventDurationPctIndexed,
    kRPVAnalysisSummaryCpuPropertyTopMemoryEventDurationAvgIndexed,
    kRPVAnalysisSummaryCpuPropertyNumCounters,
    kRPVAnalysisSummaryCpuPropertyCounterIndexed,
    kRPVAnalysisSummaryCpuPropertyCounterMinIndexed,
    kRPVAnalysisSummaryCpuPropertyCounterMaxIndexed,
    kRPVAnalysisSummaryCpuPropertyCounterMeanIndexed,
    kRPVAnalysisSummaryCpuPropertyCounterStdDevIndexed,
    __kRPVAnalysisSummaryCpuPropertiesLast
} rocprofvis_analysis_summary_cpu_property_t;

/*
 * Properties for a process or thread at the trace, node, and process and thread aggregation levels.
 */
typedef enum rocprofvis_analysis_summary_process_thread_property_t : uint32_t
{
    __kRPVAnalysisSummaryProcessThreadPropertiesFirst = 0xF0500000,
    kRPVAnalysisSummaryProcessThreadPropertyLaunchEventsInvocationsTotal = __kRPVAnalysisSummaryProcessThreadPropertiesFirst,
    kRPVAnalysisSummaryProcessThreadPropertyLaunchEventsDurationTotal,
    kRPVAnalysisSummaryProcessThreadPropertyNumTopLaunchEvents,
    kRPVAnalysisSummaryProcessThreadPropertyTopLaunchEventNameIndexed,
    kRPVAnalysisSummaryProcessThreadPropertyTopLaunchEventInvocationsIndexed,
    kRPVAnalysisSummaryProcessThreadPropertyTopLaunchEventDurationSumIndexed,
    kRPVAnalysisSummaryProcessThreadPropertyTopLaunchEventDurationMinIndexed,
    kRPVAnalysisSummaryProcessThreadPropertyTopLaunchEventDurationMaxIndexed,
    kRPVAnalysisSummaryProcessThreadPropertyTopLaunchEventDurationPctIndexed,
    kRPVAnalysisSummaryProcessThreadPropertyTopLaunchEventDurationAvgIndexed,
    __kRPVAnalysisSummaryProcessThreadPropertiesLast
} rocprofvis_analysis_summary_process_thread_property_t;

/*
 * Arguments to fetch summary metrics from the summary controller
 */
typedef enum rocprofvis_analysis_summary_arguments_t : uint32_t
{
    kRPVAnalysisSummaryArgsStartTimestamp = 0x12000000,
    kRPVAnalysisSummaryArgsEndTimestamp,
} rocprofvis_analysis_summary_arguments_t;

/*
* Duration-weighted statistics for a counter track over a time range.
*/
typedef struct rocprofvis_analysis_counter_statistics_t
{
    double min_value;   // minimum counter value over the range
    double max_value;   // maximum counter value over the range
    double mean_value;  // duration-weighted mean counter value over the range
    double std_dev;     // duration-weighted standard deviation over the range
} rocprofvis_analysis_counter_statistics_t;

/*
* Calculates duration-weighted statistics for the specified counter track within the given time range.
* @param controller The system trace controller instance.
* @param track The handle track to analyze.
* @param start_time The start time in ns of the analysis range.
* @param end_time The end time in ns of the analysis range.
* @param result The future object to store the result.
* @param output The output struct to write the counter statistics.
* @returns kRocProfVisResultSuccess or an error code.
*/
rocprofvis_result_t rocprofvis_analysis_fetch_counter_statistics(rocprofvis_controller_t* controller, rocprofvis_controller_track_t* track, double start_time, double end_time, rocprofvis_controller_future_t* result, rocprofvis_analysis_counter_statistics_t* output);

/*
* Calculates the queue utilization for the specified track within the given time range.
* @param controller The system trace controller instance.
* @param track The handle track to analyze.
* @param start_time The start time in ns of the analysis range.
* @param end_time The end time in ns of the analysis range.
* @param result The future object to store the result.
* @param output The output value to write the queue utilization.
* @returns kRocProfVisResultSuccess or an error code.
*/
rocprofvis_result_t rocprofvis_analysis_fetch_queue_utilization(rocprofvis_controller_t* controller, rocprofvis_controller_track_t* track, double start_time, double end_time, rocprofvis_controller_future_t* result, double* output);

/*
* Setup the summary based on the values in 'args' and fetch data.
* @param controller The system trace controller instance.
* @param args The arguments that setup the summary.
* @param result The future to wait on.
* @param output The summary metrics container to write to.
* @returns kRocProfVisResultSuccess or an error code.
*/
rocprofvis_result_t rocprofvis_analysis_fetch_summary(rocprofvis_controller_t* controller, rocprofvis_controller_arguments_t* args, rocprofvis_controller_future_t* result, rocprofvis_controller_summary_metrics_t* output);

/*
* Returns the summary kernel-instance table.
* @param controller The system trace controller instance.
* @param table Out-param that receives the table handle.
* @returns kRocProfVisResultSuccess or an error code.
*/
rocprofvis_result_t rocprofvis_analysis_get_summary_kernel_instance_table(rocprofvis_controller_t* controller, rocprofvis_handle_t** table);

/*
* Returns the summary memory-instance table.
* @param controller The system trace controller instance.
* @param table Out-param that receives the table handle.
* @returns kRocProfVisResultSuccess or an error code.
*/
rocprofvis_result_t rocprofvis_analysis_get_summary_memory_instance_table(rocprofvis_controller_t* controller, rocprofvis_handle_t** table);

/*
* Returns the summary thread-instance table.
* @param controller The system trace controller instance.
* @param table Out-param that receives the table handle.
* @returns kRocProfVisResultSuccess or an error code.
*/
rocprofvis_result_t rocprofvis_analysis_get_summary_thread_instance_table(rocprofvis_controller_t* controller, rocprofvis_handle_t** table);

/*
* Returns the instrumented-thread events table.
* @param controller The system trace controller instance.
* @param table Out-param that receives the table handle.
* @returns kRocProfVisResultSuccess or an error code.
*/
rocprofvis_result_t rocprofvis_analysis_get_instrumented_events_table(rocprofvis_controller_t* controller, rocprofvis_handle_t** table);

/*
* Returns the dispatch events table.
* @param controller The system trace controller instance.
* @param table Out-param that receives the table handle.
* @returns kRocProfVisResultSuccess or an error code.
*/
rocprofvis_result_t rocprofvis_analysis_get_dispatch_events_table(rocprofvis_controller_t* controller, rocprofvis_handle_t** table);

/*
* Returns the memory-allocation events table.
* @param controller The system trace controller instance.
* @param table Out-param that receives the table handle.
* @returns kRocProfVisResultSuccess or an error code.
*/
rocprofvis_result_t rocprofvis_analysis_get_memory_allocation_events_table(rocprofvis_controller_t* controller, rocprofvis_handle_t** table);

/*
* Returns the memory-copy events table.
* @param controller The system trace controller instance.
* @param table Out-param that receives the table handle.
* @returns kRocProfVisResultSuccess or an error code.
*/
rocprofvis_result_t rocprofvis_analysis_get_memory_copy_events_table(rocprofvis_controller_t* controller, rocprofvis_handle_t** table);

/*
* Returns the hip events table.
* @param controller The system trace controller instance.
* @param table Out-param that receives the table handle.
* @returns kRocProfVisResultSuccess or an error code.
*/
rocprofvis_result_t rocprofvis_analysis_get_hip_events_table(rocprofvis_controller_t* controller, rocprofvis_handle_t** table);

/*
* Returns the sampled events table.
* @param controller The system trace controller instance.
* @param table Out-param that receives the table handle.
* @returns kRocProfVisResultSuccess or an error code.
*/
rocprofvis_result_t rocprofvis_analysis_get_sampled_events_table(rocprofvis_controller_t* controller, rocprofvis_handle_t** table);

/*
* Allocates a caller-owned events table filtered to one operation.
*
* The rocprofvis_analysis_get_*_events_table calls above hand back tables the
* controller keeps per trace, so two readers of the same category share a row
* cache: whoever fetches last is what both of them see. Use this instead when a
* reader must not disturb what another one is showing.
*
* Free with rocprofvis_controller_table_free.
* @param op The event operation the table is filtered to, see rocprofvis_dm_event_operation_t.
* @param table Out-param that receives the table handle.
* @returns kRocProfVisResultSuccess or an error code.
*/
rocprofvis_result_t rocprofvis_analysis_events_table_alloc(uint64_t op, rocprofvis_handle_t** table);

/*
* Fetches rows for an analysis table.
* @param controller The system trace controller instance.
* @param table A table handle from one of the rocprofvis_analysis_get_*_events_table getters.
* @param args Query arguments.
* @param result The future object used to wait for completion.
* @param output Array that receives the fetched rows.
* @returns kRocProfVisResultSuccess or an error code.
*/
rocprofvis_result_t rocprofvis_analysis_fetch_table(rocprofvis_controller_t* controller, rocprofvis_controller_table_t* table, rocprofvis_controller_arguments_t* args, rocprofvis_controller_future_t* result, rocprofvis_controller_array_t* output);

/*
* Exports an analysis table to a CSV file.
* @param controller The system trace controller instance.
* @param table A table handle from one of the rocprofvis_analysis_get_*_events_table getters.
* @param args Query arguments.
* @param result The future object used to wait for completion.
* @param path Filesystem path where the CSV file will be written.
* @returns kRocProfVisResultSuccess or an error code.
*/
rocprofvis_result_t rocprofvis_analysis_table_export_csv(rocprofvis_controller_t* controller, rocprofvis_controller_table_t* table, rocprofvis_controller_arguments_t* args, rocprofvis_controller_future_t* result, char const* path);

/*
* Releases the analysis data associated with the given controller. Call when
* closing the controller.
* @param controller The system trace controller instance.
*/
void rocprofvis_analysis_free_trace_data(rocprofvis_controller_t* controller);

#ifdef __cplusplus
}
#endif
