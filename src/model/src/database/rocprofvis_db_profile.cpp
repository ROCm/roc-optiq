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
#include <sstream>

namespace RocProfVis
{
namespace DataModel
{


int ProfileDatabase::CallbackAddEventRecord(void *data, int argc, char **argv, char **azColName){
    ROCPROFVIS_ASSERT_MSG_RETURN(argc==9, ERROR_DATABASE_QUERY_PARAMETERS_MISMATCH, 1);
    ROCPROFVIS_ASSERT_MSG_RETURN(data, ERROR_SQL_QUERY_PARAMETERS_CANNOT_BE_NULL, 1);
    rocprofvis_db_sqlite_callback_parameters* callback_params = (rocprofvis_db_sqlite_callback_parameters*)data;
    ProfileDatabase* db = (ProfileDatabase*)callback_params->db;
    rocprofvis_db_record_data_t record;
    if (callback_params->future->Interrupted()) return 1;
    record.event.id.bitfield.event_op = std::stol(argv[0]);
    record.event.id.bitfield.event_id = std::stoll(argv[5]);
    record.event.timestamp = std::stoll( argv[1] );
    record.event.duration = std::stoll(argv[2]) - record.event.timestamp;
    record.event.category = std::stol(argv[3]);
    record.event.symbol = std::stol(argv[4])+(record.event.id.bitfield.event_op==kRocProfVisDmOperationDispatch?db->m_symbols_offset:0);
    
    if (db->BindObject()->FuncAddRecord(callback_params->handle,record) != kRocProfVisDmResultSuccess) return 1;  
 
    callback_params->future->CountThisRow();
    return 0;
}

int ProfileDatabase::CallbackAddPmcRecord(void *data, int argc, char **argv, char **azColName){
    ROCPROFVIS_ASSERT_MSG_RETURN(data, ERROR_SQL_QUERY_PARAMETERS_CANNOT_BE_NULL, 1);
    ROCPROFVIS_ASSERT_MSG_RETURN(argc==2, ERROR_DATABASE_QUERY_PARAMETERS_MISMATCH, 1);
    rocprofvis_db_sqlite_callback_parameters* callback_params = (rocprofvis_db_sqlite_callback_parameters*)data;
    ProfileDatabase* db = (ProfileDatabase*)callback_params->db;
    rocprofvis_db_record_data_t record;
    if (callback_params->future->Interrupted()) return 1;
    record.pmc.timestamp = std::stoll( argv[1] );
    record.pmc.value = std::stod( argv[2] );
    if (db->BindObject()->FuncAddRecord(callback_params->handle,record) != kRocProfVisDmResultSuccess) return 1;
    callback_params->future->CountThisRow();
    return 0;
}

int ProfileDatabase::CallbackAddAnyRecord(void* data, int argc, char** argv, char** azColName) {
    ROCPROFVIS_ASSERT_MSG_RETURN(argc == 9, ERROR_DATABASE_QUERY_PARAMETERS_MISMATCH, 1);
    ROCPROFVIS_ASSERT_MSG_RETURN(data, ERROR_SQL_QUERY_PARAMETERS_CANNOT_BE_NULL, 1);
    rocprofvis_db_sqlite_callback_parameters* callback_params = (rocprofvis_db_sqlite_callback_parameters*)data;
    ProfileDatabase* db = (ProfileDatabase*)callback_params->db;
    if (callback_params->future->Interrupted()) return 1;
    rocprofvis_dm_track_id_t track_id;
    rocprofvis_db_record_data_t record;
    record.event.id.bitfield.event_op = std::stol(argv[0]);
    if (db->FindTrackId(argv[6], argv[7], argv[8], record.event.id.bitfield.event_op, track_id) == kRocProfVisDmResultSuccess) {
        if (record.event.id.bitfield.event_op > 0) {       
            record.event.id.bitfield.event_id = std::stoll(argv[5]);
            record.event.timestamp = std::stoll(argv[1]);
            record.event.duration = std::stoll(argv[2]) - record.event.timestamp;
            record.event.category = std::stoll(argv[3]);
            record.event.symbol = std::stoll(argv[4]);
            if (kRocProfVisDmResultSuccess != db->RemapStringIds(record)) return 0;
        }
        else {
            record.pmc.timestamp = std::stoll(argv[1]);
            record.pmc.value = std::stod(argv[2]);
        }
        if (db->BindObject()->FuncAddRecord((*(slice_array_t*)callback_params->handle)[track_id], record) != kRocProfVisDmResultSuccess) return 1;
    }
    callback_params->future->CountThisRow();
    return 0;
}

int ProfileDatabase::CallbackAddFlowTrace(void *data, int argc, char **argv, char **azColName){
    ROCPROFVIS_ASSERT_MSG_RETURN(data, ERROR_SQL_QUERY_PARAMETERS_CANNOT_BE_NULL, 1);
    ROCPROFVIS_ASSERT_MSG_RETURN(argc==7, ERROR_DATABASE_QUERY_PARAMETERS_MISMATCH, 1);
    rocprofvis_db_sqlite_callback_parameters* callback_params = (rocprofvis_db_sqlite_callback_parameters*)data;
    ProfileDatabase* db = (ProfileDatabase*)callback_params->db;
    if (callback_params->future->Interrupted()) return 1;
    rocprofvis_db_flow_data_t record;
    record.id.bitfield.event_op = std::stoll( argv[1] );
    if (db->FindTrackId(argv[3], argv[4], argv[5], record.id.bitfield.event_op, record.track_id) == kRocProfVisDmResultSuccess) {
        record.id.bitfield.event_id = std::stoll( argv[2] );
        record.time = std::stoll( argv[6] );
        if (db->BindObject()->FuncAddFlow(callback_params->handle,record) != kRocProfVisDmResultSuccess) return 1;
    }
    callback_params->future->CountThisRow();
    return 0;
}

int ProfileDatabase::CallbackAddExtInfo(void* data, int argc, char** argv, char** azColName) {
    ROCPROFVIS_ASSERT_MSG_RETURN(data, ERROR_SQL_QUERY_PARAMETERS_CANNOT_BE_NULL, 1);
    rocprofvis_db_sqlite_callback_parameters* callback_params = (rocprofvis_db_sqlite_callback_parameters*)data;
    ProfileDatabase* db = (ProfileDatabase*)callback_params->db;
    rocprofvis_db_ext_data_t record;
    if (callback_params->future->Interrupted()) return 1;
    record.category = callback_params->subquery;
    for (int i = 0; i < argc; i++)
    {
        record.name = azColName[i];
        record.data = argv[i];
        if (record.data != nullptr) {
            if (db->BindObject()->FuncAddExtDataRecord(callback_params->handle, record) != kRocProfVisDmResultSuccess) return 1;
        }
    }  
    callback_params->future->CountThisRow();
    return 0;
}

rocprofvis_dm_result_t ProfileDatabase::BuildTrackQuery(rocprofvis_dm_index_t index, rocprofvis_dm_string_t & query){
    std::stringstream ss;
    int size = TrackPropertiesAt(index)->query.size();
    ROCPROFVIS_ASSERT_MSG_RETURN(size, "Error! SQL query cannot be empty!", kRocProfVisDmResultUnknownError);
    if (size > 1) ss << "SELECT * FROM (";
    for (int i=0; i < size; i++){
        if (i > 0) ss << " UNION ";
        ss << TrackPropertiesAt(index)->query[i];
    } 
    if (size > 1) ss << ")";
    ss << " where ";
    for (int i = 0; i < NUMBER_OF_TRACK_IDENTIFICATION_PARAMETERS; i++) {
        if (i > 0) ss << " and ";
        ss << TrackPropertiesAt(index)->process_tag[i] << "==" << TrackPropertiesAt(index)->process_id[i];
    }
    query = ss.str();
    return kRocProfVisDmResultSuccess;
}

rocprofvis_dm_result_t ProfileDatabase::BuildSliceQuery(rocprofvis_dm_timestamp_t start, rocprofvis_dm_timestamp_t end, rocprofvis_db_num_of_tracks_t num, rocprofvis_db_track_selection_t tracks, rocprofvis_dm_string_t& query, slice_array_t& slices) {
    slice_query_t slice_query_map;
    for (int i = 0; i < num; i++){
        slices[tracks[i]]=BindObject()->FuncAddSlice(BindObject()->trace_object, tracks[i], start, end);
        rocprofvis_dm_track_params_t* props = TrackPropertiesAt(tracks[i]);
        for (int j = 0; j < props->query.size(); j++) {
            std::string q = props->query[j]; 
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
    query = "SELECT * FROM ( ";
    for (std::map<std::string, std::string>::iterator it_query = slice_query_map.begin(); it_query != slice_query_map.end(); ++it_query) {
        if (it_query!=slice_query_map.begin()) query += " UNION ";
        query += it_query->first;
        query += it_query->second;
        query += ") and start >= ";
        query += std::to_string(start);
        query += " and end < ";
        query += std::to_string(end);
    }
    query += ") ORDER BY start;";
    return kRocProfVisDmResultSuccess;

}

rocprofvis_dm_result_t ProfileDatabase::BuildTableQuery(rocprofvis_dm_timestamp_t start, rocprofvis_dm_timestamp_t end, rocprofvis_db_num_of_tracks_t num, rocprofvis_db_track_selection_t tracks, rocprofvis_dm_charptr_t sort_column, uint64_t max_count, uint64_t offset, bool count_only, rocprofvis_dm_string_t& query) {
    slice_query_t slice_query_map;
    for (int i = 0; i < num; i++){
        rocprofvis_dm_track_params_t* props = TrackPropertiesAt(tracks[i]);
        for (int j = 0; j < props->query.size(); j++) {
            std::string q = props->query[j]; 
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
            std::string subquery;
            if (BuildTrackQuery(tracks[i], subquery) != kRocProfVisDmResultSuccess) break;
            query << subquery
                << " start >= " << start
                << " and end < " << end
                << " ORDER BY start;";
            ShowProgress(step, query.str().c_str(),kRPVDbBusy, future);
            if (kRocProfVisDmResultSuccess != ExecuteSQLQuery(future, query.str().c_str(), slice, TrackPropertiesAt(i)->track_category == kRocProfVisDmPmcTrack ? &CallbackAddPmcRecord : &CallbackAddEventRecord)) break;
        }

        if (i < num) break;
#else
        std::string query;
        slice_array_t slices;
        if (BuildSliceQuery(start, end, num, tracks, query, slices) != kRocProfVisDmResultSuccess) break;
        ShowProgress(100, query.c_str(), kRPVDbBusy, future);
        if (kRocProfVisDmResultSuccess != ExecuteSQLQuery(future, query.c_str(), &slices , &CallbackAddAnyRecord)) break;
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


int ProfileDatabase::CalculateEventLevels(void* data, int argc, char** argv, char** azColName)
{
    ROCPROFVIS_ASSERT_MSG_RETURN(argc == 7, ERROR_DATABASE_QUERY_PARAMETERS_MISMATCH, 1);
    ROCPROFVIS_ASSERT_MSG_RETURN(data, ERROR_SQL_QUERY_PARAMETERS_CANNOT_BE_NULL, 1);
    rocprofvis_db_sqlite_callback_parameters* callback_params =
        (rocprofvis_db_sqlite_callback_parameters*) data;
    ProfileDatabase* db = (ProfileDatabase*) callback_params->db;
    if(callback_params->future->Interrupted())
    {
        return 1;
    }
    uint64_t process_id[NUMBER_OF_TRACK_IDENTIFICATION_PARAMETERS];
    for(int i = 0; i < NUMBER_OF_TRACK_IDENTIFICATION_PARAMETERS;i++)
    {
        process_id[i] = std::stoll(argv[i]);
    }
    rocprofvis_dm_event_id_t id;
    id.bitfield.event_op = std::stol(argv[3]);
    id.bitfield.event_id = std::stoll(argv[4]);
    uint64_t start_time = std::stoll(argv[5]);
    uint64_t end_time = std::stoll(argv[6]);
    uint32_t level=0;

    for(int i = db->m_event_timing_params.size()-1; i >= 0; i--)
    {
        int id_counter = 0;
        for(; id_counter < NUMBER_OF_TRACK_IDENTIFICATION_PARAMETERS; id_counter++)
        {
            if(process_id[id_counter] !=
               db->m_event_timing_params[i].process_id[id_counter])
            {
                break;
            }
        }
        if(id_counter < NUMBER_OF_TRACK_IDENTIFICATION_PARAMETERS)
        {
            continue;
        }
        if(start_time > db->m_event_timing_params[i].end_time)
        {
            db->m_event_timing_params.erase(db->m_event_timing_params.begin() + i);
        } 
        else if(start_time < db->m_event_timing_params[i].end_time)
        {
            level = db->m_event_timing_params[i].level + 1;
            break;
        }
    }
    db->m_event_timing_params.push_back(
        { { process_id[TRACK_ID_NODE], process_id[TRACK_ID_PID_OR_AGENT], process_id[TRACK_ID_TID_OR_QUEUE]}, start_time, end_time, level });
    db->BindObject()->FuncAddEventLevel(db->BindObject()->trace_object, id, level);
    callback_params->future->CountThisRow();
    return 0;
}

}  // namespace DataModel
}  // namespace RocProfVis