// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_controller.h"

#ifdef __cplusplus
extern "C"
{
#endif

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
