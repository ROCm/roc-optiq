// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_compute_view.h"
#include "rocprofvis_settings_manager.h"
#include "widgets/rocprofvis_tab_container.h"
#include <unordered_set>

namespace RocProfVis
{
namespace View
{

 

// Shared state
static uint32_t                      s_workload_id = 0;
static std::unordered_set<uint32_t>  s_kernel_ids;
static std::shared_ptr<TabContainer> s_tabs;
 
ComputeView::ComputeView()
: m_view_created(false)
{
    m_tool_bar    = std::make_shared<RocCustomWidget>([this]() { RenderToolbar(); });
    m_widget_name = GenUniqueName("ComputeView");
}

ComputeView::~ComputeView() {}

void
ComputeView::Update()
{
    m_data_provider.Update();
    if(!m_view_created)
    {
        CreateView();
        m_view_created = true;
    }
    if(m_data_provider.GetState() == ProviderState::kReady && m_tester)
        m_tester->Update();
}

void
ComputeView::CreateView()
{
    m_tester = std::make_unique<ComputeTester>(m_data_provider);
}
void
ComputeView::DestroyView()
{
    m_view_created = false;
}

bool
ComputeView::LoadTrace(rocprofvis_controller_t* controller, const std::string& file_path)
{
    return m_data_provider.FetchTrace(controller, file_path);
}

void
ComputeView::Render()
{
    if(m_data_provider.GetState() == ProviderState::kLoading)
        RenderLoadingScreen(m_data_provider.GetProgressMessage());
    else if(m_tester)
        m_tester->Render();
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
    ImGuiStyle& style = ImGui::GetStyle();
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 4));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
    ImGui::BeginChild("Toolbar", ImVec2(-1, 0),
                      ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_FrameStyle);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, style.FramePadding);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, style.FrameRounding);
    ImGui::Button("Compute Toolbar", ImVec2(-1, 0));
    ImGui::PopStyleVar(4);
    ImGui::EndChild();
}

 

ComputeTester::ComputeTester(DataProvider& data_provider)
: m_data_provider(data_provider)
{
    m_widget_name = GenUniqueName("ComputeTester");
}

ComputeTester::~ComputeTester() {}

void
ComputeTester::Update()
{
    if(s_tabs) s_tabs->Update();
}

void
ComputeTester::Render()
{
    const auto& workloads = m_data_provider.ComputeModel().GetWorkloads();

    // Workload dropdown
    ImGui::SetNextItemWidth(ImGui::GetFrameHeight() * 15.0f);
    const char* wl_preview =
        workloads.count(s_workload_id) ? workloads.at(s_workload_id).name.c_str() : "-";
    if(ImGui::BeginCombo("Workload", wl_preview))
    {
        if(ImGui::Selectable("-", s_workload_id == 0))
        {
            s_workload_id = 0;
            s_kernel_ids.clear();
        }
        for(const auto& wp : workloads)
            if(ImGui::Selectable(wp.second.name.c_str(), s_workload_id == wp.first))
            {
                s_workload_id = wp.first;
                s_kernel_ids.clear();
            }
        ImGui::EndCombo();
    }

    if(!workloads.count(s_workload_id)) return;
    const auto& workload = workloads.at(s_workload_id);

    // Kernel dropdown
    ImGui::SameLine();
    ImGui::SetNextItemWidth(ImGui::GetFrameHeight() * 15.0f);
    std::string k_preview = s_kernel_ids.empty()
                                ? "Select Kernels"
                                : std::to_string(s_kernel_ids.size()) + " kernel(s)";
    if(ImGui::BeginCombo("Kernels", k_preview.c_str()))
    {
        for(const auto& kp : workload.kernels)
        {
            bool sel = s_kernel_ids.count(kp.first);
            if(ImGui::Checkbox(kp.second.name.c_str(), &sel))
            {
                if(sel)
                    s_kernel_ids.insert(kp.first);
                else
                    s_kernel_ids.erase(kp.first);
            }
        }
        ImGui::EndCombo();
    }

    // Fetch button
    ImGui::SameLine();
    ImGui::BeginDisabled(s_kernel_ids.empty());
    if(ImGui::Button("Fetch All Metrics"))
    {
        m_data_provider.ComputeModel().ClearMetricValues();
        std::vector<uint32_t> kernel_ids(s_kernel_ids.begin(), s_kernel_ids.end());
        std::vector<MetricsRequestParams::MetricID> metric_ids;
        // Fetch all metrics for all categories/tables
        for(const auto& cp : workload.available_metrics.tree)
            for(const auto& tp : cp.second.tables)
                metric_ids.push_back({cp.first, tp.first, std::nullopt});
        m_data_provider.FetchMetrics(MetricsRequestParams(workload.id, kernel_ids, metric_ids));
    }
    ImGui::EndDisabled();

    // Rebuild tabs on workload change
    static uint32_t last_id = 0;
    if(last_id != s_workload_id)
    {
        s_tabs = std::make_shared<TabContainer>();
        for(const auto& cp : workload.available_metrics.tree)
        {
            const auto& cat    = cp.second;
            auto        widget = std::make_shared<RocCustomWidget>(
                [this, &cat]() { RenderCategory(cat); });
            s_tabs->AddTab({ cat.name, std::to_string(cp.first), widget, false });
        }
        last_id = s_workload_id;
    }

    if(s_tabs) s_tabs->Render();
}

void
ComputeTester::RenderCategory(const AvailableMetrics::Category& cat)
{
    const auto& metrics = m_data_provider.ComputeModel().GetMetricsData();

    ImGui::BeginChild("scroll", ImVec2(-1, -1));
    for(const auto& tbl_pair : cat.tables)
    {
        uint32_t    tbl_id = tbl_pair.first;
        const auto& tbl    = tbl_pair.second;

        ImGui::SeparatorText(tbl.name.c_str());
        if(ImGui::BeginTable("##t", 4, ImGuiTableFlags_Borders))
        {
            ImGui::TableSetupColumn("Metric");
            ImGui::TableSetupColumn("Avg");
            ImGui::TableSetupColumn("Min");
            ImGui::TableSetupColumn("Max");
            ImGui::TableHeadersRow();

            for(const auto& entry_pair : tbl.entries)
            {
                uint32_t    eid   = entry_pair.first;
                const auto& entry = entry_pair.second;

                double val   = 0;
                bool   found = false;
                for(const auto& mv : metrics)
                    if(mv && mv->entry.category_id == cat.id &&
                       mv->entry.table_id == tbl_id && mv->entry.id == eid)
                    {
                        val   = mv->value;
                        found = true;
                        break;
                    }

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(entry.name.c_str());
                ImGui::TableNextColumn();
                if(found)
                {
                  
                    ImGui::Text("%.2f", val);
                }
                ImGui::TableNextColumn();
                if(found) ImGui::Text("%.2f", val);
                ImGui::TableNextColumn();
                if(found) ImGui::Text("%.2f", val);
            }
            ImGui::EndTable();
        }
    }
    ImGui::EndChild();
}

}  // namespace View
}  // namespace RocProfVis
