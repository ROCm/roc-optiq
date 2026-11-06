#include "profiler_hub_lib_interface.h"
#include "spdlog/spdlog.h"
#define ANSI_COLOR_ERROR     "\x1b[31m"

const char* error_fmt = ANSI_COLOR_ERROR"Profiler hub failed : {}";

profiler_hub_future_handle_t profiler_hub_future_alloc(profiler_hub::progress_callback_t progress_callback) {
	try {
		return profiler_hub::interface::FutureAlloc(progress_callback);
	}
	catch (const std::exception& e)
	{
		spdlog::error(error_fmt, e.what());
		return nullptr;
	}
}

profiler_hub_result_t profiler_hub_future_free(profiler_hub_future_handle_t handle) {
	try {
		return profiler_hub::interface::FutureFree(handle);
	}
	catch (const std::exception& e)
	{
		spdlog::error(error_fmt, e.what());
		return kProfilerHubStatusUnknownError;
	}
}

profiler_hub_result_t profiler_hub_future_wait(profiler_hub_future_handle_t handle, uint64_t timeout_ms) {
	try {
		return profiler_hub::interface::FutureWait(handle, timeout_ms);
	}
	catch (const std::exception& e)
	{
		spdlog::error(error_fmt, e.what());
		return kProfilerHubStatusUnknownError;
	}
}

profiler_hub_result_t profiler_hub_future_cancel(profiler_hub_future_handle_t handle) {
	try {
		return profiler_hub::interface::FutureCancel(handle);
	}
	catch (const std::exception& e)
	{
		spdlog::error(error_fmt, e.what());
		return kProfilerHubStatusUnknownError;
	}
}

profiler_hub_db_type_t profiler_hub_db_identify_type(
	profiler_hub_string_t trace_file_path
) {
	profiler_hub_db_type_t type = kDbNotSupported;
	try {
		type = profiler_hub::interface::DetectTrace(trace_file_path);
	}
	catch (std::exception& e)
	{
		spdlog::error(error_fmt, e.what());
		type = kDbNotSupported;
	}
	return type;
}


profiler_hub_trace_handle_t profiler_hub_open_trace(
	profiler_hub_string_t trace_file_path
) {
	profiler_hub_trace_handle_t trace = nullptr;
	try {
		trace = profiler_hub::interface::OpenTrace(trace_file_path);
	}
	catch (std::exception& e)
	{
		spdlog::error(error_fmt, e.what());
		trace = nullptr;
	}
	return trace;
}

profiler_hub_result_t profiler_hub_set_trace_properties(profiler_hub_trace_handle_t trace,
	client_trace_handle_t client_trace,
	profiler_hub_string_t config_path,
	size_t histogram_bucket_count)
{
	profiler_hub_result_t result = kProfilerHubStatusUnknownError;
	try {
		result = profiler_hub::interface::SetTraceProperties(trace, client_trace, config_path, histogram_bucket_count);
	}
	catch (std::exception& e)
	{
		spdlog::error(error_fmt, e.what());
		result = kProfilerHubStatusUnknownError;
	}
	return result;
}
	

profiler_hub_result_t profiler_hub_read_metadata(
	profiler_hub_future_handle_t future_handle,
	profiler_hub_trace_handle_t trace
) {
	profiler_hub_result_t result = kProfilerHubStatusUnknownError;
	try {
		result = profiler_hub::interface::ReadTraceMetadata(future_handle, trace);
	}
	catch (std::exception& e)
	{
		spdlog::error(error_fmt, e.what());
		result = kProfilerHubStatusUnknownError;
	}
	return result;
}

profiler_hub_result_t profiler_hub_close_trace(
	profiler_hub_future_handle_t future_handle,
	profiler_hub_trace_handle_t trace
) {
	profiler_hub_result_t result = kProfilerHubStatusUnknownError;
	try {
		result = profiler_hub::interface::CloseTrace(trace);
	}
	catch (std::exception& e)
	{
		spdlog::error(error_fmt, e.what());
		result = kProfilerHubStatusUnknownError;
	}
	return result;
}

profiler_hub_value_type_t profiler_hub_get_info(
	profiler_hub_trace_handle_t trace,
	profiler_hub_instance_id_t instance,
	profiler_hub_property_category_t category,
	uint64_t row_key,
	profiler_hub_string_t property_tag,
	profiler_hub_optional_t value // OUT
) {
	profiler_hub_value_type_t type = kPprofilerHubDataTypeUndefined;
	try {
		type = profiler_hub::interface::GetProperty(trace, instance, category, row_key, property_tag, value);
	}
	catch (std::exception& e)
	{
		spdlog::error(error_fmt, e.what());
		type = kPprofilerHubDataTypeUndefined;
	}
	return type;
}

