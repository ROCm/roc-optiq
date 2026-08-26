// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_db_query_manager.h"
#include "rocprofvis_c_interface.h"

namespace RocProfVis
{
namespace DataModel
{

    rocprofvis_event_data_category_enum_t
        QueryManager::GetColumnDataCategory( const rocprofvis_event_data_category_map_t category_map,
            rocprofvis_dm_event_operation_t op,
            std::string                     name)
    {
        auto it_op = category_map.find(op);
        if(it_op != category_map.end())
        {
            auto it = it_op->second.find(name);
            if(it != it_op->second.end())
            {
                return it->second;
            }
        }
        it_op = category_map.find(kRocProfVisDmOperationNoOp);
        if(it_op != category_map.end())
        {
            auto it = it_op->second.find(name);
            if(it != it_op->second.end())
            {
                return it->second;
            }
        }
        return kRocProfVisEventEssentialDataUncategorized;
    }

int QueryManager::CallbackGetValue(void* data, int argc, sqlite3_stmt* stmt, char** azColName){
    ROCPROFVIS_ASSERT_MSG_RETURN(argc==1, ERROR_DATABASE_QUERY_PARAMETERS_MISMATCH, 1);
    ROCPROFVIS_ASSERT_MSG_RETURN(data, ERROR_SQL_QUERY_PARAMETERS_CANNOT_BE_NULL, 1);
    void*  func = (void*)&CallbackGetValue;
    rocprofvis_db_sqlite_callback_parameters* callback_params = (rocprofvis_db_sqlite_callback_parameters*)data;
    QueryManager* db = (QueryManager*) callback_params->db;
    std::string * string_ptr = (rocprofvis_dm_string_t*)callback_params->handle;
    ROCPROFVIS_ASSERT_MSG_RETURN(string_ptr, ERROR_SQL_QUERY_PARAMETERS_CANNOT_BE_NULL, 1);
    *string_ptr = db->Sqlite3ColumnText(func, stmt, azColName, 0);
    return 0;
} 

int QueryManager::CallbackMakeHistogramPerTrack(void* data, int argc, sqlite3_stmt* stmt,
    char** azColName) {
    ROCPROFVIS_ASSERT_MSG_RETURN(argc == 4, ERROR_DATABASE_QUERY_PARAMETERS_MISMATCH, 1);
    ROCPROFVIS_ASSERT_MSG_RETURN(data, ERROR_SQL_QUERY_PARAMETERS_CANNOT_BE_NULL, 1);
    void *func = (void*)&CallbackMakeHistogramPerTrack;
    rocprofvis_db_sqlite_callback_parameters* callback_params =
        (rocprofvis_db_sqlite_callback_parameters*) data;
    QueryManager* db = (QueryManager*) callback_params->db;
    if(callback_params->future->Interrupted()) return SQLITE_ABORT;
    uint32_t index                             = db->Sqlite3ColumnInt(func, stmt, azColName, 3);
    uint32_t bucket_number = db->Sqlite3ColumnInt(func, stmt, azColName, 0);
    uint32_t event_count = db->Sqlite3ColumnInt(func, stmt, azColName, 1);
    double bucket_value = db->Sqlite3ColumnDouble(func, stmt, azColName, 2);
    db->TrackPropertiesAt(index)->histogram[bucket_number] = std::make_pair(event_count, bucket_value);
    callback_params->future->CountThisRow();
    return 0;
}

int QueryManager::CallbackRunQuery(void *data, int argc, sqlite3_stmt* stmt, char **azColName){
    ROCPROFVIS_ASSERT_MSG_RETURN(data, ERROR_SQL_QUERY_PARAMETERS_CANNOT_BE_NULL, 1);
    rocprofvis_db_sqlite_callback_parameters* callback_params = (rocprofvis_db_sqlite_callback_parameters*)data;
    QueryManager* db = (QueryManager*)callback_params->db;
    void* func = (void*)&CallbackRunQuery;
    if (callback_params->future->Interrupted()) return 1;
    rocprofvis_dm_table_row_t row =
        db->BindObject()->FuncAddTableRow(callback_params->handle);
    ROCPROFVIS_ASSERT_MSG_RETURN(row, ERROR_TABLE_ROW_CANNOT_BE_NULL, 1);
    
    if(0 == callback_params->future->GetProcessedRowsCount())
    {
        for (int i=0; i < argc; i++)
        {
            if (kRocProfVisDmResultSuccess != db->BindObject()->FuncAddTableColumn(callback_params->handle,azColName[i])) return 1;
        }
    }
    for (int i=0; i < argc; i++)
    {
        std::string column_text = db->Sqlite3ColumnText(func, stmt, azColName, i);
        if (kRocProfVisDmResultSuccess != db->BindObject()->FuncAddTableRowCell(row, column_text.c_str())) return 1;
    }

    callback_params->future->CountThisRow();
    return 0;
}


rocprofvis_dm_result_t QueryManager::BuildSliceQuery(rocprofvis_dm_timestamp_t start, rocprofvis_dm_timestamp_t end, rocprofvis_db_num_of_tracks_t num, rocprofvis_db_track_selection_t tracks, rocprofvis_dm_string_t& query, slice_array_t& slices) {
    (void) num;
    slice_query_map_t slice_query_map;
    bool timed_query = false;
    bool pmc_query = false;

    rocprofvis_dm_track_params_t* props = TrackPropertiesAt(*tracks);
    DbInstance* db_instance = (DbInstance*)props->track_indentifiers.db_instance;

    start += TraceProperties()->db_inst_start_time[db_instance->GuidIndex()];
    end += TraceProperties()->db_inst_start_time[db_instance->GuidIndex()];

    if (props->track_indentifiers.category == kRocProfVisDmPmcTrack)
    {
        pmc_query = true;
    }
    BuildSliceQueryMap(slice_query_map, props, props->track_indentifiers.category ==  kRocProfVisDmStreamTrack? kRPVRocpdQuerySliceByStream : kRPVRocpdQuerySliceByQueue);
    if (start > props->min_ts || end < props->max_ts)
    {
        timed_query = true;
    }


    query = "SELECT *, ";
    query += std::to_string(*tracks);
    query += " as track_id FROM(";
    for (auto it_query = slice_query_map.begin(); it_query != slice_query_map.end(); ++it_query) {
        if (it_query!=slice_query_map.begin()) query += " UNION ALL ";
        query += it_query->first;
        query += it_query->second[db_instance->GuidIndex()];
        query += ")";
        if(timed_query)
        {
            query += " and ";
            if (pmc_query)
            {
                query += Builder::START_SERVICE_NAME;
                query += " BETWEEN ";
                query += std::to_string(start);
                query += " and ";
                query += std::to_string(end);
            }
            else
            {
                query += Builder::START_SERVICE_NAME;
                query += " < ";
                query += std::to_string(end);
                query += " and ";
                query += Builder::END_SERVICE_NAME;
                query += " > ";
                query += std::to_string(start);
            }
        }
    }
    query += ");";
    return kRocProfVisDmResultSuccess;

}

rocprofvis_dm_result_t QueryManager::BuildCounterSliceLeftNeighbourQuery(rocprofvis_dm_timestamp_t start, rocprofvis_dm_timestamp_t end, rocprofvis_dm_index_t track_index, rocprofvis_dm_string_t& query) {
    slice_query_map_t slice_query_map;
    rocprofvis_dm_track_params_t* props = TrackPropertiesAt(track_index);
    DbInstance* db_instance = (DbInstance*)props->track_indentifiers.db_instance;

    start += TraceProperties()->db_inst_start_time[db_instance->GuidIndex()];
    end += TraceProperties()->db_inst_start_time[db_instance->GuidIndex()];

    BuildSliceQueryMap(slice_query_map, props, props->track_indentifiers.category ==  kRocProfVisDmStreamTrack? kRPVRocpdQuerySliceByStream : kRPVRocpdQuerySliceByQueue);

    if (!slice_query_map.empty()) {
        auto it_query = slice_query_map.begin();
        query = "SELECT *, ";
        query += std::to_string(track_index);
        query += " as track_id FROM(";
        query += it_query->first;
        query += it_query->second[db_instance->GuidIndex()];
        query += ") and ";
        query += Builder::START_SERVICE_NAME;
        query += " < ";
        query += std::to_string(start);
        query += std::string(" ORDER BY ") + Builder::START_SERVICE_NAME + " DESC LIMIT 1 )";
    }
    return kRocProfVisDmResultSuccess;

}

rocprofvis_dm_result_t QueryManager::BuildCounterSliceRightNeighbourQuery(rocprofvis_dm_timestamp_t start, rocprofvis_dm_timestamp_t end, rocprofvis_dm_index_t track_index, rocprofvis_dm_string_t& query) {
    slice_query_map_t slice_query_map;
    rocprofvis_dm_track_params_t* props = TrackPropertiesAt(track_index);
    DbInstance* db_instance = (DbInstance*)props->track_indentifiers.db_instance;

    start += TraceProperties()->db_inst_start_time[db_instance->GuidIndex()];
    end += TraceProperties()->db_inst_start_time[db_instance->GuidIndex()];

    BuildSliceQueryMap(slice_query_map, props, props->track_indentifiers.category ==  kRocProfVisDmStreamTrack? kRPVRocpdQuerySliceByStream : kRPVRocpdQuerySliceByQueue);

    if (!slice_query_map.empty()) {
        auto it_query = slice_query_map.begin();
        query = "SELECT *, ";
        query += std::to_string(track_index);
        query += " as track_id FROM(";
        query += it_query->first;
        query += it_query->second[db_instance->GuidIndex()];
        query += ") and ";
        query += Builder::START_SERVICE_NAME;
        query += " > ";
        query += std::to_string(end);
        query += std::string(" ORDER BY ") + Builder::START_SERVICE_NAME + " ASC LIMIT 1 )";
    }
    return kRocProfVisDmResultSuccess;

}

rocprofvis_dm_event_operation_t QueryManager::GetTableQueryOperation(std::string query)
{
    std::string select_str = "SELECT ";
    if (query.find(select_str) == 0)
    {
        return (rocprofvis_dm_event_operation_t)std::atol(query.substr(select_str.size(), 1).c_str());
    }
    return kRocProfVisDmOperationNoOp;
}




bool QueryManager::IsEmptyRange(uint32_t track, uint64_t start, uint64_t end) {
    if (TABLE_QUERY_UNPACK_OP_TYPE(track) != 0)
        return false;
    DbInstance* instance = (DbInstance*)TrackPropertiesAt(track)->track_indentifiers.db_instance;
    ROCPROFVIS_ASSERT_MSG_RETURN(instance, ERROR_NODE_KEY_CANNOT_BE_NULL, true);
    uint64_t start_bucket =
        (start - TraceProperties()->db_inst_start_time[instance->GuidIndex()]) / TraceProperties()->histogram_bucket_size;

    uint64_t end_bucket =
        (end - TraceProperties()->db_inst_start_time[instance->GuidIndex()]) / TraceProperties()->histogram_bucket_size;


    if (TABLE_QUERY_UNPACK_OP_TYPE(track) != 0)
    {
        auto it = TraceProperties()->histogram.lower_bound(static_cast<uint32_t>(start_bucket));
        while (it != TraceProperties()->histogram.end() && it->first <= end_bucket) {
            if (it->second > 0) {
                return false;
            }
            ++it;
        }
    }
    else
    {
        auto it = TrackPropertiesAt(TABLE_QUERY_UNPACK_TRACK_ID(track))->histogram.lower_bound(static_cast<uint32_t>(start_bucket));
        while (it != TrackPropertiesAt(TABLE_QUERY_UNPACK_TRACK_ID(track))->histogram.end() && it->first <= end_bucket) {
            if (it->second.first > 0) {
                return false;
            }
            ++it;
        }
    }

    return true;
}


rocprofvis_dm_result_t
QueryManager::BuildCompoundQuery(
    rocprofvis_dm_table_use_case_enum_t use_case, rocprofvis_dm_timestamp_t start,
    rocprofvis_dm_timestamp_t end, rocprofvis_db_num_of_tracks_t num,
    rocprofvis_db_track_selection_t tracks,
    std::vector<slice_query_map_t>& slice_query_map_array, rocprofvis_dm_charptr_t where,
    rocprofvis_dm_charptr_t filter, rocprofvis_dm_charptr_t group,
    rocprofvis_dm_charptr_t group_cols, rocprofvis_dm_charptr_t sort_column,
    rocprofvis_dm_sort_order_t sort_order, uint64_t max_count, uint64_t offset,
    bool count_only, rocprofvis_dm_string_t& query)
{
    query = "";

    size_t thread_count = std::thread::hardware_concurrency();
    bool   event_table  = false;

    // Total events across all operations, used to size each big track's parallel split. Track-
    // independent, so compute once here rather than re-summing per large track below.
    uint64_t total_events = 0;
    for(int op = kRocProfVisDmOperationLaunch; op < kRocProfVisDmNumOperation; op++)
    {
        total_events += TraceProperties()->events_count[op];
    }

    for(int i = 0; i < slice_query_map_array.size(); i++)
    {
        rocprofvis_dm_index_t track = tracks[i];
        track                       = TABLE_QUERY_UNPACK_TRACK_ID(track);

        // Split a track's time range into windows proportional to its share of all events, so
        // one very large track (e.g. a 3M-event region track) is packed by many threads in
        // parallel instead of being the fetch's single-threaded long pole while small tracks'
        // threads sit idle. Scaled by the core count so the dominant track can use most cores.
        int divider = 1;
        if(TABLE_QUERY_UNPACK_OP_TYPE(tracks[i]) == kRocProfVisDmOperationNoOp)
        {
            rocprofvis_dm_track_params_t* dprops = TrackPropertiesAt(track);
            if(dprops->record_count > SINGLE_THREAD_RECORDS_COUNT_LIMIT && total_events > 0)
            {
                divider = static_cast<int>((dprops->record_count * thread_count) / total_events);
            }
        }
        else
        {
            divider = static_cast<int>(thread_count / slice_query_map_array.size());
        }
        // Never fewer than one window, and never more than the core count (a NoOp combined
        // track's record_count can transiently exceed the summed per-op counts).
        if(divider < 1)
        {
            divider = 1;
        }
        else if(thread_count > 0 && divider > static_cast<int>(thread_count))
        {
            divider = static_cast<int>(thread_count);
        }
        for(auto it_query = slice_query_map_array[i].begin();
            it_query != slice_query_map_array[i].end(); ++it_query)
        {
            auto op = GetTableQueryOperation(it_query->first);
            if(op > kRocProfVisDmOperationNoOp)
            {
                event_table = true;
            }
            if(TABLE_QUERY_UNPACK_OP_TYPE(track) == 0)
            {
                rocprofvis_dm_track_params_t* props = TrackPropertiesAt(track);
                if(props->record_count < SINGLE_THREAD_RECORDS_COUNT_LIMIT ||
                    op == kRocProfVisDmOperationMemoryAllocate ||
                    op == kRocProfVisDmOperationMemoryCopy)
                    divider = 1;
            }
            uint64_t step = (end - start) / divider;
            for(auto it_instance = it_query->second.begin();
                it_instance != it_query->second.end(); ++it_instance)
            {
                DbInstance* db_inst = DbInstancePtrAt(it_instance->first);
                ROCPROFVIS_ASSERT_MSG_RETURN(db_inst, ERROR_NODE_KEY_CANNOT_BE_NULL,
                    kRocProfVisDmResultUnknownError);
                uint64_t begin =
                    start + TraceProperties()->db_inst_start_time[db_inst->GuidIndex()];
                for(int j = 0; j < divider; j++)
                {
                    uint64_t fetch_start = begin + (step * j);
                    uint64_t fetch_end   = begin + (step * j) + step;
                    if(IsEmptyRange(tracks[i], fetch_start, fetch_end)) continue;
                    query += it_query->first;
                    if(it_instance->second.empty())
                    {
                        query += " WHERE ";
                    }
                    else
                    {
                        query += it_instance->second;
                        query += ") and ";
                    }

                    query += Builder::END_SERVICE_NAME;
                    query += " >= ";
                    query += std::to_string(fetch_start);
                    query += " and ";
                    query += Builder::START_SERVICE_NAME;
                    query += (j == divider - 1) ? " <= " : " < ";
                    query += std::to_string(fetch_end);
                    if(where && strlen(where))
                    {
                        query += " AND ";
                        query += where;
                    }
                    query += ";";
                    query += std::to_string(tracks[i]);
                    query += ";";
                    query += std::to_string(it_instance->first);
                    query += "\n";
                }
            }
        }
    }
    if(query.empty())
    {
        return kRocProfVisDmResultSuccess;
    }
    query += "-- CMD: TYPE ";
    switch(use_case)
    {
    case kRPVDMTableUseCaseEventTrackTable:
    {
        query += std::to_string(kRPVTableDataTypeEvent);
        break;
    }
    case kRPVDMTableUseCaseSampleTrackTable:
    {
        query += std::to_string(kRPVTableDataTypeSample);
        break;
    }
    case kRPVDMTableUseCaseEventSearch:
    {
        query += std::to_string(kRPVTableDataTypeSearch);
        break;
    }
    default:
    {
        return kRocProfVisDmResultInvalidParameter;
        break;
    }
    }
    query += "\n";

    if(group && strlen(group))
    {
        query += "-- CMD: GROUP ";
        if(group_cols && strlen(group_cols))
        {
            if(!FilterExpression::StartsWithSubstring(group, group_cols))
            {
                query += group_cols;
                query += ", ";
            }
            query += group;
        }
        else
        {
            query += group;
            bool sample_query = false;
            if(TABLE_QUERY_UNPACK_OP_TYPE(tracks[0]) == 0)
            {
                sample_query =
                    TrackPropertiesAt(tracks[0])->track_indentifiers.category ==
                    kRocProfVisDmPmcTrack;
            }
            else
            {
                sample_query =
                    (rocprofvis_dm_event_operation_t) TABLE_QUERY_UNPACK_OP_TYPE(
                        tracks[0]) == kRocProfVisDmOperationNoOp;
            }
            if(sample_query)
            {
                query += ", COUNT(*) as count, AVG(value) as avg_value, MIN(value) as "
                    "min_value, MAX(value) as max_value";
            }
            else
            {
                query += ", COUNT(*) as num_invocations, AVG(duration) as avg_duration, "
                    "MIN(duration) as min_duration, MAX(duration) as max_duration";
            }
        }
        query += "\n";
    }

    if(filter && strlen(filter))
    {
        query += "-- CMD: FILTER ";
        query += filter;
        query += "\n";
    }

    if(sort_column && strlen(sort_column))
    {
        query += "-- CMD: SORT";
        if(sort_order == kRPVDMSortOrderAsc)
        {
            query += " ASC ";
        }
        else
        {
            query += " DESC ";
        }
        query += sort_column;
        query += "\n";
    }
    if(count_only)
    {
        query += "-- CMD: COUNT";
        query += "\n";
    }
    else
    {
        if(max_count)
        {
            query += "-- CMD: LIMIT ";
            query += std::to_string(max_count);
            query += "\n";
        }
        if(offset)
        {
            query += "-- CMD: OFFSET ";
            query += std::to_string(offset);
            query += "\n";
        }
    }
    return kRocProfVisDmResultSuccess;
}


rocprofvis_dm_result_t
QueryManager::BuildTableQuery(
    rocprofvis_dm_table_use_case_enum_t use_case, 
    rocprofvis_dm_timestamp_t start, rocprofvis_dm_timestamp_t end, 
    rocprofvis_db_num_of_tracks_t num, rocprofvis_db_track_selection_t tracks, 
    rocprofvis_dm_charptr_t where, rocprofvis_dm_charptr_t filter,
    rocprofvis_dm_charptr_t group, rocprofvis_dm_charptr_t group_cols,
    rocprofvis_dm_charptr_t sort_column, rocprofvis_dm_sort_order_t sort_order,
    uint64_t max_count, uint64_t offset, bool count_only, rocprofvis_dm_string_t& query)
{
    std::vector<slice_query_map_t> slice_query_map_array;
    slice_query_map_array.resize(num);
    for(int i = 0; i < num; i++)
    {
        rocprofvis_dm_index_t track = tracks[i];
        if(TABLE_QUERY_UNPACK_OP_TYPE(track) == 0)
        {
            track                               = TABLE_QUERY_UNPACK_TRACK_ID(track);
            rocprofvis_dm_track_params_t* props = TrackPropertiesAt(track);
            BuildSliceQueryMap(slice_query_map_array[i], props, kRPVRocpdQueryTable);
        }
        else 
        {
            track = TABLE_QUERY_UNPACK_OP_TYPE(track);
            for(auto db_inst : DbInstances())
            {
                // Skip instances removed in place, so the table view stops surfacing a
                // removed trace's rows/counts.
                if(!IsInstanceActive(db_inst.first.FileIndex()))
                {
                    continue;
                }
                slice_query_map_array[i][GetEventOperationQuery(
                    (rocprofvis_dm_event_operation_t) track)][db_inst.first.GuidIndex()];
            }
        }
    }
    bool slice_query_map_empty = true;
    for(slice_query_map_t& query_map : slice_query_map_array)
    {
        if(!query_map.empty())
        {
            slice_query_map_empty = false;
            break;
        }
    }
    if(slice_query_map_empty)
    {
        return kRocProfVisDmResultSuccess;    
    }
    return BuildCompoundQuery(use_case, start, end, num, tracks, slice_query_map_array,
        where, filter, group, group_cols, sort_column, sort_order,
        max_count, offset, count_only, query);
}

rocprofvis_dm_result_t
QueryManager::BuildEventSearchQuery(
    rocprofvis_dm_timestamp_t start, rocprofvis_dm_timestamp_t end,
    rocprofvis_db_num_of_tracks_t num, rocprofvis_db_track_selection_t ops,
    rocprofvis_dm_charptr_t where,
    rocprofvis_dm_num_string_table_filters_t num_string_table_filters,
    rocprofvis_dm_string_table_filters_t     string_table_filters,
    bool include_substring,
    rocprofvis_dm_charptr_t sort_column, rocprofvis_dm_sort_order_t sort_order,
    uint64_t max_count, uint64_t offset, bool count_only, rocprofvis_dm_string_t& query)
{
    std::vector<slice_query_map_t> slice_query_map_array;
    table_string_id_filter_map_t string_id_filter_map;
    rocprofvis_dm_result_t string_filter_result = BuildTableStringIdFilter(num_string_table_filters, string_table_filters, include_substring, string_id_filter_map);
    slice_query_map_array.resize(num);
    for(int i = 0; i < num; i++)
    {
        rocprofvis_dm_index_t op = TABLE_QUERY_UNPACK_OP_TYPE(ops[i]);
        if (num_string_table_filters > 0)
        {
            if (string_filter_result == kRocProfVisDmResultSuccess && string_id_filter_map.count((rocprofvis_dm_event_operation_t)op) > 0)
            {
                auto filters = string_id_filter_map.at((rocprofvis_dm_event_operation_t)op);
                for (auto it = filters.begin(); it != filters.end(); ++it)
                {
                    slice_query_map_array[i][GetEventOperationQuery((rocprofvis_dm_event_operation_t)op)][it->first] = std::string(" WHERE ") + it->second;
                }
            }
        }
    }
    bool slice_query_map_empty = true;
    for(slice_query_map_t& query_map : slice_query_map_array)
    {
        if(!query_map.empty())
        {
            slice_query_map_empty = false;
            break;
        }
    }
    if(slice_query_map_empty)
    {
        return kRocProfVisDmResultSuccess;
    }
    return BuildCompoundQuery(kRPVDMTableUseCaseEventSearch, start, end, num, ops, slice_query_map_array,
        where, nullptr, nullptr, nullptr, sort_column, sort_order,
        max_count, offset, count_only, query);
}

rocprofvis_dm_result_t  QueryManager::ExecuteQuery(
    rocprofvis_dm_charptr_t query,
    rocprofvis_dm_charptr_t description,
    Future* future){

    ROCPROFVIS_ASSERT_MSG_RETURN(future, ERROR_FUTURE_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
    while (true)
    {
        ROCPROFVIS_ASSERT_MSG_BREAK(BindObject()->trace_properties, ERROR_TRACE_PROPERTIES_CANNOT_BE_NULL);
        ROCPROFVIS_ASSERT_MSG_BREAK(BindObject()->trace_properties->metadata_loaded, ERROR_METADATA_IS_NOT_LOADED);
        rocprofvis_dm_table_t table = BindObject()->FuncAddTable(BindObject()->trace_object, query, description);
        ROCPROFVIS_ASSERT_MSG_RETURN(table, ERROR_TABLE_CANNOT_BE_NULL, kRocProfVisDmResultUnknownError);
        std::unordered_map<uint32_t, std::unordered_map<std::string, rocprofvis_db_compound_query_info>> queries;
        std::vector<rocprofvis_db_compound_query_command> commands;
        std::set<uint32_t> tracks;
        if (TableProcessor::IsCompoundQuery(query, queries, tracks,  commands))
        {
            auto it = std::find_if(commands.begin(), commands.end(), [](rocprofvis_db_compound_query_command& cmd) { return cmd.name == "TYPE"; });
            rocprofvis_db_compound_table_type data_type = kRPVTableDataTypeEvent;
            if (it != commands.end())
            {
                data_type = (rocprofvis_db_compound_table_type)std::atol(it->parameter.c_str());
            }
            bool query_updated = !m_table_processor[data_type].IsCurrentQuery(queries);
            m_table_processor[data_type].SaveCurrentQuery(queries);
            if (kRocProfVisDmResultSuccess != m_table_processor[data_type].ExecuteCompoundQuery(future, queries, tracks, commands, table, query_updated)) break;
        }
        else
        {
            ShowProgress(100, "Direct database query is not supported!",kRPVDbSuccess, future);
            return future->SetPromise(kRocProfVisDmResultNotSupported);
        }

        ShowProgress(100, "Query successfully executed!",kRPVDbSuccess, future);
        return future->SetPromise(kRocProfVisDmResultSuccess);
    }
    ShowProgress(0, "Query could not be executed!", kRPVDbError, future );
    return future->SetPromise(future->Interrupted() ? kRocProfVisDmResultDbAbort : kRocProfVisDmResultDbAccessFailed); 
}

rocprofvis_dm_result_t  QueryManager::ReadTraceSlice( 
    rocprofvis_dm_timestamp_t start,
    rocprofvis_dm_timestamp_t end,
    rocprofvis_dm_hashed_timestamp_tag_t tag,
    rocprofvis_db_num_of_tracks_t num,
    rocprofvis_db_track_selection_t tracks,
    Future* future) {
    ROCPROFVIS_ASSERT_MSG_RETURN(future, ERROR_FUTURE_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
    ROCPROFVIS_ASSERT_MSG(BindObject()->trace_properties, ERROR_TRACE_PROPERTIES_CANNOT_BE_NULL);
    ROCPROFVIS_ASSERT_MSG(BindObject()->trace_properties->metadata_loaded, ERROR_METADATA_IS_NOT_LOADED);
    // We never used multiple tracks request for single slice. And with multinode support it becomes very cumbersome. 
    // Disabling this feature, but leave the interface untouched for now
    ROCPROFVIS_ASSERT_MSG(num == 1, ERROR_UNSUPPORTED_FEATURE);

    rocprofvis_dm_track_params_t* props = TrackPropertiesAt(*tracks);
    if(props->track_indentifiers.category == kRocProfVisDmPmcTrack)
    {
        return ReadTracePMCSlice(start, end, tag, tracks, true, true, future);
    }
    else
    {
        while (true)
        {
            std::string slice_query;
            slice_array_t slices;

            slices[*tracks]=BindObject()->FuncAddSlice(BindObject()->trace_object, *tracks, start, end, tag);
            rocprofvis_dm_result_t result = BuildSliceQuery(start, end, num, tracks, slice_query, slices);
            std::string query;

            if (result == kRocProfVisDmResultSuccess)
            {
                result = ExecuteSQLQuery(future, (DbInstance*)props->track_indentifiers.db_instance, slice_query.c_str(), &slices, m_callback_add_any_record);
                BindObject()->FuncCompleteSlice(slices[*tracks]);
            }

            if(kRocProfVisDmResultSuccess != result)
            {
                BindObject()->FuncRemoveSlice(BindObject()->trace_object, *tracks, slices[*tracks]);
                break;
            }
            ShowProgress(100 - future->Progress(), "Time slice successfully loaded!", kRPVDbSuccess, future);
            return future->SetPromise(kRocProfVisDmResultSuccess);
        }
    }

    ShowProgress(0, "Not all tracks are loaded!", kRPVDbError, future );
    return future->SetPromise(future->Interrupted() ? kRocProfVisDmResultDbAbort : kRocProfVisDmResultDbAccessFailed);    
}

rocprofvis_dm_result_t
QueryManager::ReadTracePMCSlice(rocprofvis_dm_timestamp_t            start,
    rocprofvis_dm_timestamp_t            end,
    rocprofvis_dm_hashed_timestamp_tag_t tag,
    rocprofvis_db_track_selection_t      track,
    bool left_neighbor, bool right_neighbor,
    Future* future){
    ROCPROFVIS_ASSERT_MSG_RETURN(future, ERROR_FUTURE_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
    ROCPROFVIS_ASSERT_MSG(BindObject()->trace_properties, ERROR_TRACE_PROPERTIES_CANNOT_BE_NULL);
    ROCPROFVIS_ASSERT_MSG(BindObject()->trace_properties->metadata_loaded, ERROR_METADATA_IS_NOT_LOADED);

    rocprofvis_dm_track_params_t* props = TrackPropertiesAt(*track);
    if(props->track_indentifiers.category == kRocProfVisDmPmcTrack)
    {
        while (true)
        {
            std::string slice_query;
            slice_array_t slices;

            slices[*track]=BindObject()->FuncAddSlice(BindObject()->trace_object, *track, start, end, tag);
            rocprofvis_dm_result_t result = BuildSliceQuery(start, end, 1, track, slice_query, slices);
            std::string query;

            if (result == kRocProfVisDmResultSuccess)
            {
                if (left_neighbor)
                {
                    result = BuildCounterSliceLeftNeighbourQuery(start, end, *track, query);
                    if (result != kRocProfVisDmResultSuccess) break;
                    result = ExecuteSQLQuery(future,(DbInstance*)props->track_indentifiers.db_instance, query.c_str(), &slices, m_callback_add_any_record);
                    if (result != kRocProfVisDmResultSuccess) break;
                }

                if (result == kRocProfVisDmResultSuccess)
                {
                    result = ExecuteSQLQuery(future, (DbInstance*)props->track_indentifiers.db_instance, slice_query.c_str(), &slices, m_callback_add_any_record);

                    if (result == kRocProfVisDmResultSuccess && right_neighbor)
                    {
                        query = "";
                        future->ResetRowCount();
                        if (BuildCounterSliceRightNeighbourQuery(start, end, *track, query) != kRocProfVisDmResultSuccess) break;
                        if (ExecuteSQLQuery(future, (DbInstance*)props->track_indentifiers.db_instance, query.c_str(), &slices, m_callback_add_any_record) != kRocProfVisDmResultSuccess) break;

                        if (future->GetProcessedRowsCount() == 0)
                        {
                            rocprofvis_db_record_data_t record; 
                            auto db_instance = (DbInstance*)props->track_indentifiers.db_instance;
                            record.pmc.timestamp = TraceProperties()->db_inst_end_time[db_instance->GuidIndex()]-TraceProperties()->db_inst_start_time[db_instance->GuidIndex()];
                            record.pmc.value = future->GetRuntimeStorageValue<double>(kRPVFutureStorageSampleValue,0);

                            if (BindObject()->FuncAddRecord(slices[*track], record) != kRocProfVisDmResultSuccess)
                                break;

                        }
                    }


                    BindObject()->FuncCompleteSlice(slices[*track]);

                }
            }

            if(kRocProfVisDmResultSuccess != result)
            {
                BindObject()->FuncRemoveSlice(BindObject()->trace_object, *track, slices[*track]);
                break;
            }
            ShowProgress(100 - future->Progress(), "Time slice successfully loaded!", kRPVDbSuccess, future);
            return future->SetPromise(kRocProfVisDmResultSuccess);
        }    
    }

    ShowProgress(0, "Not all tracks are loaded!", kRPVDbError, future );
    return future->SetPromise(future->Interrupted() ? kRocProfVisDmResultDbAbort : kRocProfVisDmResultDbAccessFailed);

}

rocprofvis_dm_result_t QueryManager::RunCacheQueries(Future* future, std::vector<std::pair<std::string, std::string>>& info_table_list, RpvSqliteExecuteQueryCallback   callback, bool async){
    std::vector<std::thread> threads;
    rocprofvis_dm_result_t result = kRocProfVisDmResultNotLoaded;

    auto get_info_table_task = [&](DbInstance* db_instance, std::string query, std::string tag) {
        Future* sub_future = future->AddSubFuture();
        result = ExecuteSQLQuery(sub_future, db_instance, query.c_str(), tag.c_str(), (rocprofvis_dm_handle_t)CachedTables(db_instance->GuidIndex()), callback);
        future->DeleteSubFuture(sub_future);
        };

    if (async)
    {
        for (auto& guid_info : DbInstances())
        {
            if (!ShouldProcessInstance(guid_info.first.FileIndex())) continue;
            for (auto table : info_table_list)
            {
                threads.emplace_back(
                    get_info_table_task,
                    &guid_info.first,
                    table.second,
                    table.first);
            }
        }
        for (auto& t : threads)
            t.join();


        if (result == kRocProfVisDmResultSuccess)
        {
            for (auto& guid_info : DbInstances())
            {
                if (!ShouldProcessInstance(guid_info.first.FileIndex())) continue;
                for (auto table : info_table_list)
                {
                    auto handle = CachedTables(guid_info.first.GuidIndex())->GetTableHandle(table.first.c_str());
                    BindObject()->FuncAddInfoTable(BindObject()->trace_object, guid_info.first.GuidIndex(), table.first.c_str(), handle);
                }
            }
        }
    }
    else
    {
        for (auto& guid_info : DbInstances())
        {
            if (!ShouldProcessInstance(guid_info.first.FileIndex())) continue;
            for (auto table : info_table_list)
            {
                get_info_table_task(&guid_info.first, table.second, table.first);
                auto handle = CachedTables(guid_info.first.GuidIndex())->GetTableHandle(table.first.c_str());
                BindObject()->FuncAddInfoTable(BindObject()->trace_object, guid_info.first.GuidIndex(), table.first.c_str(), handle);
            }
        }
    }
    return result;
}

uint32_t QueryManager::CalculateParallelProcessSplitCount(uint32_t track_index)
{ 
    uint32_t split_count = 1;
    if (TrackPropertiesAt(track_index)->record_count > SINGLE_THREAD_RECORDS_COUNT_LIMIT)
    {
        size_t total_event_count = 0;
        for (int j = kRocProfVisDmOperationLaunch; j < kRocProfVisDmNumOperation; j++)
        {
            total_event_count += TraceProperties()->events_count[j];
        }
        split_count = static_cast<uint32_t>((TrackPropertiesAt(track_index)->record_count * 10) / total_event_count);

        if (split_count == 0)
        {
            split_count = 1;
        }
        else if ((TrackPropertiesAt(track_index)->record_count / split_count) < SINGLE_THREAD_RECORDS_COUNT_LIMIT)
        {
            split_count = static_cast<uint32_t>((TrackPropertiesAt(track_index)->record_count + SINGLE_THREAD_RECORDS_COUNT_LIMIT) / SINGLE_THREAD_RECORDS_COUNT_LIMIT);
        }
    }
    return split_count;
}

rocprofvis_dm_result_t
QueryManager::ExecuteQueryForAllTracksAsync(
    uint32_t flags, 
    rocprofvis_dm_index_t query_type,
    rocprofvis_dm_charptr_t prefix,
    rocprofvis_dm_charptr_t suffix,
    RpvSqliteExecuteQueryCallback callback,
    std::function<std::string(rocprofvis_dm_track_params_t*, rocprofvis_dm_charptr_t)> func_prepare,
    std::function<void(rocprofvis_dm_track_params_t*)> func_clear,
    guid_list_t run_for_db_instances)
{
    std::vector<Future*> futures;
    rocprofvis_dm_index_t  qtype  = query_type;
    rocprofvis_dm_result_t result = kRocProfVisDmResultSuccess;
    futures.reserve(NumTracks());
    for(int i = 0; i < NumTracks(); i++)
    {
        DbInstance* db_instance = (DbInstance*)TrackPropertiesAt(i)->track_indentifiers.db_instance;
        ROCPROFVIS_ASSERT_MSG_RETURN(db_instance != nullptr, ERROR_NODE_KEY_CANNOT_BE_NULL, kRocProfVisDmResultUnknownError);
        if (std::find_if(run_for_db_instances.begin(), run_for_db_instances.end(), [db_instance](GuidInfo& guid_info) 
            { return guid_info.first.GuidIndex() == db_instance->GuidIndex(); }) == run_for_db_instances.end())
        {
            continue;
        }       
        if(TrackPropertiesAt(i)->track_indentifiers.category != kRocProfVisDmPmcTrack && (flags & kRocProfVisDmIncludePmcTracksOnly))
        {
            continue;
        }
        if(TrackPropertiesAt(i)->track_indentifiers.category == kRocProfVisDmPmcTrack && (flags & (kRocProfVisDmIncludePmcTracks | kRocProfVisDmIncludePmcTracksOnly)) == 0)
        {
            continue;
        }
        if(TrackPropertiesAt(i)->track_indentifiers.category == kRocProfVisDmStreamTrack && (flags & kRocProfVisDmIncludeStreamTracks) == 0)
        {
            continue;
        }
        if (kRPVRocpdQuerySliceByTrackSliceQuery == query_type)
        {
            qtype = kRPVRocpdQuerySliceByQueue;
            if(TrackPropertiesAt(i)->track_indentifiers.category == kRocProfVisDmStreamTrack)
            {
                qtype = kRPVRocpdQuerySliceByStream; 
            }
        }

        uint32_t split_count = (flags & kRocProfVisDmTrySplitTrack) ? CalculateParallelProcessSplitCount(i) : 1;

        for (uint32_t j = 0; j < split_count; j++)
        {
            futures.push_back((Future*)rocprofvis_db_future_alloc(nullptr));
            std::string async_query = func_prepare(TrackPropertiesAt(i), prefix);
            async_query += std::to_string(i);
            async_query += " AS _track_id_ ";

            if (BuildTrackQuery(i, qtype, async_query, split_count, j) !=
                kRocProfVisDmResultSuccess)
            {
                continue;
            }
            async_query += suffix;
            futures.back()->SetAsyncQuery(async_query);

            try
            {
                futures.back()->SetWorker(std::move(
                    std::thread(QueryManager::ExecuteSQLQueryStatic, this,
                        futures.back(),
                        db_instance,
                        futures.back()->GetAsyncQueryPtr(), callback)));
            }
            catch (const std::exception& ex)
            {
                result = kRocProfVisDmResultUnknownError;
                ROCPROFVIS_ASSERT_MSG_BREAK(false, ex.what());
            }
        }
    }
    for(int i = 0; i < futures.size(); i++)
    {
        if(futures[i] != nullptr)
        {
            if(kRocProfVisDmResultSuccess !=
                rocprofvis_db_future_wait(futures[i], UINT64_MAX))
            {
                result = kRocProfVisDmResultUnknownError;
            }
            rocprofvis_db_future_free(futures[i]);
        }

    }
    for (int i = 0; i < NumTracks(); i++)
    {
        func_clear(TrackPropertiesAt(i));
    }
    return result;
}

rocprofvis_dm_result_t
QueryManager::ExecuteQueriesAsync(
    std::vector<std::pair<DbInstance*, std::string>>& queries,
    Future* parent,
    rocprofvis_dm_handle_t handle,
    RpvSqliteExecuteQueryCallback callback)
{
    rocprofvis_dm_result_t result = kRocProfVisDmResultSuccess;
    // Register each worker as a sub-future of the parent so it is reachable by
    // the parent's SetInterrupted() (cancellation) and is freed through the
    // mutex-protected DeleteSubFuture() path. AddSubFuture() both allocates and
    // registers; WaitAndDeleteSubFuture() waits, then unregisters and frees.
    std::vector<Future*> futures(queries.size(), nullptr);
    for(int i = 0; i < queries.size(); i++)
    {
        futures[i] = parent->AddSubFuture();
        try
        {
            futures[i]->SetWorker(std::move(
                std::thread(ExecuteSQLQueryStaticWithHandle, this,
                    futures[i],
                    queries[i].first,
                    queries[i].second.c_str(), handle, i, callback)));
        } catch(const std::exception& ex)
        {
            // The worker thread never started, so the sub-future's promise will
            // never be set; unregister and free it now to avoid a wait below
            // that would block forever on an unfulfilled promise.
            parent->DeleteSubFuture(futures[i]);
            futures[i] = nullptr;
            result = kRocProfVisDmResultUnknownError;
            ROCPROFVIS_ASSERT_MSG_BREAK(false, ex.what());
        }       
    }
    for(int i = 0; i < queries.size(); i++)
    {
        if(futures[i] != nullptr)
        {
            if(kRocProfVisDmResultSuccess !=
                parent->WaitAndDeleteSubFuture(futures[i]))
            {
                result = kRocProfVisDmResultUnknownError;
            }
            futures[i] = nullptr;
        }
    }
    return result;
}


std::string QueryManager::GetHistogramQueryPrefix(uint64_t bucket_size)
{
    const char* start_time_substring = "%START_TIME%";
    const char* histogram_content_version = "4";

    std::string histogram_query_prefix = "WITH params AS ( SELECT ";
    histogram_query_prefix += start_time_substring;
    histogram_query_prefix += " AS start_time, ";
    histogram_query_prefix += std::to_string(bucket_size);
    histogram_query_prefix += " AS bucket_size, ";
    histogram_query_prefix += histogram_content_version;
    histogram_query_prefix += " AS version ), ";
    histogram_query_prefix += "events_src AS ( SELECT (id + (";
    histogram_query_prefix += Builder::OPERATION_SERVICE_NAME;
    histogram_query_prefix += " << 60)) as event_id, ";
    histogram_query_prefix += Builder::START_SERVICE_NAME;
    histogram_query_prefix += " as start_ts, ";
    histogram_query_prefix += Builder::END_SERVICE_NAME;
    histogram_query_prefix += " as end_ts, ";
    return histogram_query_prefix;
}

std::string QueryManager::GetHistogramQuerySuffix()
{
    std::string histogram_query_suffix = "), ";
    histogram_query_suffix += "event_bucket_ranges AS( "
        "SELECT "
        "e._track_id_, "
        "e.event_id, "
        "e.start_ts, "
        "e.end_ts, "
        "MAX(0, (e.start_ts - p.start_time) / p.bucket_size) AS start_bucket, "
        "MAX(0, (e.end_ts - 1 - p.start_time) / p.bucket_size) AS end_bucket "
        "FROM events_src e "
        "JOIN params p"
        "), ";
    histogram_query_suffix += "expanded_buckets AS ("
        "SELECT "
        "_track_id_, "
        "event_id, "
        "start_ts, "
        "end_ts, "
        "start_bucket AS bucket_no, "
        "end_bucket "
        "FROM event_bucket_ranges "
        "UNION ALL "
        "SELECT "
        "_track_id_, "
        "event_id, "
        "start_ts, "
        "end_ts, "
        "bucket_no + 1, "
        "end_bucket "
        "FROM expanded_buckets "
        "WHERE bucket_no < end_bucket "
        "),";
    histogram_query_suffix += "bucket_events AS ("
        "SELECT "
        "eb._track_id_, "
        "eb.bucket_no,"
        "eb.event_id,"
        "MAX(eb.start_ts, p.start_time + eb.bucket_no * p.bucket_size ) AS overlap_start, "
        "MIN(eb.end_ts, p.start_time + (eb.bucket_no + 1) * p.bucket_size ) AS overlap_end "
        "FROM expanded_buckets eb "
        "JOIN params p "
        ") ";
    histogram_query_suffix += "SELECT "
        "bucket_no, "
        "COUNT(DISTINCT event_id) AS event_count, "
        "SUM(overlap_end - overlap_start)  AS total_duration, "
        "_track_id_ "
        "FROM bucket_events "
        "WHERE overlap_end > overlap_start "
        "GROUP BY bucket_no "
        "ORDER BY bucket_no ";
    return histogram_query_suffix;
}


rocprofvis_dm_result_t QueryManager::ExportTableCSV(rocprofvis_dm_charptr_t query,
    rocprofvis_dm_charptr_t file_path,
    Future* future)
{
    ROCPROFVIS_ASSERT_MSG_RETURN(file_path, "Output path cannot be NULL.", kRocProfVisDmResultInvalidParameter);
    ROCPROFVIS_ASSERT_MSG_RETURN(future, ERROR_FUTURE_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
    ROCPROFVIS_ASSERT_MSG_RETURN(BindObject()->trace_object, ERROR_TRACE_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
    rocprofvis_dm_result_t result = kRocProfVisDmResultInvalidParameter;
    Future* internal_future = future->AddSubFuture();
    result = ExecuteQuery(query, "ExportTableCSV", internal_future);
    future->WaitAndDeleteSubFuture(internal_future);
    if (result == kRocProfVisDmResultSuccess)
    {
        rocprofvis_db_compound_table_type data_type = kRPVTableDataTypeEvent;
        std::unordered_map<uint32_t, std::unordered_map<std::string, rocprofvis_db_compound_query_info>> queries;
        std::vector<rocprofvis_db_compound_query_command> commands;
        std::set<uint32_t> tracks;
        TableProcessor::IsCompoundQuery(query, queries, tracks,  commands);
        auto it = std::find_if(commands.begin(), commands.end(), [](rocprofvis_db_compound_query_command& cmd) { return cmd.name == "TYPE"; });
        if (it != commands.end())
        {
            data_type = (rocprofvis_db_compound_table_type)std::atol(it->parameter.c_str());
            result = m_table_processor[data_type].ExportToCSV(file_path);
            if (result == kRocProfVisDmResultSuccess)
            {
                ShowProgress(100, "CSV export success", kRPVDbSuccess, future);        
            }
        }
    }
    BindObject()->FuncRemoveTable(BindObject()->trace_object, query);
    if (result != kRocProfVisDmResultSuccess)
    {
        ShowProgress(0, "CSV export failed", kRPVDbError, future);
    }
    return future->SetPromise(result);
}


}  // namespace DataModel
}  // namespace RocProfVis
