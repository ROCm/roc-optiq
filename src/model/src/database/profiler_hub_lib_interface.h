#pragma once

#include "profiler_hub_interface.h"

extern "C"
{
	profiler_hub_future_handle_t profiler_hub_future_alloc(profiler_hub::progress_callback_t progress_callback);
	profiler_hub_result_t profiler_hub_future_free(profiler_hub_future_handle_t handle);
	profiler_hub_result_t profiler_hub_future_wait(profiler_hub_future_handle_t handle, uint64_t timeout_ms);
	profiler_hub_result_t profiler_hub_future_cancel(profiler_hub_future_handle_t handle);

	profiler_hub_db_type_t profiler_hub_db_identify_type(
		profiler_hub_string_t trace_file_path
	);

	profiler_hub_trace_handle_t profiler_hub_open_trace(
		profiler_hub_string_t trace_file_path
	);

	profiler_hub_result_t profiler_hub_set_trace_properties(profiler_hub_trace_handle_t trace,
		client_trace_handle_t client_trace,
		profiler_hub_string_t config_path,
		size_t histogram_bucket_count);

	profiler_hub_result_t profiler_hub_read_metadata(
		profiler_hub_future_handle_t future_handle,
		profiler_hub_trace_handle_t trace
	);

	profiler_hub_result_t profiler_hub_close_trace(
		profiler_hub_future_handle_t future_handle,
		profiler_hub_trace_handle_t trace
	);

	profiler_hub_value_type_t profiler_hub_get_info(
		profiler_hub_trace_handle_t trace,
		profiler_hub_instance_id_t instance,
		profiler_hub_property_category_t category,
		uint64_t row_key,
		profiler_hub_string_t property_tag,
		profiler_hub_optional_t value // OUT
	);

	profiler_hub_result_t profiler_hub_get_time_slice(
		profiler_hub_future_handle_t future_handle,
		profiler_hub_trace_handle_t trace,
		profiler_hub_track_id_t track_id,
		uint64_t timestamp_start,
		uint64_t timestamp_end
	);

	profiler_hub_result_t profiler_hub_get_table_time_slice(
		profiler_hub_future_handle_t future_handle,
		profiler_hub_trace_handle_t trace,
		profiler_hub_table_handle_t table_handle,
		profiler_hub_track_id_t track_id,
		uint64_t timestamp_start,
		uint64_t timestamp_end
	);

	profiler_hub_result_t profiler_hub_get_search_time_slice(
		profiler_hub_future_handle_t future_handle,
		profiler_hub_trace_handle_t trace,
		profiler_hub_instance_id_t instance,
		profiler_hub_table_handle_t table_handle,
		size_t num_operations,
		profiler_hub_event_operation_t* operations,
		uint64_t timestamp_start,
		uint64_t timestamp_end,
		size_t num_search_strings,
		profiler_hub_search_strings_t string_filters
	);

	profiler_hub_result_t profiler_hub_get_event_data_flow(
		profiler_hub_future_handle_t future_handle,
		profiler_hub_trace_handle_t trace,
		profiler_hub_instance_id_t instance,
		profiler_hub_event_operation_t operation,
		profiler_hub_event_id_t event_id
	);

	profiler_hub_result_t profiler_hub_get_event_extended_data(
		profiler_hub_future_handle_t future_handle,
		profiler_hub_trace_handle_t trace,
		profiler_hub_instance_id_t instance,
		profiler_hub_event_operation_t operation,
		profiler_hub_event_id_t event_id
	);

	profiler_hub_result_t profiler_hub_get_event_stack_trace(
		profiler_hub_future_handle_t future_handle,
		profiler_hub_trace_handle_t trace,
		profiler_hub_instance_id_t instance,
		profiler_hub_event_operation_t operation,
		profiler_hub_event_id_t event_id
	);

	profiler_hub_result_t profiler_hub_trim_save_trace(
		profiler_hub_future_handle_t future_handle,
		profiler_hub_trace_handle_t trace,
		uint64_t timestamp_start,
		uint64_t timestamp_end);

}