profiler_hub_result_t profiler_hub_get_time_slice(
	profiler_hub_future_handle_t future_handle,
	profiler_hub_trace_handle_t trace,
	profiler_hub_track_id_t track_id,
	uint64_t timestamp_start,
	uint64_t timestamp_end
) {
	profiler_hub_result_t result = kProfilerHubStatusUnknownError;
	try {
		result = profiler_hub::interface::GetTimeSlice(future_handle, trace, track_id, timestamp_start, timestamp_end);
	}
	catch (std::exception& e)
	{
		spdlog::error(error_fmt, e.what());
		result = kProfilerHubStatusUnknownError;
	}
	return result;
}

profiler_hub_result_t profiler_hub_get_table_time_slice(
	profiler_hub_future_handle_t future_handle,
	profiler_hub_trace_handle_t trace,
	profiler_hub_table_handle_t table_handle,
	profiler_hub_track_id_t track_id,
	uint64_t timestamp_start,
	uint64_t timestamp_end
) {
	profiler_hub_result_t result = kProfilerHubStatusUnknownError;
	try {
		result = profiler_hub::interface::GetTableTimeSlice(
			future_handle, 
			trace, 
			table_handle, 
			track_id, 
			timestamp_start, 
			timestamp_end);
	}
	catch (std::exception& e)
	{
		spdlog::error(error_fmt, e.what());
		result = kProfilerHubStatusUnknownError;
	}
	return result;
}

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
) {
	profiler_hub_result_t result = kProfilerHubStatusUnknownError;
	try {
			result = profiler_hub::interface::GetSearchTimeSlice(
				future_handle, 
				trace, 
				instance, 
				table_handle, 
				num_operations, 
				operations, 
				timestamp_start, 
				timestamp_end, 
				num_search_strings,
				string_filters);
	}
	catch (std::exception& e)
	{
		spdlog::error(error_fmt, e.what());
		result = kProfilerHubStatusUnknownError;
	}
	return result;
}

profiler_hub_result_t profiler_hub_get_event_data_flow(
	profiler_hub_future_handle_t future_handle,
	profiler_hub_trace_handle_t trace,
	profiler_hub_instance_id_t instance,
	profiler_hub_event_operation_t operation,
	profiler_hub_event_id_t event_id
) {
	profiler_hub_result_t result = kProfilerHubStatusUnknownError;
	try {
		result = profiler_hub::interface::GetEventDataFlow(
			future_handle, 
			trace, 
			instance, 
			operation, 
			event_id);
	}
	catch (std::exception& e)
	{
		spdlog::error(error_fmt, e.what());
		result = kProfilerHubStatusUnknownError;
	}
	return result;
}

profiler_hub_result_t profiler_hub_get_event_extended_data(
	profiler_hub_future_handle_t future_handle,
	profiler_hub_trace_handle_t trace,
	profiler_hub_instance_id_t instance,
	profiler_hub_event_operation_t operation,
	profiler_hub_event_id_t event_id
) {
	profiler_hub_result_t result = kProfilerHubStatusUnknownError;
	try {
		result = profiler_hub::interface::GetEventExtendedData(
			future_handle, 
			trace, 
			instance, 
			operation, 
			event_id);
	}
	catch (std::exception& e)
	{
		spdlog::error(error_fmt, e.what());
		result = kProfilerHubStatusUnknownError;
	}
	return result;
}

profiler_hub_result_t profiler_hub_get_event_stack_trace(
	profiler_hub_future_handle_t future_handle,
	profiler_hub_trace_handle_t trace,
	profiler_hub_instance_id_t instance,
	profiler_hub_event_operation_t operation,
	profiler_hub_event_id_t event_id
) {
	profiler_hub_result_t result = kProfilerHubStatusUnknownError;
	try {
		result = profiler_hub::interface::GetEventStackTrace(
			future_handle, 
			trace, 
			instance, 
			operation, 
			event_id);
	}
	catch (std::exception& e)
	{
		spdlog::error(error_fmt, e.what());
		result = kProfilerHubStatusUnknownError;
	}
	return result;
}

profiler_hub_result_t profiler_hub_trim_save_trace(
	profiler_hub_future_handle_t future_handle,
	profiler_hub_trace_handle_t trace,
	uint64_t timestamp_start,
	uint64_t timestamp_end) {
	profiler_hub_result_t result = kProfilerHubStatusUnknownError;
	try {
		result = profiler_hub::interface::TrimTraceDatabase(
			future_handle, 
			trace, 
			timestamp_start, 
			timestamp_end;
	}
	catch (std::exception& e)
	{
		spdlog::error(error_fmt, e.what());
		result = kProfilerHubStatusUnknownError;
	}
	return result;
}
