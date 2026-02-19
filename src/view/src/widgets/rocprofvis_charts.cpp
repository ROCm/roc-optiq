// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_charts.h"
#include "implot/implot.h"
#include <algorithm>

constexpr ImVec2 ITEM_SPACING_DEFAULT = ImVec2(8, 4);
constexpr float MINIMUM_CHART_SIZE = 250.0f;

namespace RocProfVis
{
namespace View
{
PieChart::PiePlacement
PieChart::CalculatePiePlacement(float padding_px, float radius_scale)
{
    const ImVec2 plot_pos  = ImPlot::GetPlotPos();
    const ImVec2 plot_size = ImPlot::GetPlotSize();

    const float width         = std::max(0.0f, plot_size.x - 2.0f * padding_px);
    const float hight         = std::max(0.0f, plot_size.y - 2.0f * padding_px);
    const float radius_px     = 0.5f * std::min(width, hight) * radius_scale;

    const ImVec2 center_px =
        ImVec2(plot_pos.x + plot_size.x * 0.5f, plot_pos.y + plot_size.y * 0.5f);

    // Convert center pixel -> plot coordinates
    const ImPlotPoint center = ImPlot::PixelsToPlot(center_px);

    // Convert pixel radius -> plot units by measuring delta in plot space
    const ImPlotPoint px =
        ImPlot::PixelsToPlot(ImVec2(center_px.x + radius_px, center_px.y));
    const ImPlotPoint py =
        ImPlot::PixelsToPlot(ImVec2(center_px.x, center_px.y + radius_px));

    const double rx = std::abs(px.x - center.x);
    const double ry = std::abs(py.y - center.y);

    const double r_plot = std::min(rx, ry);

    return PiePlacement{ center.x, center.y, r_plot };
};

PieChart::PieChart() { m_chart_title = "Kernels Pie"; }

void
ChartBase::UpdateData(ChartData data)
{
    m_data = std::move(data);
}

ChartData
ChartData::GenerateChartData(KernelMetric metric, const WorkloadInfo& workload)
{
    ChartData data;
    data.m_y_axis_name = "Kernels";
    data.m_x_axis_name = KernelInfo::GetMetricName(metric);
    double total       = 0.0;
    for(const auto& kernel : workload.kernels)
    {
        data.m_labels.push_back(kernel.second.name.c_str());
        data.m_x_values.push_back(static_cast<double>(kernel.second.Get(metric)));
        data.m_y_values.push_back(static_cast<double>(data.m_y_values.size()));
        data.m_x_max = std::max(data.m_x_max, data.m_x_values.back());
        total += data.m_x_values.back();
    }

    data.m_fractions.resize(data.m_x_values.size());
    for(size_t i = 0; i < data.m_x_values.size(); ++i)
    {
        data.m_fractions[i] = data.m_x_values[i] / total;
    }

    return data;
}

void
PieChart::Render()
{
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ITEM_SPACING_DEFAULT);
    ImGui::Separator();
    ImGui::PopStyleVar();

    ImGui::PushID(m_chart_title);
    const float  avail_h = ImGui::GetContentRegionAvail().y;
    const ImVec2 plot_size(-1, std::max(MINIMUM_CHART_SIZE, avail_h));
    if(ImPlot::BeginPlot(m_chart_title, plot_size,
                         ImPlotFlags_Equal | ImPlotFlags_NoInputs))
    {
        ImPlot::SetupAxes(nullptr, nullptr, ImPlotAxisFlags_NoDecorations,
                          ImPlotAxisFlags_NoDecorations);
        ImPlot::SetupAxesLimits(0, 1, 0, 1);

        auto place = CalculatePiePlacement();

        ImPlot::PlotPieChart(
            m_data.m_labels.data(), m_data.m_fractions.data(),
            static_cast<int>(m_data.m_fractions.size()), place.x, place.y, place.radius,
            [](double value, char* buff, int size, void*) -> int {
                if(value > 0.05) return snprintf(buff, size, "%.1f%%", value * 100.0);
                buff[0] = '\0';
                return 0;
            });

        ImPlot::EndPlot();
    }
    ImGui::PopID();
}

BarChart::BarChart() { m_chart_title = "Kernels Bar"; }

void
BarChart::Render()
{
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ITEM_SPACING_DEFAULT);
    ImGui::Separator();
    ImGui::PopStyleVar();

    ImPlot::PushColormap(ImPlotColormap_Plasma);

    const float  avail_h = ImGui::GetContentRegionAvail().y;
    const ImVec2 plot_size(-1, std::max(MINIMUM_CHART_SIZE, avail_h));
    if(ImPlot::BeginPlot(m_chart_title, plot_size,
                         ImPlotFlags_NoInputs | ImPlotFlags_NoLegend))
    {
        ImPlot::SetupAxis(ImAxis_X1, m_data.m_x_axis_name.c_str(),
                          ImPlotAxisFlags_NoSideSwitch | ImPlotAxisFlags_NoHighlight);
        ImPlot::SetupAxis(ImAxis_Y1, m_data.m_y_axis_name.c_str(),
                          ImPlotAxisFlags_NoSideSwitch | ImPlotAxisFlags_NoHighlight);

        ImPlot::SetupAxisTicks(ImAxis_Y1, m_data.m_y_values.data(),
                               static_cast<int>(m_data.m_y_values.size()),
                               m_data.m_labels.data());
        ImPlot::SetupAxisLimits(ImAxis_X1, 0.0,
                                m_data.m_x_max > 0.0 ? m_data.m_x_max * 1.05 : 1.0,
                                ImPlotCond_Always);
        ImPlot::SetupAxisLimits(ImAxis_Y1, -0.5,
                                static_cast<double>(m_data.m_y_values.size()) - 0.5,
                                ImPlotCond_Always);

        const int count = static_cast<int>(m_data.m_x_values.size());
        for(int i = 0; i < count; ++i)
        {
            const double color =
                count > 1 ? static_cast<double>(i) / (count - 1) : 0.5;
            ImPlot::SetNextFillStyle(ImPlot::SampleColormap(color));
            ImPlot::PlotBars(m_data.m_labels[i], &m_data.m_x_values[i], 1, m_bar_height,
                             static_cast<double>(i), ImPlotBarsFlags_Horizontal);
        }

        ImPlot::EndPlot();
    }
    ImPlot::PopColormap();
}
} // namespace View
}  // namespace RocProfVis
