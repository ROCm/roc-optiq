// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once
#include "rocprofvis_event_manager.h"
#include "widgets/rocprofvis_widget.h"
#include "widgets/rocprofvis_charts.h"

namespace RocProfVis
{
namespace View
{

class DataProvider;
class ComputeSelection;
class Roofline;

struct WorkloadInfo;

class ComputeSummaryView : public RocWidget
{
public:
    ComputeSummaryView(DataProvider&                     data_provider,
                       std::shared_ptr<ComputeSelection> compute_selection);
    ~ComputeSummaryView();

    void Update() override;
    void Render() override;

private:
    void CalculateCombosWidth();
    void RenderKernelsTable(const WorkloadInfo& workload);
    void UpdataChartData(const WorkloadInfo& workload, uint8_t metric_id);
    void UpdateKernelMetrics();

    DataProvider& m_data_provider;

    std::shared_ptr<ComputeSelection> m_compute_selection;
    std::unique_ptr<Roofline>         m_roofline;
    PieChart                          m_pie_chart;
    BarChart                          m_bar_chart;

    const std::vector<std::string_view> m_chart_views = { "Pie Chart", "Bar Chart" };
    std::vector<std::string_view>       m_metric_views;
    uint32_t                            m_workload_id;
    float                               m_chart_combo_width;
    float                               m_metric_combo_width;
    uint8_t                             m_selected_metric;
    uint8_t                             m_selected_chart_id;

    uint64_t m_client_id;

    EventManager::SubscriptionToken m_workload_selection_changed_token;
    EventManager::SubscriptionToken m_metrics_fetched_token;
    EventManager::SubscriptionToken m_font_changed_token;
};

}  // namespace View
}  // namespace RocProfVis
