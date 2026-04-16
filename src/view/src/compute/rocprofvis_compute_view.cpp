// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_compute_view.h"
#include "model/compute/rocprofvis_compute_data_model.h"
#include "rocprofvis_compute_comparison.h"
#include "rocprofvis_compute_roofline.h"
#include "rocprofvis_compute_summary.h"
#include "rocprofvis_compute_table_view.h"
#include "rocprofvis_compute_tester.h"
#include "rocprofvis_compute_workload_view.h"
#include "rocprofvis_event_manager.h"
#include "rocprofvis_settings_manager.h"
#include "widgets/rocprofvis_gui_helpers.h"
#include "widgets/rocprofvis_notification_manager.h"
#include "rocprofvis_compute_kernel_details.h"

#include "implot/implot.h"
#include "spdlog/spdlog.h"

namespace RocProfVis
{
namespace View
{

ComputeView::ComputeView()
: m_view_created(false)
, m_tab_container(nullptr)
{
    m_tool_bar = std::make_shared<RocCustomWidget>([this]() { this->RenderToolbar(); });
    m_widget_name = GenUniqueName("ComputeView");

    m_data_provider.SetTraceLoadedCallback([this](const std::string& trace_path,
                                                  uint64_t           response_code) {
        if(response_code != kRocProfVisResultSuccess)
        {
            spdlog::error("Failed to load trace: {}", response_code);
            NotificationManager::GetInstance().Show("Failed to load trace: " + trace_path,
                                                    NotificationLevel::Error);
        }
        else
        {
            // select the first workload by default when a trace is loaded
            const auto& workloads = m_data_provider.ComputeModel().GetWorkloads();  
            if(!workloads.empty())
            {
                m_compute_selection->SelectWorkload(workloads.begin()->first);
            }
        }
    });

    m_data_provider.SetFetchMetricsCallback(
        [this](const std::string& trace_path, uint64_t client_id, bool success) {
            if(!success)
            {
                NotificationManager::GetInstance().Show(
                    "Failed to fetch metrics for trace: " + trace_path,
                    NotificationLevel::Error);
            }
            else
            {
#ifdef ROCPROFVIS_DEVELOPER_MODE
                NotificationManager::GetInstance().Show(
                    "Successfully fetched metrics for client: " + std::to_string(client_id),
                    NotificationLevel::Success);
#endif
                // trigger metrics fetched event to update the UI
                EventManager::GetInstance()->AddEvent(
                    std::make_shared<ComputeMetricsFetchedEvent>(client_id, trace_path));
            }
        });

    m_data_provider.SetTableDataReadyCallback(
        [](const std::string& trace_path, uint64_t request_id, uint64_t response_code) {
            if(response_code != kRocProfVisResultSuccess)
            {
                NotificationManager::GetInstance().Show(
                    "Failed to fetch table data for trace: " + trace_path,
                    NotificationLevel::Error);
            }

             // trigger new table data event to update the UI
            EventManager::GetInstance()->AddEvent(
                std::make_shared<TableDataEvent>(trace_path, request_id, response_code));
        });
}

ComputeView::~ComputeView() {}

void
ComputeView::Update()
{
    auto last_state = m_data_provider.GetState();
    m_data_provider.Update();

    if(!m_view_created)
    {
        CreateView();
        m_view_created = true;
    }

    auto new_state = m_data_provider.GetState();

    // new file loaded
    if(last_state != new_state && new_state == ProviderState::kReady)
    {
    }

    if(new_state == ProviderState::kReady)
    {
        if(m_tab_container)
        {
            m_tab_container->Update();
        }
    }
}

void
ComputeView::CreateView()
{
    m_compute_selection = std::make_shared<ComputeSelection>(m_data_provider);

    m_tab_container = std::make_shared<TabContainer>();
    m_tab_container->AddTab(TabItem{"Summary View", "compute_summary_view", std::make_shared<ComputeSummaryView>(m_data_provider, m_compute_selection), false});
    m_tab_container->AddTab(TabItem{ "Kernel Details", "compute_kernel_details_view", std::make_shared<ComputeKernelDetailsView>(m_data_provider, m_compute_selection), false });
    m_tab_container->AddTab(TabItem{ "Table View", "compute_table_view", std::make_shared<ComputeTableView>(m_data_provider, m_compute_selection), false });
    m_tab_container->AddTab(TabItem{"Workload Details", "compute_workload_view", std::make_shared<ComputeWorkloadView>(m_data_provider, m_compute_selection), false});
    m_tab_container->AddTab(TabItem{"Baseline Comparison", "compute_comparison_view", std::make_shared<ComputeComparisonView>(m_data_provider, m_compute_selection), false});
#ifdef ROCPROFVIS_DEVELOPER_MODE
    m_tab_container->AddTab(TabItem{"Compute Tester", "compute_tester_view", std::make_shared<ComputeTester>(m_data_provider, m_compute_selection), false});
#endif
    m_tab_container->SetAllowToolTips(false);
}

void
ComputeView::DestroyView()
{
    m_view_created = false;
}

bool
ComputeView::LoadTrace(rocprofvis_controller_t* controller, const std::string& file_path)
{
    bool result = false;
    result      = m_data_provider.FetchTrace(controller, file_path);
    return result;
}

void
ComputeView::Render()
{
    if(m_data_provider.GetState() == ProviderState::kLoading)
    {
        RenderLoadingScreen(m_data_provider.GetProgressMessage());
    }
    else
    {
        if(m_tab_container)
        {
            m_tab_container->Render();
        }
    }
}

std::shared_ptr<RocWidget>
ComputeView::GetToolbar()
{
    return m_tool_bar;
}

void
ComputeView::RenderToolbar()
{
    const ImGuiStyle& style = SettingsManager::GetInstance().GetDefaultStyle();
    ImGui::PushStyleColor(
        ImGuiCol_ChildBg,
        ImGui::ColorConvertU32ToFloat4(m_settings_manager.GetColor(Colors::kBgPanel)));
    ImGui::PushStyleColor(ImGuiCol_Border,
                          ImGui::ColorConvertU32ToFloat4(
                              m_settings_manager.GetColor(Colors::kBorderColor)));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, style.WindowPadding);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0.0f);
    ImGui::BeginChild("Toolbar", ImVec2(-1, 0),
                      ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Borders);

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, style.FramePadding);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, style.FrameRounding);
    ImGui::AlignTextToFramePadding();

    RenderWorkloadSelection();

    // pop content style
    ImGui::PopStyleVar(2);
    ImGui::EndChild();
    // pop child window style
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
}

