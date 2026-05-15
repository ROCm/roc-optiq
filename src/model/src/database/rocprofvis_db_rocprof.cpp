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
        db->m_db_version = db->Sqlite3ColumnText(func, stmt, azColName, 2);
    }
    callback_params->future->CountThisRow();
    return 0;
}

int RocprofDatabase::ProcessTrack(rocprofvis_dm_track_params_t& track_params, rocprofvis_dm_charptr_t*  newqueries)
{
    ROCPROFVIS_ASSERT_MSG_RETURN(track_params.track_indentifiers.db_instance != nullptr, ERROR_NODE_KEY_CANNOT_BE_NULL, 1);
    DbInstance* db_instance = (DbInstance*)track_params.track_indentifiers.db_instance;
    rocprofvis_dm_track_params_it it = TrackTracker()->FindTrackParamsIterator(track_params.track_indentifiers, db_instance->GuidIndex());
    UpdateQueryForTrack(it, track_params, newqueries);
    if(it == TrackPropertiesEnd())
    {
    	TrackTracker()->AddTrack(track_params.track_indentifiers, db_instance->GuidIndex(), track_params.track_indentifiers.track_id);
    	
        if(track_params.track_indentifiers.category == kRocProfVisDmRegionMainTrack ||
            track_params.track_indentifiers.category == kRocProfVisDmRegionSampleTrack)
        {
                
            track_params.track_indentifiers.name[TRACK_ID_PID] = CachedTables(db_instance->GuidIndex())->GetTableCell("Process", track_params.track_indentifiers.id[TRACK_ID_PID], "command");
            track_params.track_indentifiers.name[TRACK_ID_PID] += "(";
            track_params.track_indentifiers.name[TRACK_ID_PID] += std::to_string(track_params.track_indentifiers.id[TRACK_ID_PID]);
            track_params.track_indentifiers.name[TRACK_ID_PID] += ")";

            track_params.track_indentifiers.name[TRACK_ID_TID] = CachedTables(db_instance->GuidIndex())->GetTableCell("Thread", track_params.track_indentifiers.id[TRACK_ID_TID], "name");
            track_params.track_indentifiers.name[TRACK_ID_TID] += "(";
            track_params.track_indentifiers.name[TRACK_ID_TID] += std::to_string(track_params.track_indentifiers.id[TRACK_ID_TID]);
            track_params.track_indentifiers.name[TRACK_ID_TID] += ")";
        }
        else if(track_params.track_indentifiers.category == kRocProfVisDmKernelDispatchTrack ||
            track_params.track_indentifiers.category == kRocProfVisDmMemoryAllocationTrack ||
            track_params.track_indentifiers.category == kRocProfVisDmMemoryCopyTrack ||
            track_params.track_indentifiers.category == kRocProfVisDmPmcTrack)
        {
            track_params.track_indentifiers.name[TRACK_ID_AGENT] = CachedTables(db_instance->GuidIndex())->GetTableCell("Agent", track_params.track_indentifiers.id[TRACK_ID_AGENT], "product_name");
            track_params.track_indentifiers.name[TRACK_ID_AGENT] += "(";
            track_params.track_indentifiers.name[TRACK_ID_AGENT] += CachedTables(db_instance->GuidIndex())->GetTableCell("Agent", track_params.track_indentifiers.id[TRACK_ID_AGENT], "type_index");
            track_params.track_indentifiers.name[TRACK_ID_AGENT] += ")";
            if(track_params.track_indentifiers.category == kRocProfVisDmKernelDispatchTrack ||
                track_params.track_indentifiers.category == kRocProfVisDmMemoryAllocationTrack ||
                track_params.track_indentifiers.category == kRocProfVisDmMemoryCopyTrack)
            {
                track_params.track_indentifiers.name[TRACK_ID_QUEUE] = CachedTables(db_instance->GuidIndex())->GetTableCell("Queue", track_params.track_indentifiers.id[TRACK_ID_QUEUE], "name");
            }
            else {
                track_params.track_indentifiers.name[TRACK_ID_QUEUE] = CachedTables(db_instance->GuidIndex())->GetTableCell("PMC", track_params.track_indentifiers.id[TRACK_ID_QUEUE], "symbol");
            }

        }
        else if(track_params.track_indentifiers.category == kRocProfVisDmStreamTrack)
        {
            track_params.track_indentifiers.name[TRACK_ID_STREAM] = CachedTables(db_instance->GuidIndex())->GetTableCell("Stream", track_params.track_indentifiers.id[TRACK_ID_STREAM], "name");
            track_params.track_indentifiers.name[TRACK_ID_PID] = track_params.track_indentifiers.name[TRACK_ID_STREAM];
        }

        if (kRocProfVisDmResultSuccess != AddTrackProperties(track_params)) return 1;

        if (BindObject()->FuncAddTrack(BindObject()->trace_object, TrackPropertiesLast()) != kRocProfVisDmResultSuccess) return 1; 

        if (BindObject()->FuncAddTopologyNode(BindObject()->trace_object, &track_params.track_indentifiers) != kRocProfVisDmResultSuccess) return 1; 

        if (CachedTables(db_instance->GuidIndex())->PopulateTrackExtendedDataTemplate(this,db_instance->GuidIndex(),  "Node", track_params.track_indentifiers.id[TRACK_ID_NODE]) != kRocProfVisDmResultSuccess) return 1;
        if(track_params.track_indentifiers.category == kRocProfVisDmRegionMainTrack ||
            track_params.track_indentifiers.category == kRocProfVisDmRegionSampleTrack)
        {
            if (CachedTables(db_instance->GuidIndex())->PopulateTrackExtendedDataTemplate(this, db_instance->GuidIndex(),  "Process", track_params.track_indentifiers.id[TRACK_ID_PID]) != kRocProfVisDmResultSuccess) return 1;
            if (CachedTables(db_instance->GuidIndex())->PopulateTrackExtendedDataTemplate(this, db_instance->GuidIndex(), "Thread", track_params.track_indentifiers.id[TRACK_ID_TID]) != kRocProfVisDmResultSuccess) return 1;
        } 
        else if(track_params.track_indentifiers.category == kRocProfVisDmStreamTrack)
        {
            if (CachedTables(db_instance->GuidIndex())->PopulateTrackExtendedDataTemplate(this, db_instance->GuidIndex(),  "Process", track_params.track_indentifiers.id[TRACK_ID_PID]) != kRocProfVisDmResultSuccess) return 1;
            if (CachedTables(db_instance->GuidIndex())->PopulateTrackExtendedDataTemplate(this, db_instance->GuidIndex(), "Stream", track_params.track_indentifiers.id[TRACK_ID_STREAM]) != kRocProfVisDmResultSuccess) return 1;         
        }
        else if(track_params.track_indentifiers.category == kRocProfVisDmKernelDispatchTrack ||
            track_params.track_indentifiers.category == kRocProfVisDmMemoryAllocationTrack ||
            track_params.track_indentifiers.category == kRocProfVisDmMemoryCopyTrack ||
            track_params.track_indentifiers.category == kRocProfVisDmPmcTrack)
        {
            if (CachedTables(db_instance->GuidIndex())->PopulateTrackExtendedDataTemplate(this, db_instance->GuidIndex(), "Agent", track_params.track_indentifiers.id[TRACK_ID_AGENT]) != kRocProfVisDmResultSuccess) return 1; 
            if(track_params.track_indentifiers.category == kRocProfVisDmKernelDispatchTrack ||
                track_params.track_indentifiers.category == kRocProfVisDmMemoryAllocationTrack ||
                track_params.track_indentifiers.category == kRocProfVisDmMemoryCopyTrack)
            {
                if (CachedTables(db_instance->GuidIndex())->PopulateTrackExtendedDataTemplate(this, db_instance->GuidIndex(), "Queue", track_params.track_indentifiers.id[TRACK_ID_QUEUE]) != kRocProfVisDmResultSuccess) return 1;
            }
            else
            {
                if(track_params.track_indentifiers.id[TRACK_ID_QUEUE] > 0)
                    if (CachedTables(db_instance->GuidIndex())->PopulateTrackExtendedDataTemplate(this, db_instance->GuidIndex(), "PMC", track_params.track_indentifiers.id[TRACK_ID_COUNTER]) != kRocProfVisDmResultSuccess) return 1;
            }
        }
    }
    else
    {
        it->get()->load_id.insert(*track_params.load_id.begin());
        track_params.track_indentifiers.track_id = it->get()->track_indentifiers.track_id;
    }
    return 0;
}


