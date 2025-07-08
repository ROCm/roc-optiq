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

#include "rocprofvis_db_rocpd.h"
#include <sstream>
#include <string.h>

namespace RocProfVis
{
namespace DataModel
{

rocprofvis_dm_result_t RocpdDatabase::FindTrackId(
                                                    const char* node,
                                                    const char* process,
                                                    const char* subprocess,
                                                    rocprofvis_dm_op_t operation,    
                                                    rocprofvis_dm_track_id_t& track_id) {
    if (operation > 0)
    {
        auto it1 = find_track_map.find(std::stol(process));
        if (it1!=find_track_map.end()){
            auto it2 = it1->second.find(std::stol(subprocess));
            if (it2!=it1->second.end()){
                track_id = it2->second;
                return kRocProfVisDmResultSuccess;
            }
        }
        return kRocProfVisDmResultNotLoaded;
    } else
    {
        auto it1 = find_track_pmc_map.find(std::stol(process));
        if (it1!=find_track_pmc_map.end()){
            auto it2 = it1->second.find(subprocess);
            if (it2!=it1->second.end()){
                track_id = it2->second;
                return kRocProfVisDmResultSuccess;
            }
        }
        return kRocProfVisDmResultNotLoaded;
    }
}

rocprofvis_dm_result_t RocpdDatabase::RemapStringIds(rocprofvis_db_record_data_t & record)
{
    if (!RemapStringId(record.event.category)) return kRocProfVisDmResultNotLoaded;
    if (!RemapStringId(record.event.symbol)) return kRocProfVisDmResultNotLoaded;
    return kRocProfVisDmResultSuccess;
}

const bool RocpdDatabase::RemapStringId(uint64_t & id)
{
    string_index_map_t::const_iterator pos = m_string_map.find(id);
    if (pos == m_string_map.end()) return false;
    id = pos->second;
    return true;
}

rocprofvis_dm_size_t RocpdDatabase::GetMemoryFootprint()
{
    rocprofvis_dm_size_t size = sizeof(RocpdDatabase)+Database::GetMemoryFootprint();
    size+=m_string_map.size()* (sizeof(uint64_t) + sizeof(uint32_t));
    size+=m_string_map.size() * 3 * sizeof(void*);
    return size;
}


int RocpdDatabase::CallBackAddTrack(void *data, int argc, sqlite3_stmt* stmt, char **azColName){
    ROCPROFVIS_ASSERT_MSG_RETURN(argc== NUMBER_OF_TRACK_IDENTIFICATION_PARAMETERS + 1, ERROR_DATABASE_QUERY_PARAMETERS_MISMATCH, 1);
    ROCPROFVIS_ASSERT_MSG_RETURN(data, ERROR_SQL_QUERY_PARAMETERS_CANNOT_BE_NULL, 1);
    rocprofvis_dm_track_params_t track_params = {0};
    rocprofvis_db_sqlite_callback_parameters* callback_params = (rocprofvis_db_sqlite_callback_parameters*)data;
    RocpdDatabase* db = (RocpdDatabase*)callback_params->db;
    if (callback_params->future->Interrupted()) return 1;
    track_params.track_id = (rocprofvis_dm_track_id_t)db->NumTracks();
    track_params.track_category = (rocprofvis_dm_track_category_t)sqlite3_column_int(stmt, TRACK_ID_CATEGORY);
    for (int i = 0; i < NUMBER_OF_TRACK_IDENTIFICATION_PARAMETERS; i++) {
        track_params.process_tag[i] = azColName[i];
        char* arg = (char*) sqlite3_column_text(stmt, i);
        track_params.process_id_numeric[i] = Database::IsNumber(arg);
        if(track_params.process_id_numeric[i])
        {
            track_params.process_id[i] = sqlite3_column_int(stmt, i);
        }
        else
        {
            track_params.process_name[i] = arg;
        }   
    }
    if(!db->TrackExist(track_params, callback_params->subquery,
                       callback_params->table_subquery))
    {

        track_params.process_name[TRACK_ID_PID_OR_AGENT] = ProcessNameSuffixFor(track_params.track_category);
        track_params.process_name[TRACK_ID_PID_OR_AGENT] += (char*)sqlite3_column_text(stmt, TRACK_ID_PID_OR_AGENT);

        if (track_params.track_category != kRocProfVisDmPmcTrack){
            track_params.process_name[TRACK_ID_TID_OR_QUEUE] = SubProcessNameSuffixFor(track_params.track_category);
            track_params.process_name[TRACK_ID_TID_OR_QUEUE] += (char*)sqlite3_column_text(stmt, TRACK_ID_TID_OR_QUEUE);
            db->find_track_map[track_params.process_id[TRACK_ID_PID_OR_AGENT]][track_params.process_id[TRACK_ID_TID_OR_QUEUE]] = track_params.track_id;
        } else
        {
            track_params.process_name[TRACK_ID_TID_OR_QUEUE] = (char*)sqlite3_column_text(stmt, TRACK_ID_TID_OR_QUEUE);
            db->find_track_pmc_map[track_params.process_id[TRACK_ID_PID_OR_AGENT]][track_params.process_name[TRACK_ID_TID_OR_QUEUE]] = track_params.track_id;
        }
        db->OpenConnection((sqlite3**) &track_params.db_connection);
        if (kRocProfVisDmResultSuccess != db->AddTrackProperties(track_params)) return 1;
        if (db->BindObject()->FuncAddTrack(db->BindObject()->trace_object, db->TrackPropertiesLast()) != kRocProfVisDmResultSuccess) return 1;  
        if (track_params.track_category == kRocProfVisDmRegionTrack) {
            db->CachedTables()->AddTableCell("Process", track_params.process_id[TRACK_ID_PID], azColName[TRACK_ID_PID], (char*)sqlite3_column_text(stmt, TRACK_ID_PID));
            db->CachedTables()->AddTableCell("Thread", track_params.process_id[TRACK_ID_TID], azColName[TRACK_ID_TID], (char*)sqlite3_column_text(stmt, TRACK_ID_TID));
            if (db->CachedTables()->PopulateTrackExtendedDataTemplate(db, "Process", track_params.process_id[TRACK_ID_PID]) != kRocProfVisDmResultSuccess) return 1;
            if (db->CachedTables()->PopulateTrackExtendedDataTemplate(db, "Thread", track_params.process_id[TRACK_ID_TID]) != kRocProfVisDmResultSuccess) return 1;
        } else
        if (track_params.track_category == kRocProfVisDmKernelTrack || track_params.track_category == kRocProfVisDmPmcTrack)
        {
            db->CachedTables()->AddTableCell("Agent", track_params.process_id[TRACK_ID_AGENT], azColName[TRACK_ID_AGENT], (char*)sqlite3_column_text(stmt, TRACK_ID_AGENT));
            db->CachedTables()->AddTableCell("Queue", track_params.process_id[TRACK_ID_QUEUE], azColName[TRACK_ID_QUEUE], (char*)sqlite3_column_text(stmt, TRACK_ID_QUEUE));
            if (db->CachedTables()->PopulateTrackExtendedDataTemplate(db, "Agent", track_params.process_id[TRACK_ID_AGENT]) != kRocProfVisDmResultSuccess) return 1;
            if (db->CachedTables()->PopulateTrackExtendedDataTemplate(db, "Queue", track_params.process_id[TRACK_ID_QUEUE]) != kRocProfVisDmResultSuccess) return 1;
        }
    }
    callback_params->future->CountThisRow();
    return 0;
}

int RocpdDatabase::CallBackAddString(void *data, int argc, sqlite3_stmt* stmt, char **azColName){
    ROCPROFVIS_ASSERT_MSG_RETURN(argc==2, ERROR_DATABASE_QUERY_PARAMETERS_MISMATCH, 1);
    ROCPROFVIS_ASSERT_MSG_RETURN(data, ERROR_SQL_QUERY_PARAMETERS_CANNOT_BE_NULL, 1);
    rocprofvis_db_sqlite_callback_parameters* callback_params = (rocprofvis_db_sqlite_callback_parameters*)data;
    RocpdDatabase* db = (RocpdDatabase*)callback_params->db;
    if (callback_params->future->Interrupted()) return 1;
    std::stringstream ids((char*)sqlite3_column_text(stmt, 1));
    uint32_t string_id = db->BindObject()->FuncAddString(db->BindObject()->trace_object, (char*)sqlite3_column_text(stmt, 0));
    if (string_id == INVALID_INDEX) return 1;
    
    for (uint64_t id; ids >> id;) {   
        db->m_string_map[id] = string_id;
        if (ids.peek() == ',') ids.ignore();
    }
    callback_params->future->CountThisRow();
    return 0;
}


int RocpdDatabase::CallbackAddStackTrace(void *data, int argc, sqlite3_stmt* stmt, char **azColName){
    ROCPROFVIS_ASSERT_MSG_RETURN(data, ERROR_SQL_QUERY_PARAMETERS_CANNOT_BE_NULL, 1);
    ROCPROFVIS_ASSERT_MSG_RETURN(argc==4, ERROR_DATABASE_QUERY_PARAMETERS_MISMATCH, 1);
    rocprofvis_db_sqlite_callback_parameters* callback_params = (rocprofvis_db_sqlite_callback_parameters*)data;
    RocpdDatabase* db = (RocpdDatabase*)callback_params->db;
    rocprofvis_db_stack_data_t record;
    if (callback_params->future->Interrupted()) return 1;
    record.symbol = (char*)sqlite3_column_text(stmt, 0);
    record.args = (char*)sqlite3_column_text(stmt, 1);
    record.line = (char*)sqlite3_column_text(stmt, 2);
    record.depth = sqlite3_column_int(stmt, 3);
    if (db->BindObject()->FuncAddStackFrame(callback_params->handle,record) != kRocProfVisDmResultSuccess) return 1;
    callback_params->future->CountThisRow();
    return 0;
}       

rocprofvis_dm_result_t  RocpdDatabase::ReadTraceMetadata(Future* future)
{
    ROCPROFVIS_ASSERT_MSG_RETURN(future, ERROR_FUTURE_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
    while (true)
    {
        ROCPROFVIS_ASSERT_MSG_BREAK(BindObject()->trace_properties, ERROR_TRACE_PROPERTIES_CANNOT_BE_NULL);

        ShowProgress(10, "Create CPU tracks indexes", kRPVDbBusy, future);
        ExecuteSQLQuery(future,"CREATE INDEX pid_tid_idx ON rocpd_api(pid,tid);");

        ShowProgress(10, "Create GPU tracks indexes", kRPVDbBusy, future );
        ExecuteSQLQuery(future,"CREATE INDEX gid_qid_idx ON rocpd_op(gpuId, queueId);");
        
        ShowProgress(10, "Create PMC tracks indexes", kRPVDbBusy, future );
        ExecuteSQLQuery(future,"CREATE INDEX monitorTypeIdx on rocpd_monitor(monitorType);");

        ShowProgress(5, "Adding CPU tracks", kRPVDbBusy, future );
        if (kRocProfVisDmResultSuccess != ExecuteSQLQuery(future, 
                        "select DISTINCT 0 as const, pid, tid, 2 as category from rocpd_api;", 
                        "select 1 as op, start, end, apiName_id, args_id, id, 0, pid, tid {,L.level} from rocpd_api {INNER JOIN event_levels_api L ON id = L.eid} ",
                        "select id, apiName, args, start, end, pid, tid from api ",
                        &CallBackAddTrack)) break;

        ShowProgress(5, "Adding GPU tracks", kRPVDbBusy, future );
        if (kRocProfVisDmResultSuccess != ExecuteSQLQuery(future, 
                        "select DISTINCT 0 as const, gpuId, queueId, 3 as category from rocpd_op;",
                        "select 2 as op, start, end, opType_id, description_id, id, 0, gpuId, queueId  {,L.level} from rocpd_op {INNER JOIN event_levels_op L ON id = L.eid} ",
                        "select id, opType, description, start, end,  gpuId, queueId  from op ",
                        &CallBackAddTrack)) break;

        ShowProgress(5, "Adding PMC tracks", kRPVDbBusy, future );
        if (kRocProfVisDmResultSuccess != ExecuteSQLQuery(future, 
                        "select DISTINCT 0 as const, deviceId, monitorType, 1 as category from rocpd_monitor where deviceId > 0;", 
                        "select 0 as op, start, value, start as end, 0, 0, 0, deviceId, monitorType {,0} from rocpd_monitor ",
                        "select id, monitorType, CAST(value AS REAL) as value, start, start as end, deviceId  from rocpd_monitor ",
                        &CallBackAddTrack)) break;

        ShowProgress(20, "Loading strings", kRPVDbBusy, future );
        if (kRocProfVisDmResultSuccess != ExecuteSQLQuery(future, "SELECT string, GROUP_CONCAT(id) AS ids FROM rocpd_string GROUP BY string;", &CallBackAddString)) break;

        ShowProgress(5, "Collect track items count, minimum and maximum timestamps", kRPVDbBusy, future);
        TraceProperties()->start_time = UINT64_MAX;
        TraceProperties()->end_time   = 0;
        TraceProperties()->events_count[kRocProfVisDmOperationLaunch] = 0;
        TraceProperties()->events_count[kRocProfVisDmOperationDispatch] = 0;

        if(kRocProfVisDmResultSuccess !=
           ExecuteQueryForAllTracksAsync(true,
               "SELECT COUNT(*), MIN(start), MAX(end), op, ", ";", &CallbackGetTrackProperties,
               [](rocprofvis_dm_track_params_t* params) {
               }))
        {
            break;
        }

        if(SQLITE_OK != DetectTable(GetServiceConnection(), "event_levels_api", false) ||
           SQLITE_OK != DetectTable(GetServiceConnection(), "event_levels_op", false))
        {
            m_event_levels[kRocProfVisDmOperationLaunch].reserve(
                TraceProperties()->events_count[kRocProfVisDmOperationLaunch]);
            m_event_levels[kRocProfVisDmOperationDispatch].reserve(
                TraceProperties()->events_count[kRocProfVisDmOperationDispatch]);

            ShowProgress(10, "Calculate event levels", kRPVDbBusy, future);
            if(kRocProfVisDmResultSuccess !=
               ExecuteQueryForAllTracksAsync(
                   false, "SELECT *, ", " ORDER BY start;", &CalculateEventLevels,
                   [](rocprofvis_dm_track_params_t* params) {
                       params->m_event_timing_params.clear();
                   }))
            {
                break;
            }

            ShowProgress(10, "Save event levels", kRPVDbBusy, future);
            SQLInsertParams params[] = { { "eid", "INTEGER PRIMARY KEY" },
                                         { "level", "INTEGER" } };
            CreateSQLTable(
                "event_levels_api", params, 2,
                m_event_levels[kRocProfVisDmOperationLaunch].size(),
                [&](sqlite3_stmt* stmt, int index) {
                    sqlite3_bind_int64(
                        stmt, 1, m_event_levels[kRocProfVisDmOperationLaunch][index].id);
                    sqlite3_bind_int(
                        stmt, 2,
                        m_event_levels[kRocProfVisDmOperationLaunch][index].level);
                });
            CreateSQLTable(
                "event_levels_op", params, 2,
                m_event_levels[kRocProfVisDmOperationDispatch].size(),
                [&](sqlite3_stmt* stmt, int index) {
                    sqlite3_bind_int64(
                        stmt, 1,
                        m_event_levels[kRocProfVisDmOperationDispatch][index].id);
                    sqlite3_bind_int(
                        stmt, 2,
                        m_event_levels[kRocProfVisDmOperationDispatch][index].level);
                });
        }
      
        TraceProperties()->metadata_loaded=true;
        ShowProgress(100-future->Progress(), "Trace metadata successfully loaded", kRPVDbSuccess, future );
        return future->SetPromise(kRocProfVisDmResultSuccess);

    }
    ShowProgress(0, "Trace metadata not loaded!", kRPVDbError, future );
    return future->SetPromise(future->Interrupted() ? kRocProfVisDmResultTimeout : kRocProfVisDmResultDbAccessFailed);
}

rocprofvis_dm_result_t  RocpdDatabase::ReadFlowTraceInfo(
        rocprofvis_dm_event_id_t event_id,
        Future* future)
{
    ROCPROFVIS_ASSERT_MSG_RETURN(future, ERROR_FUTURE_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
    while (true)
    {
        ROCPROFVIS_ASSERT_MSG_BREAK(BindObject()->trace_properties, ERROR_TRACE_PROPERTIES_CANNOT_BE_NULL);
        ROCPROFVIS_ASSERT_MSG_BREAK(BindObject()->trace_properties->metadata_loaded, ERROR_METADATA_IS_NOT_LOADED);
        rocprofvis_dm_flowtrace_t flowtrace = BindObject()->FuncAddFlowTrace(BindObject()->trace_object, event_id);
        ROCPROFVIS_ASSERT_MSG_BREAK(flowtrace, ERROR_FLOW_TRACE_CANNOT_BE_NULL);
        std::stringstream query;
        if (event_id.bitfield.event_op == kRocProfVisDmOperationLaunch)
        {
            query << "select rocpd_api_ops.api_id, 2, rocpd_api_ops.op_id, 0, gpuId, queueId, rocpd_op.start from rocpd_api_ops "
                "INNER JOIN rocpd_api on rocpd_api_ops.api_id = rocpd_api.id "
                "INNER JOIN rocpd_op on rocpd_api_ops.op_id = rocpd_op.id where rocpd_api_ops.api_id = ";
            query << event_id.bitfield.event_id;  
            ShowProgress(0, query.str().c_str(),kRPVDbBusy, future);
            if (kRocProfVisDmResultSuccess != ExecuteSQLQuery(future, query.str().c_str(), flowtrace, &CallbackAddFlowTrace)) break;
        } else
        if (event_id.bitfield.event_op == kRocProfVisDmOperationDispatch)
        {
            query << "select rocpd_api_ops.op_id, 1, rocpd_api_ops.api_id, 0, pid, tid, rocpd_api.end from rocpd_api_ops "
                "INNER JOIN rocpd_api on rocpd_api_ops.api_id = rocpd_api.id "
                "INNER JOIN rocpd_op on rocpd_api_ops.op_id = rocpd_op.id where rocpd_api_ops.op_id = ";
            query << event_id.bitfield.event_id << ";";  
            ShowProgress(0, query.str().c_str(),kRPVDbBusy, future);
            if (kRocProfVisDmResultSuccess != ExecuteSQLQuery(future, query.str().c_str(), flowtrace, &CallbackAddFlowTrace)) break;
        } else
        {
            ShowProgress(0, "Flow trace is not available for specified operation type!", kRPVDbError, future );
            return future->SetPromise(kRocProfVisDmResultInvalidParameter);
        }
        ShowProgress(100, "Flow trace successfully loaded!",kRPVDbSuccess, future);
        return future->SetPromise(kRocProfVisDmResultSuccess);
    }
    ShowProgress(0, "Flow trace not loaded!", kRPVDbError, future );
    return future->SetPromise(future->Interrupted() ? kRocProfVisDmResultTimeout : kRocProfVisDmResultDbAccessFailed);
}

rocprofvis_dm_result_t  RocpdDatabase::ReadStackTraceInfo(
        rocprofvis_dm_event_id_t event_id,
        Future* future)
{
    ROCPROFVIS_ASSERT_MSG_RETURN(future, ERROR_FUTURE_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
    while (true)
    {
        ROCPROFVIS_ASSERT_MSG_BREAK(BindObject()->trace_properties, ERROR_TRACE_PROPERTIES_CANNOT_BE_NULL);
        ROCPROFVIS_ASSERT_MSG_BREAK(BindObject()->trace_properties->metadata_loaded, ERROR_METADATA_IS_NOT_LOADED);
        rocprofvis_dm_stacktrace_t stacktrace = BindObject()->FuncAddStackTrace(BindObject()->trace_object, event_id);
        ROCPROFVIS_ASSERT_MSG_BREAK(stacktrace, ERROR_STACK_TRACE_CANNOT_BE_NULL);
        std::stringstream query;
        if (event_id.bitfield.event_op == kRocProfVisDmOperationLaunch || event_id.bitfield.event_op == kRocProfVisDmOperationMemoryAllocate)
        {
            query << "select s2.string as hip_api, s3.string as args, s1.string as frame, sf.depth "
                     "from rocpd_stackframe sf join rocpd_string s1 on sf.name_id=s1.id join rocpd_api ap on "
                     "sf.api_ptr_id=ap.id join rocpd_string s2 on ap.apiName_id=s2.id join rocpd_string s3 on ap.args_id=s3.id where ap.id  == ";
            query << event_id.bitfield.event_id << ";";  
            ShowProgress(0, query.str().c_str(),kRPVDbBusy, future);
            if (kRocProfVisDmResultSuccess != ExecuteSQLQuery(future, query.str().c_str(), stacktrace, &CallbackAddStackTrace)) break;
        } else
        {
            ShowProgress(0, "Stack trace is not available for specified operation type!", kRPVDbError, future );
            return future->SetPromise(kRocProfVisDmResultInvalidParameter);
        }
        ShowProgress(100, "Stack trace successfully loaded!",kRPVDbSuccess, future);
        return future->SetPromise(kRocProfVisDmResultSuccess);
    }
    ShowProgress(0, "Stack trace not loaded!", kRPVDbError, future );
    return future->SetPromise(future->Interrupted() ? kRocProfVisDmResultTimeout : kRocProfVisDmResultDbAccessFailed);
}


rocprofvis_dm_result_t  RocpdDatabase::ReadExtEventInfo(
            rocprofvis_dm_event_id_t event_id, 
            Future* future){
    ROCPROFVIS_ASSERT_MSG_RETURN(future, ERROR_FUTURE_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
    while (true)
    {
        ROCPROFVIS_ASSERT_MSG_BREAK(BindObject()->trace_properties, ERROR_TRACE_PROPERTIES_CANNOT_BE_NULL);
        ROCPROFVIS_ASSERT_MSG_BREAK(BindObject()->trace_properties->metadata_loaded, ERROR_METADATA_IS_NOT_LOADED);
        rocprofvis_dm_extdata_t extdata = BindObject()->FuncAddExtData(BindObject()->trace_object, event_id);
        ROCPROFVIS_ASSERT_MSG_BREAK(extdata, ERROR_EXT_DATA_CANNOT_BE_NULL);
        std::stringstream query;
        if (event_id.bitfield.event_op == kRocProfVisDmOperationLaunch)
        {
            query << "select * from api where id == ";
            query << event_id.bitfield.event_id << ";";  
            ShowProgress(0, query.str().c_str(),kRPVDbBusy, future);
            if (kRocProfVisDmResultSuccess != ExecuteSQLQuery(future, query.str().c_str(), "Properties", extdata, &CallbackAddExtInfo)) break;
            query.str("");
            query << "select * from copy where id == ";
            query << event_id.bitfield.event_id << ";";  
            ShowProgress(50, query.str().c_str(),kRPVDbBusy, future);
            if (kRocProfVisDmResultSuccess != ExecuteSQLQuery(future, query.str().c_str(), "Copy", extdata, &CallbackAddExtInfo)) break;
        } else
        if (event_id.bitfield.event_op == kRocProfVisDmOperationDispatch)
        {
            query << "select * from op where id == ";
            query << event_id.bitfield.event_id << ";";  
            ShowProgress(0, query.str().c_str(),kRPVDbBusy, future);
            if (kRocProfVisDmResultSuccess != ExecuteSQLQuery(future, query.str().c_str(), "Properties", extdata, &CallbackAddExtInfo)) break;
            query.str("");
            query << "select * from copyop where id == ";
            query << event_id.bitfield.event_id << ";";  
            ShowProgress(25, query.str().c_str(),kRPVDbBusy, future);
            if (kRocProfVisDmResultSuccess != ExecuteSQLQuery(future, query.str().c_str(), "Copy", extdata, &CallbackAddExtInfo)) break;
            query.str("");
            query << "select * from kernel where id == ";
            query << event_id.bitfield.event_id << ";";  
            ShowProgress(50, query.str().c_str(),kRPVDbBusy, future);
            if (kRocProfVisDmResultSuccess != ExecuteSQLQuery(future, query.str().c_str(), "Dispatch", extdata, &CallbackAddExtInfo)) break;
        } else    
        {
            ShowProgress(0, "Extended data not available for specified operation type!", kRPVDbError, future );
            return future->SetPromise(kRocProfVisDmResultInvalidParameter);
        }
        ShowProgress(50, "Extended data successfully loaded!",kRPVDbSuccess, future);
        return future->SetPromise(kRocProfVisDmResultSuccess);
    }
    ShowProgress(0, "Extended data  not loaded!", kRPVDbError, future );
    return future->SetPromise(future->Interrupted() ? kRocProfVisDmResultTimeout : kRocProfVisDmResultDbAccessFailed);    
}

}  // namespace DataModel
}  // namespace RocProfVis