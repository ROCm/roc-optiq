// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_compute_kernel_details.h"
#include "rocprofvis_compute_roofline.h"
#include "rocprofvis_compute_selection.h"
#include "rocprofvis_data_provider.h"
#include "rocprofvis_event_manager.h"
#include "rocprofvis_compute_kernel_metric_table.h"
#include "rocprofvis_settings_manager.h"
#include "widgets/rocprofvis_gui_helpers.h"

#include "imgui.h"

namespace
{
constexpr float MEMORY_CHART_MIN_WIDTH = 2300.0f;
constexpr float SOL_TABLE_MIN_WIDTH    = 600.0f;
constexpr float ROOFLINE_MIN_WIDTH     = 600.0f;
constexpr float FLEX_ITEM_GROW         = 1.0f;

constexpr float KERNEL_TABLE_PANEL_PADDING = 4.0f;
}  // namespace

namespace RocProfVis
{
namespace View
{

ComputeKernelDetailsView::ComputeKernelDetailsView(
    DataProvider& data_provider, std::shared_ptr<ComputeSelection> compute_selection)
: RocWidget()
, m_data_provider(data_provider)
, m_memory_chart(data_provider, compute_selection)
, m_roofline(nullptr)
, m_roofline_flex_item(nullptr)
, m_kernel_metric_table(nullptr)
, m_compute_selection(compute_selection)
, m_client_id(IdGenerator::GetInstance().GenerateId())
, m_sol_table(std::make_shared<MetricTableWidget>(data_provider, compute_selection,
                                                  METRIC_CAT_SOL, METRIC_TABLE_SOL))
, m_workload_selection_changed_token(EventManager::InvalidSubscriptionToken)
, m_kernel_selection_changed_token(EventManager::InvalidSubscriptionToken)
, m_new_table_data_token(EventManager::InvalidSubscriptionToken)
, m_metrics_fetched_token(EventManager::InvalidSubscriptionToken)
, m_send_metric_to_kernel_details_token(EventManager::InvalidSubscriptionToken)
{
    SubscribeToEvents();

    m_roofline =
        std::make_shared<RocProfVis::View::Roofline>(data_provider, Roofline::SingleKernel);
    m_kernel_metric_table =
        std::make_shared<RocProfVis::View::KernelMetricTable>(data_provider,
                                                              compute_selection);

    auto memory_chart_wrapper = std::make_shared<RocCustomWidget>([this]() {
        SettingsManager& settings = SettingsManager::GetInstance();
        ImGui::PushStyleColor(ImGuiCol_ChildBg, settings.GetColor(Colors::kBgPanel));
        ImGui::PushStyleColor(ImGuiCol_Border, settings.GetColor(Colors::kBorderColor));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding,
                            settings.GetDefaultStyle().ChildRounding);
        ImGui::BeginChild("memory_chart_card", ImVec2(0, 0),
                          ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Borders |
                              ImGuiChildFlags_AlwaysUseWindowPadding);
        SectionTitle("Memory Chart");
        m_memory_chart.Render();
        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(2);
    });

