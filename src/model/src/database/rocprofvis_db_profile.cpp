// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_db_profile.h"
#include "rocprofvis_db_expression_filter.h"
#include "rocprofvis_c_interface.h"
#include <sstream>
#include <unordered_set>
#include <cfloat>

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

bool
ProfileDatabase::isServiceColumn(const char* name)
{
    static const std::vector<std::string> service_columns = {
        Builder::SPACESAVER_SERVICE_NAME,
        Builder::AGENT_ID_SERVICE_NAME,
        Builder::QUEUE_ID_SERVICE_NAME,
        Builder::STREAM_ID_SERVICE_NAME,
        Builder::OPERATION_SERVICE_NAME,
        Builder::PROCESS_ID_SERVICE_NAME,
        Builder::THREAD_ID_SERVICE_NAME
    };
    for (std::string service_column : service_columns)
    {
        if(service_column == name) return true;
    }
    return false;
}

rocprofvis_dm_event_operation_t ProfileDatabase::GetTableQueryOperation(std::string query)
{
    std::string select_str = "SELECT ";
    if (query.find(select_str) == 0)
    {
        return (rocprofvis_dm_event_operation_t)std::atol(query.substr(select_str.size(), 1).c_str());
    }
    return kRocProfVisDmOperationNoOp;
}


