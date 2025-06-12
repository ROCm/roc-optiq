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

int ProfileDatabase::CallbackGetTrackProperties(void* data, int argc, sqlite3_stmt* stmt,
                                            char** azColName)
{
    ROCPROFVIS_ASSERT_MSG_RETURN(argc == 5, ERROR_DATABASE_QUERY_PARAMETERS_MISMATCH, 1);
    ROCPROFVIS_ASSERT_MSG_RETURN(data, ERROR_SQL_QUERY_PARAMETERS_CANNOT_BE_NULL, 1);
    rocprofvis_db_sqlite_callback_parameters* callback_params =
        (rocprofvis_db_sqlite_callback_parameters*) data;
    ProfileDatabase*            db = (ProfileDatabase*) callback_params->db;
    if(callback_params->future->Interrupted()) return 1;
    uint32_t index = sqlite3_column_int(stmt, 4);
    db->TrackPropertiesAt(index)->record_count = sqlite3_column_int64(stmt, 0);
    db->TrackPropertiesAt(index)->min_ts       = sqlite3_column_int64(stmt, 1);
    db->TrackPropertiesAt(index)->max_ts       = sqlite3_column_int64(stmt, 2);
    int op                                     = sqlite3_column_int(stmt, 3);
    db->TraceProperties()->events_count[op] += db->TrackPropertiesAt(index)->record_count;
    db->TraceProperties()->start_time = std::min(db->TraceProperties()->start_time,db->TrackPropertiesAt(index)->min_ts);
    db->TraceProperties()->end_time  = std::max(db->TraceProperties()->end_time,db->TrackPropertiesAt(index)->max_ts);
    callback_params->future->CountThisRow();
    return 0;
}

int ProfileDatabase::CallbackAddEventRecord(void *data, int argc, sqlite3_stmt* stmt, char **azColName){
    ROCPROFVIS_ASSERT_MSG_RETURN(argc==10, ERROR_DATABASE_QUERY_PARAMETERS_MISMATCH, 1);
    ROCPROFVIS_ASSERT_MSG_RETURN(data, ERROR_SQL_QUERY_PARAMETERS_CANNOT_BE_NULL, 1);
    rocprofvis_db_sqlite_callback_parameters* callback_params = (rocprofvis_db_sqlite_callback_parameters*)data;
    ProfileDatabase* db = (ProfileDatabase*)callback_params->db;
    rocprofvis_db_record_data_t record;
    if (callback_params->future->Interrupted()) return 1;
    record.event.id.bitfield.event_op = sqlite3_column_int(stmt, 0);
    record.event.id.bitfield.event_id = sqlite3_column_int64(stmt, 5);
    record.event.timestamp = sqlite3_column_int64(stmt, 1);
    record.event.duration = sqlite3_column_int64(stmt, 2) - record.event.timestamp;
    record.event.category = sqlite3_column_int(stmt, 3);
    record.event.symbol = sqlite3_column_int(stmt, 4)+(record.event.id.bitfield.event_op==kRocProfVisDmOperationDispatch?db->m_symbols_offset:0);
    record.event.level = sqlite3_column_int(stmt, 9);
    
    if (db->BindObject()->FuncAddRecord(callback_params->handle,record) != kRocProfVisDmResultSuccess) return 1;  
 
    callback_params->future->CountThisRow();
    return 0;
}

int ProfileDatabase::CallbackAddPmcRecord(void *data, int argc, sqlite3_stmt* stmt, char **azColName){
    ROCPROFVIS_ASSERT_MSG_RETURN(data, ERROR_SQL_QUERY_PARAMETERS_CANNOT_BE_NULL, 1);
    ROCPROFVIS_ASSERT_MSG_RETURN(argc==2, ERROR_DATABASE_QUERY_PARAMETERS_MISMATCH, 1);
    rocprofvis_db_sqlite_callback_parameters* callback_params = (rocprofvis_db_sqlite_callback_parameters*)data;
    ProfileDatabase* db = (ProfileDatabase*)callback_params->db;
    rocprofvis_db_record_data_t record;
    if (callback_params->future->Interrupted()) return 1;
    record.pmc.timestamp = sqlite3_column_int64(stmt, 1);
    record.pmc.value = sqlite3_column_int(stmt,2 );
    if (db->BindObject()->FuncAddRecord(callback_params->handle,record) != kRocProfVisDmResultSuccess) return 1;
    callback_params->future->CountThisRow();
    return 0;
}

