// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_db_rocprof.h"
#include <sstream>
#include <string.h>
#include <filesystem>

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
    
    auto it_op = find_track_map.find(strcmp(subprocess, "-1") == 0 ? kRPVTrackSearchIdStreams  | (node << 16) : 
        GetTrackSearchId(TranslateOperationToTrackCategory((rocprofvis_dm_event_operation_t) operation)) | (node << 16));
    if(it_op != find_track_map.end())
    {
        auto it_process = it_op->second.find(process);
        if(it_process != it_op->second.end())
        {
            auto it_subprocess = it_process->second.find(std::atoll(subprocess));
            if(it_subprocess != it_process->second.end())
            {
                track_id = it_subprocess->second;
                return kRocProfVisDmResultSuccess;
            }
        }
    }
    return kRocProfVisDmResultNotLoaded;
   
}

rocprofvis_dm_result_t RocprofDatabase::RemapStringIds(rocprofvis_db_record_data_t & record)
{
    rocprofvis_db_string_id_t string_id_category = { (uint32_t)record.event.category, (uint32_t)record.event.id.bitfield.event_node, rocprofvis_db_string_type_t::kRPVStringTypeNameOrCategory };
    rocprofvis_db_string_id_t string_id_symbol = { (uint32_t)record.event.symbol, (uint32_t)record.event.id.bitfield.event_node, record.event.id.bitfield.event_op == kRocProfVisDmOperationDispatch ? 
        rocprofvis_db_string_type_t::kRPVStringTypeKernelSymbol : 
        rocprofvis_db_string_type_t::kRPVStringTypeNameOrCategory };
    auto it_cat = m_string_index_map.find(string_id_category);
    auto it_sym = m_string_index_map.find(string_id_symbol);
    if (it_cat != m_string_index_map.end())
    {
        record.event.category = it_cat->second;
    }
    if (it_sym != m_string_index_map.end())
    {
        record.event.symbol = it_sym->second;
    }
    return kRocProfVisDmResultSuccess;
}

rocprofvis_dm_result_t RocprofDatabase::RemapStringIds(rocprofvis_db_flow_data_t& record)
{
    rocprofvis_db_string_id_t string_id_category = { (uint32_t)record.category_id, (uint32_t)record.id.bitfield.event_node, rocprofvis_db_string_type_t::kRPVStringTypeNameOrCategory };
    rocprofvis_db_string_id_t string_id_symbol = { (uint32_t)record.symbol_id, (uint32_t)record.id.bitfield.event_node, record.id.bitfield.event_op == kRocProfVisDmOperationDispatch ? 
        rocprofvis_db_string_type_t::kRPVStringTypeKernelSymbol : 
        rocprofvis_db_string_type_t::kRPVStringTypeNameOrCategory };
    auto it_cat = m_string_index_map.find(string_id_category);
    auto it_sym = m_string_index_map.find(string_id_symbol);
    if (it_cat != m_string_index_map.end())
    {
        record.category_id = it_cat->second;
    }
    if (it_sym != m_string_index_map.end())
    {
        record.symbol_id = it_sym->second;
    }
    return kRocProfVisDmResultSuccess;
}

rocprofvis_dm_result_t RocprofDatabase::StringIndexToId(rocprofvis_dm_index_t index, std::vector<rocprofvis_db_string_id_t>& ids)
{
    auto it = m_string_id_map.find(index);
    if (it != m_string_id_map.end())
    {
        ids = it->second;
    }
    return kRocProfVisDmResultSuccess;
}

rocprofvis_dm_result_t RocprofDatabase::RemapStringId(uint64_t id, rocprofvis_db_string_type_t type, uint32_t node, uint64_t & result) {
    rocprofvis_db_string_id_t string_id = { (uint32_t)id, node, type };
    result = id;
    auto it = m_string_index_map.find(string_id);
    if (it != m_string_index_map.end())
    {
        result = it->second;
        return kRocProfVisDmResultSuccess;
    }
    return kRocProfVisDmResultNotLoaded;
}

int RocprofDatabase::CallbackParseMetadata(void* data, int argc, sqlite3_stmt* stmt,
    char** azColName) {
    ROCPROFVIS_ASSERT_MSG_RETURN(argc==3, ERROR_DATABASE_QUERY_PARAMETERS_MISMATCH, 1);
    ROCPROFVIS_ASSERT_MSG_RETURN(data, ERROR_SQL_QUERY_PARAMETERS_CANNOT_BE_NULL, 1);
    void* func = (void*)&CallbackParseMetadata;
    rocprofvis_dm_track_params_t track_params = {0};
    rocprofvis_db_sqlite_callback_parameters* callback_params = (rocprofvis_db_sqlite_callback_parameters*)data;
    RocprofDatabase* db = (RocprofDatabase*)callback_params->db;
    std::string tag = db->Sqlite3ColumnText(func, stmt, azColName, 1);
    if (tag == "schema_version")
    {
        db->db_version = db->Sqlite3ColumnText(func, stmt, azColName, 2);
    }
    callback_params->future->CountThisRow();
    return 0;
}