int RocprofDatabase::CallbackCaptureMemoryActivity(void* data, int argc, sqlite3_stmt* stmt, char** azColName) {
    ROCPROFVIS_ASSERT_MSG_RETURN(argc==rocprofvis_db_sqlite_memory_alloc_activity_query_format::NUM_PARAMS, ERROR_DATABASE_QUERY_PARAMETERS_MISMATCH, 1);
    ROCPROFVIS_ASSERT_MSG_RETURN(data, ERROR_SQL_QUERY_PARAMETERS_CANNOT_BE_NULL, 1);
    void*  func = (void*)&CallbackCaptureMemoryActivity;
    rocprofvis_db_sqlite_callback_parameters* callback_params = (rocprofvis_db_sqlite_callback_parameters*)data;
    ROCPROFVIS_ASSERT_MSG_RETURN(callback_params->db_instance != nullptr, ERROR_NODE_KEY_CANNOT_BE_NULL, 1);
    RocprofDatabase* db = (RocprofDatabase*)callback_params->db;
    if(callback_params->future->Interrupted()) return SQLITE_ABORT;
    rocprofvis_db_memalloc_activity_t memact = {};
    memact.start = db->Sqlite3ColumnInt64(func, stmt, azColName, 7);
    memact.end = db->Sqlite3ColumnInt64(func, stmt, azColName, 8);
    memact.address = db->Sqlite3ColumnInt64(func, stmt, azColName, 9);
    memact.size= db->Sqlite3ColumnInt64(func, stmt, azColName, 10);
    memact.id = db->Sqlite3ColumnInt(func, stmt, azColName, 0);
    memact.pid = db->Sqlite3ColumnInt(func, stmt, azColName, 1);
    memact.stream_id = db->Sqlite3ColumnInt(func, stmt, azColName, 4);
    memact.agent_id = db->Sqlite3ColumnInt(func, stmt, azColName, 2);
    memact.queue_id = db->Sqlite3ColumnInt(func, stmt, azColName, 3);
    memact.track_id = db->Sqlite3ColumnInt(func, stmt, azColName, 11);
    std::string type_str = db->Sqlite3ColumnText(func, stmt, azColName, 5);
    std::string level_str = db->Sqlite3ColumnText(func, stmt, azColName, 6);
    bool type_found = false;
    bool level_found = false;
    for (int i=kRPVMemActivityAlloc; i < kRPVMemActivityNumTypes; i++)
    {
        if (type_str == Builder::mem_alloc_types[i])
        {
            memact.type = (rocprofvis_db_memalloc_type_t)i;
            type_found = true;
            break;
        }
    }
    for (int i=kRPVMemLevelReal; i < kRPVMemLevelNumLevels; i++)
    {
        if (level_str == Builder::mem_alloc_levels[i])
        {
            memact.level = (rocprofvis_db_memalloc_level_t)i;
            level_found = true;
            break;
        }
    }
    if (!type_found || !level_found)
    {
        spdlog::error("Memory activity type or level not found: type={}, level={}", type_str, level_str);
        return 0; // Skip this record
    }
    auto& vec = db->m_memalloc_activity[callback_params->db_instance->GuidIndex()];
    if (memact.type == kRPVMemActivityFree)
    {
        auto it = std::find_if(vec.rbegin(), vec.rend(), [memact](rocprofvis_db_memalloc_activity_t& _memact) {return memact.address == _memact.address; });
        if (it != vec.rend())
        {
            memact.agent_id = it->agent_id;
            memact.track_id = it->track_id;
            memact.size = it->size;
            db->m_memfree_stream_to_agent[callback_params->db_instance->GuidIndex()][memact.stream_id] = it->agent_id;
        }
        else
        {
            memact.agent_id = callback_params->future->GetRuntimeStorageValue(kRPVFutureStorageEventId, (uint64_t)memact.agent_id);
            memact.track_id = callback_params->future->GetRuntimeStorageValue(kRPVFutureStorageTrackId, (uint64_t)memact.track_id);
            db->m_memfree_stream_to_agent[callback_params->db_instance->GuidIndex()][memact.stream_id] = memact.agent_id;
        }
    }
    else
    {
        callback_params->future->SetRuntimeStorageValue(kRPVFutureStorageEventId, (uint64_t)memact.agent_id);
        callback_params->future->SetRuntimeStorageValue(kRPVFutureStorageTrackId, (uint64_t)memact.track_id);
    }
    vec.push_back(memact);
    callback_params->future->CountThisRow();
    return 0;
}

/**
 * @brief Create and populate the in-memory/SQLite table that stores memory activity records.
 *
 * This function defines the schema for the memory activity table (allocation/free events,
 * agent/queue/stream identifiers, sizes, and time ranges) and configures how rows are
 * stored while iterating over the underlying SQLite result set. It also reports progress
 * for the asynchronous operation through the provided Future instance.
 *
 * @param future Pointer to a Future used to track and signal progress/completion while
 *               building the memory activity table. Must remain valid for the duration
 *               of this call.
 *
 * @return kRocProfVisDmResultSuccess on success, or an appropriate error code if table
 *         creation or population fails.
 */
