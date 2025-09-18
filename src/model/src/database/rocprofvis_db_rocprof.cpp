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
                                                    uint64_t node,
                                                    uint32_t process,
                                                    const char* subprocess,
                                                    rocprofvis_dm_op_t operation,
                                                    rocprofvis_dm_track_id_t& track_id) {
    

    auto it_node = find_track_map.find(node);
    if (it_node!=find_track_map.end()){
        auto it_process = it_node->second.find(process);
        if (it_process!=it_node->second.end()){
            auto it_subprocess = it_process->second.find(std::atol(subprocess));
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

rocprofvis_dm_result_t RocprofDatabase::RemapStringIds(rocprofvis_db_flow_data_t& record)
{
    if(record.id.bitfield.event_op == kRocProfVisDmOperationDispatch)
        record.symbol_id += m_symbols_offset;
    return kRocProfVisDmResultSuccess;
}


int RocprofDatabase::CallBackAddTrack(void *data, int argc, sqlite3_stmt* stmt, char **azColName){
    ROCPROFVIS_ASSERT_MSG_RETURN(argc==NUMBER_OF_TRACK_IDENTIFICATION_PARAMETERS+1, ERROR_DATABASE_QUERY_PARAMETERS_MISMATCH, 1);
    ROCPROFVIS_ASSERT_MSG_RETURN(data, ERROR_SQL_QUERY_PARAMETERS_CANNOT_BE_NULL, 1);
    void* func = (void*)&CallBackAddTrack;
    rocprofvis_dm_track_params_t track_params = {0};
    rocprofvis_db_sqlite_callback_parameters* callback_params = (rocprofvis_db_sqlite_callback_parameters*)data;
    RocprofDatabase* db = (RocprofDatabase*)callback_params->db;
    if(callback_params->future->Interrupted()) return SQLITE_ABORT;
    track_params.track_id = (rocprofvis_dm_track_id_t)db->NumTracks();
    track_params.process.category = (rocprofvis_dm_track_category_t)db->Sqlite3ColumnInt(func, stmt, azColName,TRACK_ID_CATEGORY);
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
    
    rocprofvis_dm_track_params_it it = db->FindTrack(track_params.process);
    db->UpdateQueryForTrack(it, track_params, callback_params->query);
    if(it == db->TrackPropertiesEnd())
    {
        db->find_track_map[track_params.process.id[TRACK_ID_NODE]][track_params.process.id[TRACK_ID_PID_OR_AGENT]][track_params.process.id[TRACK_ID_TID_OR_QUEUE]] = track_params.track_id;
        if (track_params.process.category == kRocProfVisDmRegionTrack) {

            track_params.process.name[TRACK_ID_PID] = db->CachedTables()->GetTableCell("Process", track_params.process.id[TRACK_ID_PID], "command");
            track_params.process.name[TRACK_ID_PID] += "(";
            track_params.process.name[TRACK_ID_PID] += std::to_string(track_params.process.id[TRACK_ID_PID]);
            track_params.process.name[TRACK_ID_PID] += ")";

            track_params.process.name[TRACK_ID_TID] = db->CachedTables()->GetTableCell("Thread", track_params.process.id[TRACK_ID_TID], "name");
            track_params.process.name[TRACK_ID_TID] += "(";
            track_params.process.name[TRACK_ID_TID] += std::to_string(track_params.process.id[TRACK_ID_TID]);
            track_params.process.name[TRACK_ID_TID] += ")";
        }
        else if(track_params.process.category == kRocProfVisDmKernelDispatchTrack ||
                track_params.process.category == kRocProfVisDmMemoryAllocationTrack ||
                track_params.process.category == kRocProfVisDmMemoryCopyTrack ||
                track_params.process.category == kRocProfVisDmPmcTrack)
          {
                track_params.process.name[TRACK_ID_AGENT] = db->CachedTables()->GetTableCell("Agent", track_params.process.id[TRACK_ID_AGENT], "product_name");
                track_params.process.name[TRACK_ID_AGENT] += "(";
                track_params.process.name[TRACK_ID_AGENT] += db->CachedTables()->GetTableCell("Agent", track_params.process.id[TRACK_ID_AGENT], "type_index");
                track_params.process.name[TRACK_ID_AGENT] += ")";
                if(track_params.process.category == kRocProfVisDmKernelDispatchTrack ||
                   track_params.process.category == kRocProfVisDmMemoryAllocationTrack ||
                   track_params.process.category == kRocProfVisDmMemoryCopyTrack)
                {
                    track_params.process.name[TRACK_ID_QUEUE] = db->CachedTables()->GetTableCell("Queue", track_params.process.id[TRACK_ID_QUEUE], "name");
                }
                else {
                    track_params.process.name[TRACK_ID_QUEUE] = db->CachedTables()->GetTableCell("PMC", track_params.process.id[TRACK_ID_QUEUE], "name");
                }
                
          }
          else if(track_params.process.category == kRocProfVisDmStreamTrack)
        {
                track_params.process.name[TRACK_ID_STREAM] = db->CachedTables()->GetTableCell("Stream", track_params.process.id[TRACK_ID_STREAM], "name");
        }
        if (kRocProfVisDmResultSuccess != db->AddTrackProperties(track_params)) return 1;
        if (db->BindObject()->FuncAddTrack(db->BindObject()->trace_object, db->TrackPropertiesLast()) != kRocProfVisDmResultSuccess) return 1;  
        if (db->CachedTables()->PopulateTrackExtendedDataTemplate(db, "Node", track_params.process.id[TRACK_ID_NODE]) != kRocProfVisDmResultSuccess) return 1;
        if(track_params.process.category == kRocProfVisDmRegionTrack)
        {
            if (db->CachedTables()->PopulateTrackExtendedDataTemplate(db, "Process", track_params.process.id[TRACK_ID_PID]) != kRocProfVisDmResultSuccess) return 1;
            if (db->CachedTables()->PopulateTrackExtendedDataTemplate(db, "Thread", track_params.process.id[TRACK_ID_TID]) != kRocProfVisDmResultSuccess) return 1;
        } 
        else if(track_params.process.category == kRocProfVisDmStreamTrack)
        {
            if (db->CachedTables()->PopulateTrackExtendedDataTemplate(db, "Stream", track_params.process.id[TRACK_ID_STREAM]) != kRocProfVisDmResultSuccess) return 1; 
        }
        else if(track_params.process.category == kRocProfVisDmKernelDispatchTrack ||
                track_params.process.category == kRocProfVisDmMemoryAllocationTrack ||
                track_params.process.category == kRocProfVisDmMemoryCopyTrack ||
                track_params.process.category == kRocProfVisDmPmcTrack)
        {
            if (db->CachedTables()->PopulateTrackExtendedDataTemplate(db, "Agent", track_params.process.id[TRACK_ID_AGENT]) != kRocProfVisDmResultSuccess) return 1; 
            if(track_params.process.category == kRocProfVisDmKernelDispatchTrack ||
               track_params.process.category == kRocProfVisDmMemoryAllocationTrack ||
               track_params.process.category == kRocProfVisDmMemoryCopyTrack)
            {
                if (db->CachedTables()->PopulateTrackExtendedDataTemplate(db, "Queue", track_params.process.id[TRACK_ID_QUEUE]) != kRocProfVisDmResultSuccess) return 1;
            }
            else
            {
                if(track_params.process.id[TRACK_ID_QUEUE] > 0)
                    if (db->CachedTables()->PopulateTrackExtendedDataTemplate(db, "PMC", track_params.process.id[TRACK_ID_QUEUE]) != kRocProfVisDmResultSuccess) return 1;
            }
        }
    }
    callback_params->future->CountThisRow();
    return 0;
}

int RocprofDatabase::CallBackAddString(void *data, int argc, sqlite3_stmt* stmt, char **azColName){
    ROCPROFVIS_ASSERT_MSG_RETURN(argc==1, ERROR_DATABASE_QUERY_PARAMETERS_MISMATCH, 1);
    ROCPROFVIS_ASSERT_MSG_RETURN(data, ERROR_SQL_QUERY_PARAMETERS_CANNOT_BE_NULL, 1);
    void*  func = (void*)&CallBackAddString;
    rocprofvis_db_sqlite_callback_parameters* callback_params = (rocprofvis_db_sqlite_callback_parameters*)data;
    RocprofDatabase* db = (RocprofDatabase*)callback_params->db;
    if(callback_params->future->Interrupted()) return SQLITE_ABORT;
    uint32_t stringId = db->BindObject()->FuncAddString(db->BindObject()->trace_object, (char*) db->Sqlite3ColumnText(func, stmt, azColName,0));
    callback_params->future->CountThisRow();
    return 0;
}


int RocprofDatabase::CallbackCacheTable(void *data, int argc, sqlite3_stmt* stmt, char **azColName){
    ROCPROFVIS_ASSERT_MSG_RETURN(data, ERROR_SQL_QUERY_PARAMETERS_CANNOT_BE_NULL, 1);
    void* func = (void*)&CallbackCacheTable;
    rocprofvis_db_sqlite_callback_parameters* callback_params = (rocprofvis_db_sqlite_callback_parameters*)data;
    RocprofDatabase* db = (RocprofDatabase*)callback_params->db;
    DatabaseCache * ref_tables = (DatabaseCache *)callback_params->handle;
    uint64_t id = db->Sqlite3ColumnInt64(func, stmt, azColName,0);
    for (int i=0; i < argc; i++)
    {
        ref_tables->AddTableCell(callback_params->query[kRPVCacheTableName], id, azColName[i],
                                 (char*) db->Sqlite3ColumnText(func, stmt, azColName, i));
    }
    callback_params->future->CountThisRow();
    return 0;
}

int
RocprofDatabase::CallbackNodeEnumeration(void* data, int argc, sqlite3_stmt* stmt,
                                         char** azColName)
{
    ROCPROFVIS_ASSERT_MSG_RETURN(data, ERROR_SQL_QUERY_PARAMETERS_CANNOT_BE_NULL, 1);
    void*  func = (void*)&CallbackNodeEnumeration;
    rocprofvis_db_sqlite_callback_parameters* callback_params =
        (rocprofvis_db_sqlite_callback_parameters*) data;
    RocprofDatabase* db = (RocprofDatabase*) callback_params->db;
    guid_list_t*     guid_list = (guid_list_t*) callback_params->handle;
    std::string      table_name_befor_guid = callback_params->query[kRPVCacheTableName];
    std::string      table_name            = (char*) db->Sqlite3ColumnText(func, stmt, azColName, 0);
    size_t           pos                   = table_name.find(table_name_befor_guid);
    if(pos == 0)
    {
        guid_list->push_back(table_name.substr(table_name_befor_guid.length()));
    }
    callback_params->future->CountThisRow();
    return 0;
}

int
RocprofDatabase::CallbackAddStackTrace(void* data, int argc, sqlite3_stmt* stmt,
                                       char** azColName)
{
    ROCPROFVIS_ASSERT_MSG_RETURN(data, ERROR_SQL_QUERY_PARAMETERS_CANNOT_BE_NULL, 1);
    ROCPROFVIS_ASSERT_MSG_RETURN(argc == 2, ERROR_DATABASE_QUERY_PARAMETERS_MISMATCH, 1);
    void*  func = (void*)&CallbackAddStackTrace;
    rocprofvis_db_sqlite_callback_parameters* callback_params =
        (rocprofvis_db_sqlite_callback_parameters*) data;
    RocprofDatabase*           db = (RocprofDatabase*) callback_params->db;
    rocprofvis_db_stack_data_t record;
    if(callback_params->future->Interrupted()) return 1;
    record.symbol = (char*) db->Sqlite3ColumnText(func, stmt, azColName, 0);
    record.line   = (char*) db->Sqlite3ColumnText(func, stmt, azColName, 1);
    record.args   = "";
    record.depth  = callback_params->future->GetProcessedRowsCount();
    if(db->BindObject()->FuncAddStackFrame(callback_params->handle, record) !=
       kRocProfVisDmResultSuccess)
        return 1;
    callback_params->future->CountThisRow();
    return 0;
}   

rocprofvis_dm_result_t
RocprofDatabase::CreateIndexes()
{ 
    std::vector<std::string> vec;
    for(int i = 0; i < GuidList().size(); i++)
    {
        vec.push_back(std::string("CREATE INDEX IF NOT EXISTS rocpd_region_idx_") + GuidList()[i] +
                 " ON rocpd_region_" + GuidList()[i] + "(nid,pid,tid,start);");
        vec.push_back(std::string("CREATE INDEX IF NOT EXISTS rocpd_kernel_dispatch_idx_") + GuidList()[i] +
                 " ON rocpd_kernel_dispatch_" + GuidList()[i] + "(nid,agent_id,queue_id,start);");
        vec.push_back(std::string("CREATE INDEX IF NOT EXISTS rocpd_kernel_dispatch_stream_idx_") + GuidList()[i] +
                 " ON rocpd_kernel_dispatch_" + GuidList()[i] + "(nid,stream_id,start);");
        vec.push_back(std::string("CREATE INDEX IF NOT EXISTS rocpd_memory_allocate_idx_") + GuidList()[i] +
                 " ON rocpd_memory_allocate_" + GuidList()[i] + "(nid,agent_id,queue_id,start);");
        vec.push_back(std::string("CREATE INDEX IF NOT EXISTS rocpd_memory_allocate_stream_idx_") + GuidList()[i] +
                 " ON rocpd_memory_allocate_" + GuidList()[i] + "(nid,stream_id,start);");
        vec.push_back(std::string("CREATE INDEX IF NOT EXISTS rocpd_memory_copy_idx_") + GuidList()[i] +
                 " ON rocpd_memory_copy_" + GuidList()[i] + "(nid,dst_agent_id,queue_id,start);");
        vec.push_back(std::string("CREATE INDEX IF NOT EXISTS rocpd_memory_copy_stream_idx_") + GuidList()[i] +
                 " ON rocpd_memory_copy_" + GuidList()[i] + "(nid,stream_id,start);");
    }

    return ExecuteTransaction(vec);
}


rocprofvis_dm_result_t  RocprofDatabase::ReadTraceMetadata(Future* future)
{
    ROCPROFVIS_ASSERT_MSG_RETURN(future, ERROR_FUTURE_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
    while (true)
    {
        ROCPROFVIS_ASSERT_MSG_BREAK(BindObject()->trace_properties, ERROR_TRACE_PROPERTIES_CANNOT_BE_NULL);
        std::string value;

        ShowProgress(2, "Detect Nodes", kRPVDbBusy, future);
        ExecuteSQLQuery(future,
                        "SELECT name from sqlite_master where type = 'table' and name like 'rocpd_info_node_%';",
                        "rocpd_info_node_", (rocprofvis_dm_handle_t) &GuidList(),
                        &CallbackNodeEnumeration);
        if(GuidList().size() > 1)
        {
            spdlog::debug("Multi-node databases are not yet supported!");
            break;
        }

        ShowProgress(5, "Indexing tables (once per database)", kRPVDbBusy, future);
        CreateIndexes();

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
        ExecuteSQLQuery(future,"SELECT id, guid, nid, pid, agent_id, target_arch, COALESCE(event_code,0) as event_code, COALESCE(instance_id,0) as instance_id, name, symbol, description, long_description, component, units, value_type, block, expression, is_constant, is_derived, extdata from rocpd_info_pmc;", "PMC", (rocprofvis_dm_handle_t)CachedTables(), &CallbackCacheTable);

        CachedTables()->AddTableCell("PMC", -1, "name", "MALLOC");
                
        ShowProgress(5, "Adding HIP API tracks", kRPVDbBusy, future );
        if (kRocProfVisDmResultSuccess != ExecuteSQLQuery(future,
            { 
                // Track query by agent/queue
                 Builder::Select(rocprofvis_db_sqlite_track_query_format(
                     { { Builder::QParam("nid", Builder::NODE_ID_SERVICE_NAME),
                         Builder::QParam("pid", Builder::PROCESS_ID_SERVICE_NAME),
                         Builder::QParam("tid", Builder::THREAD_ID_SERVICE_NAME),
                         Builder::QParamCategory(kRocProfVisDmRegionTrack) },
                       { Builder::From("rocpd_region") } })),
                 // Track query by stream
                 "",
                 // Level query
                 Builder::Select(rocprofvis_db_sqlite_level_query_format(
                     { { Builder::QParamOperation(kRocProfVisDmOperationLaunch),
                         Builder::QParam("start", Builder::START_SERVICE_NAME), 
                         Builder::QParam("end", Builder::END_SERVICE_NAME),
                         Builder::QParam("id"),
                         Builder::QParam("nid", Builder::NODE_ID_SERVICE_NAME),
                         Builder::QParam("pid", Builder::PROCESS_ID_SERVICE_NAME),
                         Builder::QParam("tid", Builder::THREAD_ID_SERVICE_NAME),
                         Builder::SpaceSaver(0) },
                       { Builder::From("rocpd_region") } })),
                 // Slice query by queue
                 Builder::Select(rocprofvis_db_sqlite_slice_query_format(
                     { { Builder::QParamOperation(kRocProfVisDmOperationLaunch),
                         Builder::QParam("R.start", Builder::START_SERVICE_NAME), 
                         Builder::QParam("R.end", Builder::END_SERVICE_NAME),
                         Builder::QParam("E.category_id"),
                         Builder::QParam("R.name_id"), 
                         Builder::QParam("R.id"),
                         Builder::QParam("R.nid", Builder::NODE_ID_SERVICE_NAME),
                         Builder::QParam("R.pid", Builder::PROCESS_ID_SERVICE_NAME),
                         Builder::QParam("R.tid", Builder::THREAD_ID_SERVICE_NAME),
                         Builder::QParam("L.level") },
                       { Builder::From("rocpd_region", "R"),
                         Builder::InnerJoin("rocpd_event", "E", "E.id = R.event_id AND E.guid = R.guid"),
                         Builder::LeftJoin(Builder::LevelTable("launch"), "L", "R.id = L.eid") } })),
                 // Slice query by Stream
                 "",
                 // Table query
                 Builder::Select(rocprofvis_db_sqlite_table_query_format(
                     { this,
                         { Builder::QParamOperation(kRocProfVisDmOperationLaunch),
                         Builder::QParam("R.id"),
                         Builder::QParam("( SELECT string FROM `rocpd_string` RS WHERE "
                                         "RS.id = E.category_id AND RS.guid = E.guid)",
                                         "category"),
                         Builder::QParam("S.string", "name"),
                         Builder::QParamBlank("stream"),
                         Builder::QParamBlank("queue"),
                         Builder::QParam("R.nid", Builder::NODE_ID_SERVICE_NAME),
                         Builder::QParam("P.pid", Builder::PROCESS_ID_PUBLIC_NAME),
                         Builder::QParam("T.tid", Builder::THREAD_ID_PUBLIC_NAME),
                         Builder::QParamBlank("device_index"),
                         Builder::QParamBlank("device"),
                         Builder::QParamBlank("device_name"),
                         Builder::QParam("R.start", Builder::START_SERVICE_NAME),
                         Builder::QParam("R.end", Builder::END_SERVICE_NAME),
                         Builder::QParam("(R.end-R.start)", "duration"),

                         Builder::QParamBlank("GridSizeX"),
                         Builder::QParamBlank("GridSizeY"),
                         Builder::QParamBlank("GridSizeZ"),

                         Builder::QParamBlank("WGSizeX"),
                         Builder::QParamBlank("WGSizeY"),
                         Builder::QParamBlank("WGSizeZ"),

                         Builder::QParamBlank("LDSSize"),
                         Builder::QParamBlank("ScratchSize"),

                         Builder::QParamBlank("StaticLDSSize"),
                         Builder::QParamBlank("StaticScratchSize"),

                         Builder::QParamBlank("size"),
                         Builder::QParamBlank("address"),
                         Builder::QParamBlank("level"),

                         Builder::QParamBlank("SrcIndex"),
                         Builder::QParamBlank("SrcDevice"),
                         Builder::QParamBlank("SrcName"),
                         Builder::QParamBlank("SrcAddr"),

                         Builder::QParam("R.pid", Builder::PROCESS_ID_SERVICE_NAME),
                         Builder::QParam("R.tid", Builder::THREAD_ID_SERVICE_NAME),
                         Builder::QParam("-1", Builder::STREAM_ID_SERVICE_NAME) },
                       { Builder::From("rocpd_region", "R"),
                         Builder::InnerJoin("rocpd_string", "S", "S.id = R.name_id AND S.guid = R.guid"),
                         Builder::InnerJoin("rocpd_event", "E", "E.id = R.event_id AND E.guid = R.guid"),
                         Builder::InnerJoin("rocpd_info_process", "P", "P.id = R.pid AND P.guid = R.guid"),
                         Builder::InnerJoin("rocpd_info_thread", "T", "T.id = R.tid AND T.guid = R.guid") } })),
            },
                    &CallBackAddTrack)) break;

        ShowProgress(5, "Adding kernel dispatch tracks", kRPVDbBusy, future );
        if (kRocProfVisDmResultSuccess != ExecuteSQLQuery(future,
            {
                // Track query by agent/queue
                 Builder::Select(rocprofvis_db_sqlite_track_query_format(
                     { { Builder::QParam("nid", Builder::NODE_ID_SERVICE_NAME),
                         Builder::QParam("agent_id", Builder::AGENT_ID_SERVICE_NAME),
                         Builder::QParam("queue_id", Builder::QUEUE_ID_SERVICE_NAME),
                         Builder::QParamCategory(kRocProfVisDmKernelDispatchTrack) },
                       { Builder::From("rocpd_kernel_dispatch") } })),
                 // Track query by stream
                 Builder::Select(rocprofvis_db_sqlite_track_query_format(
                     { { Builder::QParam("nid", Builder::NODE_ID_SERVICE_NAME),
                         Builder::QParam("stream_id", Builder::STREAM_ID_SERVICE_NAME),
                         Builder::SpaceSaver(-1),
                         Builder::QParamCategory(kRocProfVisDmStreamTrack) },
                       { Builder::From("rocpd_kernel_dispatch") } })),
                 // Level query
                 Builder::Select(rocprofvis_db_sqlite_level_query_format(
                     { { Builder::QParamOperation(kRocProfVisDmOperationDispatch),
                         Builder::QParam("start", Builder::START_SERVICE_NAME), 
                         Builder::QParam("end", Builder::END_SERVICE_NAME),
                         Builder::QParam("id"),
                         Builder::QParam("nid", Builder::NODE_ID_SERVICE_NAME),
                         Builder::QParam("agent_id", Builder::AGENT_ID_SERVICE_NAME),
                         Builder::QParam("queue_id", Builder::QUEUE_ID_SERVICE_NAME),
                         Builder::QParam("stream_id", Builder::STREAM_ID_SERVICE_NAME) },
                         { Builder::From("rocpd_kernel_dispatch") } })),
                 // Slice query by queue
                 Builder::Select(rocprofvis_db_sqlite_slice_query_format(
                     { { Builder::QParamOperation(kRocProfVisDmOperationDispatch),
                         Builder::QParam("K.start", Builder::START_SERVICE_NAME), 
                         Builder::QParam("K.end", Builder::END_SERVICE_NAME),
                         Builder::QParam("E.category_id"), 
                         Builder::QParam("K.kernel_id"),
                         Builder::QParam("K.id"),
                         Builder::QParam("K.nid", Builder::NODE_ID_SERVICE_NAME),
                         Builder::QParam("K.agent_id", Builder::AGENT_ID_SERVICE_NAME),
                         Builder::QParam("K.queue_id", Builder::QUEUE_ID_SERVICE_NAME),
                         Builder::QParam("L.level") },
                       { Builder::From("rocpd_kernel_dispatch", "K"),
                         Builder::InnerJoin("rocpd_event", "E", "E.id = K.event_id AND E.guid = K.guid"),
                         Builder::LeftJoin(Builder::LevelTable("dispatch"), "L", "K.id = L.eid") } })),
                 // Slice query by stream
                 Builder::Select(rocprofvis_db_sqlite_slice_query_format(
                     { { Builder::QParamOperation(kRocProfVisDmOperationDispatch),
                         Builder::QParam("K.start", Builder::START_SERVICE_NAME), 
                         Builder::QParam("K.end", Builder::END_SERVICE_NAME),
                         Builder::QParam("E.category_id"), 
                         Builder::QParam("K.kernel_id"),
                         Builder::QParam("K.id"),
                         Builder::QParam("K.nid", Builder::NODE_ID_SERVICE_NAME),
                         Builder::QParam("K.stream_id", Builder::STREAM_ID_SERVICE_NAME),
                         Builder::SpaceSaver(-1),
                         Builder::QParam("L.level_for_stream", "level") },
                       { Builder::From("rocpd_kernel_dispatch", "K"),
                         Builder::InnerJoin("rocpd_event", "E", "E.id = K.event_id AND E.guid = K.guid"),
                         Builder::LeftJoin(Builder::LevelTable("dispatch"), "L", "K.id = L.eid") } })),
                 // Table query
                 Builder::Select(rocprofvis_db_sqlite_table_query_format(
                     { this,
                         { Builder::QParamOperation(kRocProfVisDmOperationDispatch),
                         Builder::QParam("K.id"),
                         Builder::QParam("( SELECT string FROM `rocpd_string` RS WHERE "
                                         "RS.id = E.category_id AND RS.guid = E.guid)",
                                         "category"),
                         Builder::QParam("S.display_name", "name"),
                         Builder::QParam("ST.name", "stream"),
                         Builder::QParam("Q.name", "queue"),
                         Builder::QParam("K.nid", Builder::NODE_ID_SERVICE_NAME),
                         Builder::QParam("K.pid", Builder::PROCESS_ID_PUBLIC_NAME),
                         Builder::QParam("K.tid", Builder::THREAD_ID_PUBLIC_NAME),
                         Builder::QParam("AG.absolute_index", "device_index"),
                         Builder::QParam(Builder::Concat({"AG.type","AG.type_index"}), "device"),
                         Builder::QParam("AG.name", "device_name"),
                         Builder::QParam("K.start", Builder::START_SERVICE_NAME),
                         Builder::QParam("K.end", Builder::END_SERVICE_NAME),
                         Builder::QParam("(K.end-K.start)", "duration"),

                         Builder::QParam("K.grid_size_x", "GridSizeX"),
                         Builder::QParam("K.grid_size_y", "GridSizeY"),
                         Builder::QParam("K.grid_size_z", "GridSizeZ"),

                         Builder::QParam("K.workgroup_size_x", "WGSizeX"),
                         Builder::QParam("K.workgroup_size_y", "WGSizeY"),
                         Builder::QParam("K.workgroup_size_z", "WGSizeZ"),

                         Builder::QParam("K.group_segment_size", "LDSSize"),
                         Builder::QParam("K.private_segment_size", "ScratchSize"),

                         Builder::QParam("S.group_segment_size", "StaticLDSSize"),
                         Builder::QParam("S.private_segment_size", "StaticScratchSize"),

                         Builder::QParamBlank("size"),
                         Builder::QParamBlank("address"),
                         Builder::QParamBlank("level"),

                         Builder::QParamBlank("SrcIndex"),
                         Builder::QParamBlank("SrcDevice"),
                         Builder::QParamBlank("SrcName"),
                         Builder::QParamBlank("SrcAddr"),

                         Builder::QParam("K.agent_id", Builder::AGENT_ID_SERVICE_NAME),
                         Builder::QParam("K.queue_id", Builder::QUEUE_ID_SERVICE_NAME),
                         Builder::QParam("K.stream_id", Builder::STREAM_ID_SERVICE_NAME) },
                       { Builder::From("rocpd_kernel_dispatch", "K"),
                         Builder::InnerJoin("rocpd_info_agent", "AG", "AG.id = K.agent_id AND AG.guid = K.guid"),
                         Builder::InnerJoin("rocpd_event", "E", "E.id = K.event_id AND E.guid = K.guid"),
                         Builder::InnerJoin("rocpd_info_kernel_symbol", "S", "S.id = K.kernel_id AND S.guid = K.guid"),
                         Builder::LeftJoin("rocpd_info_queue", "Q", "Q.id = K.queue_id AND Q.guid = K.guid"),
                         Builder::LeftJoin("rocpd_info_stream", "ST", "ST.id = K.stream_id AND ST.guid = K.guid")
                     } })),
            },

                    &CallBackAddTrack)) break;

        ShowProgress(5, "Adding memory allocation tracks", kRPVDbBusy, future );
        if (kRocProfVisDmResultSuccess != ExecuteSQLQuery(future, 
            { // Track query by agent/queue
                 Builder::Select(rocprofvis_db_sqlite_track_query_format(
                     { { Builder::QParam("nid", Builder::NODE_ID_SERVICE_NAME),
                         Builder::QParam("agent_id", Builder::AGENT_ID_SERVICE_NAME),
                         Builder::QParam("queue_id", Builder::QUEUE_ID_SERVICE_NAME),
                         Builder::QParamCategory(kRocProfVisDmMemoryAllocationTrack) },
                       { Builder::From("rocpd_memory_allocate") } })),
                 // Track query by stream
                 Builder::Select(rocprofvis_db_sqlite_track_query_format(
                     { { Builder::QParam("nid", Builder::NODE_ID_SERVICE_NAME),
                         Builder::QParam("stream_id", Builder::STREAM_ID_SERVICE_NAME),
                         Builder::SpaceSaver(-1),
                         Builder::QParamCategory(kRocProfVisDmStreamTrack) },
                       { Builder::From("rocpd_memory_allocate") } })),
                 // Level query
                 Builder::Select(rocprofvis_db_sqlite_level_query_format(
                     { { Builder::QParamOperation(kRocProfVisDmOperationMemoryAllocate),
                         Builder::QParam("start", Builder::START_SERVICE_NAME), 
                         Builder::QParam("end", Builder::END_SERVICE_NAME),
                         Builder::QParam("id"),
                         Builder::QParam("nid", Builder::NODE_ID_SERVICE_NAME),
                         Builder::QParam("agent_id", Builder::AGENT_ID_SERVICE_NAME),
                         Builder::QParam("queue_id", Builder::QUEUE_ID_SERVICE_NAME),
                         Builder::QParam("stream_id", Builder::STREAM_ID_SERVICE_NAME) },
                       { Builder::From("rocpd_memory_allocate") } })),
                 // Slice query by queue
                 Builder::Select(rocprofvis_db_sqlite_slice_query_format(
                     { { Builder::QParamOperation(kRocProfVisDmOperationMemoryAllocate),
                         Builder::QParam("M.start", Builder::START_SERVICE_NAME), 
                         Builder::QParam("M.end", Builder::END_SERVICE_NAME),
                         Builder::QParam("E.category_id"), 
                         Builder::QParam("E.category_id"),
                         Builder::QParam("M.id"),
                         Builder::QParam("M.nid", Builder::NODE_ID_SERVICE_NAME),
                         Builder::QParam("M.agent_id", Builder::AGENT_ID_SERVICE_NAME),
                         Builder::QParam("M.queue_id", Builder::QUEUE_ID_SERVICE_NAME),
                         Builder::QParam("L.level") },
                       { Builder::From("rocpd_memory_allocate", "M"),
                         Builder::InnerJoin("rocpd_event", "E", "E.id = M.event_id AND E.guid = M.guid"),
                         Builder::LeftJoin(Builder::LevelTable("mem_alloc"), "L", "M.id = L.eid") } })),
                 // Slice query by stream
                 Builder::Select(rocprofvis_db_sqlite_slice_query_format(
                       { { Builder::QParamOperation(kRocProfVisDmOperationMemoryAllocate),
                         Builder::QParam("M.start", Builder::START_SERVICE_NAME), 
                         Builder::QParam("M.end", Builder::END_SERVICE_NAME),
                         Builder::QParam("E.category_id"), 
                         Builder::QParam("E.category_id"),
                         Builder::QParam("M.id"),
                         Builder::QParam("M.nid", Builder::NODE_ID_SERVICE_NAME),
                         Builder::QParam("M.stream_id", Builder::STREAM_ID_SERVICE_NAME),
                         Builder::SpaceSaver(-1),
                         Builder::QParam("L.level_for_stream", "level") },
                       { Builder::From("rocpd_memory_allocate", "M"),
                         Builder::InnerJoin("rocpd_event", "E", "E.id = M.event_id AND E.guid = M.guid"),
                         Builder::LeftJoin(Builder::LevelTable("mem_alloc"), "L", "M.id = L.eid") } })),
                 // Table query
                 Builder::Select(rocprofvis_db_sqlite_table_query_format(
                     { this,
                         { Builder::QParamOperation(kRocProfVisDmOperationMemoryAllocate),
                         Builder::QParam("M.id"),
                         Builder::QParam("( SELECT string FROM `rocpd_string` RS WHERE "
                                         "RS.id = E.category_id AND RS.guid = E.guid)",
                                         "category"),
                         Builder::QParam("M.type", "name"),
                         Builder::QParam("ST.name", "stream"),
                         Builder::QParam("Q.name", "queue"),
                         Builder::QParam("M.nid", Builder::NODE_ID_SERVICE_NAME),
                         Builder::QParam("M.pid", Builder::PROCESS_ID_PUBLIC_NAME),
                         Builder::QParam("M.tid", Builder::THREAD_ID_PUBLIC_NAME),
                         Builder::QParam("AG.absolute_index", "device_index"),
                         Builder::QParam(Builder::Concat({"AG.type","AG.type_index"}), "device"),
                         Builder::QParam("AG.name", "device_name"),
                         Builder::QParam("M.start", Builder::START_SERVICE_NAME),
                         Builder::QParam("M.end", Builder::END_SERVICE_NAME),
                         Builder::QParam("(M.end-M.start)", "duration"),

                         Builder::QParamBlank("GridSizeX"),
                         Builder::QParamBlank("GridSizeY"),
                         Builder::QParamBlank("GridSizeZ"),

                         Builder::QParamBlank("WGSizeX"),
                         Builder::QParamBlank("WGSizeY"),
                         Builder::QParamBlank("WGSizeZ"),

                         Builder::QParamBlank("LDSSize"),
                         Builder::QParamBlank("ScratchSize"),

                         Builder::QParamBlank("StaticLDSSize"),
                         Builder::QParamBlank("StaticScratchSize"),

                         Builder::QParam("M.size", "size"),
                         Builder::QParam("M.address", "address"),
                         Builder::QParam("M.level", "level"),

                         Builder::QParamBlank("SrcIndex"),
                         Builder::QParamBlank("SrcDevice"),
                         Builder::QParamBlank("SrcName"),
                         Builder::QParamBlank("SrcAddr"),

                         Builder::QParam("M.agent_id", Builder::AGENT_ID_SERVICE_NAME),
                         Builder::QParam("M.queue_id", Builder::QUEUE_ID_SERVICE_NAME),
                         Builder::QParam("M.stream_id", Builder::STREAM_ID_SERVICE_NAME) },
                       { Builder::From("rocpd_memory_allocate", "M"),
                         Builder::LeftJoin("rocpd_info_agent", "AG", "AG.id = M.agent_id AND AG.guid = M.guid"),
                         Builder::LeftJoin("rocpd_info_queue", "Q", "Q.id = M.queue_id AND Q.guid = M.guid"),
                         Builder::LeftJoin("rocpd_info_stream", "ST", "ST.id = M.stream_id AND ST.guid = M.guid"),
                         Builder::InnerJoin("rocpd_event", "E", "E.id = M.event_id AND E.guid = M.guid") } })),

            },
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
   
        ShowProgress(5, "Adding memory copy tracks", kRPVDbBusy, future );
        if (kRocProfVisDmResultSuccess != ExecuteSQLQuery(future, 
            {
                    // Track query by agent/queue
                    Builder::Select(rocprofvis_db_sqlite_track_query_format(
                        { { Builder::QParam("nid",          Builder::NODE_ID_SERVICE_NAME),
                            Builder::QParam("dst_agent_id", Builder::AGENT_ID_SERVICE_NAME),
                            Builder::QParam("queue_id",     Builder::QUEUE_ID_SERVICE_NAME),
                            Builder::QParamCategory(kRocProfVisDmMemoryCopyTrack) },
                          { Builder::From("rocpd_memory_copy") } })),
                    // Track query by stream
                    Builder::Select(rocprofvis_db_sqlite_track_query_format(
                        { { Builder::QParam("nid", Builder::NODE_ID_SERVICE_NAME),
                            Builder::QParam("stream_id", Builder::STREAM_ID_SERVICE_NAME),
                            Builder::SpaceSaver(-1),
                            Builder::QParamCategory(kRocProfVisDmStreamTrack) },
                        { Builder::From("rocpd_memory_copy") } })),
                    // Level query
                    Builder::Select(rocprofvis_db_sqlite_level_query_format(
                     { {    Builder::QParamOperation(kRocProfVisDmOperationMemoryCopy),
                            Builder::QParam("start", Builder::START_SERVICE_NAME), 
                            Builder::QParam("end", Builder::END_SERVICE_NAME), 
                            Builder::QParam("id"),
                            Builder::QParam("nid", Builder::NODE_ID_SERVICE_NAME),
                            Builder::QParam("dst_agent_id", Builder::AGENT_ID_SERVICE_NAME),
                            Builder::QParam("queue_id", Builder::QUEUE_ID_SERVICE_NAME),
                            Builder::QParam("stream_id", Builder::STREAM_ID_SERVICE_NAME)},
                        { Builder::From("rocpd_memory_copy") } })),
                     // Slice query by queue
                     Builder::Select(rocprofvis_db_sqlite_slice_query_format(
                         { { Builder::QParamOperation(kRocProfVisDmOperationMemoryCopy),
                             Builder::QParam("M.start", Builder::START_SERVICE_NAME), 
                             Builder::QParam("M.end", Builder::END_SERVICE_NAME),
                             Builder::QParam("E.category_id"), 
                             Builder::QParam("M.name_id"),
                             Builder::QParam("M.id"),
                             Builder::QParam("M.nid", Builder::NODE_ID_SERVICE_NAME),
                             Builder::QParam("M.dst_agent_id", Builder::AGENT_ID_SERVICE_NAME),
                             Builder::QParam("M.queue_id", Builder::QUEUE_ID_SERVICE_NAME),
                             Builder::QParam("L.level") },
                         { Builder::From("rocpd_memory_copy", "M"),
                           Builder::InnerJoin("rocpd_event", "E", "E.id = M.event_id AND E.guid = M.guid"),
                           Builder::LeftJoin(Builder::LevelTable("mem_copy"), "L", "M.id = L.eid")
                         } })),
                     // Slice query by stream
                     Builder::Select(rocprofvis_db_sqlite_slice_query_format(
                         { { Builder::QParamOperation(kRocProfVisDmOperationMemoryCopy),
                             Builder::QParam("M.start", Builder::START_SERVICE_NAME), 
                             Builder::QParam("M.end", Builder::END_SERVICE_NAME),
                             Builder::QParam("E.category_id"), 
                             Builder::QParam("M.name_id"),
                             Builder::QParam("M.id"),
                             Builder::QParam("M.nid", Builder::NODE_ID_SERVICE_NAME),
                             Builder::QParam("M.stream_id", Builder::STREAM_ID_SERVICE_NAME),
                             Builder::SpaceSaver(-1),
                             Builder::QParam("L.level_for_stream", "level") },
                           { Builder::From("rocpd_memory_copy", "M"),
                             Builder::InnerJoin("rocpd_event", "E", "E.id = M.event_id AND E.guid = M.guid"),
                             Builder::LeftJoin(Builder::LevelTable("mem_copy"), "L", "M.id = L.eid") } })),
                    // Table query
                     Builder::Select(rocprofvis_db_sqlite_table_query_format(
                         { this,
                             { Builder::QParamOperation(kRocProfVisDmOperationMemoryCopy),
                             Builder::QParam("M.id"), 
                             Builder::QParam("( SELECT string FROM `rocpd_string` RS WHERE RS.id = E.category_id AND RS.guid = E.guid)", "category"),
                             Builder::QParam("S.string", "name"),
                             Builder::QParam("ST.name", "stream"),
                             Builder::QParam("Q.name", "queue"),
                             Builder::QParam("M.nid", Builder::NODE_ID_SERVICE_NAME),
                             Builder::QParam("M.pid", Builder::PROCESS_ID_PUBLIC_NAME),
                             Builder::QParam("M.tid", Builder::THREAD_ID_PUBLIC_NAME),
                             Builder::QParam("DSTAG.absolute_index", "device_index"),
                             Builder::QParam(Builder::Concat({"DSTAG.type","DSTAG.type_index"}), "device"),
                             Builder::QParam("DSTAG.name", "device_name"),
                             Builder::QParam("M.start", Builder::START_SERVICE_NAME), 
                             Builder::QParam("M.end", Builder::END_SERVICE_NAME),
                             Builder::QParam("(M.end-M.start)", "duration"),

                             Builder::QParamBlank("GridSizeX"),
                             Builder::QParamBlank("GridSizeY"),
                             Builder::QParamBlank("GridSizeZ"),

                             Builder::QParamBlank("WGSizeX"),
                             Builder::QParamBlank("WGSizeY"),
                             Builder::QParamBlank("WGSizeZ"),

                             Builder::QParamBlank("LDSSize"),
                             Builder::QParamBlank("ScratchSize"),

                             Builder::QParamBlank("StaticLDSSize"),
                             Builder::QParamBlank("StaticScratchSize"),

                             Builder::QParam("M.size", "size"),
                             Builder::QParam("M.dst_address", "address"),
                             Builder::QParamBlank("level"),

                             Builder::QParam("SRCAG.absolute_index", "SrcIndex"),
                             Builder::QParam(Builder::Concat({"SRCAG.type","SRCAG.type_index"}), "SrcDevice"),
                             Builder::QParam("SRCAG.name", "SrcName"),
                             Builder::QParam("M.src_address", "SrcAddr"),

                             Builder::QParam("M.dst_agent_id", Builder::AGENT_ID_SERVICE_NAME),
                             Builder::QParam("M.queue_id", Builder::QUEUE_ID_SERVICE_NAME),
                             Builder::QParam("M.stream_id", Builder::STREAM_ID_SERVICE_NAME)},
                           { Builder::From("rocpd_memory_copy", "M"),
                             Builder::InnerJoin("rocpd_string", "S", "S.id = M.name_id AND S.guid = M.guid"),
                             Builder::InnerJoin("rocpd_info_agent", "DSTAG", "DSTAG.id = M.dst_agent_id AND DSTAG.guid = M.guid"),
                             Builder::InnerJoin("rocpd_info_agent", "SRCAG", "SRCAG.id = M.src_agent_id AND SRCAG.guid = M.guid"),
                             Builder::LeftJoin("rocpd_info_queue", "Q", "Q.id = M.queue_id AND Q.guid = M.guid"),
                             Builder::LeftJoin("rocpd_info_stream", "ST", "ST.id = M.stream_id AND ST.guid = M.guid"),
                             Builder::InnerJoin("rocpd_event", "E", "E.id = M.event_id AND E.guid = M.guid")
                         } })),

            },
                    &CallBackAddTrack)) break;
        
        // PMC schema is not fully defined yet
        ShowProgress(5, "Adding performance counters tracks", kRPVDbBusy, future );
        if (kRocProfVisDmResultSuccess != ExecuteSQLQuery(future,   
            {
                    // Track query by agent/queue
                     Builder::Select(rocprofvis_db_sqlite_track_query_format(
                     { { Builder::QParam("K.nid", Builder::NODE_ID_SERVICE_NAME),
                         Builder::QParam("K.agent_id", Builder::AGENT_ID_SERVICE_NAME),
                         Builder::QParam("PMC_I.id", Builder::COUNTER_ID_SERVICE_NAME),
                         Builder::QParamCategory(kRocProfVisDmPmcTrack) },
                       { Builder::From("rocpd_pmc_event", "PMC_E"),
                         Builder::InnerJoin("rocpd_info_pmc", "PMC_I", "PMC_I.id = PMC_E.pmc_id AND PMC_I.guid = PMC_E.guid"),
                         Builder::InnerJoin("rocpd_kernel_dispatch", "K", "K.event_id = PMC_E.event_id AND K.guid = PMC_E.guid")
                         } })),
                    // Track query by stream
                    "",
                    // Level query, for COUNT only
                    Builder::Select(rocprofvis_db_sqlite_level_query_format(
                     { { Builder::QParamOperation(kRocProfVisDmOperationNoOp),
                         Builder::QParam("K.start", Builder::START_SERVICE_NAME), 
                         Builder::QParam("K.end", Builder::END_SERVICE_NAME),
                         Builder::SpaceSaver(0), 
                         Builder::QParam("K.nid", Builder::NODE_ID_SERVICE_NAME),
                         Builder::QParam("K.agent_id", Builder::AGENT_ID_SERVICE_NAME),
                         Builder::QParam("PMC_I.id", Builder::COUNTER_ID_SERVICE_NAME),
                         Builder::SpaceSaver(0)
                       },
                       { Builder::From("rocpd_pmc_event", "PMC_E"),
                         Builder::InnerJoin("rocpd_info_pmc", "PMC_I", "PMC_I.id = PMC_E.pmc_id AND PMC_I.guid = PMC_E.guid"),
                         Builder::InnerJoin("rocpd_kernel_dispatch", "K", "K.event_id = PMC_E.event_id AND K.guid = PMC_E.guid")
                        } })),
                    // Slice query by queue
                    Builder::Select(rocprofvis_db_sqlite_slice_query_format(
                     { { Builder::QParamOperation(kRocProfVisDmOperationNoOp),
                         Builder::QParam("K.start", Builder::START_SERVICE_NAME), 
                         Builder::QParam("PMC_E.value", Builder::COUNTER_VALUE_SERVICE_NAME),
                         Builder::QParam("K.end", Builder::END_SERVICE_NAME),
                         Builder::SpaceSaver(0),
                         Builder::SpaceSaver(0),
                         Builder::QParam("K.nid", Builder::NODE_ID_SERVICE_NAME),
                         Builder::QParam("K.agent_id", Builder::AGENT_ID_SERVICE_NAME),
                         Builder::QParam("PMC_I.id", Builder::COUNTER_ID_SERVICE_NAME),
                         Builder::QParam("PMC_E.value", "level"),                        
                       },
                       { Builder::From("rocpd_pmc_event", "PMC_E"),
                         Builder::InnerJoin("rocpd_info_pmc", "PMC_I", "PMC_I.id = PMC_E.pmc_id AND PMC_I.guid = PMC_E.guid"),
                         Builder::InnerJoin("rocpd_kernel_dispatch", "K", "K.event_id = PMC_E.event_id AND K.guid = PMC_E.guid")
                        } })),
                     "",
                    // Table query
                    Builder::Select(rocprofvis_db_sqlite_sample_table_query_format(
                     { { Builder::QParamOperation(kRocProfVisDmOperationNoOp),
                         Builder::QParam("K.start", Builder::START_SERVICE_NAME), 
                         Builder::QParam("K.end", Builder::END_SERVICE_NAME),
                         Builder::QParam("K.nid", Builder::NODE_ID_SERVICE_NAME),
                         Builder::QParam("K.agent_id", Builder::AGENT_ID_SERVICE_NAME),
                         Builder::QParam("PMC_I.id", Builder::COUNTER_ID_SERVICE_NAME),
                         Builder::QParam("PMC_E.value", Builder::COUNTER_VALUE_SERVICE_NAME)
                       },
                       { Builder::From("rocpd_pmc_event", "PMC_E"),
                         Builder::InnerJoin("rocpd_info_pmc", "PMC_I", "PMC_I.id = PMC_E.pmc_id AND PMC_I.guid = PMC_E.guid"),
                         Builder::InnerJoin("rocpd_kernel_dispatch", "K", "K.event_id = PMC_E.event_id AND K.guid = PMC_E.guid")
                        } })),

            },
                    &CallBackAddTrack)) break;
                                                                    

        ShowProgress(20, "Loading strings", kRPVDbBusy, future );
        BindObject()->FuncAddString(BindObject()->trace_object, ""); // 0 index string
        if (kRocProfVisDmResultSuccess != ExecuteSQLQuery(future, "SELECT string FROM rocpd_string ORDER BY id;", &CallBackAddString)) break;
        if (kRocProfVisDmResultSuccess != ExecuteSQLQuery(future,"SELECT COUNT(*) FROM rocpd_string;", &CallbackGetValue, m_symbols_offset)) break;
        if (kRocProfVisDmResultSuccess != ExecuteSQLQuery(future, "SELECT display_name FROM rocpd_info_kernel_symbol;", &CallBackAddString)) break;

        ShowProgress(5, "Counting events",
                     kRPVDbBusy, future);
        TraceProperties()->events_count[kRocProfVisDmOperationLaunch]   = 0;
        TraceProperties()->events_count[kRocProfVisDmOperationDispatch] = 0;
        TraceProperties()->events_count[kRocProfVisDmOperationMemoryAllocate] = 0;
        TraceProperties()->events_count[kRocProfVisDmOperationMemoryCopy]     = 0;

        if(kRocProfVisDmResultSuccess !=
           ExecuteQueryForAllTracksAsync(
               kRocProfVisDmIncludePmcTracks | kRocProfVisDmIncludeStreamTracks,
               kRPVQueryLevel,
               "SELECT COUNT(*), op, ",
               "", &CallbackGetTrackRecordsCount,
               [](rocprofvis_dm_track_params_t* params) {}))
        {
            break;
        }

        std::vector<std::string> to_drop = Builder::OldLevelTables("launch");
        for (auto table : to_drop)
        {
            DropSQLTable(table.c_str());
        }
        to_drop = Builder::OldLevelTables("dispatch");
        for(auto table : to_drop)
        {
            DropSQLTable(table.c_str());
        }
        to_drop = Builder::OldLevelTables("mem_alloc");
        for(auto table : to_drop)
        {
            DropSQLTable(table.c_str());
        }
        to_drop = Builder::OldLevelTables("mem_copy");
        for(auto table : to_drop)
        {
            DropSQLTable(table.c_str());
        }


        if(SQLITE_OK != DetectTable(GetServiceConnection(), Builder::LevelTable("launch").c_str(), false) ||
           SQLITE_OK != DetectTable(GetServiceConnection(), Builder::LevelTable("dispatch").c_str(), false) ||
           SQLITE_OK != DetectTable(GetServiceConnection(), Builder::LevelTable("mem_alloc").c_str(), false) ||
           SQLITE_OK != DetectTable(GetServiceConnection(), Builder::LevelTable("mem_copy").c_str(), false))
        {
            m_event_levels[kRocProfVisDmOperationLaunch].reserve(
                TraceProperties()->events_count[kRocProfVisDmOperationLaunch]);
            m_event_levels[kRocProfVisDmOperationDispatch].reserve(
                TraceProperties()->events_count[kRocProfVisDmOperationDispatch]);
            m_event_levels[kRocProfVisDmOperationMemoryAllocate].reserve(
                TraceProperties()->events_count[kRocProfVisDmOperationMemoryAllocate]);
            m_event_levels[kRocProfVisDmOperationMemoryCopy].reserve(
                TraceProperties()->events_count[kRocProfVisDmOperationMemoryCopy]);

             ShowProgress(10, "Calculating event levels (once per database)", kRPVDbBusy, future);
            if(kRocProfVisDmResultSuccess !=
               ExecuteQueryForAllTracksAsync(
                   kRocProfVisDmIncludeStreamTracks, 
                   kRPVQueryLevel, "SELECT *, ", (std::string(" ORDER BY ")+Builder::START_SERVICE_NAME).c_str(), &CalculateEventLevels,
                   [](rocprofvis_dm_track_params_t* params) {
                       params->m_active_events.clear();
                   }))
            {
                break;
            }

            for (int j = 0; j < kRocProfVisDmNumOperation; j++)
            {
                if(m_event_levels[j].size() > 0)
                {
                    std::sort(m_event_levels[j].begin(),
                                m_event_levels[j].end(),
                                [](const rocprofvis_db_event_level_t& a,
                                    const rocprofvis_db_event_level_t& b) {
                                    return a.id < b.id;
                                });
                }
            }

            SQLInsertParams params[] = { { "eid", "INTEGER PRIMARY KEY" }, { "level", "INTEGER" } , { "level_for_stream", "INTEGER" }};
            CreateSQLTable(
                Builder::LevelTable("launch").c_str(), params, 3,
                m_event_levels[kRocProfVisDmOperationLaunch].size(),
                [&](sqlite3_stmt* stmt, int index) {
                    sqlite3_bind_int64(
                        stmt, 1, m_event_levels[kRocProfVisDmOperationLaunch][index].id);
                    sqlite3_bind_int(
                        stmt, 2,
                        m_event_levels[kRocProfVisDmOperationLaunch][index].level_for_queue);
                    sqlite3_bind_int(
                        stmt, 3, 0);
                });
            m_event_levels[kRocProfVisDmOperationLaunch].clear();
            m_event_levels_id_to_index[kRocProfVisDmOperationLaunch].clear();
            CreateSQLTable(
                Builder::LevelTable("dispatch").c_str(), params, 3,
                m_event_levels[kRocProfVisDmOperationDispatch].size(),
                [&](sqlite3_stmt* stmt, int index) {
                    sqlite3_bind_int64(
                        stmt, 1,
                        m_event_levels[kRocProfVisDmOperationDispatch][index].id);
                    sqlite3_bind_int(
                        stmt, 2,
                        m_event_levels[kRocProfVisDmOperationDispatch][index].level_for_queue);
                    sqlite3_bind_int(
                        stmt, 3,
                        m_event_levels[kRocProfVisDmOperationDispatch][index].level_for_stream);
                });
            m_event_levels[kRocProfVisDmOperationDispatch].clear();
            m_event_levels_id_to_index[kRocProfVisDmOperationDispatch].clear();
            CreateSQLTable(
                Builder::LevelTable("mem_alloc").c_str(), params, 3,
                m_event_levels[kRocProfVisDmOperationMemoryAllocate].size(),
                [&](sqlite3_stmt* stmt, int index) {
                    sqlite3_bind_int64(
                        stmt, 1,
                        m_event_levels[kRocProfVisDmOperationMemoryAllocate][index].id);
                    sqlite3_bind_int(
                        stmt, 2,
                        m_event_levels[kRocProfVisDmOperationMemoryAllocate][index].level_for_queue);
                    sqlite3_bind_int(
                        stmt, 3,
                        m_event_levels[kRocProfVisDmOperationMemoryAllocate][index].level_for_stream);
                });
            m_event_levels[kRocProfVisDmOperationMemoryAllocate].clear();
            m_event_levels_id_to_index[kRocProfVisDmOperationMemoryAllocate].clear();
            CreateSQLTable(
                Builder::LevelTable("mem_copy").c_str(), params, 3,
                m_event_levels[kRocProfVisDmOperationMemoryCopy].size(),
                [&](sqlite3_stmt* stmt, int index) {
                    sqlite3_bind_int64(
                        stmt, 1,
                        m_event_levels[kRocProfVisDmOperationMemoryCopy][index].id);
                    sqlite3_bind_int(
                        stmt, 2,
                        m_event_levels[kRocProfVisDmOperationMemoryCopy][index].level_for_queue);
                    sqlite3_bind_int(
                        stmt, 3,
                        m_event_levels[kRocProfVisDmOperationMemoryCopy][index].level_for_stream);
                });
            m_event_levels[kRocProfVisDmOperationMemoryCopy].clear();
            m_event_levels_id_to_index[kRocProfVisDmOperationMemoryCopy].clear();
        }

        ShowProgress(5, "Collecting track properties",
                     kRPVDbBusy, future);
        TraceProperties()->start_time = UINT64_MAX;
        TraceProperties()->end_time   = 0;

        if(kRocProfVisDmResultSuccess !=
           ExecuteQueryForAllTracksAsync(
               kRocProfVisDmIncludePmcTracks | kRocProfVisDmIncludeStreamTracks, kRPVQuerySliceByTrackSliceQuery,
               "SELECT MIN(startTs), MAX(endTs), MIN(level), MAX(level), ",
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
    return future->SetPromise(future->Interrupted() ? kRocProfVisDmResultDbAbort : kRocProfVisDmResultDbAccessFailed);
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
        std::string query;
        if (event_id.bitfield.event_op == kRocProfVisDmOperationLaunch)
        {
            query = Builder::SelectAll(
            Builder::Select(rocprofvis_db_sqlite_dataflow_query_format(
                { {
                      Builder::QParamOperation(kRocProfVisDmOperationLaunch),
                      Builder::QParam("E2.id", "id"),
                      Builder::QParam("R2.id"),
                      Builder::QParam("R2.nid", Builder::AGENT_ID_SERVICE_NAME),
                      Builder::QParam("R2.pid", Builder::PROCESS_ID_SERVICE_NAME),
                      Builder::QParam("R2.tid", Builder::THREAD_ID_SERVICE_NAME),
                      Builder::QParam("R2.start"),
                      Builder::QParam("E2.category_id"),
                      Builder::QParam("R2.name_id"),
                      Builder::QParam("L.level"),
                  },
                  { Builder::From("rocpd_region", "R1"),
                    Builder::InnerJoin("rocpd_event", "E1",
                                       "R1.event_id = E1.id AND R1.guid = E1.guid"),
                    Builder::InnerJoin("rocpd_event", "E2",
                                       "E1.stack_id = E2.stack_id AND E1.id != E2.id AND E1.guid = E2.guid"),
                    Builder::InnerJoin("rocpd_region", "R2",
                                       "R2.event_id = E2.id AND R2.guid = E2.guid"),
                    Builder::InnerJoin(Builder::LevelTable("launch"), "L", 
                                       "R2.id = L.eid") },
                  { Builder::Where(
                      "R1.id", "==", std::to_string(event_id.bitfield.event_id)) } })) + 
            Builder::Union() +
            Builder::Select(rocprofvis_db_sqlite_dataflow_query_format(
                { {
                      Builder::QParamOperation(kRocProfVisDmOperationDispatch),
                      Builder::QParam("E2.id", "id"),
                      Builder::QParam("K.id"),
                      Builder::QParam("K.nid", Builder::AGENT_ID_SERVICE_NAME),
                      Builder::QParam("K.agent_id", Builder::AGENT_ID_SERVICE_NAME),
                      Builder::QParam("K.queue_id", Builder::QUEUE_ID_SERVICE_NAME),
                      Builder::QParam("K.start"),
                      Builder::QParam("E2.category_id"),
                      Builder::QParam("K.kernel_id"),
                      Builder::QParam("L.level"),
                  },
                  { Builder::From("rocpd_region", "R"),
                    Builder::InnerJoin("rocpd_event", "E1",
                                       "R.event_id = E1.id AND R.guid = E1.guid"),
                    Builder::InnerJoin("rocpd_event", "E2",
                                       "E1.stack_id = E2.stack_id AND E1.id != E2.id AND E1.guid = E2.guid"),
                    Builder::InnerJoin("rocpd_kernel_dispatch", "K",
                                       "K.event_id = E2.id AND K.guid = E2.guid"),
                    Builder::InnerJoin(Builder::LevelTable("dispatch"), "L", 
                                       "K.id = L.eid")
                },
                  { Builder::Where(
                      "R.id", "==", std::to_string(event_id.bitfield.event_id)) } })) + 
            Builder::Union() +
            Builder::Select(rocprofvis_db_sqlite_dataflow_query_format(
                { {
                      Builder::QParamOperation(kRocProfVisDmOperationMemoryCopy),
                      Builder::QParam("E2.id", "id"),
                      Builder::QParam("M.id"),
                      Builder::QParam("M.nid", Builder::AGENT_ID_SERVICE_NAME),
                      Builder::QParam("M.dst_agent_id", Builder::AGENT_ID_SERVICE_NAME),
                      Builder::QParam("M.queue_id", Builder::QUEUE_ID_SERVICE_NAME),
                      Builder::QParam("M.start"),
                      Builder::QParam("E2.category_id"),
                      Builder::QParam("M.name_id"),
                      Builder::QParam("L.level"),
                  },
                  { Builder::From("rocpd_region", "R"),
                    Builder::InnerJoin("rocpd_event", "E1",
                                       "R.event_id = E1.id AND R.guid = E1.guid"),
                    Builder::InnerJoin("rocpd_event", "E2",
                                       "E1.stack_id = E2.stack_id AND E1.id != E2.id AND E1.guid = E2.guid"),
                    Builder::InnerJoin("rocpd_memory_copy", "M",
                                       "M.event_id = E2.id AND M.guid = E2.guid"),
                    Builder::InnerJoin(Builder::LevelTable("mem_copy"), "L", 
                                       "M.id = L.eid") },
                  { Builder::Where(
                      "R.id", "==", std::to_string(event_id.bitfield.event_id)) } })) +
            Builder::Union() +
            Builder::Select(rocprofvis_db_sqlite_dataflow_query_format(
                { {
                      Builder::QParamOperation(kRocProfVisDmOperationMemoryAllocate),
                      Builder::QParam("E2.id", "id"),
                      Builder::QParam("M.id"),
                      Builder::QParam("M.nid", Builder::AGENT_ID_SERVICE_NAME),
                      Builder::QParam("M.agent_id", Builder::AGENT_ID_SERVICE_NAME),
                      Builder::QParam("M.queue_id", Builder::QUEUE_ID_SERVICE_NAME),
                      Builder::QParam("M.start"),
                      Builder::QParam("E2.category_id"),
                      Builder::QParam("E2.category_id"), // This should be name_id, but Alloc table does not have column
                      Builder::QParam("L.level"),
                  },
                  { Builder::From("rocpd_region", "R"),
                    Builder::InnerJoin("rocpd_event", "E1",
                                       "R.event_id = E1.id AND R.guid = E1.guid"),
                    Builder::InnerJoin("rocpd_event", "E2",
                                       "E1.stack_id = E2.stack_id AND E1.id != E2.id AND E1.guid = E2.guid"),
                    Builder::InnerJoin("rocpd_memory_allocate", "M",
                                       "M.event_id = E2.id AND M.guid = E2.guid"),
                    Builder::InnerJoin(Builder::LevelTable("mem_alloc"), "L", 
                                       "M.id = L.eid") },
                  { Builder::Where(
                      "R.id", "==", std::to_string(event_id.bitfield.event_id)) } }))
                   );
            ShowProgress(0, query.c_str(),kRPVDbBusy, future);
            if (kRocProfVisDmResultSuccess != ExecuteSQLQuery(future, query.c_str(), flowtrace, &CallbackAddFlowTrace)) break;
        } else
        if (event_id.bitfield.event_op == kRocProfVisDmOperationDispatch)
        {
            query = Builder::SelectAll(
            Builder::Select(rocprofvis_db_sqlite_dataflow_query_format(
                { {
                      Builder::QParamOperation(kRocProfVisDmOperationLaunch),
                      Builder::QParam("E2.id", "id"),
                      Builder::QParam("R.id"),
                      Builder::QParam("R.nid", Builder::AGENT_ID_SERVICE_NAME),
                      Builder::QParam("R.pid", Builder::PROCESS_ID_SERVICE_NAME),
                      Builder::QParam("R.tid", Builder::THREAD_ID_SERVICE_NAME),
                      Builder::QParam("R.start"),
                      Builder::QParam("E2.category_id"),
                      Builder::QParam("R.name_id"),
                      Builder::QParam("L.level"),
                  },
                  { Builder::From("rocpd_kernel_dispatch", "K"),
                    Builder::InnerJoin("rocpd_event", "E1",
                                       "K.event_id = E1.id AND K.guid = E1.guid"),
                    Builder::InnerJoin("rocpd_event", "E2",
                                       "E1.stack_id = E2.stack_id AND E1.id != E2.id AND E1.guid = E2.guid"),
                    Builder::InnerJoin("rocpd_region", "R",
                                       "R.event_id = E2.id AND R.guid = E2.guid"),
                    Builder::InnerJoin(Builder::LevelTable("launch"), "L", 
                                       "R.id = L.eid") },
                  { Builder::Where(
                      "K.id", "==", std::to_string(event_id.bitfield.event_id)) } })) +
            Builder::Union() +
            Builder::Select(rocprofvis_db_sqlite_dataflow_query_format(
                { {
                      Builder::QParamOperation(kRocProfVisDmOperationDispatch),
                      Builder::QParam("E2.id", "id"),
                      Builder::QParam("K2.id"),
                      Builder::QParam("K2.nid", Builder::AGENT_ID_SERVICE_NAME),
                      Builder::QParam("K2.agent_id", Builder::AGENT_ID_SERVICE_NAME),
                      Builder::QParam("K2.queue_id", Builder::QUEUE_ID_SERVICE_NAME),
                      Builder::QParam("K2.start"),
                      Builder::QParam("E2.category_id"),
                      Builder::QParam("K2.kernel_id"),
                      Builder::QParam("L.level"),
                  },
                  { Builder::From("rocpd_kernel_dispatch", "K1"),
                    Builder::InnerJoin("rocpd_event", "E1",
                                       "K1.event_id = E1.id AND K1.guid = E1.guid"),
                    Builder::InnerJoin("rocpd_event", "E2",
                                       "E1.stack_id = E2.stack_id AND E1.id != E2.id AND E1.guid = E2.guid"),
                    Builder::InnerJoin("rocpd_kernel_dispatch", "K2",
                                       "K2.event_id = E2.id AND K2.guid = E2.guid"),
                    Builder::InnerJoin(Builder::LevelTable("dispatch"), "L", 
                                       "K2.id = L.eid")
                },
                  { Builder::Where(
                      "K1.id", "==", std::to_string(event_id.bitfield.event_id)) } }))
            );
            ShowProgress(0, query.c_str(),kRPVDbBusy, future);
            if (kRocProfVisDmResultSuccess != ExecuteSQLQuery(future, query.c_str(), flowtrace, &CallbackAddFlowTrace)) break;
        } else
        if (event_id.bitfield.event_op == kRocProfVisDmOperationMemoryCopy)
        {
            query = Builder::SelectAll(
            Builder::Select(rocprofvis_db_sqlite_dataflow_query_format(
                { {
                        Builder::QParamOperation(kRocProfVisDmOperationLaunch),
                        Builder::QParam("E2.id", "id"),
                        Builder::QParam("R.id"),
                        Builder::QParam("R.nid", Builder::AGENT_ID_SERVICE_NAME),
                        Builder::QParam("R.pid", Builder::PROCESS_ID_SERVICE_NAME),
                        Builder::QParam("R.tid", Builder::THREAD_ID_SERVICE_NAME),
                        Builder::QParam("R.start"),
                        Builder::QParam("E2.category_id"),
                        Builder::QParam("R.name_id"),
                        Builder::QParam("L.level"),
                    },
                    { Builder::From("rocpd_memory_copy", "M"),
                    Builder::InnerJoin("rocpd_event", "E1",
                                        "M.event_id = E1.id AND M.guid = E1.guid"),
                    Builder::InnerJoin("rocpd_event", "E2",
                                        "E1.stack_id = E2.stack_id AND E1.id != E2.id AND E1.guid = E2.guid"),
                    Builder::InnerJoin("rocpd_region", "R",
                                        "R.event_id = E2.id AND R.guid = E2.guid"),
                    Builder::InnerJoin(Builder::LevelTable("launch"), "L", 
                                        "R.id = L.eid") },
                    { Builder::Where("M.id", "==",
                                    std::to_string(event_id.bitfield.event_id)) } })) +
            Builder::Union() +
            Builder::Select(rocprofvis_db_sqlite_dataflow_query_format(
            { {
                    Builder::QParamOperation(kRocProfVisDmOperationMemoryCopy),
                    Builder::QParam("E2.id", "id"),
                    Builder::QParam("M2.id"),
                    Builder::QParam("M2.nid", Builder::AGENT_ID_SERVICE_NAME),
                    Builder::QParam("M2.dst_agent_id", Builder::AGENT_ID_SERVICE_NAME),
                    Builder::QParam("M2.queue_id", Builder::QUEUE_ID_SERVICE_NAME),
                    Builder::QParam("M2.start"),
                    Builder::QParam("E2.category_id"),
                    Builder::QParam("M2.name_id"),
                    Builder::QParam("L.level"),
                },
                { Builder::From("rocpd_memory_copy", "M1"),
                Builder::InnerJoin("rocpd_event", "E1",
                                    "M1.event_id = E1.id AND M1.guid = E1.guid"),
                Builder::InnerJoin("rocpd_event", "E2",
                                    "E1.stack_id = E2.stack_id AND E1.id != E2.id AND E1.guid = E2.guid"),
                Builder::InnerJoin("rocpd_memory_copy", "M2",
                                    "M2.event_id = E2.id AND M2.guid = E2.guid"),
                Builder::InnerJoin(Builder::LevelTable("mem_copy"), "L", 
                                    "M2.id = L.eid") },
                { Builder::Where(
                    "M1.id", "==", std::to_string(event_id.bitfield.event_id)) } }))
            );
            ShowProgress(0, query.c_str(), kRPVDbBusy, future);
            if (kRocProfVisDmResultSuccess != ExecuteSQLQuery(future, query.c_str(), flowtrace, &CallbackAddFlowTrace)) break;
        }
        else
        if (event_id.bitfield.event_op == kRocProfVisDmOperationMemoryAllocate)
        {
            query = Builder::SelectAll(
                Builder::Select(rocprofvis_db_sqlite_dataflow_query_format(
                    { {
                          Builder::QParamOperation(kRocProfVisDmOperationLaunch),
                          Builder::QParam("E2.id", "id"),
                          Builder::QParam("R.id"),
                          Builder::QParam("R.nid", Builder::AGENT_ID_SERVICE_NAME),
                          Builder::QParam("R.pid", Builder::PROCESS_ID_SERVICE_NAME),
                          Builder::QParam("R.tid", Builder::THREAD_ID_SERVICE_NAME),
                          Builder::QParam("R.start"),
                          Builder::QParam("E2.category_id"),
                          Builder::QParam("R.name_id"),
                          Builder::QParam("L.level"),
                      },
                      { Builder::From("rocpd_memory_allocate", "M"),
                        Builder::InnerJoin("rocpd_event", "E1",
                                           "M.event_id = E1.id AND M.guid = E1.guid"),
                        Builder::InnerJoin("rocpd_event", "E2",
                                           "E1.stack_id = E2.stack_id AND E1.id != E2.id AND E1.guid = E2.guid"),
                        Builder::InnerJoin("rocpd_region", "R",
                                           "R.event_id = E2.id AND R.guid = E2.guid"),
                        Builder::InnerJoin(Builder::LevelTable("launch"), "L", 
                                           "R.id = L.eid") },
                      { Builder::Where("M.id", "==",
                                       std::to_string(event_id.bitfield.event_id)) } })));
            //query <<    "SELECT E.id as id, 1, E.correlation_id, R.nid, R.pid, R.tid, R.end "
            //            "FROM _rocpd_memory_allocate MA "
            //            "INNER JOIN rocpd_event E ON MA.event_id = E.id "
            //            "INNER JOIN rocpd_region R ON R.id = E.correlation_id "
            //            "WHERE MA.id == ";
            //query << event_id.bitfield.event_id << ";";

            ShowProgress(0, query.c_str(), kRPVDbBusy, future);
            if (kRocProfVisDmResultSuccess != ExecuteSQLQuery(future, query.c_str(), flowtrace, &CallbackAddFlowTrace)) break;
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
    return future->SetPromise(future->Interrupted() ? kRocProfVisDmResultDbAbort : kRocProfVisDmResultDbAccessFailed);
}

rocprofvis_dm_result_t RocprofDatabase::SaveTrimmedData(rocprofvis_dm_timestamp_t start,
    rocprofvis_dm_timestamp_t end,
                                 rocprofvis_dm_charptr_t new_db_path, Future* future)
{
    ROCPROFVIS_ASSERT_MSG_RETURN(new_db_path, "New DB path cannot be NULL.",
                                 kRocProfVisDmResultInvalidParameter);
    ROCPROFVIS_ASSERT_MSG_RETURN(future, ERROR_FUTURE_CANNOT_BE_NULL,
                                 kRocProfVisDmResultInvalidParameter);
    rocprofvis_dm_result_t result = kRocProfVisDmResultInvalidParameter;

    std::string query;
    rocprofvis_db_sqlite_trim_parameters trim_tables;
    rocprofvis_db_sqlite_trim_parameters trim_views;

    Future* internal_future = new Future(nullptr);

    {
        RocprofDatabase rpDb(new_db_path);
        result = rpDb.Open();

        if(result == kRocProfVisDmResultSuccess)
        {
            ShowProgress(1, "Iterate over tables", kRPVDbBusy, future);
            query = "SELECT name, sql FROM sqlite_master WHERE type='table';";
            rocprofvis_db_sqlite_callback_parameters params = {
                this,
                internal_future,
                &trim_tables,
                &CallbackTrimTableQuery,
                { query.c_str(), "", "", "" },
                static_cast<rocprofvis_dm_track_id_t>(-1)
            };

            result = ExecuteSQLQuery(query.c_str(), &params);
        }

        if(result == kRocProfVisDmResultSuccess)
        {
            for(auto const& table : trim_tables.tables)
            {
                if(table.first != "sqlite_master" && table.first != "sqlite_sequence")
                {
                    if(result == kRocProfVisDmResultSuccess)
                    {
                        std::string msg = "Copy table " + table.first;
                        ShowProgress(1, msg.c_str(), kRPVDbBusy, future);

                        query = "DROP TABLE ";
                        query += table.first;
                        query += ";";
                        rpDb.ExecuteSQLQuery(internal_future, query.c_str());

                        query = table.second;
                        result = rpDb.ExecuteSQLQuery(internal_future, query.c_str());
                    }
                    else
                    {
                        break;
                    }
                }
            }
        }

        if(result == kRocProfVisDmResultSuccess)
        {
            ShowProgress(1, "Iterate over views", kRPVDbBusy, future);
            query = "SELECT name, sql FROM sqlite_master WHERE type='view';";
            rocprofvis_db_sqlite_callback_parameters params = {
                this,
                internal_future,
                &trim_views,
                &CallbackTrimTableQuery,
                { query.c_str(), "", "", "" },
                static_cast<rocprofvis_dm_track_id_t>(-1)
            };

            result = ExecuteSQLQuery(query.c_str(), &params);
        }

        if(result == kRocProfVisDmResultSuccess)
        {
            for(auto const& view : trim_views.tables)
            {
                if(result == kRocProfVisDmResultSuccess)
                {
                    std::string msg = "Copy views " + view.first;
                    ShowProgress(1, msg.c_str(), kRPVDbBusy, future);

                    query = "DROP VIEW ";
                    query += view.first;
                    query += ";";
                    rpDb.ExecuteSQLQuery(internal_future, query.c_str());

                    query = view.second;

                    result = rpDb.ExecuteSQLQuery(internal_future, query.c_str());
                }
                else
                {
                    break;
                }
            }
        }

        if(result == kRocProfVisDmResultSuccess)
        {
            ShowProgress(0, "Attaching old DB to new", kRPVDbBusy, future);
            query = "ATTACH DATABASE '";
            query += Path();
            query += "' as 'oldDb';";
            result = rpDb.ExecuteSQLQuery(internal_future, query.c_str());
        }

        if(result == kRocProfVisDmResultSuccess)
        {
            for(auto const& table : trim_tables.tables)
            {
                if(table.first != "sqlite_master" && table.first != "sqlite_sequence")
                {
                    if(result == kRocProfVisDmResultSuccess)
                    {
                        std::string msg = "Copy table " + table.first;
                        ShowProgress(1, msg.c_str(), kRPVDbBusy, future);

                        if(result == kRocProfVisDmResultSuccess)
                        {
                            if(strstr(table.first.c_str(), "rocpd_kernel_dispatch") ||
                               strstr(table.first.c_str(), "rocpd_memory_allocate") ||
                               strstr(table.first.c_str(), "rocpd_memory_copy") ||
                               strstr(table.first.c_str(), "rocpd_region"))
                            {
                                query = "INSERT INTO ";
                                query += table.first;
                                query += " SELECT * FROM oldDb.";
                                query += table.first;
                                query += " WHERE start < ";
                                query += std::to_string(end);
                                query += " AND end > ";
                                query += std::to_string(start);
                                query += ";";
                            }
                            else
                            {
                                query = "INSERT INTO ";
                                query += table.first;
                                query += " SELECT * FROM oldDb.";
                                query += table.first;
                                query += ";";
                            }

                            result = rpDb.ExecuteSQLQuery(internal_future, query.c_str());
                        }
                        else
                        {
                            break;
                        }
                    }
                    else
                    {
                        break;
                    }
                }
            }
        }

        if(result == kRocProfVisDmResultSuccess)
        {
            ShowProgress(0, "Detaching old DB", kRPVDbBusy, future);
            query  = "DETACH oldDb;";
            result = rpDb.ExecuteSQLQuery(internal_future, query.c_str());
        }
    }

    if (result == kRocProfVisDmResultSuccess)
    {
        ShowProgress(100, "Trace trimmed successfully!", kRPVDbSuccess, future);        
    }
    else
    {
        ShowProgress(0, "Failed to trim track!", kRPVDbError, future);
    }

    delete internal_future;

    return future->SetPromise(result);
}

rocprofvis_dm_result_t  RocprofDatabase::ReadStackTraceInfo(
        rocprofvis_dm_event_id_t event_id,
        Future* future)
{
    ROCPROFVIS_ASSERT_MSG_RETURN(future, ERROR_FUTURE_CANNOT_BE_NULL,
                                 kRocProfVisDmResultInvalidParameter);
    while(true)
    {
        ROCPROFVIS_ASSERT_MSG_BREAK(BindObject()->trace_properties,
                                    ERROR_TRACE_PROPERTIES_CANNOT_BE_NULL);
        ROCPROFVIS_ASSERT_MSG_BREAK(BindObject()->trace_properties->metadata_loaded,
                                    ERROR_METADATA_IS_NOT_LOADED);
        rocprofvis_dm_stacktrace_t stacktrace =
            BindObject()->FuncAddStackTrace(BindObject()->trace_object, event_id);
        ROCPROFVIS_ASSERT_MSG_BREAK(stacktrace, ERROR_EXT_DATA_CANNOT_BE_NULL);
        std::stringstream query;
        if(event_id.bitfield.event_op == kRocProfVisDmOperationLaunch)
        {
            query << "select call_stack, line_info from regions where id == ";
            query << event_id.bitfield.event_id << ";";
            ShowProgress(0, query.str().c_str(), kRPVDbBusy, future);
            if(kRocProfVisDmResultSuccess != ExecuteSQLQuery(future, query.str().c_str(),
                                                             stacktrace,
                                                             &CallbackAddStackTrace))
            {
                break;
            }

        }
        else
        {
            ShowProgress(0, "Stack trace is not available for specified operation type!",
                         kRPVDbError, future);
            return future->SetPromise(kRocProfVisDmResultInvalidParameter);
        }

        ShowProgress(100, "Extended data successfully loaded!", kRPVDbSuccess, future);
        return future->SetPromise(kRocProfVisDmResultSuccess);
    }
    ShowProgress(0, "Extended data  not loaded!", kRPVDbError, future);
    return future->SetPromise(future->Interrupted() ? kRocProfVisDmResultTimeout
                                                    : kRocProfVisDmResultDbAccessFailed);
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
        std::string query;
        if (event_id.bitfield.event_op == kRocProfVisDmOperationLaunch)
        {
            query = "select * from regions where id == ";
            query += std::to_string(event_id.bitfield.event_id);
            ShowProgress(0, query.c_str(),kRPVDbBusy, future);
            if(kRocProfVisDmResultSuccess !=
               ExecuteSQLQuery(
                   future, query.c_str(), "Properties", extdata,
                   (rocprofvis_dm_event_operation_t) event_id.bitfield.event_op,
                   &CallbackAddExtInfo))
                break;
            query = Builder::Select(rocprofvis_db_sqlite_essential_data_query_format(
                { { Builder::QParamOperation(kRocProfVisDmOperationLaunch),
                    Builder::QParam("R.nid", Builder::NODE_ID_SERVICE_NAME),
                    Builder::QParam("R.pid", Builder::PROCESS_ID_SERVICE_NAME),
                    Builder::QParam("R.tid", Builder::THREAD_ID_SERVICE_NAME),
                    Builder::QParam("-1", Builder::STREAM_ID_SERVICE_NAME),
                    Builder::QParam("L.level"),
                    Builder::QParam("L.level_for_stream") },
                  { Builder::From("rocpd_region", "R"),
                    Builder::InnerJoin("rocpd_event", "E", "E.id = R.event_id"),
                    Builder::LeftJoin(Builder::LevelTable("launch"), "L", "R.id = L.eid") },
                  { Builder::Where(
                      "R.id", "==", std::to_string(event_id.bitfield.event_id)) } }));
            if(kRocProfVisDmResultSuccess != ExecuteSQLQuery(future, query.c_str(), extdata, &CallbackAddEssentialInfo)) break;

        } else
        if (event_id.bitfield.event_op == kRocProfVisDmOperationDispatch)
        {
            query = "select * from kernels where id == ";
            query += std::to_string(event_id.bitfield.event_id); 
            ShowProgress(0, query.c_str(),kRPVDbBusy, future);
            if(kRocProfVisDmResultSuccess !=
               ExecuteSQLQuery(
                   future, query.c_str(), "Properties", extdata,
                   (rocprofvis_dm_event_operation_t) event_id.bitfield.event_op,
                   &CallbackAddExtInfo))
                break;
            query = Builder::Select(rocprofvis_db_sqlite_essential_data_query_format(
                { { Builder::QParamOperation(kRocProfVisDmOperationDispatch),
                    Builder::QParam("K.nid", Builder::NODE_ID_SERVICE_NAME),
                    Builder::QParam("K.agent_id", Builder::AGENT_ID_SERVICE_NAME),
                    Builder::QParam("K.queue_id", Builder::QUEUE_ID_SERVICE_NAME),
                    Builder::QParam("K.stream_id", Builder::STREAM_ID_SERVICE_NAME), 
                    Builder::QParam("L.level"), 
                    Builder::QParam("L.level_for_stream") },
                  { Builder::From("rocpd_kernel_dispatch", "K"),
                    Builder::LeftJoin(Builder::LevelTable("dispatch"), "L", "K.id = L.eid") },
                  { Builder::Where(
                      "K.id", "==", std::to_string(event_id.bitfield.event_id)) } }));
            if(kRocProfVisDmResultSuccess != ExecuteSQLQuery(future, query.c_str(), extdata, &CallbackAddEssentialInfo)) break;
        } else   
        if (event_id.bitfield.event_op == kRocProfVisDmOperationMemoryAllocate)
        {
            query = "select * from memory_allocations where id == ";
            query += std::to_string(event_id.bitfield.event_id);
            ShowProgress(0, query.c_str(), kRPVDbBusy, future);
            if(kRocProfVisDmResultSuccess !=
               ExecuteSQLQuery(
                   future, query.c_str(), "Properties", extdata,
                   (rocprofvis_dm_event_operation_t) event_id.bitfield.event_op,
                   &CallbackAddExtInfo))
                break;
            query = Builder::Select(rocprofvis_db_sqlite_essential_data_query_format(
                { { Builder::QParamOperation(kRocProfVisDmOperationMemoryAllocate),
                    Builder::QParam("M.nid", Builder::NODE_ID_SERVICE_NAME),
                    Builder::QParam("M.agent_id", Builder::AGENT_ID_SERVICE_NAME),
                    Builder::QParam("M.queue_id", Builder::QUEUE_ID_SERVICE_NAME),
                    Builder::QParam("M.stream_id", Builder::STREAM_ID_SERVICE_NAME), 
                    Builder::QParam("L.level"),
                    Builder::QParam("L.level_for_stream") },
                  { Builder::From("rocpd_memory_allocate", "M"),
                    Builder::LeftJoin(Builder::LevelTable("mem_alloc"), "L", "M.id = L.eid") },
                  { Builder::Where(
                      "M.id", "==", std::to_string(event_id.bitfield.event_id)) } }));
            if(kRocProfVisDmResultSuccess != ExecuteSQLQuery(future, query.c_str(), extdata, &CallbackAddEssentialInfo)) break;
        } else
        if (event_id.bitfield.event_op == kRocProfVisDmOperationMemoryCopy)
        {
            query = "select * from memory_copies where id == ";
            query += std::to_string(event_id.bitfield.event_id);
            ShowProgress(0, query.c_str(), kRPVDbBusy, future);
            if(kRocProfVisDmResultSuccess !=
               ExecuteSQLQuery(
                   future, query.c_str(), "Properties", extdata,
                   (rocprofvis_dm_event_operation_t) event_id.bitfield.event_op,
                   &CallbackAddExtInfo))
                break;
            query = Builder::Select(rocprofvis_db_sqlite_essential_data_query_format(
                { { Builder::QParamOperation(kRocProfVisDmOperationMemoryCopy),
                    Builder::QParam("M.nid", Builder::NODE_ID_SERVICE_NAME),
                    Builder::QParam("M.dst_agent_id", Builder::AGENT_ID_SERVICE_NAME),
                    Builder::QParam("M.queue_id", Builder::QUEUE_ID_SERVICE_NAME),
                    Builder::QParam("M.stream_id", Builder::STREAM_ID_SERVICE_NAME), 
                    Builder::QParam("L.level"), 
                    Builder::QParam("L.level_for_stream") },
                  { Builder::From("rocpd_memory_copy", "M"),
                    Builder::LeftJoin(Builder::LevelTable("mem_copy"), "L", "M.id = L.eid") },
                  { Builder::Where(
                      "M.id", "==", std::to_string(event_id.bitfield.event_id)) } }));
            if(kRocProfVisDmResultSuccess != ExecuteSQLQuery(future, query.c_str(), extdata, &CallbackAddEssentialInfo)) break;
        } else
        {
            ShowProgress(0, "Extended data not available for specified operation type!", kRPVDbError, future );
            return future->SetPromise(kRocProfVisDmResultInvalidParameter);
        }
        ShowProgress(100, "Extended data successfully loaded!",kRPVDbSuccess, future);
        return future->SetPromise(kRocProfVisDmResultSuccess);
    }
    ShowProgress(0, "Extended data  not loaded!", kRPVDbError, future );
    return future->SetPromise(future->Interrupted() ? kRocProfVisDmResultDbAbort : kRocProfVisDmResultDbAccessFailed);    
}

}  // namespace DataModel
}  // namespace RocProfVis
