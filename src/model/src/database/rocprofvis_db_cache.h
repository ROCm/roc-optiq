// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_common_types.h"
#include "rocprofvis_db_information_catalog.h"

#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace RocProfVis
{
namespace DataModel
{

class Database;
class SystemDatabase;

/**
 * @brief Adapter-facing cache. Storage is an InformationCatalog. Existing
 *        callers keep these methods; new readers use Catalog().
 */
class DatabaseCache
{
public:
    void AddTableCell(const char* table_name, uint64_t row_id, uint32_t column_index,
                      std::string cell_value);
    void AddTableCell(const char* table_name, uint64_t row_id, std::string column_name,
                      rocprofvis_db_data_type_t column_type, std::string cell_value);
    void AddTableRow(const char* table_name, uint64_t row_id);
    void AddTableColumn(const char* table_name, std::string column_name,
                        rocprofvis_db_data_type_t column_type);
    const char* GetTableCell(const char* table_name, uint64_t row_id,
                             std::string column_name);
    const char* GetTableCellByIndex(const char* table_name, uint32_t row_index,
                                    std::string column_name);
    void*       GetTableHandle(const char* table_name);

    InformationCatalog&       Catalog();
    const InformationCatalog& Catalog() const;

    rocprofvis_dm_result_t PopulateTrackExtendedDataTemplate(SystemDatabase* db,
                                                             uint32_t        node_id,
                                                             std::string     table_name,
                                                             uint64_t        instance_id);
    rocprofvis_dm_result_t PopulateTrackTopologyData(
        Database* db, rocprofvis_dm_track_identifiers_t* track_indentifiers,
        uint32_t db_instance_id, std::string table_name, uint64_t instance_id);
    rocprofvis_dm_size_t GetMemoryFootprint(void);

private:
    InformationCatalog m_catalog;
};

class StringTable
{
public:
    uint32_t    ToInt(const char* s);
    const char* ToString(uint32_t id) const;
    void        Clear();

private:
    mutable std::shared_mutex                 m_mutex;
    std::unordered_map<std::string, uint32_t> m_str_to_id;
    std::vector<rocprofvis_dm_string_t>       m_id_to_str;
};

}  // namespace DataModel
}  // namespace RocProfVis