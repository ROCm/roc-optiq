// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "imgui.h"
#include "rocprofvis_data_provider.h"
#include "rocprofvis_event_manager.h"
#include "widgets/rocprofvis_widget.h"
#include <array>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace RocProfVis
{
namespace View
{

class SettingsManager;
class TableDataEvent;
class TimelineSelection;

class InfiniteScrollTable : public RocWidget
{
public:
    InfiniteScrollTable(DataProvider& dp, TableType table_type,
                        rocprofvis_controller_table_type_t        request_table_type,
                        uint64_t                                  request_id,
                        const std::function<const TablesModel&()> table_model,
                        const std::function<TablesModel&()>       table_model_mutable,
                        std::shared_ptr<TimelineSelection> timeline_selection = nullptr,
                        uint64_t                           default_sort_column_index = 1,
                        rocprofvis_controller_sort_order_t default_sort_order =
                            kRPVControllerSortOrderAscending,
                        const std::string& friendly_name = "",
                        const std::string& no_data_text  = "");
    virtual ~InfiniteScrollTable();

    virtual void Update() override;
    virtual void Render() override;

    void HandleNewTableData(std::shared_ptr<RocEvent> e);

    // Important columns in the table
    enum ImportantColumns
    {
        kUUId,
        kDbEventId,
        kCategory,
        kName,
        kTrackId,
        kStreamId,
        kNumImportantColumns
    };

    friend struct EventSearchTestPeer;

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
        std::string group_by;
        std::string group_columns;
        std::string filter;
    };

    virtual void UpdateFetchParams(std::shared_ptr<TableRequestParams>& params) const;
    virtual void FormatData() const;
    virtual void IndexColumns();
    virtual void RowSelected(const ImGuiMouseButton mouse_button);

    uint64_t                      SelectedRowToTrackID(size_t track_id_column_index,
                                                       size_t stream_id_column_index) const;
    std::pair<uint64_t, uint64_t> SelectedRowToTimeRange() const;
    void                          SelectedRowToClipboard() const;
    void                          SelectedCellToClipboard(bool use_formatted_data) const;
    void                          SelectedRowNavigateEvent(size_t track_id_column_index,
                                                           size_t stream_id_column_index) const;
    void                          SelectedRowContextMenu();

    // Signal for data fetch, caller should prepare to recieve UpdateFetchParams. 
    void RequestFetch();
    void RequestFilter();

    void FormatTimeColumns() const;
    void ExportToFile() const;

    // Filter row...
    const std::string& ActiveFilterRowClause() const;
    void               DisplayFilterRow(bool display);
    void               ResetFilterRow();

    FilterOptions                      m_filter_options;
    uint64_t                           m_sort_column_index;
    uint64_t                           m_default_sort_column_index;
    rocprofvis_controller_sort_order_t m_sort_order;
    rocprofvis_controller_sort_order_t m_default_sort_order;
    std::vector<size_t>                m_important_column_idxs;

    std::vector<int> m_hidden_column_indices;  // This must be sorted in increasing order.
    std::array<size_t, kNumTimeColumns> m_time_column_indices;

    TableType m_table_type;  // Type of table (e.g., EventTable, SampleTable)
    rocprofvis_controller_table_type_t  m_request_table_type;
    uint64_t                            m_request_id;
    std::function<const TablesModel&()> m_table_model;
    std::function<TablesModel&()>       m_table_model_mutable;
    DataProvider&                       m_data_provider;
    SettingsManager&                    m_settings;
    std::shared_ptr<TimelineSelection>  m_timeline_selection;

    uint64_t m_fetch_chunk_size;

    bool m_data_changed;

    // Track the selected row for context menu actions
    int m_selected_row;
    int m_selected_column;
    int m_hovered_row;

    bool m_horizontal_scroll;

private:
    struct FilterInput
    {
        std::string                            column_name;
        rocprofvis_controller_primitive_type_t column_type;
        std::string                            input;
        bool                                   is_time;  // Value is a timestamp (ns).
    };

    void FetchData();
    void RenderCell(const std::string* cell_text, int row, int column);
    void RenderContextMenu();
    void ProcessSortOrFilterRequest(uint64_t frame_count);

    // Hover tooltip explaining how to filter a column, with an example.
    void RenderFilterHelpTooltip(const FilterInput& input) const;

    // Sizes non-user-resized columns to their widest loaded value, clamped to
    // [MIN_COLUMN_WIDTH_EM, MAX_COLUMN_FIT_WIDTH_EM]. With grow_only, columns only
    // grow (scroll page-in); otherwise they fit fresh. Call while the table is active.
    void FitColumnsToContent(bool grow_only);

    // Flags columns the user manually resized so FitColumnsToContent skips them.
    void DetectUserColumnResizes();

    int      m_fetch_pad_items;
    int      m_fetch_threshold_items;
    uint64_t m_fetch_start_row;
    bool     m_fetch_data;
    bool     m_fetch_cancelled;

    bool m_filter_requested;

    // Internal state flags below
    bool     m_open_context_menu;
    bool     m_skip_data_fetch;
    bool     m_refit_pending;    // Fresh content-change data is loaded; re-fit columns.
    bool     m_refit_requested;  // A content change (filter/selection) is in flight.
    bool     m_grow_pending;     // A scroll page loaded; grow columns to fit it.
    bool     m_columns_emptied;  // Table went empty; the next fit forgets manual sizes.
    // Keyed by column name so adding/removing columns never disturbs existing sizing.
    std::unordered_set<std::string>        m_user_sized_columns;  // User-resized columns.
    std::unordered_map<std::string, float> m_column_fit_widths;   // Last auto-fit width.
    uint64_t m_last_total_row_count;
    ImVec2   m_last_table_size;

    // Filter row...
    bool                                         m_display_filter_row;
    bool                                         m_reset_filter_row;
    bool                                         m_update_filter_row;
    std::vector<FilterInput*>                    m_displayed_filter_row_inputs;
    std::string                                  m_active_filter_row_clause;
    std::unordered_set<FilterInput*>             m_active_filter_row_inputs;
    std::unordered_map<std::string, FilterInput> m_filter_row_inputs;

    std::string m_no_data_text;
    std::string m_export_notification_id;

    EventManager::SubscriptionToken m_new_table_data_token;
    EventManager::SubscriptionToken m_format_changed_token;
    EventManager::SubscriptionToken m_request_progress_update_token;
};

}  // namespace View
}  // namespace RocProfVis
