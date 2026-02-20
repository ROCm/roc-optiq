// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_compute_table_view.h"
#include "model/compute/rocprofvis_compute_data_model.h"

namespace RocProfVis
{
namespace View
{

ComputeTableView::ComputeTableView(DataProvider& data_provider)
: RocWidget()
, m_data_provider(data_provider)
{}

void
ComputeTableView::Update()
{
    if(m_tabs) m_tabs->Update();
}

void
ComputeTableView::Render()
{
    const auto& workloads = m_data_provider.ComputeModel().GetWorkloads();

    ImGui::SetNextItemWidth(ImGui::GetFrameHeight() * 15.0f);
    const char* wl_preview =
        workloads.count(m_workload_id) ? workloads.at(m_workload_id).name.c_str() : "-";
    if(ImGui::BeginCombo("Workload", wl_preview))
    {
        if(ImGui::Selectable("-", m_workload_id == 0))
        {
            m_workload_id = 0;
            m_kernel_ids.clear();
        }
        for(const auto& wp : workloads)
            if(ImGui::Selectable(wp.second.name.c_str(), m_workload_id == wp.first))
            {
                m_workload_id = wp.first;
                m_kernel_ids.clear();
            }
        ImGui::EndCombo();
    }

    if(!workloads.count(m_workload_id)) return;
    const auto& workload = workloads.at(m_workload_id);

    ImGui::SameLine();
    ImGui::SetNextItemWidth(ImGui::GetFrameHeight() * 15.0f);
    std::string k_preview = m_kernel_ids.empty()
                                ? "Select Kernels"
                                : std::to_string(m_kernel_ids.size()) + " kernel(s)";
    if(ImGui::BeginCombo("Kernels", k_preview.c_str()))
    {
        for(const auto& kp : workload.kernels)
        {
            bool sel = m_kernel_ids.count(kp.first);
            if(ImGui::Checkbox(kp.second.name.c_str(), &sel))
            {
                if(sel)
                    m_kernel_ids.insert(kp.first);
                else
                    m_kernel_ids.erase(kp.first);
            }
        }
        ImGui::EndCombo();
    }

    ImGui::SameLine();
    ImGui::BeginDisabled(m_kernel_ids.empty());
    if(ImGui::Button("Fetch All Metrics"))
    {
        m_data_provider.ComputeModel().ClearMetricValues();
        std::vector<uint32_t> kernel_ids(m_kernel_ids.begin(), m_kernel_ids.end());
        std::vector<MetricsRequestParams::MetricID> metric_ids;
        for(const auto& cp : workload.available_metrics.tree)
            for(const auto& tp : cp.second.tables)
                metric_ids.push_back({cp.first, tp.first, std::nullopt});
        m_data_provider.FetchMetrics(MetricsRequestParams(workload.id, kernel_ids, metric_ids));
    }
    ImGui::EndDisabled();

    if(m_last_workload_id != m_workload_id)
    {
        m_tabs = std::make_shared<TabContainer>();
        for(const auto& cp : workload.available_metrics.tree)
        {
            const auto& cat    = cp.second;
            auto        widget = std::make_shared<RocCustomWidget>(
                [this, &cat]() { RenderCategory(cat); });
            m_tabs->AddTab({ cat.name, std::to_string(cp.first), widget, false });
        }
        m_last_workload_id = m_workload_id;
    }

    if(m_tabs) m_tabs->Render();
}

void
ComputeTableView::RenderCategory(const AvailableMetrics::Category& cat)
{
    const auto& metrics = m_data_provider.ComputeModel().GetMetricsData();

    ImGui::BeginChild("scroll", ImVec2(-1, -1));
    for(const auto& tbl_pair : cat.tables)
    {
        uint32_t    tbl_id = tbl_pair.first;
        const auto& tbl    = tbl_pair.second;

        int num_columns = 1 + static_cast<int>(tbl.value_names.size());
        if(num_columns < 2) num_columns = 2;

        ImGui::SeparatorText(tbl.name.c_str());
        if(ImGui::BeginTable("##t", num_columns, ImGuiTableFlags_Borders))
        {
            ImGui::TableSetupColumn("Metric");
            if(tbl.value_names.empty())
            {
                ImGui::TableSetupColumn("Value");
            }
            else
            {
                for(const auto& vn : tbl.value_names)
                    ImGui::TableSetupColumn(vn.c_str());
            }
            ImGui::TableHeadersRow();

            for(const auto& entry_pair : tbl.entries)
            {
                uint32_t    eid   = entry_pair.first;
                const auto& entry = entry_pair.second;

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(entry.name.c_str());

                if(tbl.value_names.empty())
                {
                    ImGui::TableNextColumn();
                    for(const auto& mv : metrics)
                        if(mv && mv->entry.category_id == cat.id &&
                           mv->entry.table_id == tbl_id && mv->entry.id == eid)
                        {
                            ImGui::Text("%.2f", mv->value);
                            break;
                        }
                }
                else
                {
                    for(const auto& vn : tbl.value_names)
                    {
                        ImGui::TableNextColumn();
                        for(const auto& mv : metrics)
                            if(mv && mv->entry.category_id == cat.id &&
                               mv->entry.table_id == tbl_id && mv->entry.id == eid &&
                               mv->name == vn)
                            {
                                ImGui::Text("%.2f", mv->value);
                                break;
                            }
                    }
                }
            }
            ImGui::EndTable();
        }
    }
    ImGui::EndChild();
}

}  // namespace View
}  // namespace RocProfVis
