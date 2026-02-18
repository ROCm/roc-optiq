// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_compute_summary_view.h"
#include "implot/implot.h"

constexpr float DROPDOWN_SCALE = 15.0f;

namespace RocProfVis
{
namespace View
{
void
NewComputeSummaryView::RenderSummaryView(const WorkloadInfo& workload)
{
    m_kernel_table_renderer(workload);

    static uint32_t selected_chart_id = 0;
    ImGui::SetNextItemWidth(ImGui::GetFrameHeight() * DROPDOWN_SCALE);
    if(ImGui::BeginCombo("ChartView", m_chart_views[selected_chart_id].data()))
    {
        for(uint32_t index = 0; index < m_chart_views.size(); index++)
        {
            const bool is_selected = (selected_chart_id == index);
            if(ImGui::Selectable(m_chart_views[index].data(), is_selected))
            {
                selected_chart_id = index;
            }

            if(is_selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    ImGui::SameLine();

    static uint32_t selected_metric = 0;
    ImGui::SetNextItemWidth(ImGui::GetFrameHeight() * DROPDOWN_SCALE);
    if(ImGui::BeginCombo("Metric", m_metric_views[selected_metric].data()))
    {
        for(uint32_t index = 0; index < m_metric_views.size(); index++)
        {
            const bool is_selected = (selected_metric == index);
            if(ImGui::Selectable(m_metric_views[index].data(), is_selected))
            {
                selected_metric = index;
            }

            if(is_selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    ImGui::BeginChild("Chart");
    if(selected_chart_id == 0)
        RenderPieChart(workload, GetMetricNameByIndex(selected_metric));
    else if(selected_chart_id == 1)
        RenderBarChart(workload, GetMetricNameByIndex(selected_metric));

    ImGui::EndChild();  // Chart
}

KernelMetric
NewComputeSummaryView::GetMetricNameByIndex(uint32_t index) const
{
    switch (index)
    {
        case 0: return KernelMetric::InvocationCount;
        case 1: return KernelMetric::DurationTotal;
        case 2: return KernelMetric::DurationMin;
        case 3: return KernelMetric::DurationMax;
        case 4: return KernelMetric::DurationMean;
        case 5: return KernelMetric::DurationMedian;
        default:
            assert(false && "Invalid metric index");
            return KernelMetric::InvocationCount;  // Default case, should not happen
    }
}

void
NewComputeSummaryView::RenderPieChart(const WorkloadInfo& workload, KernelMetric metric)
{
    m_pie_chart.UpdateData(
        workload.GenerateChartData(metric));  // TODO: Avoid updating each frames, think
                                              // how to do it if it changes
    m_pie_chart.Render();
}

void
NewComputeSummaryView::RenderBarChart(const WorkloadInfo& workload, KernelMetric metric)
{
    m_bar_chart.UpdateData(workload.GenerateChartData(metric));
    m_bar_chart.Render();
}


} // namespace View
}  // namespace RocProfVis