int ProfileDatabase::CallbackAddAnyRecord(void* data, int argc, sqlite3_stmt* stmt, char** azColName) {
    ROCPROFVIS_ASSERT_MSG_RETURN(argc == 10, ERROR_DATABASE_QUERY_PARAMETERS_MISMATCH, 1);
    ROCPROFVIS_ASSERT_MSG_RETURN(data, ERROR_SQL_QUERY_PARAMETERS_CANNOT_BE_NULL, 1);
    rocprofvis_db_sqlite_callback_parameters* callback_params = (rocprofvis_db_sqlite_callback_parameters*)data;
    ProfileDatabase* db = (ProfileDatabase*)callback_params->db;
    if (callback_params->future->Interrupted()) return 1;
    rocprofvis_dm_track_id_t track_id;
    rocprofvis_db_record_data_t record;
    record.event.id.bitfield.event_op = sqlite3_column_int(stmt, 0);
    if (db->FindTrackId((char*)sqlite3_column_text(stmt,6), (char*)sqlite3_column_text(stmt,7), (char*)sqlite3_column_text(stmt,8), record.event.id.bitfield.event_op, track_id) == kRocProfVisDmResultSuccess) {
        if (record.event.id.bitfield.event_op > 0) {       
            record.event.id.bitfield.event_id = sqlite3_column_int64(stmt, 5);
            record.event.timestamp = sqlite3_column_int64(stmt, 1);
            record.event.duration = sqlite3_column_int64(stmt, 2) - record.event.timestamp;
            record.event.category = sqlite3_column_int64(stmt, 3);
            record.event.symbol = sqlite3_column_int64(stmt, 4);
            record.event.level   = sqlite3_column_int64(stmt, 9);
            if (kRocProfVisDmResultSuccess != db->RemapStringIds(record)) return 0;
        }
        else {
            record.pmc.timestamp = sqlite3_column_int64(stmt, 1);
            record.pmc.value = sqlite3_column_int(stmt,2);
        }
        if (db->BindObject()->FuncAddRecord((*(slice_array_t*)callback_params->handle)[track_id], record) != kRocProfVisDmResultSuccess) return 1;
    }
    callback_params->future->CountThisRow();
    return 0;
}

int ProfileDatabase::CallbackAddFlowTrace(void *data, int argc, sqlite3_stmt* stmt, char **azColName){
    ROCPROFVIS_ASSERT_MSG_RETURN(data, ERROR_SQL_QUERY_PARAMETERS_CANNOT_BE_NULL, 1);
    ROCPROFVIS_ASSERT_MSG_RETURN(argc==7, ERROR_DATABASE_QUERY_PARAMETERS_MISMATCH, 1);
    rocprofvis_db_sqlite_callback_parameters* callback_params = (rocprofvis_db_sqlite_callback_parameters*)data;
    ProfileDatabase* db = (ProfileDatabase*)callback_params->db;
    if (callback_params->future->Interrupted()) return 1;
    rocprofvis_db_flow_data_t record;
    record.id.bitfield.event_op = sqlite3_column_int(stmt,1 );
    if (db->FindTrackId((char*)sqlite3_column_text(stmt,3), (char*)sqlite3_column_text(stmt,4), (char*)sqlite3_column_text(stmt,5), record.id.bitfield.event_op, record.track_id) == kRocProfVisDmResultSuccess) {
        record.id.bitfield.event_id = sqlite3_column_int64(stmt, 2 );
        record.time = sqlite3_column_int64(stmt, 6 );
        if (db->BindObject()->FuncAddFlow(callback_params->handle,record) != kRocProfVisDmResultSuccess) return 1;
    }
    callback_params->future->CountThisRow();
    return 0;
}

