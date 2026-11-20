// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_c_interface_types.h"

#include <cstdint>
#include <map>
#include <shared_mutex>
#include <string>
#include <vector>

namespace RocProfVis
{
namespace DataModel
{

/** @brief One metadata value. A null value is distinct from integer zero. */
class CatalogValue
{
public:
    CatalogValue();

    rocprofvis_db_data_type_t Type() const;
    bool                      HasInt() const;
    bool                      HasDouble() const;
    int64_t                   Int() const;
    double                    Double() const;
    /** @brief Text form. Empty when the value was not recorded. Stable until
     *         the owning record is replaced. */
    const char* Text() const;

    void SetNull();
    void SetInt(int64_t value);
    void SetDouble(double value);
    void SetText(rocprofvis_db_data_type_t type, const std::string& text);

private:
    rocprofvis_db_data_type_t m_type;
    bool                      m_has_int;
    bool                      m_has_double;
    int64_t                   m_int;
    double                    m_double;
    std::string               m_text;
};

/** @brief One metadata row, addressed by its id. */
class CatalogRecord
{
public:
    explicit CatalogRecord(uint64_t id);

    uint64_t            Id() const;
    uint32_t            NumCells() const;
    const char*         CellText(uint32_t column_index) const;
    const CatalogValue* Cell(uint32_t column_index) const;

    void SetCell(uint32_t column_index, const CatalogValue& value);

private:
    uint64_t                  m_id;
    std::vector<CatalogValue> m_cells;
};

/** @brief One named metadata table. Row pointers stay valid until another row
 *         is added to this table. */
class CatalogTable
{
public:
    explicit CatalogTable(const std::string& name);

    const char* Name() const;
    uint32_t    NumColumns() const;
    size_t      NumRows() const;

    uint32_t AddColumn(const std::string& name, rocprofvis_db_data_type_t type);
    void     AddRow(uint64_t id);
    void     SetCellText(uint32_t column_index, uint64_t row_id, const std::string& text);
    void     SetCell(uint64_t row_id, const std::string& column_name,
                     const CatalogValue& value);

    const char* CellText(uint64_t row_id, const std::string& column_name) const;
    const char* CellTextByIndex(uint64_t row_index, const std::string& column_name) const;
    const char* CellTextAt(uint64_t row_index, uint32_t column_index) const;
    /** @brief First row whose @p column_name text equals @p value.
     *         @p row_id receives that row's "id" column, not the catalog
     *         row key. False when nothing matches; @p row_id is unchanged. */
    bool        FindRowId(const std::string& column_name, const std::string& value,
                          uint64_t& row_id) const;
    const char* ColumnName(uint32_t column_index) const;
    rocprofvis_db_data_type_t ColumnType(uint32_t column_index) const;
    CatalogRecord*            RowAt(uint64_t row_index);
    const CatalogRecord*      Find(uint64_t row_id) const;

private:
    CatalogRecord& MutableRow(uint64_t row_id);

    std::string                                                    m_name;
    std::vector<std::pair<std::string, rocprofvis_db_data_type_t>> m_columns;
    std::vector<CatalogRecord>                                     m_rows;
    std::map<std::string, uint32_t>                                m_column_index;
    std::map<uint64_t, uint32_t>                                   m_row_index;
};

/** @brief Stream-to-hardware relationship copied from profiler-hub. Optional
 *         ids use the is_valid flags; zero can be a real id. */
struct stream_hardware_relation_t
{
    uint32_t category;
    uint32_t nid;
    uint32_t pid;
    uint64_t stream_id;
    uint64_t agent_id;
    uint64_t queue_id;
    uint64_t stream_track_id;
    uint64_t hardware_track_id;
    uint8_t  agent_is_valid;
    uint8_t  queue_is_valid;
    uint8_t  stream_track_is_valid;
    uint8_t  hardware_track_is_valid;
};

/**
 * @brief Per-database metadata catalog. Adapters write it while a trace is
 *        opening. Readers use Find and the table views; a missing table is not
 *        created by a read.
 */
class InformationCatalog
{
public:
    InformationCatalog();

    void SetNull(const char* table_name, uint64_t row_id, const char* column_name);
    void SetInt(const char* table_name, uint64_t row_id, const char* column_name,
                int64_t value);
    void SetDouble(const char* table_name, uint64_t row_id, const char* column_name,
                   double value);
    void SetText(const char* table_name, uint64_t row_id, const char* column_name,
                 const char* value);

    void AddColumn(const char* table_name, const std::string& column_name,
                   rocprofvis_db_data_type_t type);
    void AddRow(const char* table_name, uint64_t row_id);
    void SetCellText(const char* table_name, uint64_t row_id, uint32_t column_index,
                     const std::string& text);
    void SetNamedCellText(const char* table_name, uint64_t row_id,
                          const std::string& column_name, rocprofvis_db_data_type_t type,
                          const std::string& text);

    const char* CellText(const char* table_name, uint64_t row_id,
                         const std::string& column_name) const;
    const char* CellTextByIndex(const char* table_name, uint32_t row_index,
                                const std::string& column_name) const;
    /** @brief Reverse of CellText. @p row_id is the matching row's "id"
     *         column. Does not create a missing table. */
    bool                 FindRowId(const char* table_name, const std::string& column_name,
                                   const std::string& value, uint64_t& row_id) const;
    const CatalogRecord* Find(const char* table_name, uint64_t row_id) const;
    CatalogTable*        Table(const char* table_name);
    const CatalogTable*  FindTable(const char* table_name) const;

    void   AddStreamHardwareRelation(const stream_hardware_relation_t& relation);
    size_t NumStreamHardwareRelations() const;
    const stream_hardware_relation_t* StreamHardwareRelationAt(size_t index) const;

    void Freeze();
    bool IsFrozen() const;

private:
    CatalogTable& MutableTable(const char* table_name);

    std::map<std::string, CatalogTable>     m_tables;
    std::vector<stream_hardware_relation_t> m_stream_hardware;
    bool                                    m_frozen;
    mutable std::shared_mutex               m_mutex;
};

}  // namespace DataModel
}  // namespace RocProfVis
