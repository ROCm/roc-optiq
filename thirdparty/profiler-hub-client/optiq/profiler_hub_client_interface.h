#pragma once

#include "profiler_hub_interface_types.h"

namespace profiler_hub::client::interface
{

    extern "C"
    {
        // report a single trace instance. Usually based on GUID (for rocpd 3.x and later), otherwise just synthesized single instance.
        // trace - handle of a trace, considering single profiler hub instance handles multiple traces.
        // id - instance index
        // file - which file the instance comes from, in case of multi-file trace
        // uuid - UUID of the instance, if applicable
        profiler_hub_result_t AddInstance(
            client_trace_handle_t trace,
            profiler_hub_instance_id_t id,
            profiler_hub_string_t file,
            profiler_hub_string_t uuid
        );

        // helps to collect strings table for a trace. It's better to replace any string passed to data-model in future requests with integer id. To do so profiler-hub must have keep remapping look-up tables.
        // trace - handle of a trace, considering single profiler hub instance handles multiple traces.
        // string - a string from any column
        // string_id - string index, expected to start with 0 when first string submitted, but can be re-purposed. The idea is data-model will match any string id from future calls to the strin in the table.
        profiler_hub_result_t AddString(
            client_trace_handle_t trace,
            profiler_hub_string_t string,
            uint32_t string_id
        );

        // This will convert profiler-hub track representation to data-model
        // trace - handle of a trace, considering single profiler hub instance handles multiple traces.
        // instance - what trace instance the track belong to
        // category - track category, as enumerated in profiler_hub_track_category_t
        // track_id - expected to be sequential number, but can be re-mapped if needed
        // track_name - optional track name
        // node_id - full node id from database, 0 if not applicable
        // node_name - node name if applicable
        // process_id - full process id, not table index
        // process_name - process name, if applicable
        // thread_id - full thread id, not table index
        // thread_name - thread name, if applicable
        // stream_id - stream id
        // thread_name - stream name, if applicable
        // agent_type - agent type as defined in profiler_hub_agent_type_t
        // agent_id - agent typed id
        // agent_name - agent name, if applicable
        // queue_id - queue id
        // queue_name - queue name, if applicable
        // counter_id - counter id (for rocpd 3.x it will be pmc info id, for original rocpd schema - synthesized, for perfetto - counter track id)
        // counter_name - counter name, required, if counter track
        // records_count - how many records the track have
        // min_timestamp - minimum timestamp
        // max_timestamp - maximum timestamp
        // min_level_or_value - minimum level for event track - expected 0, minimum value for counter track 
        // max_level_or_value - maximum level for event track, maximum value for counter track
        profiler_hub_result_t AddTrack(
            client_trace_handle_t trace,
            profiler_hub_instance_id_t instance,
            profiler_hub_track_id_t track_id,
            profiler_hub_string_t track_name, 
            profiler_hub_track_category_t category,
            profiler_hub_optional_int_t node_id,
            profiler_hub_string_t node_name,
            profiler_hub_optional_int_t process_id,
            profiler_hub_string_t process_name,
            profiler_hub_optional_int_t thread_id,
            profiler_hub_string_t thread_name,
            profiler_hub_optional_int_t stream_id,
            profiler_hub_string_t stream_name,
            profiler_hub_agent_type_t agent_type,
            profiler_hub_optional_int_t agent_id,
            profiler_hub_string_t agent_name,
            profiler_hub_optional_int_t queue_id,
            profiler_hub_string_t queue_name,
            profiler_hub_optional_int_t counter_id,
            profiler_hub_string_t counter_name,
            uint32_t records_count,
            uint64_t min_timestamp,
            uint64_t max_timestamp,
            double min_level_or_value,
            double max_level_or_value
        );

        // Adds histogram bucket to track histogram
        // trace - handle of a trace, considering single profiler hub instance handles multiple traces.
        // track_id - track id, the combination of trace+track_id can be replaced with track_handler, but then AddTrack has to return the handler and profiler-hub must keep it for reference
        // bucket_number - bucket number, no need to send zero count buckets, so the value is not sequential
        // events_count - number of events in the bucket, skip zero event buckets
        // bucket_value - average value for counter sample buckets, for events can be max level (tbd)
        profiler_hub_result_t AddTrackHistogramBucket(
            client_trace_handle_t trace,
            profiler_hub_track_id_t track_id,
            uint32_t bucket_number,
            uint32_t events_count,
            double bucket_value
        );

        // Add track extended info
        // trace - handle of a trace, considering single profiler hub instance handles multiple traces.
        // track_id - track id, the combination of trace+track_id can be replaced with track_handler, but then AddTrack has to return the handler and profiler-hub must keep it for reference
        // category - can be table name from where extended data has been taken, or synthesized
        // name - can be column name from where extended data has been taken, or synthesized
        // type - SQL type, as described in profiler_hub_value_type_t
        // value - value as a void pointer to a value , will be cast based on type
        profiler_hub_result_t  AddTrackExtendedInfo(
            client_trace_handle_t trace,
            profiler_hub_track_id_t track_id,
            profiler_hub_string_t category,
            profiler_hub_string_t name,
            profiler_hub_value_type_t type,
            profiler_hub_value_handle_t value
        );

        // Add time slice container for requested track and time window
        // trace - handle of a trace, considering single profiler hub instance handles multiple traces.
        // track_id - track id, the combination of trace+track_id can be replaced with track_handler, but then AddTrack has to return the handler and profiler-hub must keep it for reference
        // timestamp_start - requested time start
        // timestamp_end - requested time end
        profiler_hub_timeslice_handle_t AddTimeSliceContainer(
            client_trace_handle_t trace,
            profiler_hub_track_id_t track_id,
            uint64_t timestamp_start,
            uint64_t timestamp_end
        );

        // Add Event record to time slice container
        // container - time slice container handle
        // operation - event operation, as described in profiler_hub_event_operation_t
        // event_id - event id relative to operation
        // timestamp - start time of the event
        // duration - event duration 
        // category_id - index of category in string table, be ready to remap database table string index into data-model string table
        // symbol_id - index of symbol in string table, be ready to remap database table string index into data-model string table
        // level - event level for event stacking. The same level should be used to generate call stack trace. It seems to be most reliable and universal way, unlike parent_id/parent_stack_id combination
        profiler_hub_result_t AddEventRecord(
            profiler_hub_timeslice_handle_t container,
            profiler_hub_event_operation_t operation,
            profiler_hub_event_id_t event_id,
            uint64_t timestamp,
            uint64_t duration,
            profiler_hub_string_id_t category_id,
            profiler_hub_string_id_t symbol_id,
            profiler_hub_event_level_t level
        );

        // Add PMC record to time slice container
        // container - time slice container handle
        // timestamp - start time of the counter sample
        // value - counter sample value
        profiler_hub_result_t AddPmcRecord(
            profiler_hub_timeslice_handle_t container,
            uint64_t timestamp,
            double value
        );

        // Add an empty row to table processor container
        // container - table processor container handle
        profiler_hub_table_row_handle_t AddTableRowContainer(
            profiler_hub_table_handle_t container
        );

        // Add column name and type to the table processor container
        // container - table processor container handle
        // name - column name
        // type - column SQL type, as enumerated in profiler_hub_value_type_t
        profiler_hub_result_t AddTableColumn(
            profiler_hub_table_handle_t container,
            profiler_hub_string_t name,
            profiler_hub_value_type_t type
        );

        // Add table cell value as string. Although all strings should be converted to string table index inside profiler-hub
        // container - table row container handle
        // column_index - column index
        // value - value as string
        profiler_hub_result_t AddTableCellAsString(
            profiler_hub_table_row_handle_t container,
            uint32_t column_index,
            profiler_hub_string_t value
        );

        // Add table cell value as integer. 
        // container - table row container handle
        // column_index - column index
        // value - value as integer
        profiler_hub_result_t AddTableCellAsInt(
            profiler_hub_table_row_handle_t container,
            uint32_t column_index,
            uint64_t value
        );

        // Add table cell value as floating point. 
        // container - table row container handle
        // column_index - column index
        // value - value as double
        profiler_hub_result_t AddTableCellAsDouble(
            profiler_hub_table_row_handle_t container,
            uint32_t column_index,
            double value
        );

        // Add flow trace container
        // trace - handle of a trace, considering single profiler hub instance handles multiple traces.
        // instance - multi-node instance,
        // operation - event operation, as described in profiler_hub_event_operation_t
        // event_id - event id relative to operation
        profiler_hub_flowtrace_handle_t AddEventFlowTraceContainer(
            client_trace_handle_t trace,
            profiler_hub_instance_id_t instance,
            profiler_hub_event_operation_t operation,
            profiler_hub_event_id_t event_id
        );

        // Add flow trace endpoint
        // container - flow trace container handle
        // operation - event operation, as described in profiler_hub_event_operation_t
        // event_id - event id relative to operation
        // direction - incoming/outgoing
        // timestamp - endpoint timestamp
        // duration - endpoint duration
        // category_id - endpoint category id
        // symbol_id - endpoint symbol id
        // level - endpoint level
        profiler_hub_result_t AddEventDataFlowEndPoint(
            profiler_hub_flowtrace_handle_t container,
            profiler_hub_event_operation_t operation,
            profiler_hub_event_id_t event_id,
            profiler_hub_flow_direction_t direction,
            uint64_t timestamp,
            uint64_t duration,
            profiler_hub_string_id_t category_id,
            profiler_hub_string_id_t symbol_id,
            profiler_hub_event_level_t level
        );

        // Add extended data container
        // trace - handle of a trace, considering single profiler hub instance handles multiple traces.
        // instance - multi-node instance, based on GUID when applicable
        // operation - event operation, as described in profiler_hub_event_operation_t
        // event_id - event id relative to operation
        profiler_hub_ext_data_handle_t AddEventExtDataContainer(
            client_trace_handle_t trace,
            profiler_hub_instance_id_t instance,
            profiler_hub_event_operation_t operation,
            profiler_hub_event_id_t event_id
        );

        // Add extended data record
        // container - extended data container
        // category - data category. Can be table name
        // name - can be column name
        // type - SQL type, as described in profiler_hub_value_type_t
        // value - pointer to a value of type
        profiler_hub_result_t  AddEventExtendedInfo(
            profiler_hub_ext_data_handle_t container,
            profiler_hub_string_t category,
            profiler_hub_string_t name,
            profiler_hub_value_type_t type,
            profiler_hub_string_t value
        );

        // Add essential data to extended data table
        // container - extended data container
        // track_id - event track id
        // stream_track_id - event stream track id, set to -1 if event does not belong to any stream
        // level - event level
        // stream_level - event level on stream track, set to -1 if event doesn't belong to any stream
        profiler_hub_result_t  AddEventEssentialInfo(
            profiler_hub_ext_data_handle_t container,
            profiler_hub_track_id_t track_id,
            profiler_hub_track_id_t stream_track_id,
            profiler_hub_event_level_t level,
            profiler_hub_event_level_t stream_level
        );

        // Add event arguments info
        // container - extended data container
        // position - argument position
        // name - argument name
        // type - argument type
        profiler_hub_result_t  AddEventArgumentsInfo(
            profiler_hub_ext_data_handle_t container,
            uint32_t position,
            profiler_hub_string_t name,
            profiler_hub_string_t type,
            profiler_hub_string_t value
        );

        // Add event call stack container
        // trace - handle of a trace, considering single profiler hub instance handles multiple traces.
        // instance - multi-node instance, based on GUID when applicable
        // operation - event operation, as described in profiler_hub_event_operation_t
        // event_id - event id relative to operation
        profiler_hub_call_stack_handle_t AddEventCallStackContainer(
            client_trace_handle_t trace,
            profiler_hub_instance_id_t instance,
            profiler_hub_event_operation_t operation,
            profiler_hub_event_id_t event_id
        );

        // Add call stack frame to container
        // container - call stack container
        // symbol - stack frame symbol
        // file - stack frame file
        // line - stack frame code line
        // depth - stack frame depth
        profiler_hub_result_t  AddEventCallStackFrame(
            profiler_hub_call_stack_handle_t container,
            profiler_hub_string_t function,
            profiler_hub_string_t file,
            profiler_hub_string_t line,
            profiler_hub_string_t address,
            uint32_t depth
        );

    }

}  // namespace ProfilerHub