#include "profiler_hub_interface.h"
#include "profiler_hub_client_interface.h"
#include "profiler_hub_trace.h"
#include "profiler_hub_missing.hpp"
#include "profiler_hub_future.hpp"


namespace profiler_hub::interface
{
	profiler_hub_track_category_t profiler_hub_trace_t::get_track_category(profiler_hub::reader_types::track_type_t type, profiler_hub::reader_types::region_track_kind_t region_kind)
	{
		switch (type)
		{
		case profiler_hub::reader_types::track_type_t::cpu_thread: 
			if (region_kind == profiler_hub::reader_types::region_track_kind_t::main)
				return kPprofilerHubCategoryRegionInstrumented;
			else if (region_kind == profiler_hub::reader_types::region_track_kind_t::sample)
				return kPprofilerHubCategoryRegionSampled;
			else return kPprofilerHubCategoryUndefined;
		case profiler_hub::reader_types::track_type_t::gpu_queue: return kPprofilerHubCategoryKernelDispatch;
		case profiler_hub::reader_types::track_type_t::memory : return kPprofilerHubCategoryMemoryCopy;
		case profiler_hub::reader_types::track_type_t::memory_activity : return kPprofilerHubCategoryMemoryAllocate;
		case profiler_hub::reader_types::track_type_t::kernel_dispatch_pmc : return kPprofilerHubCategoryPerformanceCounter;
		case profiler_hub::reader_types::track_type_t::stream : return kPprofilerHubCategoryStream;
		default: return kPprofilerHubCategoryUndefined;
		}
	}

	profiler_hub_agent_type_t profiler_hub_trace_t::get_agent_type(std::string& type)
	{
		if (type == "CPU")
			return kProfilerHubAgentCPU;
		else if (type == "GPU")
			return kProfilerHubAgentGPU;
		else if (type == "NIC")
			return kProfilerHubAgentNIC;
		else
			return kProfilerHubNotAgent;
	}

    profiler_hub_value_type_t profiler_hub_trace_t::get_node_info_by_tag(size_t row_key, const char* property_tag, profiler_hub_optional_t value) {
        auto& nodes = get_reader()->get_all_nodes();
        auto it = std::find_if(nodes.begin(), nodes.end(), [row_key](profiler_hub::reader_types::node_info_ptr_t node_info) { return node_info->node_id == row_key; });
        if (it != nodes.end())
        {
            static const std::unordered_map<std::string, std::function<profiler_hub_value_type_t()>>
                properties_dispatcher =
            {
                { "id",            [it, value] { *value = &it->get()->node_id;              return kPprofilerHubDataTypeInt; }},
                { "hash",          [it, value] { *value = &it->get()->hash;                 return kPprofilerHubDataTypeInt; }},
                { "machine_id",    [it, value] { *value = it->get()->machine_id.c_str();    return kPprofilerHubDataTypeString; }},
                { "system_name",   [it, value] { *value = it->get()->system_name.c_str();   return kPprofilerHubDataTypeString; }},
                { "hostname",      [it, value] { *value = it->get()->hostname.c_str();      return kPprofilerHubDataTypeString; }},
                { "release",       [it, value] { *value = it->get()->release.c_str();       return kPprofilerHubDataTypeString; }},
                { "version",       [it, value] { *value = it->get()->version.c_str();       return kPprofilerHubDataTypeString; }},
                { "hardware_name", [it, value] { *value = it->get()->hardware_name.c_str(); return kPprofilerHubDataTypeString; }},
                { "domain_name",   [it, value] { *value = it->get()->domain_name.c_str();   return kPprofilerHubDataTypeString; }},
            };
            auto it = properties_dispatcher.find(property_tag);
            if (it != properties_dispatcher.end())
            {
                return it->second();
            }
        }
        return kPprofilerHubDataTypeUndefined;
    }

