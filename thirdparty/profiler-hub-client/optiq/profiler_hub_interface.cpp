#include "profiler_hub_trace.h"
#include "profiler_hub_interface.h"
#include "profiler_hub_future.hpp"


namespace profiler_hub::interface
{

	profiler_hub_future_handle_t FutureAlloc(progress_callback_t progress_callback) {
		return new future_t(progress_callback, nullptr);
	}

	profiler_hub_result_t FutureFree(
		profiler_hub_future_handle_t future_handle
	) {
		if (!future_handle)
			return kProfilerHubStatusInvalidArgument;
		future_t* future = static_cast<future_t*>(future_handle);
		delete future;
		return kProfilerHubStatusSuccess;		
	}

	profiler_hub_result_t FutureWait(
		profiler_hub_future_handle_t future_handle,
		uint64_t timeout_ms
	) {
		if (!future_handle)
			return kProfilerHubStatusInvalidArgument;
		future_t* future = static_cast<future_t*>(future_handle);
		return future->wait_for_completion(timeout_ms);
	}

	profiler_hub_result_t FutureCancel(
		profiler_hub_future_handle_t future_handle
	) {
		if (!future_handle)
			return kProfilerHubStatusInvalidArgument;
		future_t* future = static_cast<future_t*>(future_handle);
		future->set_interrupted();
		return kProfilerHubStatusSuccess;
	}

	profiler_hub_db_type_t DetectTrace(
		profiler_hub_string_t trace_file_path
	) {

		return profiler_hub_trace_t::detect_trace(trace_file_path);
	}

	profiler_hub_trace_handle_t OpenTrace(
		profiler_hub_string_t trace_file_path	
		)
	{
		profiler_hub_trace_t* ph_trace = new profiler_hub_trace_t(trace_file_path);
		return (profiler_hub_trace_handle_t)ph_trace;
	}

	profiler_hub_result_t SetTraceProperties(
		profiler_hub_trace_handle_t trace,
		client_trace_handle_t client_trace,
		profiler_hub_string_t config_dir_path,
		size_t histogram_bucket_count
	)
	{
		profiler_hub_trace_t* ph_trace = static_cast<profiler_hub_trace_t*>(trace);
		if (ph_trace == nullptr)
		{
			return kProfilerHubStatusInvalidArgument;
		}
		ph_trace->set_trace_properties(client_trace, config_dir_path, histogram_bucket_count);
	}

	profiler_hub_result_t ReadTraceMetadata(
		profiler_hub_future_handle_t future_handle,
		profiler_hub_trace_handle_t trace
	) 
	{
		future_t* future = static_cast<future_t*>(future_handle);
		if (future == nullptr)
		{
			return kProfilerHubStatusInvalidArgument;
		}
		profiler_hub_trace_t* ph_trace = static_cast<profiler_hub_trace_t*>(trace);
		if (ph_trace == nullptr)
		{
			return kProfilerHubStatusInvalidArgument;
		}
		std::thread worker([ph_trace, future]()
			{
				future->set_promise(ph_trace->open_trace(future));
			});
		future->set_worker(std::move(worker));
		return kProfilerHubStatusSuccess;
	}

	profiler_hub_result_t CloseTrace(
		profiler_hub_trace_handle_t trace
	) {
		profiler_hub_trace_t* ph_trace = static_cast<profiler_hub_trace_t*>(trace);
		if (ph_trace == nullptr)
		{
			return kProfilerHubStatusInvalidArgument;
		}
		delete ph_trace;
		return kProfilerHubStatusSuccess;
	}

