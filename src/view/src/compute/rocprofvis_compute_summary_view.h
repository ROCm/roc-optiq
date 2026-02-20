// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once
#include "../model/compute/rocprofvis_compute_model_types.h"
#include "../widgets/rocprofvis_charts.h"
#include "rocprofvis_data_provider.h"
#include "rocprofvis_event_manager.h"

namespace RocProfVis
{
namespace View
{

class ComputeSummaryView : public RocWidget
{
public:
    ComputeSummaryView(DataProvider& data_provider);
    ~ComputeSummaryView();
    void Render() override;

private:
    void CalculateCombosWidth();
    void RenderKernelsTable(const WorkloadInfo& workload);
    void UpdataChartData(const WorkloadInfo& workload, uint8_t metric_id);
    void UpdateKernelMetrics();

    const std::vector<std::string_view> m_chart_views = { "Pie Chart", "Bar Chart" };
    std::vector<std::string_view>       m_metric_views;

    PieChart m_pie_chart;
    BarChart m_bar_chart;

    DataProvider&                   m_data_provider;
    uint32_t                        m_workload_id;
    float                           m_chart_combo_width;
    float                           m_metric_combo_width;
    uint8_t                         m_selected_metric;
    uint8_t                         m_selected_chart_id;
    EventManager::SubscriptionToken m_font_changed_token;
};

} // namespace View
} // namespace RocProfVis
