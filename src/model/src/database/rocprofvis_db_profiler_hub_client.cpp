#include "profiler_hub_client_interface.h"
#include "rocprofvis_db_profiler_hub.h"
#include "rocprofvis_db_profiler_hub_client.h"
#include "rocprofvis_db_query_builder.h"

namespace RocProfVis::DataModel
{


    profiler_hub_result_t ProfilerHubClientMethods::AddInstance(
        client_trace_handle_t trace,
        profiler_hub_instance_id_t id,
        profiler_hub_string_t file,
        profiler_hub_string_t uuid
    ) {
        ProfilerHub* db = (ProfilerHub*)trace;
        uint32_t file_id = db->DbFiles().ToInt(file);
        db->DbInstances().push_back({ DbInstance(file_id, static_cast<uint32_t>(db->DbInstances().size())), uuid });
        return kProfilerHubStatusSuccess;
    }

    profiler_hub_result_t ProfilerHubClientMethods::AddString(
        client_trace_handle_t trace,
        profiler_hub_string_t string,
        uint32_t string_id

    ) {
        ProfilerHub* db = (ProfilerHub*)trace;
        uint32_t string_index = db->BindObject()->FuncAddString(db->BindObject()->trace_object, string);
        db->m_string_index_map[string_id] = string_index;
        return kProfilerHubStatusSuccess;
    }