	profiler_hub_value_type_t GetProperty(
		profiler_hub_trace_handle_t trace,
		profiler_hub_instance_id_t instance,
		profiler_hub_property_category_t category,
		uint64_t row_key,
		profiler_hub_string_t property_tag,
		profiler_hub_optional_t value // OUT
	) {
		profiler_hub_trace_t* ph_trace = static_cast<profiler_hub_trace_t*>(trace);
		if (ph_trace == nullptr)
		{
			return kPprofilerHubDataTypeUndefined;
		}
		if (value == nullptr)
		{
			return kPprofilerHubDataTypeUndefined;
		}

		switch (category)
		{
		case kProfilerHubPropertyNode:
			return ph_trace->get_node_info_by_tag(row_key, property_tag, value);
		case kProfilerHubPropertyProcess:
			return ph_trace->get_process_info_by_tag(row_key, property_tag, value);
		case kProfilerHubPropertyThread:
			return ph_trace->get_thread_info_by_tag(row_key, property_tag, value);
		case kProfilerHubPropertyAgent:
			return ph_trace->get_agent_info_by_tag(row_key, property_tag, value);
		case kProfilerHubPropertyQueue:
			return ph_trace->get_queue_info_by_tag(row_key, property_tag, value);
		case kProfilerHubPropertyStream:
			return ph_trace->get_stream_info_by_tag(row_key, property_tag, value);
		case kProfilerHubPropertyCounter:
			return ph_trace->get_pmc_info_by_tag(row_key, property_tag, value);
		}

		return kPprofilerHubDataTypeUndefined;
		
	}

	profiler_hub_result_t GetTimeSlice(
		profiler_hub_future_handle_t future_handle,
		profiler_hub_trace_handle_t trace,
		profiler_hub_track_id_t track_id,
		uint64_t timestamp_start,
		uint64_t timestamp_end
	) {
		future_t* future = static_cast<future_t*>(future_handle);
		if (future == nullptr)
		{
			return kProfilerHubStatusInvalidArgument;
		}
		profiler_hub_trace_t* ph_trace = static_cast<profiler_hub_trace_t*>(trace);
		if (ph_trace == nullptr)
		{
			return kProfilerHubStatusInvalidArgument;
		}
		std::thread worker([ph_trace, future, track_id, timestamp_start, timestamp_end]()
			{
				future->set_promise(ph_trace->get_time_slice(future, track_id, timestamp_start, timestamp_end));
			});
		future->set_worker(std::move(worker));
		return kProfilerHubStatusSuccess;
	}

	profiler_hub_result_t GetTableTimeSlice(
		profiler_hub_future_handle_t future_handle,
		profiler_hub_trace_handle_t trace,
		profiler_hub_table_handle_t table_handle,
		profiler_hub_track_id_t track_id,
		uint64_t timestamp_start,
		uint64_t timestamp_end
	) {
		future_t* future = static_cast<future_t*>(future_handle);
		if (future == nullptr)
		{
			return kProfilerHubStatusInvalidArgument;
		}
		profiler_hub_trace_t* ph_trace = static_cast<profiler_hub_trace_t*>(trace);
		if (ph_trace == nullptr)
		{
			return kProfilerHubStatusInvalidArgument;
		}
		std::thread worker([ph_trace, future, table_handle, track_id, timestamp_start, timestamp_end]()
			{
				future->set_promise(ph_trace->get_table_time_slice(future, table_handle, track_id, timestamp_start, timestamp_end));
			});
		future->set_worker(std::move(worker));
		return kProfilerHubStatusSuccess;
	}

	profiler_hub_result_t GetSearchTimeSlice(
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
		future_t* future = static_cast<future_t*>(future_handle);
		if (future == nullptr)
		{
			return kProfilerHubStatusInvalidArgument;
		}
		profiler_hub_trace_t* ph_trace = static_cast<profiler_hub_trace_t*>(trace);
		if (ph_trace == nullptr)
		{
			return kProfilerHubStatusInvalidArgument;
		}
		std::thread worker([ph_trace, future, table_handle, num_operations, operations, timestamp_start, timestamp_end, num_search_strings, string_filters]()
			{
				future->set_promise(ph_trace->get_search_time_slice(future, table_handle, num_operations, operations, timestamp_start, timestamp_end, num_search_strings, string_filters));
			});
		future->set_worker(std::move(worker));
		return kProfilerHubStatusSuccess;

	}

