// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_compute_kernel_details.h"
#include "rocprofvis_compute_roofline.h"
#include "rocprofvis_compute_selection.h"
#include "rocprofvis_data_provider.h"
#include "rocprofvis_event_manager.h"
#include "rocprofvis_compute_kernel_metric_table.h"
#include "rocprofvis_settings_manager.h"
#include "icons/rocprovfis_icon_defines.h"
#include "widgets/rocprofvis_gui_helpers.h"

#include "imgui.h"

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
, m_kernel_metric_table(nullptr)
, m_compute_selection(compute_selection)
, m_client_id(IdGenerator::GetInstance().GenerateId())
, m_sol_table(std::make_shared<MetricTableWidget>(data_provider, compute_selection, METRIC_CAT_SOL, METRIC_TABLE_SOL))
, m_workload_selection_changed_token(EventManager::InvalidSubscriptionToken)
, m_kernel_selection_changed_token(EventManager::InvalidSubscriptionToken)
, m_new_table_data_token(EventManager::InvalidSubscriptionToken)
, m_metrics_fetched_token(EventManager::InvalidSubscriptionToken)
{
    SubscribeToEvents();

    m_roofline = std::make_shared<RocProfVis::View::Roofline>(data_provider);
    m_kernel_metric_table = std::make_shared<RocProfVis::View::KernelMetricTable>(data_provider, compute_selection);

    auto memory_chart_wrapper = std::make_shared<RocCustomWidget>([this]() {
        ImGui::Text("Memory Chart");
        m_memory_chart.Render();
    });

    m_flex_container.items = {
        {"memory_chart", memory_chart_wrapper,  2300.0f, 700.0f, 1.0f},
        {"sol_table",    m_sol_table,           700.0f,  700.0f, 1.0f},
        {"roofline",     m_roofline,            1000.0f, 700.0f, 1.0f},
    };

    auto table_panel = std::make_shared<RocCustomWidget>([this]() {
        SettingsManager& settings   = SettingsManager::GetInstance();
        ImFont*          icon_font  = settings.GetFontManager().GetIconFont(FontType::kDefault);
        ImU32            header_bg  = settings.GetColor(Colors::kTableHeaderBg);
        ImU32            border_col = settings.GetColor(Colors::kBorderGray);
        ImGuiStyle&      style      = ImGui::GetStyle();

        float  avail_w  = ImGui::GetContentRegionAvail().x;
        float  header_h = ImGui::GetFrameHeightWithSpacing();

        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        ImVec2      cursor    = ImGui::GetCursorScreenPos();

        draw_list->AddRectFilled(cursor,
                                 ImVec2(cursor.x + avail_w, cursor.y + header_h),
                                 header_bg, style.ChildRounding);
        draw_list->AddRect(cursor,
                           ImVec2(cursor.x + avail_w, cursor.y + header_h),
                           border_col, style.ChildRounding);

        float icon_y_offset = (header_h - ImGui::GetFontSize()) * 0.5f;
        ImGui::SetCursorScreenPos(
            ImVec2(cursor.x + style.FramePadding.x, cursor.y + icon_y_offset));

        const char* icon = m_show_kernel_table ? ICON_EYE : ICON_EYE_SLASH;
        if(IconButton(icon, icon_font, ImVec2(0, 0),
                      m_show_kernel_table ? "Hide Table" : "Show Table",
                      style.WindowPadding, true, style.FramePadding))
        {
            m_show_kernel_table = !m_show_kernel_table;
        }

        ImGui::SameLine(0.0f, 8.0f);
        ImGui::SetCursorScreenPos(
            ImVec2(ImGui::GetCursorScreenPos().x, cursor.y + icon_y_offset));
        ImGui::Text("Kernel Selection Table");

        ImGui::SetCursorScreenPos(
            ImVec2(cursor.x, cursor.y + header_h + style.ItemSpacing.y));

        if(m_show_kernel_table && m_kernel_metric_table)
            m_kernel_metric_table->Render();
    });

    auto flex_panel = std::make_shared<RocCustomWidget>([this]() {
        ImGui::Dummy(ImVec2(0.0f, 4.0f));
        m_flex_container.Render();
    });

    auto top_item    = LayoutItem::CreateFromWidget(table_panel);
    auto bottom_item = LayoutItem::CreateFromWidget(flex_panel);

    m_vsplit = std::make_unique<VSplitContainer>(top_item, bottom_item);
    m_vsplit->SetMinTopHeight(40.0f);
    m_vsplit->SetMinBottomHeight(200.0f);
    m_vsplit->SetSplit(0.25f);

    m_widget_name = GenUniqueName("ComputeKernelDetailsView");
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
}

void ComputeKernelDetailsView::SubscribeToEvents()
{
    auto workload_changed_handler = [this](std::shared_ptr<RocEvent> e) {
        auto evt = std::dynamic_pointer_cast<ComputeSelectionChangedEvent>(e);
        if(evt && evt->GetSourceId() == m_data_provider.GetTraceFilePath())
        {
            // Fetch pivot table data
            if(m_kernel_metric_table)
            {
                //clear existing model table data
                m_data_provider.ComputeModel().GetKernelSelectionTable().Clear();
                m_kernel_metric_table->FetchData(evt->GetId());
            }
            m_sol_table->Clear();
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
                m_roofline->SetWorkload(m_compute_selection->GetSelectedWorkload());
                m_roofline->SetKernel(evt->GetId());
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
                m_sol_table->FetchMetrics();
            }
            if(m_sol_table->GetClientId() == evt->GetClientId())
            {
                m_sol_table->UpdateTable();
            }
        }
    };
    m_metrics_fetched_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kComputeMetricsFetched), metrics_fetched_handler);

    // subscribe to fetch table data event 
    auto new_table_data_handler = [this](std::shared_ptr<RocEvent> e) {
        if(auto table_data_event = std::dynamic_pointer_cast<TableDataEvent>(e) )
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
    if(m_vsplit)
        m_vsplit->Render();
}

}  // namespace View
}  // namespace RocProfVis
