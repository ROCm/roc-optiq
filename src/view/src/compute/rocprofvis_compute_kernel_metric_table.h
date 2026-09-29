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
    // One workload's rows arrived (or failed); the last one shows them all.
    void HandleNewData(bool success = true);
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

    // The kernels shown, in the table's order and after its filters, for a
    // kernel list that mirrors the table. Replaced only when a query's rows have
    // all arrived; HasShownKernels is false until the first have.
    struct ShownKernel
    {
        uint32_t workload_id;
        uint32_t kernel_id;
    };
    bool HasShownKernels() const { return !m_header.empty(); }
    const std::vector<ShownKernel>& GetShownKernels() const { return m_shown_kernels; }
    // Longest total duration first, the kernel list's own order.
    bool        IsDefaultSort() const;
    bool        IsSortAscending() const;
    std::string GetSortColumnName() const;
    size_t      GetActiveFilterCount() const;
    void        ClearAllFilters();

    friend struct KernelMetricTableTestPeer;

private:
    // Rows come per workload (the pivot query takes one): each response is
    // collected, and the last one merges and sorts them all.
    void StartFetchCycle();
    void RequestWorkloadRows(uint32_t workload_id);
    void FinishFetchCycle();
    // A Workload column (in place of the hidden id) when there are several.
    bool ShowsWorkloadColumn() const;
    void RenderColumnFilter(int column_index);
    void ApplyFilters();
    bool ValidateFilterExpression(const char* expr, bool is_numeric_column);
    void ComputeColumnMaxValues(const std::vector<std::vector<std::string>>& data);
    void RenderBarChartContextMenu(int col);
    void AppendMetricQuery(const std::string& query, const AvailableMetrics::Entry& entry,
                           const std::string& value_name);

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
    std::vector<ShownKernel>              m_shown_kernels;
    // The fetch cycle in progress.
    std::vector<uint32_t>                 m_fetch_workloads;
    size_t                                m_fetch_index  = 0;
    bool                                  m_cycle_active = false;
    bool                                  m_fetch_next   = false;
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
