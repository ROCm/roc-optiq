// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_common_types.h"
#include <vector>
#include <unordered_map>
#include <shared_mutex>

namespace RocProfVis
{
namespace DataModel
{

//types for creating multi-dimensional map array to cache non-essential track information
typedef std::map<std::string, std::string> table_dict_t;
typedef std::map<uint64_t, table_dict_t> table_map_t;
typedef std::map<std::string, table_map_t> ref_map_t;

class Database;

// Helper class to manage cached information tables (node, agent, queue, process, thread information)

class TableCache
{
public:
    struct Row {
        uint64_t id;
        std::vector<std::string> values;
    };

    uint32_t AddColumn(const std::string& name, rocprofvis_db_data_type_t type);

    void AddRow(uint64_t id);

    void AddCell(uint32_t column_index, uint64_t row_id, std::string cell);

    const char* GetCell(uint64_t row_id, const char* column_name);

    const char* GetCell(uint64_t row_index, uint32_t column_index);

    const char* GetCellByIndex(uint64_t row_index, const char* column_name);

    const char* GetColumnName(uint32_t column_index);

    const rocprofvis_db_data_type_t GetColumnType(uint32_t column_index);

    void* GetRow(uint64_t row_index);

    uint32_t NumColumns() { return m_columns.size(); }
    size_t NumRows() { return m_rows.size(); }

private:
    std::string name;
    std::vector<std::pair<std::string, rocprofvis_db_data_type_t>> m_columns;
    std::vector<Row> m_rows;
    std::unordered_map<std::string, uint32_t> m_column_index;
    std::unordered_map<uint64_t, uint32_t> m_row_index;
};

class DatabaseCache 
{
    public:
        void AddTableCell(const char* table_name, uint64_t row_id, uint32_t column_index, const char* cell_value);
        void AddTableCell(const char* table_name, uint64_t row_id, const char* column_name, rocprofvis_db_data_type_t column_type, const char* cell_value);
        void AddTableRow(const char* table_name, uint64_t row_id);
        void AddTableColumn(const char* table_name, const char* column_name, rocprofvis_db_data_type_t column_type);
        const char* GetTableCell(const char* table_name, uint64_t row_id, const char* column_name);
        const char* GetTableCellByIndex(const char* table_name, uint32_t row_index, const char* column_name);
        void* GetTableHandle(const char* table_name);
      
        // Populate track extended data objects and topology tree with table content
        rocprofvis_dm_result_t PopulateTrackExtendedDataTemplate(Database * db, uint32_t node_id, const char* table_name, uint64_t instance_id ); 
        // Populate track extended data objects and topology tree with table content
        rocprofvis_dm_result_t PopulateTrackTopologyData(Database * db, rocprofvis_dm_track_identifiers_t * track_indentifiers, uint32_t db_instance_id, const char* table_name, uint64_t process_id );
        // Get amount of memory used by the cached values map
        rocprofvis_dm_size_t    GetMemoryFootprint(void); 

    private:
        
        std::map<std::string, TableCache> tables;
        std::shared_mutex m_mutex;

};

class StringTable 
{
public:        
    uint32_t ToInt(const char* s);
    const char* ToString(uint32_t id) const;
    void Clear();

private:
    mutable std::shared_mutex m_mutex;
    std::unordered_map<std::string, uint32_t> m_str_to_id;
    std::vector<rocprofvis_dm_string_t> m_id_to_str;
};

}  // namespace DataModel
}  // namespace RocProfVis