    profiler_hub_value_type_t profiler_hub_trace_t::get_process_info_by_tag(size_t row_key, const char* property_tag, profiler_hub_optional_t value) {
        auto& processes = get_reader()->get_all_processes();
        auto it = std::find_if(processes.begin(), processes.end(), [row_key](profiler_hub::reader_types::process_info_ptr_t process_info) { return process_info->pid == row_key; });
        if (it != processes.end())
        {
            static const std::unordered_map<std::string, std::function<profiler_hub_value_type_t()>>
                properties_dispatcher =
            {
                { "id",          [it, value] { *value = &it->get()->pid;                        return kPprofilerHubDataTypeInt; }},
                { "nid",         [it, value] { *value = &it->get()->node_info->node_id;         return kPprofilerHubDataTypeInt; }},
                { "init",        [it, value] { if (!it->get()->init.has_value())  return kPprofilerHubDataTypeUndefined; *value = &it->get()->init;  return kPprofilerHubDataTypeInt; }},
                { "fini",        [it, value] { if (!it->get()->fini.has_value())  return kPprofilerHubDataTypeUndefined; *value = &it->get()->fini;  return kPprofilerHubDataTypeInt; }},
                { "start",       [it, value] { if (!it->get()->start.has_value()) return kPprofilerHubDataTypeUndefined; *value = &it->get()->start; return kPprofilerHubDataTypeInt; }},
                { "end",         [it, value] { if (!it->get()->end.has_value())   return kPprofilerHubDataTypeUndefined; *value = &it->get()->end;   return kPprofilerHubDataTypeInt; }},
                { "command",     [it, value] { *value = it->get()->command.c_str();              return kPprofilerHubDataTypeString; }},
                { "environment", [it, value] { *value = it->get()->environment.c_str();          return kPprofilerHubDataTypeString; }},
                { "extdata",     [it, value] { *value = it->get()->extdata.c_str();              return kPprofilerHubDataTypeString; }},
            };
            auto it = properties_dispatcher.find(property_tag);
            if (it != properties_dispatcher.end())
            {
                return it->second();
            }
        }
        return kPprofilerHubDataTypeUndefined;
    }

    profiler_hub_value_type_t profiler_hub_trace_t::get_agent_info_by_tag(size_t row_key, const char* property_tag, profiler_hub_optional_t value) {
        auto& agents = get_reader()->get_all_agents();
        auto it = std::find_if(agents.begin(), agents.end(), [row_key](profiler_hub::reader_types::agent_info_ptr_t agent_info) { return agent_info->id == row_key; });
        if (it != agents.end())
        {
            static const std::unordered_map<std::string, std::function<profiler_hub_value_type_t()>>
                properties_dispatcher =
            {
                { "id",             [it, value] { *value = &it->get()->id;                          return kPprofilerHubDataTypeInt; }},
                { "nid",            [it, value] { *value = &it->get()->node_info->node_id;          return kPprofilerHubDataTypeInt; }},
                { "type_index",     [it, value] { *value = &it->get()->type_index;                  return kPprofilerHubDataTypeInt; }},
                { "absolute_index", [it, value] { if (!it->get()->absolute_index.has_value()) return kPprofilerHubDataTypeUndefined; *value = &it->get()->absolute_index; return kPprofilerHubDataTypeInt; }},
                { "logical_index",  [it, value] { if (!it->get()->logical_index.has_value())  return kPprofilerHubDataTypeUndefined; *value = &it->get()->logical_index;  return kPprofilerHubDataTypeInt; }},
                { "uuid",           [it, value] { if (!it->get()->uuid.has_value())           return kPprofilerHubDataTypeUndefined; *value = &it->get()->uuid;           return kPprofilerHubDataTypeInt; }},
                { "type",           [it, value] { *value = it->get()->agent_type.c_str();           return kPprofilerHubDataTypeString; }},
                { "name",           [it, value] { *value = it->get()->name.c_str();                 return kPprofilerHubDataTypeString; }},
                { "model_name",     [it, value] { *value = it->get()->model_name.c_str();           return kPprofilerHubDataTypeString; }},
                { "vendor_name",    [it, value] { *value = it->get()->vendor_name.c_str();          return kPprofilerHubDataTypeString; }},
                { "product_name",   [it, value] { *value = it->get()->product_name.c_str();         return kPprofilerHubDataTypeString; }},
                { "user_name",      [it, value] { *value = it->get()->user_name.c_str();            return kPprofilerHubDataTypeString; }},
                { "extdata",        [it, value] { *value = it->get()->extdata.c_str();              return kPprofilerHubDataTypeString; }},
            };
            auto it = properties_dispatcher.find(property_tag);
            if (it != properties_dispatcher.end())
            {
                return it->second();
            }
        }
        return kPprofilerHubDataTypeUndefined;
    }

