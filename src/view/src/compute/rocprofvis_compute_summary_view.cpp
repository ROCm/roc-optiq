// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_compute_summary_view.h"
#include "implot/implot.h"

constexpr float DROPDOWN_SCALE = 15.0f;

namespace RocProfVis
{
namespace View
{
ComputeSummaryView::ComputeSummaryView(DataProvider& data_provider)
: RocWidget()
, m_data_provider(data_provider)
, m_workload_id(0) 
, m_selected_metric(1) //default metric - duration total
{
    CalculateCombosWidth();
    auto font_changed_handler = [this](std::shared_ptr<RocEvent> e) {
        this->CalculateCombosWidth();
    };
    m_font_changed_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kFontSizeChanged), font_changed_handler);

}
ComputeSummaryView::~ComputeSummaryView() 
{
    EventManager::GetInstance()->Unsubscribe(
        static_cast<int>(RocEvents::kFontSizeChanged), m_font_changed_token);
};

void
ComputeSummaryView::Render()
{
    const std::unordered_map<uint32_t, WorkloadInfo>& workloads =
        m_data_provider.ComputeModel().GetWorkloads();
    ImGui::SetNextItemWidth(ImGui::GetFrameHeight() * DROPDOWN_SCALE);
    if(ImGui::BeginCombo("Workloads",
                         workloads.count(m_workload_id) > 0
                                          ? workloads.at(m_workload_id).name.c_str()
                             : "-"))
    {
        if(ImGui::Selectable("-", m_workload_id == 0))
        {
            m_workload_id = 0;
        }
        for(const std::pair<const uint32_t, WorkloadInfo>& workload : workloads)
        {
            if(ImGui::Selectable(workload.second.name.c_str(),
                                 m_workload_id == workload.second.id))
            {
                m_workload_id = workload.second.id;
                UpdataChartData(workload.second, m_selected_metric);
            }
        }
        ImGui::EndCombo();
    }

    if(workloads.count(m_workload_id) > 0)
    {
        const WorkloadInfo& workload = workloads.at(m_workload_id);
        RenderKernelsTable(workload);

        static uint32_t selected_chart_id = 0;
        ImGui::SetNextItemWidth(m_chart_combo_width);
        if(ImGui::BeginCombo("##ChartView", m_chart_views[selected_chart_id].data()))
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
        ImGui::SetNextItemWidth(m_metric_combo_width);
        if(ImGui::BeginCombo("##Metric", m_metric_views[m_selected_metric].data()))
        {
            for(uint32_t index = 0; index < m_metric_views.size(); index++)
            {
                const bool is_selected = (m_selected_metric == index);
                if(ImGui::Selectable(m_metric_views[index].data(), is_selected))
                {
                    m_selected_metric = index;
                    UpdataChartData(workload, m_selected_metric);
                }

                if(is_selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        ImGui::BeginChild("Chart");
        if(selected_chart_id == 0)
            m_pie_chart.Render();
        else if(selected_chart_id == 1)
            m_bar_chart.Render();

        ImGui::EndChild();  // Chart
    }
}

void
ComputeSummaryView::CalculateCombosWidth()
{
    m_chart_combo_width  = 0.0f;
    m_metric_combo_width = 0.0f;

    ImGuiStyle& style = ImGui::GetStyle();

    const float frame_padding_lr = style.FramePadding.x * 2.0f;
    const float inner_spacing    = style.ItemInnerSpacing.x;
    const float arrow_area = ImGui::GetFrameHeight();  // approximate arrow / preview area
    const float extra_margin_px = 8.0f;                // small visual margin
    const float min_width_px    = 80.0f;               // sensible minimum

    float max_chart_text_w = 0.0f;
    for(const auto& chart : m_chart_views)
    {
        const ImVec2 ts = ImGui::CalcTextSize(chart.data());
        if(ts.x > max_chart_text_w) max_chart_text_w = ts.x;
    }

    // Compute longest label width for metric views
    float max_metric_text_w = 0.0f;
    for(const auto& metric : m_metric_views)
    {
        const ImVec2 ts = ImGui::CalcTextSize(metric.data());
        if(ts.x > max_metric_text_w) max_metric_text_w = ts.x;
    }

    // Final widths include padding, spacing, arrow area and a small margin.
    float chart_total = max_chart_text_w + frame_padding_lr + inner_spacing + arrow_area +
                        extra_margin_px;
    float metric_total = max_metric_text_w + frame_padding_lr + inner_spacing +
                         arrow_area + extra_margin_px;

    if(chart_total < min_width_px) chart_total = min_width_px;
    if(metric_total < min_width_px) metric_total = min_width_px;

    m_chart_combo_width  = chart_total;
    m_metric_combo_width = metric_total;
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
ComputeSummaryView::UpdataChartData(const WorkloadInfo& workload, uint32_t metric_id)
{
    ChartData chart_data =
        ChartData::GenerateChartData(GetMetricNameByIndex(metric_id), workload);

    m_pie_chart.UpdateData(chart_data);
    m_bar_chart.UpdateData(chart_data);
}

} // namespace View
}  // namespace RocProfVis