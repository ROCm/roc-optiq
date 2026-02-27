// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_compute_view.h"
#include "rocprofvis_compute_table_view.h"
#include "rocprofvis_compute_kernel_details.h"
#include "rocprofvis_compute_summary.h"
#include "implot/implot.h"
#include "rocprofvis_event_manager.h"
#include "rocprofvis_compute_roofline.h"
#include "rocprofvis_settings_manager.h"
#include "model/compute/rocprofvis_compute_data_model.h"
#include "widgets/rocprofvis_notification_manager.h"

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
    ImGuiStyle& style          = ImGui::GetStyle();
    ImVec2      frame_padding  = style.FramePadding;
    float       frame_rounding = style.FrameRounding;

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 4));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
    ImGui::BeginChild("Toolbar", ImVec2(-1, 0),
                      ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_FrameStyle);

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, frame_padding);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, frame_rounding);
    ImGui::AlignTextToFramePadding();

    RenderWorkloadSelection();

    // pop content style
    ImGui::PopStyleVar(2);
    ImGui::EndChild();
    // pop child window style
    ImGui::PopStyleVar(2);
}

void
ComputeView::RenderWorkloadSelection()
{
    if(!m_compute_selection)
    {
        return;
    }

    const std::unordered_map<uint32_t, WorkloadInfo>& workloads =
        m_data_provider.ComputeModel().GetWorkloads();

    uint32_t workload_id = m_compute_selection->GetSelectedWorkload();
    ImGui::Text("Workload:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(ImGui::GetFrameHeight() * 10.0f);
    if(ImGui::BeginCombo("##Workloads", workloads.count(workload_id) > 0
                                          ? workloads.at(workload_id).name.c_str()
                                          : "-"))
    {
        if(ImGui::Selectable("-", workload_id == ComputeSelection::INVALID_SELECTION_ID))
        {
            m_compute_selection->SelectWorkload(ComputeSelection::INVALID_SELECTION_ID);
        }
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
    ImGui::SameLine();
    ImGui::Text("Kernel:");
    ImGui::SameLine();
    uint32_t kernel_id = m_compute_selection->GetSelectedKernel();
    const KernelInfo* kernel_info = m_data_provider.ComputeModel().GetKernelInfo(workload_id, kernel_id);

    std::vector<const KernelInfo*> kernel_info_list =
        m_data_provider.ComputeModel().GetKernelInfoList(workload_id);
    ImGui::SetNextItemWidth(ImGui::GetFrameHeight() * 10.0f);
    if(ImGui::BeginCombo("##Kernels", kernel_info ? kernel_info->name.c_str() : "-"))
    {
        if(ImGui::Selectable("-", kernel_id == ComputeSelection::INVALID_SELECTION_ID))
        {
            m_compute_selection->SelectKernel(ComputeSelection::INVALID_SELECTION_ID);
        }
        for(const KernelInfo* info : kernel_info_list)
        {
            if(ImGui::Selectable(info->name.c_str(), kernel_id == info->id))
            {
                m_compute_selection->SelectKernel(info->id);
            }
        }
        ImGui::EndCombo();
    }
}

ComputeTester::ComputeTester(DataProvider& data_provider, std::shared_ptr<ComputeSelection> compute_selection)
: m_data_provider(data_provider)
, m_compute_selection(compute_selection)
, m_selections({ true, 0, {}, {}, SelectionState::FP32, {}, {}, {} })
, m_display_names({
      {
          { kRPVControllerRooflineCeilingComputeVALUI8, "VALU I8" },
          { kRPVControllerRooflineCeilingComputeMFMAI8, "MFMA I8" },
          { kRPVControllerRooflineCeilingComputeMFMAFP4, "MFMA FP4" },
          { kRPVControllerRooflineCeilingComputeMFMAFP6, "MFMA FP6" },
          { kRPVControllerRooflineCeilingComputeMFMAFP8, "MFMA FP8" },
          { kRPVControllerRooflineCeilingComputeMFMABF16, "MFMA BF16" },
          { kRPVControllerRooflineCeilingComputeVALUFP16, "VALU FP16" },
          { kRPVControllerRooflineCeilingComputeMFMAFP16, "MFMA FP16" },
          { kRPVControllerRooflineCeilingComputeVALUI32, "VALU I32" },
          { kRPVControllerRooflineCeilingComputeVALUFP32, "VALU FP32" },
          { kRPVControllerRooflineCeilingComputeMFMAFP32, "MFMA FP32" },
          { kRPVControllerRooflineCeilingComputeVALUI64, "VALU I64" },
          { kRPVControllerRooflineCeilingComputeVALUFP64, "VALU FP64" },
          { kRPVControllerRooflineCeilingComputeMFMAFP64, "MFMA FP64" },
      },
      {
          { kRPVControllerRooflineCeilingTypeBandwidthHBM, "HBM" },
          { kRPVControllerRooflineCeilingTypeBandwidthL2, "L2" },
          { kRPVControllerRooflineCeilingTypeBandwidthL1, "L1" },
          { kRPVControllerRooflineCeilingTypeBandwidthLDS, "LDS" },
      },
      {
          { kRPVControllerRooflineKernelIntensityTypeHBM, "HBM" },
          { kRPVControllerRooflineKernelIntensityTypeL2, "L2" },
          { kRPVControllerRooflineKernelIntensityTypeL1, "L1" },
      },
  })
{}

ComputeTester::~ComputeTester() {}

void
ComputeTester::Update()
{}

void
ComputeTester::Render()
{
    const std::unordered_map<uint32_t, WorkloadInfo>& workloads =
        m_data_provider.ComputeModel().GetWorkloads();

    uint32_t global_workload_id = m_compute_selection
                                     ? m_compute_selection->GetSelectedWorkload()
                                     : ComputeSelection::INVALID_SELECTION_ID;
    const WorkloadInfo* selected_wl = workloads.count(global_workload_id)
                                         ? &workloads.at(global_workload_id)
                                         : nullptr;
    m_query_builder.SetWorkload(selected_wl);

    if(ImGui::Button("Open Query Builder"))
    {
        m_query_builder.Open();
    }
    if(!m_query_builder.GetQueryString().empty())
    {
        ImGui::SameLine();
        ImGui::Text("Query: %s", m_query_builder.GetQueryString().c_str());
    }
    m_query_builder.Render();
    ImGui::NewLine();

    ImGui::SetNextItemWidth(ImGui::GetFrameHeight() * 15.0f);
    if(ImGui::BeginCombo("Workloads",
                         workloads.count(m_selections.workload_id) > 0
                             ? workloads.at(m_selections.workload_id).name.c_str()
                             : "-"))
    {
        if(ImGui::Selectable("-", m_selections.workload_id == 0))
        {
            m_selections.workload_id = 0;
        }
        for(const std::pair<const uint32_t, WorkloadInfo>& workload : workloads)
        {
            if(ImGui::Selectable(workload.second.name.c_str(),
                                 m_selections.workload_id == workload.second.id))
            {
                m_selections.workload_id = workload.second.id;
                m_selections.kernel_ids.clear();
                m_selections.metric_ids.clear();
            }
        }
        ImGui::EndCombo();
    }
    if(workloads.count(m_selections.workload_id) > 0)
    {
        ImGui::BeginChild("sv");
        ImGui::BeginChild("info", ImVec2(0, 0),
                          ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);
        const WorkloadInfo& workload = workloads.at(m_selections.workload_id);
        if(workload.system_info.size() == 2 &&
           workload.system_info[0].size() == workload.system_info[1].size())
        {
            ImGui::BeginChild("sys_info",
                              ImVec2(ImGui::GetContentRegionAvail().x / 2.0f, 0.0f),
                              ImGuiChildFlags_AutoResizeY);
            ImGui::Text("System Information");
            if(ImGui::BeginTable("", static_cast<int>(workload.system_info.size()),
                                 ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders))
            {
                for(size_t i = 0; i < workload.system_info[0].size(); i++)
                {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::Text(workload.system_info[0][i].c_str());
                    ImGui::TableNextColumn();
                    ImGui::Text(workload.system_info[1][i].c_str());
                }
                ImGui::EndTable();
            }
            ImGui::EndChild();
        }
        if(workload.profiling_config.size() == 2 &&
           workload.profiling_config[0].size() == workload.profiling_config[1].size())
        {
            ImGui::SameLine();
            ImGui::BeginChild("config", ImVec2(0, 0), ImGuiChildFlags_AutoResizeY);
            ImGui::Text("Profiling Configuration");
            if(ImGui::BeginTable("", static_cast<int>(workload.profiling_config.size()),
                                 ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders))
            {
                for(size_t i = 0; i < workload.profiling_config[0].size(); i++)
                {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::Text(workload.profiling_config[0][i].c_str());
                    ImGui::TableNextColumn();
                    ImGui::Text(workload.profiling_config[1][i].c_str());
                }
                ImGui::EndTable();
            }
            ImGui::EndChild();
        }
        ImGui::EndChild();
        ImGui::NewLine();
        ImGui::BeginChild("metrics", ImVec2(0, 0),
                          ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);
        ImGui::Text("Available Metrics");
        for(const std::pair<const uint32_t, AvailableMetrics::Category>& category :
            workload.available_metrics.tree)
        {
            ImGui::PushID(static_cast<int>(category.first));
            if(ImGui::TreeNodeEx(std::string("[" + std::to_string(category.second.id) +
                                             "] " + category.second.name)
                                     .c_str()))
            {
                for(const std::pair<const uint32_t, AvailableMetrics::Table>& table :
                    category.second.tables)
                {
                    ImGui::PushID(static_cast<int>(table.first));
                    std::string partial_metric_id = std::to_string(category.second.id) +
                                                    "." + std::to_string(table.second.id);
                    bool table_selected =
                        m_selections.metric_ids.count(category.first) > 0 &&
                        m_selections.metric_ids.at(category.first).count(table.first) >
                            0 &&
                        m_selections.metric_ids.at(category.first).at(table.first).first;
                    bool table_selected_changed = ImGui::Selectable("", table_selected);
                    if(table_selected_changed)
                    {
                        if(table_selected)
                        {
                            if(m_selections.metric_ids.at(category.first).size() == 1)
                            {
                                m_selections.metric_ids.erase(category.first);
                            }
                            else
                            {
                                m_selections.metric_ids.at(category.first)
                                    .erase(table.first);
                            }
                        }
                        else
                        {
                            m_selections.metric_ids[category.first][table.first] = { true,
                                                                                     {} };
                        }
                    }
                    ImGui::SameLine(0.0f);
                    ImGui::BeginDisabled(table_selected);
                    if(ImGui::TreeNodeEx(
                           std::string("[" + partial_metric_id + "] " + table.second.name)
                               .c_str(),
                           ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Bullet))
                    {
                        if(ImGui::BeginTable("", 5,
                                             ImGuiTableFlags_RowBg |
                                                 ImGuiTableFlags_Borders |
                                                 ImGuiTableFlags_SizingStretchSame))
                        {
                            int col = 0;
                            for(const std::pair<const uint32_t, AvailableMetrics::Entry&>&
                                    entry : table.second.entries)
                            {
                                if(col % 5 == 0)
                                {
                                    ImGui::TableNextRow();
                                }
                                ImGui::TableNextColumn();
                                bool entry_selected =
                                    m_selections.metric_ids.count(category.first) > 0 &&
                                    m_selections.metric_ids.at(category.first)
                                            .count(table.first) > 0 &&
                                    m_selections.metric_ids.at(category.first)
                                            .at(table.first)
                                            .second.count(entry.first) > 0;
                                if(ImGui::Selectable(
                                       std::string("[" + partial_metric_id + "." +
                                                   std::to_string(entry.second.id) +
                                                   "] " + entry.second.name)
                                           .c_str(),
                                       entry_selected))
                                {
                                    if(entry_selected)
                                    {
                                        if(m_selections.metric_ids.at(category.first)
                                               .at(table.first)
                                               .second.size() == 1)
                                        {
                                            if(m_selections.metric_ids.at(category.first)
                                                   .size() == 1)
                                            {
                                                m_selections.metric_ids.erase(
                                                    category.first);
                                            }
                                            else
                                            {
                                                m_selections.metric_ids.at(category.first)
                                                    .erase(table.first);
                                            }
                                        }
                                        else
                                        {
                                            m_selections.metric_ids.at(category.first)
                                                .at(table.first)
                                                .second.erase(entry.first);
                                        }
                                    }
                                    else
                                    {
                                        m_selections
                                            .metric_ids[category.first][table.first]
                                            .second.insert(entry.first);
                                    }
                                }
                                if(ImGui::BeginItemTooltip())
                                {
                                    ImGui::PushTextWrapPos(500.0f);
                                    ImGui::Text("Description: ");
                                    ImGui::SameLine();
                                    ImGui::Text(entry.second.description.c_str());
                                    ImGui::Text("Unit: ");
                                    ImGui::SameLine();
                                    ImGui::Text(entry.second.unit.c_str());
                                    ImGui::PopTextWrapPos();
                                    ImGui::EndTooltip();
                                }
                                col++;
                            }
                            ImGui::EndTable();
                        }
                        ImGui::TreePop();
                    }
                    ImGui::EndDisabled();
                    ImGui::PopID();
                }
                ImGui::TreePop();
            }
            ImGui::PopID();
        }
        ImGui::EndChild();
        ImGui::NewLine();
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
                ImGui::SameLine();
                bool kernel_selected =
                    m_selections.kernel_ids.count(kernel.second.id) > 0;
                if(ImGui::Selectable("", kernel_selected,
                                     ImGuiSelectableFlags_SpanAllColumns))
                {
                    if(kernel_selected)
                    {
                        m_selections.kernel_ids.erase(kernel.second.id);
                    }
                    else
                    {
                        m_selections.kernel_ids.insert(kernel.second.id);
                    }
                }
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
        ImGui::NewLine();
        ImGui::BeginChild("fetcher", ImVec2(0, 0),
                          ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);
        ImGui::Text("Fetcher");
        if(ImGui::BeginTable(
               "selected_metrics", 1,
               ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders |
                   ImGuiTableFlags_SizingStretchSame,
               ImVec2((ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize("for").x) /
                          2.0f,
                      0.0f)))
        {
            ImGui::TableSetupColumn("Metric ID(s)");
            ImGui::TableHeadersRow();
            if(m_selections.metric_ids.empty())
            {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextDisabled("None");
            }
            else
            {
                for(const std::pair<
                        const uint32_t,
                        std::unordered_map<
                            uint32_t, std::pair<bool, std::unordered_set<uint32_t>>>>&
                        category : m_selections.metric_ids)
                {
                    for(const std::pair<const uint32_t,
                                        std::pair<bool, std::unordered_set<uint32_t>>>&
                            table : category.second)
                    {
                        if(table.second.second.empty())
                        {
                            ImGui::TableNextRow();
                            ImGui::TableNextColumn();
                            ImGui::Text("%u.%u", category.first, table.first);
                        }
                        else
                        {
                            for(const uint32_t& entry : table.second.second)
                            {
                                ImGui::TableNextRow();
                                ImGui::TableNextColumn();
                                ImGui::Text("%u.%u.%u", category.first, table.first,
                                            entry);
                            }
                        }
                    }
                }
            }

            ImGui::EndTable();
        }
        ImGui::SameLine();
        ImGui::SetCursorPosY(ImGui::GetItemRectSize().y / 2.0f);
        ImGui::Text(" for ");
        ImGui::SameLine();
        if(ImGui::BeginTable("selected_kernels", 1,
                             ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders |
                                 ImGuiTableFlags_SizingStretchSame))
        {
            ImGui::TableSetupColumn("Kernel ID(s)");
            ImGui::TableHeadersRow();
            if(m_selections.kernel_ids.empty())
            {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextDisabled("None");
            }
            for(const uint32_t& id : m_selections.kernel_ids)
            {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text(std::to_string(id).c_str());
            }
            ImGui::EndTable();
        }
        ImGui::NewLine();
        ImGui::BeginDisabled(m_selections.metric_ids.empty() ||
                             m_selections.kernel_ids.empty());

        if(ImGui::Button("Clear all clients"))
        {
            m_data_provider.ComputeModel().ClearAllMetricValues();
        }
        ImGui::SameLine();
        uint64_t client_id = 0;
        bool do_clear = false;
        if(ImGui::Button("Clear client 1"))
        {
            client_id = 1;
            do_clear = true;
        }
        ImGui::SameLine();
        if(ImGui::Button("Clear client 2"))
        {
            client_id = 2;
            do_clear = true;
        }
        ImGui::SameLine();
        bool do_fetch = false;
        if(ImGui::Button("Fetch client 1"))
        {
            do_fetch = true;
            client_id = 1;
        }
        ImGui::SameLine();
        if(ImGui::Button("Fetch client 2"))
        {
            do_fetch = true;
            client_id = 2;
        }

        if(do_clear)
        {
            for(const uint32_t& kernel_id : m_selections.kernel_ids)
            {
                m_data_provider.ComputeModel().ClearMetricValues(client_id, kernel_id);
            }
        }
        if(do_fetch)
        {
            std::vector<uint32_t> kernel_ids;
            kernel_ids.insert(kernel_ids.end(), m_selections.kernel_ids.begin(),
                              m_selections.kernel_ids.end());
            std::vector<MetricsRequestParams::MetricID> metric_ids;
            for(const std::pair<
                    const uint32_t,
                    std::unordered_map<uint32_t,
                                       std::pair<bool, std::unordered_set<uint32_t>>>>&
                    category : m_selections.metric_ids)
            {
                for(const std::pair<const uint32_t,
                                    std::pair<bool, std::unordered_set<uint32_t>>>&
                        table : category.second)
                {
                    if(table.second.second.empty())
                    {
                        metric_ids.emplace_back(MetricsRequestParams::MetricID{
                            category.first, table.first, std::nullopt });
                    }
                    else
                    {
                        for(const uint32_t& entry : table.second.second)
                        {
                            metric_ids.emplace_back(MetricsRequestParams::MetricID{
                                category.first, table.first, entry });
                        }
                    }
                }
            }

            m_data_provider.FetchMetrics(
                MetricsRequestParams(workload.id, kernel_ids, metric_ids, client_id));
        }
        ImGui::EndDisabled();
        ImGui::NewLine();

        // loop through clients (mock client ids 1 and 2)
        for(uint64_t c_id : { 1, 2 })
        {
            ImGui::Separator();
            ImGui::Text("Client ID: %llu", c_id);
            // Display fetched metrics for selected kernels
            for(const uint32_t& kernel_id : m_selections.kernel_ids)
            {

                ImGui::Text("Metrics for Kernel ID: %u", kernel_id);
                const std::vector<std::shared_ptr<MetricValue>>* metrics =
                    m_data_provider.ComputeModel().GetMetricsData(c_id, kernel_id);
                
                if(metrics == nullptr)
                {
                    ImGui::TextDisabled("No metrics fetched for this kernel.");
                    continue;
                }
                if(ImGui::BeginTable(
                    "results", 5,
                    ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders |
                        ImGuiTableFlags_SizingStretchSame,
                    ImVec2(0.0f, metrics->empty()
                                        ? ImGui::GetTextLineHeightWithSpacing()
                                        : ImGui::GetTextLineHeightWithSpacing() *
                                            (1.0f + static_cast<float>(metrics->size())))))
                {
                    ImGui::TableSetupColumn("Metric ID");
                    ImGui::TableSetupColumn("Metric Name");
                    ImGui::TableSetupColumn("Kernel ID");
                    ImGui::TableSetupColumn("Value Name");
                    ImGui::TableSetupColumn("Value");
                    ImGui::TableHeadersRow();
                    if(metrics->empty())
                    {
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::TextDisabled("None");
                    }
                    for(const std::shared_ptr<MetricValue>& metric : *metrics)
                    {
                        if(metric && metric->entry && metric->kernel)
                        {
                            for(const std::pair<const std::string, double>& value :
                                metric->values)
                            {
                                ImGui::TableNextRow();
                                ImGui::TableNextColumn();
                                ImGui::Text("%u.%u.%u", metric->entry->category_id,
                                            metric->entry->table_id, metric->entry->id);
                                ImGui::TableNextColumn();
                                ImGui::Text(metric->entry->name.c_str());
                                ImGui::TableNextColumn();
                                ImGui::Text("%u", metric->kernel->id);
                                ImGui::TableNextColumn();
                                ImGui::Text(value.first.c_str());
                                ImGui::TableNextColumn();
                                ImGui::Text("%f", value.second);
                            }
                        }
                    }
                    ImGui::EndTable();
                }
            }
        }
        ImGui::EndChild();
        
        ImGui::NewLine();
        ImGui::BeginChild("SOL", ImVec2(0, 0),
                          ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);
        ImGui::Text("Speed of light (SOL)");

        TableKey table_key = { 2, 1 }; // SOL
        //assume client id 1 and kernel id 1 for test
        ComputeDataModel::MetricValuesByEntryId* sol_metrics =
            m_data_provider.ComputeModel().GetMetricValuesByTable(1, 1, table_key.id);
        if(!sol_metrics || sol_metrics->empty())
        {
            ImGui::TextDisabled("No SOL metrics available for client 1.");
        }
        else
        {
            if(ImGui::BeginTable("sol_table", 5,
                              ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders |
                                  ImGuiTableFlags_SizingStretchSame,
                              ImVec2(0.0f, 0.0f))) {
            ImGui::TableSetupColumn("Metric ID");
            ImGui::TableSetupColumn("Metric Name");
            ImGui::TableSetupColumn("Kernel ID");
            ImGui::TableSetupColumn("Value Name");
            ImGui::TableSetupColumn("Value");
            ImGui::TableHeadersRow();
            for(auto it = sol_metrics->begin(); it != sol_metrics->end(); ++it)
            {
                const std::shared_ptr<MetricValue>& metric = it->second;
                if(metric && metric->entry && metric->kernel)
                {
                    for(const std::pair<const std::string, double>& value :
                        metric->values)
                    {
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::Text("%u.%u.%u", metric->entry->category_id,
                                    metric->entry->table_id, metric->entry->id);
                        ImGui::TableNextColumn();
                        ImGui::Text(metric->entry->name.c_str());
                        ImGui::TableNextColumn();
                        ImGui::Text("%u", metric->kernel->id);
                        ImGui::TableNextColumn();
                        ImGui::Text(value.first.c_str());
                        ImGui::TableNextColumn();
                        ImGui::Text("%f", value.second);
                    }
                }
            }

            ImGui::EndTable();
            }
        }
        ImGui::EndChild();

        ImGui::NewLine();
        ImGui::BeginChild("roofline", ImVec2(0, 0),
                          ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);
        int i = 0;
        if(ImPlot::BeginPlot("Roofline", ImVec2(-1, 0), ImPlotFlags_Crosshairs))
        {
            ImPlot::SetupAxisScale(ImAxis_X1, ImPlotScale_Log10);
            ImPlot::SetupAxisScale(ImAxis_Y1, ImPlotScale_Log10);

            for(const std::pair<const uint32_t, KernelInfo>& kernel : workload.kernels)
            {
                for(const std::pair<
                        const rocprofvis_controller_roofline_kernel_intensity_type_t,
                        KernelInfo::Roofline::Intensity>& intensity :
                    kernel.second.roofline.intensities)
                {
                    if(m_selections.intensities.count(kernel.second.id) > 0 &&
                       m_selections.intensities.at(kernel.second.id)
                               .count(intensity.second.type) > 0)
                    {
                        ImGui::PushID(i++);
                        ImPlot::PlotScatter("", &intensity.second.position.x,
                                            &intensity.second.position.y, 1);
                        ImGui::PopID();
                    }
                }
            }
            for(const auto& compute_it : workload.roofline.ceiling_compute)
            {
                for(const auto& bandwidth_it : compute_it.second)
                {
                    if(m_selections.ceilings_compute.count(compute_it.first) > 0 &&
                       m_selections.ceilings_compute.at(compute_it.first)
                               .count(bandwidth_it.first) > 0)
                    {
                        ImGui::PushID(i++);
                        ImPlot::PlotLineG(
                            "",
                            [](int idx, void* user_data) -> ImPlotPoint {
                                const WorkloadInfo::Roofline::Line* line =
                                    static_cast<const WorkloadInfo::Roofline::Line*>(
                                        user_data);
                                ImPlotPoint point(-1.0, -1.0);
                                if(line)
                                {
                                    if(idx == 0)
                                    {
                                        point.x = line->p1.x;
                                        point.y = line->p1.y;
                                    }
                                    else
                                    {
                                        point.x = line->p2.x;
                                        point.y = line->p2.y;
                                    }
                                }
                                return point;
                            },
                            (void*) &bandwidth_it.second.position, 2);
                        ImGui::PopID();
                    }
                }
            }
            for(const auto& bandwidth_it : workload.roofline.ceiling_bandwidth)
            {
                for(const auto& compute_it : bandwidth_it.second)
                {
                    if(m_selections.ceilings_bandwidth.count(bandwidth_it.first) > 0 &&
                       m_selections.ceilings_bandwidth.at(bandwidth_it.first)
                               .count(compute_it.first) > 0)
                    {
                        ImGui::PushID(i++);
                        ImPlot::PlotLineG(
                            "",
                            [](int idx, void* user_data) -> ImPlotPoint {
                                const WorkloadInfo::Roofline::Line* line =
                                    static_cast<const WorkloadInfo::Roofline::Line*>(
                                        user_data);
                                ImPlotPoint point(-1.0, -1.0);
                                if(line)
                                {
                                    if(idx == 0)
                                    {
                                        point.x = line->p1.x;
                                        point.y = line->p1.y;
                                    }
                                    else
                                    {
                                        point.x = line->p2.x;
                                        point.y = line->p2.y;
                                    }
                                }
                                return point;
                            },
                            (void*) &compute_it.second.position, 2);
                        ImGui::PopID();
                    }
                }
            }
            ImPlot::EndPlot();
        }
        int preset_idx = static_cast<int>(m_selections.roofline_preset);
        if(m_selections.init ||
           ImGui::Combo("Presets", &preset_idx, "FP32\0FP64\0Custom\0\0"))
        {
            m_selections.roofline_preset =
                static_cast<SelectionState::RooflinePreset>(preset_idx);
            if(m_selections.roofline_preset == SelectionState::FP32)
            {
                m_selections.ceilings_compute = {
                    { kRPVControllerRooflineCeilingComputeMFMAFP32,
                      { kRPVControllerRooflineCeilingTypeBandwidthLDS } },
                    { kRPVControllerRooflineCeilingComputeVALUFP32,
                      { kRPVControllerRooflineCeilingTypeBandwidthLDS } }
                };
                m_selections.ceilings_bandwidth = {
                    { kRPVControllerRooflineCeilingTypeBandwidthHBM,
                      { kRPVControllerRooflineCeilingComputeMFMAFP32 } },
                    { kRPVControllerRooflineCeilingTypeBandwidthL2,
                      { kRPVControllerRooflineCeilingComputeMFMAFP32 } },
                    { kRPVControllerRooflineCeilingTypeBandwidthL1,
                      { kRPVControllerRooflineCeilingComputeMFMAFP32 } },
                    { kRPVControllerRooflineCeilingTypeBandwidthLDS,
                      { kRPVControllerRooflineCeilingComputeMFMAFP32 } },
                };
                for(const std::pair<const uint32_t, KernelInfo>& kernel :
                    workload.kernels)
                {
                    for(const std::pair<
                            const rocprofvis_controller_roofline_kernel_intensity_type_t,
                            KernelInfo::Roofline::Intensity>& intensity :
                        kernel.second.roofline.intensities)
                    {
                        m_selections.intensities[kernel.second.id].insert(
                            intensity.second.type);
                    }
                }
            }
            else if(m_selections.roofline_preset == SelectionState::FP64)
            {
                m_selections.ceilings_compute = {
                    { kRPVControllerRooflineCeilingComputeMFMAFP64,
                      { kRPVControllerRooflineCeilingTypeBandwidthLDS } },
                    { kRPVControllerRooflineCeilingComputeVALUFP64,
                      { kRPVControllerRooflineCeilingTypeBandwidthLDS } }
                };
                m_selections.ceilings_bandwidth = {
                    { kRPVControllerRooflineCeilingTypeBandwidthHBM,
                      { kRPVControllerRooflineCeilingComputeMFMAFP64 } },
                    { kRPVControllerRooflineCeilingTypeBandwidthL2,
                      { kRPVControllerRooflineCeilingComputeMFMAFP64 } },
                    { kRPVControllerRooflineCeilingTypeBandwidthL1,
                      { kRPVControllerRooflineCeilingComputeMFMAFP64 } },
                    { kRPVControllerRooflineCeilingTypeBandwidthLDS,
                      { kRPVControllerRooflineCeilingComputeMFMAFP64 } },
                };
                for(const std::pair<const uint32_t, KernelInfo>& kernel :
                    workload.kernels)
                {
                    for(const std::pair<
                            const rocprofvis_controller_roofline_kernel_intensity_type_t,
                            KernelInfo::Roofline::Intensity>& intensity :
                        kernel.second.roofline.intensities)
                    {
                        m_selections.intensities[kernel.second.id].insert(
                            intensity.second.type);
                    }
                }
            }
        }
        if(m_selections.roofline_preset == SelectionState::Custom &&
           ImGui::BeginTable("customizer", 2,
                             ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders |
                                 ImGuiTableFlags_SizingStretchSame))
        {
            ImGui::TableSetupColumn("Type");
            ImGui::TableSetupColumn("Element");
            ImGui::TableHeadersRow();
            for(const std::pair<const uint32_t, KernelInfo>& kernel : workload.kernels)
            {
                for(const std::pair<
                        const rocprofvis_controller_roofline_kernel_intensity_type_t,
                        KernelInfo::Roofline::Intensity>& intensity :
                    kernel.second.roofline.intensities)
                {
                    ImGui::PushID(i++);
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::Text("Kernel Intensity");
                    ImGui::SameLine();
                    bool show = m_selections.intensities.count(kernel.second.id) > 0 &&
                                m_selections.intensities.at(kernel.second.id)
                                        .count(intensity.second.type) > 0;
                    if(ImGui::Selectable("", show, ImGuiSelectableFlags_SpanAllColumns))
                    {
                        if(show)
                        {
                            m_selections.intensities.at(kernel.second.id)
                                .erase(intensity.second.type);
                        }
                        else
                        {
                            m_selections.intensities[kernel.second.id].insert(
                                intensity.second.type);
                        }
                    }
                    ImGui::TableNextColumn();
                    ImGui::Text("ID:%u - %s", kernel.second.id,
                                m_display_names.intensity.at(intensity.second.type));
                    ImGui::PopID();
                }
            }
            for(const auto& compute_it : workload.roofline.ceiling_compute)
            {
                for(const auto& bandwidth_it : compute_it.second)
                {
                    ImGui::PushID(i++);
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::Text("Compute Ceiling");
                    ImGui::SameLine();
                    bool show =
                        m_selections.ceilings_compute.count(compute_it.first) > 0 &&
                        m_selections.ceilings_compute.at(compute_it.first)
                                .count(bandwidth_it.first) > 0;
                    if(ImGui::Selectable("", show, ImGuiSelectableFlags_SpanAllColumns))
                    {
                        if(show)
                        {
                            m_selections.ceilings_compute.at(compute_it.first)
                                .erase(bandwidth_it.first);
                        }
                        else
                        {
                            m_selections.ceilings_compute[compute_it.first].insert(
                                bandwidth_it.first);
                        }
                    }
                    ImGui::TableNextColumn();
                    ImGui::Text("%s-%s",
                                m_display_names.ceiling_compute.at(compute_it.first),
                                m_display_names.ceiling_bandwidth.at(bandwidth_it.first));
                    ImGui::PopID();
                }
            }
            for(const auto& bandwidth_it : workload.roofline.ceiling_bandwidth)
            {
                for(const auto& compute_it : bandwidth_it.second)
                {
                    ImGui::PushID(i++);
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::Text("Bandwidth Ceiling");
                    ImGui::SameLine();
                    bool show =
                        m_selections.ceilings_bandwidth.count(bandwidth_it.first) > 0 &&
                        m_selections.ceilings_bandwidth.at(bandwidth_it.first)
                                .count(compute_it.first) > 0;
                    if(ImGui::Selectable("", show, ImGuiSelectableFlags_SpanAllColumns))
                    {
                        if(show)
                        {
                            m_selections.ceilings_bandwidth.at(bandwidth_it.first)
                                .erase(compute_it.first);
                        }
                        else
                        {
                            m_selections.ceilings_bandwidth[bandwidth_it.first].insert(
                                compute_it.first);
                        }
                    }
                    ImGui::TableNextColumn();
                    ImGui::Text("%s-%s",
                                m_display_names.ceiling_bandwidth.at(bandwidth_it.first),
                                m_display_names.ceiling_compute.at(compute_it.first));
                    ImGui::PopID();
                }
            }
            ImGui::EndTable();
        }
        ImGui::EndChild();
        ImGui::NewLine();
        ImGui::BeginChild("value_names_lookup", ImVec2(0, 0),
                          ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);
        ImGui::Text("Value Names Lookup");
        ImGui::InputText("Metric ID (e.g. 3.1.2)", m_value_names_input,
                         sizeof(m_value_names_input));

        const WorkloadInfo& wl = workloads.at(m_selections.workload_id);
        ImGui::Text("Workload: %s (metrics: %zu, categories: %zu)",
                    wl.name.c_str(),
                    wl.available_metrics.list.size(),
                    wl.available_metrics.tree.size());

        std::string input(m_value_names_input);
        auto        dot1 = input.find('.');
        auto        dot2 = (dot1 != std::string::npos) ? input.find('.', dot1 + 1)
                                                        : std::string::npos;
        if(dot1 != std::string::npos && dot2 != std::string::npos &&
           dot2 + 1 < input.size())
        {
            uint32_t cat_id   = static_cast<uint32_t>(std::stoul(input.substr(0, dot1)));
            uint32_t tbl_id   = static_cast<uint32_t>(
                std::stoul(input.substr(dot1 + 1, dot2 - dot1 - 1)));
            uint32_t entry_id = static_cast<uint32_t>(
                std::stoul(input.substr(dot2 + 1)));

            ImGui::Text("Looking up: cat=%u, table=%u, entry=%u", cat_id, tbl_id, entry_id);

            if(wl.available_metrics.tree.count(cat_id))
            {
                const auto& cat = wl.available_metrics.tree.at(cat_id);
                if(cat.tables.count(tbl_id))
                {
                    const auto& tbl = cat.tables.at(tbl_id);
                    if(tbl.entries.count(entry_id))
                    {
                        const AvailableMetrics::Entry& entry = tbl.entries.at(entry_id);
                        ImGui::Text("Metric: [%u.%u.%u] %s",
                                    entry.category_id, entry.table_id, entry.id,
                                    entry.name.c_str());
                    }
                    else
                    {
                        ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1),
                                           "Entry %u not found in table %u (table has %zu entries)",
                                           entry_id, tbl_id, tbl.entries.size());
                    }
                    if(tbl.value_names.empty())
                    {
                        ImGui::TextDisabled("No value names found for table %u", tbl_id);
                    }
                    else
                    {
                        ImGui::Text("Value Names (%zu):", tbl.value_names.size());
                        for(const std::string& vn : tbl.value_names)
                        {
                            ImGui::BulletText("%s", vn.c_str());
                        }
                    }
                }
                else
                {
                    ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1),
                                       "Table %u not found in category %u", tbl_id, cat_id);
                }
            }
            else
            {
                ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1),
                                   "Category %u not found", cat_id);
            }
        }
        ImGui::EndChild();
        ImGui::EndChild();
        m_selections.init = false;
    }

    RenderKernelSelectionTable();
}