int ProfileDatabase::CallbackAddExtInfo(void* data, int argc, sqlite3_stmt* stmt, char** azColName) {
    ROCPROFVIS_ASSERT_MSG_RETURN(data, ERROR_SQL_QUERY_PARAMETERS_CANNOT_BE_NULL, 1);
    rocprofvis_db_sqlite_callback_parameters* callback_params = (rocprofvis_db_sqlite_callback_parameters*)data;
    ProfileDatabase* db = (ProfileDatabase*)callback_params->db;
    rocprofvis_db_ext_data_t record;
    if (callback_params->future->Interrupted()) return 1;
    record.category = callback_params->subquery;
    for (int i = 0; i < argc; i++)
    {
        record.name = azColName[i];
        record.data = (char*)sqlite3_column_text(stmt,i);
        if (record.data != nullptr) {
            if (db->BindObject()->FuncAddExtDataRecord(callback_params->handle, record) != kRocProfVisDmResultSuccess) return 1;
        }
    }  
    callback_params->future->CountThisRow();
    return 0;
}

rocprofvis_dm_result_t
ProfileDatabase::BuildTrackQuery(rocprofvis_dm_index_t index,
                                 rocprofvis_dm_string_t& query, 
                                 bool for_time_slice)
{
    std::stringstream ss;
    int size = TrackPropertiesAt(index)->query.size();
    ROCPROFVIS_ASSERT_MSG_RETURN(size, "Error! SQL query cannot be empty!", kRocProfVisDmResultUnknownError);
    ss << query << " FROM (";
    for (int i=0; i < size; i++){
        if (i > 0) ss << " UNION ";
        std::string q = TrackPropertiesAt(index)->query[i];
        if(!for_time_slice)
        {
            for(int i = 0; i < 2; i++)
            {
                size_t start = q.find('{');
                size_t end   = q.find('}');
                if(start != std::string::npos && end != std::string::npos && end > start)
                {
                    q.erase(start, end - start + 1);
                }
            }
        }
        else
        {
            std::replace(q.begin(), q.end(), '{', ' ');
            std::replace(q.begin(), q.end(), '}', ' ');
        }
        ss << q;
    } 
    ss << ") where ";
    int count = 0;
    for (int i = 0; i < NUMBER_OF_TRACK_IDENTIFICATION_PARAMETERS; i++) {
        if(TrackPropertiesAt(index)->process_tag[i] == "const")
        {
            continue;
        }
        if(count > 0)
        {
            ss << " and ";
        }
        ss << TrackPropertiesAt(index)->process_tag[i] << "=="; 
        if(TrackPropertiesAt(index)->process_id_numeric[i])
        {
            ss << TrackPropertiesAt(index)->process_id[i];
        }
        else
        {
            ss << "'" << TrackPropertiesAt(index)->process_name[i] << "'";
        }
        count++;
    }
    query = ss.str();
    return kRocProfVisDmResultSuccess;
}


