// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_charts.h"
#include "implot/implot.h"

constexpr ImVec2 ITEM_SPACING_DEFAULT = ImVec2(8, 4);

namespace RocProfVis
{
namespace View
{

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
    if(ImPlot::BeginPlot(m_chart_title, ImVec2(-1, 0),
                         ImPlotFlags_Equal | ImPlotFlags_NoInputs))
    {
        ImPlot::SetupAxes(nullptr, nullptr, ImPlotAxisFlags_NoDecorations,
                          ImPlotAxisFlags_NoDecorations);
        ImPlot::SetupAxesLimits(0, 1, 0, 1);

        const ImVec2 plot_size = ImPlot::GetPlotSize();
        const float  margin_px = 8.0f;  // TODO: move to constants
        const float  max_radius_px =
            0.5f * (std::min(plot_size.x, plot_size.y)) -margin_px;
        const double radius =
            0.08;  // TODO: Calculate radius
                   // std::max(0.0f, max_radius_px) /
                   // std::min(plot_size.x, plot_size.y);  // normalized 0..0.5-ish

        const double cx     = 0.5;
        const double cy     = 0.5;
        const double angle0 = 90.0;

        ImPlot::PlotPieChart(
            m_data.m_labels.data(), m_data.m_fractions.data(),
            static_cast<int>(m_data.m_fractions.size()), cx, cy, radius,
            [](double value, char* buff, int size, void*) -> int {
                if(value > 0.05) return snprintf(buff, size, "%.1f%%", value * 100.0);
                buff[0] = '\0';
                return 0;
            },
            nullptr, angle0, ImPlotPieChartFlags_None);

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

    if(ImPlot::BeginPlot(title, ImVec2(-1, 0),
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