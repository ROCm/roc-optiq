// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include "imgui.h"
#include "rocprofvis_data_provider.h"
#include "widgets/rocprofvis_widget.h"
#include <string>
#include <vector>

namespace RocProfVis
{
namespace View
{

constexpr const char* ROWCONTEXTMENU_POPUP_NAME = "RowContextMenu";

class SettingsManager;
class TableDataEvent;

class InfiniteScrollTable : public RocWidget
{
public:
    InfiniteScrollTable(DataProvider& dp, TableType table_type = TableType::kEventTable);
    virtual ~InfiniteScrollTable() {};

    virtual void Update() override;
    virtual void Render() override;
    void SetTableType(TableType type) { m_table_type = type; }

    void HandleNewTableData(std::shared_ptr<TableDataEvent> e);

protected:
    struct FilterOptions
    {
        int  column_index;
        char group_columns[256];
        char filter[256];
    };

    virtual void FormatData();
    virtual void FetchData(const TableRequestParams& params) const = 0;
    virtual void RenderContextMenu() const;

    std::vector<std::string> m_column_names;
    std::vector<const char*> m_column_names_ptr;
    FilterOptions            m_filter_options;
    FilterOptions            m_pending_filter_options;

    TableType m_table_type;  // Type of table (e.g., EventTable, SampleTable)
    rocprofvis_controller_table_type_t m_req_table_type;

    DataProvider&    m_data_provider;
    SettingsManager& m_settings;

    uint64_t m_fetch_chunk_size;

    bool m_data_changed;

    // Track the selected row for context menu actions
    int m_selected_row = -1;

private:
    void RenderLoadingIndicator() const;
    bool XButton(const char* id) const;

    int m_fetch_pad_items;
    int m_fetch_threshold_items;

    // Internal state flags below
    bool     m_skip_data_fetch;
    uint64_t m_last_total_row_count;
    ImVec2   m_last_table_size;
};

}  // namespace View
}  // namespace RocProfVis
