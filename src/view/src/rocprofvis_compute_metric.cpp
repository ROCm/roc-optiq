// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_compute_metric.h"

namespace RocProfVis
{
namespace View
{

constexpr ImVec2 ITEM_SPACING_DEFAULT = ImVec2(8, 4);
constexpr ImPlotColormap PLOT_COLOR_MAP = ImPlotColormap_Plasma;
constexpr float PLOT_COLOR_MAP_WIDTH = 0.1f;
constexpr double PLOT_BAR_SIZE = 0.67;
constexpr int TABLE_THRESHOLD_HIGH = 80;
constexpr int TABLE_THRESHOLD_MID = 50;
constexpr ImU32 TABLE_COLOR_HIGH = IM_COL32(255, 18, 10, 255);
constexpr ImU32 TABLE_COLOR_MID = IM_COL32(255, 169, 10, 255);
constexpr ImU32 TABLE_COLOR_SEARCH = IM_COL32(0, 255, 0, 255);

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
                    const char* title = m_metric_data->m_plots[i].m_title.c_str();
                    std::vector<const char*>& series_names = m_metric_data->m_plots[i].m_y_axis.m_tick_labels;
                    std::vector<double>& x_values = m_metric_data->m_plots[i].m_series[0].m_x_values;
                    std::vector<double>& y_values = m_metric_data->m_plots[i].m_series[0].m_y_values;

                    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ITEM_SPACING_DEFAULT);
                    ImGui::Separator();
                    ImGui::PopStyleVar();
                    if (ImPlot::BeginPlot(title, ImVec2(-1, 0), ImPlotFlags_Equal | ImPlotFlags_NoInputs)) {
                        ImPlot::SetupAxes(nullptr, nullptr, ImPlotAxisFlags_NoDecorations, ImPlotAxisFlags_NoDecorations);
                        ImPlot::SetupAxesLimits(0, 1, 0, 1);
                        double angle = 90;
                        for (int j = 0; j < x_values.size(); j ++)
                        {
                            const char* label[1] = {series_names[j]};
                            double value[1] = {x_values[j] / 100};
                            ImGui::PushID(j);
                            ImPlot::PlotPieChart(label, value, 1, 0.5, 0.5, 0.1, 
                            [](double value, char* buff, int size, void*) -> int 
                            {
                                //ImplotFormatter callback.
                                snprintf(buff, size, (value > 0.05) ? "%.1f%%" : "", value * 100);
                                return 0;
                            },
                            nullptr, angle, ImPlotPieChartFlags_None);
                            ImGui::PopID();
                            angle += x_values[j] / 100 * 360;
                        }
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
                    const char* x_label = m_metric_data->m_plots[i].m_x_axis.m_name.c_str();
                    const char* y_label = m_metric_data->m_plots[i].m_y_axis.m_name.c_str();
                    const float& x_min = m_metric_data->m_plots[i].m_x_axis.m_min;
                    const float& x_max = m_metric_data->m_plots[i].m_x_axis.m_max;
                    std::vector<const char*>& series_names = m_metric_data->m_plots[i].m_y_axis.m_tick_labels;
                    std::vector<double>& x_values = m_metric_data->m_plots[i].m_series[0].m_x_values;
                    std::vector<double>& y_values = m_metric_data->m_plots[i].m_series[0].m_y_values;

                    if (ImPlot::BeginPlot(title, ImVec2(content_region.x * (1 - PLOT_COLOR_MAP_WIDTH), 0), ImPlotFlags_NoMenus | ImPlotFlags_Crosshairs)) {
                        ImPlot::SetupAxes(x_label, y_label, ImPlotAxisFlags_NoInitialFit | ImPlotAxisFlags_Lock | ImPlotAxisFlags_NoSideSwitch | ImPlotAxisFlags_NoHighlight, 
                                          ImPlotAxisFlags_NoInitialFit | ImPlotAxisFlags_Lock | ImPlotAxisFlags_NoSideSwitch | ImPlotAxisFlags_NoHighlight);
                        ImPlot::SetupAxisLimits(ImAxis_X1, x_min, x_max, ImPlotCond_None);
                        ImPlot::SetupAxisLimits(ImAxis_Y1, -PLOT_BAR_SIZE, series_names.size() - 1 + PLOT_BAR_SIZE, ImPlotCond_None);
                        ImPlot::SetupAxisTicks(ImAxis_Y1, y_values.data(), y_values.size(), series_names.data());
                        ImPlot::PushStyleColor(ImPlotCol_Line, IM_COL32(0, 0, 0, 255));
                        for (int j = 0; j < x_values.size(); j ++)
                        {
                            double& value = x_values[j];

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

                        if (m_metric_data->m_table.m_values[r][c].m_highlight)
                        {
                            ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, TABLE_COLOR_SEARCH);
                        }
                        else if (m_metric_data->m_table.m_values[r][c].m_colorize && m_metric_data->m_table.m_values[r][c].m_metric)
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

void ComputeMetricGroup::Search(const std::string& term)
{
    if (m_metric_data)
    {
        std::regex exp(term, std::regex_constants::icase);
        for (std::vector<rocprofvis_compute_metrics_table_cell_t>& row : m_metric_data->m_table.m_values)
        {            
            bool match = !term.empty() && !(term.length() == 1 && term == " ") && std::regex_search(row[0].m_value, exp);
            for (rocprofvis_compute_metrics_table_cell_t& cell : row)
            {
                cell.m_highlight = match;
            }
        }
    }
}

ComputeMetricRoofline::ComputeMetricRoofline(std::shared_ptr<ComputeDataProvider> data_provider, roofline_grouping_mode_t grouping)
: m_data_provider(data_provider) 
, m_roof_data(nullptr)
, m_intensity_data(nullptr)
, m_group_mode(grouping)
{
}

ComputeMetricRoofline::~ComputeMetricRoofline() {}

void ComputeMetricRoofline::Update()
{
    if (m_data_provider)
    {
        m_roof_data = m_data_provider->GetMetricGroup(std::string("roofline.csv"));
        m_intensity_data = m_data_provider->GetMetricGroup(std::string("pmc_perf.csv"));
    }
}

roofline_grouping_mode_t ComputeMetricRoofline::GetGroupMode()
{
    return m_group_mode;
}

void ComputeMetricRoofline::SetGroupMode(roofline_grouping_mode_t grouping)
{
    m_group_mode = grouping;
}

void ComputeMetricRoofline::Render()
{
    if (m_roof_data && m_intensity_data)
    {
        rocprofvis_compute_metrics_plot_t& intensity_plot = m_intensity_data->m_plots[m_group_mode];
        for (int i = 0; i < m_roof_data->m_plots.size(); i ++)
        {
            rocprofvis_compute_metrics_plot_t& roof_plot = m_roof_data->m_plots[i];
            if (ImGui::CollapsingHeader(roof_plot.m_title.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
            {            
                ImGui::PushID(i);
                if(ImPlot::BeginPlot(roof_plot.m_title.c_str(), ImVec2(-1, 500), ImPlotFlags_NoMenus | ImPlotFlags_Crosshairs)) 
                {
                    ImPlot::SetupAxes(roof_plot.m_x_axis.m_name.c_str(), roof_plot.m_y_axis.m_name.c_str(), ImPlotAxisFlags_NoInitialFit |ImPlotAxisFlags_NoSideSwitch | ImPlotAxisFlags_NoHighlight, 
                                      ImPlotAxisFlags_NoInitialFit | ImPlotAxisFlags_NoSideSwitch | ImPlotAxisFlags_NoHighlight);
                    ImPlot::SetupAxisScale(ImAxis_X1, ImPlotScale_Log10);
                    ImPlot::SetupAxisScale(ImAxis_Y1, ImPlotScale_Log10);
                    ImPlot::SetupAxisLimits(ImAxis_X1, roof_plot.m_x_axis.m_min, roof_plot.m_x_axis.m_max, ImPlotCond_None);
                    ImPlot::SetupAxisLimits(ImAxis_Y1, roof_plot.m_y_axis.m_min, roof_plot.m_y_axis.m_max, ImPlotCond_None);
                    ImPlot::SetupAxisLimitsConstraints(ImAxis_X1, roof_plot.m_x_axis.m_min, roof_plot.m_x_axis.m_max);
                    ImPlot::SetupAxisLimitsConstraints(ImAxis_Y1, roof_plot.m_y_axis.m_min, roof_plot.m_y_axis.m_max);
                    ImPlot::SetupLegend(ImPlotLocation_East, ImPlotLegendFlags_Outside);
                    for (rocprofvis_compute_metric_plot_series_t& series : roof_plot.m_series)
                    {
                        ImPlot::PlotLine(series.m_name.c_str(), series.m_x_values.data(), series.m_y_values.data(), series.m_x_values.size());
                    }
                    for (rocprofvis_compute_metric_plot_series_t& series : intensity_plot.m_series)
                    {
                        ImPlot::PlotScatter(series.m_name.c_str(), series.m_x_values.data(), series.m_y_values.data(), series.m_x_values.size());
                    }
                    ImPlot::EndPlot();
                }
                ImGui::PopID();
            }           
        }
    }
}

}  // namespace View
}  // namespace RocProfVis