    profiler_hub_value_type_t profiler_hub_trace_t::get_pmc_info_by_tag(size_t row_key, const char* property_tag, profiler_hub_optional_t value) {
        auto& pmcs = get_reader()->get_all_pmc_info();
        auto it = std::find_if(pmcs.begin(), pmcs.end(), [row_key](profiler_hub::reader_types::pmc_info_ptr_t pmc_info) { return pmc_info->pmc_id == row_key; });
        if (it != pmcs.end())
        {
            static const std::unordered_map<std::string, std::function<profiler_hub_value_type_t()>>
                properties_dispatcher =
            {
                { "id",               [it, value] { *value = &it->get()->pmc_id;                        return kPprofilerHubDataTypeInt; }},
                { "nid",              [it, value] { *value = &it->get()->node_info->node_id;            return kPprofilerHubDataTypeInt; }},
                { "pid",              [it, value] { *value = &it->get()->process_info->pid;             return kPprofilerHubDataTypeInt; }},
                { "agent_id",         [it, value] { *value = &it->get()->agent_info->id;                return kPprofilerHubDataTypeInt; }},
                { "event_code",       [it, value] { if (!it->get()->event_code.has_value())   return kPprofilerHubDataTypeUndefined; *value = &it->get()->event_code;   return kPprofilerHubDataTypeInt; }},
                { "instance_id",      [it, value] { if (!it->get()->instance_id.has_value())  return kPprofilerHubDataTypeUndefined; *value = &it->get()->instance_id;  return kPprofilerHubDataTypeInt; }},
                { "is_constant",      [it, value] { if (!it->get()->is_constant.has_value())  return kPprofilerHubDataTypeUndefined; *value = &it->get()->is_constant;  return kPprofilerHubDataTypeInt; }},
                { "is_derived",       [it, value] { if (!it->get()->is_derived.has_value())   return kPprofilerHubDataTypeUndefined; *value = &it->get()->is_derived;   return kPprofilerHubDataTypeInt; }},
                { "name",             [it, value] { *value = it->get()->name.c_str();                   return kPprofilerHubDataTypeString; }},
                { "target_arch",      [it, value] { *value = it->get()->target_arch.c_str();            return kPprofilerHubDataTypeString; }},
                { "symbol",           [it, value] { *value = it->get()->symbol.c_str();                 return kPprofilerHubDataTypeString; }},
                { "description",      [it, value] { *value = it->get()->description.c_str();            return kPprofilerHubDataTypeString; }},
                { "long_description", [it, value] { *value = it->get()->long_description.c_str();       return kPprofilerHubDataTypeString; }},
                { "component",        [it, value] { *value = it->get()->component.c_str();              return kPprofilerHubDataTypeString; }},
                { "units",            [it, value] { *value = it->get()->units.c_str();                  return kPprofilerHubDataTypeString; }},
                { "value_type",       [it, value] { *value = it->get()->value_type.c_str();             return kPprofilerHubDataTypeString; }},
                { "block",            [it, value] { *value = it->get()->block.c_str();                  return kPprofilerHubDataTypeString; }},
                { "expression",       [it, value] { *value = it->get()->expression.c_str();             return kPprofilerHubDataTypeString; }},
                { "extdata",          [it, value] { *value = it->get()->extdata.c_str();                return kPprofilerHubDataTypeString; }},
            };
            auto it = properties_dispatcher.find(property_tag);
            if (it != properties_dispatcher.end())
            {
                return it->second();
            }
        }
        return kPprofilerHubDataTypeUndefined;
    }