void
ComputeTester::RenderKernelSelectionTable()
{
    static std::vector<std::string> metrics = {
        "2.1.4:avg", "2.1.3:avg", "2.1.3:peak"
    };

    static char metric_id_buffer[256] = "2.1.4:avg";
    static bool fetch_requested        = true; // force fetch on first render

    static bool sort_requested    = false;
    static int  sort_column_index = -1;
    static int  sort_order        = kRPVControllerSortOrderDescending;

    static const int built_in_columns_cnt = 2; // ID and Name columns that are always present in the table

    int remove_index = -1;

    if(ImGui::Button("Fetch Kernel Metric Pivot Test"))
    {
        fetch_requested = true;
    }

    ImGui::InputText("Metric ID:", metric_id_buffer, sizeof(metric_id_buffer));
    
    if(ImGui::Button("Add Metric"))
    {
        metrics.push_back(std::string(metric_id_buffer));
        fetch_requested = true;
    }

    ImGui::Separator();

    ComputeKernelSelectionTable& table =
        m_data_provider.ComputeModel().GetKernelSelectionTable();
    const std::vector<std::string>&              header = table.GetTableHeader();
    const std::vector<std::vector<std::string>>& data   = table.GetTableData();

    if(!header.empty() && !data.empty())
    {
        if(ImGui::BeginTable("kernel_selection_table", static_cast<int>(header.size()),
                            ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders |
                                ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_Sortable,
                            ImVec2(0.0f, data.empty()
                                            ? ImGui::GetTextLineHeightWithSpacing()
                                            : ImGui::GetTextLineHeightWithSpacing() *
                                                    (1.0f + data.size()))))
        {

            for(const std::string& col_name : header)
            {
                ImGui::TableSetupColumn(col_name.c_str()); 
            }  
            ImGui::TableHeadersRow();
            if(data.empty())
            {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextDisabled("None");
            }

            // Get sort specs
            ImGuiTableSortSpecs* sort_specs = ImGui::TableGetSortSpecs();
            if(sort_specs && sort_specs->SpecsDirty)
            {
                sort_requested    = true;
                sort_column_index = sort_specs->Specs->ColumnIndex;
                sort_order =
                    (sort_specs->Specs->SortDirection == ImGuiSortDirection_Ascending)
                        ? kRPVControllerSortOrderAscending
                        : kRPVControllerSortOrderDescending;

                sort_specs->SpecsDirty = false;
            }

            for(const std::vector<std::string>& row : data)
            {
                int index = 0;
                ImGui::TableNextRow();
                for(const std::string& cell : row)
                {
                    ImGui::TableNextColumn();
                    ImGui::Text(cell.c_str());

                    if(ImGui::IsItemClicked(ImGuiMouseButton_Right))
                    {
                        remove_index   = index - built_in_columns_cnt;
                        sort_requested = true;
                        spdlog::debug("Clicked cell at row {}, column {}: {} {}",
                                      index / header.size(), index % header.size(), cell, remove_index);
                    }
                    index++;                    
                }
            }
            ImGui::EndTable();
        }
    }

    if(remove_index >= 0 && remove_index < static_cast<int>(metrics.size()))
    {
        metrics.erase(metrics.begin() + remove_index);
    }

    if(sort_requested || fetch_requested)
    {
        ComputeTableRequestParams params(1, metrics);
        
        params.m_sort_column_index = sort_column_index;
        params.m_sort_order        = static_cast<rocprofvis_controller_sort_order_t>(sort_order);

        spdlog::debug("Requesting sorted kernel selection table: column {}, order {}",
                      sort_column_index, sort_order == kRPVControllerSortOrderAscending ? "ASC" : "DESC");  
        m_data_provider.FetchMetricPivotTable(params);

        fetch_requested = false;
        sort_requested  = false;
    }
}

}  // namespace View
}  // namespace RocProfVis