rocprofvis_dm_result_t
ProfileDatabase::ExecuteQueryForAllTracksAsync(
                                                bool including_pmc,
                                                rocprofvis_dm_charptr_t prefix,
                                                rocprofvis_dm_charptr_t suffix,
                                                RpvSqliteExecuteQueryCallback callback,
                                                std::function<void(rocprofvis_dm_track_params_t*)> func_clear)
{
    std::vector<Future*> futures;
    rocprofvis_dm_result_t result = kRocProfVisDmResultSuccess;
    futures.resize(NumTracks());
    for(int i = 0; i < NumTracks(); i++)
    {
        if(TrackPropertiesAt(i)->track_category <= kRocProfVisDmPmcTrack && !including_pmc)
        {
            continue;
        }
        TrackPropertiesAt(i)->async_query = prefix;
        TrackPropertiesAt(i)->async_query += std::to_string(i);
        if(BuildTrackQuery(i, TrackPropertiesAt(i)->async_query, false) !=
           kRocProfVisDmResultSuccess)
        {
            continue;
        }
        TrackPropertiesAt(i)->async_query += suffix;
        futures[i]     = (Future*)rocprofvis_db_future_alloc(nullptr);
        try
        {
            futures[i]->SetWorker(std::move(
                std::thread(SqliteDatabase::ExecuteSQLQueryStatic, this,
                            TrackPropertiesAt(i)->db_connection,
                            futures[i],
                            TrackPropertiesAt(i)->async_query.c_str(), callback)));
        } catch(std::exception ex)
        {
            result = kRocProfVisDmResultUnknownError;
            ROCPROFVIS_ASSERT_MSG_BREAK(false, ex.what());
        }       
    }
    for(int i = 0; i < NumTracks(); i++)
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
        for (int j = 0; j < props->query.size(); j++) {
            std::string q = props->query[j]; 
            std::replace(q.begin(), q.end(), '{', ' ');
            std::replace(q.begin(), q.end(), '}', ' ');
            std::string tuple = "(";
            for (int k = 0; k < NUMBER_OF_TRACK_IDENTIFICATION_PARAMETERS; k++) {
                if (props->process_tag[k] != "const") {
                    if (tuple.length() > 1) tuple += ",";
                    if(props->process_id_numeric[k]) tuple += "coalesce(";
                    tuple += props->process_tag[k];
                    if(props->process_id_numeric[k]) tuple += ",0)";
                }
            }
            tuple += ")";
            q += " where ";
            q += tuple;
            q += " IN (";
            tuple = "(";
            for (int k = 0; k < NUMBER_OF_TRACK_IDENTIFICATION_PARAMETERS; k++) {
                if (props->process_tag[k] != "const") {
                    if (tuple.length() > 1) tuple += ",";
                    std::string id = props->process_id_numeric[k] ? std::to_string(props->process_id[k]) : std::string("'") + props->process_name[k] + "'";
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
        if (it_query!=slice_query_map.begin()) query += " UNION ";
        query += it_query->first;
        query += it_query->second;
        query += ")";
        if(timed_query)
        {
            query += " and start < ";
            query += std::to_string(end);
            query += " and end > ";
            query += std::to_string(start);
        }
    }
    query += ") ORDER BY start;";
    return kRocProfVisDmResultSuccess;

}

rocprofvis_dm_result_t
ProfileDatabase::BuildTableQuery(
    rocprofvis_dm_timestamp_t start, rocprofvis_dm_timestamp_t end,
    rocprofvis_db_num_of_tracks_t num, rocprofvis_db_track_selection_t tracks,
    rocprofvis_dm_charptr_t sort_column, rocprofvis_dm_sort_order_t sort_order,
    uint64_t max_count, uint64_t offset, bool count_only, rocprofvis_dm_string_t& query)
{
    slice_query_t slice_query_map;
    for (int i = 0; i < num; i++){
        rocprofvis_dm_track_params_t* props = TrackPropertiesAt(tracks[i]);
        for (int j = 0; j < props->table_query.size(); j++) {
            std::string q = props->table_query[j]; 
            std::string tuple = "(";
            for (int k = 0; k < NUMBER_OF_TRACK_IDENTIFICATION_PARAMETERS; k++) {
                if (props->process_tag[k] != "const") {
                    if (tuple.length() > 1) tuple += ",";
                    if(props->process_id_numeric[k]) tuple += "coalesce(";
                    tuple += props->process_tag[k];
                    if(props->process_id_numeric[k]) tuple += ",0)";
                }
            }
            tuple += ")";
            q += " where ";
            q += tuple;
            q += " IN (";
            tuple = "(";
            for (int k = 0; k < NUMBER_OF_TRACK_IDENTIFICATION_PARAMETERS; k++) {
                if (props->process_tag[k] != "const") {
                    if (tuple.length() > 1) tuple += ",";
                    std::string id = props->process_id_numeric[k] ? std::to_string(props->process_id[k]) : std::string("'") + props->process_name[k] + "'";
                    tuple += id;
                      
                }
            }
            tuple += ")";
            if (slice_query_map[q].length() > 0) slice_query_map[q] += ", ";
            slice_query_map[q] += tuple ;
        }
    }
    if(count_only)
    {
        query = "SELECT COUNT(*) AS [NumRecords] FROM ( ";
    }
    else
    { 
        query = "SELECT * FROM ( "; 
    } 
    for (std::map<std::string, std::string>::iterator it_query = slice_query_map.begin(); it_query != slice_query_map.end(); ++it_query) {
        if (it_query!=slice_query_map.begin()) query += " UNION ";
        query += it_query->first;
        query += it_query->second;
        query += ") and start >= ";
        query += std::to_string(start);
        query += " and end < ";
        query += std::to_string(end);
    }
    query += ")";
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
#if defined(SLICE_OPT) && (SLICE_OPT == 2)
        double step = 100.0/num;
        int i=0;
        for (; i < num; i++)
        {
            ROCPROFVIS_ASSERT_MSG_BREAK(tracks[i] < NumTracks(), ERROR_INDEX_OUT_OF_RANGE);
            rocprofvis_dm_slice_t slice = BindObject()->FuncAddSlice(BindObject()->trace_object,tracks[i],start,end);
            ROCPROFVIS_ASSERT_MSG_BREAK(slice, ERROR_SLICE_CANNOT_BE_NULL);
            std::stringstream query;
            std::string subquery = "SELECT *";
            if(BuildTrackQuery(tracks[i], subquery,true) !=
               kRocProfVisDmResultSuccess)
                break;
            query << subquery
                << " start < " << end
                << " and end > " << start
                << " ORDER BY start;";
            ShowProgress(step, query.str().c_str(),kRPVDbBusy, future);
            if (kRocProfVisDmResultSuccess != ExecuteSQLQuery(TrackPropertiesAt(tracks[i])->db_connection,
                    future, query.str().c_str(), slice, TrackPropertiesAt(i)->track_category == kRocProfVisDmPmcTrack ? &CallbackAddPmcRecord : &CallbackAddEventRecord)) break;
        }

        if (i < num) break;
#else
        std::string query;
        slice_array_t slices;
        if (BuildSliceQuery(start, end, num, tracks, query, slices) != kRocProfVisDmResultSuccess) break;
        ShowProgress(100, query.c_str(), kRPVDbBusy, future);
        if (kRocProfVisDmResultSuccess != ExecuteSQLQuery((sqlite3*) TrackPropertiesAt(tracks[0])->db_connection, 
                future, query.c_str(), &slices , &CallbackAddAnyRecord)) break;
#endif
        ShowProgress(100 - future->Progress(), "Time slice successfully loaded!", kRPVDbSuccess, future);
        return future->SetPromise(kRocProfVisDmResultSuccess);

    }
    ShowProgress(0, "Not all tracks are loaded!", kRPVDbError, future );
    return future->SetPromise(future->Interrupted() ? kRocProfVisDmResultTimeout : kRocProfVisDmResultDbAccessFailed);
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
    return future->SetPromise(future->Interrupted() ? kRocProfVisDmResultTimeout : kRocProfVisDmResultDbAccessFailed); 
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
    ROCPROFVIS_ASSERT_MSG_RETURN(argc == 10, ERROR_DATABASE_QUERY_PARAMETERS_MISMATCH, 1);
    ROCPROFVIS_ASSERT_MSG_RETURN(data, ERROR_SQL_QUERY_PARAMETERS_CANNOT_BE_NULL, 1);
    rocprofvis_db_sqlite_callback_parameters* callback_params =
        (rocprofvis_db_sqlite_callback_parameters*) data;
    ProfileDatabase* db = (ProfileDatabase*) callback_params->db;
    if(callback_params->future->Interrupted())
    {
        return 1;
    }
    uint64_t process_id[NUMBER_OF_TRACK_IDENTIFICATION_PARAMETERS];
    uint32_t op = sqlite3_column_int(stmt, 0);
    if (op == kRocProfVisDmOperationNoOp)
    {
        return 0;
    }
    uint64_t start_time = sqlite3_column_int64(stmt, 1);
    uint64_t end_time   = sqlite3_column_int64(stmt, 2);
    uint64_t id = sqlite3_column_int64(stmt, 5);    
    uint32_t track = sqlite3_column_int(stmt, 9);
    uint8_t level=0;
    rocprofvis_dm_track_params_t* params     = db->TrackPropertiesAt(track);
    ROCPROFVIS_ASSERT_MSG_RETURN(params!=0, ERROR_TRACE_PROPERTIES_CANNOT_BE_NULL, 1);
    
    auto it = params->m_event_timing_params.begin();
    while(it != params->m_event_timing_params.end())
    {   
        auto next_it = std::next(it, 1);
        if(start_time > it->end_time)
        {
            params->m_event_timing_params.erase(it);
        } 
        else if(start_time < it->end_time)
        {
            level = it->level + 1;
        }
        it = next_it;
    }
    params->m_event_timing_params.push_back({start_time, end_time, level });
    callback_params->future->CountThisRow();
    std::lock_guard<std::mutex> lock(db->m_level_lock);
    db->m_event_levels[op].push_back({ id, level });
    return 0;
}

}  // namespace DataModel
}  // namespace RocProfVis