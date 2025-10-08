// Copyright (c) 2025 Advanced Micro Devices, Inc. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "rocprofvis_db_profile.h"
#include "rocprofvis_c_interface.h"
#include <sstream>

namespace RocProfVis
{
namespace DataModel
{

    
rocprofvis_event_data_category_enum_t
ProfileDatabase::GetColumnDataCategory( const rocprofvis_event_data_category_map_t category_map,
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


int ProfileDatabase::CallbackMakeHistogramPerTrack(void* data, int argc, sqlite3_stmt* stmt,
    char** azColName) {
    ROCPROFVIS_ASSERT_MSG_RETURN(argc == 3, ERROR_DATABASE_QUERY_PARAMETERS_MISMATCH, 1);
    ROCPROFVIS_ASSERT_MSG_RETURN(data, ERROR_SQL_QUERY_PARAMETERS_CANNOT_BE_NULL, 1);
    void *func = (void*)&CallbackMakeHistogramPerTrack;
    rocprofvis_db_sqlite_callback_parameters* callback_params =
        (rocprofvis_db_sqlite_callback_parameters*) data;
    ProfileDatabase* db = (ProfileDatabase*) callback_params->db;
    if(callback_params->future->Interrupted()) return SQLITE_ABORT;
    uint32_t index                             = db->Sqlite3ColumnInt(func, stmt, azColName, 2);
    uint32_t bucket_number = db->Sqlite3ColumnInt(func, stmt, azColName, 0);
    uint32_t events_count = db->Sqlite3ColumnInt(func, stmt, azColName, 1);
    db->TrackPropertiesAt(index)->histogram[bucket_number] = events_count;
    callback_params->future->CountThisRow();
    return 0;
}

int
ProfileDatabase::CallbackGetTrackRecordsCount(void* data, int argc, sqlite3_stmt* stmt,
                                            char** azColName)
{
    ROCPROFVIS_ASSERT_MSG_RETURN(argc == 3, ERROR_DATABASE_QUERY_PARAMETERS_MISMATCH, 1);
    ROCPROFVIS_ASSERT_MSG_RETURN(data, ERROR_SQL_QUERY_PARAMETERS_CANNOT_BE_NULL, 1);
    void *func = (void*)&CallbackGetTrackRecordsCount;
    rocprofvis_db_sqlite_callback_parameters* callback_params =
        (rocprofvis_db_sqlite_callback_parameters*) data;
    ProfileDatabase* db = (ProfileDatabase*) callback_params->db;
    if(callback_params->future->Interrupted()) return SQLITE_ABORT;
    uint32_t index                             = db->Sqlite3ColumnInt(func, stmt, azColName, 2);
    db->TrackPropertiesAt(index)->record_count = db->Sqlite3ColumnInt64(func, stmt, azColName, 0);
    int op                                     = db->Sqlite3ColumnInt(func, stmt, azColName, 1);
    db->TraceProperties()->events_count[op] += db->TrackPropertiesAt(index)->record_count;
    callback_params->future->CountThisRow();
    return 0;
}

int
ProfileDatabase::CallbackTrimTableQuery(void* data, int argc, sqlite3_stmt* stmt,
                                                char** azColName)
{
    ROCPROFVIS_ASSERT_MSG_RETURN(data, ERROR_SQL_QUERY_PARAMETERS_CANNOT_BE_NULL, 1);
    void *func = (void*)&CallbackTrimTableQuery;
    rocprofvis_db_sqlite_callback_parameters* callback_params =
        (rocprofvis_db_sqlite_callback_parameters*) data;
    rocprofvis_db_sqlite_trim_parameters* params =
        (rocprofvis_db_sqlite_trim_parameters*) callback_params->handle;
    ProfileDatabase* db = (ProfileDatabase*) callback_params->db;
    if(callback_params->future->Interrupted()) return SQLITE_ABORT;

    char* table_name = (char*) db->Sqlite3ColumnText(func, stmt, azColName, 0);
    char* table_sql  = (char*) db->Sqlite3ColumnText(func, stmt, azColName, 1);
    params->tables.insert(std::make_pair(table_name, table_sql));

    callback_params->future->CountThisRow();
    return 0;
}

int ProfileDatabase::CallbackGetTrackProperties(void* data, int argc, sqlite3_stmt* stmt,
                                            char** azColName)
{
    ROCPROFVIS_ASSERT_MSG_RETURN(argc == 5, ERROR_DATABASE_QUERY_PARAMETERS_MISMATCH, 1);
    ROCPROFVIS_ASSERT_MSG_RETURN(data, ERROR_SQL_QUERY_PARAMETERS_CANNOT_BE_NULL, 1);
    void *func = (void*)&CallbackGetTrackProperties;
    rocprofvis_db_sqlite_callback_parameters* callback_params =
        (rocprofvis_db_sqlite_callback_parameters*) data;
    ProfileDatabase*            db = (ProfileDatabase*) callback_params->db;
    if(callback_params->future->Interrupted()) return SQLITE_ABORT;
    uint32_t index = db->Sqlite3ColumnInt(func, stmt, azColName, 4);
    db->TrackPropertiesAt(index)->min_ts       = std::min((rocprofvis_dm_timestamp_t)db->Sqlite3ColumnInt64(func, stmt, azColName, 0),db->TrackPropertiesAt(index)->min_ts);
    db->TrackPropertiesAt(index)->max_ts       = std::max((rocprofvis_dm_timestamp_t)db->Sqlite3ColumnInt64(func, stmt, azColName, 1),db->TrackPropertiesAt(index)->max_ts);
    db->TrackPropertiesAt(index)->min_value    = std::min((rocprofvis_dm_value_t)db->Sqlite3ColumnDouble(func, stmt, azColName, 2),db->TrackPropertiesAt(index)->min_value);
    db->TrackPropertiesAt(index)->max_value    = std::max((rocprofvis_dm_value_t)db->Sqlite3ColumnDouble(func, stmt, azColName, 3),db->TrackPropertiesAt(index)->max_value);

    db->TraceProperties()->start_time = std::min(db->TraceProperties()->start_time,db->TrackPropertiesAt(index)->min_ts);
    db->TraceProperties()->end_time  = std::max(db->TraceProperties()->end_time,db->TrackPropertiesAt(index)->max_ts);
    callback_params->future->CountThisRow();
    return 0;
}


int ProfileDatabase::CallbackAddAnyRecord(void* data, int argc, sqlite3_stmt* stmt, char** azColName) {
    ROCPROFVIS_ASSERT_MSG_RETURN(argc == rocprofvis_db_sqlite_slice_query_format::NUM_PARAMS,
                                 ERROR_DATABASE_QUERY_PARAMETERS_MISMATCH, 1);
    ROCPROFVIS_ASSERT_MSG_RETURN(data, ERROR_SQL_QUERY_PARAMETERS_CANNOT_BE_NULL, 1);
    void *func = (void*)&CallbackAddAnyRecord;
    rocprofvis_db_sqlite_callback_parameters* callback_params = (rocprofvis_db_sqlite_callback_parameters*)data;
    ProfileDatabase* db = (ProfileDatabase*)callback_params->db;
    if(callback_params->future->Interrupted()) return SQLITE_ABORT;
    rocprofvis_db_record_data_t record;
    record.event.id.bitfield.event_op = db->Sqlite3ColumnInt(func, stmt, azColName, 0);
    if (callback_params->track_id == -1)
    {
        uint64_t node = db->Sqlite3ColumnInt64(func, stmt, azColName, 6);
        uint64_t process = db->Sqlite3ColumnInt(func, stmt, azColName, 7);
        std::string subprocess = db->Sqlite3ColumnText(func, stmt, azColName, 8);
        if (db->FindTrackId(node, process, subprocess.c_str(), record.event.id.bitfield.event_op,
            callback_params->track_id) != kRocProfVisDmResultSuccess)
        {
            return 0;
        }
    }
    
    if(callback_params->track_id != -1)
    {
        if (record.event.id.bitfield.event_op > 0) {       
            record.event.id.bitfield.event_id = db->Sqlite3ColumnInt64(func, stmt, azColName, 5);
            record.event.timestamp = db->Sqlite3ColumnInt64(func, stmt, azColName, 1);
            record.event.duration = db->Sqlite3ColumnInt64(func, stmt, azColName, 2) - record.event.timestamp;
            record.event.category = db->Sqlite3ColumnInt64(func, stmt, azColName, 3);
            record.event.symbol = db->Sqlite3ColumnInt64(func, stmt, azColName, 4);
            record.event.level   = db->Sqlite3ColumnInt64(func, stmt, azColName, 9);
            if (kRocProfVisDmResultSuccess != db->RemapStringIds(record)) return 0;
        }
        else {
            record.pmc.timestamp = db->Sqlite3ColumnInt64(func, stmt, azColName, 1);
            record.pmc.value = db->Sqlite3ColumnInt(func, stmt, azColName,2);
        }
        if(db->BindObject()->FuncAddRecord(
               (*(slice_array_t*) callback_params->handle)[callback_params->track_id],
               record) != kRocProfVisDmResultSuccess)
            return 1;
        callback_params->future->CountThisRow();
        return 0;
    }
    return 1;
}

int ProfileDatabase::CallbackAddFlowTrace(void *data, int argc, sqlite3_stmt* stmt, char **azColName){
    ROCPROFVIS_ASSERT_MSG_RETURN(data, ERROR_SQL_QUERY_PARAMETERS_CANNOT_BE_NULL, 1);
    ROCPROFVIS_ASSERT_MSG_RETURN(argc == rocprofvis_db_sqlite_dataflow_query_format::NUM_PARAMS,
                                 ERROR_DATABASE_QUERY_PARAMETERS_MISMATCH, 1);
    void*  func = (void*)&CallbackAddFlowTrace;
    rocprofvis_db_sqlite_callback_parameters* callback_params = (rocprofvis_db_sqlite_callback_parameters*)data;
    ProfileDatabase* db = (ProfileDatabase*)callback_params->db;
    if(callback_params->future->Interrupted()) return SQLITE_ABORT;
    rocprofvis_db_flow_data_t record;

    record.id.bitfield.event_op = db->Sqlite3ColumnInt(func, stmt, azColName,0 );
    if (db->FindTrackId((uint64_t)db->Sqlite3ColumnInt64(func, stmt, azColName,3), 
                        (uint32_t)db->Sqlite3ColumnInt(func, stmt, azColName,4), 
                        (const char*)db->Sqlite3ColumnText(func, stmt, azColName,5),
                        record.id.bitfield.event_op, record.track_id) == kRocProfVisDmResultSuccess) {
        record.id.bitfield.event_id = db->Sqlite3ColumnInt64(func, stmt, azColName, 2 );
        record.time = db->Sqlite3ColumnInt64(func, stmt, azColName, 6 );
        record.category_id = db->Sqlite3ColumnInt64(func, stmt, azColName, 7);
        record.symbol_id = db->Sqlite3ColumnInt64(func, stmt, azColName, 8);
        record.level = db->Sqlite3ColumnInt64(func, stmt, azColName, 9);
        if(kRocProfVisDmResultSuccess != db->RemapStringIds(record)) return 0;
        if (db->BindObject()->FuncAddFlow(callback_params->handle,record) != kRocProfVisDmResultSuccess) return 1;
    }
    callback_params->future->CountThisRow();
    return 0;
}

int ProfileDatabase::CallbackAddExtInfo(void* data, int argc, sqlite3_stmt* stmt, char** azColName) {
    ROCPROFVIS_ASSERT_MSG_RETURN(data, ERROR_SQL_QUERY_PARAMETERS_CANNOT_BE_NULL, 1);
    void*  func = (void*)&CallbackAddExtInfo;
    rocprofvis_db_sqlite_callback_parameters* callback_params = (rocprofvis_db_sqlite_callback_parameters*)data;
    ProfileDatabase* db = (ProfileDatabase*)callback_params->db;
    rocprofvis_db_ext_data_t record;
    if (callback_params->future->Interrupted()) return SQLITE_ABORT;
    record.category = callback_params->query[kRPVCacheTableName];
    for (int i = 0; i < argc; i++)
    {
        record.name = azColName[i];
        record.type = (rocprofvis_db_data_type_t) sqlite3_column_type(stmt, i);
        record.data = (char*)db->Sqlite3ColumnText(func, stmt, azColName,i);
        record.category_enum = GetColumnDataCategory(*db->GetCategoryEnumMap(), callback_params->operation, record.name);
        if (record.data != nullptr) {
            if (db->BindObject()->FuncAddExtDataRecord(callback_params->handle, record) != kRocProfVisDmResultSuccess) return 1;
        }
    }  
    callback_params->future->CountThisRow();
    return 0;
}

int ProfileDatabase::CallbackAddEssentialInfo(void* data, int argc, sqlite3_stmt* stmt, char** azColName) {
    ROCPROFVIS_ASSERT_MSG_RETURN(data, ERROR_SQL_QUERY_PARAMETERS_CANNOT_BE_NULL, 1);
    ROCPROFVIS_ASSERT_MSG_RETURN(argc == rocprofvis_db_sqlite_essential_data_query_format::NUM_PARAMS,
                                 ERROR_DATABASE_QUERY_PARAMETERS_MISMATCH, 1);
    void*  func = (void*)&CallbackAddEssentialInfo;
    rocprofvis_db_sqlite_callback_parameters* callback_params = (rocprofvis_db_sqlite_callback_parameters*)data;
    ProfileDatabase* db = (ProfileDatabase*)callback_params->db;
    rocprofvis_db_ext_data_t record;
    if (callback_params->future->Interrupted()) return SQLITE_ABORT;
    rocprofvis_db_sqlite_track_service_data_t service_data{};

    for(int i = 0; i < argc-2; i++)
    {
        std::string column = azColName[i];
        CollectTrackServiceData(db, stmt, i, azColName, service_data);
    }

    int trackId       = -1;
    int streamTrackId = -1;
    std::string column_data;

    FindTrackIDs(db, service_data, trackId, streamTrackId);

    if(trackId != -1)
    {
        record.category = "Track";
        record.name     = "trackId";
        record.type     = kRPVDataTypeInt;
        column_data     = std::to_string(trackId).c_str();
        record.data     = column_data.c_str();
        record.category_enum = kRocProfVisEventEssentialDataTrack;
        if(db->BindObject()->FuncAddExtDataRecord(callback_params->handle, record) !=
           kRocProfVisDmResultSuccess)
            return 1;
        record.category = "Track";
        record.name     = "levelForTrack";
        record.type     = kRPVDataTypeInt;
        column_data     = std::to_string(db->Sqlite3ColumnInt64(func, stmt, azColName, argc - 2));       
        record.data     = column_data.c_str();
        record.category_enum = kRocProfVisEventEssentialDataLevel;
        if(db->BindObject()->FuncAddExtDataRecord(callback_params->handle, record) !=
           kRocProfVisDmResultSuccess)
            return 1;
    }
    if(streamTrackId != -1)
    {
        record.category = "Track";
        record.name     = "streamTrackId";
        record.type     = kRPVDataTypeInt;
        column_data     = std::to_string(streamTrackId).c_str();
        record.data     = column_data.c_str();
        record.category_enum = kRocProfVisEventEssentialDataStreamTrack;
        if(db->BindObject()->FuncAddExtDataRecord(callback_params->handle, record) !=
           kRocProfVisDmResultSuccess)
            return 1;
        record.category = "Track";
        record.name     = "levelForStreamTrack";
        record.type     = kRPVDataTypeInt;
        column_data     = std::to_string(db->Sqlite3ColumnInt64(func, stmt, azColName, argc - 1));
        record.data     = column_data.c_str();
        record.category_enum = kRocProfVisEventEssentialDataStreamLevel;
        if(db->BindObject()->FuncAddExtDataRecord(callback_params->handle, record) !=
           kRocProfVisDmResultSuccess)
            return 1;
    }
  
    callback_params->future->CountThisRow();
    return 0;
}


rocprofvis_dm_result_t
ProfileDatabase::BuildTrackQuery(rocprofvis_dm_index_t index,
                                 rocprofvis_dm_index_t type,
                                 rocprofvis_dm_string_t& query,
                                 uint32_t split_count,
                                 uint32_t split_index)
{
    std::stringstream ss;
    int               size = TrackPropertiesAt(index)->query[type].size();
    ROCPROFVIS_ASSERT_MSG_RETURN(size, "Error! SQL query cannot be empty!", kRocProfVisDmResultUnknownError);
    ss << query << " FROM (";
    for(int i = 0; i < size; i++)
    {
        if(i > 0) ss << " UNION ALL ";
        ss << TrackPropertiesAt(index)->query[type][i];

        ss << " where ";
        if(TrackPropertiesAt(index)->process.category == kRocProfVisDmRegionMainTrack)
        {
            ss << "SAMPLE.id IS NULL and ";
        }
        int count = 0;
        for(int i = 0; i < NUMBER_OF_TRACK_IDENTIFICATION_PARAMETERS; i++)
        {
            if(TrackPropertiesAt(index)->process.tag[i] == "const")
            {
                continue;
            }
            if(count > 0)
            {
                ss << " and ";
            }
            ss << TrackPropertiesAt(index)->process.tag[i] << "==";
            if(TrackPropertiesAt(index)->process.is_numeric[i])
            {
                ss << TrackPropertiesAt(index)->process.id[i];
            }
            else
            {
                ss << "'" << TrackPropertiesAt(index)->process.name[i] << "'";
            }
            count++;
        }
        if (split_count > 1)
        {
            uint64_t trace_time = TraceProperties()->end_time - TraceProperties()->start_time;
            if (trace_time > 0)
            {
                uint64_t time_bucket_size = trace_time / split_count;
                ss << " and " << Builder::START_SERVICE_NAME << " BETWEEN " << TraceProperties()->start_time + (time_bucket_size * split_index);
                ss << " and " << TraceProperties()->start_time + (time_bucket_size * (split_index+1));
            }
        }
    }
    ss << ") ";
    query = ss.str();
    return kRocProfVisDmResultSuccess;
}


rocprofvis_dm_result_t
ProfileDatabase::ExecuteQueryForAllTracksAsync(
                                                uint32_t flags, 
                                                rocprofvis_dm_index_t query_type,
                                                rocprofvis_dm_charptr_t prefix,
                                                rocprofvis_dm_charptr_t suffix,
                                                RpvSqliteExecuteQueryCallback callback,
                                                std::function<void(rocprofvis_dm_track_params_t*)> func_clear)
{
    std::vector<Future*> futures;
    rocprofvis_dm_index_t  qtype  = query_type;
    rocprofvis_dm_result_t result = kRocProfVisDmResultSuccess;
    futures.reserve(NumTracks());
    for(int i = 0; i < NumTracks(); i++)
    {
        if(TrackPropertiesAt(i)->process.category == kRocProfVisDmPmcTrack && (flags & kRocProfVisDmIncludePmcTracks) == 0)
        {
            continue;
        }
        if(TrackPropertiesAt(i)->process.category == kRocProfVisDmStreamTrack && (flags & kRocProfVisDmIncludeStreamTracks) == 0)
        {
            continue;
        }
        if (kRPVQuerySliceByTrackSliceQuery == query_type)
        {
            qtype = kRPVQuerySliceByQueue;
            if(TrackPropertiesAt(i)->process.category == kRocProfVisDmStreamTrack)
            {
                qtype = kRPVQuerySliceByStream; 
            }
        }

        uint32_t split_count = 1;
        if ((flags & kRocProfVisDmTrySplitTrack) && TrackPropertiesAt(i)->record_count > 0)
        {
            size_t total_event_count = 0;
            for (int i = kRocProfVisDmOperationLaunch; i < kRocProfVisDmNumOperation; i++)
            {
                total_event_count += TraceProperties()->events_count[i];
            }
            split_count = (TrackPropertiesAt(i)->record_count * 10) / total_event_count;

            if (split_count == 0)
            {
                split_count = 1;
            }

        }

        for (int j = 0; j < split_count; j++)
        {
            futures.push_back((Future*)rocprofvis_db_future_alloc(nullptr));
            std::string async_query = prefix;
            async_query += std::to_string(i);

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
                    std::thread(SqliteDatabase::ExecuteSQLQueryStatic, this,
                        futures.back(),
                        futures.back()->GetAsyncQueryPtr(), callback)));
            }
            catch (std::exception ex)
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
        func_clear(TrackPropertiesAt(i));
    }
    return result;
}


rocprofvis_dm_result_t ProfileDatabase::BuildSliceQuery(rocprofvis_dm_timestamp_t start, rocprofvis_dm_timestamp_t end, rocprofvis_db_num_of_tracks_t num, rocprofvis_db_track_selection_t tracks, rocprofvis_dm_string_t& query, slice_array_t& slices) {
    slice_query_t slice_query_map;
    bool timed_query = false;
    for (int i = 0; i < num; i++){
        slices[tracks[i]]=BindObject()->FuncAddSlice(BindObject()->trace_object, tracks[i], start, end);
        rocprofvis_dm_track_params_t* props = TrackPropertiesAt(tracks[i]);
        int slice_query_category = props->process.category ==  kRocProfVisDmStreamTrack? kRPVQuerySliceByStream : kRPVQuerySliceByQueue;
        for (int j = 0; j < props->query[slice_query_category].size(); j++) {
            std::string q = props->query[slice_query_category][j]; 

            std::string tuple = "(";
            for (int k = 0; k < NUMBER_OF_TRACK_IDENTIFICATION_PARAMETERS; k++) {
                if (props->process.tag[k] != "const") {
                    if (tuple.length() > 1) tuple += ",";
                    tuple += props->process.tag[k];
                }
            }
            tuple += ")";
            q += " where ";
            if (props->process.category == kRocProfVisDmRegionMainTrack)
            {
                q += "SAMPLE.id IS NULL and ";
            }
            q += tuple;
            q += " IN (";
            tuple = "(";
            for (int k = 0; k < NUMBER_OF_TRACK_IDENTIFICATION_PARAMETERS; k++) {
                if (props->process.tag[k] != "const") {
                    if (tuple.length() > 1) tuple += ",";
                    std::string id = props->process.is_numeric[k] ? std::to_string(props->process.id[k]) : std::string("'") + props->process.name[k] + "'";
                    tuple += id;
                      
                }
            }
            tuple += ")";
            if (slice_query_map[q].length() > 0) slice_query_map[q] += ", ";
            slice_query_map[q] += tuple ;
        }
        if (start > TrackPropertiesAt(tracks[i])->min_ts ||
            end < TrackPropertiesAt(tracks[i])->max_ts)
        {
            timed_query = true;
        }
    }
    query = "SELECT * FROM ( ";
    for (std::map<std::string, std::string>::iterator it_query = slice_query_map.begin(); it_query != slice_query_map.end(); ++it_query) {
        if (it_query!=slice_query_map.begin()) query += " UNION ALL ";
        query += it_query->first;
        query += it_query->second;
        query += ")";
        if(timed_query)
        {
            query += " and ";
            query += Builder::START_SERVICE_NAME;
            query += " < ";
            query += std::to_string(end);
            query += " and ";
            query += Builder::END_SERVICE_NAME;
            query += " > ";
            query += std::to_string(start);
        }
    }
    query += ") ORDER BY level, ";
    query += Builder::START_SERVICE_NAME;
    query += ";";
    return kRocProfVisDmResultSuccess;

}

rocprofvis_dm_result_t
ProfileDatabase::BuildTableQuery(
    rocprofvis_dm_timestamp_t start, rocprofvis_dm_timestamp_t end,
    rocprofvis_db_num_of_tracks_t num, rocprofvis_db_track_selection_t tracks, rocprofvis_dm_charptr_t filter,
    rocprofvis_dm_charptr_t group, rocprofvis_dm_charptr_t group_cols, rocprofvis_dm_charptr_t sort_column,
    rocprofvis_dm_sort_order_t sort_order,
    uint64_t max_count, uint64_t offset, bool count_only, rocprofvis_dm_string_t& query)
{
    slice_query_t slice_query_map;
    for (int i = 0; i < num; i++){
        rocprofvis_dm_track_params_t* props = TrackPropertiesAt(tracks[i]);
        for(int j = 0; j < props->query[kRPVQueryTable].size(); j++)
        {
            std::string q     = props->query[kRPVQueryTable][j]; 
            std::string tuple = "(";
            for (int k = 0; k < NUMBER_OF_TRACK_IDENTIFICATION_PARAMETERS; k++) {
                if (props->process.tag[k] != "const") {
                    if (tuple.length() > 1) tuple += ",";
                    //if(props->process.is_numeric[k]) tuple += "coalesce(";
                    tuple += props->process.tag[k];
                    //if(props->process.is_numeric[k]) tuple += ",0)";
                }
            }
            tuple += ")";
            q += " where ";
            if(props->process.category == kRocProfVisDmRegionMainTrack)
            {
                q += "SAMPLE.id IS NULL and ";
            }
            q += tuple;
            q += " IN (";
            tuple = "(";
            for (int k = 0; k < NUMBER_OF_TRACK_IDENTIFICATION_PARAMETERS; k++) {
                if (props->process.tag[k] != "const") {
                    if (tuple.length() > 1) tuple += ",";
                    std::string id = props->process.is_numeric[k] ? std::to_string(props->process.id[k]) : std::string("'") + props->process.name[k] + "'";
                    tuple += id;
                      
                }
            }
            tuple += ")";
            if (slice_query_map[q].length() > 0) slice_query_map[q] += ", ";
            slice_query_map[q] += tuple ;
        }
    }
    query = "WITH all_rows AS (";

    if (group && strlen(group))
    {
        query += "SELECT ";

        if (group_cols && strlen(group_cols))
        {
            query += group_cols;
        }
        else
        {
            query += group;
            query += ", COUNT(*) as num_invocations, AVG(duration) as avg_duration, "
            "MIN(duration) as min_duration, MAX(duration) as max_duration";
        }

        query += " FROM ( "; 
    }
    for (std::map<std::string, std::string>::iterator it_query = slice_query_map.begin(); it_query != slice_query_map.end(); ++it_query) {
        if (it_query!=slice_query_map.begin()) query += " UNION ALL ";
        query += it_query->first;
        query += it_query->second;
        query += ") and ";
        query += Builder::START_SERVICE_NAME;
        query += " >= ";
        query += std::to_string(start);
        query += " and ";
        query += Builder::END_SERVICE_NAME;
        query += " < ";
        query += std::to_string(end);
    }
    if (group && strlen(group))
    {
        query += ") GROUP BY ";
        query += group;
    }
    query += ")";
    if (filter && strlen(filter))
    {
        query += ", filtered_rows AS (SELECT * FROM all_rows WHERE (";
        query += filter;
        query += "))";
        if(count_only)
        {
            query += " SELECT (SELECT COUNT(*) FROM filtered_rows) AS [NumRecords], * FROM filtered_rows "; 
        }
        else
        {
            query += " SELECT * FROM filtered_rows "; 
        }
    }
    else
    {
        if(count_only)
        {
            query += " SELECT (SELECT COUNT(*) FROM all_rows) AS [NumRecords], * FROM all_rows "; 
        }
        else
        {
            query += " SELECT * FROM all_rows "; 
        }
    }
    if (sort_column && strlen(sort_column))
    {
        query += " ORDER BY ";
        query += sort_column;

        if (sort_order == kRPVDMSortOrderAsc)
        {
            query += " ASC";
        }
        else
        {
            query += " DESC";
        }
    }
    if(!count_only)
    {
        if(max_count)
        {
            query += " LIMIT ";
            query += std::to_string(max_count);
        }
        if(offset)
        {
            query += " OFFSET ";
            query += std::to_string(offset);
        }
    }
    else
    {
        query += " LIMIT 1";
    }
    query += ";";
    return kRocProfVisDmResultSuccess;
}

rocprofvis_dm_result_t  ProfileDatabase::ReadTraceSlice( 
                                                    rocprofvis_dm_timestamp_t start,
                                                    rocprofvis_dm_timestamp_t end,
                                                    rocprofvis_db_num_of_tracks_t num,
                                                    rocprofvis_db_track_selection_t tracks,
                                                    Future* future) {
    ROCPROFVIS_ASSERT_MSG_RETURN(future, ERROR_FUTURE_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
    while (true)
    {
        ROCPROFVIS_ASSERT_MSG_BREAK(BindObject()->trace_properties, ERROR_TRACE_PROPERTIES_CANNOT_BE_NULL);
        ROCPROFVIS_ASSERT_MSG_BREAK(BindObject()->trace_properties->metadata_loaded, ERROR_METADATA_IS_NOT_LOADED);

        std::string query;
        slice_array_t slices;
        if (BuildSliceQuery(start, end, num, tracks, query, slices) != kRocProfVisDmResultSuccess) break;
        ShowProgress(100, query.c_str(), kRPVDbBusy, future);
        rocprofvis_dm_result_t result =
            ExecuteSQLQuery(future, query.c_str(), &slices, &CallbackAddAnyRecord);
        for(int i = 0; i < num; i++)
        {
            BindObject()->FuncCompleteSlice(slices[tracks[i]]);
        }
        if(kRocProfVisDmResultSuccess != result)
        {
            for(int i = 0; i < num; i++)
            {
                BindObject()->FuncRemoveSlice(BindObject()->trace_object, (rocprofvis_dm_track_id_t)tracks[i], slices[tracks[i]]);
            }
            break;
        }
        ShowProgress(100 - future->Progress(), "Time slice successfully loaded!", kRPVDbSuccess, future);
        return future->SetPromise(kRocProfVisDmResultSuccess);

    }

    ShowProgress(0, "Not all tracks are loaded!", kRPVDbError, future );
    return future->SetPromise(future->Interrupted() ? kRocProfVisDmResultDbAbort : kRocProfVisDmResultDbAccessFailed);
}

rocprofvis_dm_result_t  ProfileDatabase::ExecuteQuery(
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
        if (kRocProfVisDmResultSuccess != ExecuteSQLQuery(future, query, table, &CallbackRunQuery)) break;
        ShowProgress(100, "Query successfully executed!",kRPVDbSuccess, future);
        return future->SetPromise(kRocProfVisDmResultSuccess);
    }
    ShowProgress(0, "Query could not be executed!", kRPVDbError, future );
    return future->SetPromise(future->Interrupted() ? kRocProfVisDmResultDbAbort : kRocProfVisDmResultDbAccessFailed); 
}



rocprofvis_db_type_t ProfileDatabase::Detect(rocprofvis_db_filename_t filename){
    sqlite3 *db;
    if( sqlite3_open(filename, &db) != SQLITE_OK) return rocprofvis_db_type_t::kAutodetect;

    if (DetectTable(db, "rocpd_event") == SQLITE_OK) {
        sqlite3_close(db);
        return rocprofvis_db_type_t::kRocprofSqlite;
    }

    if (DetectTable(db, "api") == SQLITE_OK) {
        sqlite3_close(db);
        return rocprofvis_db_type_t::kRocpdSqlite;
    }
    
    sqlite3_close(db);
    return rocprofvis_db_type_t::kAutodetect;
}


int ProfileDatabase::CalculateEventLevels(void* data, int argc, sqlite3_stmt* stmt, char** azColName)
{
    ROCPROFVIS_ASSERT_MSG_RETURN(argc == 9 , ERROR_DATABASE_QUERY_PARAMETERS_MISMATCH, 1);
    ROCPROFVIS_ASSERT_MSG_RETURN(data, ERROR_SQL_QUERY_PARAMETERS_CANNOT_BE_NULL, 1);
    void* func = (void*)&CalculateEventLevels;
    rocprofvis_db_sqlite_callback_parameters* callback_params =
        (rocprofvis_db_sqlite_callback_parameters*) data;
    ProfileDatabase* db = (ProfileDatabase*) callback_params->db;
    if(callback_params->future->Interrupted())
    {
        return 1;
    }
    uint32_t op = db->Sqlite3ColumnInt(func, stmt, azColName, 0);
    if (op == kRocProfVisDmOperationNoOp)
    {
        return 0;
    }
    uint64_t start_time = db->Sqlite3ColumnInt64(func, stmt, azColName, 1);
    uint64_t end_time   = db->Sqlite3ColumnInt64(func, stmt, azColName, 2);
    uint64_t id = db->Sqlite3ColumnInt64(func, stmt, azColName, 3);    
    uint32_t track = db->Sqlite3ColumnInt(func, stmt, azColName, 8);
    uint8_t level=0;
    rocprofvis_dm_track_params_t* params     = db->TrackPropertiesAt(track);
    ROCPROFVIS_ASSERT_MSG_RETURN(params!=0, ERROR_TRACE_PROPERTIES_CANNOT_BE_NULL, 1);


    auto it = params->m_active_events.begin();
    while(it != params->m_active_events.end())
    {   
        auto next_it = std::next(it, 1);
        if(start_time >= it->end_time)
        {
            params->m_active_events.erase(it);
        } 
        else if(end_time >= it->start_time)
        {
            if(level < it->level)
            {
                break;
            }

            level = it->level + 1;
        }
        it = next_it;
    }
    params->m_active_events.push_back({id | (uint64_t)op << 60, start_time, end_time, level });
    callback_params->future->CountThisRow();
    {
        std::lock_guard<std::mutex> lock(db->m_level_lock);
        auto                        it = db->m_event_levels_id_to_index[op].find(id);
        int                         index = 0;
        if(it == db->m_event_levels_id_to_index[op].end())
        {
            db->m_event_levels_id_to_index[op][id] = index = db->m_event_levels[op].size();
            db->m_event_levels[op].push_back({ id });
        }
        else
        {
            index = it->second;
        }
        if(params->process.category == kRocProfVisDmStreamTrack)
        {
            db->m_event_levels[op][index].level_for_stream = level;
        }
        else
        {
            db->m_event_levels[op][index].level_for_queue = level;
        }

    }
    return 0;
}

}  // namespace DataModel
}  // namespace RocProfVis