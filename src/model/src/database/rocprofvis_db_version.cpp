// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_db_version.h"
#include "rocprofvis_db_rocprof.h"
#include "rocprofvis_db_rocpd.h"

namespace RocProfVis
{
namespace DataModel
{

    RocprofMetadataVersionControl::RocprofMetadataVersionControl(RocprofDatabase* db) : MetadataVersionControl(db) {

        m_roc_optiq_table_properties.resize(kRocOptiqNumTables);

        m_roc_optiq_table_properties[kRocOptiqTableMemoryActivity] = {
            "roc_optiq_memory_activity_",
            kRocOptiqTablePerGuid,
            kRocOptiqTableCopyWhenTrimmed,
            kRocOptiqTableDependentOnNoDependency,
            kRocOptiqTableVersionMemoryActivity,
            db->GetMemoryActivityTableSchemaHash(),
        };
        m_roc_optiq_table_properties[kRocOptiqTableMemoryAllocate] = {
            "roc_optiq_memory_allocate_",
            kRocOptiqTablePerGuid, 
            kRocOptiqTableDisposeWhenTrimmed,
            kRocOptiqTableDependentOnMemoryActivity, 
            kRocOptiqTableVersionMemoryAllocate, 
            0,
        };
        m_roc_optiq_table_properties[kRocOptiqTableTrackInfo] = {
            "roc_optiq_track_info",
            kRocOptiqTablePerFile, 
            kRocOptiqTableDisposeWhenTrimmed,
            kRocOptiqTableDependentOnMemoryVisibility, 
            kRocOptiqTableVersionTrackInfo, 
            db->GetHistogramQueryAndSchemaHash(),
        };
        m_roc_optiq_table_properties[kRocOptiqTableKernelDispatchLevel] = {
            "roc_optiq_event_levels_dispatch_",
            kRocOptiqTablePerGuid, 
            kRocOptiqTableDisposeWhenTrimmed,
            kRocOptiqTableDependentOnAllLevelTables, 
            kRocOptiqTableVersionKernelDispatchLevel, 
            std::hash<std::string>{}(db->m_query_factory.GetRocprofKernelDispatchLevelQuery()+db->GetLevelSchemaHashStr()),
        };
        m_roc_optiq_table_properties[kRocOptiqTableRegionLevel] = {
            "roc_optiq_event_levels_launch_",
            kRocOptiqTablePerGuid, 
            kRocOptiqTableDisposeWhenTrimmed,
            kRocOptiqTableDependentOnAllLevelTables, 
            kRocOptiqTableVersionRegionLevel, 
            std::hash<std::string>{}(db->m_query_factory.GetRocprofRegionLevelQuery(false)+db->GetLevelSchemaHashStr()),
        };
        m_roc_optiq_table_properties[kRocOptiqTableRegionSampleLevel] = {
            "roc_optiq_event_levels_launch_sample_",
            kRocOptiqTablePerGuid, 
            kRocOptiqTableDisposeWhenTrimmed,
            kRocOptiqTableDependentOnAllLevelTables, 
            kRocOptiqTableVersionRegionSampleLevel, 
            std::hash<std::string>{}(db->m_query_factory.GetRocprofRegionLevelQuery(true)+db->GetLevelSchemaHashStr()),
        };
        m_roc_optiq_table_properties[kRocOptiqTableMemoryAllocLevel] = {
            "roc_optiq_event_levels_mem_alloc_",
            kRocOptiqTablePerGuid, 
            kRocOptiqTableDisposeWhenTrimmed,
            kRocOptiqTableDependentOnAllLevelTables, 
            kRocOptiqTableVersionMemoryAllocLevel, 
            std::hash<std::string>{}(db->m_query_factory.GetRocprofMemoryAllocLevelQuery()+db->GetLevelSchemaHashStr()),
        };
        m_roc_optiq_table_properties[kRocOptiqTableMemoryCopyLevel] = {
            "roc_optiq_event_levels_mem_copy_", 
            kRocOptiqTablePerGuid, 
            kRocOptiqTableDisposeWhenTrimmed,
            kRocOptiqTableDependentOnAllLevelTables, 
            kRocOptiqTableVersionMemoryCopyLevel, 
            std::hash<std::string>{}(db->m_query_factory.GetRocprofMemoryCopyLevelQuery()+db->GetLevelSchemaHashStr()),
        };
        m_roc_optiq_table_properties[kRocOptiqTableHistogram] = {
            "roc_optiq_histogram",
            kRocOptiqTablePerFile,
            kRocOptiqTableDisposeWhenTrimmed,
            kRocOptiqTableDependentOnTrackInfo, 
            kRocOptiqTableVersionMemoryCopyLevel, 
            db->GetHistogramQueryAndSchemaHash(),
        };
    }

