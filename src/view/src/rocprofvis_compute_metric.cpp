// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_compute_metric.h"

namespace RocProfVis
{
namespace View
{

constexpr ImVec2 ITEM_SPACING_DEFAULT = ImVec2(8, 4);
constexpr ImPlotColormap PLOT_COLOR_MAP = ImPlotColormap_Plasma;
constexpr double PLOT_LABEL_POSITION[20] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19};
constexpr float PLOT_COLOR_MAP_WIDTH = 0.1f;
constexpr double PLOT_BAR_SIZE = 0.67;
constexpr int TABLE_THRESHOLD_HIGH = 80;
constexpr int TABLE_THRESHOLD_MID = 50;
constexpr ImU32 TABLE_COLOR_HIGH = IM_COL32(255, 18, 10, 255);
constexpr ImU32 TABLE_COLOR_MID = IM_COL32(255, 169, 10, 255);

const std::string ComputeMetric::FormattedString()
{
    return m_formatted_string;
}

void ComputeMetric::Update()
{
    if (m_data_provider)
    {
        m_metric_data = m_data_provider->GetMetric(m_metric_category, m_metric_id);
    }
    if (m_metric_data)
    {
        if (!m_metric_data->m_unit.empty())
        {
            m_formatted_string = std::string(m_display_name).append(": ").append(std::to_string(static_cast<int>(std::round(m_metric_data->m_value))) + " " + m_metric_data->m_unit);
            size_t pct_pos = m_formatted_string.find("%");
            if (pct_pos != std::string::npos)
            {
                m_formatted_string.replace(pct_pos, 1, "%%");
            }  
        }
        else
        {
            m_formatted_string = std::string(m_display_name).append(": ").append(std::to_string(static_cast<int>(std::round(m_metric_data->m_value))));            
        }
    }
}

ComputeMetric::ComputeMetric(std::shared_ptr<ComputeDataProvider> data_provider, std::string metric_category, std::string metric_id, std::string display_name)
: m_data_provider(data_provider)
, m_metric_category(metric_category)
, m_metric_id(metric_id)
, m_metric_data(nullptr)
, m_display_name(display_name)
{}

ComputeMetric::~ComputeMetric() {}

ComputeMetricGroup::ComputeMetricGroup(std::shared_ptr<ComputeDataProvider> data_provider, std::string metric_category, 
                             compute_metric_group_render_flags_t render_flags, std::vector<int> pie_data_index, std::vector<int> bar_data_index)
: m_data_provider(data_provider)
, m_metric_category(metric_category)
, m_render_flags(render_flags)
, m_metric_data(nullptr)
, m_pie_data_index(pie_data_index)
, m_bar_data_index(bar_data_index)
{}

ComputeMetricGroup::~ComputeMetricGroup() {}

void ComputeMetricGroup::Update()
{
    if (m_data_provider)
    {
        m_metric_data = m_data_provider->GetMetricGroup(m_metric_category);
    }
}

void ComputeMetricGroup::Render()
{
	if (m_metric_data)
    {
        if (m_render_flags & kComputeMetricPie)
        {
            for (int& i : m_pie_data_index)
            {
                if (i < m_metric_data->m_plots.size())
                {
                    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ITEM_SPACING_DEFAULT);
                    ImGui::Separator();
                    ImGui::PopStyleVar();
                    if (ImPlot::BeginPlot(m_metric_data->m_plots[i].m_title.c_str(), ImVec2(-1, 0), ImPlotFlags_Equal | ImPlotFlags_NoInputs)) {
                        ImPlot::SetupAxes(nullptr, nullptr, ImPlotAxisFlags_NoDecorations, ImPlotAxisFlags_NoDecorations);
                        ImPlot::SetupAxesLimits(0, 1, 0, 1);
                        ImPlot::PlotPieChart(m_metric_data->m_plots[i].m_series_names.data(), m_metric_data->m_plots[i].m_values.data(), m_metric_data->m_plots[i].m_values.size(), 0.5, 0.5, 0.1, "%.2f", 90, ImPlotPieChartFlags_None);
                        ImPlot::EndPlot();
                    }
                }
            }
        }
        if (m_render_flags & kComputeMetricBar)
        {
            for (int& i : m_bar_data_index)
            {
                if (i < m_metric_data->m_plots.size())
                {
                    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ITEM_SPACING_DEFAULT);
                    ImGui::Separator();
                    ImGui::PopStyleVar();

                    ImVec2 content_region = ImGui::GetContentRegionAvail();
                    const char* title = m_metric_data->m_plots[i].m_title.c_str();
                    const char* x_label = m_metric_data->m_plots[i].m_x_axis.m_label.c_str();
                    const char* y_label = m_metric_data->m_plots[i].m_y_axis.m_label.c_str();
                    const float& x_min = m_metric_data->m_plots[i].m_x_axis.m_min;
                    const float& x_max = m_metric_data->m_plots[i].m_x_axis.m_max;
                    std::vector<const char*>& series_names = m_metric_data->m_plots[i].m_series_names;
                    std::vector<float>& values = m_metric_data->m_plots[i].m_values;

                    if (ImPlot::BeginPlot(title, ImVec2(content_region.x * (1 - PLOT_COLOR_MAP_WIDTH), 0), ImPlotFlags_NoMenus | ImPlotFlags_Crosshairs)) {
                        ImPlot::SetupAxes(x_label, y_label, ImPlotAxisFlags_NoInitialFit | ImPlotAxisFlags_Lock | ImPlotAxisFlags_NoSideSwitch | ImPlotAxisFlags_NoHighlight, 
                                          ImPlotAxisFlags_NoInitialFit | ImPlotAxisFlags_Lock | ImPlotAxisFlags_NoSideSwitch | ImPlotAxisFlags_NoHighlight);
                        ImPlot::SetupAxisLimits(ImAxis_X1, x_min, x_max, ImPlotCond_None);
                        ImPlot::SetupAxisLimits(ImAxis_Y1, -PLOT_BAR_SIZE, series_names.size() - 1 + PLOT_BAR_SIZE, ImPlotCond_None);
                        ImPlot::SetupAxisTicks(ImAxis_Y1, PLOT_LABEL_POSITION, values.size(), series_names.data());
                        ImPlot::PushStyleColor(ImPlotCol_Line, IM_COL32(0, 0, 0, 255));
                        for (int j = 0; j < values.size(); j ++)
                        {
                            float& value = values[j];

                            ImGui::PushID(j);
                            ImPlot::SetNextFillStyle(ImPlot::SampleColormap(x_max - x_min > 0 ? value / (x_max - x_min) : 0, PLOT_COLOR_MAP));
                            ImPlot::PlotBars("", &value, 1, PLOT_BAR_SIZE, j, ImPlotBarsFlags_Horizontal);
                            ImGui::PopID();
                        }
                        ImPlot::PopStyleColor();
                        ImPlot::EndPlot();
                    }
                    ImGui::SameLine();
                    ImPlot::ColormapScale(x_label, x_min, x_max, ImVec2(content_region.x * PLOT_COLOR_MAP_WIDTH, 0), "%g", ImPlotColormapScaleFlags_None, PLOT_COLOR_MAP);
                }
            }
        }
        if (m_render_flags & kComputeMetricTable)
        {
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ITEM_SPACING_DEFAULT);
            ImGui::SeparatorText(m_metric_data->m_table.m_title.c_str());
            ImGui::PopStyleVar(); 
            if (ImGui::BeginTable(std::string(m_metric_category).append("_table").c_str(), m_metric_data->m_table.m_column_names.size(), ImGuiTableFlags_Borders))
            {
                for (std::string& c : m_metric_data->m_table.m_column_names)
                {
                    ImGui::TableSetupColumn(c.c_str());
                }
                ImGui::TableHeadersRow();

                for (int r = 0; r < m_metric_data->m_table.m_values.size(); r ++)
                {
                    ImGui::TableNextRow();
                    for (int c = 0; c < m_metric_data->m_table.m_values[r].size(); c ++)
                    {
                        ImGui::TableSetColumnIndex(c);

                        if (m_metric_data->m_table.m_values[r][c].m_colorize && m_metric_data->m_table.m_values[r][c].m_metric)
                        {
                            if (m_metric_data->m_table.m_values[r][c].m_metric->m_value > TABLE_THRESHOLD_HIGH)
                            {
                                ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, TABLE_COLOR_HIGH);
                            }
                            else if (m_metric_data->m_table.m_values[r][c].m_metric->m_value > TABLE_THRESHOLD_MID)
                            {
                                ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, TABLE_COLOR_MID);
                            }
                        }

                        ImGui::Text(m_metric_data->m_table.m_values[r][c].m_value.c_str());
                    }
                }
                ImGui::EndTable();
            }
        }
    }
}

}  // namespace View
}  // namespace RocProfVis