int ProfileDatabase::CallBackAddTrack(void *data, int argc, sqlite3_stmt* stmt, char **azColName){
    ROCPROFVIS_ASSERT_MSG_RETURN(argc==NUMBER_OF_TRACK_IDENTIFICATION_PARAMETERS+4, ERROR_DATABASE_QUERY_PARAMETERS_MISMATCH, 1);
    ROCPROFVIS_ASSERT_MSG_RETURN(data, ERROR_SQL_QUERY_PARAMETERS_CANNOT_BE_NULL, 1);
    void* func = (void*)&CallBackAddTrack;
    rocprofvis_dm_track_params_t track_params = {0};
    rocprofvis_db_sqlite_callback_parameters* callback_params = (rocprofvis_db_sqlite_callback_parameters*)data;
    ProfileDatabase* db = (ProfileDatabase*)callback_params->db;
    if(callback_params->future->Interrupted()) return SQLITE_ABORT;
    track_params.load_id.insert(callback_params->track_id);
    track_params.track_id = (rocprofvis_dm_track_id_t)db->NumTracks();
    track_params.process.category = (rocprofvis_dm_track_category_t)db->Sqlite3ColumnInt(func, stmt, azColName,TRACK_ID_CATEGORY);
    track_params.op = track_params.record_count=db->Sqlite3ColumnInt(func, stmt, azColName,TRACK_ID_CATEGORY+1);
    track_params.record_count=db->Sqlite3ColumnInt(func, stmt, azColName,TRACK_ID_CATEGORY+3);
    if (track_params.op < kRocProfVisDmNumOperation)
    {
        db->TraceProperties()->events_count[track_params.op] += track_params.record_count;
    }
    for (int i = 0; i < NUMBER_OF_TRACK_IDENTIFICATION_PARAMETERS; i++) {
        track_params.process.tag[i] = azColName[i];
        char* arg = (char*) db->Sqlite3ColumnText(func, stmt, azColName, i);
        track_params.process.is_numeric[i] = Database::IsNumber(arg);
        if (track_params.process.is_numeric[i]) {
            track_params.process.id[i] = db->Sqlite3ColumnInt64(func, stmt, azColName, i);
        } else {
            track_params.process.name[i] = arg;
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
    ProfileDatabase* db = (ProfileDatabase*)callback_params->db;
    if(callback_params->future->Interrupted()) return SQLITE_ABORT;
    track_params.load_id.insert(callback_params->track_id);
    track_params.track_id = (rocprofvis_dm_track_id_t)db->NumTracks();
    track_params.process.category = (rocprofvis_dm_track_category_t)db->Sqlite3ColumnInt(func, stmt, azColName,kRpvDbTrackLoadCategory);
    track_params.op = track_params.record_count=db->Sqlite3ColumnInt(func, stmt, azColName,kRpvDbTrackLoadOp);
    track_params.record_count=db->Sqlite3ColumnInt(func, stmt, azColName,kRpvDbTrackLoadRecordCount);
    if (track_params.op < kRocProfVisDmNumOperation)
    {
        db->TraceProperties()->events_count[track_params.op] += track_params.record_count;
    }
    track_params.min_ts=db->Sqlite3ColumnInt64(func, stmt, azColName,kRpvDbTrackLoadMinTs);
    track_params.max_ts=db->Sqlite3ColumnInt64(func, stmt, azColName,kRpvDbTrackLoadMaxTs);
    track_params.min_value=db->Sqlite3ColumnDouble(func, stmt, azColName,kRpvDbTrackLoadMinValue);
    track_params.max_value=db->Sqlite3ColumnDouble(func, stmt, azColName,kRpvDbTrackLoadMaxValue);
    db->TraceProperties()->start_time = std::min(db->TraceProperties()->start_time,track_params.min_ts);
    db->TraceProperties()->end_time  = std::max(db->TraceProperties()->end_time,track_params.max_ts);
    track_params.process.id[TRACK_ID_NODE] = db->Sqlite3ColumnInt64(func, stmt, azColName,kRpvDbTrackLoadNodeId);
    track_params.process.id[TRACK_ID_PID_OR_AGENT] = db->Sqlite3ColumnInt64(func, stmt, azColName,kRpvDbTrackLoadProcessId);
    track_params.process.is_numeric[TRACK_ID_NODE] = true;
    track_params.process.is_numeric[TRACK_ID_PID_OR_AGENT] = true;
    std::string sub_process = db->Sqlite3ColumnText(func, stmt, azColName,kRpvDbTrackLoadSubprocessId);
    track_params.process.is_numeric[TRACK_ID_TID_OR_QUEUE] = Database::IsNumber(sub_process);
    if (track_params.process.is_numeric[TRACK_ID_TID_OR_QUEUE]) {
        track_params.process.id[TRACK_ID_TID_OR_QUEUE] = std::atoll(sub_process.c_str());
    } else {
        track_params.process.name[TRACK_ID_TID_OR_QUEUE] = sub_process;
    }   
    track_params.process.tag[TRACK_ID_NODE] = db->Sqlite3ColumnText(func, stmt, azColName,kRpvDbTrackLoadNodeTag);
    track_params.process.tag[TRACK_ID_PID_OR_AGENT] = db->Sqlite3ColumnText(func, stmt, azColName,kRpvDbTrackLoadProcessTag);
    track_params.process.tag[TRACK_ID_TID_OR_QUEUE] = db->Sqlite3ColumnText(func, stmt, azColName,kRpvDbTrackLoadSubprocessTag);

    db->ProcessTrack(track_params, callback_params->query);

    callback_params->future->CountThisRow();
    return 0;
}

int ProfileDatabase::CallbackMakeHistogramPerTrack(void* data, int argc, sqlite3_stmt* stmt,
    char** azColName) {
    ROCPROFVIS_ASSERT_MSG_RETURN(argc == 4, ERROR_DATABASE_QUERY_PARAMETERS_MISMATCH, 1);
    ROCPROFVIS_ASSERT_MSG_RETURN(data, ERROR_SQL_QUERY_PARAMETERS_CANNOT_BE_NULL, 1);
    void *func = (void*)&CallbackMakeHistogramPerTrack;
    rocprofvis_db_sqlite_callback_parameters* callback_params =
        (rocprofvis_db_sqlite_callback_parameters*) data;
    ProfileDatabase* db = (ProfileDatabase*) callback_params->db;
    if(callback_params->future->Interrupted()) return SQLITE_ABORT;
    uint32_t index                             = db->Sqlite3ColumnInt(func, stmt, azColName, 3);
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
        uint64_t process = db->Sqlite3ColumnInt(func, stmt, azColName, 7);
        std::string subprocess = db->Sqlite3ColumnText(func, stmt, azColName, 8);
        if (db->FindTrackId(0, process, subprocess.c_str(), record.event.id.bitfield.event_op,
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
            record.pmc.value = db->Sqlite3ColumnDouble(func, stmt, azColName,2);
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
    if (db->FindTrackId(0, 
                        (uint32_t)db->Sqlite3ColumnInt(func, stmt, azColName,4), 
                        (const char*)db->Sqlite3ColumnText(func, stmt, azColName,5),
                        record.id.bitfield.event_op, record.track_id) == kRocProfVisDmResultSuccess) {
        record.id.bitfield.event_id = db->Sqlite3ColumnInt64(func, stmt, azColName, 2 );
        record.time = db->Sqlite3ColumnInt64(func, stmt, azColName, 6 );
        record.category_id = db->Sqlite3ColumnInt64(func, stmt, azColName, 7);
        record.symbol_id = db->Sqlite3ColumnInt64(func, stmt, azColName, 8);
        record.level = db->Sqlite3ColumnInt64(func, stmt, azColName, 9);
        record.end_time = db->Sqlite3ColumnInt64(func, stmt, azColName, 10);    
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


void
ProfileDatabase::FindTrackIDs(
    ProfileDatabase* db, rocprofvis_db_sqlite_track_service_data_t& service_data,
    int& trackId, int & streamTrackId)
{ 
    trackId = -1;
    streamTrackId = -1;
    rocprofvis_dm_process_identifiers_t process;
    for(int i = 0; i < NUMBER_OF_TRACK_IDENTIFICATION_PARAMETERS; i++)
    {
        process.is_numeric[i] = true;
    }
    process.category = service_data.category;
    if(service_data.category == kRocProfVisDmKernelDispatchTrack ||
        service_data.category == kRocProfVisDmMemoryAllocationTrack ||
        service_data.category == kRocProfVisDmMemoryCopyTrack ||
        service_data.category == kRocProfVisDmRegionTrack ||
        service_data.category == kRocProfVisDmRegionMainTrack ||
        service_data.category == kRocProfVisDmRegionSampleTrack)
    {
        process.id[TRACK_ID_NODE]         = service_data.nid;
        process.id[TRACK_ID_PID_OR_AGENT] = service_data.process;
        process.id[TRACK_ID_TID_OR_QUEUE] = service_data.thread;
        rocprofvis_dm_track_params_it it = db->FindTrack(process);
        if(it != db->TrackPropertiesEnd())
        {
            trackId = it->get()->track_id;
        }
        process.category            = kRocProfVisDmStreamTrack;
        process.id[TRACK_ID_STREAM] = service_data.stream_id;
        process.id[TRACK_ID_QUEUE]  = -1;
        it                          = db->FindTrack(process);
        if(it != db->TrackPropertiesEnd())
        {
            streamTrackId = it->get()->track_id;
        }
    }
    else if(service_data.category == kRocProfVisDmPmcTrack)
    {
        process.id[TRACK_ID_NODE]    = service_data.nid;
        process.id[TRACK_ID_AGENT]   = service_data.process;
        process.id[TRACK_ID_COUNTER] = service_data.thread;
        if(service_data.monitor_type.length() > 0)
        {
            process.is_numeric[TRACK_ID_COUNTER] = false;
            process.name[TRACK_ID_COUNTER]       = service_data.monitor_type;
        }

        rocprofvis_dm_track_params_it it = db->FindTrack(process);
        if(it != db->TrackPropertiesEnd())
        {
            trackId = it->get()->track_id;
        }
    }
}


rocprofvis_dm_track_category_t
ProfileDatabase::TranslateOperationToTrackCategory(rocprofvis_dm_event_operation_t op) {
    switch (op)
    {
    case kRocProfVisDmOperationLaunch: return GetRegionTrackCategory();
    case kRocProfVisDmOperationLaunchSample: return kRocProfVisDmRegionSampleTrack;
    case kRocProfVisDmOperationDispatch: return kRocProfVisDmKernelDispatchTrack;
    case kRocProfVisDmOperationMemoryAllocate: return kRocProfVisDmMemoryAllocationTrack;
    case kRocProfVisDmOperationMemoryCopy: return kRocProfVisDmMemoryCopyTrack;
    case kRocProfVisDmOperationNoOp: return kRocProfVisDmPmcTrack;

    }
    return kRocProfVisDmNotATrack;
}

const rocprofvis_dm_track_search_id_t
ProfileDatabase::GetTrackSearchId(rocprofvis_dm_track_category_t category)
{
    switch (category)
    {
    case kRocProfVisDmPmcTrack:
        return kRPVTrackSearchIdCounters;
    case kRocProfVisDmRegionTrack:
    case kRocProfVisDmRegionMainTrack:
        return kRPVTrackSearchIdThreads;
    case kRocProfVisDmRegionSampleTrack:
        return kRPVTrackSearchIdThreadSamples;
    case kRocProfVisDmKernelDispatchTrack: 
        return kRPVTrackSearchIdDispatches;
    case kRocProfVisDmMemoryAllocationTrack: 
        return kRPVTrackSearchIdMemAllocs;
    case kRocProfVisDmMemoryCopyTrack: 
        return kRPVTrackSearchIdMemCopies;
    case kRocProfVisDmStreamTrack: 
        return kRPVTrackSearchIdStreams;

    }
    return kRPVTrackSearchIdUnknown;
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
        service_data.category = db->TranslateOperationToTrackCategory(service_data.op);
    }
    else if(column_name == Builder::NODE_ID_SERVICE_NAME)
    {
        service_data.nid = db->Sqlite3ColumnInt64(func, stmt, azColName, column_index);
    }
    else if(column_name == Builder::AGENT_ID_SERVICE_NAME)
    {
        service_data.process = db->Sqlite3ColumnInt(func, stmt, azColName, column_index);
    }
    else if(column_name == Builder::QUEUE_ID_SERVICE_NAME)
    {
        service_data.thread = db->Sqlite3ColumnInt(func, stmt, azColName, column_index);
    }
    else if(column_name == Builder::STREAM_ID_SERVICE_NAME)
    {
        service_data.stream_id = db->Sqlite3ColumnInt(func, stmt, azColName, column_index);
    }
    else if(column_name == Builder::PROCESS_ID_SERVICE_NAME)
    {
        service_data.process = db->Sqlite3ColumnInt(func, stmt, azColName, column_index);
    }
    else if(column_name == Builder::THREAD_ID_SERVICE_NAME)
    {
        service_data.thread = db->Sqlite3ColumnInt(func, stmt, azColName, column_index);
    }
    else if(column_name == Builder::COUNTER_ID_SERVICE_NAME)
    {
        service_data.thread = db->Sqlite3ColumnInt(func, stmt, azColName, column_index);
    }
    else if(column_name == Builder::COUNTER_NAME_SERVICE_NAME)
    {
        service_data.monitor_type =
            db->Sqlite3ColumnText(func, stmt, azColName, column_index);
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
        if ((flags & kRocProfVisDmTrySplitTrack) && TrackPropertiesAt(i)->record_count > SINGLE_THREAD_RECORDS_COUNT_LIMIT)
        {
            size_t total_event_count = 0;
            for (int j = kRocProfVisDmOperationLaunch; j < kRocProfVisDmNumOperation; j++)
            {
                total_event_count += TraceProperties()->events_count[j];
            }
            split_count = (TrackPropertiesAt(i)->record_count * 10) / total_event_count;

            if (split_count == 0)
            {
                split_count = 1;
            } else if ((TrackPropertiesAt(i)->record_count / split_count) < SINGLE_THREAD_RECORDS_COUNT_LIMIT)
            {
                split_count = (TrackPropertiesAt(i)->record_count + SINGLE_THREAD_RECORDS_COUNT_LIMIT) / SINGLE_THREAD_RECORDS_COUNT_LIMIT;
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
        
    }
    for (int i = 0; i < NumTracks(); i++)
    {
        func_clear(TrackPropertiesAt(i));
    }
    return result;
}

rocprofvis_dm_result_t
ProfileDatabase::ExecuteQueriesAsync(
    std::vector<std::string>& queries,
    std::vector<Future*> & sub_futures,
    rocprofvis_dm_handle_t handle,
    RpvSqliteExecuteQueryCallback callback)
{
    std::vector<Future*> futures = sub_futures;
    rocprofvis_dm_result_t result = kRocProfVisDmResultSuccess;
    futures.resize(queries.size());
    for(int i = 0; i < queries.size(); i++)
    {
        futures[i]     = (Future*)rocprofvis_db_future_alloc(nullptr);
        try
        {
            futures[i]->SetWorker(std::move(
                std::thread(SqliteDatabase::ExecuteSQLQueryStaticWithHandle, this,
                    futures[i],
                    queries[i].c_str(), handle, i, callback)));
        } catch(std::exception ex)
        {
            result = kRocProfVisDmResultUnknownError;
            ROCPROFVIS_ASSERT_MSG_BREAK(false, ex.what());
        }       
    }
    for(int i = 0; i < queries.size(); i++)
    {
        if(futures[i] != nullptr)
        {
            if(kRocProfVisDmResultSuccess !=
                rocprofvis_db_future_wait(futures[i], UINT64_MAX))
            {
                result = kRocProfVisDmResultUnknownError;
            }
            auto it = std::find_if(sub_futures.begin(), sub_futures.end(), [&](Future* f) { return f == futures[i]; });
            if (it != sub_futures.end())
            {
                sub_futures.erase(it);
                rocprofvis_db_future_free(*it);
                futures[i] = nullptr;
            }
        }
    }
    futures.clear();
    return result;
}

void ProfileDatabase::BuildSliceQueryMap(slice_query_t& slice_query_map, rocprofvis_dm_track_params_t* props)
{
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
}

rocprofvis_dm_result_t ProfileDatabase::BuildCounterSliceLeftNeighbourQuery(rocprofvis_dm_timestamp_t start, rocprofvis_dm_timestamp_t end, rocprofvis_dm_index_t track_index, rocprofvis_dm_string_t& query) {
    slice_query_t slice_query_map;
    bool timed_query = false;

    rocprofvis_dm_track_params_t* props = TrackPropertiesAt(track_index);
    BuildSliceQueryMap(slice_query_map, props);

    for (auto it_query = slice_query_map.begin(); it_query != slice_query_map.end(); ++it_query) {
        query += "SELECT * FROM ( ";
        query += it_query->first;
        query += it_query->second;
        query += ") and ";
        query += Builder::START_SERVICE_NAME;
        query += " < ";
        query += std::to_string(start);
        query += std::string(" ORDER BY ") + Builder::START_SERVICE_NAME + " DESC LIMIT 1 )";
        break;
    }
    return kRocProfVisDmResultSuccess;

}

rocprofvis_dm_result_t ProfileDatabase::BuildCounterSliceRightNeighbourQuery(rocprofvis_dm_timestamp_t start, rocprofvis_dm_timestamp_t end, rocprofvis_dm_index_t track_index, rocprofvis_dm_string_t& query) {
    slice_query_t slice_query_map;
    bool timed_query = false;

    rocprofvis_dm_track_params_t* props = TrackPropertiesAt(track_index);
    BuildSliceQueryMap(slice_query_map, props);

    for (auto it_query = slice_query_map.begin(); it_query != slice_query_map.end(); ++it_query) {
        query += "SELECT * FROM ( ";
        query += it_query->first;
        query += it_query->second;
        query += ") and ";
        query += Builder::START_SERVICE_NAME;
        query += " > ";
        query += std::to_string(end);
        query += std::string(" ORDER BY ") + Builder::START_SERVICE_NAME + " ASC LIMIT 1 )";
        break;
    }
    return kRocProfVisDmResultSuccess;

}

rocprofvis_dm_result_t ProfileDatabase::BuildSliceQuery(rocprofvis_dm_timestamp_t start, rocprofvis_dm_timestamp_t end, rocprofvis_db_num_of_tracks_t num, rocprofvis_db_track_selection_t tracks, rocprofvis_dm_string_t& query, slice_array_t& slices) {
    slice_query_t slice_query_map;
    bool timed_query = false;
    bool pmc_query = false;
    for (int i = 0; i < num; i++){
        slices[tracks[i]]=BindObject()->FuncAddSlice(BindObject()->trace_object, tracks[i], start, end);
        rocprofvis_dm_track_params_t* props = TrackPropertiesAt(tracks[i]);
        if (props->process.category == kRocProfVisDmPmcTrack)
        {
            pmc_query = true;
        }
        BuildSliceQueryMap(slice_query_map, props);
        if (start > props->min_ts || end < props->max_ts)
        {
            timed_query = true;
        }
    }

    query = (slice_query_map.size() > 1) ? "SELECT * FROM ( " : "";
    for (std::map<std::string, std::string>::iterator it_query = slice_query_map.begin(); it_query != slice_query_map.end(); ++it_query) {
        if (it_query!=slice_query_map.begin()) query += " UNION ALL ";
        query += it_query->first;
        query += it_query->second;
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
    query += (slice_query_map.size() > 1) ? ")" : "";
    query += std::string(" ORDER BY ") + Builder::START_SERVICE_NAME;
    if (!pmc_query)
    {
        query += std::string(", ") + Builder::EVENT_LEVEL_SERVICE_NAME;
    }
    query += ";";
    return kRocProfVisDmResultSuccess;

}

rocprofvis_dm_result_t
ProfileDatabase::BuildTableQuery(
    rocprofvis_dm_timestamp_t start, rocprofvis_dm_timestamp_t end,
    rocprofvis_db_num_of_tracks_t num, rocprofvis_db_track_selection_t tracks, rocprofvis_dm_charptr_t filter,
    rocprofvis_dm_charptr_t group, rocprofvis_dm_charptr_t group_cols, rocprofvis_dm_charptr_t sort_column,
    rocprofvis_dm_sort_order_t sort_order, rocprofvis_dm_num_string_table_filters_t num_string_table_filters, rocprofvis_dm_string_table_filters_t string_table_filters,
    uint64_t max_count, uint64_t offset, bool count_only, bool summary, rocprofvis_dm_string_t& query)
{
    std::vector<slice_query_t> slice_query_map_array;
    table_string_id_filter_map_t string_id_filter_map;
    std::string group_by_select;
    std::string group_by;
    if(summary)
    {
        bool sample_query = false;
        if(TABLE_QUERY_UNPACK_OP_TYPE(tracks[0]) == 0)
        {
            sample_query = TrackPropertiesAt(tracks[0])->process.category == kRocProfVisDmPmcTrack;
        }
        else
        {
            sample_query = (rocprofvis_dm_event_operation_t)TABLE_QUERY_UNPACK_OP_TYPE(tracks[0]) == kRocProfVisDmOperationNoOp;
        }        
        BuildTableSummaryClause(sample_query, group_by_select, group_by);
    }
    else
    {
        if(group)
        {
            group_by = group;
            if(group_cols)
            {
                group_by_select = group_cols;
            }
        }
    }
    rocprofvis_dm_result_t string_filter_result = BuildTableStringIdFilter(num_string_table_filters, string_table_filters, string_id_filter_map);
    slice_query_map_array.resize(num);
    for (int i = 0; i < num; i++){
        rocprofvis_dm_index_t track = tracks[i];
        if(TABLE_QUERY_UNPACK_OP_TYPE(track) == 0)
        {
            track = TABLE_QUERY_UNPACK_TRACK_ID(track);
            rocprofvis_dm_track_params_t* props = TrackPropertiesAt(track);
            for(int j = 0; j < props->query[kRPVQueryTable].size(); j++)
            {
                std::string q     = props->query[kRPVQueryTable][j]; 
                std::string tuple = "(";
                for (int k = 0; k < NUMBER_OF_TRACK_IDENTIFICATION_PARAMETERS; k++) {
                    if (props->process.tag[k] != "const") {
                        if (tuple.length() > 1) tuple += ",";
                        tuple += props->process.tag[k];
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
                if (slice_query_map_array[i][q].length() > 0) slice_query_map_array[i][q] += ", ";
                slice_query_map_array[i][q] += tuple ;
            }
        }
        else if(string_filter_result == kRocProfVisDmResultSuccess && string_id_filter_map.count((rocprofvis_dm_event_operation_t)TABLE_QUERY_UNPACK_OP_TYPE(track)) > 0)
        {
            track = TABLE_QUERY_UNPACK_OP_TYPE(track);
            slice_query_map_array[i][GetEventOperationQuery((rocprofvis_dm_event_operation_t)track)] = " WHERE " + string_id_filter_map.at((rocprofvis_dm_event_operation_t)track);
        }
        else
        {
            continue;
        }
        if(string_filter_result == kRocProfVisDmResultSuccess && slice_query_map_array[i].empty())
        {
            return kRocProfVisDmResultSuccess;
        }
    }
    query = "";

    size_t thread_count = std::thread::hardware_concurrency();
    bool event_table = false;
    for (int i = 0; i < num; i++)
    {
        rocprofvis_dm_index_t track = tracks[i];
        track = TABLE_QUERY_UNPACK_TRACK_ID(track);
        rocprofvis_dm_track_params_t* props = TrackPropertiesAt(track);
        int divider = thread_count / num;
        if (divider == 0) divider = 1;
        for (std::map<std::string, std::string>::iterator it_query = slice_query_map_array[i].begin(); it_query != slice_query_map_array[i].end(); ++it_query) 
        {
            auto op = GetTableQueryOperation(it_query->first);
            if (op > kRocProfVisDmOperationNoOp)
            {
                event_table = true;
            }
            if (props->record_count < SINGLE_THREAD_RECORDS_COUNT_LIMIT ||
                op == kRocProfVisDmOperationMemoryAllocate || 
                op == kRocProfVisDmOperationMemoryCopy)
                divider = 1;
            uint64_t step = (end - start) / divider;
            uint64_t begin = start;
            for (int j = 0; j < divider; j++)
            {
                query += it_query->first;
                query += it_query->second;
                query += ") and ";
                query += Builder::START_SERVICE_NAME;
                query += " >= ";
                query += std::to_string(begin+(step*j));
                query += " and ";
                query += Builder::END_SERVICE_NAME;
                query += " < ";
                query += std::to_string(begin+(step*j)+step);

                query += ";";
                query += std::to_string(track);
                query += "\n";
            }

        }
    }

    query += "-- CMD: TYPE ";
    query += string_filter_result == kRocProfVisDmResultSuccess ? "2" : event_table ? "0" : "1";
    query += "\n";

    if (!group_by.empty())
    {
        query += "-- CMD: GROUP ";
        if (!group_by_select.empty())
        {
            query += group_by_select;
        }
        else
        {
            query += group_by;
            query += ", COUNT(*) as num_invocations, AVG(duration) as avg_duration, "
                "MIN(duration) as min_duration, MAX(duration) as max_duration";
        }
        query += "\n";
    }

    if (filter && strlen(filter))
    {
        query += "-- CMD: FILTER ";
        query += filter;
        query += "\n";
    }

    if (sort_column && strlen(sort_column))
    {
        query += "-- CMD: SORT";
        if (sort_order == kRPVDMSortOrderAsc)
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
    } else
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

        std::string slice_query;
        slice_array_t slices;
        rocprofvis_dm_result_t result = BuildSliceQuery(start, end, num, tracks, slice_query, slices);
        std::string query;

        if (result == kRocProfVisDmResultSuccess)
        {
            for (int i = 0; i < num; i++)
            {
                rocprofvis_dm_track_params_t* props = TrackPropertiesAt(tracks[i]);
                if (props->process.category == kRocProfVisDmPmcTrack)
                {
                    result = BuildCounterSliceLeftNeighbourQuery(start, end, tracks[i], query);
                    if (result != kRocProfVisDmResultSuccess) break;
                    result = ExecuteSQLQuery(future, query.c_str(), &slices, &CallbackAddAnyRecord);
                    if (result != kRocProfVisDmResultSuccess) break;
                }
            }


            if (result == kRocProfVisDmResultSuccess)
            {

                result = ExecuteSQLQuery(future, slice_query.c_str(), &slices, &CallbackAddAnyRecord);

                if (result == kRocProfVisDmResultSuccess)
                {
                    query = "";
                    for (int i = 0; i < num; i++)
                    {
                        rocprofvis_dm_track_params_t* props = TrackPropertiesAt(tracks[i]);
                        if (props->process.category == kRocProfVisDmPmcTrack)
                        {
                            if (BuildCounterSliceRightNeighbourQuery(start, end, tracks[i], query) != kRocProfVisDmResultSuccess) break;
                            if (ExecuteSQLQuery(future, query.c_str(), &slices, &CallbackAddAnyRecord) != kRocProfVisDmResultSuccess) break;
                        }
                    }
                }

                for (int i = 0; i < num; i++)
                {
                    BindObject()->FuncCompleteSlice(slices[tracks[i]]);
                }
            }
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
        std::vector<std::pair<std::string, uint32_t>> queries;
        std::vector<rocprofvis_db_compound_query_command> commands;
        std::set<uint32_t> tracks;
        std::string query_without_commands = TableProcessor::QueryWithoutCommands(query);
        if (TableProcessor::IsCompoundQuery(query, queries, tracks,  commands))
        {
            auto it = std::find_if(commands.begin(), commands.end(), [](rocprofvis_db_compound_query_command& cmd) { return cmd.name == "TYPE"; });
            rocprofvis_db_compound_table_type data_type = kRPVTableDataTypeEvent;
            if (it != commands.end())
            {
                data_type = (rocprofvis_db_compound_table_type)std::atol(it->parameter.c_str());
            }
            bool query_updated = !m_table_processor[data_type].IsCurrentQuery(query_without_commands.c_str());
            m_table_processor[data_type].SaveCurrentQuery(query_without_commands.c_str());
            if (kRocProfVisDmResultSuccess != m_table_processor[data_type].ExecuteCompoundQuery(future, queries, tracks, commands, table, data_type, query_updated)) break;
        }
        else
        {
            if (kRocProfVisDmResultSuccess != ExecuteSQLQuery(future, query, table, &CallbackRunQuery)) break;
        }
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

rocprofvis_dm_result_t ProfileDatabase::ExportTableCSV(rocprofvis_dm_charptr_t query,
                                                       rocprofvis_dm_charptr_t file_path,
                                                       Future* future)
{
    ROCPROFVIS_ASSERT_MSG_RETURN(file_path, "Output path cannot be NULL.",
                                 kRocProfVisDmResultInvalidParameter);
    ROCPROFVIS_ASSERT_MSG_RETURN(future, ERROR_FUTURE_CANNOT_BE_NULL,
                                 kRocProfVisDmResultInvalidParameter);
    rocprofvis_dm_result_t result = kRocProfVisDmResultInvalidParameter;
    std::string query_without_commands = TableProcessor::QueryWithoutCommands(query);

    Future* internal_future = new Future(nullptr);


    for (int i = 0; i < kRPVTableDataTypesNum; i++)
    {
        if (m_table_processor[i].IsCurrentQuery(query_without_commands.c_str()))
        {
            result = m_table_processor[i].ExportToCSV(file_path);
            break;
        }
    }

    if (result == kRocProfVisDmResultSuccess)
    {
        ShowProgress(100, "CSV export success", kRPVDbSuccess, future);        
    }
    else
    {
        ShowProgress(0, "CSV export failed", kRPVDbError, future);
    }

    return future->SetPromise(result);
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


rocprofvis_dm_result_t ProfileDatabase::SaveTrackProperties(uint64_t hash) {
    SQLInsertParams params[] = { 
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
        { "sub_process_tag", "TEXT" }
    };

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

    } store_params;

    std::vector<store_params> v;
    uint32_t counter=0;
    for (int i = 0; i < NumTracks(); i++)
    {
        for (auto load_id : TrackPropertiesAt(i)->load_id)
        {
            store_params p;
            p.id = counter++;
            p.load_id = load_id;
            p.track_id = TrackPropertiesAt(i)->track_id;
            p.category = TrackPropertiesAt(i)->process.category;
            p.op = TrackPropertiesAt(i)->op;
            p.record_count = TrackPropertiesAt(i)->record_count;
            p.min_ts = TrackPropertiesAt(i)->min_ts;
            p.max_ts = TrackPropertiesAt(i)->max_ts;
            p.min_val = TrackPropertiesAt(i)->min_value;
            p.max_val = TrackPropertiesAt(i)->max_value;
            p.node_id = TrackPropertiesAt(i)->process.id[TRACK_ID_NODE];
            p.process_id = TrackPropertiesAt(i)->process.id[TRACK_ID_PID_OR_AGENT];
            p.subproc_numeric = TrackPropertiesAt(i)->process.is_numeric[TRACK_ID_TID_OR_QUEUE];
            p.subproc_id = TrackPropertiesAt(i)->process.id[TRACK_ID_TID_OR_QUEUE];
            p.subproc_name = TrackPropertiesAt(i)->process.name[TRACK_ID_TID_OR_QUEUE];
            p.node_tag = TrackPropertiesAt(i)->process.tag[TRACK_ID_NODE];
            p.process_tag = TrackPropertiesAt(i)->process.tag[TRACK_ID_PID_OR_AGENT];
            p.subproc_tag = TrackPropertiesAt(i)->process.tag[TRACK_ID_TID_OR_QUEUE];
            v.push_back(p);
        }
    }
    return CreateSQLTable(
        (std::string("track_info_")+std::to_string(hash)).c_str(), params, 16,
        v.size(),
        [&](sqlite3_stmt* stmt, int index) {

                sqlite3_bind_int(stmt, 1, v[index].id);
                sqlite3_bind_int(stmt, kRpvDbTrackLoadId + 1, v[index].load_id);
                sqlite3_bind_int(stmt, kRpvDbTrackLoadTrackId + 1, v[index].track_id);                
                sqlite3_bind_int(stmt, kRpvDbTrackLoadCategory + 1, v[index].category);
                sqlite3_bind_int(stmt, kRpvDbTrackLoadOp + 1, v[index].op);
                sqlite3_bind_int(stmt, kRpvDbTrackLoadRecordCount + 1, v[index].record_count);
                sqlite3_bind_int64(stmt, kRpvDbTrackLoadMinTs + 1, v[index].min_ts);
                sqlite3_bind_int64(stmt, kRpvDbTrackLoadMaxTs + 1, v[index].max_ts);
                sqlite3_bind_double(stmt, kRpvDbTrackLoadMinValue + 1, v[index].min_val);
                sqlite3_bind_double(stmt, kRpvDbTrackLoadMaxValue + 1, v[index].max_val);
                sqlite3_bind_int64(stmt, kRpvDbTrackLoadNodeId + 1, v[index].node_id);
                sqlite3_bind_int64(stmt, kRpvDbTrackLoadProcessId + 1, v[index].process_id);
                if (v[index].subproc_numeric)
                {

                    sqlite3_bind_int64(stmt, kRpvDbTrackLoadSubprocessId + 1, v[index].subproc_id);
                }
                else
                {
                    sqlite3_bind_text(stmt, kRpvDbTrackLoadSubprocessId + 1, v[index].subproc_name.c_str(), -1, SQLITE_STATIC);
                }
                sqlite3_bind_text(stmt, kRpvDbTrackLoadNodeTag + 1, v[index].node_tag.c_str(), -1, SQLITE_STATIC);
                sqlite3_bind_text(stmt, kRpvDbTrackLoadProcessTag + 1, v[index].process_tag.c_str(), -1, SQLITE_STATIC);
                sqlite3_bind_text(stmt, kRpvDbTrackLoadSubprocessTag + 1, v[index].subproc_tag.c_str(), -1, SQLITE_STATIC);
        });
}


int ProfileDatabase::CallBackLoadHistogram(void* data, int argc, sqlite3_stmt* stmt, char** azColName) {
    ROCPROFVIS_ASSERT_MSG_RETURN(argc == 4, ERROR_DATABASE_QUERY_PARAMETERS_MISMATCH, 1);
    ROCPROFVIS_ASSERT_MSG_RETURN(data, ERROR_SQL_QUERY_PARAMETERS_CANNOT_BE_NULL, 1);
    void* func = (void*)&CallBackLoadHistogram;
    rocprofvis_db_sqlite_callback_parameters* callback_params = (rocprofvis_db_sqlite_callback_parameters*)data;
    ProfileDatabase* db = (ProfileDatabase*)callback_params->db;
    if (callback_params->future->Interrupted()) return SQLITE_ABORT;
    uint32_t track_id = db->Sqlite3ColumnInt(func, stmt, azColName, 1);
    uint32_t bucket_num = db->Sqlite3ColumnInt(func, stmt, azColName, 2);
    uint32_t events_count = db->Sqlite3ColumnInt(func, stmt, azColName, 3);
    db->TrackPropertiesAt(track_id)->histogram[bucket_num] = events_count;
    callback_params->future->CountThisRow();
    return 0;

}
rocprofvis_dm_result_t ProfileDatabase::BuildHistogram(Future* future, uint32_t desired_bins) {

    SQLInsertParams params[] = { 
        { "id", "INTEGER PRIMARY KEY" },
        { "track_number", "INTEGER" },
        { "bucket_number", "INTEGER" },
        { "events_count", "INTEGER" }
    };

    typedef struct store_params {
        uint32_t id;
        uint32_t track_id;
        uint32_t bucket_num;
        uint32_t events_count;
    } store_params;

    rocprofvis_dm_result_t result = kRocProfVisDmResultSuccess;

    uint64_t trace_length =
        TraceProperties()->end_time - TraceProperties()->start_time;

    uint64_t bucket_size = (trace_length + desired_bins) / desired_bins;

    TraceProperties()->histogram_bucket_size = bucket_size;
    TraceProperties()->histogram_bucket_count = (trace_length + bucket_size) / bucket_size;

    std::string histogram_query = "SELECT (startTs - " +
        std::to_string(TraceProperties()->start_time) + ") / " +
        std::to_string(bucket_size) + " AS bucket, COUNT(*) AS count, 1 as version, ";

    std::size_t hitogram_query_hash_value = std::hash<std::string>{}(histogram_query);
    std::string histogram_table_name = std::string("histogram_") + std::to_string(hitogram_query_hash_value);
    if (CheckTableExists(histogram_table_name))
    {
        result = ExecuteSQLQuery(future, (std::string("SELECT * FROM ")+histogram_table_name).c_str(), &CallBackLoadHistogram);
    } else
    {

        while (true)
        {
            std::string name;
            if (kRocProfVisDmResultSuccess != ExecuteSQLQuery(future, "SELECT name FROM sqlite_master WHERE type = 'table' AND name LIKE 'histogram_%'", &CallbackGetValue, &name)) break;
            if (name.length() == 0) break;
            DropSQLTable(name.c_str());
        }

        result = ExecuteQueryForAllTracksAsync(
            kRocProfVisDmTrySplitTrack | kRocProfVisDmIncludeStreamTracks, kRPVQuerySliceByTrackSliceQuery,
            histogram_query.c_str(),
            "GROUP BY bucket", &CallbackMakeHistogramPerTrack,
            [](rocprofvis_dm_track_params_t* params) {});

        if (kRocProfVisDmResultSuccess == result)
        {
            std::vector<store_params> v;
            uint32_t counter = 0;
            for (int i = 0; i < NumTracks(); i++)
            {
                for (auto& [key, value] : TrackPropertiesAt(i)->histogram)
                {
                    store_params p;
                    p.id = counter++;
                    p.track_id = i;
                    p.bucket_num = key;
                    p.events_count = value;
                    v.push_back(p);
                }
            }

            result = CreateSQLTable(
                (std::string("histogram_")+std::to_string(hitogram_query_hash_value)).c_str(), params, 4,
                v.size(),
                [&](sqlite3_stmt* stmt, int index) {
                    sqlite3_bind_int(stmt, 1, v[index].id);
                    sqlite3_bind_int(stmt, 2, v[index].track_id);
                    sqlite3_bind_int(stmt, 3, v[index].bucket_num);
                    sqlite3_bind_int(stmt, 4, v[index].events_count);
                });
        }
    }
    return result;
}

}  // namespace DataModel
}  // namespace RocProfVis