// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_tables_model.h"
#include "rocprofvis_requests.h"

#include <sstream>
#include "spdlog/spdlog.h"

namespace RocProfVis
{
namespace View
{

TablesModel::TablesModel()
: m_tables(static_cast<size_t>(TableType::__kTableTypeCount))
{}

const TableInfo&
TablesModel::GetTable(TableType type) const
{
    return m_tables[static_cast<size_t>(type)];
}

TableInfo&
TablesModel::GetTable(TableType type)
{
    return m_tables[static_cast<size_t>(type)];
}

const std::vector<std::string>&
TablesModel::GetTableHeader(TableType type) const
{
    return m_tables[static_cast<size_t>(type)].table_header;
}

const std::vector<std::vector<std::string>>&
TablesModel::GetTableData(TableType type) const
{
    return m_tables[static_cast<size_t>(type)].table_data;
}

const std::vector<FormattedColumnInfo>&
TablesModel::GetFormattedTableData(TableType type) const
{
    return m_tables[static_cast<size_t>(type)].formatted_column_data;
}

std::vector<FormattedColumnInfo>&
TablesModel::GetMutableFormattedTableData(TableType type)
{
    return m_tables[static_cast<size_t>(type)].formatted_column_data;
}

std::shared_ptr<TableRequestParams>
TablesModel::GetTableParams(TableType type) const
{
    return m_tables[static_cast<size_t>(type)].table_params;
}

uint64_t
TablesModel::GetTableTotalRowCount(TableType type) const
{
    return m_tables[static_cast<size_t>(type)].total_row_count;
}

void
TablesModel::SetTableHeader(TableType type, std::vector<std::string>&& header)
{
    m_tables[static_cast<size_t>(type)].table_header = std::move(header);
}

void
TablesModel::SetTableData(TableType type, std::vector<std::vector<std::string>>&& data)
{
    m_tables[static_cast<size_t>(type)].table_data = std::move(data);
}

void
TablesModel::SetTableParams(TableType type, std::shared_ptr<TableRequestParams> params)
{
    m_tables[static_cast<size_t>(type)].table_params = params;
}

void
TablesModel::SetTableTotalRowCount(TableType type, uint64_t count)
{
    m_tables[static_cast<size_t>(type)].total_row_count = count;
}

void
TablesModel::SetFormattedColumnData(TableType                          type,
                                    std::vector<FormattedColumnInfo>&& data)
{
    m_tables[static_cast<size_t>(type)].formatted_column_data = std::move(data);
}

void
TablesModel::ClearTable(TableType type)
{
    auto& table = m_tables[static_cast<size_t>(type)];
    table.table_header.clear();
    table.table_data.clear();
    table.total_row_count = 0;
    table.table_params.reset();
    table.formatted_column_data.clear();
}

void
TablesModel::ClearAllTables()
{
    for(size_t i = 0; i < static_cast<size_t>(TableType::__kTableTypeCount); ++i)
    {
        ClearTable(static_cast<TableType>(i));
    }
}

void
TablesModel::DumpTable(TableType type)
{
    DumpTable(GetTableHeader(type), GetTableData(type));
}

void
TablesModel::DumpTable(const std::vector<std::string>&              header,
                        const std::vector<std::vector<std::string>>& data)
{
    std::ostringstream str_collector;
    for(const auto& col_header : header)
    {
        str_collector << col_header;
        str_collector << " ";
    }
    spdlog::debug("{}", str_collector.str());
    str_collector.str("");
    str_collector.clear();

    // print the event table data
    int skip_count = 0;
    for(const auto& row : data)
    {
        int col_count = 0;
        for(const auto& column : row)
        {
            str_collector << column;
            str_collector << " ";
            col_count++;
        }
        if(col_count == 0)
        {
            skip_count++;
            continue;
        }
        if(skip_count > 0)
        {
            spdlog::debug("Skipped {} empty rows", skip_count);
            skip_count = 0;
        }

        spdlog::debug("{}", str_collector.str());
        str_collector.str("");
        str_collector.clear();
    }
}

}  // namespace View
}  // namespace RocProfVis
