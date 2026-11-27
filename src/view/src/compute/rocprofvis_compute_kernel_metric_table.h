// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once
#include "model/compute/rocprofvis_compute_model_types.h"
#include "rocprofvis_presets.h"
#include "widgets/rocprofvis_gui_helpers.h"
#include "widgets/rocprofvis_query_builder.h"
#include "widgets/rocprofvis_widget.h"
#include <functional>
#include <memory>
#include <set>
#include <unordered_map>

namespace RocProfVis
{
namespace View
{

class DataProvider;
class ComputeSelection;

class KernelMetricTable : public RocWidget
{
public:
    KernelMetricTable(DataProvider&                     data_provider,
                      std::shared_ptr<ComputeSelection> compute_selection);
    void Update() override;
    void Render() override;

    void ClearData();
    // The selection's workload (metric picking follows it). The table lists every
    // workload's kernels, so it fetches only when it has nothing yet.
    void FetchData(uint32_t workload_id);
    // One workload's rows arrived, or failed. `error_message` (e.g. a filter
    // that could not be applied) may come with either and is shown once per
    // query. The last one shows them all, or keeps the previous rows when every
    // workload failed.
    void HandleNewData(bool success, const std::string& error_message);
    void SetQuery(const std::string& query);
    void SetExternalQuery(MetricId metric_id, const std::string& value_name);
    // Stretch the card and table to the parent's height (a resizable pane). The
    // pane's splitter then replaces the collapse toggle, which is hidden.
    void SetFillParent(bool fill) { m_fill_parent = fill; }
    // Skip the card and title, for a container that draws its own pane header.
    void SetChromeless(bool chromeless) { m_chromeless = chromeless; }
    // Adds "Compare with this kernel (B)" to a row's context menu.
    void SetCompareCallback(
        std::function<void(uint32_t workload_id, uint32_t kernel_id)> callback);
    // Tints the compare target's row; invalid ids for none.
    void SetMarkedKernel(uint32_t workload_id, uint32_t kernel_id);

    size_t      GetActiveFilterCount() const;
    void        ClearAllFilters();

    friend struct KernelMetricTableTestPeer;

private:
    // Rows come per workload (the pivot query takes one): each response is
    // collected, and the last one merges and sorts them all.
    void StartFetchCycle();
    // Requests the rows of the cycle's workload at m_fetch_index, skipping those
    // no row of which could pass the filters; ends the cycle after the last.
    void RequestWorkloadRows();
    // The query for `workload`. Picked metrics are matched by name, as ids
    // differ between GPU architectures, and ones it lacks are left out (see
    // m_fetch_metric_present); the filter and sort columns follow. Returns
    // false when a filter is on a metric it lacks, so none of its kernels match.
    bool BuildWorkloadQuery(const WorkloadInfo& workload, std::vector<std::string>& metrics,
                            std::unordered_map<uint64_t, std::string>& filters,
                            int& sort_column);
    void EndFetchCycle();
    void FinishFetchCycle();
    // A Workload column (in place of the hidden id) when there are several.
    bool ShowsWorkloadColumn() const;
    // Empty for an unknown workload.
    const std::string& WorkloadName(uint32_t workload_id) const;
    void RenderToolbar();
    // The table card's body: the table, or why there is none.
    void RenderTable();
    void SetupTableColumns(int column_count, float outer_width);
    // Returns the metric whose remove button was clicked, -1 for none.
    int  RenderHeaderRow(int column_count);
    void RenderFilterRow(int column_count);
    // Takes a sort clicked in the header; a change refetches.
    void UpdateSortFromTable();
    // Returns whether a cell was right-clicked (see m_cell_menu).
    bool RenderRows(bool request_pending, float line_height);
    void RenderRowSelectable(int row, const std::string& cell, bool is_selected,
                             bool is_marked);
    void RenderValueCell(int col, const std::string& cell);
    void RenderCellContextMenu();
    void RemoveMetricColumn(int remove_index);
    void RenderColumnFilter(int column_index);
    void ApplyFilters();
    void ComputeColumnMaxValues(const std::vector<std::vector<std::string>>& data);
    void RenderBarChartContextMenu(int col);
    // Ignores (with a warning) a metric already in the table.
    void AppendMetricQuery(const std::string& query, const AvailableMetrics::Entry& entry,
                           const std::string& value_name);
    // Whether the metric is a column already. Ids differ between GPU
    // architectures, so a known entry matches by name; one typed in by its query.
    bool HasMetric(const std::string& query, const AvailableMetrics::Entry& entry,
                   const std::string& value_name) const;

