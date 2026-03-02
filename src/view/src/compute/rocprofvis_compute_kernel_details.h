// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once
#include "rocprofvis_compute_memory_chart.h"
#include "rocprofvis_event_manager.h"
#include "widgets/rocprofvis_compute_widget.h"
#include "widgets/rocprofvis_flex_container.h"
#include "widgets/rocprofvis_widget.h"

namespace RocProfVis
{
namespace View
{

class DataProvider;
class ComputeSelection;
class Roofline;
class KernelMetricTable;

class ComputeKernelDetailsView : public RocWidget
{
public:
    ComputeKernelDetailsView(DataProvider&                     data_provider,
                             std::shared_ptr<ComputeSelection> compute_selection);
    ~ComputeKernelDetailsView();

    void Render() override;
    void Update() override;

private:

    void SubscribeToEvents();

    DataProvider&          m_data_provider;
    ComputeMemoryChartView m_memory_chart;

    std::shared_ptr<ComputeSelection>  m_compute_selection;
    std::shared_ptr<Roofline>          m_roofline;
    std::shared_ptr<KernelMetricTable> m_kernel_metric_table;

    uint64_t                           m_client_id;
    std::shared_ptr<MetricTableWidget> m_sol_table;

    FlexContainer m_flex_container;
    bool          m_show_kernel_table = true;

    EventManager::SubscriptionToken m_workload_selection_changed_token;
    EventManager::SubscriptionToken m_kernel_selection_changed_token;
    EventManager::SubscriptionToken m_metrics_fetched_token;
    EventManager::SubscriptionToken m_new_table_data_token;
};

}  // namespace View
}  // namespace RocProfVis