rocprofvis_dm_result_t RocprofDatabase::CreateMemoryActivityTable(Future* future)
{

    // Local structure mirroring the memory activity table schema; used while extracting
    // values from SQLite statements and inserting them into the database model.

    typedef struct store_params {
            uint32_t id;
            uint64_t nid;
            uint32_t pid;
            uint32_t agent_id;
            uint32_t queue_id;
            uint32_t stream_id;
            uint64_t pmc_id;
            std::string type;
            std::string level;
            uint64_t start;
            uint64_t end;
            uint64_t address;
            uint64_t size;
            uint32_t track_id;
    } store_params;

    rocprofvis_dm_result_t result = kRocProfVisDmResultSuccess;

    for (auto& guid_info : DbInstances())
    {
        const char* pmc_table_name = "PMC";
        std::map<uint32_t, uint64_t> allocated_memory_per_agent;
        std::map<uint32_t, uint64_t> pmc_id_per_agent;
        TableCache* pmc_table = (TableCache*)CachedTables(guid_info.first.GuidIndex())->GetTableHandle(pmc_table_name);
        auto& mem_act_per_guid = m_memalloc_activity[guid_info.first.GuidIndex()];
        std::vector<store_params> v;
        v.reserve(mem_act_per_guid.size());
        uint64_t node_id = std::atoll(CachedTables(guid_info.first.GuidIndex())->GetTableCellByIndex("Node", 0, "id"));
        std::string table_name = m_metadata_version_control.GetTableName(m_metadata_version_control.kRocOptiqTableMemoryActivity) + GuidAt(guid_info.first.GuidIndex());
        bool table_exists = false == m_metadata_version_control.MustRebuild(guid_info.first.FileIndex(), m_metadata_version_control.kRocOptiqTableMemoryActivity);
        if (mem_act_per_guid.size() > 0)
        {
            for (auto& m : mem_act_per_guid)
            {
                auto it = pmc_id_per_agent.find(m.agent_id);
                if (it == pmc_id_per_agent.end())
                {
                    uint32_t num = static_cast<uint32_t>(pmc_table->NumRows());
                    uint64_t pmc_id = num ? std::atoll(pmc_table->GetCellByIndex(num - 1, "id")) : 0;
                    pmc_id++;
                    pmc_table->AddRow(pmc_id);

                    CachedTables(guid_info.first.GuidIndex())->AddTableCell(pmc_table_name, pmc_id, "id",
                        kRPVDataTypeInt, std::to_string(pmc_id).c_str());
                    CachedTables(guid_info.first.GuidIndex())->AddTableCell(pmc_table_name, pmc_id, "guid",
                        kRPVDataTypeString, GuidSymAt(guid_info.first.GuidIndex()).c_str());
                    CachedTables(guid_info.first.GuidIndex())->AddTableCell(pmc_table_name, pmc_id, "nid",
                        kRPVDataTypeInt, std::to_string(node_id).c_str());
                    CachedTables(guid_info.first.GuidIndex())->AddTableCell(pmc_table_name, pmc_id, "pid",
                        kRPVDataTypeInt, std::to_string(m.pid).c_str());
                    CachedTables(guid_info.first.GuidIndex())->AddTableCell(pmc_table_name, pmc_id, "agent_id",
                        kRPVDataTypeInt, std::to_string(m.agent_id).c_str());
                    CachedTables(guid_info.first.GuidIndex())->AddTableCell(pmc_table_name, pmc_id, "target_arch",
                        kRPVDataTypeString, "GPU");
                    CachedTables(guid_info.first.GuidIndex())->AddTableCell(pmc_table_name, pmc_id, "event_code",
                        kRPVDataTypeInt, "0");
                    CachedTables(guid_info.first.GuidIndex())->AddTableCell(pmc_table_name, pmc_id, "instance_id",
                        kRPVDataTypeInt, "0");
                    std::string pmc_name = Builder::mem_alloc_levels[m.level] + " MEMORY";
                    CachedTables(guid_info.first.GuidIndex())->AddTableCell(pmc_table_name, pmc_id, "name",
                        kRPVDataTypeString, pmc_name.c_str());
                    CachedTables(guid_info.first.GuidIndex())->AddTableCell(pmc_table_name, pmc_id, "symbol",
                        kRPVDataTypeString, pmc_name.c_str());
                    CachedTables(guid_info.first.GuidIndex())->AddTableCell(pmc_table_name, pmc_id, "description",
                        kRPVDataTypeString, pmc_name.c_str());
                    CachedTables(guid_info.first.GuidIndex())->AddTableCell(pmc_table_name, pmc_id, "long_description",
                        kRPVDataTypeString, pmc_name.c_str());
                    CachedTables(guid_info.first.GuidIndex())->AddTableCell(pmc_table_name, pmc_id, "component",
                        kRPVDataTypeString, "");
                    CachedTables(guid_info.first.GuidIndex())->AddTableCell(pmc_table_name, pmc_id, "units",
                        kRPVDataTypeString, "bytes");
                    CachedTables(guid_info.first.GuidIndex())->AddTableCell(pmc_table_name, pmc_id, "value_type",
                        kRPVDataTypeString, "");
                    CachedTables(guid_info.first.GuidIndex())->AddTableCell(pmc_table_name, pmc_id, "block",
                        kRPVDataTypeString, "");
                    CachedTables(guid_info.first.GuidIndex())->AddTableCell(pmc_table_name, pmc_id, "expression",
                        kRPVDataTypeString, "");
                    CachedTables(guid_info.first.GuidIndex())->AddTableCell(pmc_table_name, pmc_id, "is_constant",
                        kRPVDataTypeInt, "0");
                    CachedTables(guid_info.first.GuidIndex())->AddTableCell(pmc_table_name, pmc_id, "is_derived",
                        kRPVDataTypeInt, "0");
                    CachedTables(guid_info.first.GuidIndex())->AddTableCell(pmc_table_name, pmc_id, "extdata",
                        kRPVDataTypeBlob, "{}");

                    pmc_id_per_agent[m.agent_id] = pmc_id;
                }
                if (!table_exists)
                {
                    if (m.type == kRPVMemActivityAlloc)
                    {
                        allocated_memory_per_agent[m.agent_id] += m.size;
                    }
                    else
                        if (m.type == kRPVMemActivityFree)
                        {
                            allocated_memory_per_agent[m.agent_id] -= m.size;
                        }
                    store_params r;
                    r.id = m.id;
                    r.nid = node_id;
                    r.pid = m.pid;
                    r.agent_id = m.agent_id;
                    r.queue_id = m.queue_id;
                    r.stream_id = m.stream_id;
                    r.pmc_id = pmc_id_per_agent[m.agent_id];
                    r.type = Builder::mem_alloc_types[m.type];
                    r.level = Builder::mem_alloc_levels[m.level];
                    r.start = m.start;
                    r.end = m.end;
                    r.address = m.address;
                    r.size = allocated_memory_per_agent[m.agent_id];
                    v.push_back(r);
                }
            }

            if (!table_exists)
            {
                result = CreateSQLTable(
                    table_name.c_str(),
                    s_mem_activity_schema_params,
                    v.size(),
                    [&](sqlite3_stmt* stmt, int index) {
                        store_params& p = v[index];
                        uint32_t column_index = 1;
                        sqlite3_bind_int(stmt, column_index++, p.id);
                        sqlite3_bind_int64(stmt, column_index++, p.nid);
                        sqlite3_bind_int(stmt, column_index++, p.pid);
                        sqlite3_bind_int(stmt, column_index++, p.agent_id);
                        sqlite3_bind_int(stmt, column_index++, p.queue_id);
                        sqlite3_bind_int(stmt, column_index++, p.stream_id);
                        sqlite3_bind_int64(stmt, column_index++, p.pmc_id);
                        sqlite3_bind_text(stmt, column_index++, p.type.c_str(), -1, SQLITE_STATIC);
                        sqlite3_bind_text(stmt, column_index++, p.level.c_str(), -1, SQLITE_STATIC);
                        sqlite3_bind_int64(stmt, column_index++, p.start);
                        sqlite3_bind_int64(stmt, column_index++, p.end);
                        sqlite3_bind_int64(stmt, column_index++, p.address);
                        sqlite3_bind_int64(stmt, column_index++, p.size);
                    }, guid_info.first.FileIndex());
                if (result != kRocProfVisDmResultSuccess)
                {
                    break;
                }
            }
        } else
        if (!table_exists)
        {
            result = CreateSQLTable(
                table_name.c_str(),
                s_mem_activity_schema_params,
                0,
                nullptr, 
                guid_info.first.FileIndex());
        }
    }
    return result;
}

uint64_t RocprofDatabase::GetMemoryActivityTableSchemaHash()
{
    std::string hash_str = m_query_factory.GetRocprofMemoryAllocActivityQuery() + m_query_factory.GetRocprofMemoryAllocActivityLoadQuery();
    for (auto param : s_mem_activity_schema_params)
    {
        hash_str += param.column;
        hash_str += param.type;
    }
    return std::hash<std::string>{}(hash_str);
}

