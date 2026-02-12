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

    ImGui::BeginChild("Chart");

    if(selected_chart_id == 0)
        RenderPieChart(workload);
    else if(selected_chart_id == 1)
        RenderBarChart(workload);

    ImGui::EndChild();  // Chart
}

void
NewComputeSummaryView::RenderPieChart(const WorkloadInfo& workload)
{
    m_pie_chart.UpdateData(
        workload.GenerateChartData());  // TODO: Avoid updating each frames, think how to
                                        // doit if it changes
    m_pie_chart.Render();
}

void
NewComputeSummaryView::RenderBarChart(const WorkloadInfo& workload)
{
    m_bar_chart.UpdateData(workload.GenerateChartData());
    m_bar_chart.Render();
}


} // namespace View
}  // namespace RocProfVis