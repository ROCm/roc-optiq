// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_db_profile.h"
#include "rocprofvis_db_expression_filter.h"
#include "rocprofvis_shared_types.h"
#include "rocprofvis_c_interface.h"
#include <sstream>
#include <cfloat>
#include <yaml-cpp/yaml.h>
#include "json.h"


namespace RocProfVis
{
namespace DataModel
{

int
ProfileDatabase::CallbackAddStackTrace(void* data, int argc, sqlite3_stmt* stmt,
    char** azColName)
{
    ROCPROFVIS_ASSERT_MSG_RETURN(data, ERROR_SQL_QUERY_PARAMETERS_CANNOT_BE_NULL, 1);
    ROCPROFVIS_ASSERT_MSG_RETURN(argc == 7, ERROR_DATABASE_QUERY_PARAMETERS_MISMATCH, 1);
    void*  func = (void*)&CallbackAddStackTrace;
    rocprofvis_db_sqlite_callback_parameters* callback_params =
        (rocprofvis_db_sqlite_callback_parameters*) data;
    ProfileDatabase*           db = (ProfileDatabase*) callback_params->db;
    rocprofvis_db_stack_data_t record = {"","","",0,0};
    static const char * empty_blob = "{}";
    if(callback_params->future->Interrupted()) return 1;
    enum sqlite_callstack_param_index {
        CALLSTACK_VERSION,
        CALLSTACK_REGION_ID,
        CALLSTACK_P1,
        CALLSTACK_P2,
        CALLSTACK_P3,
        CALLSTACK_SYMBOL,
        CALLSTACK_DEPTH,
    };
    uint32_t version  = db->Sqlite3ColumnInt(func, stmt, azColName, CALLSTACK_VERSION);
    record.id  = db->Sqlite3ColumnInt64(func, stmt, azColName, CALLSTACK_REGION_ID);
    std::string p1 = (char*) db->Sqlite3ColumnText(func, stmt, azColName, CALLSTACK_P1);
    std::string p2 = (char*) db->Sqlite3ColumnText(func, stmt, azColName, CALLSTACK_P2);
    std::string p3 = (char*) db->Sqlite3ColumnText(func, stmt, azColName, CALLSTACK_P3);
    std::string symbol = (char*) db->Sqlite3ColumnText(func, stmt, azColName, CALLSTACK_SYMBOL);
    record.depth = db->Sqlite3ColumnInt(func, stmt, azColName, CALLSTACK_DEPTH);

    jt::Json json_symbol;
    jt::Json json_line;
    std::string symbol_blob;
    std::string line_blob;
    if (version >= 4)
    {
        if (!p1.empty() && sqlite3_column_type(stmt, CALLSTACK_P1) != SQLITE_NULL && p1 != empty_blob)
        {
            json_symbol["name"] = p1;
        }
        else
        {
            json_symbol["name"] = symbol.c_str();
        }
        if (!p2.empty() && sqlite3_column_type(stmt, CALLSTACK_P2) != SQLITE_NULL && p2 != empty_blob)
        {
            json_symbol["file"] = p2.c_str();
        }

        symbol_blob = json_symbol.toString();

        if (!p3.empty() && sqlite3_column_type(stmt, CALLSTACK_P3) != SQLITE_NULL && p3 != empty_blob)
        {
            json_line["line_address"] = p3.c_str();
            line_blob = json_line.toString();
        }
    }
    else
    {
        if (version > 0 && !p1.empty() && sqlite3_column_type(stmt, CALLSTACK_P1) != SQLITE_NULL && p1 != empty_blob)
        {
            symbol_blob = p1;
        }
        else
        {
            if (version == 0 && !p1.empty() && sqlite3_column_type(stmt, CALLSTACK_P1) != SQLITE_NULL) //old schema
            {
                json_symbol["name"] = (symbol + " : " + p1);
            }
            else
            {
                json_symbol["name"] = symbol.c_str();
            }

            symbol_blob = json_symbol.toString();
        }
        if (!p2.empty() && sqlite3_column_type(stmt, CALLSTACK_P2) != SQLITE_NULL && p2 != empty_blob)
        {
            line_blob = p2;
        }
    }
    record.symbol = symbol_blob.c_str();
    record.line = line_blob.c_str();

    if (db->BindObject()->FuncAddStackFrame(callback_params->handle, record) !=
        kRocProfVisDmResultSuccess)
        return 1;
    callback_params->future->CountThisRow();
    return 0;
} 

int ProfileDatabase::CallbackCacheTable(void *data, int argc, sqlite3_stmt* stmt, char **azColName){
    ROCPROFVIS_ASSERT_MSG_RETURN(data, ERROR_SQL_QUERY_PARAMETERS_CANNOT_BE_NULL, 1);
    void* func = (void*)&CallbackCacheTable;
    rocprofvis_db_sqlite_callback_parameters* callback_params = (rocprofvis_db_sqlite_callback_parameters*)data;
    ProfileDatabase* db = (ProfileDatabase*)callback_params->db;
    DatabaseCache * ref_tables = (DatabaseCache *)callback_params->handle;
    std::lock_guard<std::mutex> lock(db->m_lock);
    if (callback_params->future->GetProcessedRowsCount() == 0)
    {
        for (int i = 0; i < argc; i++)
        {
            ref_tables->AddTableColumn(callback_params->query[kRPVCacheTableName].c_str(), azColName[i], (rocprofvis_db_data_type_t)sqlite3_column_type(stmt, i));
        }
    }

    uint64_t id = db->Sqlite3ColumnInt64(func, stmt, azColName, 0);
    ref_tables->AddTableRow(callback_params->query[kRPVCacheTableName].c_str(), id);
    for (int i = 0; i < argc; i++)
    {
        rocprofvis_db_data_type_t col_type = (rocprofvis_db_data_type_t)sqlite3_column_type(stmt, i);
        if (col_type == kRPVDataTypeNull && strcmp(azColName[i],"name") == 0)
        {
            ref_tables->AddTableCell(callback_params->query[kRPVCacheTableName].c_str(), id, i, callback_params->query[kRPVCacheTableName]);
        }
        else
        {
            ref_tables->AddTableCell(callback_params->query[kRPVCacheTableName].c_str(), id, i,
                (char*)db->Sqlite3ColumnText(func, stmt, azColName, i));
        }
    }

    callback_params->future->CountThisRow();
    return 0;
}

int ProfileDatabase::CallBackAddTrack(void *data, int argc, sqlite3_stmt* stmt, char **azColName){
    ROCPROFVIS_ASSERT_MSG_RETURN(argc==TRACK_ID_RECORD_COUNT+1, ERROR_DATABASE_QUERY_PARAMETERS_MISMATCH, 1);
    ROCPROFVIS_ASSERT_MSG_RETURN(data, ERROR_SQL_QUERY_PARAMETERS_CANNOT_BE_NULL, 1);
    void* func = (void*)&CallBackAddTrack;
    rocprofvis_dm_track_params_t track_params = {0};
    rocprofvis_db_sqlite_callback_parameters* callback_params = (rocprofvis_db_sqlite_callback_parameters*)data;
    ROCPROFVIS_ASSERT_MSG_RETURN(callback_params->db_instance != nullptr, ERROR_NODE_KEY_CANNOT_BE_NULL, 1);
    ProfileDatabase* db = (ProfileDatabase*)callback_params->db;
    if(callback_params->future->Interrupted()) return SQLITE_ABORT;
    db->m_add_track_mutex.lock(callback_params->db_instance->GuidIndex());
    track_params.track_indentifiers.db_instance = callback_params->db_instance;
    track_params.load_id.insert(callback_params->track_id);
    track_params.track_indentifiers.track_id = (rocprofvis_dm_track_id_t)db->NumTracks();
    track_params.track_indentifiers.category = (rocprofvis_dm_track_category_t)db->Sqlite3ColumnInt(func, stmt, azColName,TRACK_ID_CATEGORY);
    track_params.op = db->Sqlite3ColumnInt(func, stmt, azColName,TRACK_ID_OPERATION);
    track_params.track_indentifiers.process_id = db->Sqlite3ColumnInt(func, stmt, azColName,TRACK_ID_PROCESS);
    track_params.record_count=db->Sqlite3ColumnInt(func, stmt, azColName,TRACK_ID_RECORD_COUNT);
    if (track_params.op < kRocProfVisDmNumOperation)
    {
        db->TraceProperties()->events_count[track_params.op] += track_params.record_count;
    }
    for (int i = 0; i < NUMBER_OF_TRACK_IDENTIFICATION_PARAMETERS; i++) {
        track_params.track_indentifiers.tag[i] = azColName[i];
        char* arg = (char*) db->Sqlite3ColumnText(func, stmt, azColName, i);
        track_params.track_indentifiers.is_numeric[i] = Database::IsNumber(arg);
        if (track_params.track_indentifiers.is_numeric[i]) {
            track_params.track_indentifiers.id[i] = db->Sqlite3ColumnInt64(func, stmt, azColName, i);
        } else {
            track_params.track_indentifiers.name[i] = arg;
        }        
    }
    track_params.max_ts = 0;
    track_params.min_ts = UINT64_MAX;
    track_params.max_value = 0;
    track_params.min_value = DBL_MAX;

    db->ProcessTrack(track_params, callback_params->query);

    callback_params->future->CountThisRow();
    return 0;
}

int ProfileDatabase::CallBackLoadTrack(void *data, int argc, sqlite3_stmt* stmt, char **azColName){
    ROCPROFVIS_ASSERT_MSG_RETURN(argc==kRpvDbTrackLoadNumItems, ERROR_DATABASE_QUERY_PARAMETERS_MISMATCH, 1);
    ROCPROFVIS_ASSERT_MSG_RETURN(data, ERROR_SQL_QUERY_PARAMETERS_CANNOT_BE_NULL, 1);
    void* func = (void*)&CallBackLoadTrack;
    rocprofvis_dm_track_params_t track_params = {0};
    rocprofvis_db_sqlite_callback_parameters* callback_params = (rocprofvis_db_sqlite_callback_parameters*)data;
    ROCPROFVIS_ASSERT_MSG_RETURN(callback_params->db_instance != nullptr, ERROR_NODE_KEY_CANNOT_BE_NULL, 1);
    uint32_t db_instance = callback_params->db_instance->GuidIndex();
    ProfileDatabase* db = (ProfileDatabase*)callback_params->db;
    if(callback_params->future->Interrupted()) return SQLITE_ABORT;
    //std::string guid = db->Sqlite3ColumnText(func, stmt, azColName,kRpvDbTrackLoadGuid);
    //db->m_add_track_mutex.lock(guid);
    db->m_add_track_mutex.lock(callback_params->db_instance->GuidIndex());
    track_params.track_indentifiers.db_instance = callback_params->db_instance;
    track_params.load_id.insert(callback_params->track_id);
    track_params.track_indentifiers.track_id = (rocprofvis_dm_track_id_t)db->NumTracks();
    track_params.track_indentifiers.category = (rocprofvis_dm_track_category_t)db->Sqlite3ColumnInt(func, stmt, azColName,kRpvDbTrackLoadCategory);
    track_params.op = static_cast<rocprofvis_dm_op_t>(track_params.record_count=db->Sqlite3ColumnInt(func, stmt, azColName,kRpvDbTrackLoadOp));
    track_params.track_indentifiers.process_id = db->Sqlite3ColumnInt(func, stmt, azColName,kRpvDbTrackLoadPID);
    track_params.record_count=db->Sqlite3ColumnInt(func, stmt, azColName,kRpvDbTrackLoadRecordCount);
    if (track_params.op < kRocProfVisDmNumOperation)
    {
        db->TraceProperties()->events_count[track_params.op] += track_params.record_count;
    }
    track_params.min_ts=db->Sqlite3ColumnInt64(func, stmt, azColName,kRpvDbTrackLoadMinTs);
    track_params.max_ts=db->Sqlite3ColumnInt64(func, stmt, azColName,kRpvDbTrackLoadMaxTs);
    track_params.min_value=db->Sqlite3ColumnDouble(func, stmt, azColName,kRpvDbTrackLoadMinValue);
    track_params.max_value=db->Sqlite3ColumnDouble(func, stmt, azColName,kRpvDbTrackLoadMaxValue);

    db->TraceProperties()->db_inst_start_time[db_instance] = std::min(db->TraceProperties()->db_inst_start_time[db_instance], track_params.min_ts);
    db->TraceProperties()->db_inst_end_time[db_instance]  = std::max(db->TraceProperties()->db_inst_end_time[db_instance],track_params.max_ts);

    db->TraceProperties()->trace_duration  = std::max(db->TraceProperties()->trace_duration,db->TraceProperties()->db_inst_end_time[db_instance]-db->TraceProperties()->db_inst_start_time[db_instance]);

    track_params.track_indentifiers.id[TRACK_ID_NODE] = db->Sqlite3ColumnInt64(func, stmt, azColName,kRpvDbTrackLoadNodeId);
    track_params.track_indentifiers.id[TRACK_ID_PID_OR_AGENT] = db->Sqlite3ColumnInt64(func, stmt, azColName,kRpvDbTrackLoadProcessId);
    track_params.track_indentifiers.is_numeric[TRACK_ID_NODE] = true;
    track_params.track_indentifiers.is_numeric[TRACK_ID_PID_OR_AGENT] = true;
    std::string sub_process = db->Sqlite3ColumnText(func, stmt, azColName,kRpvDbTrackLoadSubprocessId);
    track_params.track_indentifiers.is_numeric[TRACK_ID_TID_OR_QUEUE] = Database::IsNumber(sub_process);
    if (track_params.track_indentifiers.is_numeric[TRACK_ID_TID_OR_QUEUE]) {
        track_params.track_indentifiers.id[TRACK_ID_TID_OR_QUEUE] = std::atoll(sub_process.c_str());
    } else {
        track_params.track_indentifiers.name[TRACK_ID_TID_OR_QUEUE] = sub_process;
    }   
    track_params.track_indentifiers.tag[TRACK_ID_NODE] = db->Sqlite3ColumnText(func, stmt, azColName,kRpvDbTrackLoadNodeTag);
    track_params.track_indentifiers.tag[TRACK_ID_PID_OR_AGENT] = db->Sqlite3ColumnText(func, stmt, azColName,kRpvDbTrackLoadProcessTag);
    track_params.track_indentifiers.tag[TRACK_ID_TID_OR_QUEUE] = db->Sqlite3ColumnText(func, stmt, azColName,kRpvDbTrackLoadSubprocessTag);

    db->ProcessTrack(track_params, callback_params->query);

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
    (void) argc;
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
    ROCPROFVIS_ASSERT_MSG_RETURN(callback_params->db_instance != nullptr, ERROR_NODE_KEY_CANNOT_BE_NULL, 1);
    uint32_t db_instance = callback_params->db_instance->GuidIndex();
    ProfileDatabase*            db = (ProfileDatabase*) callback_params->db;
    if(callback_params->future->Interrupted()) return SQLITE_ABORT;
    uint32_t index = db->Sqlite3ColumnInt(func, stmt, azColName, 4);
    db->TrackPropertiesAt(index)->min_ts       = std::min((rocprofvis_dm_timestamp_t)db->Sqlite3ColumnInt64(func, stmt, azColName, 0),db->TrackPropertiesAt(index)->min_ts);
    db->TrackPropertiesAt(index)->max_ts       = std::max((rocprofvis_dm_timestamp_t)db->Sqlite3ColumnInt64(func, stmt, azColName, 1),db->TrackPropertiesAt(index)->max_ts);
    db->TrackPropertiesAt(index)->min_value    = std::min((rocprofvis_dm_value_t)db->Sqlite3ColumnDouble(func, stmt, azColName, 2),db->TrackPropertiesAt(index)->min_value);
    db->TrackPropertiesAt(index)->max_value    = std::max((rocprofvis_dm_value_t)db->Sqlite3ColumnDouble(func, stmt, azColName, 3),db->TrackPropertiesAt(index)->max_value);

    db->TraceProperties()->db_inst_start_time[db_instance] = std::min(db->TraceProperties()->db_inst_start_time[db_instance], db->TrackPropertiesAt(index)->min_ts);
    db->TraceProperties()->db_inst_end_time[db_instance]  = std::max(db->TraceProperties()->db_inst_end_time[db_instance],db->TrackPropertiesAt(index)->max_ts);

    db->TraceProperties()->trace_duration  = std::max(db->TraceProperties()->trace_duration,db->TraceProperties()->db_inst_end_time[db_instance]-db->TraceProperties()->db_inst_start_time[db_instance]);
    callback_params->future->CountThisRow();
    return 0;
}


int ProfileDatabase::CallbackAddAnyRecord(void* data, int argc, sqlite3_stmt* stmt, char** azColName) {
    ROCPROFVIS_ASSERT_MSG_RETURN(argc == rocprofvis_db_sqlite_slice_query_format::NUM_PARAMS+1,
        ERROR_DATABASE_QUERY_PARAMETERS_MISMATCH, 1);
    ROCPROFVIS_ASSERT_MSG_RETURN(data, ERROR_SQL_QUERY_PARAMETERS_CANNOT_BE_NULL, 1);
    void *func = (void*)&CallbackAddAnyRecord;
    rocprofvis_db_sqlite_callback_parameters* callback_params = (rocprofvis_db_sqlite_callback_parameters*)data;
    ROCPROFVIS_ASSERT_MSG_RETURN(callback_params->db_instance != nullptr, ERROR_NODE_KEY_CANNOT_BE_NULL, 1);
    uint32_t db_instance = callback_params->db_instance->GuidIndex();
    ProfileDatabase* db = (ProfileDatabase*)callback_params->db;
    if(callback_params->future->Interrupted()) return SQLITE_ABORT;
    uint32_t track = db->Sqlite3ColumnInt(func, stmt, azColName, argc-1);
    rocprofvis_db_record_data_t record;
    record.event.id.bitfield.event_op = db->Sqlite3ColumnInt(func, stmt, azColName, 0);
    record.event.id.bitfield.event_node = callback_params->db_instance->GuidIndex();

    if (record.event.id.bitfield.event_op > 0) {       
        record.event.id.bitfield.event_id = db->Sqlite3ColumnInt64(func, stmt, azColName, 5);
        record.event.timestamp = db->Sqlite3ColumnInt64(func, stmt, azColName, 1);
        record.event.duration = db->Sqlite3ColumnInt64(func, stmt, azColName, 2) - record.event.timestamp;
        record.event.timestamp-=db->TraceProperties()->db_inst_start_time[db_instance];
        record.event.category = db->Sqlite3ColumnInt64(func, stmt, azColName, 3);
        record.event.symbol = db->Sqlite3ColumnInt64(func, stmt, azColName, 4);
        record.event.level   = static_cast<rocprofvis_dm_event_level_t>(db->Sqlite3ColumnInt64(func, stmt, azColName, 9));
        if (kRocProfVisDmResultSuccess != db->RemapStringIds(record)) return 0;
    }
    else {
        record.pmc.timestamp = db->Sqlite3ColumnInt64(func, stmt, azColName, 1);
        record.pmc.timestamp-=db->TraceProperties()->db_inst_start_time[db_instance];
        record.pmc.value = db->Sqlite3ColumnDouble(func, stmt, azColName,2);
        callback_params->future->SetRuntimeStorageValue(kRPVFutureStorageSampleValue, record.pmc.value);
    }
    if(db->BindObject()->FuncAddRecord(
        (*(slice_array_t*) callback_params->handle)[track],
        record) != kRocProfVisDmResultSuccess)
        return 1;
    callback_params->future->CountThisRow();
    return 0;

}

int ProfileDatabase::CallbackAddFlowTrace(void *data, int argc, sqlite3_stmt* stmt, char **azColName){
    ROCPROFVIS_ASSERT_MSG_RETURN(data, ERROR_SQL_QUERY_PARAMETERS_CANNOT_BE_NULL, 1);
    ROCPROFVIS_ASSERT_MSG_RETURN(argc == rocprofvis_db_sqlite_dataflow_query_format::NUM_PARAMS,
        ERROR_DATABASE_QUERY_PARAMETERS_MISMATCH, 1);
    void*  func = (void*)&CallbackAddFlowTrace;
    rocprofvis_db_sqlite_callback_parameters* callback_params = (rocprofvis_db_sqlite_callback_parameters*)data;
    ROCPROFVIS_ASSERT_MSG_RETURN(callback_params->db_instance != nullptr, ERROR_NODE_KEY_CANNOT_BE_NULL, 1);
    uint32_t db_instance = callback_params->db_instance->GuidIndex();
    ProfileDatabase* db = (ProfileDatabase*)callback_params->db;
    if(callback_params->future->Interrupted()) return SQLITE_ABORT;
    rocprofvis_db_flow_data_t record;

    record.id.bitfield.event_op = db->Sqlite3ColumnInt(func, stmt, azColName,0 );
    record.id.bitfield.event_node = callback_params->db_instance->GuidIndex();
    if (db->TrackTracker()->FindTrack(
        db->TrackTracker()->SearchCategoryMaskLookup((rocprofvis_dm_event_operation_t)record.id.bitfield.event_op),
        db->Sqlite3ColumnInt(func, stmt, azColName,4), 
        db->Sqlite3ColumnInt(func, stmt, azColName,5), 
        callback_params->db_instance->GuidIndex(), record.track_id))
    {
        record.id.bitfield.event_id = db->Sqlite3ColumnInt64(func, stmt, azColName, 2 );
        record.time = db->Sqlite3ColumnInt64(func, stmt, azColName, 6 );
        record.time-=db->TraceProperties()->db_inst_start_time[db_instance];
        record.category_id = db->Sqlite3ColumnInt64(func, stmt, azColName, 7);
        record.symbol_id = db->Sqlite3ColumnInt64(func, stmt, azColName, 8);
        record.level = static_cast<rocprofvis_dm_event_level_t>(db->Sqlite3ColumnInt64(func, stmt, azColName, 9));
        record.end_time = db->Sqlite3ColumnInt64(func, stmt, azColName, 10);  
        record.end_time-=db->TraceProperties()->db_inst_start_time[db_instance];
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
    ROCPROFVIS_ASSERT_MSG_RETURN(callback_params->db_instance != nullptr, ERROR_NODE_KEY_CANNOT_BE_NULL, 1);
    ProfileDatabase* db = (ProfileDatabase*)callback_params->db;
    rocprofvis_db_ext_data_t record;
    if (callback_params->future->Interrupted()) return SQLITE_ABORT;
    record.category = callback_params->query[kRPVCacheTableName].c_str();

    for (int i = 0; i < argc; i++)
    {
        record.name = azColName[i];
        std::string aux_str = record.name;
        if (aux_str == Builder::START_PUBLIC_NAME|| aux_str == Builder::END_PUBLIC_NAME)
        {
            uint64_t timestamp = db->Sqlite3ColumnInt64(func, stmt, azColName, i);
            timestamp -= db->TraceProperties()->db_inst_start_time[callback_params->db_instance->GuidIndex()];
            aux_str = std::to_string(timestamp);
            record.data = aux_str.c_str();
            record.type = kRPVDataTypeInt;
        }
        else
        {
            record.type = (rocprofvis_db_data_type_t)sqlite3_column_type(stmt, i);
            record.data = (char*)db->Sqlite3ColumnText(func, stmt, azColName, i);
        }
        record.category_enum = GetColumnDataCategory(*db->GetCategoryEnumMap(), callback_params->operation, record.name);
        record.db_instance = callback_params->db_instance->GuidIndex();
        if (record.data != nullptr) {
            if (db->BindObject()->FuncAddExtDataRecord(callback_params->handle, record) != kRocProfVisDmResultSuccess) return 1;
        }
    }  
    callback_params->future->CountThisRow();
    return 0;
}

int ProfileDatabase::CallbackAddArgumentsInfo(void* data, int argc, sqlite3_stmt* stmt, char** azColName) {
    (void) argc;
    ROCPROFVIS_ASSERT_MSG_RETURN(data, ERROR_SQL_QUERY_PARAMETERS_CANNOT_BE_NULL, 1);
    void*  func = (void*)&CallbackAddArgumentsInfo;
    rocprofvis_db_sqlite_callback_parameters* callback_params = (rocprofvis_db_sqlite_callback_parameters*)data;
    ROCPROFVIS_ASSERT_MSG_RETURN(callback_params->db_instance != nullptr, ERROR_NODE_KEY_CANNOT_BE_NULL, 1);
    ProfileDatabase* db = (ProfileDatabase*)callback_params->db;
    rocprofvis_db_argument_data_t record;
    if (callback_params->future->Interrupted()) return SQLITE_ABORT;
    record.position = db->Sqlite3ColumnInt(func, stmt, azColName,1);
    record.type = (char*)db->Sqlite3ColumnText(func, stmt, azColName,2);
    record.name = (char*)db->Sqlite3ColumnText(func, stmt, azColName,3);
    record.value = (char*)db->Sqlite3ColumnText(func, stmt, azColName,4);
    if (db->BindObject()->FuncAddArgDataRecord(callback_params->handle, record) != kRocProfVisDmResultSuccess) return 1;

    callback_params->future->CountThisRow();
    return 0;
}

int ProfileDatabase::CallbackAddEssentialInfo(void* data, int argc, sqlite3_stmt* stmt, char** azColName) {
    ROCPROFVIS_ASSERT_MSG_RETURN(data, ERROR_SQL_QUERY_PARAMETERS_CANNOT_BE_NULL, 1);
    ROCPROFVIS_ASSERT_MSG_RETURN(argc == rocprofvis_db_sqlite_essential_data_query_format::NUM_PARAMS,
        ERROR_DATABASE_QUERY_PARAMETERS_MISMATCH, 1);
    void*  func = (void*)&CallbackAddEssentialInfo;
    rocprofvis_db_sqlite_callback_parameters* callback_params = (rocprofvis_db_sqlite_callback_parameters*)data;
    ROCPROFVIS_ASSERT_MSG_RETURN(callback_params->db_instance != nullptr, ERROR_NODE_KEY_CANNOT_BE_NULL, 1);
    ProfileDatabase* db = (ProfileDatabase*)callback_params->db;
    rocprofvis_db_ext_data_t record;
    if (callback_params->future->Interrupted()) return SQLITE_ABORT;
    rocprofvis_db_sqlite_track_service_data_t service_data{};
    uint64_t event_id = callback_params->future->GetRuntimeStorageValue(kRPVFutureStorageEventId, (uint64_t)0);

    for(int i = 0; i < argc-2; i++)
    {
        std::string column = azColName[i];
        CollectTrackServiceData(db, stmt, i, azColName, service_data);
    }

    std::string column_data;

    uint32_t track_id;

    if (!db->TrackTracker()->FindTrack(
        db->TrackTracker()->SearchCategoryMaskLookup((rocprofvis_dm_event_operation_t)((rocprofvis_dm_event_id_t*)&event_id)->bitfield.event_op),
        service_data.process_id,
        service_data.sub_process_id,
        callback_params->db_instance->GuidIndex(),
        track_id))
    {
        track_id = INVALID_INDEX;
    }

    if(track_id != -1)
    {

        record.category = "Track";
        record.name = "trackId";
        record.type = kRPVDataTypeInt;
        column_data = std::to_string(track_id).c_str();
        record.data = column_data.c_str();
        record.category_enum = kRocProfVisEventEssentialDataTrack;
        record.db_instance = callback_params->db_instance->GuidIndex();
        if (db->BindObject()->FuncAddExtDataRecord(callback_params->handle, record) !=
            kRocProfVisDmResultSuccess)
            return 1;
        record.category = "Track";
        record.name = "levelForTrack";
        record.type = kRPVDataTypeInt;
        column_data = std::to_string(db->Sqlite3ColumnInt64(func, stmt, azColName, argc - 2));
        record.data = column_data.c_str();
        record.category_enum = kRocProfVisEventEssentialDataLevel;
        record.db_instance = callback_params->db_instance->GuidIndex();
        if (db->BindObject()->FuncAddExtDataRecord(callback_params->handle, record) !=
            kRocProfVisDmResultSuccess)
            return 1;
    }

    if (service_data.op == kRocProfVisDmOperationLaunch || 
        service_data.op == kRocProfVisDmOperationLaunchSample || 
        !db->TrackTracker()->FindTrack(kRocProfVisDmStreamTrack, 
            service_data.pid,
            service_data.stream_id,
            callback_params->db_instance->GuidIndex(),
            track_id))
    {
        track_id = INVALID_INDEX;
    }

    if (track_id != -1)
    {
        record.category = "Track";
        record.name = "streamTrackId";
        record.type = kRPVDataTypeInt;
        column_data = std::to_string(track_id).c_str();
        record.data = column_data.c_str();
        record.category_enum = kRocProfVisEventEssentialDataStreamTrack;
        record.db_instance = callback_params->db_instance->GuidIndex();
        if (db->BindObject()->FuncAddExtDataRecord(callback_params->handle, record) !=
            kRocProfVisDmResultSuccess)
            return 1;
        record.category = "Track";
        record.name = "levelForStreamTrack";
        record.type = kRPVDataTypeInt;
        column_data = std::to_string(db->Sqlite3ColumnInt64(func, stmt, azColName, argc - 1));
        record.data = column_data.c_str();
        record.category_enum = kRocProfVisEventEssentialDataStreamLevel;
        record.db_instance = callback_params->db_instance->GuidIndex();
        if (db->BindObject()->FuncAddExtDataRecord(callback_params->handle, record) !=
            kRocProfVisDmResultSuccess)
            return 1;
    }

    callback_params->future->CountThisRow();
    return 0;
}


bool ProfileDatabase::FindTrack(rocprofvis_dm_track_category_t category, uint64_t id_process, uint64_t id_subprocess, uint32_t db_instance, uint32_t& out_track)
{
    return TrackTracker()->FindTrack(category, id_process, id_subprocess, db_instance, out_track);
}


void
ProfileDatabase::GetTrackIdentifierIndices(
    int column_index, char** azColName,
    rocprofvis_db_sqlite_track_identifier_index_t& track_ids_indices)
{
    std::string column_name = azColName[column_index];

    if (column_name == Builder::NODE_ID_SERVICE_NAME)
    {
        track_ids_indices.nid_index = column_index;
    }
    else if (column_name == Builder::AGENT_ID_SERVICE_NAME)
    {
        track_ids_indices.process_index = column_index;
    }
    else if (column_name == Builder::QUEUE_ID_SERVICE_NAME)
    {
        track_ids_indices.sub_process_index = column_index;
    }
    else if (column_name == Builder::STREAM_ID_SERVICE_NAME)
    {
        track_ids_indices.stream_index = column_index;
    }
    else if (column_name == Builder::PROCESS_ID_SERVICE_NAME)
    {
        track_ids_indices.process_index = column_index;
    }
    else if (column_name == Builder::THREAD_ID_SERVICE_NAME)
    {
        track_ids_indices.sub_process_index = column_index;
    }
    else if (column_name == Builder::COUNTER_ID_SERVICE_NAME)
    {
        track_ids_indices.is_pmc_identifier = true;
        track_ids_indices.sub_process_index = column_index;
    }
    else if (column_name == Builder::COUNTER_NAME_SERVICE_NAME)
    {
        track_ids_indices.is_pmc_identifier = true;
        track_ids_indices.is_rocpd_pmc = true;
        track_ids_indices.sub_process_index = column_index;
    }
    else if (column_name == Builder::PROCESS_ID_PUBLIC_NAME)
    {
        track_ids_indices.pid_index = column_index;
    }
}

void
ProfileDatabase::CollectTrackServiceData(
    ProfileDatabase* db,
    sqlite3_stmt* stmt, int column_index, char** azColName,
    rocprofvis_db_sqlite_track_service_data_t& service_data)
{
    void* func = (void*)&CollectTrackServiceData;
    std::string column_name = azColName[column_index];
    if(column_name == Builder::OPERATION_SERVICE_NAME)
    {

        service_data.op = (rocprofvis_dm_event_operation_t)db->Sqlite3ColumnInt(func, stmt, azColName, column_index);
    }
    else if(column_name == Builder::NODE_ID_SERVICE_NAME)
    {
        service_data.nid = db->Sqlite3ColumnInt64(func, stmt, azColName, column_index);
    }
    else if(column_name == Builder::AGENT_ID_SERVICE_NAME)
    {
        service_data.process_id = db->Sqlite3ColumnInt(func, stmt, azColName, column_index);
    }
    else if(column_name == Builder::QUEUE_ID_SERVICE_NAME)
    {
        service_data.sub_process_id = db->Sqlite3ColumnInt(func, stmt, azColName, column_index);
    }
    else if(column_name == Builder::STREAM_ID_SERVICE_NAME)
    {
        service_data.stream_id = db->Sqlite3ColumnInt(func, stmt, azColName, column_index);
    }
    else if(column_name == Builder::PROCESS_ID_SERVICE_NAME)
    {
        service_data.process_id = db->Sqlite3ColumnInt(func, stmt, azColName, column_index);
    }
    else if(column_name == Builder::THREAD_ID_SERVICE_NAME)
    {
        service_data.sub_process_id = db->Sqlite3ColumnInt(func, stmt, azColName, column_index);
    }
    else if(column_name == Builder::PID_SERVICE_NAME)
    {
        service_data.pid = db->Sqlite3ColumnInt(func, stmt, azColName, column_index);
    }
}

rocprofvis_dm_result_t
    ProfileDatabase::BuildTrackQuery(rocprofvis_dm_index_t index,
        rocprofvis_dm_index_t type,
        rocprofvis_dm_string_t& query,
        uint32_t split_count,
        uint32_t split_index)
{
    std::stringstream ss;
    DbInstance* db_instance = (DbInstance*)TrackPropertiesAt(index)->track_indentifiers.db_instance;
    int               size = static_cast<int>(TrackPropertiesAt(index)->query[type].size());
    ROCPROFVIS_ASSERT_MSG_RETURN(size, "Error! SQL query cannot be empty!", kRocProfVisDmResultUnknownError);
    ss << query << " FROM (";
    for(int i = 0; i < size; i++)
    {
        if(i > 0) ss << " UNION ALL ";
        ss << TrackPropertiesAt(index)->query[type][i];

        ss << " where ";
        if(TrackPropertiesAt(index)->track_indentifiers.category == kRocProfVisDmRegionMainTrack)
        {
            ss << "SAMPLE.id IS NULL and ";
        }
        int count = 0;
        for(int param_index = 0; param_index < NUMBER_OF_TRACK_IDENTIFICATION_PARAMETERS; param_index++)
        {
            if(TrackPropertiesAt(index)->track_indentifiers.tag[param_index] == "const")
            {
                continue;
            }
            if(count > 0)
            {
                ss << " and ";
            }
            ss << TrackPropertiesAt(index)->track_indentifiers.tag[param_index] << "==";
            if(TrackPropertiesAt(index)->track_indentifiers.is_numeric[param_index])
            {
                ss << TrackPropertiesAt(index)->track_indentifiers.id[param_index];
            }
            else
            {
                ss << "'" << TrackPropertiesAt(index)->track_indentifiers.name[param_index] << "'";
            }
            count++;
        }
        if (split_count > 1)
        {
            uint64_t trace_time = TraceProperties()->db_inst_end_time[db_instance->GuidIndex()] - TraceProperties()->db_inst_start_time[db_instance->GuidIndex()];
            if (trace_time > 0)
            {
                uint64_t time_bucket_size = trace_time / split_count;
                ss << " and " << Builder::START_SERVICE_NAME << " BETWEEN " << TraceProperties()->db_inst_start_time[db_instance->GuidIndex()] + (time_bucket_size * split_index);
                ss << " and " << TraceProperties()->db_inst_start_time[db_instance->GuidIndex()] + (time_bucket_size * (split_index+1));
            }
        }
    }
    ss << ") ";
    query = ss.str();
    return kRocProfVisDmResultSuccess;
}

void ProfileDatabase::BuildSliceQueryMap(
    slice_query_map_t& slice_query_map, 
    rocprofvis_dm_track_params_t* props,
    rocprofvis_db_query_type_t query_type)
{
    for (int j = 0; j < props->query[query_type].size(); j++) {
        std::string q = props->query[query_type][j]; 

        std::string tuple = "(";
        for (int k = 0; k < NUMBER_OF_TRACK_IDENTIFICATION_PARAMETERS; k++) {
            if (props->track_indentifiers.tag[k] != "const") {
                if (tuple.length() > 1) tuple += ",";
                tuple += props->track_indentifiers.tag[k];
            }
        }
        tuple += ")";
        q += " where ";
        if (props->track_indentifiers.category == kRocProfVisDmRegionMainTrack)
        {
            q += "SAMPLE.id IS NULL and ";
        }
        q += tuple;
        q += " IN (";
        tuple = "(";
        for (int k = 0; k < NUMBER_OF_TRACK_IDENTIFICATION_PARAMETERS; k++) {
            if (props->track_indentifiers.tag[k] != "const") {
                if (tuple.length() > 1) tuple += ",";
                std::string id = props->track_indentifiers.is_numeric[k] ? std::to_string(props->track_indentifiers.id[k]) : std::string("'") + props->track_indentifiers.name[k] + "'";
                tuple += id;

            }
        }
        tuple += ")";
        DbInstance* instance = (DbInstance*)props->track_indentifiers.db_instance;
        if (slice_query_map[q][instance->GuidIndex()].length() > 0) slice_query_map[q][instance->GuidIndex()] += ", ";
        slice_query_map[q][instance->GuidIndex()] += tuple;
    }
}


rocprofvis_dm_result_t ProfileDatabase::DetectMultiNode(rocprofvis_db_filename_t filename, std::vector<std::string> & files)
{
    rocprofvis_dm_result_t result = kRocProfVisDmResultNotLoaded;
    std::string filepath = filename;
    if (filepath.find(".yaml", filepath.size() - 5) != std::string::npos)
    {
        std::filesystem::path safe_path;
        if (!SanitizeFilePath(filepath, safe_path)) {
            return result;
        }
        YAML::Node config = YAML::LoadFile(safe_path.string());
        auto db_files = config["rocprofiler-sdk"]["rocpd"]["files"];
        size_t pos = filepath.find_last_of("/\\");
        if (pos != std::string::npos)
        {
            filepath = filepath.substr(0, pos);
        }
        else
        {
            filepath = "";
        }
        for (auto file : db_files)
        {
            sqlite3 *db;
            std::string dbfile = filepath+"/"+file.as<std::string>();
            if (sqlite3_open(dbfile.c_str(), &db) != SQLITE_OK)
            {
                result = kRocProfVisDmResultDbAccessFailed;
                break;
            }
            if (DetectTable(db, "rocpd_event") == SQLITE_OK)
            {
                files.push_back(dbfile);
                result = kRocProfVisDmResultSuccess;
                sqlite3_close(db);              
            }
            else
            {
                result = kRocProfVisDmResultInvalidParameter;
                sqlite3_close(db); 
                break;
            }

        }
    }
    return result;
}


rocprofvis_db_type_t ProfileDatabase::Detect(rocprofvis_db_filename_t filename, std::vector<std::string> & multinode_files){
    sqlite3 *db;
    if (DetectMultiNode(filename, multinode_files) == kRocProfVisDmResultSuccess)
    {
        return rocprofvis_db_type_t::kRocprofMultinodeSqlite;
    }
    // Avoid SQLITE_OPEN_CREATE so detecting a missing file doesn't create one.
    if (sqlite3_open_v2(filename, &db, SQLITE_OPEN_READWRITE, nullptr) != SQLITE_OK)
    {
        sqlite3_close(db);
        return rocprofvis_db_type_t::kAutodetect;
    }

    if (DetectTable(db, "rocpd_event") == SQLITE_OK) {
        sqlite3_close(db);
        return rocprofvis_db_type_t::kRocprofSqlite;
    }

    if (DetectTable(db, "api") == SQLITE_OK) {
        sqlite3_close(db);
        return rocprofvis_db_type_t::kRocpdSqlite;
    }

    if (DetectTable(db, "compute_metadata", false) == SQLITE_OK) {
        sqlite3_close(db);
        return rocprofvis_db_type_t::kComputeSqlite;
    }

    sqlite3_close(db);
    return rocprofvis_db_type_t::kAutodetect;
}


int ProfileDatabase::CalculateEventLevels(void* data, int argc, sqlite3_stmt* stmt, char** azColName)
{
    ROCPROFVIS_ASSERT_MSG_RETURN(argc == rocprofvis_db_sqlite_level_query_format::NUM_PARAMS+1 , ERROR_DATABASE_QUERY_PARAMETERS_MISMATCH, 1);
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
    uint32_t track = db->Sqlite3ColumnInt(func, stmt, azColName, rocprofvis_db_sqlite_level_query_format::NUM_PARAMS);
    uint8_t level=0;
    rocprofvis_dm_track_params_t* params     = db->TrackPropertiesAt(track);
    ROCPROFVIS_ASSERT_MSG_RETURN(params!=0, ERROR_TRACE_PROPERTIES_CANNOT_BE_NULL, 1);
    DbInstance* db_instance = (DbInstance*)params->track_indentifiers.db_instance;
    ROCPROFVIS_ASSERT_MSG_RETURN(db_instance != nullptr, ERROR_NODE_KEY_CANNOT_BE_NULL, 1);
    uint64_t parent_id = 0;
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

            level = static_cast<uint8_t>(it->level + 1);
            parent_id = ((rocprofvis_dm_event_id_t*)&it->id)->bitfield.event_id;
        }
        it = next_it;
    }
    rocprofvis_dm_event_id_t id_value;
    id_value.bitfield.event_id = id;
    id_value.bitfield.event_node = db_instance->GuidIndex();
    id_value.bitfield.event_op = op;
    params->m_active_events.push_back({id_value.value, start_time, end_time, level});
    params->m_active_events.sort([](const rocprofvis_event_timing_params_t& a, const rocprofvis_event_timing_params_t& b) { return a.level < b.level;});
    callback_params->future->CountThisRow();
    {
        std::lock_guard<std::mutex> lock(db->m_level_lock);
        auto                        level_it = db->m_event_levels_id_to_index[op][db_instance->GuidIndex()].find(id);
        int                         index = 0;
        if(level_it == db->m_event_levels_id_to_index[op][db_instance->GuidIndex()].end())
        {
            index = static_cast<int>(db->m_event_levels[op][db_instance->GuidIndex()].size());
            db->m_event_levels_id_to_index[op][db_instance->GuidIndex()][id] = index;
            db->m_event_levels[op][db_instance->GuidIndex()].push_back({id, parent_id,0,0});
        }
        else
        {
            index = static_cast<int>(level_it->second);
        }
        if(params->track_indentifiers.category == kRocProfVisDmStreamTrack)
        {
            db->m_event_levels[op][db_instance->GuidIndex()][index].level_for_stream = level;
        }
        else
        {
            db->m_event_levels[op][db_instance->GuidIndex()][index].level_for_queue = level;
        }

    }
    return 0;
}


// Persist per-track aggregate properties and metadata into the database.
// This function walks the collected profiling data, derives summary
// statistics for each track (timestamps, values, process/node identifiers),
// and uses the provided Future/database context to insert one row per track
// into the track_properties table defined below.
rocprofvis_dm_result_t ProfileDatabase::SaveTrackProperties(Future* future) {
    (void) future;
    // Define the schema for the track_properties table that will receive the
    // per-track summary rows. The SQLInsertParams describes the column names
    // and their SQLite types; the order here must match the order used when
    // binding values later in this function.
    SQLInsertParams params = { 
        { "id", "INTEGER PRIMARY KEY" },
        { "load_id", "INTEGER" },
        { "track_id", "INTEGER" },
        { "category", "INTEGER" },
        { "operation", "INTEGER" },
        { "record_count", "INTEGER" },
        { "min_timestamp", "INTEGER" },
        { "max_timestamp", "INTEGER" },
        { "min_value", "REAL" },
        { "max_value", "REAL" },
        { "node_id", "INTEGER" },
        { "process_id", "INTEGER" },
        { "sub_process_id", "TEXT" },
        { "node_tag", "TEXT" },
        { "process_tag", "TEXT" },
        { "sub_process_tag", "TEXT" },
        { "guid", "TEXT" },
        { "pid", "INTEGER" }
    };

    // Temporary container holding the derived properties for a single track
    // before it is written to the database. Each field corresponds to one
    // column in the params schema above:
    //  - id/load_id: identifiers for this record and the profiler load.
    //  - track_id/category/op: logical track and event categorization.
    //  - record_count/min_ts/max_ts/min_val/max_val: basic statistics over
    //    the events associated with the track.
    //  - node_id/process_id/subproc_*: topology and process information
    //    extracted from the profiling context.
    //  - node_tag/process_tag/subproc_tag/guid/pid: human-readable labels
    //    and identifiers that assist with post-processing and visualization.
    typedef struct store_params {
        uint32_t id;
        uint32_t load_id;
        uint32_t track_id;
        uint32_t category;
        uint32_t op;
        uint64_t record_count;
        uint64_t min_ts;
        uint64_t max_ts;
        double min_val;
        double max_val;
        uint64_t node_id;
        uint64_t process_id;
        bool subproc_numeric;
        uint64_t subproc_id;
        std::string subproc_name;
        std::string node_tag;
        std::string process_tag;
        std::string subproc_tag;
        std::string guid;
        uint64_t pid;
    } store_params;

    std::map<uint32_t, std::vector<store_params>> v;
    uint32_t counter=0;
    rocprofvis_dm_result_t result = kRocProfVisDmResultSuccess;
    std::string table_name = GetMetadataVersionControl()->GetTrackInfoTableName();
    for (int i = 0; i < NumTracks(); i++)
    {
        // Write a merged track's total on one load_id row only; the loader
        // re-sums rows, so repeating it per row would multiply it on reload.
        bool first_load_id = true;
        for (auto load_id : TrackPropertiesAt(i)->load_id)
        {
            DbInstance* db_instance = (DbInstance*)TrackPropertiesAt(i)->track_indentifiers.db_instance;
            if (db_instance == nullptr) continue;
            if (!ShouldProcessInstance(db_instance->FileIndex())) continue;
            store_params p;
            p.id = counter++;
            p.load_id = load_id;
            p.track_id = TrackPropertiesAt(i)->track_indentifiers.track_id;
            p.category = TrackPropertiesAt(i)->track_indentifiers.category;
            p.op = TrackPropertiesAt(i)->op;
            p.record_count = first_load_id ? TrackPropertiesAt(i)->record_count : 0;
            first_load_id = false;
            p.min_ts = TrackPropertiesAt(i)->min_ts;
            p.max_ts = TrackPropertiesAt(i)->max_ts;
            p.min_val = TrackPropertiesAt(i)->min_value;
            p.max_val = TrackPropertiesAt(i)->max_value;
            p.node_id = TrackPropertiesAt(i)->track_indentifiers.id[TRACK_ID_NODE];
            p.process_id = TrackPropertiesAt(i)->track_indentifiers.id[TRACK_ID_PID_OR_AGENT];
            p.subproc_numeric = TrackPropertiesAt(i)->track_indentifiers.is_numeric[TRACK_ID_TID_OR_QUEUE];
            p.subproc_id = TrackPropertiesAt(i)->track_indentifiers.id[TRACK_ID_TID_OR_QUEUE];
            p.subproc_name = TrackPropertiesAt(i)->track_indentifiers.name[TRACK_ID_TID_OR_QUEUE];
            p.node_tag = TrackPropertiesAt(i)->track_indentifiers.tag[TRACK_ID_NODE];
            p.process_tag = TrackPropertiesAt(i)->track_indentifiers.tag[TRACK_ID_PID_OR_AGENT];
            p.subproc_tag = TrackPropertiesAt(i)->track_indentifiers.tag[TRACK_ID_TID_OR_QUEUE];
            p.guid = GuidAt(db_instance->GuidIndex());
            p.pid = TrackPropertiesAt(i)->track_indentifiers.process_id;
            v[db_instance->FileIndex()].push_back(p);
        }
    }
    for (auto it = v.begin(); it != v.end(); ++it)
    {
        if (false == GetMetadataVersionControl()->MustRebuildTrackInfo(it->first)) continue;

        result = CreateSQLTable(
            table_name.c_str(), 
            params, 
            v[it->first].size(),
            [&](sqlite3_stmt* stmt, int index) {
                store_params& p = v[it->first][index];
                sqlite3_bind_int(stmt, 1, p.id);
                sqlite3_bind_int(stmt, kRpvDbTrackLoadId + 1, p.load_id);
                sqlite3_bind_int(stmt, kRpvDbTrackLoadTrackId + 1, p.track_id);
                sqlite3_bind_int(stmt, kRpvDbTrackLoadCategory + 1, p.category);
                sqlite3_bind_int(stmt, kRpvDbTrackLoadOp + 1, p.op);
                sqlite3_bind_int(stmt, kRpvDbTrackLoadRecordCount + 1, static_cast<int>(p.record_count));
                sqlite3_bind_int64(stmt, kRpvDbTrackLoadMinTs + 1, p.min_ts);
                sqlite3_bind_int64(stmt, kRpvDbTrackLoadMaxTs + 1, p.max_ts);
                sqlite3_bind_double(stmt, kRpvDbTrackLoadMinValue + 1, p.min_val);
                sqlite3_bind_double(stmt, kRpvDbTrackLoadMaxValue + 1, p.max_val);
                sqlite3_bind_int64(stmt, kRpvDbTrackLoadNodeId + 1, p.node_id);
                sqlite3_bind_int64(stmt, kRpvDbTrackLoadProcessId + 1, p.process_id);
                if (p.subproc_numeric)
                {

                    sqlite3_bind_int64(stmt, kRpvDbTrackLoadSubprocessId + 1, p.subproc_id);
                }
                else
                {
                    sqlite3_bind_text(stmt, kRpvDbTrackLoadSubprocessId + 1, p.subproc_name.c_str(), -1, SQLITE_STATIC);
                }
                sqlite3_bind_text(stmt, kRpvDbTrackLoadNodeTag + 1, p.node_tag.c_str(), -1, SQLITE_STATIC);
                sqlite3_bind_text(stmt, kRpvDbTrackLoadProcessTag + 1, p.process_tag.c_str(), -1, SQLITE_STATIC);
                sqlite3_bind_text(stmt, kRpvDbTrackLoadSubprocessTag + 1, p.subproc_tag.c_str(), -1, SQLITE_STATIC);
                sqlite3_bind_text(stmt, kRpvDbTrackLoadGuid + 1, p.guid.c_str(), -1, SQLITE_STATIC);
                sqlite3_bind_int64(stmt, kRpvDbTrackLoadPID + 1, p.pid);
            }, it->first);
    }
    return result;
}


int ProfileDatabase::CallBackLoadHistogram(void* data, int argc, sqlite3_stmt* stmt, char** azColName) {
    ROCPROFVIS_ASSERT_MSG_RETURN(argc == 5, ERROR_DATABASE_QUERY_PARAMETERS_MISMATCH, 1);
    ROCPROFVIS_ASSERT_MSG_RETURN(data, ERROR_SQL_QUERY_PARAMETERS_CANNOT_BE_NULL, 1);
    void* func = (void*)&CallBackLoadHistogram;
    rocprofvis_db_sqlite_callback_parameters* callback_params = (rocprofvis_db_sqlite_callback_parameters*)data;
    ProfileDatabase* db = (ProfileDatabase*)callback_params->db;
    if (callback_params->future->Interrupted()) return SQLITE_ABORT;
    uint32_t track_id = db->Sqlite3ColumnInt(func, stmt, azColName, 1);
    uint32_t bucket_num = db->Sqlite3ColumnInt(func, stmt, azColName, 2);
    uint32_t events_count = db->Sqlite3ColumnInt(func, stmt, azColName, 3);
    double bucket_value = db->Sqlite3ColumnDouble(func, stmt, azColName, 4);
    db->TrackPropertiesAt(track_id)->histogram[bucket_num] = std::make_pair(events_count, bucket_value);
    callback_params->future->CountThisRow();
    return 0;

}



uint64_t ProfileDatabase::GetHistogramQueryAndSchemaHash() {
    std::string hash_str = GetHistogramQueryPrefix(0) + GetHistogramQuerySuffix();
    for (auto param : s_histogram_schema_params)
    {
        hash_str += param.column;
        hash_str += param.type;
    // Choose a bucket size that approximately yields the requested
    // number of bins over the full trace duration; store it so other
    // components can interpret the resulting histogram correctly.
    }
    return std::hash<std::string>{}(hash_str);
}

/**
* @brief Build a time-based histogram over the current trace.
*
* This function constructs and executes a histogram query using the
* SQL fragments produced by GetHistogramQueryPrefix() and
* GetHistogramQuerySuffix(). The histogram partitions the trace
* duration into a fixed number of buckets and aggregates events into
* those buckets based on their overlap with each bucket interval.
*
* @param future
*        Pointer to a Future object used to execute the query and/or
*        retrieve the resulting histogram data asynchronously.
* @param desired_bins
*        Requested number of histogram buckets. This value is used,
*        together with the total trace duration, to derive a bucket
*        size that determines the temporal resolution of the
*        histogram.
*
* @return rocprofvis_dm_result_t
*         kRocProfVisDmResultSuccess on success, or an appropriate
*         error code if the histogram could not be constructed.
*/
rocprofvis_dm_result_t ProfileDatabase::BuildHistogram(Future* future, uint32_t desired_bins) {

    const char* start_time_substring = "%START_TIME%";

    typedef struct store_params {
        uint32_t id;
        uint32_t track_id;
        uint32_t bucket_num;
        uint32_t events_count;
        double bucket_value;
    } store_params;

    rocprofvis_dm_result_t result = kRocProfVisDmResultSuccess;

    uint64_t trace_length = TraceProperties()->trace_duration;

    // On an incremental add keep the bucket size chosen on the initial load so the newly
    // added node's histogram buckets line up with the ones already computed.
    uint64_t bucket_size = IsIncrementalLoad()
                               ? TraceProperties()->histogram_bucket_size
                               : (trace_length + desired_bins) / desired_bins;

    // Guard against a zero bucket size (an incremental add before any full load set it, or a
    // zero desired_bins): a zero here would divide-by-zero below and in the bucket queries.
    if(bucket_size == 0)
    {
        bucket_size = (desired_bins > 0) ? (trace_length + desired_bins) / desired_bins : 1;
        if(bucket_size == 0)
        {
            bucket_size = 1;
        }
    }

    if (!IsIncrementalLoad())
    {
        TraceProperties()->histogram_bucket_size = bucket_size;
    }
    // Keep bucket_size stable across incremental adds (so buckets stay aligned) but let the
    // bucket count grow if the newly added file extends the overall trace duration.
    TraceProperties()->histogram_bucket_count = (trace_length + bucket_size) / bucket_size;


    std::string histogram_query_prefix = GetHistogramQueryPrefix(bucket_size);
    std::string histogram_query_suffix = GetHistogramQuerySuffix();

    const char* histogram_table_name = GetMetadataVersionControl()->GetHistogramTableName();
    

    for (auto& file_node : m_db_nodes)
    {
        // Incremental add: existing nodes' histograms are already loaded in memory; only
        // process the newly added node so their buckets are not loaded/duplicated again.
        if (!ShouldProcessInstance(file_node->node_id)) continue;
        std::vector<store_params> v;
        TemporaryDbInstance db_instance(file_node->node_id);
        if (false == GetMetadataVersionControl()->MustRebuildHistogram(file_node->node_id))
        {
            result = ExecuteSQLQuery(future, &db_instance, (std::string("SELECT * FROM ") + histogram_table_name).c_str(), &CallBackLoadHistogram);
        }
        else
        {
            // Only the instances of the node being processed: on an incremental add this is
            // just the new file, so ExecuteQueryForAllTracksAsync runs the histogram query
            // for the new file's tracks instead of re-querying every already-loaded file. (On
            // a full load each node's iteration handles its own tracks, so each track's
            // histogram is computed once instead of once per node.)
            guid_list_t guids_per_file;
            for (GuidInfo& guid_info : DbInstances())
            {
                if (guid_info.first.FileIndex() == file_node->node_id)
                {
                    guids_per_file.push_back(guid_info);
                }
            }

            auto insert_start_time = [&](rocprofvis_dm_track_params_t* params, rocprofvis_dm_charptr_t query) -> std::string {
                DbInstance* params_db_instance = (DbInstance*)params->track_indentifiers.db_instance;
                std::string str = query;
                size_t pos = str.find(start_time_substring);
                if (pos != std::string::npos) {
                    str.replace(pos, strlen(start_time_substring), std::to_string(TraceProperties()->db_inst_start_time[params_db_instance->GuidIndex()]));
                }
                return str;
                };

            result = ExecuteQueryForAllTracksAsync(
                kRocProfVisDmTrySplitTrack | kRocProfVisDmIncludeStreamTracks, kRPVRocpdQuerySliceByTrackSliceQuery,
                histogram_query_prefix.c_str(),
                histogram_query_suffix.c_str(), &CallbackMakeHistogramPerTrack,
                insert_start_time,
                [](rocprofvis_dm_track_params_t* params) { (void) params; },
                guids_per_file);


            if (kRocProfVisDmResultSuccess == result)
            {
                std::string histogram_query = std::string("SELECT (") + Builder::START_SERVICE_NAME + " - " +
                    start_time_substring + ") / " +
                    std::to_string(bucket_size) + " AS bucket, COUNT(*), AVG(" + Builder::COUNTER_VALUE_SERVICE_NAME+"), ";

                result = ExecuteQueryForAllTracksAsync(
                    kRocProfVisDmTrySplitTrack | kRocProfVisDmIncludePmcTracksOnly, kRPVRocpdQuerySliceByTrackSliceQuery,
                    histogram_query.c_str(),
                    "GROUP BY bucket", &CallbackMakeHistogramPerTrack,
                    insert_start_time,
                    [](rocprofvis_dm_track_params_t* params) { (void) params; },
                    guids_per_file);
            }

            if (kRocProfVisDmResultSuccess == result)
            {
                // use last known value for all missing buckets in counter's track histogram
                for (int i = 0; i < NumTracks(); i++)
                {
                    auto & data = TrackPropertiesAt(i)->histogram;

                    if (data.size() > 1)
                    {

                        auto it = data.begin();
                        auto next = std::next(it);

                        while (next != data.end()) {
                            uint32_t x0 = it->first;
                            uint32_t x1 = next->first;
                            double   y0 = it->second.second;

                            for (uint32_t x = x0 + 1; x < x1; ++x) {
                                data.emplace(x, std::make_pair(0, y0));
                            }

                            it = next;
                            ++next;
                        }
                    }
                }

                uint32_t counter = 0;
                for (int i = 0; i < NumTracks(); i++)
                {
                    DbInstance* track_db_instance = (DbInstance*)TrackPropertiesAt(i)->track_indentifiers.db_instance;
                    if (file_node->node_id == track_db_instance->FileIndex())
                    {
                        for (auto& [key, value] : TrackPropertiesAt(i)->histogram)
                        {
                            store_params p;
                            p.id = counter++;
                            p.track_id = i;
                            p.bucket_num = key;
                            p.events_count = value.first;
                            p.bucket_value = value.second;
                            v.push_back(p);
                        }
                    }
                }

                result = CreateSQLTable(
                    histogram_table_name, s_histogram_schema_params,
                    v.size(),
                    [&](sqlite3_stmt* stmt, int index) {
                        store_params& p = v[index];
                        sqlite3_bind_int(stmt, 1, p.id);
                        sqlite3_bind_int(stmt, 2, p.track_id);
                        sqlite3_bind_int(stmt, 3, p.bucket_num);
                        sqlite3_bind_int(stmt, 4, p.events_count);
                        sqlite3_bind_double(stmt, 5, p.bucket_value);
                    },
                    file_node->node_id);

            }        
        }      
    }
    for (int i = 0; i < NumTracks(); i++)
    {
        // Incremental add: TraceProperties()->histogram already holds the aggregate for the
        // previously-loaded instances. Only add the newly-processed node's tracks, otherwise
        // every existing instance would be double-counted (2*old + new) into the overview.
        if (IsIncrementalLoad())
        {
            DbInstance* inst = (DbInstance*)TrackPropertiesAt(i)->track_indentifiers.db_instance;
            if (inst && !ShouldProcessInstance(inst->FileIndex())) continue;
        }
        for (auto& [key, value] : TrackPropertiesAt(i)->histogram)
        {
            TraceProperties()->histogram[key] += value.first;
        }
    }
    return result;
}

void
ProfileDatabase::UpdateQueryForTrack(  rocprofvis_dm_track_params_it it, 
    rocprofvis_dm_track_params_t& newprops,
    std::vector<rocprofvis_dm_string_t> & newqueries)
{

    int slice_query_category        = newprops.track_indentifiers.category == kRocProfVisDmStreamTrack
        ? kRPVRocpdQuerySliceByStream
        : kRPVRocpdQuerySliceByQueue;
    int slice_source_query_category = newprops.track_indentifiers.category == kRocProfVisDmStreamTrack
        ? kRPVSourceQuerySliceByStream
        : kRPVSourceQuerySliceByQueue;
    if (it != TrackPropertiesEnd()) {
        std::vector<rocprofvis_dm_string_t>::iterator s = 
            std::find_if(it->get()->query[slice_query_category].begin(), 
                it->get()->query[slice_query_category].end(),
                [newqueries, slice_source_query_category](rocprofvis_dm_string_t& str) {
                    return str == newqueries[slice_source_query_category];
                });
        if(s == it->get()->query[slice_query_category].end())
        {
            it->get()->query[slice_query_category].push_back(
                newqueries[slice_source_query_category]);
        }

        s = std::find_if(it->get()->query[kRPVRocpdQueryTable].begin(),
            it->get()->query[kRPVRocpdQueryTable].end(),
            [newqueries](rocprofvis_dm_string_t& str) {
                return str == newqueries[kRPVSourceQueryTable];
            });
        if(s == it->get()->query[kRPVRocpdQueryTable].end())
        {
            it->get()->query[kRPVRocpdQueryTable].push_back(newqueries[kRPVSourceQueryTable]);
        }
        s = std::find_if(it->get()->query[kRPVRocpdQueryLevel].begin(),
            it->get()->query[kRPVRocpdQueryLevel].end(),
            [newqueries](rocprofvis_dm_string_t& str) {
                return str == newqueries[kRPVSourceQueryLevel];
            });
        if(s == it->get()->query[kRPVRocpdQueryLevel].end())
        {
            it->get()->query[kRPVRocpdQueryLevel].push_back(newqueries[kRPVSourceQueryLevel]);
        }
        return;
    } 
    newprops.query[slice_query_category].push_back(newqueries[slice_source_query_category]);
    newprops.query[kRPVRocpdQueryTable].push_back(newqueries[kRPVSourceQueryTable]);
    newprops.query[kRPVRocpdQueryLevel].push_back(newqueries[kRPVSourceQueryLevel]); 
}

}  // namespace DataModel
}  // namespace RocProfVis