    RocpdMetadataVersionControl::RocpdMetadataVersionControl(RocpdDatabase* db) : MetadataVersionControl(db) {

        m_roc_optiq_table_properties.resize(kRocOptiqNumTables);

        m_roc_optiq_table_properties[kRocOptiqTableTrackInfo] = {
            "roc_optiq_track_info",
            kRocOptiqTablePerFile,
            kRocOptiqTableDisposeWhenTrimmed,
            kRocOptiqTableDependentOnNoDependency, 
            kRocOptiqTableVersionMemoryCopyLevel, 
            db->GetHistogramQueryAndSchemaHash()
        };
        m_roc_optiq_table_properties[kRocOptiqTableKernelDispatchLevel] = {
            "roc_optiq_event_levels_op",
            kRocOptiqTablePerFile,
            kRocOptiqTableDisposeWhenTrimmed,
            kRocOptiqTableDependentOnAllLevelTables, 
            kRocOptiqTableVersionKernelDispatchLevel, 
            std::hash<std::string>{}(db->GetEventLevelQuery(kRocProfVisDmKernelDispatchTrack)+db->GetLevelSchemaHashStr())
        };
        m_roc_optiq_table_properties[kRocOptiqTableRegionLevel] = {
            "roc_optiq_event_levels_api",
            kRocOptiqTablePerFile,
            kRocOptiqTableDisposeWhenTrimmed,
            kRocOptiqTableDependentOnAllLevelTables, 
            kRocOptiqTableVersionRegionLevel, 
            std::hash<std::string>{}(db->GetEventLevelQuery(kRocProfVisDmRegionTrack)+db->GetLevelSchemaHashStr())
        };
        m_roc_optiq_table_properties[kRocOptiqTableHistogram] = {
            "roc_optiq_histogram",
            kRocOptiqTablePerFile,
            kRocOptiqTableDisposeWhenTrimmed,
            kRocOptiqTableDependentOnTrackInfo, 
            kRocOptiqTableVersionMemoryCopyLevel, 
            db->GetHistogramQueryAndSchemaHash()
        };
    }

    bool MetadataVersionControl::DisposeTableWhenTrimming(std::string table_name) {
        if (table_name.find(s_metadata_table_name) == 0) return true;
        auto it = find_if(m_roc_optiq_table_properties.begin(), m_roc_optiq_table_properties.end(),
            [table_name](roc_optiq_metadata_t& data) {return table_name.find(data.name) == 0 && data.trim_type == kRocOptiqTableDisposeWhenTrimmed; });
        return it != m_roc_optiq_table_properties.end();
    }

    rocprofvis_dm_result_t MetadataVersionControl::DropAllRocOtiqTables(Future* future, uint32_t file_node_id) {
        rocprofvis_dm_result_t result = kRocProfVisDmResultNotLoaded;
        rocprofvis_db_sqlite_trim_parameters tables_to_delete;
        TemporaryDbInstance db_instance(file_node_id);
        std::string query;
        for (auto name : s_pre_metadata_base_names)
        {
            query = std::string("SELECT name, sql FROM sqlite_master WHERE type='table' AND name LIKE '") + name + "%'";
            result = m_db->ExecuteSQLQuery(future, &db_instance, query.c_str(), "", (rocprofvis_dm_handle_t)&tables_to_delete, &m_db->CallbackTrimTableQuery);
        }
        for (auto prop : m_roc_optiq_table_properties)
        {
            query = std::string("SELECT name, sql FROM sqlite_master WHERE type='table' AND name LIKE '") + prop.name + "%'";
            result = m_db->ExecuteSQLQuery(future, &db_instance, query.c_str(), "", (rocprofvis_dm_handle_t)&tables_to_delete, &m_db->CallbackTrimTableQuery);
        }
        for (auto table : tables_to_delete.tables)
        {
            m_db->DropSQLTable(table.first.c_str(), file_node_id);
        }
        return result;
    }

    rocprofvis_dm_result_t MetadataVersionControl::CleanupDatabase(Future* future, bool rebuild)
    {
        rocprofvis_dm_result_t result = kRocProfVisDmResultNotLoaded;
        for (auto& file_node : m_db->m_db_nodes)
        {
            TemporaryDbInstance db_instance(file_node->node_id);
            result = DropAllRocOtiqTables(future, file_node->node_id);
            if (kRocProfVisDmResultSuccess != result) return result;
            auto indexes = m_db->GetRocpdIndexes(file_node->node_id);
            for (auto& index : indexes)
            {
                result = m_db->DropSQLIndex(index.c_str(), file_node->node_id);
                if (kRocProfVisDmResultSuccess != result) return result;
            }
            if (rebuild)
            {
                result = m_db->ExecuteSQLQuery(future, &db_instance, "VACUUM;");
            }
        }
        return result;
    }
  

