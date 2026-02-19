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
ComputeSummaryView::Render()
{
    const std::unordered_map<uint32_t, WorkloadInfo>& workloads =
        m_data_provider.ComputeModel().GetWorkloads();
    ImGui::SetNextItemWidth(ImGui::GetFrameHeight() * DROPDOWN_SCALE);
    if(ImGui::BeginCombo("Workloads",
                         workloads.count(m_selected_workload_id) > 0
                             ? workloads.at(m_selected_workload_id).name.c_str()
                             : "-"))
    {
        if(ImGui::Selectable("-", m_selected_workload_id == 0))
        {
            m_selected_workload_id = 0;
        }
        for(const std::pair<const uint32_t, WorkloadInfo>& workload : workloads)
        {
            if(ImGui::Selectable(workload.second.name.c_str(),
                                 m_selected_workload_id == workload.second.id))
            {
                m_selected_workload_id = workload.second.id;
            }
        }
        ImGui::EndCombo();
    }

    if(workloads.count(m_selected_workload_id) > 0)
    {
        const WorkloadInfo& workload = workloads.at(m_selected_workload_id);
        RenderKernelsTable(workload);

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
}

void
ComputeSummaryView::RenderKernelsTable(const WorkloadInfo& workload)
{
    ImGui::BeginChild("kernels", ImVec2(0, 0),
                        ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);
    ImGui::Text("Kernels");
    if(ImGui::BeginTable("", 8,
                            ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders |
                                ImGuiTableFlags_SizingStretchSame))
    {
        ImGui::TableSetupColumn("ID");
        ImGui::TableSetupColumn("Name");
        ImGui::TableSetupColumn("Invocation Count");
        ImGui::TableSetupColumn("Total Duration");
        ImGui::TableSetupColumn("Min Duration");
        ImGui::TableSetupColumn("Max Duration");
        ImGui::TableSetupColumn("Mean Duration");
        ImGui::TableSetupColumn("Median Duration");
        ImGui::TableHeadersRow();
        for(const std::pair<const uint32_t, KernelInfo>& kernel : workload.kernels)
        {
            ImGui::PushID(static_cast<int>(kernel.second.id));
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("%u", kernel.second.id);
            ImGui::TableNextColumn();
            ImGui::Text(kernel.second.name.c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%u", kernel.second.invocation_count);
            ImGui::TableNextColumn();
            ImGui::Text("%llu", kernel.second.duration_total);
            ImGui::TableNextColumn();
            ImGui::Text("%u", kernel.second.duration_min);
            ImGui::TableNextColumn();
            ImGui::Text("%u", kernel.second.duration_max);
            ImGui::TableNextColumn();
            ImGui::Text("%u", kernel.second.duration_mean);
            ImGui::TableNextColumn();
            ImGui::Text("%u", kernel.second.duration_median);
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    ImGui::EndChild();
    
}

KernelMetric
ComputeSummaryView::GetMetricNameByIndex(uint32_t index) const
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
            return KernelMetric::DurationTotal;
    }
}

void
ComputeSummaryView::RenderPieChart(const WorkloadInfo& workload, KernelMetric metric)
{
    m_pie_chart.UpdateData(
        workload.GenerateChartData(metric));  // TODO: Avoid updating each frames, think
                                              // how to do it if it changes
    m_pie_chart.Render();
}

void
ComputeSummaryView::RenderBarChart(const WorkloadInfo& workload, KernelMetric metric)
{
    m_bar_chart.UpdateData(workload.GenerateChartData(metric));
    m_bar_chart.Render();
}


} // namespace View
}  // namespace RocProfVis