	profiler_hub_result_t GetEventDataFlow(
		profiler_hub_future_handle_t future_handle,
		profiler_hub_trace_handle_t trace,
		profiler_hub_instance_id_t instance,
		profiler_hub_event_operation_t operation,
		profiler_hub_event_id_t event_id
	) {
		future_t* future = static_cast<future_t*>(future_handle);
		if (future == nullptr)
		{
			return kProfilerHubStatusInvalidArgument;
		}
		profiler_hub_trace_t* ph_trace = static_cast<profiler_hub_trace_t*>(trace);
		if (ph_trace == nullptr)
		{
			return kProfilerHubStatusInvalidArgument;
		}
		std::thread worker([ph_trace, future, instance, operation, event_id]()
			{
				future->set_promise(ph_trace->get_data_flow_for_event(future, instance, operation, event_id));
			});
		future->set_worker(std::move(worker));
		return kProfilerHubStatusSuccess;
	}
	
	profiler_hub_result_t GetEventExtendedData(
		profiler_hub_future_handle_t future_handle,
		profiler_hub_trace_handle_t trace,
		profiler_hub_instance_id_t instance,
		profiler_hub_event_operation_t operation,
		profiler_hub_event_id_t event_id
	) {
		future_t* future = static_cast<future_t*>(future_handle);
		if (future == nullptr)
		{
			return kProfilerHubStatusInvalidArgument;
		}
		profiler_hub_trace_t* ph_trace = static_cast<profiler_hub_trace_t*>(trace);
		if (ph_trace == nullptr)
		{
			return kProfilerHubStatusInvalidArgument;
		}
		std::thread worker([ph_trace, future, instance, operation, event_id]()
			{
				future->set_promise(ph_trace->get_event_details(future, instance, operation, event_id));
			});
		future->set_worker(std::move(worker));
		return kProfilerHubStatusSuccess;
	}

	profiler_hub_result_t GetEventStackTrace(
		profiler_hub_future_handle_t future_handle,
		profiler_hub_trace_handle_t trace,
		profiler_hub_instance_id_t instance,
		profiler_hub_event_operation_t operation,
		profiler_hub_event_id_t event_id
	) {
		future_t* future = static_cast<future_t*>(future_handle);
		if (future == nullptr)
		{
			return kProfilerHubStatusInvalidArgument;
		}
		profiler_hub_trace_t* ph_trace = static_cast<profiler_hub_trace_t*>(trace);
		if (ph_trace == nullptr)
		{
			return kProfilerHubStatusInvalidArgument;
		}
		std::thread worker([ph_trace, future, instance, operation, event_id]()
			{
				future->set_promise(ph_trace->get_event_stack_trace(future, instance, operation, event_id));
			});
		future->set_worker(std::move(worker));
		return kProfilerHubStatusSuccess;
	}

	profiler_hub_result_t TrimTraceDatabase(
		profiler_hub_future_handle_t future_handle,
		profiler_hub_trace_handle_t trace,
		uint64_t timestamp_start,
		uint64_t timestamp_end)
	{
		future_t* future = static_cast<future_t*>(future_handle);
		if (future == nullptr)
		{
			return kProfilerHubStatusInvalidArgument;
		}
		profiler_hub_trace_t* ph_trace = static_cast<profiler_hub_trace_t*>(trace);
		if (ph_trace == nullptr)
		{
			return kProfilerHubStatusInvalidArgument;
		}
		std::thread worker([ph_trace, future, timestamp_start, timestamp_end]()
			{
				future->set_promise(ph_trace->trim_trace_database(future, timestamp_start, timestamp_end));
			});
		future->set_worker(std::move(worker));
		return kProfilerHubStatusSuccess;
	}

}
