#pragma once

#include "profiler_hub_future.hpp"

namespace profiler_hub::interface
{

    extern "C"
    {
        // Allocate future object for asynchronous operations. 
        // Returns future object handle
        profiler_hub_future_handle_t FutureAlloc(progress_callback_t progress_callback);

        // Delete future object after asynchronous operation is completed
        profiler_hub_result_t FutureFree(
            profiler_hub_future_handle_t future
        );

        // Wait for asynchronous operation to complete. 
        // Returns completion status
        // timeout in milliseconds
        profiler_hub_result_t FutureWait(
            profiler_hub_future_handle_t future,
            uint64_t timeout_ms
        );

        // Cancel asynchronous operation
        profiler_hub_result_t FutureCancel(
            profiler_hub_future_handle_t future
        );

        // Detects if trace file format is supported by profiler hub
        // trace_file_path - path to the trace file
	profiler_hub_db_type_t DetectTrace(
		profiler_hub_string_t trace_file_path
	);
        // Opens trace, read and compile metadata. 
        // This method should build tracks, collect node/instance information, cache information tables, calculate levels, build histograms, etc.
        // client_trace - client trace reference
        // trace_file_path - path to the trace file

    profiler_hub_trace_handle_t OpenTrace(
            profiler_hub_string_t trace_file_path 
        );

        // Set trace properties, just a setter for trace parameters, historically separated from constructor 
        // config_dir_path - path to the location temporary files should be stored
        // histogram_bucket_count - caller should decide density of the histogram 
        profiler_hub_result_t SetTraceProperties(
            profiler_hub_trace_handle_t trace,
            client_trace_handle_t client_trace,
            profiler_hub_string_t config_dir_path,
            size_t histogram_bucket_count
        );

        // Read trace
        profiler_hub_result_t ReadTraceMetadata(
            profiler_hub_future_handle_t future_handle,
            profiler_hub_trace_handle_t trace
        );

        // Close trace, destroy trace-related objects 
        profiler_hub_result_t CloseTrace(
            profiler_hub_trace_handle_t trace
        );

        // Gets information tables property specified by caller
        // Information tables list can be bigger that tables provided by schema
        // Missing tables must be synthesized
        // Use data-model code for reference
        // trace - trace handle
        // instance - multi-node instance, determined by GUID
        // category - determines which cached table to use
        // row_key - cached table row primary key
        // property_tag - column name, usually provided by schema, sometimes synthesized
        // value - return as string
        // returns value type, kProfilerHubDataTypeUndefined if undefined
        profiler_hub_value_type_t GetProperty(
            profiler_hub_trace_handle_t trace,
            profiler_hub_instance_id_t instance,
            profiler_hub_property_category_t category,
            uint64_t row_key,
            profiler_hub_string_t property_tag,
            profiler_hub_optional_t value // OUT
        );

        // time slice request
        // trace - handle of a trace, considering single profiler hub instance handles multiple traces. 
        // track_id - it seems most practical to query time slice for single track, this method does not take instance id, because a single track must belong to single trace instance
        // timestamp_start - time-slice start
        // timestamp_end - time-slice end
        profiler_hub_result_t GetTimeSlice(
            profiler_hub_future_handle_t future,
            profiler_hub_trace_handle_t trace,
            profiler_hub_track_id_t track_id,
            uint64_t timestamp_start,
            uint64_t timestamp_end
        );

        // get events data for the Table view. The rows will be post processed, aligned, grouped, filtered in the data-model
        // trace - handle of a trace, considering single profiler hub instance handles multiple traces. 
        // table_handle - data-model table processor instance handle.
        // track_id - table data is requested per track. 
        // timestamp_start - time-slice start
        // timestamp_end - time-slice end
        profiler_hub_result_t GetTableTimeSlice(
            profiler_hub_future_handle_t future,
            profiler_hub_trace_handle_t trace,
            profiler_hub_table_handle_t table_handle,
            profiler_hub_track_id_t track_id,
            uint64_t timestamp_start,
            uint64_t timestamp_end
        );

        // search for events by provided string filters
        // trace - handle of a trace, considering single profiler hub instance handles multiple traces.
        // table_handle - data-model table processor instance handle.
        // num_operations - length of operations list. The search is done per operation (e.g. region/kernel/memalloc/memcopy) or list of operations
        // operations - list of operations
        // timestamp_start - time-slice start
        // timestamp_end - time-slice end
        // num_search_strings - number of strings in the string filter
        // string_filters - array of string to filter events
        profiler_hub_result_t GetSearchTimeSlice(
            profiler_hub_future_handle_t future,
            profiler_hub_trace_handle_t trace,
            profiler_hub_instance_id_t instance,
            profiler_hub_table_handle_t table_handle,
            size_t num_operations,
            profiler_hub_event_operation_t * operations,
            uint64_t timestamp_start,
            uint64_t timestamp_end,
            size_t num_search_strings,
            profiler_hub_search_strings_t string_filters               
        );

        // get data flow end-point events
        // trace - handle of a trace, considering single profiler hub instance handles multiple traces. 
        // instance - multi-node instance, determined by GUID
        // operation - operation type corresponds to specific event table
        // event_id - event id. Historically event_id represents primary key relative to a specific event table
        profiler_hub_result_t GetEventDataFlow(
            profiler_hub_future_handle_t future,
            profiler_hub_trace_handle_t trace,
            profiler_hub_instance_id_t instance,
            profiler_hub_event_operation_t operation,
            profiler_hub_event_id_t event_id
        );

        // get extended data properties
        // trace - handle of a trace, considering single profiler hub instance handles multiple traces. 
        // instance - multi-node instance, determined by GUID
        // operation - operation type corresponds to specific event table
        // event_id - event id. Historically event_id represents primary key relative to a specific event table
        profiler_hub_result_t GetEventExtendedData(
            profiler_hub_future_handle_t future,
            profiler_hub_trace_handle_t trace,
            profiler_hub_instance_id_t instance,
            profiler_hub_event_operation_t operation,
            profiler_hub_event_id_t event_id
        );

        // get event call stack trace
        // trace - handle of a trace, considering single profiler hub instance handles multiple traces. 
        // instance - multi-node instance, determined by GUID
        // operation - operation type corresponds to specific event table
        // event_id - event id. Historically event_id represents primary key relative to a specific event table
        profiler_hub_result_t GetEventStackTrace(
            profiler_hub_future_handle_t future,
            profiler_hub_trace_handle_t trace,
            profiler_hub_instance_id_t instance,
            profiler_hub_event_operation_t operation,
            profiler_hub_event_id_t event_id
        );

        // trim trace database to time range specified in parameters
        // trace - handle of a trace, considering single profiler hub instance handles multiple traces. 
        // timestamp_start - trim start
        // timestamp_end - trim end
        profiler_hub_result_t TrimTraceDatabase(
            profiler_hub_future_handle_t future_handle,
            profiler_hub_trace_handle_t trace,
            uint64_t timestamp_start,
            uint64_t timestamp_end);

    }

}  // namespace ProfilerHub