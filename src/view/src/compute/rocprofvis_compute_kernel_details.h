// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once
#include "widgets/rocprofvis_widget.h"
#include "rocprofvis_compute_memory_chart.h"

namespace RocProfVis
{
namespace View
{

class DataProvider;
enum class ProviderState;

class ComputeKernelDetailsView : public RocWidget
{
public:
    ComputeKernelDetailsView(DataProvider& data_provider);
    ~ComputeKernelDetailsView();

    void Render();
    void Update();

private:
    DataProvider&          m_data_provider;
    ComputeMemoryChartView m_memory_chart;
    bool                   m_memory_chart_fetched;
    ProviderState          m_last_provider_state;
};

}  // namespace View
}  // namespace RocProfVis