    profiler_hub_result_t ProfilerHubClientMethods::AddTrack(
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
    ) {
        ProfilerHub* db = (ProfilerHub*)trace;
        if (db == nullptr)
            return kProfilerHubStatusInvalidArgument;
        rocprofvis_dm_track_params_t track_params;
        if (records_count == 0)
            return kProfilerHubStatusInvalidArgument;
        switch (category)
        {

        case kPprofilerHubCategoryRegionInstrumented:
            track_params.track_indentifiers.category = kRocProfVisDmRegionMainTrack;
            break;
        case kPprofilerHubCategoryRegionSampled:
            track_params.track_indentifiers.category = kRocProfVisDmRegionSampleTrack;
            break;
        case kPprofilerHubCategoryKernelDispatch:
            track_params.track_indentifiers.category = kRocProfVisDmKernelDispatchTrack;
            break;
        case kPprofilerHubCategoryMemoryAllocate:
            track_params.track_indentifiers.category = kRocProfVisDmMemoryAllocationTrack;
            break;
        case kPprofilerHubCategoryMemoryCopy:
            track_params.track_indentifiers.category = kRocProfVisDmMemoryCopyTrack;
            break;
        case kPprofilerHubCategoryPerformanceCounter:
            track_params.track_indentifiers.category = kRocProfVisDmPmcTrack;
            break;
        case kPprofilerHubCategoryStream:
            track_params.track_indentifiers.category = kRocProfVisDmStreamTrack;
            break;
        case kPprofilerHubCategoryUndefined:
        default:
            return kProfilerHubStatusUnknownError;
        }
        track_params.track_indentifiers.track_id = track_id;
        track_params.track_indentifiers.process_id = process_id == nullptr ? 0 : *process_id;
        track_params.track_indentifiers.source_type = kRPVSystemSourceProfillerHub;
        track_params.track_indentifiers.db_instance = db->DbInstancePtrAt(instance);
        track_params.track_indentifiers.id[TRACK_ID_NODE] = node_id == nullptr ? 0 : *node_id;
        track_params.track_indentifiers.name[TRACK_ID_NODE] = node_name ? node_name : std::string("Node ")+std::to_string(*node_id);
        track_params.track_indentifiers.tag[TRACK_ID_NODE] = Builder::NODE_ID_SERVICE_NAME;
        track_params.track_indentifiers.is_numeric[TRACK_ID_NODE] = true;
        if (category == kPprofilerHubCategoryRegionInstrumented || 
            category == kPprofilerHubCategoryRegionSampled || 
            category == kPprofilerHubCategoryStream)
        {
            if (!process_id)
                return kProfilerHubStatusInvalidArgument;
            track_params.track_indentifiers.id[TRACK_ID_PID] = *process_id;
            track_params.track_indentifiers.name[TRACK_ID_PID] = process_name ? process_name : "";
            track_params.track_indentifiers.tag[TRACK_ID_PID] = Builder::PROCESS_ID_SERVICE_NAME;
            track_params.track_indentifiers.is_numeric[TRACK_ID_PID] = true;
            if (!thread_id)
                return kProfilerHubStatusInvalidArgument;
        }
        if (category == kPprofilerHubCategoryRegionInstrumented || 
            category == kPprofilerHubCategoryRegionSampled)
        {
            track_params.track_indentifiers.id[TRACK_ID_TID] = *thread_id;
            track_params.track_indentifiers.name[TRACK_ID_TID] = thread_name ? thread_name : "";
            track_params.track_indentifiers.tag[TRACK_ID_TID] = Builder::THREAD_ID_SERVICE_NAME;
            track_params.track_indentifiers.is_numeric[TRACK_ID_TID] = true;

        }
        if (category == kPprofilerHubCategoryStream)
        {
            track_params.track_indentifiers.id[TRACK_ID_STREAM] = *stream_id;
            track_params.track_indentifiers.name[TRACK_ID_STREAM] = stream_name ? stream_name : "";
            track_params.track_indentifiers.tag[TRACK_ID_STREAM] = Builder::STREAM_ID_SERVICE_NAME;
            track_params.track_indentifiers.is_numeric[TRACK_ID_STREAM] = true;
        }

        if (category == kPprofilerHubCategoryKernelDispatch || 
            category == kPprofilerHubCategoryMemoryAllocate || 
            category == kPprofilerHubCategoryMemoryCopy || 
            category == kPprofilerHubCategoryPerformanceCounter)
        {
            if (!agent_id)
                return kProfilerHubStatusInvalidArgument;
            track_params.track_indentifiers.id[TRACK_ID_AGENT] = *agent_id;
            track_params.track_indentifiers.name[TRACK_ID_AGENT] = agent_name ? agent_name : "";
            track_params.track_indentifiers.tag[TRACK_ID_AGENT] = Builder::AGENT_ID_SERVICE_NAME;
            track_params.track_indentifiers.is_numeric[TRACK_ID_AGENT] = true;
            
        }
        if (category == kPprofilerHubCategoryPerformanceCounter)
        {
            if (!counter_id)
                return kProfilerHubStatusInvalidArgument;
            track_params.track_indentifiers.id[TRACK_ID_COUNTER] = *counter_id;
            track_params.track_indentifiers.name[TRACK_ID_COUNTER] = counter_name ? counter_name : std::string("Counter ") + std::to_string(*counter_id);
            track_params.track_indentifiers.tag[TRACK_ID_COUNTER] = Builder::THREAD_ID_SERVICE_NAME;
            track_params.track_indentifiers.is_numeric[TRACK_ID_COUNTER] = true;
        }
        track_params.record_count = records_count;
        track_params.min_ts = min_timestamp;
        track_params.max_ts = max_timestamp;
        track_params.min_value = min_level_or_value;
        track_params.max_value = max_level_or_value;

        return db->ProcessTrack(track_params) == kRocProfVisDmResultSuccess ? kProfilerHubStatusSuccess : kProfilerHubStatusUnknownError;
    }


    profiler_hub_result_t ProfilerHubClientMethods::AddTrackHistogramBucket(
        client_trace_handle_t trace,
        profiler_hub_track_id_t track_id,
        uint32_t bucket_number,
        uint32_t events_count,
        double bucket_value
    ) {
        ProfilerHub* db = (ProfilerHub*)trace;
        if (db == nullptr)
            return kProfilerHubStatusInvalidArgument;
        auto it = std::find_if(db->TrackPropertiesBegin(), db->TrackPropertiesEnd(), 
            [track_id](std::unique_ptr<rocprofvis_dm_track_params_t>& params) {
            return params.get()->track_indentifiers.track_id == track_id;
            });
        if (it != db->TrackPropertiesEnd())
        {
            it->get()->histogram[bucket_number] = std::make_pair( events_count, bucket_value );   
            db->TraceProperties()->histogram[bucket_number] += events_count;
        }
        return kProfilerHubStatusNotSupported;
    }

