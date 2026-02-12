// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_db_cache.h"
#include "rocprofvis_db.h"
#include "rocprofvis_db_query_builder.h"

namespace RocProfVis
{
namespace DataModel
{

    uint32_t TableCache::AddColumn(const std::string& name, rocprofvis_db_data_type_t type)
    {
        uint32_t index = 0;
        auto it = m_column_index.find(name);
        if (it == m_column_index.end())
        {
            index = m_columns.size();
            m_column_index[name] = index;
            m_columns.push_back( std::make_pair(name,type) );
        }
        else
        {
            index = it->second;
        }
        return index;
    }

    void TableCache::AddRow(uint64_t id)
    {
        auto it = m_row_index.find(id);
        if (it == m_row_index.end())
        {
            m_row_index[id] = m_rows.size();
            m_rows.push_back({ id });
            m_rows.back().values.resize(m_columns.size());
        }
    }

    void TableCache::AddCell(uint32_t column_index, uint64_t row_id, std::string cell)
    {
        auto it = m_row_index.find(row_id);
        if (it != m_row_index.end())
        {
            if (column_index >= m_rows[it->second].values.size())
            {
                m_rows[it->second].values.resize(m_columns.size());
            }
            m_rows[it->second].values[column_index] = cell;
        }
        else
        {
            AddRow(row_id);
            m_rows.back().values[column_index] = cell;
        }
    }

    const char* TableCache::GetCell(uint64_t row_id, const char* column_name)
    {
        auto it_row = m_row_index.find(row_id);
        auto it_column = m_column_index.find(column_name);
        if (it_row != m_row_index.end() && it_column!=m_column_index.end())
        {
            return m_rows[it_row->second].values[it_column->second].c_str();
        }
        return "";
    }

    const char* TableCache::GetCell(uint64_t row_index, uint32_t column_index)
    {
        if (row_index < m_rows.size() && column_index < m_columns.size())
        {
            return m_rows[row_index].values[column_index].c_str();
        }
        return "";
    }

    const char* TableCache::GetCellByIndex(uint64_t row_index, const char* column_name)
    {
        auto it_column = m_column_index.find(column_name);
        if (row_index < m_rows.size() && it_column!=m_column_index.end())
        {
            return m_rows[row_index].values[it_column->second].c_str();
        }
        return "";
    }

    const char* TableCache::GetColumnName(uint32_t column_index)
    {
        if (column_index < m_columns.size())
        {
            return m_columns[column_index].first.c_str();
        }
        return "";
    }

    rocprofvis_db_data_type_t TableCache::GetColumnType(uint32_t column_index)
    {
        if (column_index < m_columns.size())
        {
            return m_columns[column_index].second;
        }
        return kRPVDataTypeNull;
    }

    void* TableCache::GetRow(uint64_t row_index)
    {
        if (row_index < m_rows.size())
        {
            return &m_rows[row_index];
        }
        return nullptr;
    }

    void DatabaseCache::AddTableCell(const char* table_name, uint64_t row_id, uint32_t column_index, const char* cell_value)
    {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        tables[table_name].AddCell(column_index, row_id, cell_value);
    }
    void DatabaseCache::AddTableCell(const char* table_name, uint64_t row_id, const char* column_name, rocprofvis_db_data_type_t column_type, const char* cell_value)
    {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        uint32_t column_index = tables[table_name].AddColumn(column_name, column_type);
        tables[table_name].AddCell(column_index, row_id, cell_value);
    }
    void DatabaseCache::AddTableRow(const char* table_name, uint64_t row_id)
    {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        tables[table_name].AddRow(row_id);
    }
    void DatabaseCache::AddTableColumn(const char* table_name, const char* column_name, rocprofvis_db_data_type_t column_type)
    {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        tables[table_name].AddColumn(column_name, column_type);
    }
    const char* DatabaseCache::GetTableCell(const char* table_name, uint64_t row_id, const char* column_name)
    {
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        return tables[table_name].GetCell(row_id,column_name);
    }
    const char* DatabaseCache::GetTableCellByIndex(const char* table_name, uint32_t row_index, const char* column_name)
    {
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        return tables[table_name].GetCellByIndex(row_index,column_name);
    }

    void* DatabaseCache::GetTableHandle(const char* table_name)
    {
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        return &tables[table_name];
    }

    rocprofvis_dm_result_t DatabaseCache::PopulateTrackExtendedDataTemplate(Database * db, uint32_t db_instance_id, const char* table_name, uint64_t process_id ){
        rocprofvis_dm_track_params_t* track_properties = db->TrackPropertiesLast();
        TableCache& table = tables[table_name];
        uint32_t num_columns = table.NumColumns();
        std::string str_id = std::to_string(process_id);   
        for (int i = 0; i < num_columns; i++)
        { 
            rocprofvis_db_ext_data_t record;
            record.name = table.GetColumnName(i);
            record.data     = str_id.c_str();
            record.category = table_name;
            record.type  = table.GetColumnType(i);
            record.db_instance = db_instance_id;
            rocprofvis_dm_result_t result = db->BindObject()->FuncAddExtDataRecord(track_properties->extdata, record);
            if (result != kRocProfVisDmResultSuccess) return result;
        }
        return PopulateTrackTopologyData(db, &track_properties->track_indentifiers, db_instance_id, table_name, process_id);
    }

    rocprofvis_dm_result_t DatabaseCache::PopulateTrackTopologyData(Database * db, rocprofvis_dm_track_identifiers_t * track_indentifiers, uint32_t db_instance_id, const char* table_name, uint64_t process_id ){
        TableCache& table = tables[table_name];
        uint32_t num_columns = table.NumColumns();
        for (int i = 0; i < num_columns; i++)
        { 
            const char* name = table.GetColumnName(i);
            const rocprofvis_db_data_type_t type  = table.GetColumnType(i);
            rocprofvis_dm_result_t result = db->BindObject()->FuncAddTopologyNodeProperty(
                db->BindObject()->trace_object, 
                track_indentifiers, 
                (rocprofvis_db_topology_data_type_t)type, 
                table_name,  
                name,
                (void*)table.GetCell(process_id, name));
            if (result != kRocProfVisDmResultSuccess) return result;
        }
        return kRocProfVisDmResultSuccess;
    }


    uint32_t StringTable::ToInt(const char* str) {
        std::unique_lock lock(m_mutex);

        auto it = m_str_to_id.find(str);
        if (it != m_str_to_id.end())
            return it->second;

        uint32_t id = m_id_to_str.size();
        m_id_to_str.push_back(str);
        m_str_to_id[str] = id;
        return id;
    }

    const char* StringTable::ToString(uint32_t id) const {
        std::shared_lock lock(m_mutex);
        if (id >= m_id_to_str.size()) return "";
        return m_id_to_str[id].c_str();
    }

    void StringTable::Clear() {
        std::unique_lock lock(m_mutex);
        m_id_to_str.clear();
        m_str_to_id.clear();
    }


}  // namespace DataModel
}  // namespace RocProfVis