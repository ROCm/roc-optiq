// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once
#include "../model/compute/rocprofvis_compute_model_types.h"
#include "../widgets/rocprofvis_charts.h"
#include "rocprofvis_data_provider.h"

#include <functional>

namespace RocProfVis
{
namespace View
{

class ComputeSummaryView : public RocWidget
{
public:
    ComputeSummaryView(DataProvider& data_provider)
    : RocWidget() 
    , m_data_provider(data_provider)
    , m_selected_workload_id(0)
    {};
    void Render() override;
private:
    void         RenderKernelsTable(const WorkloadInfo& workload);
    KernelMetric GetMetricNameByIndex(uint32_t metric) const;
    void RenderPieChart(const WorkloadInfo& workload, KernelMetric metric);
    void RenderBarChart(const WorkloadInfo& workload, KernelMetric metric);

    const std::vector<std::string_view> m_chart_views = { "Pie Chart", "Bar Chart" };
    const std::vector<std::string_view> m_metric_views = {
        "Invocation Count", "Duration Total", "Duration Min",
        "Duration Max",     "Duration Mean",  "Duration Median"
    };

    PieChart m_pie_chart;
    BarChart m_bar_chart;

    DataProvider& m_data_provider;
    uint32_t m_selected_workload_id;
};


} // namespace View
} // namespace RocProfVis