    profiler_hub_result_t  ProfilerHubClientMethods::AddInfoProperty(
        client_trace_handle_t trace,
        profiler_hub_instance_id_t instance,
        profiler_hub_string_t category,
        profiler_hub_string_t name,
        profiler_hub_value_type_t type,
        profiler_hub_row_id_t row_id,
        profiler_hub_value_handle_t value
    ) {
        ProfilerHub* db = (ProfilerHub*)trace;
        if (db == nullptr)
            return kProfilerHubStatusInvalidArgument;
        if (type == kPprofilerHubDataTypeString || type == kPprofilerHubDataTypeBlob)
        {
            db->CachedTables(instance)->AddTableCell(category, row_id, name, (rocprofvis_db_data_type_t)type, (const char*)value);
        }
        else
        if (type == kPprofilerHubDataTypeInt)
        {
            db->CachedTables(instance)->AddTableCell(category, row_id, name, (rocprofvis_db_data_type_t)type, std::to_string(*(size_t*)value));
        }
        else
        if (type == kPprofilerHubDataTypeDouble)
        {
            db->CachedTables(instance)->AddTableCell(category, row_id, name, (rocprofvis_db_data_type_t)type, std::to_string(*(double*)value));
        }
        else
        {
            return kProfilerHubStatusInvalidArgument;
        }
        return kProfilerHubStatusSuccess;
    }

    profiler_hub_result_t ProfilerHubClientMethods::AddEventRecord(
        client_trace_handle_t trace,
        profiler_hub_instance_id_t instance,
        profiler_hub_timeslice_handle_t container,
        profiler_hub_event_operation_t operation,
        profiler_hub_event_id_t event_id,
        uint64_t timestamp,
        uint64_t duration,
        profiler_hub_string_id_t category_id,
        profiler_hub_string_id_t symbol_id,
        profiler_hub_event_level_t level
    ) {
        rocprofvis_db_record_data_t record;
        ProfilerHub* db = (ProfilerHub*)trace;
        if (db == nullptr)
            return kProfilerHubStatusInvalidArgument;
        record.event.id.bitfield.event_op = db->ConvertProfilerHubEventType(operation);
        record.event.id.bitfield.event_node = instance;
     
        record.event.id.bitfield.event_id = event_id;
        record.event.timestamp = timestamp;
        record.event.duration = duration;
        record.event.category = category_id;
        record.event.symbol = symbol_id;
        record.event.level = level;

        if(db->BindObject()->FuncAddRecord(container, record) != kRocProfVisDmResultSuccess)
            return kProfilerHubStatusUnknownError;

        return kProfilerHubStatusSuccess;
    }

    profiler_hub_result_t ProfilerHubClientMethods::AddPmcRecord(
        client_trace_handle_t trace,
        profiler_hub_instance_id_t instance,
        profiler_hub_timeslice_handle_t container,
        uint64_t timestamp,
        double value
    ) {
        rocprofvis_db_record_data_t record;
        ProfilerHub* db = (ProfilerHub*)trace;
        if (db == nullptr)
            return kProfilerHubStatusInvalidArgument;
        record.pmc.timestamp = timestamp;
        record.pmc.value = value;
        return kProfilerHubStatusNotSupported;
    }

    profiler_hub_table_row_handle_t ProfilerHubClientMethods::AddTableRowContainer(
        client_trace_handle_t trace,
        profiler_hub_table_handle_t table_handle,
        size_t num_columns
    ) {
        ProfilerHub* db = (ProfilerHub*)trace;
        if (db == nullptr || table_handle == nullptr)
            return nullptr;
        ProfilerHubTableRow* row = db->m_table_rows_cache.AddRow(table_handle, num_columns);
        return row;
    }