    profiler_hub_value_type_t profiler_hub_trace_t::get_thread_info_by_tag(size_t row_key, const char* property_tag, profiler_hub_optional_t value) {
        auto& threads = get_reader()->get_all_threads();
        auto it = std::find_if(threads.begin(), threads.end(), [row_key](profiler_hub::reader_types::thread_info_ptr_t thread_info) { return thread_info->thread_id == row_key; });
        if (it != threads.end())
        {
            static const std::unordered_map<std::string, std::function<profiler_hub_value_type_t()>>
                properties_dispatcher =
            {
                { "id",    [it, value] { *value = &it->get()->thread_id;                    return kPprofilerHubDataTypeInt; }},
                { "nid",   [it, value] { *value = &it->get()->node_info->node_id;           return kPprofilerHubDataTypeInt; }},
                { "pid",   [it, value] { *value = &it->get()->process_info->pid;            return kPprofilerHubDataTypeInt; }},
                { "ppid",  [it, value] { if (!it->get()->parent_process_id.has_value()) return kPprofilerHubDataTypeUndefined; *value = &it->get()->parent_process_id; return kPprofilerHubDataTypeInt; }},
                { "start", [it, value] { if (!it->get()->start.has_value()) return kPprofilerHubDataTypeUndefined; *value = &it->get()->start; return kPprofilerHubDataTypeInt; }},
                { "end",   [it, value] { if (!it->get()->end.has_value())   return kPprofilerHubDataTypeUndefined; *value = &it->get()->end;   return kPprofilerHubDataTypeInt; }},
                { "name",    [it, value] { *value = it->get()->name.c_str();                return kPprofilerHubDataTypeString; }},
                { "extdata", [it, value] { *value = it->get()->extdata.c_str();             return kPprofilerHubDataTypeString; }},
            };
            auto it = properties_dispatcher.find(property_tag);
            if (it != properties_dispatcher.end())
            {
                return it->second();
            }
        }
        return kPprofilerHubDataTypeUndefined;
    }

    profiler_hub_value_type_t profiler_hub_trace_t::get_stream_info_by_tag(size_t row_key, const char* property_tag, profiler_hub_optional_t value) {
        auto& streams = get_reader()->get_all_streams();
        auto it = std::find_if(streams.begin(), streams.end(), [row_key](profiler_hub::reader_types::stream_info_ptr_t stream_info) { return stream_info->stream_id == row_key; });
        if (it != streams.end())
        {
            static const std::unordered_map<std::string, std::function<profiler_hub_value_type_t()>>
                properties_dispatcher =
            {
                { "id",      [it, value] { *value = &it->get()->stream_id;              return kPprofilerHubDataTypeInt; }},
                { "nid",     [it, value] { *value = &it->get()->node_info->node_id;     return kPprofilerHubDataTypeInt; }},
                { "pid",     [it, value] { *value = &it->get()->process_info->pid;      return kPprofilerHubDataTypeInt; }},
                { "name",    [it, value] { *value = it->get()->name.c_str();            return kPprofilerHubDataTypeString; }},
                { "extdata", [it, value] { *value = it->get()->extdata.c_str();         return kPprofilerHubDataTypeString; }},
            };
            auto it = properties_dispatcher.find(property_tag);
            if (it != properties_dispatcher.end())
            {
                return it->second();
            }
        }
        return kPprofilerHubDataTypeUndefined;
    }

    profiler_hub_value_type_t profiler_hub_trace_t::get_queue_info_by_tag(size_t row_key, const char* property_tag, profiler_hub_optional_t value) {
        auto& queues = get_reader()->get_all_queues();
        auto it = std::find_if(queues.begin(), queues.end(), [row_key](profiler_hub::reader_types::queue_info_ptr_t queue_info) { return queue_info->queue_id == row_key; });
        if (it != queues.end())
        {
            static const std::unordered_map<std::string, std::function<profiler_hub_value_type_t()>>
                properties_dispatcher =
            {
                { "id",      [it, value] { *value = &it->get()->queue_id;               return kPprofilerHubDataTypeInt; }},
                { "nid",     [it, value] { *value = &it->get()->node_info->node_id;     return kPprofilerHubDataTypeInt; }},
                { "pid",     [it, value] { *value = &it->get()->process_info->pid;      return kPprofilerHubDataTypeInt; }},
                { "name",    [it, value] { *value = it->get()->name.c_str();            return kPprofilerHubDataTypeString; }},
                { "extdata", [it, value] { *value = it->get()->extdata.c_str();         return kPprofilerHubDataTypeString; }},
            };
            auto it = properties_dispatcher.find(property_tag);
            if (it != properties_dispatcher.end())
            {
                return it->second();
            }
        }
        return kPprofilerHubDataTypeUndefined;
    }

