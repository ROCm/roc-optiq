// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_model_types.h"

#include <vector>

namespace RocProfVis
{
namespace View
{

// Table types enum (moved from DataProvider)
enum class TableType
{
    kSampleTable,
    kEventTable,
    kEventSearchTable,
    kSummaryKernelTable,
    __kTableTypeCount
};

/**
 * @brief Manages tabular data views.
 *
 * This model holds table data generated from queries (events, samples, search results).
 * Tables are typically on-demand views that can be regenerated.
 */
class TablesModel
{
public:
    TablesModel();
    ~TablesModel() = default;

    // Table access
    const TableInfo& GetTable(TableType type) const;
    TableInfo&       GetTable(TableType type);

    const std::vector<std::string>&              GetTableHeader(TableType type) const;
    const std::vector<std::vector<std::string>>& GetTableData(TableType type) const;
    const std::vector<FormattedColumnInfo>& GetFormattedTableData(TableType type) const;
    std::vector<FormattedColumnInfo>&       GetMutableFormattedTableData(TableType type);

    std::shared_ptr<TableRequestParams> GetTableParams(TableType type) const;
    uint64_t                            GetTableTotalRowCount(TableType type) const;

    // Table modification
    void SetTableHeader(TableType type, std::vector<std::string>&& header);
    void SetTableData(TableType type, std::vector<std::vector<std::string>>&& data);
    void SetTableParams(TableType type, std::shared_ptr<TableRequestParams> params);
    void SetTableTotalRowCount(TableType type, uint64_t count);
    void SetFormattedColumnData(TableType type, std::vector<FormattedColumnInfo>&& data);

    void ClearTable(TableType type);
    void ClearAllTables();

private:
    std::vector<TableInfo> m_tables;
};

}  // namespace View
}  // namespace RocProfVis