    m_flex_container.items = {
        {"memory_chart", memory_chart_wrapper, MEMORY_CHART_MIN_WIDTH, 0.0f,
         FLEX_ITEM_GROW, true},
        {"sol_table", m_sol_table, SOL_TABLE_MIN_WIDTH, 0.0f, FLEX_ITEM_GROW},
        {"roofline", m_roofline, ROOFLINE_MIN_WIDTH, 0.0f, FLEX_ITEM_GROW},
    };
    m_roofline_flex_item = m_flex_container.GetItem("roofline");
    m_widget_name        = GenUniqueName("ComputeKernelDetailsView");
}

ComputeKernelDetailsView::~ComputeKernelDetailsView()
{
    EventManager::GetInstance()->Unsubscribe(
        static_cast<int>(RocEvents::kComputeWorkloadSelectionChanged),
        m_workload_selection_changed_token);
    EventManager::GetInstance()->Unsubscribe(
        static_cast<int>(RocEvents::kComputeKernelSelectionChanged),
        m_kernel_selection_changed_token);
    EventManager::GetInstance()->Unsubscribe(
        static_cast<int>(RocEvents::kComputeMetricsFetched), m_metrics_fetched_token);
    EventManager::GetInstance()->Unsubscribe(
        static_cast<int>(RocEvents::kNewTableData), m_new_table_data_token);
    EventManager::GetInstance()->Unsubscribe(
        static_cast<int>(RocEvents::kComputeShowMetricInKernelDetails),
        m_send_metric_to_kernel_details_token);
}

void
ComputeKernelDetailsView::SubscribeToEvents()
{
    auto workload_changed_handler = [this](std::shared_ptr<RocEvent> e) {
        auto evt = std::dynamic_pointer_cast<ComputeSelectionChangedEvent>(e);
        if(evt && evt->GetSourceId() == m_data_provider.GetTraceFilePath())
        {
            if(m_kernel_metric_table)
            {
                m_data_provider.ComputeModel().GetKernelSelectionTable().Clear();
                m_kernel_metric_table->FetchData(evt->GetId());
            }
            if(m_roofline)
            {
                m_roofline->SetWorkload(evt->GetId());
            }
            if(m_sol_table)
            {
                m_sol_table->Clear();
            }
        }
    };

    m_workload_selection_changed_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kComputeWorkloadSelectionChanged),
        workload_changed_handler);

    auto kernel_changed_handler = [this](std::shared_ptr<RocEvent> e) {
        auto evt = std::dynamic_pointer_cast<ComputeSelectionChangedEvent>(e);
        if(evt && evt->GetSourceId() == m_data_provider.GetTraceFilePath())
        {
            m_memory_chart.FetchMemChartMetrics();
            if(m_roofline)
            {
                m_roofline->SetKernel(evt->GetId());
            }
            if(m_sol_table)
            {
                m_sol_table->FetchMetrics();
            }
        }
    };

    m_kernel_selection_changed_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kComputeKernelSelectionChanged),
        kernel_changed_handler);

    auto metrics_fetched_handler = [this](std::shared_ptr<RocEvent> e) {
        auto evt = std::dynamic_pointer_cast<ComputeMetricsFetchedEvent>(e);
        if(evt && evt->GetSourceId() == m_data_provider.GetTraceFilePath())
        {
            if(m_memory_chart.GetClientId() == evt->GetClientId())
            {
                m_memory_chart.UpdateMetrics();
            }
            if(m_sol_table->GetClientId() == evt->GetClientId())
            {
                m_sol_table->UpdateTable();
            }
        }
    };
    m_metrics_fetched_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kComputeMetricsFetched), metrics_fetched_handler);

    auto new_table_data_handler = [this](std::shared_ptr<RocEvent> e) {
        if(auto table_data_event = std::dynamic_pointer_cast<TableDataEvent>(e))
        {
            if(m_data_provider.GetTraceFilePath() != table_data_event->GetSourceId())
            {
                return;
            }

            if(table_data_event->GetResponseCode() != kRocProfVisResultSuccess)
            {
                return;
            }

            if(table_data_event->GetRequestID() == DataProvider::METRIC_PIVOT_TABLE_REQUEST_ID)
            {
                m_kernel_metric_table->HandleNewData();
            }
        }
    };

    m_new_table_data_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kNewTableData), new_table_data_handler);

    auto metric_navigation_handler = [this](std::shared_ptr<RocEvent> e) {
        auto evt = std::dynamic_pointer_cast<ComputeAddMetricToKernelDetailsEvent>(e);
        if(!evt || evt->GetSourceId() != m_data_provider.GetTraceFilePath())
        {
            return;
        }

        if(m_kernel_metric_table)
        {
            m_kernel_metric_table->SetExternalQuery(
                MetricId{ evt->GetCategoryId(), evt->GetTableId(), evt->GetEntryId() },
                evt->GetValueName());
        }
    };

    m_send_metric_to_kernel_details_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kComputeShowMetricInKernelDetails),
        metric_navigation_handler);
}

void
ComputeKernelDetailsView::Update()
{
    if(m_roofline)
    {
        m_roofline->Update();
    }
    if(m_kernel_metric_table)
    {
        m_kernel_metric_table->Update();
    }
}

void
ComputeKernelDetailsView::Render()
{
    ImGui::BeginChild("kernel_details", ImVec2(0, 0),
                      ImGuiChildFlags_AlwaysUseWindowPadding);
    if(m_roofline_flex_item)
    {
        // Maintain 2:1 aspect ratio until 15-frame minimum.
        m_roofline_flex_item->height =
            std::max(ImGui::GetContentRegionAvail().x / 4.0f,
                     ImGui::GetFrameHeightWithSpacing() * 15.0f);
    }
    if(m_kernel_metric_table)
    {
        m_kernel_metric_table->Render();
    }

    ImGui::Dummy(ImVec2(0.0f, std::max<float>(KERNEL_TABLE_PANEL_PADDING,
                                              ImGui::GetStyle().ItemSpacing.y)));
    m_flex_container.Render();
    ImGui::EndChild();
}

}  // namespace View
}  // namespace RocProfVis