    profiler_hub_db_type_t profiler_hub_trace_t::detect_trace(std::string trace_path) {
		return missing_t::detect_trace(trace_path);
	}

	profiler_hub_result_t profiler_hub_trace_t::open_trace(future_t * future) {
        missing_t::check_missing_client(m_client_trace);
        profiler_hub_result_t result = kProfilerHubStatusInvalidArgument;
		// initialize and compile trace metadata
        future->show_progress(m_trace_path.c_str(), 80, "Read trace", kProfilerHubAsyncBusy);
        m_storage = std::make_unique<profiler_hub::storage_t>(m_trace_path, "");
        m_reader = std::make_shared<profiler_hub::reader_t>(std::move(m_storage));

        future->show_progress(m_trace_path.c_str(), 1, "Add instances", kProfilerHubAsyncBusy);
		auto& trace_instances = missing_t::get_trace_instances();
		for (auto& instance : trace_instances)
		{
            result = client::interface::AddInstance(m_client_trace, instance.get_index(), instance.get_file(), instance.get_guid());
            if (result != kProfilerHubStatusSuccess)
            {
                return result;
            }
		}

        future->show_progress(m_trace_path.c_str(), 1, "Add strings", kProfilerHubAsyncBusy);
		auto & string_table = missing_t::get_trace_string_table();
		for (auto& [id, string] : string_table)
		{
			result = client::interface::AddString(m_client_trace, string.c_str(), id);
            if (result != kProfilerHubStatusSuccess)
            {
                return result;
            }
		}

        future->show_progress(m_trace_path.c_str(), 3, "Add tracks", kProfilerHubAsyncBusy);

		auto & tracks = m_reader->get_tracks();
		for (auto& track : tracks)
		{
			result = client::interface::AddTrack(
				m_client_trace,
				missing_t::integer("track->instance_info->id"),
				track->id.value,
				track->name.c_str(),
				get_track_category(track->type, track->region_kind),
				&track->node_info->node_id,
				track->node_info->hostname.c_str(),
				&track->process_info->pid,
				track->process_info->command.c_str(),
				track->thread_info ? &track->thread_info->thread_id : nullptr,
				nullptr,
				track->stream_info ? &track->stream_info->stream_id : nullptr,
				track->stream_info ? track->stream_info->name.c_str() : nullptr,
				track->agent_info ? get_agent_type(track->agent_info->agent_type) : kProfilerHubNotAgent,
				track->agent_info ? &track->agent_info->type_index : nullptr,
				track->agent_info ? track->agent_info->name.c_str() : nullptr,
				track->queue_info ? &track->queue_info->queue_id : nullptr,
				track->queue_info ? track->queue_info->name.c_str() : nullptr,
				track->pmc_info ? &track->pmc_info->pmc_id : nullptr,
				track->pmc_info ? track->pmc_info->name.c_str() : nullptr,
				missing_t::integer("track->records_count"), // records has to be counted using COUNT(*)
				missing_t::integer("track->min_timestamp"), // min_timestamp has to be aggregated with MIN(start_ts)
				missing_t::integer("track->max_timestamp"), // max_timestamp has to be aggregated with MAX(end_ts),
				0, // in case of counter track use MIN(value)
				track->max_lane //in case of counter track use MAX(value)
			);
            if (result != kProfilerHubStatusSuccess)
            {
                return result;
            }
		}

        future->show_progress(m_trace_path.c_str(), 3, "Add track histograms", kProfilerHubAsyncBusy);
		for (auto& track : tracks)
		{
			auto & track_histogram = missing_t::get_track_histogram(track, m_histogram_bucket_count);
			for (auto& bucket : track_histogram)
			{
				result = client::interface::AddTrackHistogramBucket(m_client_trace, track->id.value, bucket.get_bucket_number(), bucket.get_events_count(), bucket.get_bucket_value());
                if (result != kProfilerHubStatusSuccess)
                {
                    return result;
                }
			}	
		}
        return result;
	}

