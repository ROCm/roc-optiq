// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once
#include "../model/compute/rocprofvis_compute_model_types.h"
#include "../widgets/rocprofvis_charts.h"
#include <functional>

namespace RocProfVis
{
namespace View
{

class NewComputeSummaryView
{
public:
    NewComputeSummaryView(
        std::function<void(const WorkloadInfo& workload)> kernel_table_renderer)
    : m_kernel_table_renderer(kernel_table_renderer) {};
    void RenderSummaryView(const WorkloadInfo& workload);
private:
    KernelMetric GetMetricNameByIndex(uint32_t metric) const;
    void RenderPieChart(const WorkloadInfo& workload, KernelMetric metric);
    void RenderBarChart(const WorkloadInfo& workload, KernelMetric metric);
    std::function<void(const WorkloadInfo& workload)> m_kernel_table_renderer;

    const std::vector<std::string_view> m_chart_views = { "Pie Chart", "Bar Chart" };
    const std::vector<std::string_view> m_metric_views = {
        "Invocation Count", "Duration Total", "Duration Min",
        "Duration Max",     "Duration Mean",  "Duration Median"
    };

    PieChart m_pie_chart;
    BarChart m_bar_chart;
};


} // namespace View
} // namespace RocProfVis