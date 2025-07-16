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

#include "rocprofvis_db_rocprof.h"
#include <sstream>
#include <string.h>

namespace RocProfVis
{
namespace DataModel
{

rocprofvis_dm_result_t RocprofDatabase::FindTrackId(
                                                    const char* node,
                                                    const char* process,
                                                    const char* subprocess,
                                                    rocprofvis_dm_op_t operation,
                                                    rocprofvis_dm_track_id_t& track_id) {
    

    auto it_node = find_track_map.find(std::stoll(node));
    if (it_node!=find_track_map.end()){
        auto it_process = it_node->second.find(std::stol(process));
        if (it_process!=it_node->second.end()){
            auto it_subprocess = it_process->second.find(std::stol(subprocess));
            if (it_subprocess!=it_process->second.end()){
                track_id = it_subprocess->second;
                return kRocProfVisDmResultSuccess;
            }
        }
    }
    return kRocProfVisDmResultNotLoaded;
   
}

rocprofvis_dm_result_t RocprofDatabase::RemapStringIds(rocprofvis_db_record_data_t & record)
{
    if(record.event.id.bitfield.event_op==kRocProfVisDmOperationDispatch)
        record.event.symbol+=m_symbols_offset;
    return kRocProfVisDmResultSuccess;
}


int RocprofDatabase::CallBackAddTrack(void *data, int argc, sqlite3_stmt* stmt, char **azColName){
    ROCPROFVIS_ASSERT_MSG_RETURN(argc==NUMBER_OF_TRACK_IDENTIFICATION_PARAMETERS+1, ERROR_DATABASE_QUERY_PARAMETERS_MISMATCH, 1);
    ROCPROFVIS_ASSERT_MSG_RETURN(data, ERROR_SQL_QUERY_PARAMETERS_CANNOT_BE_NULL, 1);
    rocprofvis_dm_track_params_t track_params = {0};
    rocprofvis_db_sqlite_callback_parameters* callback_params = (rocprofvis_db_sqlite_callback_parameters*)data;
    RocprofDatabase* db = (RocprofDatabase*)callback_params->db;
    if (callback_params->future->Interrupted()) return 1;
    track_params.track_id = (rocprofvis_dm_track_id_t)db->NumTracks();
    track_params.track_category = (rocprofvis_dm_track_category_t)sqlite3_column_int(stmt,TRACK_ID_CATEGORY);
    for (int i = 0; i < NUMBER_OF_TRACK_IDENTIFICATION_PARAMETERS; i++) {
        track_params.process_tag[i] = azColName[i];
        char* arg = (char*) sqlite3_column_text(stmt, i);
        track_params.process_id_numeric[i] = Database::IsNumber(arg);
        if (track_params.process_id_numeric[i]) {
            track_params.process_id[i] = sqlite3_column_int64(stmt, i);
        } else {
            track_params.process_name[i] = arg;
        }        
    }
    
    if(!db->TrackExist(track_params, callback_params->query))
    {
        db->find_track_map[track_params.process_id[TRACK_ID_NODE]][track_params.process_id[TRACK_ID_PID_OR_AGENT]][track_params.process_id[TRACK_ID_TID_OR_QUEUE]] = track_params.track_id;
        if (track_params.track_category == kRocProfVisDmRegionTrack) {
            track_params.process_name[TRACK_ID_PID] = ProcessNameSuffixFor(track_params.track_category);
            track_params.process_name[TRACK_ID_PID] += (char*) sqlite3_column_text(stmt,TRACK_ID_PID);
            track_params.process_name[TRACK_ID_TID] = SubProcessNameSuffixFor(track_params.track_category);
            track_params.process_name[TRACK_ID_TID] += (char*) sqlite3_column_text(stmt,TRACK_ID_TID);

        } else
            if (track_params.track_category == kRocProfVisDmKernelTrack || track_params.track_category == kRocProfVisDmPmcTrack) {
                track_params.process_name[TRACK_ID_AGENT] = db->CachedTables()->GetTableCell("Agent", track_params.process_id[TRACK_ID_AGENT], "type");
                track_params.process_name[TRACK_ID_AGENT] += db->CachedTables()->GetTableCell("Agent", track_params.process_id[TRACK_ID_AGENT], "type_index");
                if (track_params.track_category == kRocProfVisDmKernelTrack) {
                    track_params.process_name[TRACK_ID_QUEUE] = db->CachedTables()->GetTableCell("Queue", track_params.process_id[TRACK_ID_QUEUE], "name");
                }
                else {
                    track_params.process_name[TRACK_ID_QUEUE] = db->CachedTables()->GetTableCell("PMC", track_params.process_id[TRACK_ID_QUEUE], "name");
                }
                
            }
        if (kRocProfVisDmResultSuccess != db->AddTrackProperties(track_params)) return 1;
        if (db->BindObject()->FuncAddTrack(db->BindObject()->trace_object, db->TrackPropertiesLast()) != kRocProfVisDmResultSuccess) return 1;  
        if (db->CachedTables()->PopulateTrackExtendedDataTemplate(db, "Node", track_params.process_id[TRACK_ID_NODE]) != kRocProfVisDmResultSuccess) return 1;
        if (track_params.track_category == kRocProfVisDmRegionTrack){
            if (db->CachedTables()->PopulateTrackExtendedDataTemplate(db, "Process", track_params.process_id[TRACK_ID_PID]) != kRocProfVisDmResultSuccess) return 1;
            if (db->CachedTables()->PopulateTrackExtendedDataTemplate(db, "Thread", track_params.process_id[TRACK_ID_TID]) != kRocProfVisDmResultSuccess) return 1;
        } else
        if (track_params.track_category == kRocProfVisDmKernelTrack || track_params.track_category == kRocProfVisDmPmcTrack)
        {
            if (db->CachedTables()->PopulateTrackExtendedDataTemplate(db, "Agent", track_params.process_id[TRACK_ID_AGENT]) != kRocProfVisDmResultSuccess) return 1; 
            if(track_params.track_category == kRocProfVisDmKernelTrack)
            {
                if (db->CachedTables()->PopulateTrackExtendedDataTemplate(db, "Queue", track_params.process_id[TRACK_ID_QUEUE]) != kRocProfVisDmResultSuccess) return 1;
            }
            else
            {
                if(track_params.process_id[TRACK_ID_QUEUE] > 0)
                    if (db->CachedTables()->PopulateTrackExtendedDataTemplate(db, "PMC", track_params.process_id[TRACK_ID_QUEUE]) != kRocProfVisDmResultSuccess) return 1;
            }
        }
    }
    callback_params->future->CountThisRow();
    return 0;
}

int RocprofDatabase::CallBackAddString(void *data, int argc, sqlite3_stmt* stmt, char **azColName){
    ROCPROFVIS_ASSERT_MSG_RETURN(argc==1, ERROR_DATABASE_QUERY_PARAMETERS_MISMATCH, 1);
    ROCPROFVIS_ASSERT_MSG_RETURN(data, ERROR_SQL_QUERY_PARAMETERS_CANNOT_BE_NULL, 1);
    rocprofvis_db_sqlite_callback_parameters* callback_params = (rocprofvis_db_sqlite_callback_parameters*)data;
    RocprofDatabase* db = (RocprofDatabase*)callback_params->db;
    if (callback_params->future->Interrupted()) return 1;
    uint32_t stringId = db->BindObject()->FuncAddString(db->BindObject()->trace_object, (char*) sqlite3_column_text(stmt,0));
    callback_params->future->CountThisRow();
    return 0;
}


int RocprofDatabase::CallbackCacheTable(void *data, int argc, sqlite3_stmt* stmt, char **azColName){
    ROCPROFVIS_ASSERT_MSG_RETURN(data, ERROR_SQL_QUERY_PARAMETERS_CANNOT_BE_NULL, 1);
    rocprofvis_db_sqlite_callback_parameters* callback_params = (rocprofvis_db_sqlite_callback_parameters*)data;
    RocprofDatabase* db = (RocprofDatabase*)callback_params->db;
    DatabaseCache * ref_tables = (DatabaseCache *)callback_params->handle;
    uint64_t id = sqlite3_column_int64(stmt,0);
    for (int i=0; i < argc; i++)
    {
        ref_tables->AddTableCell(callback_params->query[kRPVSqliteCacheTableName], id, azColName[i],
                                 (char*) sqlite3_column_text(stmt, i));
    }
    callback_params->future->CountThisRow();
    return 0;
}

rocprofvis_dm_result_t  RocprofDatabase::ReadTraceMetadata(Future* future)
{
    ROCPROFVIS_ASSERT_MSG_RETURN(future, ERROR_FUTURE_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
    while (true)
    {
        ROCPROFVIS_ASSERT_MSG_BREAK(BindObject()->trace_properties, ERROR_TRACE_PROPERTIES_CANNOT_BE_NULL);
        std::string value;

        ShowProgress(2, "Load Nodes information", kRPVDbBusy, future);
        ExecuteSQLQuery(future,"SELECT * from rocpd_info_node;", "Node", (rocprofvis_dm_handle_t)CachedTables(), &CallbackCacheTable);
        
        ShowProgress(2, "Load Agents information", kRPVDbBusy, future);
        ExecuteSQLQuery(future,"SELECT * from rocpd_info_agent;", "Agent", (rocprofvis_dm_handle_t)CachedTables(), &CallbackCacheTable);

        ShowProgress(2, "Load Queues information", kRPVDbBusy, future);
        ExecuteSQLQuery(future,"SELECT * from rocpd_info_queue;", "Queue", (rocprofvis_dm_handle_t)CachedTables(), &CallbackCacheTable);

        ShowProgress(2, "Load Streams information", kRPVDbBusy, future);
        ExecuteSQLQuery(future,"SELECT * from rocpd_info_stream;", "Stream", (rocprofvis_dm_handle_t)CachedTables(), &CallbackCacheTable);

        ShowProgress(2, "Load Process information", kRPVDbBusy, future);
        ExecuteSQLQuery(future,"SELECT * from rocpd_info_process;", "Process", (rocprofvis_dm_handle_t)CachedTables(), &CallbackCacheTable);

        ShowProgress(2, "Load Thread information", kRPVDbBusy, future);
        ExecuteSQLQuery(future,"SELECT * from rocpd_info_thread;", "Thread", (rocprofvis_dm_handle_t)CachedTables(), &CallbackCacheTable);

        ShowProgress(2, "Load PMC information", kRPVDbBusy, future);
        ExecuteSQLQuery(future,"SELECT * from rocpd_info_pmc;", "PMC", (rocprofvis_dm_handle_t)CachedTables(), &CallbackCacheTable);

        CachedTables()->AddTableCell("PMC", -1, "name", "MALLOC");
                
        ShowProgress(5, "Adding CPU tracks", kRPVDbBusy, future );
        if (kRocProfVisDmResultSuccess != ExecuteSQLQuery(future, 
                    "select DISTINCT nid as nodeId, pid, tid, 2 as category from rocpd_region;", 
                    "select 1 as op, start, end, id, nid as nodeId, pid, tid from rocpd_region " , 
                    "select 1 as op, R.start, R.end, E.category_id, R.name_id, R.id, R.nid as nodeId, R.pid, R.tid ,L.level as level from rocpd_region R INNER JOIN rocpd_event E ON E.id = R.event_id INNER JOIN event_levels_launch L ON R.id = L.eid " , 
                    "select id, category, name, nid as nodeId, pid, tid, start, end, duration from regions " ,
                    &CallBackAddTrack)) break;

        ShowProgress(5, "Adding Kernel Dispatch tracks", kRPVDbBusy, future );
        if (kRocProfVisDmResultSuccess != ExecuteSQLQuery(future, 
                    "select DISTINCT nid as nodeId, agent_id as agentId, queue_id as queueId, 3 as category from rocpd_kernel_dispatch;", 
                    "select 2 as op,start, end, id, nid as nodeId, agent_id as agentId, queue_id as queueId from rocpd_kernel_dispatch ",
                    "select 2 as op, KD.start, KD.end, E.category_id, KD.kernel_id, KD.id, KD.nid as nodeId, KD.agent_id as agentId, KD.queue_id as queueId ,L.level as level from rocpd_kernel_dispatch KD INNER JOIN rocpd_event E ON E.id = KD.event_id INNER JOIN event_levels_dispatch L ON KD.id = L.eid ", 
                    "select id, category, name, nid as nodeId, agent_abs_index as agentId, queue_id as queueId, stream_id as stream, pid, tid, start, end, duration from kernels " ,
                    &CallBackAddTrack)) break;

        ShowProgress(5, "Adding Memory Allocation tracks", kRPVDbBusy, future );
        if (kRocProfVisDmResultSuccess != ExecuteSQLQuery(future, 
                    "select DISTINCT nid as nodeId, agent_id as agentId, queue_id as queueId, 3 as category from rocpd_memory_allocate;", 
                    "select 3 as op, start, end, id, nid as nodeId, agent_id as agentId, queue_id as queueId from rocpd_memory_allocate ",
                    "select 3 as op, MA.start, MA.end, E.category_id, 0, MA.id, MA.nid as nodeId, MA.agent_id as agentId, MA.queue_id as queueId ,L.level as level from rocpd_memory_allocate MA INNER JOIN rocpd_event E ON E.id = MA.event_id INNER JOIN event_levels_mem_alloc L ON MA.id = L.eid ", 
                    "select id, category, type as name, nid as nodeId, agent_abs_index as agentId, queue_id as queueId, stream_id as stream, pid, tid, start, end, duration from memory_allocations " ,
                    &CallBackAddTrack)) break;
        /*
// This will not work if full track is not requested
// Comment out for now. Will need to fetch all data, then cut samples outside of time frame.
        ShowProgress(5, "Adding Memory allocation graph tracks", kRPVDbBusy, future );
        if (kRocProfVisDmResultSuccess != ExecuteSQLQuery(future, 
                    "select DISTINCT nid as nodeId, agent_id as agentId, -1 as const, 1 as category from rocpd_memory_allocate;", 
                    "select 0 as op, start, sum(CASE WHEN type = 'FREE' THEN (select -size from rocpd_memory_allocate MA1 where MA.address == MA1.address limit 1) ELSE size END) over (ORDER BY start ROWS BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW) as current, start as end, 0, 0, nid as nodeId, coalesce(agent_id, 0) as agentId, -1 as queue  from rocpd_memory_allocate MA ", 
                    &CallBackAddTrack)) break;
*/
   
        ShowProgress(5, "Adding Memory Copy tracks", kRPVDbBusy, future );
        if (kRocProfVisDmResultSuccess != ExecuteSQLQuery(future, 
                    "select DISTINCT nid as nodeId, dst_agent_id as agentId, queue_id as queueId, 3 as category from rocpd_memory_copy;", 
                    "select 4 as op, start, end, id, nid as nodeId, dst_agent_id as agentId, queue_id as queueId from rocpd_memory_copy ",
                    "select 4 as op, MC.start, MC.end, E.category_id, MC.name_id, MC.id, MC.nid as nodeId, MC.dst_agent_id as agentId, MC.queue_id as queueId ,L.level as level from rocpd_memory_copy MC INNER JOIN rocpd_event E ON E.id = MC.event_id INNER JOIN event_levels_mem_copy L ON MC.id = L.eid ",
                    "select id, category, name, nid as nodeId, dst_agent_abs_index as agentId, queue_id as queueId, stream_id as stream, pid, tid, start, end, duration from memory_copies " ,
                    &CallBackAddTrack)) break;
        
        // PMC schema is not fully defined yet
        ShowProgress(5, "Adding PMC tracks", kRPVDbBusy, future );
        if (kRocProfVisDmResultSuccess != ExecuteSQLQuery(future,   "select DISTINCT K.nid as nodeId, K.agent_id as agentId, PMC_I.id AS counter_id, 1 as category FROM rocpd_pmc_event PMC_E "
                                                                    "INNER JOIN "
                                                                    "rocpd_info_pmc PMC_I ON PMC_I.id = PMC_E.pmc_id AND PMC_I.guid = PMC_E.guid "
                                                                    "INNER JOIN "
                                                                    "rocpd_kernel_dispatch K ON K.event_id = PMC_E.event_id AND K.guid = PMC_E.guid ",  
                                                                    "select 0 as op, K.start, K.end, 0, 0, K.nid as nodeId, K.agent_id as agentId, PMC_I.id AS counter_id  FROM rocpd_pmc_event PMC_E "
                                                                    "INNER JOIN "
                                                                    "rocpd_info_pmc PMC_I ON PMC_I.id = PMC_E.pmc_id AND  PMC_I.guid = PMC_E.guid "
                                                                    "INNER JOIN "
                                                                    "rocpd_kernel_dispatch K ON K.event_id = PMC_E.event_id AND K.guid = PMC_E.guid ",
                                                                    "select 0 as op, K.start, PMC_E.value AS counter_value, K.end, 0, 0, K.nid as nodeId, K.agent_id as agentId, PMC_I.id AS counter_id , PMC_E.value as level FROM rocpd_pmc_event PMC_E "
                                                                    "INNER JOIN "
                                                                    "rocpd_info_pmc PMC_I ON PMC_I.id = PMC_E.pmc_id AND  PMC_I.guid = PMC_E.guid "
                                                                    "INNER JOIN "
                                                                    "rocpd_kernel_dispatch K ON K.event_id = PMC_E.event_id AND K.guid = PMC_E.guid ",
                                                                    "select 0 as op, K.start, PMC_E.value AS counter_value, K.end, 0, 0, K.nid as nodeId, K.agent_id as agentId, PMC_I.id AS counter_id FROM rocpd_pmc_event PMC_E "
                                                                    "INNER JOIN "
                                                                    "rocpd_info_pmc PMC_I ON PMC_I.id = PMC_E.pmc_id AND  PMC_I.guid = PMC_E.guid "
                                                                    "INNER JOIN "
                                                                    "rocpd_kernel_dispatch K ON K.event_id = PMC_E.event_id AND K.guid = PMC_E.guid ",
                                                                    &CallBackAddTrack)) break;
                                                                    

        ShowProgress(20, "Loading strings", kRPVDbBusy, future );
        BindObject()->FuncAddString(BindObject()->trace_object, ""); // 0 index string
        if (kRocProfVisDmResultSuccess != ExecuteSQLQuery(future, "SELECT string FROM rocpd_string;", &CallBackAddString)) break;
        if (kRocProfVisDmResultSuccess != ExecuteSQLQuery(future,"SELECT COUNT(*) FROM rocpd_string;", &CallbackGetValue, m_symbols_offset)) break;
        if (kRocProfVisDmResultSuccess != ExecuteSQLQuery(future, "SELECT display_name FROM rocpd_info_kernel_symbol;", &CallBackAddString)) break;

        ShowProgress(5, "Collect track items count",
                     kRPVDbBusy, future);
        TraceProperties()->events_count[kRocProfVisDmOperationLaunch]   = 0;
        TraceProperties()->events_count[kRocProfVisDmOperationDispatch] = 0;
        TraceProperties()->events_count[kRocProfVisDmOperationMemoryAllocate] = 0;
        TraceProperties()->events_count[kRocProfVisDmOperationMemoryCopy]     = 0;

        if(kRocProfVisDmResultSuccess !=
           ExecuteQueryForAllTracksAsync(
               true, kRPVQueryLevel,
               "SELECT COUNT(*), op, ",
               "", &CallbackGetTrackRecordsCount,
               [](rocprofvis_dm_track_params_t* params) {}))
        {
            break;
        }

        if(SQLITE_OK != DetectTable(GetServiceConnection(), "event_levels_launch", false) ||
           SQLITE_OK != DetectTable(GetServiceConnection(), "event_levels_dispatch", false) ||
           SQLITE_OK != DetectTable(GetServiceConnection(), "event_levels_mem_alloc", false) ||
           SQLITE_OK != DetectTable(GetServiceConnection(), "event_levels_mem_copy", false))
        {
            m_event_levels[kRocProfVisDmOperationLaunch].reserve(
                TraceProperties()->events_count[kRocProfVisDmOperationLaunch]);
            m_event_levels[kRocProfVisDmOperationDispatch].reserve(
                TraceProperties()->events_count[kRocProfVisDmOperationDispatch]);
            m_event_levels[kRocProfVisDmOperationMemoryAllocate].reserve(
                TraceProperties()->events_count[kRocProfVisDmOperationMemoryAllocate]);
            m_event_levels[kRocProfVisDmOperationMemoryCopy].reserve(
                TraceProperties()->events_count[kRocProfVisDmOperationMemoryCopy]);

             ShowProgress(10, "Calculate event levels", kRPVDbBusy, future);
            if(kRocProfVisDmResultSuccess !=
               ExecuteQueryForAllTracksAsync(
                   false, 
                   kRPVQueryLevel, "SELECT *, ", " ORDER BY start", &CalculateEventLevels,
                   [](rocprofvis_dm_track_params_t* params) {
                       params->m_event_timing_params.clear();
                   }))
            {
                break;
            }

            SQLInsertParams params[] = { { "eid", "INTEGER PRIMARY KEY" }, { "level", "INTEGER" } };
            CreateSQLTable(
                "event_levels_launch", params, 2,
                m_event_levels[kRocProfVisDmOperationLaunch].size(),
                [&](sqlite3_stmt* stmt, int index) {
                    sqlite3_bind_int64(
                        stmt, 1, m_event_levels[kRocProfVisDmOperationLaunch][index].id);
                    sqlite3_bind_int(
                        stmt, 2,
                        m_event_levels[kRocProfVisDmOperationLaunch][index].level);
                });
            CreateSQLTable(
                "event_levels_dispatch", params, 2,
                m_event_levels[kRocProfVisDmOperationDispatch].size(),
                [&](sqlite3_stmt* stmt, int index) {
                    sqlite3_bind_int64(
                        stmt, 1,
                        m_event_levels[kRocProfVisDmOperationDispatch][index].id);
                    sqlite3_bind_int(
                        stmt, 2,
                        m_event_levels[kRocProfVisDmOperationDispatch][index].level);
                });
            CreateSQLTable(
                "event_levels_mem_alloc", params, 2,
                m_event_levels[kRocProfVisDmOperationMemoryAllocate].size(),
                [&](sqlite3_stmt* stmt, int index) {
                    sqlite3_bind_int64(
                        stmt, 1,
                        m_event_levels[kRocProfVisDmOperationMemoryAllocate][index].id);
                    sqlite3_bind_int(
                        stmt, 2,
                        m_event_levels[kRocProfVisDmOperationMemoryAllocate][index]
                            .level);
                });
            CreateSQLTable(
                "event_levels_mem_copy", params, 2,
                m_event_levels[kRocProfVisDmOperationMemoryCopy].size(),
                [&](sqlite3_stmt* stmt, int index) {
                    sqlite3_bind_int64(
                        stmt, 1,
                        m_event_levels[kRocProfVisDmOperationMemoryCopy][index].id);
                    sqlite3_bind_int(
                        stmt, 2,
                        m_event_levels[kRocProfVisDmOperationMemoryCopy][index].level);
                });
        }

        ShowProgress(5, "Collect track minimum and maximum timestamps and level/value",
                     kRPVDbBusy, future);
        TraceProperties()->start_time = UINT64_MAX;
        TraceProperties()->end_time   = 0;

        if(kRocProfVisDmResultSuccess !=
           ExecuteQueryForAllTracksAsync(
               true, kRPVQuerySlice,
               "SELECT MIN(start), MAX(end), MIN(CAST(level as REAL)), MAX(CAST(level as REAL)), ",
               "", &CallbackGetTrackProperties,
               [](rocprofvis_dm_track_params_t* params) {}))
        {
            break;
        }

        TraceProperties()->metadata_loaded=true;
        ShowProgress(100-future->Progress(), "Trace metadata successfully loaded", kRPVDbSuccess, future );
        return future->SetPromise(kRocProfVisDmResultSuccess);

    }
    ShowProgress(0, "Trace metadata not loaded!", kRPVDbError, future );
    return future->SetPromise(future->Interrupted() ? kRocProfVisDmResultTimeout : kRocProfVisDmResultDbAccessFailed);
}



rocprofvis_dm_result_t  RocprofDatabase::ReadFlowTraceInfo(
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
            query << "SELECT * FROM ("
                        "SELECT E.id as id, 2, E.correlation_id, KD.nid, KD.agent_id, KD.queue_id, KD.start "
                        "FROM rocpd_region R "
                        "INNER JOIN rocpd_event E ON R.event_id = E.id "
                        "INNER JOIN rocpd_kernel_dispatch KD ON KD.id = E.correlation_id "
                        "WHERE R.id == ";
            query << event_id.bitfield.event_id;
            query << " UNION "
                        "SELECT E.id as id, 4, E.correlation_id, MC.nid, MC.dst_agent_id, coalesce(MC.queue_id,0), MC.start "
                        "FROM rocpd_region R "
                        "INNER JOIN rocpd_event E ON R.event_id = E.id "
                        "INNER JOIN _rocpd_memory_copy MC ON MC.id = E.correlation_id "
                        "WHERE R.id == ";
            query << event_id.bitfield.event_id;
                        " UNION "
                        "SELECT E.id as id, 3, E.correlation_id, MA.nid, MA.agent_id, coalesce(MA.queue_id,0), MA.start "
                        "FROM rocpd_region R "
                        "INNER JOIN rocpd_event E ON R.event_id = E.id "
                        "INNER JOIN _rocpd_memory_copy MA ON MA.id = E.correlation_id "
                        "WHERE R.id == ";
            query << event_id.bitfield.event_id;
                        query << ");";
            ShowProgress(0, query.str().c_str(),kRPVDbBusy, future);
            if (kRocProfVisDmResultSuccess != ExecuteSQLQuery(future, query.str().c_str(), flowtrace, &CallbackAddFlowTrace)) break;
        } else
        if (event_id.bitfield.event_op == kRocProfVisDmOperationDispatch)
        {
            query <<    "SELECT E.id as id, 1, E.correlation_id, R.nid, R.pid, R.tid, R.end "
                        "FROM rocpd_kernel_dispatch KD "
                        "INNER JOIN rocpd_event E ON KD.event_id = E.id "
                        "INNER JOIN rocpd_region R ON R.id = E.correlation_id "
                        "WHERE KD.id == ";
            query << event_id.bitfield.event_id << ";";  
            ShowProgress(0, query.str().c_str(),kRPVDbBusy, future);
            if (kRocProfVisDmResultSuccess != ExecuteSQLQuery(future, query.str().c_str(), flowtrace, &CallbackAddFlowTrace)) break;
        } else
        if (event_id.bitfield.event_op == kRocProfVisDmOperationMemoryCopy)
        {
            query <<    "SELECT E.id as id, 1, E.correlation_id, R.nid, R.pid, R.tid, R.end "
                        "FROM _rocpd_memory_copy MC "
                        "INNER JOIN rocpd_event E ON MC.event_id = E.id "
                        "INNER JOIN rocpd_region R ON R.id = E.correlation_id "
                        "WHERE MC.id == ";
            query << event_id.bitfield.event_id << ";";
            ShowProgress(0, query.str().c_str(), kRPVDbBusy, future);
            if (kRocProfVisDmResultSuccess != ExecuteSQLQuery(future, query.str().c_str(), flowtrace, &CallbackAddFlowTrace)) break;
        }
        else
        if (event_id.bitfield.event_op == kRocProfVisDmOperationMemoryAllocate)
        {
            query <<    "SELECT E.id as id, 1, E.correlation_id, R.nid, R.pid, R.tid, R.end "
                        "FROM _rocpd_memory_allocate MA "
                        "INNER JOIN rocpd_event E ON MA.event_id = E.id "
                        "INNER JOIN rocpd_region R ON R.id = E.correlation_id "
                        "WHERE MA.id == ";
            query << event_id.bitfield.event_id << ";";
            ShowProgress(0, query.str().c_str(), kRPVDbBusy, future);
            if (kRocProfVisDmResultSuccess != ExecuteSQLQuery(future, query.str().c_str(), flowtrace, &CallbackAddFlowTrace)) break;
        }
        else
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

rocprofvis_dm_result_t  RocprofDatabase::ReadStackTraceInfo(
        rocprofvis_dm_event_id_t event_id,
        Future* future)
{
    return future->SetPromise(kRocProfVisDmResultNotSupported);

    ROCPROFVIS_ASSERT_MSG_RETURN(future, ERROR_FUTURE_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
}


rocprofvis_dm_result_t  RocprofDatabase::ReadExtEventInfo(
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
            query << "select * from regions where id == ";
            query << event_id.bitfield.event_id << ";";  
            ShowProgress(0, query.str().c_str(),kRPVDbBusy, future);
            if (kRocProfVisDmResultSuccess != ExecuteSQLQuery(future, query.str().c_str(), "Properties", extdata, &CallbackAddExtInfo)) break;
        } else
        if (event_id.bitfield.event_op == kRocProfVisDmOperationDispatch)
        {
            query << "select * from kernels where id == ";
            query << event_id.bitfield.event_id << ";";  
            ShowProgress(0, query.str().c_str(),kRPVDbBusy, future);
            if (kRocProfVisDmResultSuccess != ExecuteSQLQuery(future, query.str().c_str(), "Properties", extdata, &CallbackAddExtInfo)) break;
        } else   
        if (event_id.bitfield.event_op == kRocProfVisDmOperationMemoryAllocate)
        {
            query << "select * from memory_allocation where id == ";
            query << event_id.bitfield.event_id << ";";
            ShowProgress(0, query.str().c_str(), kRPVDbBusy, future);
            if (kRocProfVisDmResultSuccess != ExecuteSQLQuery(future, query.str().c_str(), "Properties", extdata, &CallbackAddExtInfo)) break;
        } else
        if (event_id.bitfield.event_op == kRocProfVisDmOperationMemoryCopy)
        {
            query << "select * from memory_copies where id == ";
            query << event_id.bitfield.event_id << ";";
            ShowProgress(0, query.str().c_str(), kRPVDbBusy, future);
            if (kRocProfVisDmResultSuccess != ExecuteSQLQuery(future, query.str().c_str(), "Properties", extdata, &CallbackAddExtInfo)) break;
        } else
        {
            ShowProgress(0, "Extended data not available for specified operation type!", kRPVDbError, future );
            return future->SetPromise(kRocProfVisDmResultInvalidParameter);
        }
        ShowProgress(100, "Extended data successfully loaded!",kRPVDbSuccess, future);
        return future->SetPromise(kRocProfVisDmResultSuccess);
    }
    ShowProgress(0, "Extended data  not loaded!", kRPVDbError, future );
    return future->SetPromise(future->Interrupted() ? kRocProfVisDmResultTimeout : kRocProfVisDmResultDbAccessFailed);    
}

}  // namespace DataModel
}  // namespace RocProfVis