rocprofvis_dm_result_t RocprofDatabase::CreateAgentFriendlyMemoryAllocationTable(Future* future)
{
    rocprofvis_dm_result_t result = kRocProfVisDmResultSuccess;
    for (auto& guid_info : DbInstances())
    {
        std::string from = "rocpd_memory_allocate_";
        std::string to = m_metadata_version_control.GetTableName(m_metadata_version_control.kRocOptiqTableMemoryAllocate);
        std::string original_m_alloc_table = from + GuidAt(guid_info.first.GuidIndex());
        std::string updated_m_alloc_table = to + GuidAt(guid_info.first.GuidIndex());
        if (false == m_metadata_version_control.MustRebuild(guid_info.first.FileIndex(), m_metadata_version_control.kRocOptiqTableMemoryAllocate)) continue;
        std::string query = std::string("SELECT sql FROM sqlite_master WHERE type='table' AND name = '") + original_m_alloc_table + "'";
        std::string sql;
        result = ExecuteSQLQuery(future, DbInstancePtrAt(guid_info.first.GuidIndex()), query.c_str(), &CallbackGetValue, &sql);
        if (kRocProfVisDmResultSuccess != result) break;
        size_t pos;

        if ((pos = sql.find(from)) != std::string::npos) {
            sql.replace(pos, from.length(), to);
            result = ExecuteSQLQuery(future, DbInstancePtrAt(guid_info.first.GuidIndex()), (std::string("DROP TABLE IF EXISTS ") + updated_m_alloc_table).c_str());
            if (kRocProfVisDmResultSuccess != result) break;
            result = ExecuteSQLQuery(future, DbInstancePtrAt(guid_info.first.GuidIndex()), sql.c_str());
            if (kRocProfVisDmResultSuccess != result) break;
            query = std::string("INSERT INTO ")+updated_m_alloc_table+ " SELECT * FROM " + original_m_alloc_table + ";";
            result = ExecuteSQLQuery(future, DbInstancePtrAt(guid_info.first.GuidIndex()), query.c_str());
            if (kRocProfVisDmResultSuccess != result) break;
            sqlite3_stmt* stmt = nullptr;
            query = std::string("UPDATE ") + updated_m_alloc_table + (m_query_factory.IsVersionGreaterOrEqual("4") ? " SET track_id = ? WHERE id = ? ; " : " SET agent_id = ? WHERE id = ? ;");
            sqlite3* conn = GetServiceConnection(guid_info.first.FileIndex());
            if (sqlite3_prepare_v2(conn, query.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
            {
                spdlog::error(sqlite3_errmsg(conn));
                result = kRocProfVisDmResultDbAccessFailed;
                break;
            }
            sqlite3_exec(conn, "BEGIN;", nullptr, nullptr, nullptr);
            auto& mem_act_per_guid = m_memalloc_activity[guid_info.first.GuidIndex()];

            for (auto& m : mem_act_per_guid)
            {
                if (m_query_factory.IsVersionGreaterOrEqual("4"))
                {
                    sqlite3_bind_int(stmt, 1, m.track_id);
                }
                else
                {
                    sqlite3_bind_int(stmt, 1, m.agent_id);
                }
                sqlite3_bind_int(stmt, 2, m.id);

                if (sqlite3_step(stmt) != SQLITE_DONE)
                {
                    spdlog::error(sqlite3_errmsg(conn));
                }

                sqlite3_reset(stmt); 
                sqlite3_clear_bindings(stmt);
            }

            sqlite3_exec(conn, "COMMIT;", nullptr, nullptr, nullptr);

            sqlite3_finalize(stmt);
        }
    }
    return result;
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
        guid_list->push_back({ DbInstance(file_node, static_cast<uint32_t>(guid_list->size())), table_name.substr(table_name_befor_guid.length()) });
    }
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
        result = ExecuteTransaction( vec, db_node_id);
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
                     " ON rocpd_track_" + guid_info.second + "(nid,pid,stream_id);");
        } else
        {
            vec.push_back(std::string("CREATE INDEX IF NOT EXISTS rocpd_region_idx_") + guid_info.second +
                     " ON rocpd_region_" + guid_info.second + "(nid,pid,tid);");
            vec.push_back(std::string("CREATE INDEX IF NOT EXISTS rocpd_kernel_dispatch_idx_") + guid_info.second +
                     " ON rocpd_kernel_dispatch_" + guid_info.second + "(nid,agent_id,queue_id);");
            vec.push_back(std::string("CREATE INDEX IF NOT EXISTS rocpd_kernel_dispatch_stream_idx_") + guid_info.second +
                     " ON rocpd_kernel_dispatch_" + guid_info.second + "(nid,pid,stream_id);");
            vec.push_back(std::string("CREATE INDEX IF NOT EXISTS rocpd_memory_allocate_idx_") + guid_info.second +
                     " ON rocpd_memory_allocate_" + guid_info.second + "(nid,agent_id,queue_id);");
            vec.push_back(std::string("CREATE INDEX IF NOT EXISTS rocpd_memory_allocate_stream_idx_") + guid_info.second +
                     " ON rocpd_memory_allocate_" + guid_info.second + "(nid,pid,stream_id);");
            vec.push_back(std::string("CREATE INDEX IF NOT EXISTS rocpd_memory_copy_idx_") + guid_info.second +
                     " ON rocpd_memory_copy_" + guid_info.second + "(nid,dst_agent_id,queue_id);");
            vec.push_back(std::string("CREATE INDEX IF NOT EXISTS rocpd_memory_copy_stream_idx_") + guid_info.second +
                     " ON rocpd_memory_copy_" + guid_info.second + "(nid,pid,stream_id);");

        }
        vec.push_back(std::string("CREATE INDEX IF NOT EXISTS rocpd_region_event_idx_") + guid_info.second +
                    " ON rocpd_region_" + guid_info.second + "(event_id);");
        vec.push_back(std::string("CREATE INDEX IF NOT EXISTS rocpd_kernel_dispatch_idx_") + guid_info.second +
            " ON rocpd_kernel_dispatch_" + guid_info.second + "(event_id);");
        vec.push_back(std::string("CREATE INDEX IF NOT EXISTS rocpd_memory_allocate_idx_") + guid_info.second +
            " ON rocpd_memory_allocate_" + guid_info.second + "(event_id);");
        vec.push_back(std::string("CREATE INDEX IF NOT EXISTS rocpd_memory_copy_idx_") + guid_info.second +
            " ON rocpd_memory_copy_" + guid_info.second + "(event_id);");
        vec.push_back(std::string("CREATE INDEX IF NOT EXISTS rocpd_arg_idx_") + guid_info.second +
            " ON rocpd_arg_" + guid_info.second + "(event_id);");
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


