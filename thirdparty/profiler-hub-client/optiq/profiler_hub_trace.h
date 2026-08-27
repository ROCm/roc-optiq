#include "profiler_hub_interface.h"
#include "profiler_hub_client_interface.h"
#include "profiler-hub/reader.hpp"
#include "profiler-hub/storage.hpp"
#include "profiler_hub_missing.hpp"
#include "profiler_hub_future.hpp"

namespace profiler_hub::interface
{
	class profiler_hub_trace_t
	{
	public:
		profiler_hub_trace_t(std::string trace_path) :
			m_client_trace(nullptr), m_trace_path(trace_path), m_config_path(nullptr), m_histogram_bucket_count(300) {
		}

		void set_trace_properties(client_trace_handle_t client_trace, std::string config_path, size_t histogram_bucket_count) { 
			m_client_trace = client_trace; m_config_path = config_path; m_histogram_bucket_count = histogram_bucket_count;
		}

		profiler_hub_result_t 
			open_trace(
				future_t * future);

		profiler_hub_result_t 
			get_time_slice(
				future_t * future,
				profiler_hub_track_id_t track_id,
				uint64_t timestamp_start,
				uint64_t timestamp_end);

		profiler_hub_result_t
			get_table_time_slice(
				future_t * future,
				profiler_hub_table_handle_t table_handle,
				profiler_hub_track_id_t track_id,
				uint64_t timestamp_start,
				uint64_t timestamp_end);

		profiler_hub_result_t
			get_search_time_slice(
				future_t * future,
				profiler_hub_table_handle_t table_handle,
				size_t num_operations,
				profiler_hub_event_operation_t* operations,
				uint64_t timestamp_start,
				uint64_t timestamp_end,
				size_t num_search_strings,
				profiler_hub_search_strings_t string_filters);

		profiler_hub_result_t 
			get_data_flow_for_event(
				future_t* future, 
				profiler_hub_instance_id_t instance, 
				profiler_hub_event_operation_t operation, 
				profiler_hub_event_id_t event_id);

		profiler_hub_result_t 
			get_event_details(
				future_t* future, 
				profiler_hub_instance_id_t instance, 
				profiler_hub_event_operation_t operation, 
				profiler_hub_event_id_t event_id);

		profiler_hub_result_t 
			get_event_stack_trace(
				future_t* future,
				profiler_hub_instance_id_t instance,
				profiler_hub_event_operation_t operation,
				profiler_hub_event_id_t event_id);


		static profiler_hub_db_type_t 
			detect_trace(std::string trace_path);

		profiler_hub_result_t trim_trace_database(
			future_t* future, 
			uint64_t timestamp_start, 
			uint64_t timestamp_end);

		profiler_hub::storage_t* get_storage() { return m_storage.get(); }
		profiler_hub::reader_t* get_reader() { return m_reader.get(); }

		profiler_hub_value_type_t get_node_info_by_tag(size_t row_key, const char* property_tag, profiler_hub_optional_t value);
		profiler_hub_value_type_t get_process_info_by_tag(size_t row_key, const char* property_tag, profiler_hub_optional_t value);
		profiler_hub_value_type_t get_thread_info_by_tag(size_t row_key, const char* property_tag, profiler_hub_optional_t value);
		profiler_hub_value_type_t get_agent_info_by_tag(size_t row_key, const char* property_tag, profiler_hub_optional_t value);
		profiler_hub_value_type_t get_queue_info_by_tag(size_t row_key, const char* property_tag, profiler_hub_optional_t value);
		profiler_hub_value_type_t get_stream_info_by_tag(size_t row_key, const char* property_tag, profiler_hub_optional_t value);
		profiler_hub_value_type_t get_pmc_info_by_tag(size_t row_key, const char* property_tag, profiler_hub_optional_t value);
		
	private:
		// Type conversion can be avoided if types match completely 
		// Currently there is no corresponded track type for kProfilerHubCategoryRegionSampled on profiler hub side
		// And there is no "dma" type of track on rocOptiq side
		profiler_hub_track_category_t get_track_category(profiler_hub::reader_types::track_type_t type, profiler_hub::reader_types::region_track_kind_t region_kind);
		profiler_hub_agent_type_t get_agent_type(std::string& type);
		profiler_hub_result_t add_table_to_client(missing_types::table_ptr_t table, profiler_hub_table_handle_t client_table_handle);

	private:
		client_trace_handle_t m_client_trace;
		std::string m_trace_path;
		std::string m_config_path;
		size_t m_histogram_bucket_count;
		std::unique_ptr<profiler_hub::storage_t> m_storage;
		std::shared_ptr<profiler_hub::reader_t>  m_reader;
	};

}