    profiler_hub_result_t ProfilerHubClientMethods::AddTableCell(
        client_trace_handle_t trace,
        profiler_hub_table_row_handle_t container,
        uint32_t column_index,
        profiler_hub_string_t column_name,
        profiler_hub_value_type_t column_type,
        profiler_hub_value_handle_t value
    )
    {
        ProfilerHub* db = (ProfilerHub*)trace;
        if (db == nullptr)
            return kProfilerHubStatusInvalidArgument;
        ProfilerHubTableRow* row = (ProfilerHubTableRow*)container;    
        if (row == nullptr || !db->m_table_rows_cache.RowExists(row) || row->TableHandle() == nullptr)
            return kProfilerHubStatusInvalidArgument;
        if (column_type == kPprofilerHubDataTypeInt)
            row->SetCell(column_index, column_name, column_type, *(size_t*)value);
        else if (column_type == kPprofilerHubDataTypeDouble)
            row->SetCell(column_index, column_name, column_type, *(double*)value);
        else if (column_type == kPprofilerHubDataTypeString || column_type == kPprofilerHubDataTypeBlob)
            row->SetCell(column_index, column_name, column_type, (const char*)value);
        // maybe safer to add CommitRow method, if we cannot rely on last row index
        if (column_index + 1 == row->NumCells())
        {
            ProfilerHubTableHandler* table = (ProfilerHubTableHandler*)row->TableHandle();
            rocprofvis_db_query_callback_parameters callback_params;
            std::vector<const char*> columnNames(row->NumCells());
            for (int i = 0; i < row->NumCells(); i++)
            {
                columnNames[i] = row->CellAt(i)->name;
            }
            callback_params.db = db;
            callback_params.future = table->dm_future;
            callback_params.handle = table->handle;
            callback_params.db_instance = table->instance;
            callback_params.operation = table->op;
            callback_params.track_id = table->chunk;
            if (TableProcessor::CallbackRunCompoundQuery(&callback_params, row->NumCells(), row, (char**)columnNames.data()) == 0)
                return kProfilerHubStatusSuccess;
        }
        return kProfilerHubStatusUnknownError;
    }

    profiler_hub_result_t ProfilerHubClientMethods::AddEventDataFlowEndPoint(
        client_trace_handle_t trace,
        profiler_hub_flowtrace_handle_t container,
        profiler_hub_event_operation_t operation,
        profiler_hub_event_id_t event_id,
        profiler_hub_track_id_t track_id,
        profiler_hub_flow_direction_t direction,
        uint64_t timestamp,
        uint64_t duration,
        profiler_hub_string_id_t category_id,
        profiler_hub_string_id_t symbol_id,
        profiler_hub_event_level_t level
    ) {
        ProfilerHub* db = (ProfilerHub*)trace;
        if (db == nullptr)
            return kProfilerHubStatusInvalidArgument;
        rocprofvis_db_flow_data_t record;
        record.id.bitfield.event_id = event_id;
        record.time = timestamp;
        record.category_id = category_id;
        record.symbol_id = symbol_id;
        record.level = level;
        record.end_time = timestamp + duration;
        record.track_id = track_id;
        if (db->BindObject()->FuncAddFlow(container,record) != kRocProfVisDmResultSuccess) return kProfilerHubStatusUnknownError;
        return kProfilerHubStatusSuccess;
    }

