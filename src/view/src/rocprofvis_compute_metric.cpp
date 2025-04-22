// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_compute_metric.h"

using namespace RocProfVis::View;

constexpr ImVec2 ITEM_SPACING_DEFAULT = ImVec2(8, 4);

ComputeMetric::ComputeMetric(std::shared_ptr<ComputeDataProvider> data_provider, std::string metric_category, compute_metric_render_flags_t render_flags)
: m_data_provider(data_provider)
, m_metric_category(metric_category)
, m_render_flags(render_flags)
, m_metric_data(nullptr)
{}

ComputeMetric::~ComputeMetric() {}

void ComputeMetric::Update()
{
    m_metric_data = m_data_provider->GetMetricGroup(m_metric_category);
}

void ComputeMetric::Render()
{
	if (m_metric_data)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ITEM_SPACING_DEFAULT);
        ImGui::SeparatorText(m_metric_category.c_str());
        ImGui::PopStyleVar();
        if (m_render_flags & kComputeMetricPie)
        {
            if (ImPlot::BeginPlot("##pie", ImVec2(-1, 0), ImPlotFlags_Equal | ImPlotFlags_NoInputs)) {
                ImPlot::SetupAxes(nullptr, nullptr, ImPlotAxisFlags_NoDecorations, ImPlotAxisFlags_NoDecorations);
                ImPlot::SetupAxesLimits(0, 1, 0, 1);
                ImPlot::PlotPieChart(m_metric_data->m_plot_labels.data(), m_metric_data->m_plot_values[4].data(), m_metric_data->m_plot_values[4].size(), 0.5, 0.5, 0.1, "%.2f", 90, ImPlotPieChartFlags_None);
                ImPlot::EndPlot();
            }
        }
        if (m_render_flags & kComputeMetricBar)
        {
            if (ImPlot::BeginPlot("##bar", ImVec2(-1, 0), ImPlotFlags_NoInputs)) {
                ImPlot::SetupAxisTicks(ImAxis_X1, std::vector<double>{1, 2, 3, 4, 5, 6}.data(), m_metric_data->m_plot_values[3].size(), m_metric_data->m_plot_labels.data());
                ImPlot::PlotBars("##vbar", m_metric_data->m_plot_values[3].data(), m_metric_data->m_plot_values[3].size(), 0.7, 1);
                ImPlot::EndPlot();
            }
        }
        if (m_render_flags & kComputeMetricTable)
        {
            if (ImGui::BeginTable("table", m_metric_data->m_table_column_names.size(), ImGuiTableFlags_Borders))
            {
                for (std::string c : m_metric_data->m_table_column_names)
                {
                    ImGui::TableSetupColumn(c.c_str());
                }
                ImGui::TableHeadersRow();

                for (int r = 0; r < m_metric_data->m_table_values.size(); r ++)
                {
                    ImGui::TableNextRow();
                    for (int c = 0; c < m_metric_data->m_table_values[r].size(); c ++)
                    {
                        ImGui::TableSetColumnIndex(c);
                        ImGui::Text(m_metric_data->m_table_values[r][c].c_str());
                    }
                }
                ImGui::EndTable();
            }
        }
    }
}
