// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_compute_widget.h"
#include "compute/rocprofvis_compute_selection.h"
#include "rocprofvis_core_assert.h"
#include "rocprofvis_data_provider.h"
#include "rocprofvis_requests.h"
#include "implot.h"
#include <algorithm>
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

ComputeWidgetLegacy::ComputeWidgetLegacy(std::shared_ptr<ComputeDataProvider> data_provider) 
: m_data_provider(data_provider)
, m_id("")
{
    ROCPROFVIS_ASSERT(m_data_provider);
    m_id = GenUniqueName("");
}

ComputePlotLegacy::ComputePlotLegacy(std::shared_ptr<ComputeDataProvider> data_provider, rocprofvis_controller_compute_plot_types_t type)
: ComputeWidgetLegacy(data_provider)
, m_type(type)
, m_model(nullptr)
{

}

void ComputePlotLegacy::Update()
{
    m_model = m_data_provider->GetPlotModel(m_type);
}

ComputeTableLegacy::ComputeTableLegacy(std::shared_ptr<ComputeDataProvider> data_provider, rocprofvis_controller_compute_table_types_t type)
: ComputeWidgetLegacy(data_provider)
, m_type(type)
, m_model(nullptr)
{

}

void ComputeTableLegacy::Update()
{
    m_model = m_data_provider->GetTableModel(m_type);
}

void ComputeTableLegacy::Render()
{
    if (m_model)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ITEM_SPACING_DEFAULT);
        ImGui::SeparatorText(m_model->m_title.c_str());
        ImGui::PopStyleVar();

        std::vector<std::vector<ComputeTableCellModel>>& cells = m_model->m_cells;
        if (ImGui::BeginTable(m_id.c_str(), static_cast<int>(cells[0].size()), ImGuiTableFlags_Borders))
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
                            value = static_cast<double>(cell.m_num_value.m_uint64);
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

void ComputeTableLegacy::Search(const std::string& term)
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

ComputePlotPieLegacy::ComputePlotPieLegacy(std::shared_ptr<ComputeDataProvider> data_provider, rocprofvis_controller_compute_plot_types_t type)
: ComputePlotLegacy(data_provider, type)
{

}

void ComputePlotPieLegacy::Render()
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
        (void)y_values;

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

ComputePlotBarLegacy::ComputePlotBarLegacy(std::shared_ptr<ComputeDataProvider> data_provider, rocprofvis_controller_compute_plot_types_t type)
: ComputePlotLegacy(data_provider, type)
{

}

void ComputePlotBarLegacy::Render()
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
            ImPlot::SetupAxisTicks(ImAxis_Y1, y_values.data(), static_cast<int>(y_values.size()), series_names.data());
            ImPlot::PushStyleColor(ImPlotCol_Line, ImGui::GetColorU32(ImGui::GetStyleColorVec4(ImGuiCol_Text)));
            for (int i = 0; i < x_values.size(); i ++)
            {
                double& value = x_values[i];
                ImGui::PushID(i);
                ImPlot::SetNextFillStyle(ImPlot::SampleColormap(static_cast<float>(x_max - x_min > 0 ? value / (x_max - x_min) : 0), PLOT_COLOR_MAP));
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

ComputeMetricLegacy::ComputeMetricLegacy(std::shared_ptr<ComputeDataProvider> data_provider, rocprofvis_controller_compute_metric_types_t type, const std::string& label, const std::string& unit)
: ComputeWidgetLegacy(data_provider)
, m_type(type)
, m_name(label)
, m_unit(unit)
{

}

void ComputeMetricLegacy::Update()
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

std::string ComputeMetricLegacy::GetFormattedString() const
{
    return m_formatted_string;
}

ComputePlotRooflineLegacy::ComputePlotRooflineLegacy(std::shared_ptr<ComputeDataProvider> data_provider, rocprofvis_controller_compute_plot_types_t type)
: ComputePlotLegacy(data_provider, type)
, m_group_mode(GroupModeKernel)
, m_group_dirty(false)
{

}

void ComputePlotRooflineLegacy::Update()
{
    ComputePlotLegacy::Update();
    m_group_dirty = true;
    UpdateGroupMode();
}

void ComputePlotRooflineLegacy::Render()
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
                    ImPlot::PlotLine(m_ceilings_names[i], m_ceilings_x[i]->data(), m_ceilings_y[i]->data(), static_cast<int>(m_ceilings_x[i]->size()));
                }
                for (int i = 0; i < m_ai_names.size(); i ++)
                {
                    ImGui::PushID(i);
                    ImPlot::PlotScatter(m_ai_names[i], m_ai_x[i]->data(), m_ai_y[i]->data(), static_cast<int>(m_ai_x[i]->size()));
                    ImGui::PopID();
                }
                ImPlot::EndPlot();
            }
            ImGui::PopID();
        }
    }
}

void ComputePlotRooflineLegacy::UpdateGroupMode()
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

void ComputePlotRooflineLegacy::SetGroupMode(const GroupMode& mode)
{
    if (mode != m_group_mode)
    {
        m_group_mode = mode;
        m_group_dirty = true;
        UpdateGroupMode();
    }
}