    profiler_hub_result_t  ProfilerHubClientMethods::AddEventExtendedInfo(
        client_trace_handle_t trace,
        profiler_hub_instance_id_t instance,
        profiler_hub_ext_data_handle_t container,
        profiler_hub_string_t category,
        profiler_hub_string_t name,
        profiler_hub_value_type_t type,
        profiler_hub_string_t value
    )
    {
        ProfilerHub* db = (ProfilerHub*)trace;
        if (db == nullptr)
            return kProfilerHubStatusInvalidArgument;
        rocprofvis_db_ext_data_t record;
        record.category = category;
        record.name = name;
        record.type = (rocprofvis_db_data_type_t)type;
        record.data = value;
        record.db_instance = instance;
        if (db->BindObject()->FuncAddExtDataRecord(container,record) != kRocProfVisDmResultSuccess) return kProfilerHubStatusUnknownError;
        return kProfilerHubStatusSuccess;
    }

    profiler_hub_result_t  ProfilerHubClientMethods::AddEventEssentialInfo(
        client_trace_handle_t trace,
        profiler_hub_instance_id_t instance,
        profiler_hub_ext_data_handle_t container,
        profiler_hub_track_id_t track_id,
        profiler_hub_track_id_t stream_track_id,
        profiler_hub_event_level_t level,
        profiler_hub_event_level_t stream_level
    ) {
        ProfilerHub* db = (ProfilerHub*)trace;
        if (db == nullptr)
            return kProfilerHubStatusInvalidArgument;
        rocprofvis_db_ext_data_t record;
        record.category = "Track";
        record.name = "trackId";
        record.type = kRPVDataTypeInt;
        std::string str = std::to_string(track_id);
        record.data = str.c_str();
        record.category_enum = kRocProfVisEventEssentialDataTrack;
        record.db_instance = instance;
        if (db->BindObject()->FuncAddExtDataRecord(container, record) !=
            kRocProfVisDmResultSuccess)
            return kProfilerHubStatusUnknownError;
        record.category = "Track";
        record.name = "levelForTrack";
        record.type = kRPVDataTypeInt;
        str = std::to_string(level);
        record.data = str.c_str();
        record.category_enum = kRocProfVisEventEssentialDataLevel;
        record.db_instance = instance;
        if (db->BindObject()->FuncAddExtDataRecord(container, record) !=
            kRocProfVisDmResultSuccess)
            return kProfilerHubStatusUnknownError;
        if (stream_track_id != -1)
        {
            record.category = "Track";
            record.name = "streamTrackId";
            record.type = kRPVDataTypeInt;
            std::string str = std::to_string(stream_track_id);
            record.data = str.c_str();
            record.category_enum = kRocProfVisEventEssentialDataStreamTrack;
            record.db_instance = instance;
            if (db->BindObject()->FuncAddExtDataRecord(container, record) !=
                kRocProfVisDmResultSuccess)
                return kProfilerHubStatusUnknownError;
            record.category = "Track";
            record.name = "levelForStreamTrack";
            record.type = kRPVDataTypeInt;
            str = std::to_string(stream_level);
            record.data = str.c_str();
            record.category_enum = kRocProfVisEventEssentialDataStreamLevel;
            record.db_instance = instance;
            if (db->BindObject()->FuncAddExtDataRecord(container, record) !=
                kRocProfVisDmResultSuccess)
                return kProfilerHubStatusUnknownError;
        }
        return kProfilerHubStatusSuccess;
    }

    profiler_hub_result_t  ProfilerHubClientMethods::AddEventArgumentsInfo(
        client_trace_handle_t trace,
        profiler_hub_instance_id_t instance,
        profiler_hub_ext_data_handle_t container,
        uint32_t position,
        profiler_hub_string_t name,
        profiler_hub_string_t type,
        profiler_hub_string_t value
    ) {
        ProfilerHub* db = (ProfilerHub*)trace;
        if (db == nullptr)
            return kProfilerHubStatusInvalidArgument;
        rocprofvis_db_argument_data_t record;
        record.position = position;
        record.type = type;
        record.name = name;
        record.value = value;
        if (db->BindObject()->FuncAddArgDataRecord(container, record) != kRocProfVisDmResultSuccess) return kProfilerHubStatusUnknownError;
        return kProfilerHubStatusSuccess;
    }