int RocprofDatabase::ProcessTrack(rocprofvis_dm_track_params_t& track_params, rocprofvis_dm_charptr_t*  newqueries)
{
    ROCPROFVIS_ASSERT_MSG_RETURN(track_params.db_instance != nullptr, ERROR_NODE_KEY_CANNOT_BE_NULL, 1);
    DbInstance* db_instance = (DbInstance*)track_params.db_instance;
    rocprofvis_dm_track_params_it it = FindTrack(track_params.process, db_instance);
    UpdateQueryForTrack(it, track_params, newqueries);
    if(it == TrackPropertiesEnd())
    {
        find_track_map[GetTrackSearchId(track_params.process.category) | db_instance->GuidIndex() << 16]
            [track_params.process.id[TRACK_ID_PID_OR_AGENT]]
            [track_params.process.id[TRACK_ID_TID_OR_QUEUE]] =
            track_params.track_id;
        if(track_params.process.category == kRocProfVisDmRegionMainTrack ||
            track_params.process.category == kRocProfVisDmRegionSampleTrack)
        {
            track_params.process.name[TRACK_ID_PID] = CachedTables(db_instance->GuidIndex())->GetTableCell("Process", track_params.process.id[TRACK_ID_PID], "command");
            track_params.process.name[TRACK_ID_PID] += "(";
            track_params.process.name[TRACK_ID_PID] += std::to_string(track_params.process.id[TRACK_ID_PID]);
            track_params.process.name[TRACK_ID_PID] += ")";

            track_params.process.name[TRACK_ID_TID] = CachedTables(db_instance->GuidIndex())->GetTableCell("Thread", track_params.process.id[TRACK_ID_TID], "name");
            track_params.process.name[TRACK_ID_TID] += "(";
            track_params.process.name[TRACK_ID_TID] += std::to_string(track_params.process.id[TRACK_ID_TID]);
            track_params.process.name[TRACK_ID_TID] += ")";
        }
        else if(track_params.process.category == kRocProfVisDmKernelDispatchTrack ||
            track_params.process.category == kRocProfVisDmMemoryAllocationTrack ||
            track_params.process.category == kRocProfVisDmMemoryCopyTrack ||
            track_params.process.category == kRocProfVisDmPmcTrack)
        {
            track_params.process.name[TRACK_ID_AGENT] = CachedTables(db_instance->GuidIndex())->GetTableCell("Agent", track_params.process.id[TRACK_ID_AGENT], "product_name");
            track_params.process.name[TRACK_ID_AGENT] += "(";
            track_params.process.name[TRACK_ID_AGENT] += CachedTables(db_instance->GuidIndex())->GetTableCell("Agent", track_params.process.id[TRACK_ID_AGENT], "type_index");
            track_params.process.name[TRACK_ID_AGENT] += ")";
            if(track_params.process.category == kRocProfVisDmKernelDispatchTrack ||
                track_params.process.category == kRocProfVisDmMemoryAllocationTrack ||
                track_params.process.category == kRocProfVisDmMemoryCopyTrack)
            {
                track_params.process.name[TRACK_ID_QUEUE] = CachedTables(db_instance->GuidIndex())->GetTableCell("Queue", track_params.process.id[TRACK_ID_QUEUE], "name");
            }
            else {
                track_params.process.name[TRACK_ID_QUEUE] = CachedTables(db_instance->GuidIndex())->GetTableCell("PMC", track_params.process.id[TRACK_ID_QUEUE], "name");
            }

        }
        else if(track_params.process.category == kRocProfVisDmStreamTrack)
        {
            track_params.process.name[TRACK_ID_STREAM] = CachedTables(db_instance->GuidIndex())->GetTableCell("Stream", track_params.process.id[TRACK_ID_STREAM], "name");
        }
        if (kRocProfVisDmResultSuccess != AddTrackProperties(track_params)) return 1;
        if (BindObject()->FuncAddTrack(BindObject()->trace_object, TrackPropertiesLast()) != kRocProfVisDmResultSuccess) return 1;  
        if (CachedTables(db_instance->GuidIndex())->PopulateTrackExtendedDataTemplate(this,db_instance->GuidIndex(),  "Node", track_params.process.id[TRACK_ID_NODE]) != kRocProfVisDmResultSuccess) return 1;
        if(track_params.process.category == kRocProfVisDmRegionMainTrack ||
            track_params.process.category == kRocProfVisDmRegionSampleTrack)
        {
            if (CachedTables(db_instance->GuidIndex())->PopulateTrackExtendedDataTemplate(this, db_instance->GuidIndex(),  "Process", track_params.process.id[TRACK_ID_PID]) != kRocProfVisDmResultSuccess) return 1;
            if (CachedTables(db_instance->GuidIndex())->PopulateTrackExtendedDataTemplate(this, db_instance->GuidIndex(), "Thread", track_params.process.id[TRACK_ID_TID]) != kRocProfVisDmResultSuccess) return 1;
        } 
        else if(track_params.process.category == kRocProfVisDmStreamTrack)
        {
            if (CachedTables(db_instance->GuidIndex())->PopulateTrackExtendedDataTemplate(this, db_instance->GuidIndex(), "Stream", track_params.process.id[TRACK_ID_STREAM]) != kRocProfVisDmResultSuccess) return 1; 
        }
        else if(track_params.process.category == kRocProfVisDmKernelDispatchTrack ||
            track_params.process.category == kRocProfVisDmMemoryAllocationTrack ||
            track_params.process.category == kRocProfVisDmMemoryCopyTrack ||
            track_params.process.category == kRocProfVisDmPmcTrack)
        {
            if (CachedTables(db_instance->GuidIndex())->PopulateTrackExtendedDataTemplate(this, db_instance->GuidIndex(), "Agent", track_params.process.id[TRACK_ID_AGENT]) != kRocProfVisDmResultSuccess) return 1; 
            if(track_params.process.category == kRocProfVisDmKernelDispatchTrack ||
                track_params.process.category == kRocProfVisDmMemoryAllocationTrack ||
                track_params.process.category == kRocProfVisDmMemoryCopyTrack)
            {
                if (CachedTables(db_instance->GuidIndex())->PopulateTrackExtendedDataTemplate(this, db_instance->GuidIndex(), "Queue", track_params.process.id[TRACK_ID_QUEUE]) != kRocProfVisDmResultSuccess) return 1;
            }
            else
            {
                if(track_params.process.id[TRACK_ID_QUEUE] > 0)
                    if (CachedTables(db_instance->GuidIndex())->PopulateTrackExtendedDataTemplate(this, db_instance->GuidIndex(), "PMC", track_params.process.id[TRACK_ID_QUEUE]) != kRocProfVisDmResultSuccess) return 1;
            }
        }
    }
    else
    {
        it->get()->load_id.insert(*track_params.load_id.begin());
        track_params.track_id = it->get()->track_id;
    }
    return 0;
}


int RocprofDatabase::CallBackAddString(void *data, int argc, sqlite3_stmt* stmt, char **azColName){
    ROCPROFVIS_ASSERT_MSG_RETURN(argc==3, ERROR_DATABASE_QUERY_PARAMETERS_MISMATCH, 1);
    ROCPROFVIS_ASSERT_MSG_RETURN(data, ERROR_SQL_QUERY_PARAMETERS_CANNOT_BE_NULL, 1);
    void*  func = (void*)&CallBackAddString;
    rocprofvis_db_sqlite_callback_parameters* callback_params = (rocprofvis_db_sqlite_callback_parameters*)data;
    ROCPROFVIS_ASSERT_MSG_RETURN(callback_params->db_instance != nullptr, ERROR_NODE_KEY_CANNOT_BE_NULL, 1);
    RocprofDatabase* db = (RocprofDatabase*)callback_params->db;
    if(callback_params->future->Interrupted()) return SQLITE_ABORT;
    std::lock_guard<std::mutex> lock(db->m_lock);
    uint32_t type = db->Sqlite3ColumnInt(func, stmt, azColName, 0);
    uint32_t id = db->Sqlite3ColumnInt(func, stmt, azColName, 1);
    char* str = (char*)db->Sqlite3ColumnText(func, stmt, azColName, 2);
    auto it = db->m_string_map.find(str);
    uint32_t string_index = it != db->m_string_map.end() ? it->second : db->BindObject()->FuncAddString(db->BindObject()->trace_object, str);
    rocprofvis_db_string_id_t string_id_key = { id, callback_params->db_instance->GuidIndex(), (rocprofvis_db_string_type_t)type};
    db->m_string_index_map[string_id_key] = string_index;
    db->m_string_id_map[string_index].push_back(string_id_key);
    db->m_string_map[str] = string_index;
    callback_params->future->CountThisRow();
    return 0;
}


