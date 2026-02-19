// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once
#include "rocprofvis_widget.h"
#include "../model/compute/rocprofvis_compute_model_types.h"

namespace RocProfVis
{
namespace View
{

struct ChartData
{
    std::vector<const char*> m_labels;
    std::vector<double>      m_x_values;
    std::vector<double>      m_y_values;
    std::string              m_x_axis_name;
    std::string              m_y_axis_name;
    std::vector<double>      m_fractions;
    double                   m_x_max;

    static ChartData GenerateChartData(KernelMetric metric, const WorkloadInfo& workload);
};

class ChartBase : public RocWidget
{
public:
    ChartBase()  = default;
    ~ChartBase() = default;
    void UpdateData(ChartData data);
protected:
    const char* m_chart_title;
    ChartData   m_data;
};

class PieChart : public ChartBase
{
public:
    PieChart();
    ~PieChart() = default;
    void Render() override;

private:
    struct PiePlacement
    {
        double x      = 0.0;
        double y      = 0.0;
        double radius = 0.0;
    };
    inline PiePlacement CalculatePiePlacement(float padding_px   = 10.0f,
                                              float radius_scale = 0.95f);
};

class BarChart : public ChartBase
{
public:
    BarChart();
    ~BarChart() = default;
    void Render() override;
private:
    const double m_bar_height = 0.7;
};

}  // namespace View
}  // namespace RocProfVis