    profiler_hub_result_t  ProfilerHubClientMethods::AddEventCallStackFrame(
        client_trace_handle_t trace,
        profiler_hub_instance_id_t instance,
        profiler_hub_call_stack_handle_t container,
        profiler_hub_string_t function,
        profiler_hub_string_t file,
        profiler_hub_string_t line,
        profiler_hub_string_t address,
        uint32_t depth
    ) {
        ProfilerHub* db = (ProfilerHub*)trace;
        if (db == nullptr)
            return kProfilerHubStatusInvalidArgument;
        rocprofvis_db_stack_data_t record;
        record.symbol = function;
        record.line = line;
        record.depth = depth;
        record.args = file;
        if (db->BindObject()->FuncAddStackFrame(container, record) !=
            kRocProfVisDmResultSuccess)
            return kProfilerHubStatusUnknownError;
        return kProfilerHubStatusSuccess;
    }

}

//----------------------------------------------------------------------------------------------------------------------------------------

profiler_hub_result_t profiler_hub::client::interface::AddInstance(
    client_trace_handle_t trace,
    profiler_hub_instance_id_t id,
    profiler_hub_string_t file,
    profiler_hub_string_t uuid
) {
    return RocProfVis::DataModel::ProfilerHubClientMethods::AddInstance(trace, id, file, uuid);
}

profiler_hub_result_t profiler_hub::client::interface::AddString(
    client_trace_handle_t trace,
    profiler_hub_string_t string,
    uint32_t string_id

) {
    return RocProfVis::DataModel::ProfilerHubClientMethods::AddString(trace, string, string_id);
}

profiler_hub_result_t profiler_hub::client::interface::AddTrack(
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
){
    return RocProfVis::DataModel::ProfilerHubClientMethods::AddTrack(
        trace, instance, track_id, track_name, category, node_id, node_name, process_id, process_name,
        thread_id, thread_name, stream_id, stream_name, agent_type, agent_id, agent_name, queue_id, queue_name, 
        counter_id, counter_name, records_count, min_timestamp, max_timestamp, min_level_or_value, max_level_or_value);
}

profiler_hub_result_t profiler_hub::client::interface::AddTrackHistogramBucket(
    client_trace_handle_t trace,
    profiler_hub_track_id_t track_id,
    uint32_t bucket_number,
    uint32_t events_count,
    double bucket_value
) {
    return RocProfVis::DataModel::ProfilerHubClientMethods::AddTrackHistogramBucket(trace, track_id, bucket_number, events_count, bucket_value);
}

profiler_hub_result_t  profiler_hub::client::interface::AddInfoProperty(
    client_trace_handle_t trace,
    profiler_hub_instance_id_t instance,
    profiler_hub_string_t category,
    profiler_hub_string_t name,
    profiler_hub_value_type_t type,
    profiler_hub_row_id_t row_id,
    profiler_hub_value_handle_t value
) {
    return RocProfVis::DataModel::ProfilerHubClientMethods::AddInfoProperty(trace, instance, category, name, type, row_id, value);
}

profiler_hub_result_t profiler_hub::client::interface::AddEventRecord(
    client_trace_handle_t trace,
    profiler_hub_instance_id_t instance,
    profiler_hub_timeslice_handle_t container,
    profiler_hub_event_operation_t operation,
    profiler_hub_event_id_t event_id,
    uint64_t timestamp,
    uint64_t duration,
    profiler_hub_string_id_t category_id,
    profiler_hub_string_id_t symbol_id,
    profiler_hub_event_level_t level
)
{
    return RocProfVis::DataModel::ProfilerHubClientMethods::AddEventRecord(trace, instance, container, operation, event_id, timestamp, duration, category_id, symbol_id, level);
}

