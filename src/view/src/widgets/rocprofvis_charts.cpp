// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_charts.h"
#include "implot/implot.h"
#include <algorithm>

constexpr ImVec2 ITEM_SPACING_DEFAULT = ImVec2(8, 4);

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

void
PieChart::Render()
{
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ITEM_SPACING_DEFAULT);
    ImGui::Separator();
    ImGui::PopStyleVar();

    ImGui::PushID(m_chart_title);
    const float  avail_h = ImGui::GetContentRegionAvail().y;
    const ImVec2 plot_size(-1, std::max(80.0f, avail_h));
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

void
ChartBase::ClearData()
{
    m_data.m_labels.clear();
    m_data.m_x_values.clear();
    m_data.m_y_values.clear();
    m_data.m_fractions.clear();
}

BarChart::BarChart() { m_chart_title = "Kernels Bar"; }

void
BarChart::Render()
{
    const char* title = "Kernels Bar";

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ITEM_SPACING_DEFAULT);
    ImGui::Separator();
    ImGui::PopStyleVar();

    const double bar_height = 0.7;

    ImPlot::PushColormap(ImPlotColormap_Plasma);

    const float  avail_h = ImGui::GetContentRegionAvail().y;
    const ImVec2 plot_size(-1, std::max(80.0f, avail_h));
    if(ImPlot::BeginPlot(title, plot_size,
                         ImPlotFlags_NoInputs | ImPlotFlags_NoLegend))
    {
        ImPlot::SetupAxis(ImAxis_X1, "Invocations",
                          ImPlotAxisFlags_NoSideSwitch | ImPlotAxisFlags_NoHighlight);
        ImPlot::SetupAxis(ImAxis_Y1, "Kernels",
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
                count > 1 ? static_cast<double>(i) / (count - 1) : 0.5;  // 0..1
            ImPlot::SetNextFillStyle(ImPlot::SampleColormap(color));
            ImPlot::PlotBars(m_data.m_labels[i], &m_data.m_x_values[i], 1, bar_height,
                             static_cast<double>(i), ImPlotBarsFlags_Horizontal);
        }

        ImPlot::EndPlot();
    }
    ImPlot::PopColormap();
}
} // namespace View
}  // namespace RocProfVis