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


int RocprofDatabase::CallBackAddTrack(void *data, int argc, char **argv, char **azColName){
    ROCPROFVIS_ASSERT_MSG_RETURN(argc==NUMBER_OF_TRACK_IDENTIFICATION_PARAMETERS+1, ERROR_DATABASE_QUERY_PARAMETERS_MISMATCH, 1);
    ROCPROFVIS_ASSERT_MSG_RETURN(data, ERROR_SQL_QUERY_PARAMETERS_CANNOT_BE_NULL, 1);
    rocprofvis_dm_track_params_t track_params = {0};
    rocprofvis_db_sqlite_callback_parameters* callback_params = (rocprofvis_db_sqlite_callback_parameters*)data;
    RocprofDatabase* db = (RocprofDatabase*)callback_params->db;
    if (callback_params->future->Interrupted()) return 1;
    track_params.track_id = (rocprofvis_dm_track_id_t)db->NumTracks();
    track_params.track_category = (rocprofvis_dm_track_category_t)std::stol(argv[TRACK_ID_CATEGORY]);
    for (int i = 0; i < NUMBER_OF_TRACK_IDENTIFICATION_PARAMETERS; i++) {
        track_params.process_tag[i] = azColName[i];
        track_params.process_id_numeric[i] = Database::IsNumber(argv[i]);
        if (track_params.process_id_numeric[i]) {
            track_params.process_id[i] = std::stoll(argv[i]);
        } else {
            track_params.process_name[i] = argv[i];
        }        
    }
    
    if (!db->TrackExist(track_params, callback_params->subquery)){
        db->find_track_map[track_params.process_id[TRACK_ID_NODE]][track_params.process_id[TRACK_ID_PID_OR_AGENT]][track_params.process_id[TRACK_ID_TID_OR_QUEUE]] = track_params.track_id;
        if (track_params.track_category == kRocProfVisDmRegionTrack) {
            track_params.process_name[TRACK_ID_PID] = ProcessNameSuffixFor(track_params.track_category);
            track_params.process_name[TRACK_ID_PID] += argv[TRACK_ID_PID];
            track_params.process_name[TRACK_ID_TID] = SubProcessNameSuffixFor(track_params.track_category);
            track_params.process_name[TRACK_ID_TID] += argv[TRACK_ID_TID];

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

int RocprofDatabase::CallBackAddString(void *data, int argc, char **argv, char **azColName){
    ROCPROFVIS_ASSERT_MSG_RETURN(argc==1, ERROR_DATABASE_QUERY_PARAMETERS_MISMATCH, 1);
    ROCPROFVIS_ASSERT_MSG_RETURN(data, ERROR_SQL_QUERY_PARAMETERS_CANNOT_BE_NULL, 1);
    rocprofvis_db_sqlite_callback_parameters* callback_params = (rocprofvis_db_sqlite_callback_parameters*)data;
    RocprofDatabase* db = (RocprofDatabase*)callback_params->db;
    if (callback_params->future->Interrupted()) return 1;
    uint32_t stringId = db->BindObject()->FuncAddString(db->BindObject()->trace_object, argv[0]);
    callback_params->future->CountThisRow();
    return 0;
}


int RocprofDatabase::CallbackCacheTable(void *data, int argc, char **argv, char **azColName){
    ROCPROFVIS_ASSERT_MSG_RETURN(data, ERROR_SQL_QUERY_PARAMETERS_CANNOT_BE_NULL, 1);
    rocprofvis_db_sqlite_callback_parameters* callback_params = (rocprofvis_db_sqlite_callback_parameters*)data;
    RocprofDatabase* db = (RocprofDatabase*)callback_params->db;
    DatabaseCache * ref_tables = (DatabaseCache *)callback_params->handle;
    uint64_t id = std::stoll( argv[0] );
    for (int i=0; i < argc; i++)
    {
        ref_tables->AddTableCell(callback_params->subquery, id, azColName[i], argv[i]);
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

        ShowProgress(1, "Getting minimum timestamp", kRPVDbBusy, future );
        if (kRocProfVisDmResultSuccess != ExecuteSQLQuery(future,"SELECT MIN(start) FROM rocpd_region;", &CallbackGetValue, TraceProperties()->start_time)) break;

        ShowProgress(1, "Get maximum timestamp", kRPVDbBusy, future );
        if (kRocProfVisDmResultSuccess != ExecuteSQLQuery(future,"SELECT MAX(end) FROM ("
                                                                                                " SELECT end FROM rocpd_region"
                                                                                                " UNION ALL"
                                                                                                " SELECT timestamp FROM rocpd_sample"
                                                                                                " UNION ALL"
                                                                                                " SELECT end FROM rocpd_kernel_dispatch"
                                                                                                " UNION ALL"
                                                                                                " SELECT end FROM rocpd_memory_allocate"
                                                                                                " UNION ALL"
                                                                                                " SELECT end FROM rocpd_memory_copy"
                                                                                                " );", &CallbackGetValue, TraceProperties()->end_time)) break;

        ShowProgress(5, "Adding CPU tracks", kRPVDbBusy, future );
        if (kRocProfVisDmResultSuccess != ExecuteSQLQuery(future, 
                    "select DISTINCT nid, pid, tid, 2 from rocpd_region;", 
                    "select 1 as op, R.start, R.end, E.category_id, R.name_id, R.id, R.nid, R.pid, R.tid from rocpd_region R INNER JOIN rocpd_event E ON E.id = R.event_id " , 
                    &CallBackAddTrack)) break;

        ShowProgress(5, "Adding Kernel Dispatch tracks", kRPVDbBusy, future );
        if (kRocProfVisDmResultSuccess != ExecuteSQLQuery(future, 
                    "select DISTINCT nid, agent_id, queue_id, 3 from rocpd_kernel_dispatch;", 
                    "select 2 as op, KD.start, KD.end, E.category_id, KD.kernel_id, KD.id, KD.nid, KD.agent_id, KD.queue_id from rocpd_kernel_dispatch KD INNER JOIN rocpd_event E ON E.id = KD.event_id ", 
                    &CallBackAddTrack)) break;

        ShowProgress(5, "Adding Memory Allocation tracks", kRPVDbBusy, future );
        if (kRocProfVisDmResultSuccess != ExecuteSQLQuery(future, 
                    "select DISTINCT nid, coalesce(agent_id, 0) agent_id, coalesce(queue_id,0) queue_id, 3  from rocpd_memory_allocate;", 
                    "select 3 as op, MA.start, MA.end, E.category_id, 0, MA.id, MA.nid, coalesce(MA.agent_id,0) as agent_id, coalesce(MA.queue_id,0) queue_id from rocpd_memory_allocate MA INNER JOIN rocpd_event E ON E.id = MA.event_id ", 
                    &CallBackAddTrack)) break;

        ShowProgress(5, "Adding Memory allocation graph tracks", kRPVDbBusy, future );
        if (kRocProfVisDmResultSuccess != ExecuteSQLQuery(future, 
                    "select DISTINCT nid, coalesce(agent_id, 0) agent_id, -1 as const, 1 from rocpd_memory_allocate;", 
                    "select 0 as op, start, sum(CASE WHEN type = 'FREE' THEN (select -size from rocpd_memory_allocate MA1 where MA.address == MA1.address limit 1) ELSE size END) over (ORDER BY start ROWS BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW) as current, 0, 0, 0, nid, coalesce(agent_id, 0) agent_id, -1 as queue_id  from rocpd_memory_allocate MA ", 
                    &CallBackAddTrack)) break;

        ShowProgress(5, "Adding Memory Copy tracks", kRPVDbBusy, future );
        if (kRocProfVisDmResultSuccess != ExecuteSQLQuery(future, 
                    "select DISTINCT nid, dst_agent_id, coalesce(queue_id,0) queue_id, 3 from rocpd_memory_copy;", 
                    "select 4 as op, MC.start, MC.end, E.category_id, MC.name_id, MC.id, MC.nid, MC.dst_agent_id as agent_id, coalesce(MC.queue_id,0) queue_id from rocpd_memory_copy MC INNER JOIN rocpd_event E ON E.id = MC.event_id ", 
                    &CallBackAddTrack)) break;
        
        // PMC schema is not fully defined yet
        //ShowProgress(5, "Adding PMC tracks", kRPVDbBusy, future );
        //if (kRocProfVisDmResultSuccess != ExecuteSQLQuery(future, "select DISTINCT nid, agent_id, id, 1 from rocpd_pmc", &CallBackAddTrack)) break;

        ShowProgress(20, "Loading strings", kRPVDbBusy, future );
        BindObject()->FuncAddString(BindObject()->trace_object, ""); // 0 index string
        if (kRocProfVisDmResultSuccess != ExecuteSQLQuery(future, "SELECT string FROM rocpd_string;", &CallBackAddString)) break;
        if (kRocProfVisDmResultSuccess != ExecuteSQLQuery(future,"SELECT COUNT(*) FROM rocpd_string;", &CallbackGetValue, m_symbols_offset)) break;
        if (kRocProfVisDmResultSuccess != ExecuteSQLQuery(future, "SELECT display_name FROM rocpd_info_kernel_symbol;", &CallBackAddString)) break;

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
