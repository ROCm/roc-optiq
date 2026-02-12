// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once
#include "rocprofvis_widget.h"
#include "../model/compute/rocprofvis_compute_model_types.h"

namespace RocProfVis
{
namespace View
{

class ChartBase : public RocWidget
{
public:
    ChartBase()  = default;
    ~ChartBase() = default;
    void UpdateData(ChartData data);

protected:
    void        ClearData();
    const char* m_chart_title;
    ChartData   m_data;
};

class PieChart : public ChartBase
{
public:
    PieChart();
    ~PieChart() = default;
    void Render() override;
};

class BarChart : public ChartBase
{
public:
    BarChart();
    ~BarChart() = default;
    void Render() override;
};

}  // namespace View
}  // namespace RocProfVis