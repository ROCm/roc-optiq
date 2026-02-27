// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once
#include "rocprofvis_data_provider.h"
#include "rocprofvis_event_manager.h"
#include "widgets/rocprofvis_compute_widget.h"
#include "widgets/rocprofvis_tab_container.h"
#include <unordered_map>

namespace RocProfVis
{
namespace View
{

class ComputeSelection;

class ComputeTableView: public RocWidget
{
public:
    ComputeTableView(DataProvider& data_provider, std::shared_ptr<ComputeSelection> compute_selection);
    ~ComputeTableView();

    void Update() override;
    void Render() override;

private:
    void RenderCategory(const AvailableMetrics::Category& cat);
    void RebuildTabs();
    void FetchAllMetrics();
    void RebuildTableDataCache();

    DataProvider&                     m_data_provider;
    std::shared_ptr<ComputeSelection> m_compute_selection;
    uint64_t                          m_client_id;
    bool                              m_fetch_pending = false;
    std::shared_ptr<TabContainer>     m_tabs;
    std::unordered_map<uint64_t, MetricTableCache> m_table_widgets;

    EventManager::SubscriptionToken m_workload_selection_changed_token;
    EventManager::SubscriptionToken m_kernel_selection_changed_token;
    EventManager::SubscriptionToken m_metrics_fetched_token;
};

}  // namespace View
}  // namespace RocProfVis