rocprofvis_dm_result_t RocprofDatabase::RunCacheQueriesAsync(Future* future, std::vector<std::pair<std::string, std::string>>& info_table_list){
    std::vector<std::thread> threads;
    rocprofvis_dm_result_t result = kRocProfVisDmResultNotLoaded;

    auto get_info_table_task = [&](DbInstance* db_instance, std::string query, std::string tag) {
        Future* sub_future = future->AddSubFuture();
        result = ExecuteSQLQuery(sub_future, db_instance, query.c_str(), tag.c_str(), (rocprofvis_dm_handle_t)CachedTables(db_instance->GuidIndex()), &CallbackCacheTable);
        future->DeleteSubFuture(sub_future);
        };

    for (auto& guid_info : DbInstances())
    {
        for (auto table : info_table_list)
        {
            threads.emplace_back(
                get_info_table_task, 
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

rocprofvis_dm_result_t RocprofDatabase::GenerateInterdependencyTables(Future* future) {


    std::vector<std::pair<std::string, std::string>> info_table_list = {{ "StreamToHw", 
        std::string("SELECT ROW_NUMBER() OVER (ORDER BY ") +
        Builder::STREAM_ID_SERVICE_NAME +
        ") AS row_num, * FROM (" +
        m_query_factory.GetRocprofMemoryCopyStreamFlowQuery() +
        Builder::Union() +
        m_query_factory.GetRocprofMemoryAllocStreamFlowQuery() +
        Builder::Union() +
        m_query_factory.GetRocprofKernelDispatchStreamFlowQuery() +
        ") ORDER BY " +
        Builder::STREAM_ID_SERVICE_NAME
        }};

    return RunCacheQueriesAsync(future, info_table_list);
}

rocprofvis_dm_result_t RocprofDatabase::LoadInformationTables(Future* future) {

    std::vector<std::thread> threads;

    std::vector<std::pair<std::string, std::string>> info_table_list = {
        {"Node", "SELECT * from rocpd_info_node_%GUID%;"},
        {"Agent", "SELECT id,guid,nid,pid,coalesce(type,'NIC') as type,absolute_index,logical_index,type_index,uuid,name,model_name,vendor_name,product_name,extdata from rocpd_info_agent_%GUID%;"},
        {"Queue", "SELECT * from rocpd_info_queue_%GUID%;"},
        {"Stream", "SELECT * from rocpd_info_stream_%GUID%;"},
        {"Process", "SELECT * from rocpd_info_process_%GUID%;"},
        {"Thread", "SELECT * from rocpd_info_thread_%GUID%;"},
        {"PMC", "SELECT id, guid, nid, pid, agent_id, target_arch, COALESCE(event_code,0) as event_code, COALESCE(instance_id,0) as instance_id, name, symbol, description, long_description, component, units, value_type, block, expression, is_constant, is_derived, extdata from rocpd_info_pmc_%GUID%;"},

   };

    return RunCacheQueriesAsync(future, info_table_list);
}


rocprofvis_dm_result_t RocprofDatabase::LoadMemoryActivityData(Future* future) {
    rocprofvis_dm_result_t result = kRocProfVisDmResultNotLoaded;
    std::vector<std::thread> threads;
    auto get_memory_allocation_activity_task = [&](DbInstance * db_instance, std::string query) {
        Future* sub_future = future->AddSubFuture();
        result = ExecuteSQLQuery(sub_future, db_instance, query.c_str(), &CallbackCaptureMemoryActivity);
        future->DeleteSubFuture(sub_future);
        };

    for (auto& guid_info : DbInstances())
    {
        std::string table_name = m_metadata_version_control.GetTableName(m_metadata_version_control.kRocOptiqTableMemoryActivity) + GuidAt(guid_info.first.GuidIndex());
        if (false == m_metadata_version_control.MustRebuild(guid_info.first.FileIndex(), m_metadata_version_control.kRocOptiqTableMemoryActivity))
        {
            threads.emplace_back(
                get_memory_allocation_activity_task,
                &guid_info.first,
                m_query_factory.GetRocprofMemoryAllocActivityLoadQuery());
        }
        else
        {
            threads.emplace_back(
                get_memory_allocation_activity_task,
                &guid_info.first,
                m_query_factory.GetRocprofMemoryAllocActivityQuery());
        }
    }
    for (auto& t : threads)
        t.join();

    return result;
}


rocprofvis_dm_result_t RocprofDatabase::PopulateStreamToHardwareFlowProperties(uint32_t stream_track_index, uint32_t db_instance ){
    TableCache* table = (TableCache*)CachedTables(db_instance)->GetTableHandle("StreamToHw");
    uint32_t stream_id = TrackPropertiesAt(stream_track_index)->track_indentifiers.id[TRACK_ID_STREAM];

    for (int ind = 0; ind < table->NumRows(); ind++)
    {
        uint64_t id = std::atoll(table->GetCellByIndex(ind, Builder::STREAM_ID_SERVICE_NAME));
        if (id == stream_id)
        {
            uint32_t op = std::atoll(table->GetCellByIndex(ind, Builder::OPERATION_SERVICE_NAME));
            std::string agent = table->GetCellByIndex(ind, Builder::AGENT_ID_SERVICE_NAME);
            uint32_t agent_id = 0;
            if (agent.empty() && op == kRocProfVisDmOperationMemoryAllocate)
            {
                auto it = m_memfree_stream_to_agent[db_instance].find(stream_id);
                if (it != m_memfree_stream_to_agent[db_instance].end())
                {
                    agent_id = it->second;
                }
            }
            else
            {
                agent_id = std::atoll(table->GetCellByIndex(ind, Builder::AGENT_ID_SERVICE_NAME));
            }
            uint32_t queue_id = std::atoll(table->GetCellByIndex(ind, Builder::QUEUE_ID_SERVICE_NAME));
            uint32_t track;
            if (TrackTracker()->FindTrack(
                TrackTracker()->SearchCategoryMaskLookup((rocprofvis_dm_event_operation_t)op), 
                agent_id, 
                queue_id, 
                db_instance, track))
            {
                rocprofvis_dm_result_t result = BindObject()->FuncAddTopologyNodeProperty(
                    BindObject()->trace_object,
                    &TrackPropertiesAt(stream_track_index)->track_indentifiers,
                    kRPVTopologyDataTypeRef,
                    "QueueRef",
                    "TrackRef",
                    (void*)&TrackPropertiesAt(track)->track_indentifiers);
                if (result != kRocProfVisDmResultSuccess) return result;
            }
        }
    }
    return kRocProfVisDmResultSuccess;
}

rocprofvis_dm_result_t RocprofDatabase::PopulateUnusedAgents(uint32_t db_instance)
{
    rocprofvis_dm_result_t result = kRocProfVisDmResultSuccess;
    const char* table_name = "Agent";
    TableCache* table = (TableCache*)CachedTables(db_instance)->GetTableHandle(table_name);
    for (uint64_t i = 0; i < table->NumRows(); i++)
    {
        uint64_t agent_id = std::atoll(table->GetCellByIndex(i, "id"));
        uint64_t node_id = std::atoll(table->GetCellByIndex(i,"nid"));
        bool agent_in_use = false;
        for (int track_id = 0; track_id < NumTracks(); track_id++)
        {
            DbInstance* instance = (DbInstance*)TrackPropertiesAt(track_id)->track_indentifiers.db_instance;

            if (instance->GuidIndex() == db_instance)
            {
                rocprofvis_dm_track_identifiers_t& track_indentifiers = TrackPropertiesAt(track_id)->track_indentifiers;
                if ((track_indentifiers.category == kRocProfVisDmPmcTrack || 
                    track_indentifiers.category == kRocProfVisDmKernelDispatchTrack ||
                    track_indentifiers.category == kRocProfVisDmMemoryAllocationTrack ||
                    track_indentifiers.category == kRocProfVisDmMemoryCopyTrack) &&
                    track_indentifiers.id[TRACK_ID_NODE] == node_id && 
                    track_indentifiers.id[TRACK_ID_AGENT] == agent_id)
                {
                    agent_in_use = true;
                    break;
                }
            }            
        }
        if (agent_in_use == false)
        {
            rocprofvis_dm_track_identifiers_t track_indentifiers;

            uint64_t pid = std::atoll(table->GetCell(i,"pid"));
            track_indentifiers.category = kRocProfVisDmNotATrack;
            track_indentifiers.id[TRACK_ID_NODE] = node_id;
            track_indentifiers.id[TRACK_ID_AGENT] = agent_id;
            track_indentifiers.process_id = pid;
            track_indentifiers.is_numeric[TRACK_ID_NODE] = true;
            track_indentifiers.is_numeric[TRACK_ID_QUEUE] = true;
            track_indentifiers.tag[TRACK_ID_NODE] = "nodeId";
            track_indentifiers.tag[TRACK_ID_AGENT] = "agentId";
            track_indentifiers.tag[TRACK_ID_QUEUE] = "noData";
            track_indentifiers.db_instance = DbInstancePtrAt(db_instance);
            result = BindObject()->FuncAddTopologyNode(BindObject()->trace_object, &track_indentifiers);
            if (kRocProfVisDmResultSuccess == result || kRocProfVisDmResultInvalidParameter == result)
            {
                if (kRocProfVisDmResultSuccess == CachedTables(db_instance)->PopulateTrackTopologyData(this, &track_indentifiers, db_instance, table_name, agent_id))
                {
                    if (CachedTables(db_instance)->PopulateTrackTopologyData(this, &track_indentifiers, db_instance, "Node", node_id) == kRocProfVisDmResultSuccess)
                    {
                        result = CachedTables(db_instance)->PopulateTrackTopologyData(this, &track_indentifiers, db_instance, "Agent", agent_id);
                    }
                }
            }
        }
    }
    return result;
}

std::string RocprofDatabase::GetLevelSchemaHashStr()
{
    std::string hash_str;
    for (auto param : s_level_schema_params)
    {
        hash_str += param.column;
        hash_str += param.type;
    }
    return hash_str;
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
            TemporaryDbInstance db_instance(file_node->node_id);
            ExecuteSQLQuery(future,
                &db_instance,
                (std::string("SELECT ") + std::to_string(file_node->node_id)+" as file_node, name from sqlite_master where type = 'table' and name like 'rocpd_info_node_%'; ").c_str(),
                "rocpd_info_node_", (rocprofvis_dm_handle_t)&DbInstances(),
                &CallbackNodeEnumeration);
        }

        TraceProperties()->num_db_instances = static_cast<uint32_t>(DbInstances().size());

        ShowProgress(1, "Get version", kRPVDbBusy, future);
        std::string version;
        for (auto& guid_info : DbInstances())
        {
            if ((result = kRocProfVisDmResultSuccess) != ExecuteSQLQuery(future, &guid_info.first, "SELECT * FROM rocpd_metadata_%GUID%;", &CallbackParseMetadata)) break;
            if (!version.empty())
            {
                if (version != m_db_version)
                {
                    spdlog::debug("Cannot support different version database nodes!");
                    result = kRocProfVisDmResultNotSupported;
                    break;
                }
            }
        }
        if (result != kRocProfVisDmResultSuccess)
            break;
        m_query_factory.SetVersion(m_db_version.c_str());

        ShowProgress(5, "Indexing tables", kRPVDbBusy, future);
        CreateIndexes();

        //pre-create cache tables
        for (auto& guid_info : DbInstances())
        {
            CachedTables(guid_info.first.GuidIndex());
        }


        ShowProgress(1, "Load table version information", kRPVDbBusy, future);
        m_metadata_version_control.VerifyRocOptiqTablesVersions(future);

        ShowProgress(5, "Load Information Tables", kRPVDbBusy, future);
        LoadInformationTables(future);

        ShowProgress(5, "Collect memory activity data", kRPVDbBusy, future);

        LoadMemoryActivityData(future);
        CreateMemoryActivityTable(future);
        CreateAgentFriendlyMemoryAllocationTable(future);

        ShowProgress(1, "Collect topology data", kRPVDbBusy, future);
        GenerateInterdependencyTables(future);

        TraceProperties()->events_count[kRocProfVisDmOperationLaunch]   = 0;
        TraceProperties()->events_count[kRocProfVisDmOperationDispatch] = 0;
        TraceProperties()->events_count[kRocProfVisDmOperationMemoryAllocate] = 0;
        TraceProperties()->events_count[kRocProfVisDmOperationMemoryCopy]     = 0;
        TraceProperties()->events_count[kRocProfVisDmOperationLaunchSample]   = 0;

        TraceProperties()->tracks_info_restored = true;
        TraceProperties()->trace_duration = 0;

        for (size_t i = 0; i < DbInstances().size(); ++i)
        {
            TraceProperties()->db_inst_start_time.push_back(UINT64_MAX);
            TraceProperties()->db_inst_end_time.push_back(0);
        }
        
        std::string track_queries = m_query_factory.GetRocprofRegionTrackQuery(false) +
            m_query_factory.GetRocprofRegionTrackQuery(true) +
            m_query_factory.GetRocprofKernelDispatchTrackQuery() +
            m_query_factory.GetRocprofKernelDispatchTrackQueryForStream() +
            m_query_factory.GetRocprofMemoryAllocTrackQuery() +
            m_query_factory.GetRocprofMemoryAllocTrackQueryForStream() +
            m_query_factory.GetRocprofMemoryCopyTrackQuery() +
            m_query_factory.GetRocprofMemoryCopyTrackQueryForStream() +
            m_query_factory.GetRocprofPerformanceCountersTrackQuery() +
            m_query_factory.GetRocprofSMIPerformanceCountersTrackQuery() +
            m_query_factory.GetRocprofMemoryActivityTrackQuery();
        uint32_t load_id = 0;

        ShowProgress(5, "Adding HIP API tracks", kRPVDbBusy, future );
        {
            std::vector<std::thread> threads;
            m_add_track_mutex.init(NumDbInstances());
            auto task = [&](DbInstance* db_instance)
                {
                    Future* sub_future = future->AddSubFuture();
                    result = ExecuteSQLQuery(sub_future, db_instance, load_id,
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
                    m_add_track_mutex.unlock(db_instance->GuidIndex());
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
            m_add_track_mutex.init(NumDbInstances());
            auto task = [&](DbInstance* db_instance)
                {

                    Future* sub_future = future->AddSubFuture();
                    result = ExecuteSQLQuery(sub_future, db_instance, load_id,
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
                    m_add_track_mutex.unlock(db_instance->GuidIndex());
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
            m_add_track_mutex.init(NumDbInstances());
            auto task = [&](DbInstance* db_instance)
                {
                    Future* sub_future = future->AddSubFuture();
                    result = ExecuteSQLQuery(sub_future, db_instance, load_id,
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
                    m_add_track_mutex.unlock(db_instance->GuidIndex());
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
            m_add_track_mutex.init(NumDbInstances());
            auto task = [&](DbInstance* db_instance)
                {
                    Future* sub_future = future->AddSubFuture();
                    result = ExecuteSQLQuery(sub_future, db_instance, load_id,
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
                    m_add_track_mutex.unlock(db_instance->GuidIndex());
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
            m_add_track_mutex.init(NumDbInstances());
            auto task = [&](DbInstance* db_instance)
                {
                    Future* sub_future = future->AddSubFuture();
                    result = ExecuteSQLQuery(sub_future, db_instance, load_id,
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
                    m_add_track_mutex.unlock(db_instance->GuidIndex());
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
            m_add_track_mutex.init(NumDbInstances());
            auto task = [&](DbInstance* db_instance)
                {
                    Future* sub_future = future->AddSubFuture();
                    result = ExecuteSQLQuery(sub_future, db_instance, load_id,
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
                    m_add_track_mutex.unlock(db_instance->GuidIndex());
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
            m_add_track_mutex.init(NumDbInstances());
            auto task = [&](DbInstance* db_instance)
                {
                    Future* sub_future = future->AddSubFuture();
                    result = ExecuteSQLQuery(sub_future, db_instance, load_id,
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
                    m_add_track_mutex.unlock(db_instance->GuidIndex());
                };
            for (auto& guid_info : DbInstances())
            {
                threads.emplace_back(task, &guid_info.first);
            }
            for (auto& t : threads)
                t.join();
            load_id++;
        }

        ShowProgress(5, "Adding memory allocation activity tracks", kRPVDbBusy, future );
        {
            std::vector<std::thread> threads;
            m_add_track_mutex.init(NumDbInstances());
            auto task = [&](DbInstance* db_instance)
                {
                    Future* sub_future = future->AddSubFuture();
                    result = ExecuteSQLQuery(sub_future, db_instance, load_id,
                        { 
                            m_query_factory.GetRocprofMemoryActivityTrackQuery(),
                            "",
                            m_query_factory.GetRocprofMemoryActivityLevelQuery(),
                            m_query_factory.GetRocprofMemoryActivitySliceQuery(),
                            "",
                            m_query_factory.GetRocprofMemoryActivityTableQuery(),
                        },
                        &CallBackAddTrack, &CallBackLoadTrack);
                    future->DeleteSubFuture(sub_future);
                    m_add_track_mutex.unlock(db_instance->GuidIndex());
                };
            for (auto& guid_info : DbInstances())
            {
                threads.emplace_back(task, &guid_info.first);
            }
            for (auto& t : threads)
                t.join();
            load_id++;
        }
          
        for (auto& guid_info : DbInstances())
        {
            result = PopulateUnusedAgents( guid_info.first.GuidIndex());
            if (result != kRocProfVisDmResultSuccess) break;
        }

        if (result != kRocProfVisDmResultSuccess) break;

        for (int i = 0; i < NumTracks(); i++)
        {
            if (TrackPropertiesAt(i)->track_indentifiers.category == kRocProfVisDmStreamTrack)
            {
                DbInstance* db_instance = (DbInstance*)TrackPropertiesAt(i)->track_indentifiers.db_instance;
                result = PopulateStreamToHardwareFlowProperties( i, db_instance->GuidIndex());
                if (result != kRocProfVisDmResultSuccess) break;
            }
        }
        if (result != kRocProfVisDmResultSuccess) break;

        ShowProgress(10, "Loading strings", kRPVDbBusy, future );
        BindObject()->FuncAddString(BindObject()->trace_object, ""); // 0 index string
        {
            std::vector<std::thread> threads;
            m_add_track_mutex.init(NumDbInstances());
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
            m_add_track_mutex.init(NumDbInstances());
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

        std::vector<std::pair<std::string, rocprofvis_dm_event_operation_t>> table_properties =  {
            {m_metadata_version_control.GetTableName(m_metadata_version_control.kRocOptiqTableRegionLevel),kRocProfVisDmOperationLaunch},
            {m_metadata_version_control.GetTableName(m_metadata_version_control.kRocOptiqTableRegionSampleLevel),kRocProfVisDmOperationLaunchSample},
            {m_metadata_version_control.GetTableName(m_metadata_version_control.kRocOptiqTableKernelDispatchLevel), kRocProfVisDmOperationDispatch},
            {m_metadata_version_control.GetTableName(m_metadata_version_control.kRocOptiqTableMemoryAllocLevel),kRocProfVisDmOperationMemoryAllocate},
            {m_metadata_version_control.GetTableName(m_metadata_version_control.kRocOptiqTableMemoryCopyLevel), kRocProfVisDmOperationMemoryCopy} 
        };

        ShowProgress(10, "Calculating event levels", kRPVDbBusy, future);
        guid_list_t calculate_level_for_guids;
        for (auto& guid_info : DbInstances())
        {
            if (m_metadata_version_control.MustRebuildLevels(guid_info.first.FileIndex()))
            {
                calculate_level_for_guids.push_back(guid_info);
                for (auto prop : table_properties)
                {
                    m_event_levels[kRocProfVisDmOperationLaunch][guid_info.first.GuidIndex()].reserve(
                        TraceProperties()->events_count[prop.second]);
                }
            }
        }

        if (calculate_level_for_guids.size())
        {
            if (kRocProfVisDmResultSuccess !=
                ExecuteQueryForAllTracksAsync(
                    kRocProfVisDmIncludeStreamTracks,
                    kRPVQueryLevel, "SELECT *, ", (std::string(" ORDER BY ") + Builder::START_SERVICE_NAME).c_str(), &CalculateEventLevels,
                    [](rocprofvis_dm_track_params_t* params, rocprofvis_dm_charptr_t query) -> std::string {return query; },
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

            for (auto prop : table_properties)
            {
                for (auto& guid_info : DbInstances())
                {
                    CreateSQLTable(
                        (prop.first + guid_info.second).c_str(), s_level_schema_params,
                        m_event_levels[prop.second][guid_info.first.GuidIndex()].size(),
                        [&](sqlite3_stmt* stmt, int index) {
                            sqlite3_bind_int64(
                                stmt, 1,
                                m_event_levels[prop.second][guid_info.first.GuidIndex()][index].id);
                            sqlite3_bind_int(
                                stmt, 2,
                                m_event_levels[prop.second][guid_info.first.GuidIndex()][index].level_for_queue);
                            sqlite3_bind_int(
                                stmt, 3,
                                m_event_levels[prop.second][guid_info.first.GuidIndex()][index].level_for_stream);
                            sqlite3_bind_int(
                                stmt, 4,
                                m_event_levels[prop.second][guid_info.first.GuidIndex()][index].parent_id);
                        }, guid_info.first.FileIndex());
                    m_event_levels[prop.second][guid_info.first.GuidIndex()].clear();
                    m_event_levels_id_to_index[prop.second][guid_info.first.GuidIndex()].clear();
                }
            }
        }

        if (!TraceProperties()->tracks_info_restored)
        {
            ShowProgress(5, "Collecting track properties",
                kRPVDbBusy, future);
            TraceProperties()->trace_duration = 0;
            if (kRocProfVisDmResultSuccess !=
                ExecuteQueryForAllTracksAsync(
                    kRocProfVisDmIncludePmcTracks | kRocProfVisDmIncludeStreamTracks, kRPVQuerySliceByTrackSliceQuery,
                    "SELECT MIN(startTs), MAX(endTs), MIN(event_level), MAX(event_level), ",
                    "WHERE startTs != 0 AND endTs != 0", &CallbackGetTrackProperties,
                    [](rocprofvis_dm_track_params_t* params, rocprofvis_dm_charptr_t query) -> std::string {return query; },
                    [](rocprofvis_dm_track_params_t* params) {},
                    DbInstances()))
            {
                break;
            }
        }

        ShowProgress(5, "Save track information", kRPVDbBusy, future);
        SaveTrackProperties(future);


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
    auto IsSqliteTable = [](std::string name)
        {
            bool result = name == "sqlite_master" || name == "sqlite_sequence" || name == "sqlite_stat1";
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
    TemporaryDbInstance tmp_db_instance(0);


    std::filesystem::remove(new_db_path);

    while (true) 
    {
        RocprofDatabase rpDb(new_db_path);
        result = rpDb.Open();

        if (result != kRocProfVisDmResultSuccess) break;

        for (auto& file_node : m_db_nodes)
        {
            TemporaryDbInstance db_instance(file_node->node_id);

            ShowProgress(1, "Iterate over tables", kRPVDbBusy, future);            
            result = ExecuteSQLQuery(future, &db_instance, "SELECT name, sql FROM sqlite_master WHERE type='table';", "", (rocprofvis_dm_handle_t)&trim_tables, &CallbackTrimTableQuery);

            if (result != kRocProfVisDmResultSuccess) break;

            ShowProgress(1, "Iterate over views", kRPVDbBusy, future);
            result = ExecuteSQLQuery(future, &db_instance, "SELECT name, sql FROM sqlite_master WHERE type='view';", "", (rocprofvis_dm_handle_t)&trim_views, &CallbackTrimTableQuery);
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
            if(!IsSqliteTable(table.first) && !GetMetadataVersionControl()->DisposeTableWhenTrimming(table.first))
            {
                if(result == kRocProfVisDmResultSuccess)
                {
                    result = rpDb.ExecuteSQLQuery(future,&tmp_db_instance,  table.second.c_str());
                }
                else
                {
                    break;
                }
            }
        }

        if (result != kRocProfVisDmResultSuccess) break;

        std::vector<std::string> order = {"rocpd_kernel_dispatch", "rocpd_memory_allocate", "rocpd_memory_copy", "rocpd_region", "rocpd_sample", "rocpd_event", "rocpd_timestamp"};
        enum fixed_table_indices_t {
            kKernelDispatchTable,
            kMemoryAllocateTable,
            kMemoryCopyTable,
            kRegionTable,
            kSampleTable,
            kEventTable,
            kTimestampTable
        };
        std::unordered_map<std::string, size_t> priority;
        for (size_t i = 0; i < order.size(); ++i)
            priority[order[i]] = i;

        auto get_priority = [&](const std::string& s) -> size_t
            {
                for (size_t i = 0; i < order.size(); ++i)
                {
                    if (s.find(order[i]) != std::string::npos)
                        return i;
                }
                return order.size(); 
            };



        for (auto& guid_info : DbInstances())
        {
            std::vector<std::string> sorted_tables;

            for (const auto& [key, value] : trim_tables.tables)
            {
                if (key.find(GuidAt(guid_info.first.GuidIndex())) != std::string::npos)
                    sorted_tables.push_back(key);
            }

            std::sort(sorted_tables.begin(), sorted_tables.end(),
                [get_priority](std::string& a, std::string& b) {
                    size_t pa = get_priority(a);
                    size_t pb = get_priority(b);

                    if (pa != pb)
                        return pa < pb;

                    return a < b; 
                });

            ShowProgress(0, "Attaching old DB to new", kRPVDbBusy, future);

            rocprofvis_dm_timestamp_t fecth_start = start + TraceProperties()->db_inst_start_time[guid_info.first.GuidIndex()];
            rocprofvis_dm_timestamp_t fetch_end = end + TraceProperties()->db_inst_start_time[guid_info.first.GuidIndex()];
            auto& file_node = m_db_nodes[guid_info.first.FileIndex()];

            std::filesystem::path p(file_node->filepath);
            p = p.lexically_normal();

            result = rpDb.ExecuteSQLQuery(future, &tmp_db_instance, (std::string("ATTACH DATABASE '") + p.generic_string() + "' as 'oldDb';").c_str());

            if (result != kRocProfVisDmResultSuccess) break;

            for (auto const& table : sorted_tables)
            {
                if (!IsSqliteTable(table) && !GetMetadataVersionControl()->DisposeTableWhenTrimming(table) && CheckTableExists(table, file_node->node_id) )
                {
                    if (result == kRocProfVisDmResultSuccess)
                    {
                        std::string msg = "Copy table " + table;
                        ShowProgress(1, msg.c_str(), kRPVDbBusy, future);

                        if (result == kRocProfVisDmResultSuccess)
                        {
                            if (strstr(table.c_str(), "rocpd_kernel_dispatch") ||
                                strstr(table.c_str(), "rocpd_memory_allocate") ||
                                strstr(table.c_str(), "rocpd_memory_copy") ||
                                strstr(table.c_str(), "rocpd_region"))
                            {
                                query = "INSERT INTO ";
                                query += table;
                                query += " SELECT X.* FROM oldDb.";
                                query += table;
                                if (m_query_factory.IsVersionGreaterOrEqual("4"))
                                {
                                    query += " X INNER JOIN oldDb.";
                                    query += sorted_tables[kTimestampTable];
                                    query += " TS ON X.start_id == TS.id ";
                                    query += " INNER JOIN oldDb.";
                                    query += sorted_tables[kTimestampTable];
                                    query += " TE ON X.end_id == TE.id ";
                                    query += " WHERE TS.value < ";
                                    query += std::to_string(fetch_end);
                                    query += " AND TE.value > ";
                                    query += std::to_string(fecth_start);
                                }
                                else
                                {
                                    query += " X WHERE start < ";
                                    query += std::to_string(fetch_end);
                                    query += " AND end > ";
                                    query += std::to_string(fecth_start);
                                }

                                query += ";";
                            }
                            else if (strstr(table.c_str(), "rocpd_sample"))
                            {
                                query = "INSERT INTO ";
                                query += table;
                                query += " SELECT S.* FROM oldDb.";
                                query += table;
                                query += " S LEFT JOIN oldDb.";
                                query += sorted_tables[kRegionTable]; 
                                query += " R ON  S.event_id = R.event_id AND S.guid = R.guid ";
                                if (m_query_factory.IsVersionGreaterOrEqual("4"))
                                {
                                    query += " INNER JOIN oldDb.";
                                    query += sorted_tables[kTimestampTable]; 
                                    query += " TS ON S.timestamp_id == TS.id ";
                                    query += " INNER JOIN oldDb.";
                                    query += sorted_tables[kTimestampTable]; 
                                    query += " RS ON R.start_id == RS.id ";
                                    query += " WHERE (TS.value < ";
                                    query += std::to_string(fetch_end);
                                    query += " AND TS.value > ";
                                    query += std::to_string(fecth_start);
                                    query += ") OR RS.value == TS.value";
                                }
                                else
                                {
                                    query += " WHERE (timestamp < ";
                                    query += std::to_string(fetch_end);
                                    query += " AND timestamp > ";
                                    query += std::to_string(fecth_start);
                                    query += ") OR R.start == S.timestamp";
                                }
                                query += ";";
                            }
                            else if (strstr(table.c_str(), "rocpd_event"))
                            {
                                // This will make target rocpd_event table limited to selected events only 
                                query = "INSERT INTO ";
                                query += table;
                                query += " SELECT DISTINCT E.* FROM oldDb.";
                                query += table;
                                query += " E LEFT JOIN oldDb.";
                                query += sorted_tables[kKernelDispatchTable];
                                query += " K ON E.id = K.event_id LEFT JOIN oldDb.";
                                query += sorted_tables[kMemoryAllocateTable];
                                query += " MA ON E.id = MA.event_id LEFT JOIN oldDb.";
                                query += sorted_tables[kMemoryCopyTable];
                                query += " MC ON E.id = MC.event_id LEFT JOIN oldDb.";
                                query += sorted_tables[kRegionTable];
                                query += " R ON E.id = R.event_id  LEFT JOIN oldDb.";
                                query += sorted_tables[kSampleTable];
                                query += " S ON E.id = S.event_id ";
                                query += " WHERE K.event_id IS NOT NULL OR MA.event_id IS NOT NULL OR MC.event_id IS NOT NULL OR R.event_id IS NOT NULL OR S.event_id IS NOT NULL;";
                            }
                            else if (strstr(table.c_str(), "rocpd_arg") ||
                                strstr(table.c_str(), "rocpd_pmc_event"))
                            {
                                // This will make target rocpd_arg and rocpd_pmc_event table limited to selected events only 
                                query = "INSERT INTO ";
                                query += table;
                                query += " SELECT X.* FROM oldDb.";
                                query += table;
                                query += " X LEFT JOIN oldDb.";
                                query += sorted_tables[kEventTable];
                                query += " E ON E.id = X.event_id ";
                                query += " WHERE E.id IS NOT NULL;";
                            }
                            else
                            {
                                query = "INSERT INTO ";
                                query += table;
                                query += " SELECT * FROM oldDb.";
                                query += table;
                                query += ";";
                            }

                            result = rpDb.ExecuteSQLQuery(future,&tmp_db_instance, query.c_str());
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
            result = rpDb.ExecuteSQLQuery(future,&tmp_db_instance, "DETACH oldDb;");

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
                    result = rpDb.ExecuteSQLQuery(future,&tmp_db_instance,  it->second.c_str());
                }
                else
                {
                    result = rpDb.ExecuteSQLQuery(future, &tmp_db_instance, view.second.c_str());
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

            DbInstance* node_ptr = DbInstancePtrAt(event_id.bitfield.event_node);
            ROCPROFVIS_ASSERT_MSG_BREAK(node_ptr, ERROR_NODE_KEY_CANNOT_BE_NULL);
            ShowProgress(0, query.str().c_str(), kRPVDbBusy, future);
            if (event_id.bitfield.event_op == kRocProfVisDmOperationLaunch || event_id.bitfield.event_op == kRocProfVisDmOperationLaunchSample)
            {
                std::string level_table = event_id.bitfield.event_op == kRocProfVisDmOperationLaunch ?
                    " roc_optiq_event_levels_launch" :
                    " roc_optiq_event_levels_launch_sample";
                std::string callstack_params = m_query_factory.IsVersionGreaterOrEqual("4") ? 
                    " 4 as version, R.id, L.parent_id, R.name_id, PC.function as p1, PC.file as p2, CODE.line_number as p3" : 
                    " 3 as version, R.id, L.parent_id, R.name_id, E.call_stack as p1, E.line_info as p2, 0 as p3 ";
                std::stringstream callstack_tables; 
                callstack_tables << " rocpd_region_%GUID% R ";
                callstack_tables << " INNER JOIN " << level_table << "_%GUID% L ON R.id = L.eid ";
                callstack_tables << " INNER JOIN rocpd_event_%GUID% E ON R.event_id = E.id ";
                if (m_query_factory.IsVersionGreaterOrEqual("4"))
                {
                    callstack_tables << " INNER JOIN rocpd_call_stack_%GUID% CS ON E.stack_id = CS.id ";
                    callstack_tables << " INNER JOIN rocpd_info_pc_%GUID% PC ON CS.pc_id = PC.id ";
                    callstack_tables << " INNER JOIN rocpd_line_info_%GUID% LI ON LI.pc_id = CS.pc_id AND E.id = LI.event_id ";
                    callstack_tables << " INNER JOIN rocpd_info_source_code_%GUID% CODE ON LI.source_code_id = CODE.id ";
                }
                query << "WITH RECURSIVE stack_chain AS (SELECT 0 AS depth, " << callstack_params;
                query << " FROM " << callstack_tables.str();
                query << " WHERE R.id = " << event_id.bitfield.event_id;
                query << " UNION SELECT sc.depth + 1, " << callstack_params;
                query << " FROM " << callstack_tables.str();
                query << " JOIN stack_chain sc ON sc.parent_id = L.eid) ";
                query << " SELECT sc.version, sc.id, sc.p1, sc.p2, sc.p3, S.string, sc.depth FROM stack_chain sc ";
                query << " LEFT JOIN rocpd_string_%GUID% S ON S.id = sc.name_id ";
                query << " ORDER BY depth DESC;";
                if (kRocProfVisDmResultSuccess != ExecuteSQLQuery(future, node_ptr, query.str().c_str(),
                    stacktrace,
                    &CallbackAddStackTrace))
                {
                    break;
                }
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
        future->SetRuntimeStorageValue(kRPVFutureStorageEventId, event_id.value);
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
            future->ResetRowCount();
            query = m_query_factory.GetRocprofArgumentsInfoQueryForRegionEvent(event_id.bitfield.event_id);
            if(kRocProfVisDmResultSuccess != ExecuteSQLQuery(future, node_ptr, query.c_str(), extdata, &CallbackAddArgumentsInfo)) break;

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
            future->ResetRowCount();
            query = m_query_factory.GetRocprofArgumentsInfoQueryForKernelDispatchEvent(event_id.bitfield.event_id);
            if(kRocProfVisDmResultSuccess != ExecuteSQLQuery(future, node_ptr, query.c_str(), extdata, &CallbackAddArgumentsInfo)) break;
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
            future->ResetRowCount();
            query = m_query_factory.GetRocprofArgumentsInfoQueryForMemoryAllocEvent(event_id.bitfield.event_id);
            if(kRocProfVisDmResultSuccess != ExecuteSQLQuery(future, node_ptr, query.c_str(), extdata, &CallbackAddArgumentsInfo)) break;
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
            future->ResetRowCount();
            query = m_query_factory.GetRocprofArgumentsInfoQueryForMemoryCopyEvent(event_id.bitfield.event_id);
            if(kRocProfVisDmResultSuccess != ExecuteSQLQuery(future, node_ptr, query.c_str(), extdata, &CallbackAddArgumentsInfo)) break;
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
