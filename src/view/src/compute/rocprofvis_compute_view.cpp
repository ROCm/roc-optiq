// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_compute_view.h"
#include "model/compute/rocprofvis_compute_data_model.h"
#include "rocprofvis_compute_kernel_details.h"
#include "rocprofvis_compute_roofline.h"
#include "rocprofvis_compute_summary.h"
#include "rocprofvis_compute_table_view.h"
#include "rocprofvis_compute_tester.h"
#include "rocprofvis_compute_workload_view.h"
#include "rocprofvis_event_manager.h"
#include "rocprofvis_settings_manager.h"
#include "widgets/rocprofvis_notification_manager.h"

#include "implot/implot.h"
#include "spdlog/spdlog.h"

namespace RocProfVis
{
namespace View
{

static constexpr float WORKLOAD_WIDTH_FRAC = 0.25f;
static constexpr float WORKLOAD_MIN_WIDTH  = 140.0f;
static constexpr float KERNEL_WIDTH_FRAC   = 0.5f;
static constexpr float KERNEL_MIN_WIDTH    = 350.0f;

ComputeView::ComputeView()
: m_view_created(false)
, m_tab_container(nullptr)
{
    m_tool_bar = std::make_shared<RocCustomWidget>([this]() { this->RenderToolbar(); });
    m_widget_name = GenUniqueName("ComputeView");

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
                NotificationManager::GetInstance().Show(
                    "Successfully fetched metrics for client: " + std::to_string(client_id),
                    NotificationLevel::Success);

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
    m_tab_container->AddTab(TabItem{"Kernel Details", "compute_kernel_details_view", std::make_shared<ComputeKernelDetailsView>(m_data_provider, m_compute_selection), false});
    m_tab_container->AddTab(TabItem{"Table View", "compute_table_view", std::make_shared<ComputeTableView>(m_data_provider, m_compute_selection), false});
    m_tab_container->AddTab(TabItem{"Workload Details", "compute_workload_view", std::make_shared<ComputeWorkloadView>(m_data_provider, m_compute_selection), false});
    
    m_tab_container->AddTab(TabItem{"Compute Tester", "compute_tester_view", std::make_shared<ComputeTester>(m_data_provider, m_compute_selection), false});
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
ComputeView::RenderEditMenuOptions()
{
    ImGui::MenuItem("Compute Edit Item");
    ImGui::Separator();
}

void
ComputeView::RenderToolbar()
{
    const ImGuiStyle& default_style = SettingsManager::GetInstance().GetDefaultStyle();

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, default_style.WindowPadding);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
    ImGui::BeginChild("Toolbar", ImVec2(-1, 0),
                      ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_FrameStyle);

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, default_style.FramePadding);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, default_style.FrameRounding);
    ImGui::AlignTextToFramePadding();

    RenderWorkloadSelection();

    ImGui::PopStyleVar(2);
    ImGui::EndChild();
    ImGui::PopStyleVar(2);
}

void
ComputeView::RenderWorkloadSelection()
{
    if(!m_compute_selection)
        return;

    const std::unordered_map<uint32_t, WorkloadInfo>& workloads =
        m_data_provider.ComputeModel().GetWorkloads();

    SettingsManager&  settings      = SettingsManager::GetInstance();
    const ImGuiStyle& default_style = settings.GetDefaultStyle();

    uint32_t workload_id = m_compute_selection->GetSelectedWorkload();

    ImGui::TextUnformatted("Workload");
    ImGui::SameLine(0.0f, default_style.ItemSpacing.x * 1.5f);

    float avail = ImGui::GetContentRegionAvail().x;
    ImGui::SetNextItemWidth(std::max(avail * WORKLOAD_WIDTH_FRAC, WORKLOAD_MIN_WIDTH));
    if(ImGui::BeginCombo("##Workloads", workloads.count(workload_id) > 0
                                            ? workloads.at(workload_id).name.c_str()
                                            : "Select workload..."))
    {
        if(ImGui::Selectable("-", workload_id == ComputeSelection::INVALID_SELECTION_ID))
        {
            m_compute_selection->SelectWorkload(ComputeSelection::INVALID_SELECTION_ID);
        }
        for(const std::pair<const uint32_t, WorkloadInfo>& workload : workloads)
        {
            bool is_selected = workload_id == workload.second.id;
            if(ImGui::Selectable(workload.second.name.c_str(), is_selected))
            {
                m_compute_selection->SelectWorkload(workload.second.id);
            }
            if(is_selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    ImGui::SameLine(0.0f, default_style.ItemSpacing.x * 2.0f);

    uint32_t kernel_id = m_compute_selection->GetSelectedKernel();
    const KernelInfo* kernel_info =
        m_data_provider.ComputeModel().GetKernelInfo(workload_id, kernel_id);
    std::vector<const KernelInfo*> kernel_info_list =
        m_data_provider.ComputeModel().GetKernelInfoList(workload_id);

    ImGui::TextUnformatted("Kernel");
    ImGui::SameLine(0.0f, default_style.ItemSpacing.x * 1.5f);

    avail = ImGui::GetContentRegionAvail().x;
    ImGui::SetNextItemWidth(
        std::min(avail, std::max(KERNEL_MIN_WIDTH, avail * KERNEL_WIDTH_FRAC)));
    if(ImGui::BeginCombo("##Kernels", kernel_info ? kernel_info->name.c_str()
                                                  : "Select kernel..."))
    {
        if(ImGui::Selectable("-", kernel_id == ComputeSelection::INVALID_SELECTION_ID))
        {
            m_compute_selection->SelectKernel(ComputeSelection::INVALID_SELECTION_ID);
        }
        for(const KernelInfo* info : kernel_info_list)
        {
            bool is_selected = kernel_id == info->id;
            if(ImGui::Selectable(info->name.c_str(), is_selected))
            {
                m_compute_selection->SelectKernel(info->id);
            }
            if(is_selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

}

}  // namespace View
}  // namespace RocProfVis
