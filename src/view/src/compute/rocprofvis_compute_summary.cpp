// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_compute_summary.h"
#include "rocprofvis_compute_roofline.h"
#include "rocprofvis_compute_selection.h"
#include "rocprofvis_data_provider.h"
#include "rocprofvis_event_manager.h"

constexpr float DROPDOWN_SCALE = 15.0f;

namespace RocProfVis
{
namespace View
{

ComputeSummaryView::ComputeSummaryView(
    DataProvider& data_provider, std::shared_ptr<ComputeSelection> compute_selection)
: RocWidget()
, m_data_provider(data_provider)
, m_roofline(nullptr)
, m_compute_selection(compute_selection)
, m_client_id(IdGenerator::GetInstance().GenerateId())
, m_workload_id(0)
, m_selected_metric(KernelInfo::ToIndex(KernelMetric::DurationTotal))
, m_selected_chart_id(0)
{
    auto workload_changed_handler = [this](std::shared_ptr<RocEvent> e) {
        if(auto selection_changed_event =
               std::dynamic_pointer_cast<ComputeSelectionChangedEvent>(e))
        {
            if(m_data_provider.GetTraceFilePath() !=
               selection_changed_event->GetSourceId())
            {
                return;
            }
            m_workload_id = selection_changed_event->GetId();
            if(m_data_provider.ComputeModel().GetWorkloads().count(m_workload_id) > 0)
            {
                UpdataChartData(
                    m_data_provider.ComputeModel().GetWorkloads().at(m_workload_id),
                    m_selected_metric);
            }
            if(m_roofline)
            {
                m_roofline->SetWorkload(selection_changed_event->GetId());
            }
        }
    };

    m_workload_selection_changed_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kComputeWorkloadSelectionChanged),
        workload_changed_handler);

    auto metrics_fetched_handler = [this](std::shared_ptr<RocEvent> e) {
        if(auto metrics_fetched_event =
               std::dynamic_pointer_cast<ComputeMetricsFetchedEvent>(e))
        {
            if(m_data_provider.GetTraceFilePath() != metrics_fetched_event->GetSourceId())
            {
                return;
            }
        }
    };

    auto font_changed_handler = [this](std::shared_ptr<RocEvent> e) {
        this->CalculateCombosWidth();
    };
    m_font_changed_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kFontSizeChanged), font_changed_handler);

    m_metrics_fetched_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kComputeMetricsFetched), metrics_fetched_handler);

    m_roofline = std::make_unique<Roofline>(m_data_provider);

    UpdateKernelMetrics();
    CalculateCombosWidth();

    m_widget_name = GenUniqueName("ComputeSummaryView");
}

ComputeSummaryView::~ComputeSummaryView()
{
    EventManager::GetInstance()->Unsubscribe(
        static_cast<int>(RocEvents::kComputeWorkloadSelectionChanged),
        m_workload_selection_changed_token);
    EventManager::GetInstance()->Unsubscribe(
        static_cast<int>(RocEvents::kComputeMetricsFetched), m_metrics_fetched_token);
    EventManager::GetInstance()->Unsubscribe(
        static_cast<int>(RocEvents::kFontSizeChanged), m_font_changed_token);
}

void
ComputeSummaryView::Update()
{
    if(m_roofline)
    {
        m_roofline->Update();
    }
}

void
ComputeSummaryView::Render()
{
    ImGui::BeginChild("summary");
    const std::unordered_map<uint32_t, WorkloadInfo>& workloads =
        m_data_provider.ComputeModel().GetWorkloads();
    if(workloads.count(m_workload_id) > 0)
    {
        const WorkloadInfo& workload = workloads.at(m_workload_id);
        RenderKernelsTable(workload);

        ImGui::SetNextItemWidth(m_chart_combo_width);
        if(ImGui::BeginCombo("##ChartView", m_chart_views[m_selected_chart_id].data()))
        {
            for(uint8_t index = 0; index < m_chart_views.size(); index++)
            {
                const bool is_selected = (m_selected_chart_id == index);
                if(ImGui::Selectable(m_chart_views[index].data(), is_selected))
                {
                    m_selected_chart_id = index;
                }

                if(is_selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        ImGui::SameLine();
        ImGui::SetNextItemWidth(m_metric_combo_width);
        if(ImGui::BeginCombo("##Metric", m_metric_views[m_selected_metric].data()))
        {
            for(uint8_t index = 0; index < m_metric_views.size(); index++)
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
        if(m_selected_chart_id == 0)
            m_pie_chart.Render();
        else if(m_selected_chart_id == 1)
            m_bar_chart.Render();

        ImGui::EndChild();  // Chart
    }
    if(m_roofline)
    {
        m_roofline->Render();
    }
    ImGui::EndChild();
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

void
ComputeSummaryView::UpdataChartData(const WorkloadInfo& workload, uint8_t metric_id)
{
    ChartData chart_data =
        ChartData::GenerateChartData(static_cast<KernelMetric>(metric_id), workload);

    m_pie_chart.UpdateData(chart_data);
    m_bar_chart.UpdateData(chart_data);
}

void
ComputeSummaryView::UpdateKernelMetrics()
{
    for(uint8_t metric_index = 0; metric_index < KernelInfo::ToIndex(KernelMetric::COUNT);
        metric_index++)
    {
        m_metric_views.push_back(
            KernelInfo::GetMetricName(static_cast<KernelMetric>(metric_index)));
    }
}

}  // namespace View
}  // namespace RocProfVis