    profiler_hub_result_t 
        profiler_hub_trace_t::get_time_slice(
            future_t * future,
            profiler_hub_track_id_t track_id,
            uint64_t timestamp_start,
            uint64_t timestamp_end)
    {
        if (!m_client_trace)
        {
            throw missing_error_t("fatal error: client trace cannot be null!");
        }
        profiler_hub_result_t result = kProfilerHubStatusNotLoaded;
        reader_types::event_filter_t filter = { {timestamp_start, timestamp_end} };
        auto& tracks = m_reader->get_tracks();
        auto it = std::find_if(tracks.begin(), tracks.end(), [track_id](reader_types::track_info_ptr_t track_info) {return track_info->id.value == track_id; });
        if (it != tracks.end())
        {
            auto& events = m_reader->get_events_for_track(*it, filter);
            auto slice_container = client::interface::AddTimeSliceContainer(m_client_trace, track_id, timestamp_start, timestamp_end);
            for (auto& event : events)
            {
                result = client::interface::AddEventRecord(
                    slice_container,
                    event.unique_identifier.type,
                    event.unique_identifier.id,
                    event.start_timestamp,
                    event.end_timestamp,
                    missing_t::integer("event.category_id"),
                    missing_t::integer("event.symbol_id"),
                    missing_t::integer("event.level"));
                if (result != kProfilerHubStatusSuccess)
                {
                    return result;
                }
            }
        }
        return result;
    }

    profiler_hub_result_t profiler_hub_trace_t::add_table_to_client(missing_types::table_ptr_t table, profiler_hub_table_handle_t client_table_handle)
    {
        profiler_hub_result_t result = kProfilerHubStatusNotLoaded;
        for (auto& column : table->columns)
        {
            result = client::interface::AddTableColumn(client_table_handle, column.name.c_str(), column.type);
            if (result != kProfilerHubStatusSuccess)
            {
                return result;
            }
        }
        for (auto& row : table->rows)
        {
            if (row.cells.size() != table->columns.size())
                return kProfilerHubStatusUnknownError;
            profiler_hub_table_row_handle_t row_handle = client::interface::AddTableRowContainer(client_table_handle);
            for (int i = 0; i < row.cells.size(); i++)
            {
                if (std::holds_alternative<std::string>(row.cells[i].value))
                {
                    result = client::interface::AddTableCellAsString(row_handle, i, std::get<std::string>(row.cells[i].value).c_str());
                } else
                    if (std::holds_alternative<size_t>(row.cells[i].value))
                    {
                        result = client::interface::AddTableCellAsInt(row_handle, i, std::get<size_t>(row.cells[i].value));
                    } else
                        if (std::holds_alternative<double>(row.cells[i].value))
                        {
                            result = client::interface::AddTableCellAsDouble(row_handle, i, std::get<double>(row.cells[i].value));
                        }

                    if (result != kProfilerHubStatusSuccess)
                    {
                        return result;
                    }
            }
        }
        return result;
    }

    profiler_hub_result_t 
        profiler_hub_trace_t::get_table_time_slice(
            future_t * future,
            profiler_hub_table_handle_t client_table_handle,
            profiler_hub_track_id_t track_id,
            uint64_t timestamp_start,
            uint64_t timestamp_end)
    {
        missing_types::event_filter_t filter = { {timestamp_start, timestamp_end} };
        auto& tracks = m_reader->get_tracks();
        auto it = std::find_if(tracks.begin(), tracks.end(), [track_id](reader_types::track_info_ptr_t track_info) {return track_info->id.value == track_id; });
        if (it != tracks.end())
        {
            auto& table = missing_t::get_event_table_for_track(*it, filter, timestamp_start, timestamp_end);
            
            return add_table_to_client(table, client_table_handle);
        }
        return kProfilerHubStatusNotLoaded;
    }

