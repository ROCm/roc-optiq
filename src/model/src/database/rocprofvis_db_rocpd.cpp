// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_db_rocpd.h"
#include <sstream>
#include <string.h>
#include <filesystem>

namespace RocProfVis
{
namespace DataModel
{

rocprofvis_dm_result_t RocpdDatabase::RemapStringIds(rocprofvis_db_record_data_t & record)
{
    if (!RemapStringIdHelper(record.event.category)) return kRocProfVisDmResultNotLoaded;
    if (!RemapStringIdHelper(record.event.symbol)) return kRocProfVisDmResultNotLoaded;
    return kRocProfVisDmResultSuccess;
}

rocprofvis_dm_result_t RocpdDatabase::RemapStringIds(rocprofvis_db_flow_data_t & record)
{
    if (!RemapStringIdHelper(record.category_id)) return kRocProfVisDmResultNotLoaded;
    if (!RemapStringIdHelper(record.symbol_id)) return kRocProfVisDmResultNotLoaded;
    return kRocProfVisDmResultSuccess;
}

rocprofvis_dm_result_t RocpdDatabase::RemapStringId(uint64_t id, rocprofvis_db_string_type_t type, uint32_t node, uint64_t & result) {
    result = id;
    RemapStringIdHelper(result);
    return kRocProfVisDmResultSuccess;
}

const bool RocpdDatabase::RemapStringIdHelper(uint64_t & id)
{
    string_index_map_t::const_iterator pos = m_string_index_map.find(id);
    if (pos == m_string_index_map.end()) return false;
    id = pos->second;
    return true;
}

rocprofvis_dm_size_t RocpdDatabase::GetMemoryFootprint()
{
    rocprofvis_dm_size_t size = sizeof(RocpdDatabase)+Database::GetMemoryFootprint();
    size+=m_string_index_map.size()* (sizeof(uint64_t) + sizeof(uint32_t));
    size+=m_string_index_map.size() * 3 * sizeof(void*);
    return size;
}


int RocpdDatabase::ProcessTrack(rocprofvis_dm_track_params_t& track_params, rocprofvis_dm_charptr_t*  newqueries)
{
    ROCPROFVIS_ASSERT_MSG_RETURN(track_params.track_indentifiers.db_instance != nullptr, ERROR_NODE_KEY_CANNOT_BE_NULL, 1);
    DbInstance* db_instance = (DbInstance*)track_params.track_indentifiers.db_instance;
    rocprofvis_dm_track_params_it it = TrackTracker()->FindTrackParamsIterator(track_params.track_indentifiers, db_instance->GuidIndex());
    UpdateQueryForTrack(it, track_params, newqueries);    
    if(it == TrackPropertiesEnd())
    {

        if (track_params.track_indentifiers.category != kRocProfVisDmPmcTrack){
            track_params.track_indentifiers.name[TRACK_ID_TID_OR_QUEUE] = SubProcessNameSuffixFor(track_params.track_indentifiers.category);
            track_params.track_indentifiers.name[TRACK_ID_TID_OR_QUEUE] += std::to_string(track_params.track_indentifiers.id[TRACK_ID_TID_OR_QUEUE]);
        } 

        TrackTracker()->AddTrack(track_params.track_indentifiers, db_instance->GuidIndex(), track_params.track_indentifiers.track_id);

        if (track_params.track_indentifiers.category == kRocProfVisDmPmcTrack)
        {
            uint32_t counter_id = TrackTracker()->GetStringIdentifierIndex(track_params.track_indentifiers.name[TRACK_ID_COUNTER].c_str());
            track_params.track_indentifiers.id[TRACK_ID_COUNTER] = counter_id;
        }
        if (track_params.track_indentifiers.category == kRocProfVisDmKernelDispatchTrack ||
            track_params.track_indentifiers.category == kRocProfVisDmMemoryAllocationTrack ||
            track_params.track_indentifiers.category == kRocProfVisDmMemoryCopyTrack ||
            track_params.track_indentifiers.category == kRocProfVisDmPmcTrack)
        {
            auto it_pid = m_pid_map.find(track_params.track_indentifiers.id[TRACK_ID_AGENT]);
            if (it_pid != m_pid_map.end())
            {
                track_params.track_indentifiers.process_id = it_pid->second;
            }
        }

        if (kRocProfVisDmResultSuccess != AddTrackProperties(track_params)) return 1;

        if (BindObject()->FuncAddTrack(BindObject()->trace_object, TrackPropertiesLast()) != kRocProfVisDmResultSuccess) return 1; 

        if (BindObject()->FuncAddTopologyNode(BindObject()->trace_object, &track_params.track_indentifiers) != kRocProfVisDmResultSuccess) return 1; 

        uint64_t node_id = track_params.track_indentifiers.id[TRACK_ID_NODE];
        std::string db_name = std::filesystem::path(Path()).stem().string();

        CachedTables(0)->AddTableCell("Node", node_id, Builder::DB_ID_PUBLIC_NAME, kRPVDataTypeInt, std::to_string(node_id).c_str());
        CachedTables(0)->AddTableCell("Node", node_id, Builder::NODE_HOSTNAME_SERVICE_NAME, kRPVDataTypeString, db_name.c_str());
        CachedTables(0)->AddTableCell("Node", node_id, Builder::NODE_SYSTEM_SERVICE_NAME, kRPVDataTypeString, Builder::NOT_APLICABLE);
        CachedTables(0)->AddTableCell("Node", node_id, Builder::NODE_RELEASE_SERVICE_NAME, kRPVDataTypeString, Builder::NOT_APLICABLE);
        CachedTables(0)->AddTableCell("Node", node_id, Builder::NODE_VERSION_SERVICE_NAME, kRPVDataTypeString, Builder::NOT_APLICABLE);
        if (CachedTables(0)->PopulateTrackExtendedDataTemplate(this, 0, "Node", node_id) != kRocProfVisDmResultSuccess) return 1;

        if (track_params.track_indentifiers.category == kRocProfVisDmRegionTrack) {
            uint64_t process_id = track_params.track_indentifiers.id[TRACK_ID_PID];
            uint64_t thread_id = track_params.track_indentifiers.id[TRACK_ID_TID];
            CachedTables(0)->AddTableCell("Process", process_id, Builder::DB_ID_PUBLIC_NAME, kRPVDataTypeInt, std::to_string(process_id).c_str());
            CachedTables(0)->AddTableCell("Process", process_id, Builder::PROCESS_COMMAND_SERVICE_NAME, kRPVDataTypeString, db_name.c_str());
            CachedTables(0)->AddTableCell("Process", process_id, Builder::START_PUBLIC_NAME, kRPVDataTypeInt, "0");
            CachedTables(0)->AddTableCell("Process", process_id, Builder::END_PUBLIC_NAME, kRPVDataTypeInt, "0");
            CachedTables(0)->AddTableCell("Process", process_id, Builder::PROCESS_ENVIRONMENT_SERVICE_NAME, kRPVDataTypeString, "N/A");
            CachedTables(0)->AddTableCell("Thread", thread_id, Builder::DB_ID_PUBLIC_NAME, kRPVDataTypeInt, std::to_string(thread_id).c_str());
            CachedTables(0)->AddTableCell("Thread", thread_id, Builder::NAME_PUBLIC_NAME, kRPVDataTypeString, track_params.track_indentifiers.name[TRACK_ID_TID].c_str());
            CachedTables(0)->AddTableCell("Thread", thread_id, Builder::START_PUBLIC_NAME, kRPVDataTypeInt, "0");
            CachedTables(0)->AddTableCell("Thread", thread_id, Builder::END_PUBLIC_NAME, kRPVDataTypeInt, "0");
            CachedTables(0)->AddTableCell("Thread", thread_id, Builder::TID_SERVICE_NAME, kRPVDataTypeInt, std::to_string(thread_id).c_str());
            
            if (CachedTables(0)->PopulateTrackExtendedDataTemplate(this, 0, "Process", process_id) != kRocProfVisDmResultSuccess) return 1;
            if (CachedTables(0)->PopulateTrackExtendedDataTemplate(this, 0, "Thread", thread_id) != kRocProfVisDmResultSuccess) return 1;
        } else 
        if(track_params.track_indentifiers.category == kRocProfVisDmKernelDispatchTrack || 
            track_params.track_indentifiers.category == kRocProfVisDmMemoryAllocationTrack || 
            track_params.track_indentifiers.category == kRocProfVisDmMemoryCopyTrack || 
            track_params.track_indentifiers.category == kRocProfVisDmPmcTrack)
        {
            uint64_t agent_id = track_params.track_indentifiers.id[TRACK_ID_AGENT];
            uint64_t queue_id = track_params.track_indentifiers.id[TRACK_ID_QUEUE];
            CachedTables(0)->AddTableCell("Agent", agent_id, Builder::DB_ID_PUBLIC_NAME, kRPVDataTypeInt, std::to_string(agent_id).c_str());
            CachedTables(0)->AddTableCell("Agent", agent_id, Builder::AGENT_TYPE_SERVICE_NAME, kRPVDataTypeString, "GPU");
            CachedTables(0)->AddTableCell("Agent", agent_id, Builder::AGENT_TYPE_INDEX_SERVICE_NAME, kRPVDataTypeInt, std::to_string(agent_id).c_str());
            CachedTables(0)->AddTableCell("Agent", agent_id, Builder::PID_SERVICE_NAME, kRPVDataTypeInt, std::to_string(track_params.track_indentifiers.process_id).c_str());
            CachedTables(0)->AddTableCell("Agent", agent_id, Builder::AGENT_PRODUCT_NAME_SERVICE_NAME, kRPVDataTypeString, track_params.track_indentifiers.name[TRACK_ID_AGENT].c_str());
            CachedTables(0)->AddTableCell("Queue", queue_id, Builder::DB_ID_PUBLIC_NAME, kRPVDataTypeInt, std::to_string(queue_id).c_str());
            CachedTables(0)->AddTableCell("Queue", queue_id, Builder::NAME_PUBLIC_NAME, kRPVDataTypeString, track_params.track_indentifiers.name[TRACK_ID_QUEUE].c_str());
            CachedTables(0)->AddTableCell("Queue", queue_id, Builder::PID_SERVICE_NAME, kRPVDataTypeInt, std::to_string(track_params.track_indentifiers.process_id).c_str());
            if (CachedTables(0)->PopulateTrackExtendedDataTemplate(this, 0, "Agent", agent_id) != kRocProfVisDmResultSuccess) return 1;
            if (CachedTables(0)->PopulateTrackExtendedDataTemplate(this, 0, "Queue", queue_id) != kRocProfVisDmResultSuccess) return 1;
            if (track_params.track_indentifiers.category == kRocProfVisDmPmcTrack)
            {
                uint64_t counter_id = track_params.track_indentifiers.id[TRACK_ID_COUNTER];
                CachedTables(0)->AddTableCell("PMC", counter_id, Builder::DB_ID_PUBLIC_NAME, kRPVDataTypeInt, std::to_string(counter_id).c_str());
                CachedTables(0)->AddTableCell("PMC", counter_id, Builder::NAME_PUBLIC_NAME, kRPVDataTypeString, track_params.track_indentifiers.name[TRACK_ID_COUNTER].c_str());
                CachedTables(0)->AddTableCell("PMC", counter_id, Builder::COUNTER_DESCRIPTION_SERVICE_NAME, kRPVDataTypeString, "");
                CachedTables(0)->AddTableCell("PMC", counter_id, Builder::COUNTER_UNITS_SERVICE_NAME, kRPVDataTypeString, "");
                CachedTables(0)->AddTableCell("PMC", counter_id, Builder::COUNTER_VALUE_TYPE_SERVICE_NAME, kRPVDataTypeString, "");
                CachedTables(0)->AddTableCell("PMC", counter_id, Builder::PID_SERVICE_NAME, kRPVDataTypeInt, std::to_string(track_params.track_indentifiers.process_id).c_str());
                if (CachedTables(0)->PopulateTrackExtendedDataTemplate(this, 0, "PMC", counter_id) != kRocProfVisDmResultSuccess) return 1;
            }
        }
    }
    else
    {
        it->get()->load_id.insert(*track_params.load_id.begin());
    }
    return 0;
}

int RocpdDatabase::CallBackAgentToProcess(void *data, int argc, sqlite3_stmt* stmt, char **azColName){
    ROCPROFVIS_ASSERT_MSG_RETURN(argc==3, ERROR_DATABASE_QUERY_PARAMETERS_MISMATCH, 1);
    ROCPROFVIS_ASSERT_MSG_RETURN(data, ERROR_SQL_QUERY_PARAMETERS_CANNOT_BE_NULL, 1);
    void* func = (void*)&CallBackAgentToProcess;
    rocprofvis_db_sqlite_callback_parameters* callback_params = (rocprofvis_db_sqlite_callback_parameters*)data;
    RocpdDatabase* db = (RocpdDatabase*)callback_params->db;
    if(callback_params->future->Interrupted()) return SQLITE_ABORT;
    rocprofvis_dm_process_id pid = db->Sqlite3ColumnInt(func, stmt, azColName, 0);
    uint32_t agent = db->Sqlite3ColumnInt(func, stmt, azColName, 1);
    db->m_pid_map[agent] = pid;
    callback_params->future->CountThisRow();
    return 0;
}

int RocpdDatabase::CallBackAddString(void *data, int argc, sqlite3_stmt* stmt, char **azColName){
    ROCPROFVIS_ASSERT_MSG_RETURN(argc==2, ERROR_DATABASE_QUERY_PARAMETERS_MISMATCH, 1);
    ROCPROFVIS_ASSERT_MSG_RETURN(data, ERROR_SQL_QUERY_PARAMETERS_CANNOT_BE_NULL, 1);
    void* func = (void*)&CallBackAddString;
    rocprofvis_db_sqlite_callback_parameters* callback_params = (rocprofvis_db_sqlite_callback_parameters*)data;
    RocpdDatabase* db = (RocpdDatabase*)callback_params->db;
    if(callback_params->future->Interrupted()) return SQLITE_ABORT;
    std::stringstream ids((char*)db->Sqlite3ColumnText(func, stmt, azColName, 1));
    uint32_t string_id = db->BindObject()->FuncAddString(db->BindObject()->trace_object, (char*)db->Sqlite3ColumnText(func, stmt, azColName, 0));
    if (string_id == INVALID_INDEX) return 1;
    
    for (uint64_t id; ids >> id;) {   
        db->m_string_index_map[id] = string_id;
        rocprofvis_db_string_id_t key = { id, 0, rocprofvis_db_string_type_t::kRPVStringTypeNameOrCategory};
        db->m_string_id_map[string_id].push_back(key);
        if (ids.peek() == ',') ids.ignore();
    }
    callback_params->future->CountThisRow();
    return 0;
}


int RocpdDatabase::CallbackAddStackTrace(void *data, int argc, sqlite3_stmt* stmt, char **azColName){
    ROCPROFVIS_ASSERT_MSG_RETURN(data, ERROR_SQL_QUERY_PARAMETERS_CANNOT_BE_NULL, 1);
    ROCPROFVIS_ASSERT_MSG_RETURN(argc==4, ERROR_DATABASE_QUERY_PARAMETERS_MISMATCH, 1);
    void* func = (void*)&CallbackAddStackTrace;
    rocprofvis_db_sqlite_callback_parameters* callback_params = (rocprofvis_db_sqlite_callback_parameters*)data;
    RocpdDatabase* db = (RocpdDatabase*)callback_params->db;
    rocprofvis_db_stack_data_t record;
    if(callback_params->future->Interrupted()) return SQLITE_ABORT;
    record.symbol = (char*)db->Sqlite3ColumnText(func, stmt, azColName, 0);
    record.args = (char*)db->Sqlite3ColumnText(func, stmt, azColName, 1);
    record.line = (char*)db->Sqlite3ColumnText(func, stmt, azColName, 2);
    record.depth = db->Sqlite3ColumnInt(func, stmt, azColName, 3);
    if (db->BindObject()->FuncAddStackFrame(callback_params->handle,record) != kRocProfVisDmResultSuccess) return 1;
    callback_params->future->CountThisRow();
    return 0;
}       

rocprofvis_dm_result_t
RocpdDatabase::CreateIndexes()
{
    std::vector<std::string> vec;
    vec.push_back("CREATE INDEX IF NOT EXISTS pid_tid_idx ON rocpd_api(pid, tid, start);");
    vec.push_back("CREATE INDEX IF NOT EXISTS gid_qid_idx ON rocpd_op(gpuId, queueId, start);");
    vec.push_back("CREATE INDEX IF NOT EXISTS monitorTypeIdx on rocpd_monitor(gpuId,monitorType,start);");

    return ExecuteTransaction(vec);
}

rocprofvis_dm_result_t  RocpdDatabase::ReadTraceMetadata(Future* future)
{
    ROCPROFVIS_ASSERT_MSG_RETURN(future, ERROR_FUTURE_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
    while (true)
    {
        ROCPROFVIS_ASSERT_MSG_BREAK(BindObject()->trace_properties, ERROR_TRACE_PROPERTIES_CANNOT_BE_NULL);
        TraceProperties()->events_count[kRocProfVisDmOperationLaunch]   = 0;
        TraceProperties()->events_count[kRocProfVisDmOperationDispatch] = 0;
        TraceProperties()->tracks_info_restored = true;
        TraceProperties()->tracks_info_id_mismatch = false;
        TraceProperties()->start_time = UINT64_MAX;
        TraceProperties()->end_time = 0;
        TraceProperties()->num_db_instances = 1;

        ShowProgress(10, "Indexing tables", kRPVDbBusy, future);
        CreateIndexes();

        DbInstances().push_back({SingleNodeDbInstance(), ""});

        std::string track_queries = GetEventTrackQuery(kRocProfVisDmRegionTrack) + 
            GetEventTrackQuery(kRocProfVisDmKernelDispatchTrack) + 
            GetEventTrackQuery(kRocProfVisDmPmcTrack);
        std::size_t track_queries_hash_value = std::hash<std::string>{}(track_queries);
        uint32_t load_id = 0;

        if (kRocProfVisDmResultSuccess != ExecuteSQLQuery(future, DbInstancePtrAt(0),
            "SELECT DISTINCT pid, gpuId, queueId FROM rocpd_api_ops INNER JOIN api ON rocpd_api_ops.api_id = api.id INNER JOIN op ON rocpd_api_ops.op_id = op.id;", &CallBackAgentToProcess)) break;

        ShowProgress(5, "Adding HIP API tracks", kRPVDbBusy, future );
        m_add_track_mutex.reset();
        if(kRocProfVisDmResultSuccess != ExecuteSQLQuery(
            future, DbInstancePtrAt(0), track_queries_hash_value, load_id++,
            { 
                        // Track query by pid/tid
                      GetEventTrackQuery(kRocProfVisDmRegionTrack),   
                        // Track query by stream
                        "",
                        // Level query
                     Builder::Select(rocprofvis_db_sqlite_level_query_format(
                     { { Builder::QParamOperation(kRocProfVisDmOperationLaunch),
                         Builder::QParam("start", Builder::START_SERVICE_NAME), 
                         Builder::QParam("end", Builder::END_SERVICE_NAME),
                         Builder::QParam("id"), 
                         Builder::SpaceSaver(0),
                         Builder::QParam("pid", Builder::PROCESS_ID_SERVICE_NAME),
                         Builder::QParam("tid", Builder::THREAD_ID_SERVICE_NAME),
                         Builder::SpaceSaver(0) },
                       { Builder::From("rocpd_api", MultiNode::No) } })),
                        // Slice query by queue
                     Builder::Select(rocprofvis_db_sqlite_slice_query_format(
                     { { Builder::QParamOperation(kRocProfVisDmOperationLaunch),
                         Builder::QParam("start", Builder::START_SERVICE_NAME), 
                         Builder::QParam("end", Builder::END_SERVICE_NAME),
                         Builder::QParam("args_id"), 
                         Builder::QParam("apiName_id"),
                         Builder::QParam("id"),
                         Builder::SpaceSaver(0),
                         Builder::QParam("pid", Builder::PROCESS_ID_SERVICE_NAME),
                         Builder::QParam("tid", Builder::THREAD_ID_SERVICE_NAME),
                         Builder::QParam("L.level", Builder::EVENT_LEVEL_SERVICE_NAME),
                         Builder::QParamCategory(kRocProfVisDmRegionTrack )
                         },
                       { Builder::From("rocpd_api",MultiNode::No),
                         Builder::LeftJoin(Builder::LevelTable("api"), "L", "id = L.eid", MultiNode::No) } })),
                        // Slice query by stream
                        "",
                        // Table query
                     GetEventOperationQuery(kRocProfVisDmOperationLaunch)
                     
            },
                        &CallBackAddTrack, &CallBackLoadTrack)) break;

        ShowProgress(5, "Adding kernel dispatch tracks", kRPVDbBusy, future );
        m_add_track_mutex.reset();
        if (kRocProfVisDmResultSuccess != ExecuteSQLQuery(
            future, DbInstancePtrAt(0), track_queries_hash_value, load_id++,
            {
                        // Track query by agent/queue
                     GetEventTrackQuery(kRocProfVisDmKernelDispatchTrack),  
                        // Track query by stream
                        "",
                        // Level query
                     Builder::Select(rocprofvis_db_sqlite_level_query_format(
                       { { Builder::QParamOperation(kRocProfVisDmOperationDispatch),
                         Builder::QParam("start", Builder::START_SERVICE_NAME), 
                         Builder::QParam("end", Builder::END_SERVICE_NAME),
                         Builder::QParam("id"), 
                         Builder::SpaceSaver(0),
                         Builder::QParam("gpuId", Builder::AGENT_ID_SERVICE_NAME),
                         Builder::QParam("queueId", Builder::QUEUE_ID_SERVICE_NAME),
                         Builder::SpaceSaver(0) },
                       { Builder::From("rocpd_op", MultiNode::No) } })),
                        // Slice query by queue
                     Builder::Select(rocprofvis_db_sqlite_slice_query_format(
                       { { Builder::QParamOperation(kRocProfVisDmOperationDispatch),
                         Builder::QParam("start", Builder::START_SERVICE_NAME), 
                         Builder::QParam("end", Builder::END_SERVICE_NAME),
                         Builder::QParam("opType_id"), 
                         Builder::QParam("description_id"),
                         Builder::QParam("id"), 
                         Builder::SpaceSaver(0),
                         Builder::QParam("gpuId", Builder::AGENT_ID_SERVICE_NAME),
                           Builder::QParam("queueId", Builder::QUEUE_ID_SERVICE_NAME),
                         Builder::QParam("L.level", Builder::EVENT_LEVEL_SERVICE_NAME),
                         Builder::QParamCategory(kRocProfVisDmKernelDispatchTrack)
                       },
                       { Builder::From("rocpd_op", MultiNode::No),
                         Builder::LeftJoin(Builder::LevelTable("op"), "L", "id = L.eid",MultiNode::No) } })),
                        // Slice query by stream
                        "",
                        // Table query
                         GetEventOperationQuery(kRocProfVisDmOperationDispatch)
            },
                        &CallBackAddTrack, &CallBackLoadTrack)) break;

        ShowProgress(5, "Adding performance counters tracks", kRPVDbBusy, future );
        m_add_track_mutex.reset();
        if (kRocProfVisDmResultSuccess != ExecuteSQLQuery(
            future, DbInstancePtrAt(0),track_queries_hash_value, load_id++,
            { 
                        // Track query by agent/monitorType
                     GetEventTrackQuery(kRocProfVisDmPmcTrack),
                        // Track query by stream
                        "",
                        // Level query
                     Builder::Select(rocprofvis_db_sqlite_level_query_format(
                     { { Builder::QParamOperation(kRocProfVisDmOperationNoOp),
                         Builder::QParam("start", Builder::START_SERVICE_NAME), 
                         Builder::QParam("start", Builder::END_SERVICE_NAME),
                         Builder::SpaceSaver(0),
                         Builder::SpaceSaver(0),
                         Builder::QParam("deviceId", Builder::AGENT_ID_SERVICE_NAME),
                         Builder::QParam("monitorType", Builder::COUNTER_NAME_SERVICE_NAME),
                         Builder::SpaceSaver(0) },
                       { Builder::From("rocpd_monitor", MultiNode::No) } })),
                        // Slice query by monitorType
                     Builder::Select(rocprofvis_db_sqlite_slice_query_format(
                     { { Builder::QParamOperation(kRocProfVisDmOperationNoOp),
                         Builder::QParam("start", Builder::START_SERVICE_NAME), 
                         Builder::QParam("value", Builder::COUNTER_VALUE_SERVICE_NAME), 
                         Builder::QParam("start", Builder::END_SERVICE_NAME), 
                         Builder::SpaceSaver(0),
                         Builder::SpaceSaver(0), 
                         Builder::SpaceSaver(0),
                         Builder::QParam("deviceId", Builder::AGENT_ID_SERVICE_NAME),
                         Builder::QParam("monitorType", Builder::COUNTER_NAME_SERVICE_NAME),
                         Builder::QParam("CAST(value AS REAL)", Builder::EVENT_LEVEL_SERVICE_NAME),
                         Builder::QParamCategory(kRocProfVisDmPmcTrack)
                         },
                       { Builder::From("rocpd_monitor", MultiNode::No) } })),
                        // Slice query by stream
                        "",
                         GetEventOperationQuery(kRocProfVisDmOperationNoOp)
            },
                        &CallBackAddTrack, &CallBackLoadTrack)) break;

        ShowProgress(20, "Loading strings", kRPVDbBusy, future );
        if (kRocProfVisDmResultSuccess != ExecuteSQLQuery(future, DbInstancePtrAt(0),"SELECT string, GROUP_CONCAT(id) AS ids FROM rocpd_string GROUP BY string;", &CallBackAddString)) break;

        {
            std::vector<std::string> to_drop;
            Builder::OldLevelTables("api", to_drop);
            for (auto table : to_drop)
            {
                DropSQLTable(table.c_str());
            }
        }
        {
            std::vector<std::string> to_drop;
            Builder::OldLevelTables("op", to_drop);
            for (auto table : to_drop)
            {
                DropSQLTable(table.c_str());
            }
        }

        if(SQLITE_OK != DetectTable(GetServiceConnection(), Builder::LevelTable("api").c_str(), false) ||
           SQLITE_OK != DetectTable(GetServiceConnection(), Builder::LevelTable("op").c_str(), false))
        {
            m_event_levels[kRocProfVisDmOperationLaunch][DbInstancePtrAt(0)->GuidIndex()].reserve(
                TraceProperties()->events_count[kRocProfVisDmOperationLaunch]);
            m_event_levels[kRocProfVisDmOperationDispatch][DbInstancePtrAt(0)->GuidIndex()].reserve(
                TraceProperties()->events_count[kRocProfVisDmOperationDispatch]);

            ShowProgress(10, "Calculating event levels", kRPVDbBusy, future);
            if(kRocProfVisDmResultSuccess !=
               ExecuteQueryForAllTracksAsync(
                   0,
                   kRPVQueryLevel,
                   "SELECT *, ", (std::string(" ORDER BY ")+Builder::START_SERVICE_NAME).c_str(), &CalculateEventLevels,
                   [](rocprofvis_dm_track_params_t* params) {
                       params->m_active_events.clear();
                   },
                   { DbInstances() }))
            {
                break;
            }

            ShowProgress(10, "Save event levels", kRPVDbBusy, future);
            SQLInsertParams params[] = { { "eid", "INTEGER PRIMARY KEY" },
                                         { "level", "INTEGER" } };
            CreateSQLTable(
                Builder::LevelTable("api").c_str(), params, 2,
                m_event_levels[kRocProfVisDmOperationLaunch][DbInstancePtrAt(0)->GuidIndex()].size(),
                [&](sqlite3_stmt* stmt, int index) {
                    sqlite3_bind_int64(
                        stmt, 1, m_event_levels[kRocProfVisDmOperationLaunch][DbInstancePtrAt(0)->GuidIndex()][index].id);
                    sqlite3_bind_int(
                        stmt, 2,
                        m_event_levels[kRocProfVisDmOperationLaunch][DbInstancePtrAt(0)->GuidIndex()][index].level_for_queue);
                });
            m_event_levels[kRocProfVisDmOperationLaunch].clear();
            m_event_levels_id_to_index[kRocProfVisDmOperationLaunch].clear();
            CreateSQLTable(
                Builder::LevelTable("op").c_str(), params, 2,
                m_event_levels[kRocProfVisDmOperationDispatch][DbInstancePtrAt(0)->GuidIndex()].size(),
                [&](sqlite3_stmt* stmt, int index) {
                    sqlite3_bind_int64(
                        stmt, 1,
                        m_event_levels[kRocProfVisDmOperationDispatch][DbInstancePtrAt(0)->GuidIndex()][index].id);
                    sqlite3_bind_int(
                        stmt, 2,
                        m_event_levels[kRocProfVisDmOperationDispatch][DbInstancePtrAt(0)->GuidIndex()][index].level_for_queue);
                });
            m_event_levels[kRocProfVisDmOperationDispatch][DbInstancePtrAt(0)->GuidIndex()].clear();
            m_event_levels_id_to_index[kRocProfVisDmOperationDispatch][DbInstancePtrAt(0)->GuidIndex()].clear();
        }
      
        if (!TraceProperties()->tracks_info_restored)
        {
            ShowProgress(5, "Collecting track properties",
                kRPVDbBusy, future);
            if (kRocProfVisDmResultSuccess !=
                ExecuteQueryForAllTracksAsync(kRocProfVisDmIncludePmcTracks | kRocProfVisDmIncludeStreamTracks,
                    kRPVQuerySliceByTrackSliceQuery,
                    "SELECT MIN(startTs), MAX(endTs), MIN(event_level), MAX(event_level), ", 
                    "WHERE startTs != 0 AND endTs != 0", &CallbackGetTrackProperties,
                    [](rocprofvis_dm_track_params_t* params) {
                    },
                    { DbInstances() }))
            {
                break;
            }
        }

        ShowProgress(5, "Save track information", kRPVDbBusy, future);
        SaveTrackProperties(future, track_queries_hash_value);

        ShowProgress(5, "Collecting track histogram", kRPVDbBusy, future);
        BuildHistogram(future, 500);

        TraceProperties()->metadata_loaded=true;
        BindObject()->FuncMetadataLoaded(BindObject()->trace_object);
        ShowProgress(100-future->Progress(), "Trace metadata successfully loaded", kRPVDbSuccess, future );
        return future->SetPromise(kRocProfVisDmResultSuccess);

    }
    ShowProgress(0, "Trace metadata not loaded!", kRPVDbError, future );
    return future->SetPromise(future->Interrupted() ? kRocProfVisDmResultDbAbort : kRocProfVisDmResultDbAccessFailed);
}

rocprofvis_dm_result_t RocpdDatabase::SaveTrimmedData(rocprofvis_dm_timestamp_t start,
                                 rocprofvis_dm_timestamp_t end,
                                 rocprofvis_dm_charptr_t new_db_path, Future* future)
{
    ROCPROFVIS_ASSERT_MSG_RETURN(new_db_path, "New DB path cannot be NULL.",
                                 kRocProfVisDmResultInvalidParameter);
    ROCPROFVIS_ASSERT_MSG_RETURN(future, ERROR_FUTURE_CANNOT_BE_NULL,
                                 kRocProfVisDmResultInvalidParameter);
    rocprofvis_dm_result_t result = kRocProfVisDmResultInvalidParameter;

    std::string                          query;
    rocprofvis_db_sqlite_trim_parameters trim_tables;
    rocprofvis_db_sqlite_trim_parameters trim_views;

    std::filesystem::remove(new_db_path);

    while (true) {
        RocpdDatabase rpDb(new_db_path);
        result = rpDb.Open();

        if (result != kRocProfVisDmResultSuccess) break;
        result = ExecuteSQLQuery(future, DbInstancePtrAt(0), "SELECT name, sql FROM sqlite_master WHERE type='table';", "", (rocprofvis_dm_handle_t)&trim_tables, &CallbackTrimTableQuery);

        ShowProgress(1, "Iterate over views", kRPVDbBusy, future);
        result = ExecuteSQLQuery(future, DbInstancePtrAt(0), "SELECT name, sql FROM sqlite_master WHERE type='view';", "", (rocprofvis_dm_handle_t)&trim_views, &CallbackTrimTableQuery);

        if (result != kRocProfVisDmResultSuccess) break;
        for(auto const& table : trim_tables.tables)
        {
            if(table.first != "sqlite_master" && table.first != "sqlite_sequence")
            {
                if(result == kRocProfVisDmResultSuccess)
                {
                    std::string msg = "Copy table " + table.first;
                    ShowProgress(1, msg.c_str(), kRPVDbBusy, future);
                    result = rpDb.ExecuteSQLQuery(future,DbInstancePtrAt(0), table.second.c_str());
                }
                else
                {
                    break;
                }
            }
        }

        if (result != kRocProfVisDmResultSuccess) break;
        for(auto const& view : trim_views.tables)
        {
            if(result == kRocProfVisDmResultSuccess)
            {
                std::string msg = "Copy views " + view.first;
                ShowProgress(1, msg.c_str(), kRPVDbBusy, future);
                result = rpDb.ExecuteSQLQuery(future,DbInstancePtrAt(0), view.second.c_str());
            }
            else
            {
                break;
            }
        }

        if (result != kRocProfVisDmResultSuccess) break;

        ShowProgress(0, "Attaching old DB to new", kRPVDbBusy, future);
        result = rpDb.ExecuteSQLQuery(future, DbInstancePtrAt(0), (std::string("ATTACH DATABASE '") + Path() + "' as 'oldDb';").c_str());

        if (result != kRocProfVisDmResultSuccess) break;

        for(auto const& table : trim_tables.tables)
        {
            if(table.first != "sqlite_master" && table.first != "sqlite_sequence")
            {
                if (result != kRocProfVisDmResultSuccess) break;

                std::string msg = "Copy table " + table.first;
                ShowProgress(1, msg.c_str(), kRPVDbBusy, future);

                if(!strcmp(table.first.c_str(), "rocpd_api") ||
                    !strcmp(table.first.c_str(), "rocpd_monitor") ||
                    !strcmp(table.first.c_str(), "rocpd_op"))
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

                result = rpDb.ExecuteSQLQuery(future, DbInstancePtrAt(0), query.c_str());                

            }
        }

        if (result != kRocProfVisDmResultSuccess) break;

        ShowProgress(0, "Detaching old DB", kRPVDbBusy, future);
        query  = "DETACH oldDb;";
        result = rpDb.ExecuteSQLQuery(future, DbInstancePtrAt(0), "DETACH oldDb;");

        ShowProgress(100, "Trace trimmed successfully!", kRPVDbSuccess, future);
        return future->SetPromise(result);
    }

    ShowProgress(0, "Failed to trim track!", kRPVDbError, future);
    return future->SetPromise(result);
}

rocprofvis_dm_result_t RocpdDatabase::BuildTableStringIdFilter(rocprofvis_dm_num_string_table_filters_t num_string_table_filters,
    rocprofvis_dm_string_table_filters_t     string_table_filters,
    table_string_id_filter_map_t&            filter)
{
    rocprofvis_dm_result_t result = kRocProfVisDmResultNotLoaded;
    if(num_string_table_filters > 0)
    {
        std::vector<rocprofvis_dm_index_t> string_indices;
        result = BindObject()->FuncGetStringIndices(BindObject()->trace_object, num_string_table_filters, string_table_filters, string_indices);
        ROCPROFVIS_ASSERT_RETURN(result == kRocProfVisDmResultSuccess, result);
        std::string string;
        for(const rocprofvis_dm_index_t& index : string_indices)
        {
            std::vector<rocprofvis_db_string_id_t> ids;
            if (kRocProfVisDmResultSuccess == StringIndexToId(index, ids))
            {
                for (auto id : ids)
                {
                    if (!string.empty())
                        string += ", ";
                    string += std::to_string(id.m_string_id);
                }
            }
        }
        if(!string.empty())
        {
            filter[kRocProfVisDmOperationLaunch][0] = std::string(Builder::CATEGORY_REFERENCE_RPD) + " IN (" + string + ") OR " + Builder::EVENT_NAME_REFERENCE_RPD + " IN(" + string;
            filter[kRocProfVisDmOperationDispatch][0] = std::string(Builder::CATEGORY_REFERENCE_RPD) + " IN (" + string + ") OR " + Builder::EVENT_NAME_REFERENCE_RPD + " IN(" + string;
        }
    }   
    return result;
}

rocprofvis_dm_string_t RocpdDatabase::GetEventOperationQuery(const rocprofvis_dm_event_operation_t operation)
{
    switch(operation)
    {
        case kRocProfVisDmOperationLaunch:
        {
            return Builder::Select(rocprofvis_db_sqlite_rocpd_table_query_format(
                { { Builder::QParamOperation(kRocProfVisDmOperationLaunch),
                Builder::QParam("id", Builder::ID_PUBLIC_NAME),
                Builder::QParam("id", Builder::DB_ID_PUBLIC_NAME),
                Builder::QParam("apiName_id", Builder::CATEGORY_REFERENCE_RPD),
                Builder::QParam("args_id", Builder::EVENT_NAME_REFERENCE_RPD), 
                Builder::QParam("start", Builder::START_SERVICE_NAME),
                Builder::QParam("end", Builder::END_SERVICE_NAME),
                Builder::QParam("(end-start)", Builder::DURATION_PUBLIC_NAME),
                Builder::QParam("pid", Builder::PROCESS_ID_SERVICE_NAME),
                Builder::QParam("tid", Builder::THREAD_ID_SERVICE_NAME) },
                { Builder::From("rocpd_api", MultiNode::No) } }));
        }
        case kRocProfVisDmOperationDispatch:
        {
            return Builder::Select(rocprofvis_db_sqlite_rocpd_table_query_format(
                { { Builder::QParamOperation(kRocProfVisDmOperationDispatch),
                Builder::QParam("id", Builder::ID_PUBLIC_NAME),
                Builder::QParam("id", Builder::DB_ID_PUBLIC_NAME),
                Builder::QParam("opType_id", Builder::CATEGORY_REFERENCE_RPD),
                Builder::QParam("description_id", Builder::EVENT_NAME_REFERENCE_RPD), 
                Builder::QParam("start", Builder::START_SERVICE_NAME),
                Builder::QParam("end", Builder::END_SERVICE_NAME),
                Builder::QParam("(end-start)", Builder::DURATION_PUBLIC_NAME),
                Builder::QParam("gpuId", Builder::AGENT_ID_SERVICE_NAME),
                Builder::QParam("queueId", Builder::QUEUE_ID_SERVICE_NAME) },
                { Builder::From("rocpd_op", MultiNode::No) } }));
        }
        case kRocProfVisDmOperationNoOp:
        {
            return Builder::Select(rocprofvis_db_sqlite_rocpd_sample_table_query_format(
                {this,
                { Builder::QParamOperation(kRocProfVisDmOperationNoOp),
                Builder::QParam("id", Builder::ID_PUBLIC_NAME),
                Builder::QParam("id", Builder::DB_ID_PUBLIC_NAME),
                Builder::QParam("monitorType", Builder::COUNTER_NAME_REFERENCE_RPD),
                Builder::QParam("value", Builder::COUNTER_VALUE_SERVICE_NAME), 
                Builder::QParam("start", Builder::START_SERVICE_NAME),
                Builder::QParam("start", Builder::END_SERVICE_NAME),
                Builder::QParam("monitorType", Builder::COUNTER_NAME_SERVICE_NAME),
                Builder::QParam("deviceId",  Builder::AGENT_ID_SERVICE_NAME)},
                { Builder::From("rocpd_monitor", MultiNode::No) } }));
        }
        default:
        {
            return "";
        }
    }
}

rocprofvis_dm_string_t RocpdDatabase::GetEventTrackQuery(const rocprofvis_dm_track_category_t category)
{
    switch (category)
    {
        case kRocProfVisDmRegionTrack:
        {
            return Builder::Select(rocprofvis_db_sqlite_track_query_format(
                { { Builder::SpaceSaver(0),
                Builder::QParam("pid", Builder::PROCESS_ID_SERVICE_NAME),
                Builder::QParam("tid", Builder::THREAD_ID_SERVICE_NAME),
                Builder::QParam("0", Builder::PROCESS_ID_PUBLIC_NAME),
                Builder::QParamCategory(kRocProfVisDmRegionTrack),
                Builder::QParamOperation(kRocProfVisDmOperationLaunch),
                Builder::StoreConfigVersion()
                    },
                { Builder::From("rocpd_api", MultiNode::No) } }));
        }
        case kRocProfVisDmKernelDispatchTrack:
        {
            return Builder::Select(rocprofvis_db_sqlite_track_query_format(
                { { Builder::SpaceSaver(0),
                Builder::QParam("gpuId", Builder::AGENT_ID_SERVICE_NAME),
                Builder::QParam("queueId", Builder::QUEUE_ID_SERVICE_NAME),
                Builder::QParam("0", Builder::PROCESS_ID_PUBLIC_NAME),
                Builder::QParamCategory(kRocProfVisDmKernelDispatchTrack),
                Builder::QParamOperation(kRocProfVisDmOperationDispatch),
                Builder::StoreConfigVersion()
                    },
                { Builder::From("rocpd_op", MultiNode::No) } }));
        }
        case kRocProfVisDmPmcTrack:
        {
            return Builder::Select(rocprofvis_db_sqlite_track_query_format(
                { { Builder::SpaceSaver(0),
                Builder::QParam("deviceId", Builder::AGENT_ID_SERVICE_NAME),
                Builder::QParam("monitorType", Builder::COUNTER_NAME_SERVICE_NAME),
                Builder::QParam("0", Builder::PROCESS_ID_PUBLIC_NAME),
                Builder::QParamCategory(kRocProfVisDmPmcTrack),
                Builder::QParamOperation(kRocProfVisDmOperationNoOp),
                Builder::StoreConfigVersion()
                    },
                { Builder::From("rocpd_monitor where deviceId > 0", MultiNode::No) } }));
        }
        default:
        {
            return "";
        }
    }
}

rocprofvis_dm_result_t RocpdDatabase::StringIndexToId(rocprofvis_dm_index_t index, std::vector<rocprofvis_db_string_id_t>& id)
{
    rocprofvis_dm_result_t result = kRocProfVisDmResultInvalidParameter;
    if(m_string_id_map.count(index) > 0)
    {
        id = m_string_id_map.at(index);
        result = kRocProfVisDmResultSuccess;
    }
    return result;
}

rocprofvis_dm_result_t
RocpdDatabase::BuildTableSummaryClause(bool sample_query,
                                 rocprofvis_dm_string_t& select,
                                 rocprofvis_dm_string_t& group_by)
{
    if(sample_query)
    {
        select = "counter, AVG(value) AS avg_value, MIN(value) AS min_value, MAX(value) AS max_value";
        group_by = "counter";
    }
    else
    {
        select = "name, COUNT(*) AS num_invocations, AVG(duration) AS avg_duration, MIN(duration) AS min_duration, MAX(duration) AS max_duration, SUM(duration) AS total_duration";
        group_by = "name";
    }
    return kRocProfVisDmResultSuccess;
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
            query << "select 2, rocpd_api_ops.api_id, rocpd_api_ops.op_id, 0, gpuId, queueId, rocpd_op.start, rocpd_op.opType_id, rocpd_op.description_id, " << Builder::LevelTable("op") << ".level, rocpd_op.end AS end "
                "from rocpd_api_ops "
                "INNER JOIN rocpd_api on rocpd_api_ops.api_id = rocpd_api.id "
                "INNER JOIN rocpd_op on rocpd_api_ops.op_id = rocpd_op.id "
                "INNER JOIN " << Builder::LevelTable("op") << " on rocpd_api_ops.op_id = " << Builder::LevelTable("op") << ".eid " 
                "where rocpd_api_ops.api_id = ";
            query << event_id.bitfield.event_id;  
            ShowProgress(0, query.str().c_str(),kRPVDbBusy, future);
            if (kRocProfVisDmResultSuccess != ExecuteSQLQuery(future, DbInstancePtrAt(0), query.str().c_str(), flowtrace, &CallbackAddFlowTrace)) break;
        } else
        if (event_id.bitfield.event_op == kRocProfVisDmOperationDispatch)
        {
            query << "select 1, rocpd_api_ops.op_id, rocpd_api_ops.api_id, 0, pid, tid, rocpd_api.apiName_id, rocpd_api.args_id, " << Builder::LevelTable("api") << ".level, rocpd_api.end "
                "from rocpd_api_ops "
                "INNER JOIN rocpd_api on rocpd_api_ops.api_id = rocpd_api.id "
                "INNER JOIN rocpd_op on rocpd_api_ops.op_id = rocpd_op.id "
                "INNER JOIN " << Builder::LevelTable("api") << " on rocpd_api_ops.api_id = " << Builder::LevelTable("api") << ".eid " 
                "where rocpd_api_ops.op_id = ";
            query << event_id.bitfield.event_id << ";";  
            ShowProgress(0, query.str().c_str(),kRPVDbBusy, future);
            if (kRocProfVisDmResultSuccess != ExecuteSQLQuery(future,DbInstancePtrAt(0), query.str().c_str(), flowtrace, &CallbackAddFlowTrace)) break;
        } else
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
#ifdef SUPPORT_OLD_SCHEMA_STACK_TRACE 
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
        ShowProgress(100, "Stack trace successfully loaded!", kRPVDbSuccess, future);
#else
        ShowProgress(100, "Stack trace is not supported for old schema database!", kRPVDbSuccess, future);
#endif

        return future->SetPromise(kRocProfVisDmResultSuccess);
    }
    ShowProgress(0, "Stack trace not loaded!", kRPVDbError, future);
    return future->SetPromise(future->Interrupted() ? kRocProfVisDmResultDbAbort
                                                    : kRocProfVisDmResultDbAccessFailed);
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
            query << "select *, end-start as duration from api where id == ";
            query << event_id.bitfield.event_id << ";";  
            ShowProgress(0, query.str().c_str(),kRPVDbBusy, future);
            if(kRocProfVisDmResultSuccess !=
               ExecuteSQLQuery(
                   future, DbInstancePtrAt(0),query.str().c_str(), "Properties", extdata,
                   (rocprofvis_dm_event_operation_t) event_id.bitfield.event_op,
                   &CallbackAddExtInfo))
                break;
            query.str("");
            query << "select *, end-start as duration from copy where id == ";
            query << event_id.bitfield.event_id << ";";  
            ShowProgress(50, query.str().c_str(),kRPVDbBusy, future);
            if(kRocProfVisDmResultSuccess !=
               ExecuteSQLQuery(
                   future, DbInstancePtrAt(0), query.str().c_str(), "Copy", extdata,
                   (rocprofvis_dm_event_operation_t) event_id.bitfield.event_op,
                   &CallbackAddExtInfo))
                break;

