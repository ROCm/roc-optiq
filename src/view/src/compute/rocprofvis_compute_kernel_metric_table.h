// Copyright (c) 2024 Advanced Micro Devices, Inc. All rights reserved.
// SPDX-License-Identifier: MIT

#pragma once
#include "widgets/rocprofvis_widget.h"
#include "widgets/rocprofvis_query_builder.h"
#include "model/compute/rocprofvis_compute_model_types.h"

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
    void FetchData(uint32_t workload_id);
    void HandleNewData();
    void SetQuery(const std::string& query);
    void SetExternalQuery(MetricId metric_id, const std::string& value_name);

private:
    void RenderColumnFilter(int column_index);
    void ApplyFilters();
    void ClearAllFilters();
    bool ValidateFilterExpression(const char* expr, bool is_numeric_column);
    void ComputeColumnMaxValues(const std::vector<std::vector<std::string>>& data);
    void RenderBarChartContextMenu(int col);
    void AppendMetricQuery(const std::string& query, const AvailableMetrics::Entry& entry,
                           const std::string& value_name);

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
    bool m_sort_specs_initialized;

    int  m_selected_row;

    bool m_fetch_requested;
    uint32_t m_workload_id;

    // used for selecting kernel
    std::shared_ptr<ComputeSelection> m_compute_selection;
    uint32_t                          m_selected_kernel_id_local;
    bool                              m_update_table_selection;
    bool                              m_allow_deselect;
    
    bool m_show_kernel_table;

    // Filter storage - vector aligned with column indices
    // Vector size = PERMANENT_COLUMN_COUNT + m_metrics_params.size()
    // Index 0-3: permanent columns (ID, Name, Duration, Invocations)
    // Index 4+: metric columns
    std::vector<ColumnFilter> m_column_filters;        // Active filters
    std::vector<ColumnFilter> m_pending_column_filters; // User editing

    std::set<int>                     m_bar_chart_columns;
    std::unordered_map<int, double>   m_column_max_values;
};

}  // namespace View
}  // namespace RocProfVis