    profiler_hub_result_t 
        profiler_hub_trace_t::get_search_time_slice(
            future_t * future,
            profiler_hub_table_handle_t client_table_handle,
            size_t num_operations,
            profiler_hub_event_operation_t* operations,
            uint64_t timestamp_start,
            uint64_t timestamp_end,
            size_t num_search_strings,
            profiler_hub_search_strings_t string_filters)
    {
        std::vector<reader_types::event_type_t> op_types;
        for (int i = 0; i < num_operations; i++)
        {
            op_types.push_back(operations[i]);
        }
        std::vector<std::string> search_strings;
        for (int i = 0; i < num_search_strings; i++)
        {
            search_strings.push_back(string_filters[i]);
        }
        missing_types::event_filter_t filter = { {timestamp_start, timestamp_end}, {}, {}, op_types, search_strings };
        
        auto& table = missing_t::get_event_table(filter, timestamp_start, timestamp_end);
        return add_table_to_client(table, client_table_handle);
    }

    profiler_hub_result_t profiler_hub_trace_t::get_data_flow_for_event(
        future_t* future,
        profiler_hub_instance_id_t instance,
        profiler_hub_event_operation_t operation,
        profiler_hub_event_id_t event_id) {
        missing_t::check_missing_client(m_client_trace);
        profiler_hub_result_t result = kProfilerHubStatusNotLoaded;
        auto & flow_endpoints = m_reader->get_flows_for_event(reader_types::detail::event_id_access::make(operation, event_id));
        profiler_hub_flowtrace_handle_t flow_container = client::interface::AddEventFlowTraceContainer(m_client_trace, instance, operation, event_id);
        for (auto& endpoint : flow_endpoints)
        {
            size_t dest_id = reader_types::detail::event_id_access::row_id(endpoint.dest);
            profiler_hub_event_operation_t dest_type = reader_types::detail::event_id_access::type(endpoint.dest);
            size_t src_id = reader_types::detail::event_id_access::row_id(endpoint.source);
            profiler_hub_event_operation_t src_type = reader_types::detail::event_id_access::type(endpoint.source);
            if (event_id == src_id)
            {
                result = client::interface::AddEventDataFlowEndPoint(
                    flow_container,
                    dest_type,
                    dest_id,
                    kProfilerHubDirectionOutgoing,
                    missing_t::integer("reader_types::detail::event_id_access::timestamp(endpoint.dest)"),
                    missing_t::integer("reader_types::detail::event_id_access::duration(endpoint.dest)"),
                    missing_t::integer("reader_types::detail::event_id_access::category_id(endpoint.dest)"),
                    missing_t::integer("reader_types::detail::event_id_access::symbol_id(endpoint.dest)"),
                    missing_t::integer("reader_types::detail::event_id_access::level(endpoint.dest)"));
                if (kProfilerHubStatusSuccess != result)
                    return result;
            }
            else if (event_id == dest_id)
            {
                result = client::interface::AddEventDataFlowEndPoint(
                    flow_container,
                    src_type,
                    src_id,
                    kProfilerHubDirectionIncoming,
                    missing_t::integer("reader_types::detail::event_id_access::timestamp(endpoint.src)"),
                    missing_t::integer("reader_types::detail::event_id_access::duration(endpoint.src)"),
                    missing_t::integer("reader_types::detail::event_id_access::category_id(endpoint.src)"),
                    missing_t::integer("reader_types::detail::event_id_access::symbol_id(endpoint.src)"),
                    missing_t::integer("reader_types::detail::event_id_access::level(endpoint.src)"));
                if (kProfilerHubStatusSuccess != result)
                    return result;
            }
            else
            {
                return kProfilerHubStatusUnknownError;
            }
        }
        return result;
    }