	rocprofvis_dm_result_t MetadataVersionControl::VerifyRocOptiqTablesVersions(Future* future) {
        rocprofvis_dm_result_t result = kRocProfVisDmResultNotLoaded;
        std::string query = "SELECT * from ";
        std::string metadata_table_name = s_metadata_table_name;
        std::string hash_str;
        for (auto prop : m_roc_optiq_table_properties)
        {
            hash_str = hash_str +
                prop.name + 
                std::to_string(prop.hash) +
                std::to_string(prop.type) +
                std::to_string(prop.trim_type) +
                std::to_string(prop.dependency);
        }
        for (auto& file_node : m_db->m_db_nodes)
        {
            hash_str = hash_str + file_node->filepath.c_str();
        }
        hash_str = std::to_string(std::hash<std::string>{}(hash_str));
        metadata_table_name = metadata_table_name + "_" + hash_str;
        query += metadata_table_name;
        SQLInsertParams metadata_schema_params = { { "id", "INTEGER PRIMARY KEY" }, { "name", "TEXT" }, { "type", "INTEGER" } , { "version", "INTEGER" }, { "hash", "INTEGER" } };
        for (auto& file_node : m_db->m_db_nodes)
        {
            DatabaseCache metadata_table;
            TemporaryDbInstance db_instance(file_node->node_id);
            future->ResetRowCount();
            bool update_metadata_table = false;
            if (false == m_db->CheckTableExists(metadata_table_name, file_node->node_id))
            {             
                result = DropAllRocOtiqTables(future, file_node->node_id);
                update_metadata_table=true;
                m_rebuild_all = true;
            }
            else
            {
                result = m_db->ExecuteSQLQuery(future, &db_instance, query.c_str(), s_metadata_cache_table_name, (rocprofvis_dm_handle_t)&metadata_table, &m_db->CallbackCacheTable);
                if (result == kRocProfVisDmResultSuccess)
                {
                    TableCache* table = (TableCache*)metadata_table.GetTableHandle(s_metadata_cache_table_name);
                    uint32_t dependency_mask = 0;
                    for (int i = 0; i < table->NumRows(); i++)
                    {
                        uint32_t id = std::atol(table->GetCellByIndex(i, "id"));
                        std::string base_name = table->GetCellByIndex(i, "name");
                        roc_optiq_table_type type = (roc_optiq_table_type)std::atol(table->GetCellByIndex(i, "type"));
                        uint64_t hash = std::atoll(table->GetCellByIndex(i, "hash"));
                        uint32_t version = std::atol(table->GetCellByIndex(i, "version"));
                        roc_optiq_metadata_t& data = m_roc_optiq_table_properties[id];

                        if (data.type == kRocOptiqTablePerGuid)
                        {
                            for (auto& guid_info : m_db->DbInstances())
                            {
                                if (guid_info.first.FileIndex() == file_node->node_id)
                                {
                                    std::string table_name = base_name + m_db->GuidAt(guid_info.first.GuidIndex());
                                    if (false == m_db->CheckTableExists(table_name, file_node->node_id))
                                    {
                                        data.rebuild[file_node->node_id] = true;
                                        update_metadata_table = true;
                                        dependency_mask |= 1 << id;
                                    }
                                    else
                                    {
                                        if (version != data.version || hash != data.hash || (data.dependency & dependency_mask))
                                        {
                                            m_db->DropSQLTable(table_name.c_str(), file_node->node_id);
                                            data.rebuild[file_node->node_id] = true;
                                            update_metadata_table = true;
                                            dependency_mask |= 1 << id;
                                        }
                                    }
                                }
                            }
                        }
                        else
                        {
                            if (false == m_db->CheckTableExists(base_name, file_node->node_id))
                            {
                                data.rebuild[file_node->node_id] = true;
                                update_metadata_table = true;
                                dependency_mask |= 1 << id;
                            }
                            else
                            {
                                if (version != data.version || hash != data.hash || (data.dependency & dependency_mask))
                                {
                                    m_db->DropSQLTable(base_name.c_str(), file_node->node_id);
                                    data.rebuild[file_node->node_id] = true;
                                    update_metadata_table = true;
                                    dependency_mask |= 1 << id;
                                }
                            }
                        }
                    }
                }
                else
                {
                    update_metadata_table = true;
                    m_rebuild_all = true;
                }
            }
            if (update_metadata_table)
            {
                result = m_db->CreateSQLTable(
                    metadata_table_name.c_str(),
                    metadata_schema_params,
                    m_roc_optiq_table_properties.size(),
                    [&](sqlite3_stmt* stmt, int index) {
                        roc_optiq_metadata_t& p = m_roc_optiq_table_properties[index];
                        uint32_t column_index = 1;
                        sqlite3_bind_int(stmt, column_index++, index);
                        sqlite3_bind_text(stmt, column_index++, p.name.c_str(), -1, SQLITE_STATIC);
                        sqlite3_bind_int(stmt, column_index++, p.type);
                        sqlite3_bind_int(stmt, column_index++, p.version);
                        sqlite3_bind_int64(stmt, column_index++, p.hash);
                    }, file_node->node_id);
            }

        }
        return result;
	}

}  // namespace DataModel
}  // namespace RocProfVis