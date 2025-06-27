// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_compute_widget.h"
#include "rocprofvis_core_assert.h"
#include "implot.h"
#include <regex>

namespace RocProfVis
{
namespace View
{

constexpr ImVec2 ITEM_SPACING_DEFAULT = ImVec2(8, 4);
constexpr ImPlotColormap PLOT_COLOR_MAP = ImPlotColormap_Plasma;
constexpr float PLOT_COLOR_MAP_WIDTH = 0.1f;
constexpr double PLOT_BAR_SIZE = 0.67;
constexpr double TABLE_THRESHOLD_HIGH = 80;
constexpr double TABLE_THRESHOLD_MID = 50;
constexpr ImU32 TABLE_COLOR_HIGH = IM_COL32(255, 18, 10, 255);
constexpr ImU32 TABLE_COLOR_MID = IM_COL32(255, 169, 10, 255);
constexpr ImU32 TABLE_COLOR_SEARCH = IM_COL32(0, 255, 0, 255);
constexpr ImVec4 TABLE_COLOR_SEARCH_TEXT = ImVec4(0, 0, 0, 1);

ComputeWidget::ComputeWidget(std::shared_ptr<ComputeDataProvider2> data_provider) 
: m_data_provider(data_provider)
, m_id("")
{
    ROCPROFVIS_ASSERT(m_data_provider);
    m_id = GenUniqueName("");
}

ComputePlot::ComputePlot(std::shared_ptr<ComputeDataProvider2> data_provider, rocprofvis_controller_compute_plot_types_t type)
: ComputeWidget(data_provider)
, m_type(type)
, m_model(nullptr)
{

}

void ComputePlot::Update()
{
    m_model = m_data_provider->GetPlotModel(m_type);
}

ComputeTable::ComputeTable(std::shared_ptr<ComputeDataProvider2> data_provider, rocprofvis_controller_compute_table_types_t type)
: ComputeWidget(data_provider)
, m_type(type)
, m_model(nullptr)
{

}

void ComputeTable::Update()
{
    m_model = m_data_provider->GetTableModel(m_type);
}

void ComputeTable::Render()
{
    if (m_model)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ITEM_SPACING_DEFAULT);
        ImGui::SeparatorText(m_model->m_title.c_str());
        ImGui::PopStyleVar();

        std::vector<std::vector<ComputeTableCellModel>>& cells = m_model->m_cells;
        if (ImGui::BeginTable(m_id.c_str(), cells[0].size(), ImGuiTableFlags_Borders))
        {
            for (std::string& c : m_model->m_column_names)
            {
                ImGui::TableSetupColumn(c.c_str());
            }
            ImGui::TableHeadersRow();

            for (int r = 0; r < cells.size(); r ++)
            {
                ImGui::TableNextRow();
                for (int c = 0; c < cells[r].size(); c ++)
                {
                    ImGui::TableSetColumnIndex(c);                        
                    ComputeTableCellModel& cell = cells[r][c];
                    if (cell.m_highlight)
                    {
                        ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, TABLE_COLOR_SEARCH);
                    }
                    else if (cell.m_colorize)
                    {
                        double value = 0;
                        if (cell.m_type == kRPVControllerPrimitiveTypeDouble)
                        {
                            value = cell.m_num_value.m_double;
                        }
                        else if (cell.m_type == kRPVControllerPrimitiveTypeUInt64)
                        {
                            value = cell.m_num_value.m_uint64;
                        }
                        if (value > TABLE_THRESHOLD_HIGH)
                        {
                            ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, TABLE_COLOR_HIGH);
                        }
                        else if (value > TABLE_THRESHOLD_MID)
                        {
                            ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, TABLE_COLOR_MID);
                        }
                    }

                    ImGui::TextColored(cell.m_highlight ? TABLE_COLOR_SEARCH_TEXT : ImGui::GetStyleColorVec4(ImGuiCol_Text), cell.m_str_value.c_str());
                }
            }
            ImGui::EndTable();
        }
    }
}