int RocprofDatabase::CallbackCacheTable(void *data, int argc, sqlite3_stmt* stmt, char **azColName){
    ROCPROFVIS_ASSERT_MSG_RETURN(data, ERROR_SQL_QUERY_PARAMETERS_CANNOT_BE_NULL, 1);
    void* func = (void*)&CallbackCacheTable;
    rocprofvis_db_sqlite_callback_parameters* callback_params = (rocprofvis_db_sqlite_callback_parameters*)data;
    RocprofDatabase* db = (RocprofDatabase*)callback_params->db;
    DatabaseCache * ref_tables = (DatabaseCache *)callback_params->handle;
    std::lock_guard<std::mutex> lock(db->m_lock);
    if (callback_params->future->GetProcessedRowsCount() == 0)
    {
        for (int i = 0; i < argc; i++)
        {
            ref_tables->AddTableColumn(callback_params->query[kRPVCacheTableName], azColName[i]);
        }
    }

    uint64_t id = db->Sqlite3ColumnInt64(func, stmt, azColName, 0);
    ref_tables->AddTableRow(callback_params->query[kRPVCacheTableName], id);
    for (int i = 0; i < argc; i++)
    {
        ref_tables->AddTableCell(callback_params->query[kRPVCacheTableName], id, i,
            (char*)db->Sqlite3ColumnText(func, stmt, azColName, i));
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
    uint32_t         file_node             = db->Sqlite3ColumnInt(func, stmt, azColName, 0);
    std::string      table_name            = (char*) db->Sqlite3ColumnText(func, stmt, azColName, 1);
    size_t           pos                   = table_name.find(table_name_befor_guid);
    if(pos == 0)
    {
        guid_list->push_back({ DbInstance(file_node, guid_list->size()), table_name.substr(table_name_befor_guid.length()) });
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
    uint32_t file_node_id = -1;
    rocprofvis_dm_result_t result = kRocProfVisDmResultNotLoaded;
    std::vector<std::thread> threads;
    auto task = [&](std::vector<std::string> queries, uint32_t db_node_id) {
        result = ExecuteTransaction(vec, file_node_id);
        };
    for (auto& guid_info : DbInstances())
    {
        if (file_node_id == -1)
        {
            file_node_id = guid_info.first.FileIndex();
        }
        else
        if (file_node_id != guid_info.first.FileIndex() && vec.size() > 0)
        {
            threads.emplace_back(task, vec, file_node_id);            
            vec.clear();
            if (result != kRocProfVisDmResultSuccess)
            {
                break;
            }
        }
        if(m_query_factory.IsVersionGreaterOrEqual("4"))
        {
            vec.push_back(std::string("CREATE INDEX IF NOT EXISTS rocpd_region_track_idx_") + guid_info.second +
                     " ON rocpd_track_" + guid_info.second + "(nid,pid,tid);");
            vec.push_back(std::string("CREATE INDEX IF NOT EXISTS rocpd_kernel_dispatch_track_idx_") + guid_info.second +
                     " ON rocpd_track_" + guid_info.second + "(nid,agent_id,queue_id);");
            vec.push_back(std::string("CREATE INDEX IF NOT EXISTS rocpd_stream_idx_") + guid_info.second +
                     " ON rocpd_track_" + guid_info.second + "(nid,stream_id);");
        } else
        {
            vec.push_back(std::string("CREATE INDEX IF NOT EXISTS rocpd_region_idx_") + guid_info.second +
                     " ON rocpd_region_" + guid_info.second + "(nid,pid,tid);");
            vec.push_back(std::string("CREATE INDEX IF NOT EXISTS rocpd_kernel_dispatch_idx_") + guid_info.second +
                     " ON rocpd_kernel_dispatch_" + guid_info.second + "(nid,agent_id,queue_id);");
            vec.push_back(std::string("CREATE INDEX IF NOT EXISTS rocpd_kernel_dispatch_stream_idx_") + guid_info.second +
                     " ON rocpd_kernel_dispatch_" + guid_info.second + "(nid,stream_id);");
            vec.push_back(std::string("CREATE INDEX IF NOT EXISTS rocpd_memory_allocate_idx_") + guid_info.second +
                     " ON rocpd_memory_allocate_" + guid_info.second + "(nid,agent_id,queue_id);");
            vec.push_back(std::string("CREATE INDEX IF NOT EXISTS rocpd_memory_allocate_stream_idx_") + guid_info.second +
                     " ON rocpd_memory_allocate_" + guid_info.second + "(nid,stream_id);");
            vec.push_back(std::string("CREATE INDEX IF NOT EXISTS rocpd_memory_copy_idx_") + guid_info.second +
                     " ON rocpd_memory_copy_" + guid_info.second + "(nid,dst_agent_id,queue_id);");
            vec.push_back(std::string("CREATE INDEX IF NOT EXISTS rocpd_memory_copy_stream_idx_") + guid_info.second +
                     " ON rocpd_memory_copy_" + guid_info.second + "(nid,stream_id);");

        }
        vec.push_back(std::string("CREATE INDEX IF NOT EXISTS rocpd_region_event_idx_") + guid_info.second +
                    " ON rocpd_region_" + guid_info.second + "(event_id);");
        vec.push_back(std::string("CREATE INDEX IF NOT EXISTS rocpd_sample_event_idx_") + guid_info.second +
                    " ON rocpd_sample_" + guid_info.second + "(event_id);");
        vec.push_back(std::string("CREATE INDEX IF NOT EXISTS rocpd_region_stack_idx_") + guid_info.second +
                    " ON rocpd_event_" + guid_info.second + "(stack_id);");
    }
    if (vec.size() > 0 && file_node_id!=-1)
    {
        threads.emplace_back(task, vec, file_node_id);            
    }
    for (auto& t : threads)
        t.join();
    return kRocProfVisDmResultSuccess;
}

rocprofvis_dm_result_t RocprofDatabase::LoadInformationTables(Future* future) {

    std::vector<std::thread> threads;
    rocprofvis_dm_result_t result = kRocProfVisDmResultNotLoaded;

    auto task = [&](DbInstance * db_instance, std::string query, std::string tag) {
            Future* sub_future = future->AddSubFuture();
            result = ExecuteSQLQuery(sub_future, db_instance, query.c_str(), tag.c_str(), (rocprofvis_dm_handle_t)CachedTables(db_instance->GuidIndex()), &CallbackCacheTable);
            future->DeleteSubFuture(sub_future);
        };

    std::vector<std::pair<std::string, std::string>> info_table_list = {
        {"Node", "SELECT * from rocpd_info_node_%GUID%;"},
        {"Agent", "SELECT * from rocpd_info_agent_%GUID%;"},
        {"Queue", "SELECT * from rocpd_info_queue_%GUID%;"},
        {"Stream", "SELECT * from rocpd_info_stream_%GUID%;"},
        {"Process", "SELECT * from rocpd_info_process_%GUID%;"},
        {"Thread", "SELECT * from rocpd_info_thread_%GUID%;"},
        {"PMC", "SELECT id, guid, nid, pid, agent_id, target_arch, COALESCE(event_code,0) as event_code, COALESCE(instance_id,0) as instance_id, name, symbol, description, long_description, component, units, value_type, block, expression, is_constant, is_derived, extdata from rocpd_info_pmc_%GUID%;"},
        {"StreamToQueue", "SELECT ROW_NUMBER() OVER (ORDER BY tuple) AS id, d.stream_id, d.queue_id FROM (SELECT DISTINCT concat(nid,'-',pid,'-',stream_id,'-',queue_id) as tuple, stream_id, queue_id FROM rocpd_kernel_dispatch_%GUID%) AS d;"},
        {"AgentToStream", "SELECT ROW_NUMBER() OVER (ORDER BY tuple) AS id, d.agent_id, d.stream_id FROM (SELECT DISTINCT concat(nid,'-',pid,'-',agent_id,'-',stream_id) as tuple, agent_id, stream_id FROM rocpd_kernel_dispatch_%GUID%) AS d;"},
        {"AgentToQueue", "SELECT ROW_NUMBER() OVER (ORDER BY tuple) AS id, d.agent_id, d.queue_id FROM (SELECT DISTINCT concat(nid,'-',pid,'-',agent_id,'-',queue_id) as tuple, agent_id, queue_id FROM rocpd_kernel_dispatch_%GUID%) AS d;"}
    };
    for (auto& guid_info : DbInstances())
    {
        for (auto table : info_table_list)
        {
            threads.emplace_back(
                task, 
                &guid_info.first,
                table.second, 
                table.first);
        }
    }
    for (auto& t : threads)
        t.join();

    if (result == kRocProfVisDmResultSuccess)
    {
        for (auto& guid_info : DbInstances())
        {
            for (auto table : info_table_list)
            {
                auto handle = CachedTables(guid_info.first.GuidIndex())->GetTableHandle(table.first.c_str());
                BindObject()->FuncAddInfoTable(BindObject()->trace_object, guid_info.first.GuidIndex(), table.first.c_str(), handle);
            }
        }
    }
    return result;
}

rocprofvis_dm_result_t  RocprofDatabase::ReadTraceMetadata(Future* future)
{
    ROCPROFVIS_ASSERT_MSG_RETURN(future, ERROR_FUTURE_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
    while (true)
    {
        ROCPROFVIS_ASSERT_MSG_BREAK(BindObject()->trace_properties, ERROR_TRACE_PROPERTIES_CANNOT_BE_NULL);
        std::string value;
        rocprofvis_dm_result_t result = kRocProfVisDmResultSuccess;

        ShowProgress(2, "Detect Nodes", kRPVDbBusy, future);
        for (auto& file_node : m_db_nodes)
        {
            ExecuteSQLQuery(future,
                &TemporaryDbInstance(file_node->node_id),
                (std::string("SELECT ") + std::to_string(file_node->node_id)+" as file_node, name from sqlite_master where type = 'table' and name like 'rocpd_info_node_%'; ").c_str(),
                "rocpd_info_node_", (rocprofvis_dm_handle_t)&DbInstances(),
                &CallbackNodeEnumeration);
        }

        TraceProperties()->num_db_instances = DbInstances().size();

        ShowProgress(1, "Get version", kRPVDbBusy, future);
        std::string version;
        for (auto& guid_info : DbInstances())
        {
            if ((result = kRocProfVisDmResultSuccess) != ExecuteSQLQuery(future, &guid_info.first, "SELECT * FROM rocpd_metadata_%GUID%;", &CallbackParseMetadata)) break;
            if (!version.empty())
            {
                if (version != db_version)
                {
                    spdlog::debug("Cannot support different version database nodes!");
                    result = kRocProfVisDmResultNotSupported;
                    break;
                }
            }
        }
        if (result != kRocProfVisDmResultSuccess)
            break;
        m_query_factory.SetVersion(db_version.c_str());

        ShowProgress(5, "Indexing tables", kRPVDbBusy, future);
        CreateIndexes();

        

        ShowProgress(10, "Load Information Tables", kRPVDbBusy, future);
        LoadInformationTables(future);

        TraceProperties()->events_count[kRocProfVisDmOperationLaunch]   = 0;
        TraceProperties()->events_count[kRocProfVisDmOperationDispatch] = 0;
        TraceProperties()->events_count[kRocProfVisDmOperationMemoryAllocate] = 0;
        TraceProperties()->events_count[kRocProfVisDmOperationMemoryCopy]     = 0;
        TraceProperties()->events_count[kRocProfVisDmOperationLaunchSample]   = 0;

        TraceProperties()->tracks_info_restored = true;
        TraceProperties()->tracks_info_id_mismatch = false;
        TraceProperties()->start_time = UINT64_MAX;
        TraceProperties()->end_time = 0;

        std::string track_queries = m_query_factory.GetRocprofRegionTrackQuery(false) +
            m_query_factory.GetRocprofRegionTrackQuery(true) +
            m_query_factory.GetRocprofKernelDispatchTrackQuery() +
            m_query_factory.GetRocprofKernelDispatchTrackQueryForStream() +
            m_query_factory.GetRocprofMemoryAllocTrackQuery() +
            m_query_factory.GetRocprofMemoryAllocTrackQueryForStream() +
            m_query_factory.GetRocprofMemoryCopyTrackQuery() +
            m_query_factory.GetRocprofMemoryCopyTrackQueryForStream() +
            m_query_factory.GetRocprofPerformanceCountersTrackQuery() +
            m_query_factory.GetRocprofSMIPerformanceCountersTrackQuery();
        size_t track_queries_hash_value = std::hash<std::string>{}(track_queries);
        uint32_t load_id = 0;
        m_add_track_mutex.Init(DbInstances());
        ShowProgress(5, "Adding HIP API tracks", kRPVDbBusy, future );
        {
            std::vector<std::thread> threads;
            m_add_track_mutex.reset();
            auto task = [&](DbInstance* db_instance)
                {
                    Future* sub_future = future->AddSubFuture();
                    result = ExecuteSQLQuery(sub_future, db_instance, track_queries_hash_value, load_id,
                        {
                            m_query_factory.GetRocprofRegionTrackQuery(false),
                            "",
                            m_query_factory.GetRocprofRegionLevelQuery(false),
                            m_query_factory.GetRocprofRegionSliceQuery(false),
                            "",
                            m_query_factory.GetRocprofRegionTableQuery(false),
                        },
                        &CallBackAddTrack, &CallBackLoadTrack);
                    future->DeleteSubFuture(sub_future);
                    m_add_track_mutex.unlock();
                };
            for (auto& guid_info : DbInstances())
            {
                threads.emplace_back(task, &guid_info.first);
            }
            for (auto& t : threads)
                t.join();
            load_id++;
        }


        ShowProgress(5, "Adding HIP API Sample tracks", kRPVDbBusy, future );
        {
            std::vector<std::thread> threads;
            m_add_track_mutex.reset();
            auto task = [&](DbInstance* db_instance)
                {

                    Future* sub_future = future->AddSubFuture();
                    result = ExecuteSQLQuery(sub_future, db_instance, track_queries_hash_value, load_id,
                    { 
                        m_query_factory.GetRocprofRegionTrackQuery(true),
                        "",
                        m_query_factory.GetRocprofRegionLevelQuery(true),
                        m_query_factory.GetRocprofRegionSliceQuery(true),
                        "",
                        m_query_factory.GetRocprofRegionTableQuery(true),
                    },
                    &CallBackAddTrack, &CallBackLoadTrack);
                    future->DeleteSubFuture(sub_future);
                    m_add_track_mutex.unlock();
                };
            for (auto& guid_info : DbInstances())
            {
                threads.emplace_back(task, &guid_info.first);
            }
            for (auto& t : threads)
                t.join();
            load_id++;
        }

        ShowProgress(5, "Adding kernel dispatch tracks", kRPVDbBusy, future );
        {
            std::vector<std::thread> threads;
            m_add_track_mutex.reset();
            auto task = [&](DbInstance* db_instance)
                {
                    Future* sub_future = future->AddSubFuture();
                    result = ExecuteSQLQuery(sub_future, db_instance, track_queries_hash_value, load_id,
                    { 
                        m_query_factory.GetRocprofKernelDispatchTrackQuery(),
                        m_query_factory.GetRocprofKernelDispatchTrackQueryForStream(),
                        m_query_factory.GetRocprofKernelDispatchLevelQuery(),
                        m_query_factory.GetRocprofKernelDispatchSliceQuery(),
                        m_query_factory.GetRocprofKernelDispatchSliceQueryForStream(),
                        m_query_factory.GetRocprofKernelDispatchTableQuery(),
                    },
                    &CallBackAddTrack, &CallBackLoadTrack);
                    future->DeleteSubFuture(sub_future);
                    m_add_track_mutex.unlock();
                };
            for (auto& guid_info : DbInstances())
            {
                threads.emplace_back(task, &guid_info.first);
            }
            for (auto& t : threads)
                t.join();
            load_id++;
        }

        ShowProgress(5, "Adding memory allocation tracks", kRPVDbBusy, future );
        {
            std::vector<std::thread> threads;
            m_add_track_mutex.reset();
            auto task = [&](DbInstance* db_instance)
                {
                    Future* sub_future = future->AddSubFuture();
                    result = ExecuteSQLQuery(sub_future, db_instance, track_queries_hash_value, load_id,
                    { 
                        m_query_factory.GetRocprofMemoryAllocTrackQuery(),
                        m_query_factory.GetRocprofMemoryAllocTrackQueryForStream(),
                        m_query_factory.GetRocprofMemoryAllocLevelQuery(),
                        m_query_factory.GetRocprofMemoryAllocSliceQuery(),
                        m_query_factory.GetRocprofMemoryAllocSliceQueryForStream(),
                        m_query_factory.GetRocprofMemoryAllocTableQuery(),
                    },
                    &CallBackAddTrack, &CallBackLoadTrack);
                    future->DeleteSubFuture(sub_future);
                    m_add_track_mutex.unlock();
                };
            for (auto& guid_info : DbInstances())
            {
                threads.emplace_back(task, &guid_info.first);
            }
            for (auto& t : threads)
                t.join();
            load_id++;
        }

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
        {
            std::vector<std::thread> threads;
            m_add_track_mutex.reset();
            auto task = [&](DbInstance* db_instance)
                {
                    Future* sub_future = future->AddSubFuture();
                    result = ExecuteSQLQuery(sub_future, db_instance, track_queries_hash_value, load_id,
                    { 
                        m_query_factory.GetRocprofMemoryCopyTrackQuery(),
                        m_query_factory.GetRocprofMemoryCopyTrackQueryForStream(),
                        m_query_factory.GetRocprofMemoryCopyLevelQuery(),
                        m_query_factory.GetRocprofMemoryCopySliceQuery(),
                        m_query_factory.GetRocprofMemoryCopySliceQueryForStream(),
                        m_query_factory.GetRocprofMemoryCopyTableQuery(),
                    },
                    &CallBackAddTrack, &CallBackLoadTrack);
                    future->DeleteSubFuture(sub_future);
                    m_add_track_mutex.unlock();
                };
            for (auto& guid_info : DbInstances())
            {
                threads.emplace_back(task, &guid_info.first);
            }
            for (auto& t : threads)
                t.join();
            load_id++;
        }

        // PMC schema is not fully defined yet
        ShowProgress(5, "Adding performance counters tracks", kRPVDbBusy, future );
        {
            std::vector<std::thread> threads;
            m_add_track_mutex.reset();
            auto task = [&](DbInstance* db_instance)
                {
                    Future* sub_future = future->AddSubFuture();
                    result = ExecuteSQLQuery(sub_future, db_instance, track_queries_hash_value, load_id,
                    { 
                        m_query_factory.GetRocprofPerformanceCountersTrackQuery(),
                        "",
                        m_query_factory.GetRocprofPerformanceCountersLevelQuery(),
                        m_query_factory.GetRocprofPerformanceCountersSliceQuery(),
                        "",
                        m_query_factory.GetRocprofPerformanceCountersTableQuery(),
                    },
                    &CallBackAddTrack, &CallBackLoadTrack);
                    future->DeleteSubFuture(sub_future);
                    m_add_track_mutex.unlock();
                };
            for (auto& guid_info : DbInstances())
            {
                threads.emplace_back(task, &guid_info.first);
            }
            for (auto& t : threads)
                t.join();
            load_id++;
        }
                   
        // PMC schema is not fully defined yet
        ShowProgress(5, "Adding performance smi counters tracks", kRPVDbBusy, future );
        {
            std::vector<std::thread> threads;
            m_add_track_mutex.reset();
            auto task = [&](DbInstance* db_instance)
                {
                    Future* sub_future = future->AddSubFuture();
                    result = ExecuteSQLQuery(sub_future, db_instance, track_queries_hash_value, load_id,
                    { 
                        m_query_factory.GetRocprofSMIPerformanceCountersTrackQuery(),
                        "",
                        m_query_factory.GetRocprofSMIPerformanceCountersLevelQuery(),
                        m_query_factory.GetRocprofSMIPerformanceCountersSliceQuery(),
                        "",
                        m_query_factory.GetRocprofSMIPerformanceCountersTableQuery(),
                    },
                    &CallBackAddTrack, &CallBackLoadTrack);
                    future->DeleteSubFuture(sub_future);
                    m_add_track_mutex.unlock();
                };
            for (auto& guid_info : DbInstances())
            {
                threads.emplace_back(task, &guid_info.first);
            }
            for (auto& t : threads)
                t.join();
            load_id++;
        }
                                                                    

        ShowProgress(10, "Loading strings", kRPVDbBusy, future );
        BindObject()->FuncAddString(BindObject()->trace_object, ""); // 0 index string
        {
            std::vector<std::thread> threads;
            m_add_track_mutex.reset();
            auto task = [&](DbInstance* db_instance)
                {
                    Future* sub_future = future->AddSubFuture();
                    result = ExecuteSQLQuery(sub_future, db_instance, (std::string("SELECT ") + 
                        std::to_string((uint32_t)rocprofvis_db_string_type_t::kRPVStringTypeNameOrCategory) + 
                        ", id, string FROM rocpd_string_%GUID% ORDER BY id; ").c_str(), &CallBackAddString);
                    future->DeleteSubFuture(sub_future);
                };
            for (auto& guid_info : DbInstances())
            {
                threads.emplace_back(task, &guid_info.first);
            }
            for (auto& t : threads)
                t.join();
        }

        ShowProgress(10, "Loading kenel symbols", kRPVDbBusy, future );
        {
            std::vector<std::thread> threads;
            m_add_track_mutex.reset();
            auto task = [&](DbInstance* db_instance)
                {
                    Future* sub_future = future->AddSubFuture();
                    result = ExecuteSQLQuery(sub_future, db_instance, (std::string("SELECT ") + 
                        std::to_string((uint32_t)rocprofvis_db_string_type_t::kRPVStringTypeKernelSymbol) + 
                        ", id, display_name FROM rocpd_info_kernel_symbol_%GUID%;").c_str(), &CallBackAddString);
                    future->DeleteSubFuture(sub_future);
                };
            for (auto& guid_info : DbInstances())
            {
                threads.emplace_back(task, &guid_info.first);
            }
            for (auto& t : threads)
                t.join();
        }

        m_string_map.clear();

        std::vector<std::pair<std::string, rocprofvis_dm_event_operation_t>> operation_strings =  {
            {"launch",kRocProfVisDmOperationLaunch},
            {"launch_sample",kRocProfVisDmOperationLaunchSample},
            {"dispatch", kRocProfVisDmOperationDispatch},
            {"mem_alloc",kRocProfVisDmOperationMemoryAllocate},
            {"mem_copy", kRocProfVisDmOperationMemoryCopy} 
        };

        for (auto operation : operation_strings)
        {
            std::vector<std::string> to_drop;
            Builder::OldLevelTables(operation.first, to_drop);
            for (auto& guid_info : DbInstances())
            {
                Builder::OldLevelTables(operation.first, to_drop, guid_info.second);
                for (auto table : to_drop)
                {
                    DropSQLTable(table.c_str(), guid_info.first.FileIndex());
                }
            }
        }

        ShowProgress(10, "Calculating event levels", kRPVDbBusy, future);
        guid_list_t calculate_level_for_guids;
        bool event_levels_exist = true;
        for (auto& guid_info : DbInstances())
        {            
            for (auto operation : operation_strings)
            {
                if (SQLITE_OK != DetectTable(GetServiceConnection(guid_info.first.FileIndex()), Builder::LevelTable(operation.first, guid_info.second).c_str(), false))
                {
                    event_levels_exist = false;
                    break;
                }
            }
        }

        if (!event_levels_exist)
        {
            for (auto& guid_info : DbInstances())
            {
                calculate_level_for_guids.push_back(guid_info);
                for (auto operation : operation_strings)
                {
                    m_event_levels[kRocProfVisDmOperationLaunch][guid_info.first.GuidIndex()].reserve(
                        TraceProperties()->events_count[operation.second]);
                }
            }

            if (kRocProfVisDmResultSuccess !=
                ExecuteQueryForAllTracksAsync(
                    kRocProfVisDmIncludeStreamTracks,
                    kRPVQueryLevel, "SELECT *, ", (std::string(" ORDER BY ") + Builder::START_SERVICE_NAME).c_str(), &CalculateEventLevels,
                    [](rocprofvis_dm_track_params_t* params) {
                        params->m_active_events.clear();
                    }, calculate_level_for_guids))
            {
                break;
            }

            for (int j = 0; j < kRocProfVisDmNumOperation; j++)
            {
                for (auto it = m_event_levels[j].begin(); it != m_event_levels[j].end(); ++it)
                {
                    if (it->second.size() > 0)
                    {
                        std::sort(it->second.begin(),
                            it->second.end(),
                            [](const rocprofvis_db_event_level_t& a,
                                const rocprofvis_db_event_level_t& b) {
                                    return a.id < b.id;
                            });
                    }
                }
            }

            SQLInsertParams params[] = { { "eid", "INTEGER PRIMARY KEY" }, { "level", "INTEGER" } , { "level_for_stream", "INTEGER" } };
            for (auto operation : operation_strings)
            {
                for (auto& guid_info : DbInstances())
                {
                    CreateSQLTable(
                        Builder::LevelTable(operation.first, guid_info.second).c_str(), params, 3,
                        m_event_levels[operation.second][guid_info.first.GuidIndex()].size(),
                        [&](sqlite3_stmt* stmt, int index) {
                            sqlite3_bind_int64(
                                stmt, 1,
                                m_event_levels[operation.second][guid_info.first.GuidIndex()][index].id);
                            sqlite3_bind_int(
                                stmt, 2,
                                m_event_levels[operation.second][guid_info.first.GuidIndex()][index].level_for_queue);
                            sqlite3_bind_int(
                                stmt, 3,
                                m_event_levels[operation.second][guid_info.first.GuidIndex()][index].level_for_stream);
                        }, guid_info.first.FileIndex());
                    m_event_levels[operation.second][guid_info.first.GuidIndex()].clear();
                    m_event_levels_id_to_index[operation.second][guid_info.first.GuidIndex()].clear();
                }
            }
        }

        if (!TraceProperties()->tracks_info_restored)
        {
            ShowProgress(5, "Collecting track properties",
                kRPVDbBusy, future);
            TraceProperties()->start_time = UINT64_MAX;
            TraceProperties()->end_time = 0;
            if (kRocProfVisDmResultSuccess !=
                ExecuteQueryForAllTracksAsync(
                    kRocProfVisDmIncludePmcTracks | kRocProfVisDmIncludeStreamTracks, kRPVQuerySliceByTrackSliceQuery,
                    "SELECT MIN(startTs), MAX(endTs), MIN(event_level), MAX(event_level), ",
                    "WHERE startTs != 0 AND endTs != 0", &CallbackGetTrackProperties,
                    [](rocprofvis_dm_track_params_t* params) {},
                    DbInstances()))
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
        DbInstance* node_ptr = DbInstancePtrAt(event_id.bitfield.event_node);
        ROCPROFVIS_ASSERT_MSG_BREAK(node_ptr!=nullptr, ERROR_NODE_KEY_CANNOT_BE_NULL);
        std::string query;
        if(event_id.bitfield.event_op == kRocProfVisDmOperationLaunch ||
           event_id.bitfield.event_op == kRocProfVisDmOperationLaunchSample)
        {
            query = m_query_factory.GetRocprofDataFlowQueryForRegionEvent(event_id.bitfield.event_id);
            ShowProgress(0, query.c_str(),kRPVDbBusy, future);
            if (kRocProfVisDmResultSuccess != ExecuteSQLQuery(future, node_ptr, query.c_str(), flowtrace, &CallbackAddFlowTrace)) break;
        } else
        if (event_id.bitfield.event_op == kRocProfVisDmOperationDispatch)
        {
            query = m_query_factory.GetRocprofDataFlowQueryForKernelDispatchEvent(event_id.bitfield.event_id);
            ShowProgress(0, query.c_str(),kRPVDbBusy, future);
            if (kRocProfVisDmResultSuccess != ExecuteSQLQuery(future, node_ptr, query.c_str(), flowtrace, &CallbackAddFlowTrace)) break;
        } else
        if (event_id.bitfield.event_op == kRocProfVisDmOperationMemoryCopy)
        {
            query = m_query_factory.GetRocprofDataFlowQueryForMemoryCopyEvent(event_id.bitfield.event_id);
            ShowProgress(0, query.c_str(), kRPVDbBusy, future);
            if (kRocProfVisDmResultSuccess != ExecuteSQLQuery(future, node_ptr, query.c_str(), flowtrace, &CallbackAddFlowTrace)) break;
        }
        else
        if (event_id.bitfield.event_op == kRocProfVisDmOperationMemoryAllocate)
        {
            query = m_query_factory.GetRocprofDataFlowQueryForMemoryAllocEvent(event_id.bitfield.event_id);
            ShowProgress(0, query.c_str(), kRPVDbBusy, future);
            if (kRocProfVisDmResultSuccess != ExecuteSQLQuery(future, node_ptr, query.c_str(), flowtrace, &CallbackAddFlowTrace)) break;
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

    auto IsVisualizerTable = [](std::string name)
        {
            bool result = name.find("histogram_") == 0 || name.find("track_info_") == 0 || name.find("event_levels_") == 0;
            return result;
        };

    auto IsSqliteTable = [](std::string name)
        {
            bool result = name == "sqlite_master" || name == "sqlite_sequence";
            return result;
        };

    ROCPROFVIS_ASSERT_MSG_RETURN(new_db_path, "New DB path cannot be NULL.",
                                 kRocProfVisDmResultInvalidParameter);
    ROCPROFVIS_ASSERT_MSG_RETURN(future, ERROR_FUTURE_CANNOT_BE_NULL,
                                 kRocProfVisDmResultInvalidParameter);
    rocprofvis_dm_result_t result = kRocProfVisDmResultInvalidParameter;

    std::string query;
    rocprofvis_db_sqlite_trim_parameters trim_tables;
    rocprofvis_db_sqlite_trim_parameters trim_views;
    rocprofvis_db_sqlite_trim_parameters rocpd_trim_views;



    std::filesystem::remove(new_db_path);

    while (true) 
{
        RocprofDatabase rpDb(new_db_path);
        result = rpDb.Open();

        if (result != kRocProfVisDmResultSuccess) break;

        for (auto& file_node : m_db_nodes)
        {
            ShowProgress(1, "Iterate over tables", kRPVDbBusy, future);
            result = ExecuteSQLQuery(future, &TemporaryDbInstance(file_node->node_id), "SELECT name, sql FROM sqlite_master WHERE type='table';", "", (rocprofvis_dm_handle_t)&trim_tables, &CallbackTrimTableQuery);

            if (result != kRocProfVisDmResultSuccess) break;

            ShowProgress(1, "Iterate over views", kRPVDbBusy, future);
            result = ExecuteSQLQuery(future, &TemporaryDbInstance(file_node->node_id), "SELECT name, sql FROM sqlite_master WHERE type='view';", "", (rocprofvis_dm_handle_t)&trim_views, &CallbackTrimTableQuery);
            if (result != kRocProfVisDmResultSuccess) break;
        }

        if (result != kRocProfVisDmResultSuccess) break;

        for (auto it = trim_views.tables.begin(); it != trim_views.tables.end(); ++it)
        {
            if (it->first.find("rocpd_") == 0)
            {
                auto new_view_it = rocpd_trim_views.tables.find(it->first);
                bool new_view = false;
                if (new_view_it == rocpd_trim_views.tables.end())
                {
                    new_view = true;
                    auto insert_result = rocpd_trim_views.tables.insert(std::make_pair(it->first, std::string("CREATE VIEW ") + it->first + " AS SELECT * FROM "));
                    if (insert_result.second)
                    {
                        new_view_it = insert_result.first;
                        for (auto table : trim_tables.tables)
                        {
                            if (table.first.find(it->first) == 0)
                            {
                                if (!new_view)
                                {
                                    new_view_it->second += " UNION ALL SELECT * FROM ";
                                }
                                new_view_it->second += table.first;
                                new_view = false;
                            }
                        }
                    }
                }
            }
        }

        if (result != kRocProfVisDmResultSuccess) break;

        for(auto const& table : trim_tables.tables)
        {
            if(!IsSqliteTable(table.first) && !IsVisualizerTable(table.first))
            {
                if(result == kRocProfVisDmResultSuccess)
                {
                    result = rpDb.ExecuteSQLQuery(future,&TemporaryDbInstance(0),  table.second.c_str());
                }
                else
                {
                    break;
                }
            }
        }

        if (result != kRocProfVisDmResultSuccess) break;

       
        for (auto& file_node : m_db_nodes)
        {
            ShowProgress(0, "Attaching old DB to new", kRPVDbBusy, future);

            result = rpDb.ExecuteSQLQuery(future, &TemporaryDbInstance(0), (std::string("ATTACH DATABASE '") + file_node->filepath + "' as 'oldDb';").c_str());

            if (result != kRocProfVisDmResultSuccess) break;

            for (auto const& table : trim_tables.tables)
            {
                if (!IsSqliteTable(table.first) && !IsVisualizerTable(table.first) && CheckTableExists(table.first, file_node->node_id))
                {
                    if (result == kRocProfVisDmResultSuccess)
                    {
                        std::string msg = "Copy table " + table.first;
                        ShowProgress(1, msg.c_str(), kRPVDbBusy, future);

                        if (result == kRocProfVisDmResultSuccess)
                        {
                            if (strstr(table.first.c_str(), "rocpd_kernel_dispatch") ||
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
                            else if (strstr(table.first.c_str(), "rocpd_sample"))
                            {
                                query = "INSERT INTO ";
                                query += table.first;
                                query += " SELECT S.* FROM oldDb.";
                                query += table.first;
                                query += " S LEFT JOIN rocpd_region R ON  S.event_id = R.event_id AND S.guid = R.guid ";
                                query += " WHERE (timestamp < ";
                                query += std::to_string(end);
                                query += " AND timestamp > ";
                                query += std::to_string(start);
                                query += ") OR R.start == S.timestamp";
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

                            result = rpDb.ExecuteSQLQuery(future,&TemporaryDbInstance(0), query.c_str());
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

            ShowProgress(0, "Detaching old DB", kRPVDbBusy, future);
            result = rpDb.ExecuteSQLQuery(future,&TemporaryDbInstance(0), "DETACH oldDb;");

            if (result != kRocProfVisDmResultSuccess) break;

        }

        if (result != kRocProfVisDmResultSuccess) break;

        for(auto const& view : trim_views.tables)
        {
            if(result == kRocProfVisDmResultSuccess)
            {
                auto it = rocpd_trim_views.tables.find(view.first);
                if (it != rocpd_trim_views.tables.end())
                {
                    result = rpDb.ExecuteSQLQuery(future,&TemporaryDbInstance(0),  it->second.c_str());
                }
                else
                {
                    result = rpDb.ExecuteSQLQuery(future, &TemporaryDbInstance(0), view.second.c_str());
                }
            }
            else
            {
                break;
            }
        }

        if (result != kRocProfVisDmResultSuccess) break;

        ShowProgress(100, "Trace trimmed successfully!", kRPVDbSuccess, future);
        return future->SetPromise(result);
    }


    ShowProgress(0, "Failed to trim track!", kRPVDbError, future);
    return future->SetPromise(result);
}

rocprofvis_dm_result_t RocprofDatabase::BuildTableStringIdFilter( rocprofvis_dm_num_string_table_filters_t num_string_table_filters, 
    rocprofvis_dm_string_table_filters_t string_table_filters, table_string_id_filter_map_t& filter)
{
    rocprofvis_dm_result_t result = kRocProfVisDmResultNotLoaded;
    if(num_string_table_filters > 0)
    {
        std::vector<rocprofvis_dm_index_t> string_indices;
        result = BindObject()->FuncGetStringIndices(BindObject()->trace_object, num_string_table_filters, string_table_filters, string_indices);
        ROCPROFVIS_ASSERT_RETURN(result == kRocProfVisDmResultSuccess, result);
        std::unordered_map<uint32_t, std::string> string_ids;
        std::unordered_map<uint32_t, std::string> kernel_ids;
        for(const rocprofvis_dm_index_t& index : string_indices)
        {
            std::vector<rocprofvis_db_string_id_t> string_id_array;
            result = StringIndexToId(index, string_id_array);
            ROCPROFVIS_ASSERT_RETURN(result == kRocProfVisDmResultSuccess && string_id_array.size() > 0, result);
            for (auto string_id_obj : string_id_array)
            {
                
                if (string_id_obj.m_string_type == rocprofvis_db_string_type_t::kRPVStringTypeKernelSymbol)
                {
                    auto it = kernel_ids.find(string_id_obj.m_guid_id);
                    if (it == kernel_ids.end())
                    {
                        kernel_ids[string_id_obj.m_guid_id]=std::to_string(string_id_obj.m_string_id);
                    }
                    else
                    {
                        kernel_ids[string_id_obj.m_guid_id] += std::string(", ") + std::to_string(string_id_obj.m_string_id);
                    }
                }
                else
                {
                    auto it = string_ids.find(string_id_obj.m_guid_id);
                    if (it == string_ids.end())
                    {
                        string_ids[string_id_obj.m_guid_id]=std::to_string(string_id_obj.m_string_id);
                    }
                    else
                    {
                        string_ids[string_id_obj.m_guid_id] += std::string(", ") + std::to_string(string_id_obj.m_string_id);
                    }
                }
            }
        }


        if (string_ids.size() > 0)
        {
            for (auto it = string_ids.begin(); it != string_ids.end(); ++it)
            {
                filter[kRocProfVisDmOperationLaunch][it->first] = " SAMPLE.id IS NULL AND R.name_id IN (" + it->second;
                filter[kRocProfVisDmOperationDispatch][it->first] = " (K.region_name_id IN (" + it->second;
                filter[kRocProfVisDmOperationLaunchSample][it->first] = "R.name_id IN (" + it->second;

                auto k_it = kernel_ids.find(it->first);
                if (k_it != kernel_ids.end())
                {
                    filter[kRocProfVisDmOperationDispatch][it->first] += ") OR K.kernel_id IN (" + k_it->second;
                }
                filter[kRocProfVisDmOperationDispatch][it->first] += ") ";
            }
        }
        else
        {
            for (auto it = kernel_ids.begin(); it != kernel_ids.end(); ++it)
            {
                filter[kRocProfVisDmOperationDispatch][it->first] = " K.kernel_id IN (" + it->second;
            }
        }
    }
    return result;
}

rocprofvis_dm_string_t RocprofDatabase::GetEventOperationQuery(const rocprofvis_dm_event_operation_t operation)
{
    switch(operation)
    {
        case kRocProfVisDmOperationLaunch:
        {
            return m_query_factory.GetRocprofRegionTableQuery(false);
        }
        case kRocProfVisDmOperationDispatch:
        {
            return m_query_factory.GetRocprofKernelDispatchTableQuery();
        }
        case kRocProfVisDmOperationMemoryAllocate:
        {
            return m_query_factory.GetRocprofMemoryAllocTableQuery();
        }
        case kRocProfVisDmOperationMemoryCopy:
        {
            return m_query_factory.GetRocprofMemoryCopyTableQuery();
        }
        case kRocProfVisDmOperationLaunchSample:
        {
            return m_query_factory.GetRocprofRegionTableQuery(true);
        }
        default:
        {
            return "";
        }
    }
}

rocprofvis_dm_result_t
RocprofDatabase::BuildTableSummaryClause(bool sample_query,
                                   rocprofvis_dm_string_t& select,
                                   rocprofvis_dm_string_t& group_by)
{
    if(sample_query)
    {
        select += Builder::COUNTER_ID_PUBLIC_NAME;
        select += ", AVG(";
        select += Builder::COUNTER_VALUE_PUBLIC_NAME;
        select += ") AS avg_value, MIN(";
        select += Builder::COUNTER_VALUE_PUBLIC_NAME;
        select += ") AS min_value, MAX(";
        select += Builder::COUNTER_VALUE_PUBLIC_NAME;
        select += ") AS max_value";
        group_by += Builder::COUNTER_ID_PUBLIC_NAME;
    }
    else
    {
        select = "name, COUNT(*) AS num_invocations, AVG(duration) AS avg_duration, MIN(duration) AS min_duration, MAX(duration) AS max_duration, SUM(duration) AS total_duration";
        group_by = "name";
    }
    return kRocProfVisDmResultSuccess;
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
        if(event_id.bitfield.event_op == kRocProfVisDmOperationLaunch ||
           event_id.bitfield.event_op == kRocProfVisDmOperationLaunchSample)
        {
            DbInstance* node_ptr = DbInstancePtrAt(event_id.bitfield.event_node);
            ROCPROFVIS_ASSERT_MSG_BREAK(node_ptr, ERROR_NODE_KEY_CANNOT_BE_NULL);
            query << "SELECT E.call_stack, E.line_info from rocpd_region_%GUID% R INNER JOIN  rocpd_event_%GUID% E ON R.event_id = E.id where R.id == ";
            query << event_id.bitfield.event_id << ";";
            ShowProgress(0, query.str().c_str(), kRPVDbBusy, future);
            if(kRocProfVisDmResultSuccess != ExecuteSQLQuery(future, node_ptr, query.str().c_str(),
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
        DbInstance* node_ptr = DbInstancePtrAt(event_id.bitfield.event_node);
        ROCPROFVIS_ASSERT_MSG_BREAK(node_ptr, ERROR_NODE_KEY_CANNOT_BE_NULL);
        if(event_id.bitfield.event_op == kRocProfVisDmOperationLaunch ||
           event_id.bitfield.event_op == kRocProfVisDmOperationLaunchSample)
        {
            query = "select * from regions where id == ";
            query += std::to_string(event_id.bitfield.event_id);
            query += " and guid = '";
            query += GuidSymAt(node_ptr->GuidIndex());
            query += "'";
            ShowProgress(0, query.c_str(),kRPVDbBusy, future);
            if(kRocProfVisDmResultSuccess !=
               ExecuteSQLQuery(
                   future, node_ptr, query.c_str(), "Properties", extdata,
                   (rocprofvis_dm_event_operation_t) event_id.bitfield.event_op,
                   &CallbackAddExtInfo))
                break;
            query = m_query_factory.GetRocprofEssentialInfoQueryForRegionEvent(event_id.bitfield.event_id, event_id.bitfield.event_op == kRocProfVisDmOperationLaunchSample);
            if(kRocProfVisDmResultSuccess != ExecuteSQLQuery(future, node_ptr, query.c_str(), extdata, &CallbackAddEssentialInfo)) break;

        } else
        if (event_id.bitfield.event_op == kRocProfVisDmOperationDispatch)
        {
            query = "select * from kernels where id == ";
            query += std::to_string(event_id.bitfield.event_id); 
            query += " and guid = '";
            query += GuidSymAt(node_ptr->GuidIndex());
            query += "'";
            ShowProgress(0, query.c_str(),kRPVDbBusy, future);
            if(kRocProfVisDmResultSuccess !=
               ExecuteSQLQuery(
                   future, node_ptr, query.c_str(), "Properties", extdata,
                   (rocprofvis_dm_event_operation_t) event_id.bitfield.event_op,
                   &CallbackAddExtInfo))
                break;
            query = m_query_factory.GetRocprofEssentialInfoQueryForKernelDispatchEvent(event_id.bitfield.event_id);
            if(kRocProfVisDmResultSuccess != ExecuteSQLQuery(future, node_ptr, query.c_str(), extdata, &CallbackAddEssentialInfo)) break;
        } else   
        if (event_id.bitfield.event_op == kRocProfVisDmOperationMemoryAllocate)
        {
            query = "select * from memory_allocations where id == ";
            query += std::to_string(event_id.bitfield.event_id);
            query += " and guid = '";
            query += GuidSymAt(node_ptr->GuidIndex());
            query += "'";
            ShowProgress(0, query.c_str(), kRPVDbBusy, future);
            if(kRocProfVisDmResultSuccess !=
               ExecuteSQLQuery(
                   future, node_ptr, query.c_str(), "Properties", extdata,
                   (rocprofvis_dm_event_operation_t) event_id.bitfield.event_op,
                   &CallbackAddExtInfo))
                break;
            query = m_query_factory.GetRocprofEssentialInfoQueryForMemoryAllocEvent(event_id.bitfield.event_id);
            if(kRocProfVisDmResultSuccess != ExecuteSQLQuery(future, node_ptr, query.c_str(), extdata, &CallbackAddEssentialInfo)) break;
        } else
        if (event_id.bitfield.event_op == kRocProfVisDmOperationMemoryCopy)
        {
            query = "select * from memory_copies where id == ";
            query += std::to_string(event_id.bitfield.event_id);
            query += " and guid = '";
            query += GuidSymAt(node_ptr->GuidIndex());
            query += "'";
            ShowProgress(0, query.c_str(), kRPVDbBusy, future);
            if(kRocProfVisDmResultSuccess !=
               ExecuteSQLQuery(
                   future, node_ptr, query.c_str(), "Properties", extdata,
                   (rocprofvis_dm_event_operation_t) event_id.bitfield.event_op,
                   &CallbackAddExtInfo))
                break;
            query = m_query_factory.GetRocprofEssentialInfoQueryForMemoryCopyEvent(event_id.bitfield.event_id);
            if(kRocProfVisDmResultSuccess != ExecuteSQLQuery(future, node_ptr, query.c_str(), extdata, &CallbackAddEssentialInfo)) break;
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
