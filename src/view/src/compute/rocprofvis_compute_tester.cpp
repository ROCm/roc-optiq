// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_compute_tester.h"
#include "rocprofvis_event_manager.h"
#include "implot/implot.h"
#include "spdlog/spdlog.h"

namespace RocProfVis
{
namespace View
{

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
        m_query_builder.Show(
            [this](const std::string& query) {
                // Handle query confirmation if needed
                spdlog::debug("Query confirmed: {}", query);
            });
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
        for(const auto* cat : workload.available_metrics.ordered_categories)
        {
            ImGui::PushID(static_cast<int>(cat->id));
            if(ImGui::TreeNodeEx(std::string("[" + std::to_string(cat->id) +
                                             "] " + cat->name)
                                     .c_str()))
            {
                for(const auto* tbl : cat->ordered_tables)
                {
                    ImGui::PushID(static_cast<int>(tbl->id));
                    std::string partial_metric_id = std::to_string(cat->id) +
                                                    "." + std::to_string(tbl->id);
                    bool table_selected =
                        m_selections.metric_ids.count(cat->id) > 0 &&
                        m_selections.metric_ids.at(cat->id).count(tbl->id) >
                            0 &&
                        m_selections.metric_ids.at(cat->id).at(tbl->id).first;
                    bool table_selected_changed = ImGui::Selectable("", table_selected);
                    if(table_selected_changed)
                    {
                        if(table_selected)
                        {
                            if(m_selections.metric_ids.at(cat->id).size() == 1)
                            {
                                m_selections.metric_ids.erase(cat->id);
                            }
                            else
                            {
                                m_selections.metric_ids.at(cat->id)
                                    .erase(tbl->id);
                            }
                        }
                        else
                        {
                            m_selections.metric_ids[cat->id][tbl->id] = { true,
                                                                          {} };
                        }
                    }
                    ImGui::SameLine(0.0f);
                    ImGui::BeginDisabled(table_selected);
                    if(ImGui::TreeNodeEx(
                           std::string("[" + partial_metric_id + "] " + tbl->name)
                               .c_str(),
                           ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Bullet))
                    {
                        if(ImGui::BeginTable("", 5,
                                             ImGuiTableFlags_RowBg |
                                                 ImGuiTableFlags_Borders |
                                                 ImGuiTableFlags_SizingStretchSame))
                        {
                            int col = 0;
                            for(const auto* entry : tbl->ordered_entries)
                            {
                                if(col % 5 == 0)
                                {
                                    ImGui::TableNextRow();
                                }
                                ImGui::TableNextColumn();
                                bool entry_selected =
                                    m_selections.metric_ids.count(cat->id) > 0 &&
                                    m_selections.metric_ids.at(cat->id)
                                            .count(tbl->id) > 0 &&
                                    m_selections.metric_ids.at(cat->id)
                                            .at(tbl->id)
                                            .second.count(entry->id) > 0;
                                if(ImGui::Selectable(
                                       std::string("[" + partial_metric_id + "." +
                                                   std::to_string(entry->id) +
                                                   "] " + entry->name)
                                           .c_str(),
                                       entry_selected))
                                {
                                    if(entry_selected)
                                    {
                                        if(m_selections.metric_ids.at(cat->id)
                                               .at(tbl->id)
                                               .second.size() == 1)
                                        {
                                            if(m_selections.metric_ids.at(cat->id)
                                                   .size() == 1)
                                            {
                                                m_selections.metric_ids.erase(
                                                    cat->id);
                                            }
                                            else
                                            {
                                                m_selections.metric_ids.at(cat->id)
                                                    .erase(tbl->id);
                                            }
                                        }
                                        else
                                        {
                                            m_selections.metric_ids.at(cat->id)
                                                .at(tbl->id)
                                                .second.erase(entry->id);
                                        }
                                    }
                                    else
                                    {
                                        m_selections
                                            .metric_ids[cat->id][tbl->id]
                                            .second.insert(entry->id);
                                    }
                                }
                                if(ImGui::BeginItemTooltip())
                                {
                                    ImGui::PushTextWrapPos(500.0f);
                                    ImGui::Text("Description: ");
                                    ImGui::SameLine();
                                    ImGui::Text(entry->description.c_str());
                                    ImGui::Text("Unit: ");
                                    ImGui::SameLine();
                                    ImGui::Text(entry->unit.c_str());
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
                ImGui::Text("%llu", kernel.second.dispatch_metrics[KernelInfo::InvocationCount]);
                ImGui::TableNextColumn();
                ImGui::Text("%llu", kernel.second.dispatch_metrics[KernelInfo::DurationTotal]);
                ImGui::TableNextColumn();
                ImGui::Text("%llu", kernel.second.dispatch_metrics[KernelInfo::DurationMin]);
                ImGui::TableNextColumn();
                ImGui::Text("%llu", kernel.second.dispatch_metrics[KernelInfo::DurationMax]);
                ImGui::TableNextColumn();
                ImGui::Text("%llu", kernel.second.dispatch_metrics[KernelInfo::DurationMean]);
                ImGui::TableNextColumn();
                ImGui::Text("%llu", kernel.second.dispatch_metrics[KernelInfo::DurationMedian]);
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
        ImGui::BeginDisabled(m_selections.metric_ids.empty());

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
                m_data_provider.ComputeModel().ClearKernelMetricValues(client_id, kernel_id);
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
                    m_data_provider.ComputeModel().GetKernelMetricsData(c_id, kernel_id);
                
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
            m_data_provider.ComputeModel().GetKernelMetricValuesByTable(1, 1, table_key.id);
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
}

}  // namespace View
}  // namespace RocProfVis