void
MetricTableCache::Populate(const AvailableMetrics::Table& table,
                           const MetricValueLookup&       get_value)
{
    m_title    = table.name;
    m_table_id = "##" + table.name;
    m_column_names.clear();
    m_rows.clear();
    m_rows.reserve(table.entries.size());

    m_column_names.push_back("Metric");
    if(table.value_names.empty())
    {
        m_column_names.push_back("Value");
    }
    else
    {
        for(const auto& vn : table.value_names)
            m_column_names.push_back(vn);
    }
    m_column_names.push_back("Unit");

    char buf[64];
    for(const auto& entry_pair : table.entries)
    {
        uint32_t    eid   = entry_pair.first;
        const auto& entry = entry_pair.second;

        Row row;
        row.name = entry.name;
        row.unit = entry.unit.empty() ? "N/A" : entry.unit;

        auto mv = get_value(eid);

        if(table.value_names.empty())
        {
            if(mv && mv->entry && !mv->values.empty())
            {
                snprintf(buf, sizeof(buf), "%.2f", mv->values.begin()->second);
                row.values.push_back(buf);
            }
            else
            {
                row.values.emplace_back();
            }
        }
        else
        {
            for(const auto& vn : table.value_names)
            {
                if(mv && mv->entry && mv->values.count(vn))
                {
                    snprintf(buf, sizeof(buf), "%.2f", mv->values.at(vn));
                    row.values.push_back(buf);
                }
                else
                {
                    row.values.emplace_back();
                }
            }
        }

        m_rows.push_back(std::move(row));
    }
}

void
MetricTableCache::Render() const
{
    if(m_rows.empty())
        return;

    int num_columns = static_cast<int>(m_column_names.size());

    ImGui::SeparatorText(m_title.c_str());
    if(!ImGui::BeginTable(m_table_id.c_str(), num_columns, ImGuiTableFlags_Borders))
        return;

    for(const auto& col : m_column_names)
        ImGui::TableSetupColumn(col.c_str());
    ImGui::TableHeadersRow();

    for(const auto& row : m_rows)
    {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(row.name.c_str());

        for(const auto& val : row.values)
        {
            ImGui::TableNextColumn();
            if(!val.empty())
            {
                ImGui::TextUnformatted(val.c_str());
            }
            else
            {
                ImGui::TextDisabled("N/A");
            }
        }

        ImGui::TableNextColumn();
        if(row.unit != "N/A")
        {
            ImGui::TextUnformatted(row.unit.c_str());
        }
        else
        {
            ImGui::TextDisabled("N/A");
        }
    }
    ImGui::EndTable();
}

void
MetricTableCache::Clear()
{
    m_rows.clear();
    m_column_names.clear();
}

bool
MetricTableCache::Empty() const
{
    return m_rows.empty();
}

MetricTableWidget::MetricTableWidget(DataProvider& data_provider,
                                     std::shared_ptr<ComputeSelection> compute_selection,
                                     uint32_t category_id, uint32_t table_id)
: RocWidget()
, m_data_provider(data_provider)
, m_compute_selection(compute_selection)
, m_category_id(category_id)
, m_table_id(table_id)
, m_client_id(IdGenerator::GetInstance().GenerateId())
{
    m_widget_name = GenUniqueName("MetricTableWidget");
}

void
MetricTableWidget::Render()
{
    m_table.Render();
}

void
MetricTableWidget::Clear()
{
    m_table.Clear();
}

void
MetricTableWidget::FetchMetrics()
{
    m_table.Clear();
    m_data_provider.ComputeModel().ClearMetricValues(m_client_id);

    uint32_t workload_id = m_compute_selection->GetSelectedWorkload();
    uint32_t kernel_id   = m_compute_selection->GetSelectedKernel();
    if(workload_id == ComputeSelection::INVALID_SELECTION_ID ||
       kernel_id == ComputeSelection::INVALID_SELECTION_ID)
    {
        return;
    }

    m_data_provider.FetchMetrics(MetricsRequestParams(
        workload_id, {kernel_id}, {{m_category_id, m_table_id, std::nullopt}}, m_client_id));
}

void
MetricTableWidget::UpdateTable()
{
    uint32_t workload_id = m_compute_selection->GetSelectedWorkload();
    uint32_t kernel_id   = m_compute_selection->GetSelectedKernel();
    if(workload_id == ComputeSelection::INVALID_SELECTION_ID ||
       kernel_id == ComputeSelection::INVALID_SELECTION_ID)
    {
        return;
    }

    auto& model = m_data_provider.ComputeModel();
    if(!model.GetWorkloads().count(workload_id))
        return;

    const auto& tree = model.GetWorkloads().at(workload_id).available_metrics.tree;
    if(!tree.count(m_category_id) || !tree.at(m_category_id).tables.count(m_table_id))
        return;

    m_table.Populate(tree.at(m_category_id).tables.at(m_table_id), [&](uint32_t eid) {
        return model.GetMetricValue(m_client_id, kernel_id, m_category_id, m_table_id, eid);
    });
}

}  // namespace View
}  // namespace RocProfVis
