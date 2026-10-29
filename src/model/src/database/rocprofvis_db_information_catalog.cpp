// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_db_information_catalog.h"

#include <charconv>
#include <system_error>
#include <utility>

namespace RocProfVis
{
namespace DataModel
{

static bool
parse_int(const std::string& text, int64_t& value)
{
    if(text.empty())
    {
        return false;
    }
    const char*                  begin  = text.data();
    const char*                  end    = begin + text.size();
    int64_t                      parsed = 0;
    const std::from_chars_result result = std::from_chars(begin, end, parsed);
    if(result.ec != std::errc() || result.ptr != end)
    {
        return false;
    }
    value = parsed;
    return true;
}

static bool
parse_double(const std::string& text, double& value)
{
    if(text.empty())
    {
        return false;
    }
    const char*                  begin  = text.data();
    const char*                  end    = begin + text.size();
    double                       parsed = 0.0;
    const std::from_chars_result result = std::from_chars(begin, end, parsed);
    if(result.ec != std::errc() || result.ptr != end)
    {
        return false;
    }
    value = parsed;
    return true;
}

CatalogValue::CatalogValue()
: m_type(kRPVDataTypeNull)
, m_has_int(false)
, m_has_double(false)
, m_int(0)
, m_double(0.0)
{}

rocprofvis_db_data_type_t
CatalogValue::Type() const
{
    return m_type;
}

bool
CatalogValue::HasInt() const
{
    return m_has_int;
}

bool
CatalogValue::HasDouble() const
{
    return m_has_double;
}

int64_t
CatalogValue::Int() const
{
    return m_int;
}

double
CatalogValue::Double() const
{
    return m_double;
}

const char*
CatalogValue::Text() const
{
    return m_text.c_str();
}

void
CatalogValue::SetNull()
{
    m_type       = kRPVDataTypeNull;
    m_has_int    = false;
    m_has_double = false;
    m_int        = 0;
    m_double     = 0.0;
    m_text.clear();
}

void
CatalogValue::SetInt(int64_t value)
{
    m_type       = kRPVDataTypeInt;
    m_has_int    = true;
    m_has_double = false;
    m_int        = value;
    m_double     = 0.0;
    m_text       = std::to_string(value);
}

void
CatalogValue::SetDouble(double value)
{
    m_type       = kRPVDataTypeDouble;
    m_has_int    = false;
    m_has_double = true;
    m_int        = 0;
    m_double     = value;
    m_text       = std::to_string(value);
}

void
CatalogValue::SetText(rocprofvis_db_data_type_t type, const std::string& text)
{
    m_type       = type;
    m_text       = text;
    m_has_int    = false;
    m_has_double = false;
    m_int        = 0;
    m_double     = 0.0;
    if(type == kRPVDataTypeInt)
    {
        m_has_int = parse_int(text, m_int);
    }
    else if(type == kRPVDataTypeDouble)
    {
        m_has_double = parse_double(text, m_double);
    }
}

CatalogRecord::CatalogRecord(uint64_t id)
: m_id(id)
{}

uint64_t
CatalogRecord::Id() const
{
    return m_id;
}

uint32_t
CatalogRecord::NumCells() const
{
    return static_cast<uint32_t>(m_cells.size());
}

const char*
CatalogRecord::CellText(uint32_t column_index) const
{
    if(column_index >= m_cells.size())
    {
        return "";
    }
    return m_cells[column_index].Text();
}

const CatalogValue*
CatalogRecord::Cell(uint32_t column_index) const
{
    if(column_index >= m_cells.size())
    {
        return nullptr;
    }
    return &m_cells[column_index];
}

void
CatalogRecord::SetCell(uint32_t column_index, const CatalogValue& value)
{
    if(column_index >= m_cells.size())
    {
        m_cells.resize(static_cast<size_t>(column_index) + 1U);
    }
    m_cells[column_index] = value;
}

CatalogTable::CatalogTable(const std::string& name)
: m_name(name)
{}

const char*
CatalogTable::Name() const
{
    return m_name.c_str();
}

uint32_t
CatalogTable::NumColumns() const
{
    return static_cast<uint32_t>(m_columns.size());
}

size_t
CatalogTable::NumRows() const
{
    return m_rows.size();
}

uint32_t
CatalogTable::AddColumn(const std::string& name, rocprofvis_db_data_type_t type)
{
    const std::map<std::string, uint32_t>::iterator it = m_column_index.find(name);
    if(it != m_column_index.end())
    {
        return it->second;
    }
    const uint32_t index = static_cast<uint32_t>(m_columns.size());
    m_column_index.emplace(name, index);
    m_columns.push_back(std::make_pair(name, type));
    return index;
}

void
CatalogTable::AddRow(uint64_t id)
{
    MutableRow(id);
}

CatalogRecord&
CatalogTable::MutableRow(uint64_t row_id)
{
    const std::map<uint64_t, uint32_t>::iterator it = m_row_index.find(row_id);
    if(it != m_row_index.end())
    {
        return m_rows[it->second];
    }
    const uint32_t index = static_cast<uint32_t>(m_rows.size());
    m_row_index.emplace(row_id, index);
    m_rows.push_back(CatalogRecord(row_id));
    return m_rows.back();
}

void
CatalogTable::SetCellText(uint32_t column_index, uint64_t row_id, const std::string& text)
{
    CatalogValue              value;
    rocprofvis_db_data_type_t type = kRPVDataTypeString;
    if(column_index < m_columns.size())
    {
        type = m_columns[column_index].second;
    }
    value.SetText(type, text);
    MutableRow(row_id).SetCell(column_index, value);
}

void
CatalogTable::SetCell(uint64_t row_id, const std::string& column_name,
                      const CatalogValue& value)
{
    const uint32_t column_index = AddColumn(column_name, value.Type());
    MutableRow(row_id).SetCell(column_index, value);
}

const char*
CatalogTable::CellText(uint64_t row_id, const std::string& column_name) const
{
    const std::map<std::string, uint32_t>::const_iterator column =
        m_column_index.find(column_name);
    const std::map<uint64_t, uint32_t>::const_iterator row = m_row_index.find(row_id);
    if(column == m_column_index.end() || row == m_row_index.end())
    {
        return "";
    }
    return m_rows[row->second].CellText(column->second);
}

const char*
CatalogTable::CellTextByIndex(uint64_t row_index, const std::string& column_name) const
{
    const std::map<std::string, uint32_t>::const_iterator column =
        m_column_index.find(column_name);
    if(row_index >= m_rows.size() || column == m_column_index.end())
    {
        return "";
    }
    return m_rows[static_cast<size_t>(row_index)].CellText(column->second);
}

const char*
CatalogTable::CellTextAt(uint64_t row_index, uint32_t column_index) const
{
    if(row_index >= m_rows.size())
    {
        return "";
    }
    return m_rows[static_cast<size_t>(row_index)].CellText(column_index);
}

bool
CatalogTable::FindRowId(const std::string& column_name, const std::string& value,
                        uint64_t& row_id) const
{
    const std::map<std::string, uint32_t>::const_iterator column =
        m_column_index.find(column_name);
    if(column == m_column_index.end())
    {
        return false;
    }
    const std::map<std::string, uint32_t>::const_iterator id_column =
        m_column_index.find("id");
    if(id_column == m_column_index.end())
    {
        return false;
    }
    const uint32_t column_index = column->second;
    const uint32_t id_index     = id_column->second;
    for(size_t row_index = 0; row_index < m_rows.size(); ++row_index)
    {
        const CatalogValue* cell = m_rows[row_index].Cell(column_index);
        if(cell == nullptr || value != cell->Text())
        {
            continue;
        }
        const CatalogValue* id_cell = m_rows[row_index].Cell(id_index);
        if(id_cell == nullptr)
        {
            continue;
        }
        int64_t parsed = 0;
        if(id_cell->HasInt())
        {
            parsed = id_cell->Int();
        }
        else if(!parse_int(id_cell->Text(), parsed))
        {
            continue;
        }
        if(parsed < 0)
        {
            continue;
        }
        row_id = static_cast<uint64_t>(parsed);
        return true;
    }
    return false;
}

const char*
CatalogTable::ColumnName(uint32_t column_index) const
{
    if(column_index >= m_columns.size())
    {
        return "";
    }
    return m_columns[column_index].first.c_str();
}

rocprofvis_db_data_type_t
CatalogTable::ColumnType(uint32_t column_index) const
{
    if(column_index >= m_columns.size())
    {
        return kRPVDataTypeNull;
    }
    return m_columns[column_index].second;
}

CatalogRecord*
CatalogTable::RowAt(uint64_t row_index)
{
    if(row_index >= m_rows.size())
    {
        return nullptr;
    }
    return &m_rows[static_cast<size_t>(row_index)];
}

const CatalogRecord*
CatalogTable::Find(uint64_t row_id) const
{
    const std::map<uint64_t, uint32_t>::const_iterator row = m_row_index.find(row_id);
    if(row == m_row_index.end())
    {
        return nullptr;
    }
    return &m_rows[row->second];
}

InformationCatalog::InformationCatalog()
: m_frozen(false)
{}

void
InformationCatalog::SetNull(const char* table_name, uint64_t row_id,
                            const char* column_name)
{
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    if(m_frozen || table_name == nullptr || column_name == nullptr)
    {
        return;
    }
    CatalogValue value;
    value.SetNull();
    MutableTable(table_name).SetCell(row_id, column_name, value);
}

void
InformationCatalog::SetInt(const char* table_name, uint64_t row_id,
                           const char* column_name, int64_t value)
{
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    if(m_frozen || table_name == nullptr || column_name == nullptr)
    {
        return;
    }
    CatalogValue cell;
    cell.SetInt(value);
    MutableTable(table_name).SetCell(row_id, column_name, cell);
}

void
InformationCatalog::SetDouble(const char* table_name, uint64_t row_id,
                              const char* column_name, double value)
{
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    if(m_frozen || table_name == nullptr || column_name == nullptr)
    {
        return;
    }
    CatalogValue cell;
    cell.SetDouble(value);
    MutableTable(table_name).SetCell(row_id, column_name, cell);
}

void
InformationCatalog::SetText(const char* table_name, uint64_t row_id,
                            const char* column_name, const char* value)
{
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    if(m_frozen || table_name == nullptr || column_name == nullptr)
    {
        return;
    }
    CatalogValue cell;
    if(value == nullptr)
    {
        cell.SetNull();
    }
    else
    {
        cell.SetText(kRPVDataTypeString, value);
    }
    MutableTable(table_name).SetCell(row_id, column_name, cell);
}

void
InformationCatalog::AddColumn(const char* table_name, const std::string& column_name,
                              rocprofvis_db_data_type_t type)
{
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    if(m_frozen || table_name == nullptr)
    {
        return;
    }
    MutableTable(table_name).AddColumn(column_name, type);
}

void
InformationCatalog::AddRow(const char* table_name, uint64_t row_id)
{
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    if(m_frozen || table_name == nullptr)
    {
        return;
    }
    MutableTable(table_name).AddRow(row_id);
}

void
InformationCatalog::SetCellText(const char* table_name, uint64_t row_id,
                                uint32_t column_index, const std::string& text)
{
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    if(m_frozen || table_name == nullptr)
    {
        return;
    }
    MutableTable(table_name).SetCellText(column_index, row_id, text);
}

void
InformationCatalog::SetNamedCellText(const char* table_name, uint64_t row_id,
                                     const std::string&        column_name,
                                     rocprofvis_db_data_type_t type,
                                     const std::string&        text)
{
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    if(m_frozen || table_name == nullptr)
    {
        return;
    }
    CatalogTable&  table        = MutableTable(table_name);
    const uint32_t column_index = table.AddColumn(column_name, type);
    table.SetCellText(column_index, row_id, text);
}

const char*
InformationCatalog::CellText(const char* table_name, uint64_t row_id,
                             const std::string& column_name) const
{
    std::shared_lock<std::shared_mutex>                       lock(m_mutex);
    const std::map<std::string, CatalogTable>::const_iterator table =
        m_tables.find(table_name == nullptr ? "" : table_name);
    if(table == m_tables.end())
    {
        return "";
    }
    return table->second.CellText(row_id, column_name);
}

bool
InformationCatalog::FindRowId(const char* table_name, const std::string& column_name,
                              const std::string& value, uint64_t& row_id) const
{
    std::shared_lock<std::shared_mutex>                       lock(m_mutex);
    const std::map<std::string, CatalogTable>::const_iterator table =
        m_tables.find(table_name == nullptr ? "" : table_name);
    if(table == m_tables.end())
    {
        return false;
    }
    return table->second.FindRowId(column_name, value, row_id);
}

const char*
InformationCatalog::CellTextByIndex(const char* table_name, uint32_t row_index,
                                    const std::string& column_name) const
{
    std::shared_lock<std::shared_mutex>                       lock(m_mutex);
    const std::map<std::string, CatalogTable>::const_iterator table =
        m_tables.find(table_name == nullptr ? "" : table_name);
    if(table == m_tables.end())
    {
        return "";
    }
    return table->second.CellTextByIndex(row_index, column_name);
}

const CatalogRecord*
InformationCatalog::Find(const char* table_name, uint64_t row_id) const
{
    std::shared_lock<std::shared_mutex>                       lock(m_mutex);
    const std::map<std::string, CatalogTable>::const_iterator table =
        m_tables.find(table_name == nullptr ? "" : table_name);
    if(table == m_tables.end())
    {
        return nullptr;
    }
    return table->second.Find(row_id);
}

CatalogTable*
InformationCatalog::Table(const char* table_name)
{
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    return &MutableTable(table_name == nullptr ? "" : table_name);
}

const CatalogTable*
InformationCatalog::FindTable(const char* table_name) const
{
    std::shared_lock<std::shared_mutex>                       lock(m_mutex);
    const std::map<std::string, CatalogTable>::const_iterator table =
        m_tables.find(table_name == nullptr ? "" : table_name);
    if(table == m_tables.end())
    {
        return nullptr;
    }
    return &table->second;
}

void
InformationCatalog::AddStreamHardwareRelation(const stream_hardware_relation_t& relation)
{
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    if(m_frozen)
    {
        return;
    }
    m_stream_hardware.push_back(relation);
}

size_t
InformationCatalog::NumStreamHardwareRelations() const
{
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    return m_stream_hardware.size();
}

const stream_hardware_relation_t*
InformationCatalog::StreamHardwareRelationAt(size_t index) const
{
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    if(index >= m_stream_hardware.size())
    {
        return nullptr;
    }
    return &m_stream_hardware[index];
}

void
InformationCatalog::Freeze()
{
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    m_frozen = true;
}

bool
InformationCatalog::IsFrozen() const
{
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    return m_frozen;
}

CatalogTable&
InformationCatalog::MutableTable(const char* table_name)
{
    const std::string name = table_name == nullptr ? "" : table_name;
    const std::map<std::string, CatalogTable>::iterator it = m_tables.find(name);
    if(it != m_tables.end())
    {
        return it->second;
    }
    const std::pair<std::map<std::string, CatalogTable>::iterator, bool> inserted =
        m_tables.emplace(name, CatalogTable(name));
    return inserted.first->second;
}

}  // namespace DataModel
}  // namespace RocProfVis
