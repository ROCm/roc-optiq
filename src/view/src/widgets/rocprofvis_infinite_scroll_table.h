// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "imgui.h"
#include "rocprofvis_data_provider.h"
#include "rocprofvis_event_manager.h"
#include "widgets/rocprofvis_widget.h"
#include <array>
#include <string>
#include <vector>

namespace RocProfVis
{
namespace View
{
constexpr uint64_t INVALID_UINT64_INDEX = std::numeric_limits<uint64_t>::max();

class SettingsManager;
class TableDataEvent;

class InfiniteScrollTable : public RocWidget
{
public:
    InfiniteScrollTable(DataProvider& dp, TableType table_type,
                        const std::string& no_data_text = "");
    virtual ~InfiniteScrollTable();

    virtual void Update() override;
    virtual void Render() override;

    void HandleNewTableData(std::shared_ptr<RocEvent> e);

    // Important columns in the table
    enum ImportantColumns
    {
        kId,
        kEventId,
        kName,
        kTrackId,
        kStreamId,
        kNumImportantColumns
    };

protected:
    enum TimeColumns
    {
        kTimeStartNs = 0,
        kTimeEndNs,
        kDurationNs,
        kNumTimeColumns
    };
    struct FilterOptions
    {
		char        where[256];
        std::string group_by;
        char        group_columns[256];
        char        filter[256];
    };

    virtual void FormatData() const;
    virtual void IndexColumns();
    virtual void RowSelected(const ImGuiMouseButton mouse_button);

    uint64_t                      GetRequestID() const;
    uint64_t                      SelectedRowToTrackID(size_t track_id_column_index,
                                                       size_t stream_id_column_index) const;
    std::pair<uint64_t, uint64_t> SelectedRowToTimeRange() const;
    void                          SelectedRowToClipboard() const;
    void                          FormatTimeColumns() const;
    void                          SelectedRowNavigateEvent(size_t track_id_column_index,
                                                           size_t stream_id_column_index) const;
    void                          ExportToFile() const;

    FilterOptions            m_filter_options;
    FilterOptions            m_pending_filter_options;
    std::vector<size_t>      m_important_column_idxs;

    std::vector<int> m_hidden_column_indices;  // This must be sorted in increasing order.
    std::array<size_t, kNumTimeColumns> m_time_column_indices;

    TableType m_table_type;  // Type of table (e.g., EventTable, SampleTable)
    rocprofvis_controller_table_type_t m_req_table_type;

    DataProvider&    m_data_provider;
    SettingsManager& m_settings;

    uint64_t m_fetch_chunk_size;

    bool m_data_changed;
    bool m_filter_requested;

    // Track the selected row for context menu actions
    int m_selected_row;
    int m_selected_column;
    int m_hovered_row;

    bool m_horizontal_scroll;

private:
    void RenderLoadingIndicator() const;
    void RenderCell(const std::string* cell_text, int row, int column);
    void RenderFirstColumnCell(const std::string* cell_text, int row);
    void ProcessSortOrFilterRequest(rocprofvis_controller_sort_order_t sort_order,
                            uint64_t sort_column_index, uint64_t frame_count);

    int m_fetch_pad_items;
    int m_fetch_threshold_items;

    // Internal state flags below
    bool     m_skip_data_fetch;
    uint64_t m_last_total_row_count;
    ImVec2   m_last_table_size;

    std::string m_no_data_text;

    EventManager::SubscriptionToken m_new_table_data_token;
    EventManager::SubscriptionToken m_format_changed_token;
};

}  // namespace View
}  // namespace RocProfVis