void
ComputeView::RenderWorkloadSelection()
{
    if(!m_compute_selection)
    {
        return;
    }

    const ImGuiStyle& style          = SettingsManager::GetInstance().GetDefaultStyle();

    const std::unordered_map<uint32_t, WorkloadInfo>& workloads =
        m_data_provider.ComputeModel().GetWorkloads();

    uint32_t workload_id = m_compute_selection->GetSelectedWorkload();
    ImGui::Text("Workload:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(ImGui::GetFrameHeight() * 10.0f);
    ImGui::BeginDisabled(workloads.empty());
    if(ImGui::BeginCombo("##Workloads", workloads.count(workload_id) > 0
                                          ? workloads.at(workload_id).name.c_str()
                                          : "-"))
    {

        for(const std::pair<const uint32_t, WorkloadInfo>& workload : workloads)
        {
            if(ImGui::Selectable(workload.second.name.c_str(),
                                 workload_id == workload.second.id))
            {
                m_compute_selection->SelectWorkload(workload.second.id);
            }
        }
        ImGui::EndCombo();
    }
    ImGui::EndDisabled();
    ImGui::SameLine(0, style.ItemSpacing.x);
    VerticalSeparator();
    ImGui::SameLine(0, style.ItemSpacing.x);
    ImGui::Text("Kernel:");
    ImGui::SameLine();
    uint32_t kernel_id = m_compute_selection->GetSelectedKernel();
    const KernelInfo* kernel_info = m_data_provider.ComputeModel().GetKernelInfo(workload_id, kernel_id);

    std::vector<const KernelInfo*> kernel_info_list =
        m_data_provider.ComputeModel().GetKernelInfoList(workload_id);
    ImGui::SetNextItemWidth(ImGui::GetFrameHeight() * 10.0f);
    ImGui::BeginDisabled(kernel_info_list.empty());
    if(ImGui::BeginCombo("##Kernels", kernel_info ? kernel_info->name.c_str() : "-"))
    {
        for(const KernelInfo* info : kernel_info_list)
        {
            if(ImGui::Selectable(info->name.c_str(), kernel_id == info->id))
            {
                m_compute_selection->SelectKernel(info->id);
            }
        }
        ImGui::EndCombo();
    }
    ImGui::EndDisabled();
}

}  // namespace View
}  // namespace RocProfVis
