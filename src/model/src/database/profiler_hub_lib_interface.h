#pragma once

#ifdef USE_PROFILER_HUB

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
		profiler_hub_trace_handle_t trace
	);

	profiler_hub_result_t profiler_hub_get_time_slice(
		profiler_hub_future_handle_t future_handle,
		profiler_hub_trace_handle_t trace,
		profiler_hub_track_id_t track_id,
		profiler_hub_timeslice_handle_t slice_container,
		uint64_t timestamp_start,
		uint64_t timestamp_end
	);

	profiler_hub_result_t profiler_hub_get_pmc_time_slice(
		profiler_hub_future_handle_t future_handle,
		profiler_hub_trace_handle_t trace,
		profiler_hub_track_id_t track_id,
		profiler_hub_timeslice_handle_t slice_container,
		uint64_t timestamp_start,
		uint64_t timestamp_end,
		bool left_neighbor,
		bool right_neighbor
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
		profiler_hub_event_operation_t operation,
		uint64_t timestamp_start,
		uint64_t timestamp_end,
		size_t num_search_strings,
		profiler_hub_search_strings_t string_filters
	);

	profiler_hub_result_t profiler_hub_get_event_data_flow(
		profiler_hub_future_handle_t future_handle,
		profiler_hub_trace_handle_t trace,
		profiler_hub_instance_id_t instance,
		profiler_hub_flowtrace_handle_t container,
		profiler_hub_event_operation_t operation,
		profiler_hub_event_id_t event_id
	);

	profiler_hub_result_t profiler_hub_get_event_extended_data(
		profiler_hub_future_handle_t future_handle,
		profiler_hub_trace_handle_t trace,
		profiler_hub_instance_id_t instance,
		profiler_hub_ext_data_handle_t container,
		profiler_hub_event_operation_t operation,
		profiler_hub_event_id_t event_id
	);

	profiler_hub_result_t profiler_hub_get_event_stack_trace(
		profiler_hub_future_handle_t future_handle,
		profiler_hub_trace_handle_t trace,
		profiler_hub_instance_id_t instance,
		profiler_hub_call_stack_handle_t container,
		profiler_hub_event_operation_t operation,
		profiler_hub_event_id_t event_id
	);

	profiler_hub_result_t profiler_hub_trim_save_trace(
		profiler_hub_future_handle_t future_handle,
		profiler_hub_trace_handle_t trace,
		uint64_t timestamp_start,
		uint64_t timestamp_end,
		profiler_hub_string_t new_path);

}

#endif