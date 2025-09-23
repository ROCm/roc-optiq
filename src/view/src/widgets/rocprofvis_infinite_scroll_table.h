// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include "imgui.h"
#include "rocprofvis_data_provider.h"
#include "rocprofvis_events.h"
#include "widgets/rocprofvis_widget.h"
#include <string>
#include <vector>

namespace RocProfVis
{
namespace View
{

class SettingsManager;
class TimelineSelection;

class InfiniteScrollTable : public RocWidget
{
public:
    InfiniteScrollTable(DataProvider&                      dp,
                        std::shared_ptr<TimelineSelection> timeline_selection,
                        TableType table_type = TableType::kEventTable);

    void Update() override;
    void Render() override;
    void SetTableType(TableType type) { m_table_type = type; }

    void HandleTrackSelectionChanged();
    void HandleNewTableData(std::shared_ptr<TableDataEvent> e);

private:
    // Important columns in the table
    enum ImportantColumns
    {
        kId,
        kName,
        kTimeStartNs,
        kTimeEndNs,
        kDurationNs,
        kTrackId,
        kStreamId,
        kNumImportantColumns
    };

    struct FilterOptions
    {
        int  column_index;
        char group_columns[256];
        char filter[256];
    };
    void RenderContextMenu() const;
    void RenderLoadingIndicator() const;
    bool XButton(const char* id) const;

    uint64_t GetTrackIdHelper(
        const std::vector<std::vector<std::string>>& table_data) const;

    std::vector<std::string> m_column_names;
    std::vector<const char*> m_column_names_ptr;
    FilterOptions            m_filter_options;
    FilterOptions            m_pending_filter_options;

    TableType m_table_type;  // Type of table (e.g., EventTable, SampleTable)
    rocprofvis_controller_table_type_t m_req_table_type;

    DataProvider&                      m_data_provider;
    SettingsManager&                   m_settings;
    std::shared_ptr<TimelineSelection> m_timeline_selection;

    int m_fetch_chunk_size;
    int m_fetch_pad_items;
    int m_fetch_threshold_items;

    // Internal state flags below
    bool     m_skip_data_fetch;
    uint64_t m_last_total_row_count;
    bool     m_data_changed;
    bool     m_defer_track_selection_changed;

    // Track the selected row for context menu actions
    int    m_selected_row = -1;
    ImVec2 m_last_table_size;

    // Keep track of currently selected tracks for this table type
    std::vector<uint64_t> m_selected_tracks;

    std::vector<size_t> m_important_column_idxs;
};

}  // namespace View
}  // namespace RocProfVis