    class Preset : public PresetComponent
    {
    public:
        Preset(KernelMetricTable& widget);

        bool ToJson(jt::Json& json) override;
        bool FromJson(jt::Json& json) override;
        void Reset() override;

    private:
        KernelMetricTable& m_widget;
    };

    struct MetricInfo
    {
        AvailableMetrics::Entry entry;
        std::string             value_name;
    };

    // Filter configuration per column
    struct ColumnFilter
    {
        char filter_text[256];  // User input expression
        bool is_active;         // Has content

        ColumnFilter() : filter_text{0}, is_active(false) {}
        bool IsSet() const { return is_active && filter_text[0] != '\0'; }
    };

    std::vector<MetricInfo>  m_metrics_info;
    std::vector<std::string> m_metrics_params;

    std::vector<std::string> m_metrics_column_names;
    std::vector<std::string> m_permanent_column_names;

    DataProvider& m_data_provider;
    QueryBuilder  m_query_builder;

    int  m_sort_column_index;
    int  m_sort_order;

    int  m_selected_row;

    bool m_fetch_requested;
    uint32_t m_workload_id;

    // Shown rows, every workload's, with each row's workload.
    std::vector<std::string>              m_header;
    std::vector<std::vector<std::string>> m_rows;
    std::vector<uint32_t>                 m_row_workloads;
    // The fetch cycle in progress.
    std::vector<uint32_t>                 m_fetch_workloads;
    size_t                                m_fetch_index     = 0;
    size_t                                m_fetch_failures  = 0;
    size_t                                m_fetch_successes = 0;
    // Per picked metric, whether the request in flight queries it; its rows
    // lack the others' columns, which are filled in empty (N/A).
    std::vector<bool>                     m_fetch_metric_present;
    bool                                  m_fetch_error_shown = false;
    bool                                  m_cycle_active   = false;
    bool                                  m_fetch_next   = false;
    // A request has finished but its TableDataEvent is still queued. A new
    // cycle waits for it, or it would be taken as the new cycle's first rows.
    bool                                  m_response_outstanding = false;
    std::vector<std::string>              m_pending_header;
    std::vector<std::vector<std::string>> m_pending_rows;
    std::vector<uint32_t>                 m_pending_row_workloads;
    uint32_t                              m_selected_workload_id_local;
    uint32_t                              m_marked_workload_id;
    uint32_t                              m_marked_kernel_id;
    bool                                  m_scroll_to_selected = false;

    // used for selecting kernel
    std::shared_ptr<ComputeSelection> m_compute_selection;
    uint32_t                          m_selected_kernel_id_local;
    bool                              m_update_table_selection;
    bool                              m_allow_deselect;
    std::function<void(uint32_t workload_id, uint32_t kernel_id)> m_compare_callback;

    bool m_show_kernel_table;
    bool m_fill_parent = false;
    bool m_chromeless  = false;

    // Filter storage - vector aligned with column indices
    // Vector size = PERMANENT_COLUMN_COUNT + m_metrics_params.size()
    // Index 0-3: permanent columns (ID, Name, Duration, Invocations)
    // Index 4+: metric columns
    std::vector<ColumnFilter> m_column_filters;          // Active filters
    std::vector<ColumnFilter> m_pending_column_filters;  // User editing

    std::set<int>                   m_bar_chart_columns;
    std::unordered_map<int, double> m_column_max_values;

    // Row/column whose right-click opened the copy context menu.
    CellMenuTarget m_cell_menu;

    std::unique_ptr<Preset> m_preset;
};

}  // namespace View
}  // namespace RocProfVis