void ComputeTable::Search(const std::string& term)
{
    if (m_model)
    {
        std::regex exp(term, std::regex_constants::icase);
        for (std::vector<ComputeTableCellModel>& row : m_model->m_cells)
        {            
            bool match = !term.empty() && !(term.length() == 1 && term == " ") && std::regex_search(row[0].m_str_value, exp);
            for (ComputeTableCellModel& cell : row)
            {
                cell.m_highlight = match;
            }
        }
    }
}

ComputePlotPie::ComputePlotPie(std::shared_ptr<ComputeDataProvider2> data_provider, rocprofvis_controller_compute_plot_types_t type)
: ComputePlot(data_provider, type)
{

}

void ComputePlotPie::Render()
{
    if (m_model)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ITEM_SPACING_DEFAULT);
        ImGui::Separator();
        ImGui::PopStyleVar();

        const char* title = m_model->m_title.c_str();
        std::vector<const char*>& series_names = m_model->m_y_axis.m_tick_labels;
        std::vector<double>& x_values = m_model->m_series[0].m_x_values;
        std::vector<double>& y_values = m_model->m_series[0].m_y_values;

        ImGui::PushID(m_id.c_str());
        if (ImPlot::BeginPlot(title, ImVec2(-1, 0), ImPlotFlags_Equal | ImPlotFlags_NoInputs)) {
            ImPlot::SetupAxes(nullptr, nullptr, ImPlotAxisFlags_NoDecorations, ImPlotAxisFlags_NoDecorations);
            ImPlot::SetupAxesLimits(0, 1, 0, 1);
            double angle = 90;
            for (int i = 0; i < x_values.size(); i ++)
            {
                const char* label[1] = {series_names[i]};
                double value[1] = {x_values[i] / 100};
                ImGui::PushID(i);
                ImPlot::PlotPieChart(label, value, 1, 0.5, 0.5, 0.1, 
                [](double value, char* buff, int size, void*) -> int 
                {
                    //ImplotFormatter callback.
                    snprintf(buff, size, (value > 0.05) ? "%.1f%%" : "", value * 100);
                    return 0;
                },
                nullptr, angle, ImPlotPieChartFlags_None);
                ImGui::PopID();
                angle += x_values[i] / 100 * 360;
            }
            ImPlot::EndPlot();
        }
        ImGui::PopID();
    }
}

ComputePlotBar::ComputePlotBar(std::shared_ptr<ComputeDataProvider2> data_provider, rocprofvis_controller_compute_plot_types_t type)
: ComputePlot(data_provider, type)
{

}

