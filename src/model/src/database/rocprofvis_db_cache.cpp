// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_db_cache.h"
#include "rocprofvis_db_query_builder.h"
#include "rocprofvis_db_systems.h"

namespace RocProfVis
{
namespace DataModel
{

void
DatabaseCache::AddTableCell(const char* table_name, uint64_t row_id,
                            uint32_t column_index, std::string cell_value)
{
    m_catalog.SetCellText(table_name, row_id, column_index, cell_value);
}
void
DatabaseCache::AddTableCell(const char* table_name, uint64_t row_id,
                            std::string               column_name,
                            rocprofvis_db_data_type_t column_type, std::string cell_value)
{
    m_catalog.SetNamedCellText(table_name, row_id, column_name, column_type, cell_value);
}
void
DatabaseCache::AddTableRow(const char* table_name, uint64_t row_id)
{
    m_catalog.AddRow(table_name, row_id);
}
void
DatabaseCache::AddTableColumn(const char* table_name, std::string column_name,
                              rocprofvis_db_data_type_t column_type)
{
    m_catalog.AddColumn(table_name, column_name, column_type);
}
const char*
DatabaseCache::GetTableCell(const char* table_name, uint64_t row_id,
                            std::string column_name)
{
    return m_catalog.CellText(table_name, row_id, column_name);
}
const char*
DatabaseCache::GetTableCellByIndex(const char* table_name, uint32_t row_index,
                                   std::string column_name)
{
    return m_catalog.CellTextByIndex(table_name, row_index, column_name);
}

void*
DatabaseCache::GetTableHandle(const char* table_name)
{
    return m_catalog.Table(table_name);
}

InformationCatalog&
DatabaseCache::Catalog()
{
    return m_catalog;
}

const InformationCatalog&
DatabaseCache::Catalog() const
{
    return m_catalog;
}

rocprofvis_dm_size_t
DatabaseCache::GetMemoryFootprint()
{
    // TODO : Calculate memory size
    size_t size = 0;
    return size;
}

rocprofvis_dm_result_t
DatabaseCache::PopulateTrackExtendedDataTemplate(SystemDatabase* db,
                                                 uint32_t        db_instance_id,
                                                 std::string     table_name,
                                                 uint64_t        process_id)
{
    rocprofvis_dm_track_params_t* track_properties = db->TrackPropertiesLast();
    CatalogTable*                    table            = m_catalog.Table(table_name.c_str());
    if(table == nullptr)
    {
        return kRocProfVisDmResultInvalidParameter;
    }
    uint32_t    num_columns = table->NumColumns();
    std::string str_id      = std::to_string(process_id);
    for(uint32_t i = 0; i < num_columns; i++)
    {
        rocprofvis_db_ext_data_t record;
        record.name        = table->ColumnName(i);
        record.data        = str_id.c_str();
        record.category    = table_name.c_str();
        record.type        = table->ColumnType(i);
        record.db_instance = db_instance_id;
        rocprofvis_dm_result_t result =
            db->BindObject()->FuncAddExtDataRecord(track_properties->extdata, record);
        if(result != kRocProfVisDmResultSuccess) return result;
    }
    return PopulateTrackTopologyData(db, &track_properties->track_indentifiers,
                                     db_instance_id, table_name, process_id);
}

rocprofvis_dm_result_t
DatabaseCache::PopulateTrackTopologyData(
    Database* db, rocprofvis_dm_track_identifiers_t* track_indentifiers,
    uint32_t db_instance_id, std::string table_name, uint64_t process_id)
{
    (void) db_instance_id;
    CatalogTable* table = m_catalog.Table(table_name.c_str());
    if(table == nullptr)
    {
        return kRocProfVisDmResultInvalidParameter;
    }
    uint32_t num_columns = table->NumColumns();
    for(uint32_t i = 0; i < num_columns; i++)
    {
        const char*                     name = table->ColumnName(i);
        const rocprofvis_db_data_type_t type = table->ColumnType(i);
        rocprofvis_dm_result_t result = db->BindObject()->FuncAddTopologyNodeProperty(
            db->BindObject()->trace_object, track_indentifiers,
            (rocprofvis_db_topology_data_type_t) type, table_name.c_str(), name,
            (void*) table->CellText(process_id, name));
        if(result != kRocProfVisDmResultSuccess) return result;
    }
    return kRocProfVisDmResultSuccess;
}

uint32_t
StringTable::ToInt(const char* str)
{
    std::unique_lock lock(m_mutex);

    auto it = m_str_to_id.find(str);
    if(it != m_str_to_id.end()) return it->second;

    uint32_t id = static_cast<uint32_t>(m_id_to_str.size());
    m_id_to_str.push_back(str);
    m_str_to_id[str] = id;
    return id;
}

const char*
StringTable::ToString(uint32_t id) const
{
    std::shared_lock lock(m_mutex);
    if(id >= m_id_to_str.size()) return "";
    return m_id_to_str[id].c_str();
}

void
StringTable::Clear()
{
    std::unique_lock lock(m_mutex);
    m_id_to_str.clear();
    m_str_to_id.clear();
}

}  // namespace DataModel
}  // namespace RocProfVis