profiler_hub_result_t profiler_hub::client::interface::AddPmcRecord(
    client_trace_handle_t trace,
    profiler_hub_instance_id_t instance,
    profiler_hub_timeslice_handle_t container,
    uint64_t timestamp,
    double value
)
{
    return RocProfVis::DataModel::ProfilerHubClientMethods::AddPmcRecord(trace, instance, container, timestamp, value);
}

profiler_hub_table_row_handle_t profiler_hub::client::interface::AddTableRowContainer(
    client_trace_handle_t trace,
    profiler_hub_table_handle_t container,
    size_t num_columns
) {
    return RocProfVis::DataModel::ProfilerHubClientMethods::AddTableRowContainer(trace, container, num_columns);
}

profiler_hub_result_t profiler_hub::client::interface::AddTableCell(
    client_trace_handle_t trace,
    profiler_hub_table_row_handle_t container,
    uint32_t column_index,
    profiler_hub_string_t column_name,
    profiler_hub_value_type_t column_type,
    profiler_hub_value_handle_t value
)
{
    return RocProfVis::DataModel::ProfilerHubClientMethods::AddTableCell(trace, container, column_index, column_name, column_type, value);
}


profiler_hub_result_t profiler_hub::client::interface::AddEventDataFlowEndPoint(
    client_trace_handle_t trace,
    profiler_hub_flowtrace_handle_t container,
    profiler_hub_event_operation_t operation,
    profiler_hub_event_id_t event_id,
    profiler_hub_track_id_t track_id,
    profiler_hub_flow_direction_t direction,
    uint64_t timestamp,
    uint64_t duration,
    profiler_hub_string_id_t category_id,
    profiler_hub_string_id_t symbol_id,
    profiler_hub_event_level_t level
) {
    return RocProfVis::DataModel::ProfilerHubClientMethods::AddEventDataFlowEndPoint(trace, container, operation, event_id,track_id,  direction, timestamp, duration, category_id, symbol_id, level);
}

profiler_hub_result_t  profiler_hub::client::interface::AddEventExtendedInfo(
    client_trace_handle_t trace,
    profiler_hub_instance_id_t instance,
    profiler_hub_ext_data_handle_t container,
    profiler_hub_string_t category,
    profiler_hub_string_t name,
    profiler_hub_value_type_t type,
    profiler_hub_string_t value
) {
    return RocProfVis::DataModel::ProfilerHubClientMethods::AddEventExtendedInfo(trace, instance, container, category, name, type, value);
}

profiler_hub_result_t  profiler_hub::client::interface::AddEventEssentialInfo(
    client_trace_handle_t trace,
    profiler_hub_instance_id_t instance,
    profiler_hub_ext_data_handle_t container,
    profiler_hub_track_id_t track_id,
    profiler_hub_track_id_t stream_track_id,
    profiler_hub_event_level_t level,
    profiler_hub_event_level_t stream_level
) {
    return RocProfVis::DataModel::ProfilerHubClientMethods::AddEventEssentialInfo(trace, instance, container, track_id, stream_track_id, level, stream_level);
}

profiler_hub_result_t  profiler_hub::client::interface::AddEventArgumentsInfo(
    client_trace_handle_t trace,
    profiler_hub_instance_id_t instance,
    profiler_hub_ext_data_handle_t container,
    uint32_t position,
    profiler_hub_string_t name,
    profiler_hub_string_t type,
    profiler_hub_string_t value
) {
    return RocProfVis::DataModel::ProfilerHubClientMethods::AddEventArgumentsInfo(trace, instance, container, position, name, type, value);
}

profiler_hub_result_t  profiler_hub::client::interface::AddEventCallStackFrame(
    client_trace_handle_t trace,
    profiler_hub_instance_id_t instance,
    profiler_hub_call_stack_handle_t container,
    profiler_hub_string_t function,
    profiler_hub_string_t file,
    profiler_hub_string_t line,
    profiler_hub_string_t address,
    uint32_t depth
) {
    return RocProfVis::DataModel::ProfilerHubClientMethods::AddEventCallStackFrame(trace, instance, container, function, file, line, address, depth);
}