void ComputePlotBar::Render()
{
    if (m_model)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ITEM_SPACING_DEFAULT);
        ImGui::Separator();
        ImGui::PopStyleVar();

        ImVec2 content_region = ImGui::GetContentRegionAvail();
        const char* title = m_model->m_title.c_str();
        const char* x_label = m_model->m_x_axis.m_name.c_str();
        const char* y_label = m_model->m_y_axis.m_name.c_str();
        const double& x_min = 0;
        const double& x_max = m_model->m_x_axis.m_max;
        std::vector<const char*>& series_names = m_model->m_y_axis.m_tick_labels;
        std::vector<double>& x_values = m_model->m_series[0].m_x_values;
        std::vector<double>& y_values = m_model->m_series[0].m_y_values;

        ImGui::PushID(m_id.c_str());
        if (ImPlot::BeginPlot(title, ImVec2(content_region.x * (1 - PLOT_COLOR_MAP_WIDTH), 0), ImPlotFlags_NoMenus | ImPlotFlags_Crosshairs)) {
            ImPlot::SetupAxis(ImAxis_X1, x_label, ImPlotAxisFlags_NoInitialFit | ImPlotAxisFlags_Lock | ImPlotAxisFlags_NoSideSwitch | ImPlotAxisFlags_NoHighlight);
            ImPlot::SetupAxis(ImAxis_Y1, y_label, ImPlotAxisFlags_NoInitialFit | ImPlotAxisFlags_Lock | ImPlotAxisFlags_NoSideSwitch | ImPlotAxisFlags_NoHighlight);
            ImPlot::SetupAxisLimits(ImAxis_X1, x_min, x_max * 1.01f, ImPlotCond_None);
            ImPlot::SetupAxisLimits(ImAxis_Y1, -PLOT_BAR_SIZE, series_names.size() - 1 + PLOT_BAR_SIZE, ImPlotCond_None);
            ImPlot::SetupAxisTicks(ImAxis_Y1, y_values.data(), y_values.size(), series_names.data());
            ImPlot::PushStyleColor(ImPlotCol_Line, ImGui::GetColorU32(ImGui::GetStyleColorVec4(ImGuiCol_Text)));
            for (int i = 0; i < x_values.size(); i ++)
            {
                double& value = x_values[i];
                ImGui::PushID(i);
                ImPlot::SetNextFillStyle(ImPlot::SampleColormap(x_max - x_min > 0 ? value / (x_max - x_min) : 0, PLOT_COLOR_MAP));
                ImPlot::PlotBars("", &value, 1, PLOT_BAR_SIZE, i, ImPlotBarsFlags_Horizontal);
                ImGui::PopID();
            }
            ImPlot::PopStyleColor();
            ImPlot::EndPlot();
        }
        ImGui::PopID();
        ImGui::SameLine();
        ImPlot::ColormapScale(x_label, x_min, x_max, ImVec2(content_region.x * PLOT_COLOR_MAP_WIDTH, 0), "%g", ImPlotColormapScaleFlags_None, PLOT_COLOR_MAP);
    }
}

ComputeMetric::ComputeMetric(std::shared_ptr<ComputeDataProvider2> data_provider, rocprofvis_controller_compute_metric_types_t type, const std::string& label, const std::string& unit)
: ComputeWidget(data_provider)
, m_type(type)
, m_name(label)
, m_unit(unit)
{

}

void ComputeMetric::Update()
{
    m_model = m_data_provider->GetMetricModel(m_type);
    if (m_model)
    {
        if (!m_name.empty())
        {
            m_formatted_string = m_name + ": ";
        }       
        if (m_model->m_type == kRPVControllerPrimitiveTypeDouble)
        {
            m_formatted_string += std::to_string(m_model->m_value.m_double);
        }
        else if (m_model->m_type == kRPVControllerPrimitiveTypeUInt64)
        {
            m_formatted_string += std::to_string(m_model->m_value.m_uint64);
        }
        if (!m_unit.empty())
        {
            m_formatted_string += " " + m_unit;
            if (m_unit == "%")
            {
                m_formatted_string += "%";
            }
        }
    }
}

std::string ComputeMetric::GetFormattedString() const
{
    return m_formatted_string;
}

ComputePlotRoofline::ComputePlotRoofline(std::shared_ptr<ComputeDataProvider> data_provider, rocprofvis_controller_compute_plot_types_t type)
: ComputePlot(data_provider, type)
, m_group_mode(GroupModeKernel)
, m_group_dirty(false)
{

}

void ComputePlotRoofline::Update()
{
    ComputePlot::Update();
    m_group_dirty = true;
    UpdateGroupMode();
}