            std::string level_query = Builder::Select(rocprofvis_db_sqlite_essential_data_query_format(
                { { Builder::QParamOperation(kRocProfVisDmOperationLaunch),
                    Builder::SpaceSaver(0),
                    Builder::QParam("pid", Builder::PROCESS_ID_SERVICE_NAME),
                    Builder::QParam("tid", Builder::THREAD_ID_SERVICE_NAME),
                    Builder::SpaceSaver(0), 
                    Builder::QParam("L.level"),
                    Builder::SpaceSaver(0) },
                  { Builder::From("rocpd_api", MultiNode::No),
                    Builder::LeftJoin(Builder::LevelTable("api"), "L", "id = L.eid", MultiNode::No) },
                  { Builder::Where(
                      "id", "==", std::to_string(event_id.bitfield.event_id)) } }));
            if(kRocProfVisDmResultSuccess !=
               ExecuteSQLQuery(future, DbInstancePtrAt(0), level_query.c_str(), extdata, &CallbackAddEssentialInfo))
                break;
        } else
        if (event_id.bitfield.event_op == kRocProfVisDmOperationDispatch)
        {
            query << "select *, end-start as duration, 'GPU' as agent_type from op where id == ";
            query << event_id.bitfield.event_id << ";";  
            ShowProgress(0, query.str().c_str(),kRPVDbBusy, future);
            if(kRocProfVisDmResultSuccess !=
               ExecuteSQLQuery(
                   future, DbInstancePtrAt(0), query.str().c_str(), "Properties", extdata,
                   (rocprofvis_dm_event_operation_t) event_id.bitfield.event_op,
                   &CallbackAddExtInfo))
                break;
            query.str("");
            query << "select * from copyop where id == ";
            query << event_id.bitfield.event_id << ";";  
            ShowProgress(25, query.str().c_str(),kRPVDbBusy, future);
            if(kRocProfVisDmResultSuccess !=
               ExecuteSQLQuery(
                   future, DbInstancePtrAt(0), query.str().c_str(), "Copy", extdata,
                   (rocprofvis_dm_event_operation_t) event_id.bitfield.event_op,
                   &CallbackAddExtInfo))
                break;
            query.str("");
            query << "select * from kernel where id == ";
            query << event_id.bitfield.event_id << ";";  
            ShowProgress(50, query.str().c_str(),kRPVDbBusy, future);
            if(kRocProfVisDmResultSuccess !=
               ExecuteSQLQuery(
                   future, DbInstancePtrAt(0), query.str().c_str(), "Dispatch", extdata,
                   (rocprofvis_dm_event_operation_t) event_id.bitfield.event_op,
                   &CallbackAddExtInfo))
                break;

            std::string level_query =
                Builder::Select(rocprofvis_db_sqlite_essential_data_query_format(
                    { { Builder::QParamOperation(kRocProfVisDmOperationLaunch),
                        Builder::SpaceSaver(0),
                        Builder::QParam("gpuId", Builder::AGENT_ID_SERVICE_NAME),
                        Builder::QParam("queueId", Builder::QUEUE_ID_SERVICE_NAME),
                        Builder::SpaceSaver(0), Builder::QParam("L.level"),
                        Builder::SpaceSaver(0) },
                      { Builder::From("rocpd_op", MultiNode::No),
                        Builder::LeftJoin(Builder::LevelTable("op"), "L", "id = L.eid", MultiNode::No) },
                      { Builder::Where(
                          "id", "==", std::to_string(event_id.bitfield.event_id)) } }));
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