    profiler_hub_result_t profiler_hub_trace_t::get_event_details(
        future_t* future,
        profiler_hub_instance_id_t instance,
        profiler_hub_event_operation_t operation,
        profiler_hub_event_id_t event_id) {
        missing_t::check_missing_client(m_client_trace);
        profiler_hub_result_t result = kProfilerHubStatusNotLoaded;
        auto & event_info = m_reader->get_event_info(reader_types::detail::event_id_access::make(operation, event_id));
        if (event_info.has_value())
        {
            profiler_hub_ext_data_handle_t event_info_container = client::interface::AddEventExtDataContainer(m_client_trace, instance, operation, event_id);
            for (auto& prop : event_info->properties)
            {
                if (std::holds_alternative<std::string>(prop.value))
                {
                    result = client::interface::AddEventExtendedInfo(event_info_container, "profiler-hub", prop.key.c_str(), kPprofilerHubDataTypeString, std::get<std::string>(prop.value).c_str());
                } else
                    if (std::holds_alternative<size_t>(prop.value))
                    {
                        result = client::interface::AddEventExtendedInfo(event_info_container, "profiler-hub", prop.key.c_str(), kPprofilerHubDataTypeInt, std::to_string(std::get<uint64_t>(prop.value)).c_str());
                    } else
                        if (std::holds_alternative<double>(prop.value))
                        {
                            result = client::interface::AddEventExtendedInfo(event_info_container, "profiler-hub", prop.key.c_str(), kPprofilerHubDataTypeInt, std::to_string(std::get<double>(prop.value)).c_str());
                        }
                        else
                        {
                            result = kProfilerHubStatusUnknownError;
                        }

                    if (kProfilerHubStatusSuccess != result)
                        return result;
            }
            result = client::interface::AddEventEssentialInfo(event_info_container,
                missing_t::integer("event_info->track->id.value"),
                missing_t::integer("event_info->stream_track->id.value"),
                missing_t::integer("event_info->level"),
                missing_t::integer("event_info->stream_track_level")
            );
            if (kProfilerHubStatusSuccess != result)
                return result;
            auto event_arguments = m_reader->get_arguments(reader_types::detail::event_id_access::make(operation, event_id));
            for (auto& arg : event_arguments)
            {
                result = client::interface::AddEventArgumentsInfo(event_info_container, arg->position, arg->name.c_str(), arg->type.c_str(), arg->value.c_str());
                if (kProfilerHubStatusSuccess != result)
                    return result;
            }

        }
        else
        {
            return kProfilerHubStatusNotLoaded;
        }
        return result;
    }

    profiler_hub_result_t profiler_hub_trace_t::get_event_stack_trace(
        future_t* future,
        profiler_hub_instance_id_t instance,
        profiler_hub_event_operation_t operation,
        profiler_hub_event_id_t event_id) {
        missing_t::check_missing_client(m_client_trace);
        profiler_hub_result_t result = kProfilerHubStatusNotLoaded;
        profiler_hub_call_stack_handle_t call_stack_container = client::interface::AddEventCallStackContainer(m_client_trace, instance, operation, event_id);
        auto & call_stack = m_reader->get_call_stack(reader_types::detail::event_id_access::make(operation, event_id));
        size_t depth = 0;
        for (auto& stack_frame : call_stack)
        {
            if (stack_frame.program_counter.has_value())
            {
                result = client::interface::AddEventCallStackFrame(
                    call_stack_container,
                    stack_frame.program_counter->function.c_str(),
                    stack_frame.program_counter->filename.c_str(),
                    stack_frame.program_counter->line_number.has_value() ? std::to_string(stack_frame.program_counter->line_number.value()).c_str() : "",
                    stack_frame.address_range.has_value() ? std::to_string(stack_frame.address_range->address_base).c_str() : "",
                    depth++);
                if (kProfilerHubStatusSuccess != result)
                    return result;
            }
        }
        return result;
    }


    profiler_hub_result_t trim_trace_database(
        future_t* future,
        uint64_t timestamp_start,
        uint64_t timestamp_end)
    {
        return missing_t::trim_trace_database(timestamp_start, timestamp_end);
    }
    
}
