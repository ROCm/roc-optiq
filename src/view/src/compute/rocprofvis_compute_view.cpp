// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_compute_view.h"
#include "rocprofvis_settings_manager.h"

namespace RocProfVis
{
namespace View
{

ComputeView::ComputeView()
: m_view_created(false)
, m_tester(nullptr)
{
    m_tool_bar = std::make_shared<RocCustomWidget>([this]() { this->RenderToolbar(); });
    m_widget_name = GenUniqueName("ComputeView");
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
        if(m_tester)
        {
            m_tester->Update();
        }
    }
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
        if(m_tester)
        {
            m_tester->Render();
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

    ImGui::Button("Compute Toolbar", ImVec2(-1, 0));

    // pop content style
    ImGui::PopStyleVar(2);
    ImGui::EndChild();
    // pop child window style
    ImGui::PopStyleVar(2);
}

ComputeTester::ComputeTester(DataProvider& data_provider)
: m_data_provider(data_provider)
, m_selections({ 0, {}, {} })
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
    ImGui::SetNextItemWidth(ImGui::GetFrameHeight() * 15.0f);
    if(ImGui::BeginCombo("Workloads",
                         workloads.count(m_selections.workload_id)
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
    if(workloads.count(m_selections.workload_id))
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
            if(ImGui::BeginTable("", workload.system_info.size(),
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
            if(ImGui::BeginTable("", workload.profiling_config.size(),
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
            ImGui::PushID(category.first);
            if(ImGui::TreeNodeEx(std::string("[" + std::to_string(category.second.id) +
                                             "] " + category.second.name)
                                     .c_str()))
            {
                for(const std::pair<const uint32_t, AvailableMetrics::Table>& table :
                    category.second.tables)
                {
                    ImGui::PushID(table.first);
                    std::string partial_metric_id = std::to_string(category.second.id) +
                                                    "." + std::to_string(table.second.id);
                    bool table_selected =
                        m_selections.metric_ids.count(category.first) &&
                        m_selections.metric_ids.at(category.first).count(table.first) &&
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
                                    m_selections.metric_ids.count(category.first) &&
                                    m_selections.metric_ids.at(category.first)
                                        .count(table.first) &&
                                    m_selections.metric_ids.at(category.first)
                                        .at(table.first)
                                        .second.count(entry.first);
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
                ImGui::PushID(kernel.second.id);
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("%u", kernel.second.id);
                ImGui::SameLine();
                bool kernel_selected = m_selections.kernel_ids.count(kernel.second.id);
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
        if(ImGui::Button("Fetch", ImVec2(-1.0f, 0.0f)))
        {
            m_data_provider.ComputeModel().ClearMetricValues();
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
                MetricsRequestParams(workload.id, kernel_ids, metric_ids));
        }
        ImGui::EndDisabled();
        ImGui::NewLine();
        const std::vector<std::unique_ptr<MetricValue>>& metrics =
            m_data_provider.ComputeModel().GetMetricsData();
        if(ImGui::BeginTable("results", 5,
                             ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders |
                                 ImGuiTableFlags_SizingStretchSame,
                             ImVec2(0.0f, metrics.empty()
                                              ? ImGui::GetTextLineHeightWithSpacing()
                                              : ImGui::GetTextLineHeightWithSpacing() *
                                                    (1.0f + metrics.size()))))
        {
            ImGui::TableSetupColumn("Metric ID");
            ImGui::TableSetupColumn("Metric Name");
            ImGui::TableSetupColumn("Kernel ID");
            ImGui::TableSetupColumn("Value Name");
            ImGui::TableSetupColumn("Value");
            ImGui::TableHeadersRow();
            if(metrics.empty())
            {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextDisabled("None");
            }
            for(const std::unique_ptr<MetricValue>& metric : metrics)
            {
                if(metric)
                {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::Text("%u.%u.%u", metric->entry.category_id,
                                metric->entry.table_id, metric->entry.id);
                    ImGui::TableNextColumn();
                    ImGui::Text(metric->entry.name.c_str());
                    ImGui::TableNextColumn();
                    ImGui::Text("%u", metric->kernel.id);
                    ImGui::TableNextColumn();
                    ImGui::Text(metric->name.c_str());
                    ImGui::TableNextColumn();
                    ImGui::Text("%f", metric->value);
                }
            }
            ImGui::EndTable();
        }
        ImGui::EndChild();
        ImGui::EndChild();
    }
}

}  // namespace View
}  // namespace RocProfVis
