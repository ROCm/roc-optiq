// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once
#include "rocprofvis_compute_memory_chart.h"
#include "rocprofvis_event_manager.h"
#include "widgets/rocprofvis_compute_widget.h"
#include "widgets/rocprofvis_widget.h"

namespace RocProfVis
{
namespace View
{

class DataProvider;
class ComputeSelection;
class Roofline;

class ComputeKernelDetailsView : public RocWidget
{
public:
    ComputeKernelDetailsView(DataProvider&                     data_provider,
                             std::shared_ptr<ComputeSelection> compute_selection);
    ~ComputeKernelDetailsView();

    void Render() override;
    void Update() override;

private:
    DataProvider&          m_data_provider;
    ComputeMemoryChartView m_memory_chart;

    std::shared_ptr<ComputeSelection> m_compute_selection;
    std::unique_ptr<Roofline>         m_roofline;
    MetricTableWidget                 m_sol_table;

    EventManager::SubscriptionToken m_workload_selection_changed_token;
    EventManager::SubscriptionToken m_kernel_selection_changed_token;
    EventManager::SubscriptionToken m_metrics_fetched_token;
};

}  // namespace View
}  // namespace RocProfVis