void ComputePlotRoofline::Render()
{
    if (m_model && !m_group_dirty)
    {
        const char* title = m_model->m_title.c_str();
        const char* x_label = m_model->m_x_axis.m_name.c_str();
        const char* y_label = m_model->m_y_axis.m_name.c_str();
        const double& x_min = m_model->m_x_axis.m_min_non_zero;
        const double& x_max = m_model->m_x_axis.m_max;
        const double& y_min = m_model->m_y_axis.m_min_non_zero;
        const double& y_max = m_model->m_y_axis.m_max;

        if (ImGui::CollapsingHeader(title, ImGuiTreeNodeFlags_DefaultOpen))
        {            
            ImGui::PushID(0);
            if(ImPlot::BeginPlot(title, ImVec2(-1, 500), ImPlotFlags_NoMenus | ImPlotFlags_Crosshairs)) 
            {
                ImPlot::SetupAxis(ImAxis_X1, x_label, ImPlotAxisFlags_NoInitialFit | ImPlotAxisFlags_NoSideSwitch | ImPlotAxisFlags_NoHighlight);
                ImPlot::SetupAxis(ImAxis_Y1, y_label, ImPlotAxisFlags_NoInitialFit | ImPlotAxisFlags_NoSideSwitch | ImPlotAxisFlags_NoHighlight);
                ImPlot::SetupAxisScale(ImAxis_X1, ImPlotScale_Log10);
                ImPlot::SetupAxisScale(ImAxis_Y1, ImPlotScale_Log10);
                ImPlot::SetupAxisLimits(ImAxis_X1, x_min * 0.5f, x_max, ImPlotCond_None);
                ImPlot::SetupAxisLimits(ImAxis_Y1, y_min * 0.5f, y_max * 2, ImPlotCond_None);
                ImPlot::SetupAxisLimitsConstraints(ImAxis_X1, x_min * 0.5f, x_max);
                ImPlot::SetupAxisLimitsConstraints(ImAxis_Y1, y_min * 0.5f, y_max * 2);
                ImPlot::SetupLegend(ImPlotLocation_East, ImPlotLegendFlags_Outside);
                for (int i = 0; i < m_ceilings_names.size(); i ++)
                {
                    ImPlot::PlotLine(m_ceilings_names[i], m_ceilings_x[i]->data(), m_ceilings_y[i]->data(), m_ceilings_x[i]->size());
                }
                for (int i = 0; i < m_ai_names.size(); i ++)
                {
                    ImGui::PushID(i);
                    ImPlot::PlotScatter(m_ai_names[i], m_ai_x[i]->data(), m_ai_y[i]->data(), m_ai_x[i]->size());
                    ImGui::PopID();
                }
                ImPlot::EndPlot();
            }
            ImGui::PopID();
        }
    }
}

void ComputePlotRoofline::UpdateGroupMode()
{
    if (m_group_dirty && m_model)
    {
        m_ceilings_names.clear();
        m_ceilings_x.clear();
        m_ceilings_y.clear();
        m_ai_names.clear();
        m_ai_x.clear();
        m_ai_y.clear();

        for (ComputePlotSeriesModel& series : m_model->m_series)
        {
            bool kernel_series = series.m_name.find("Kernel") != std::string::npos;
            bool dispatch_series = series.m_name.find("Dispatch") != std::string::npos;
            bool ai_series = kernel_series || dispatch_series;
            if (ai_series)
            {
                if (m_group_mode == GroupModeKernel && kernel_series || m_group_mode == GroupModeDispatch && dispatch_series)
                {
                    m_ai_names.push_back(series.m_name.c_str());
                    m_ai_x.push_back(&series.m_x_values);
                    m_ai_y.push_back(&series.m_y_values);
                }
            }
            else
            {
                m_ceilings_names.push_back(series.m_name.c_str());
                m_ceilings_x.push_back(&series.m_x_values);
                m_ceilings_y.push_back(&series.m_y_values);
            }
        }
        ROCPROFVIS_ASSERT(!m_ceilings_names.empty() && !m_ceilings_x.empty() && !m_ceilings_y.empty() && !m_ai_names.empty() && !m_ai_x.empty() && !m_ai_y.empty());
        m_group_dirty = false;
    }
}

void ComputePlotRoofline::SetGroupMode(const GroupMode& mode)
{
    if (mode != m_group_mode)
    {
        m_group_mode = mode;
        m_group_dirty = true;
        UpdateGroupMode();
    }
}

}  // namespace View
}  // namespace RocProfVis
