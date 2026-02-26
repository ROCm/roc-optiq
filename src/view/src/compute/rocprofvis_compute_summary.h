// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once
#include "rocprofvis_event_manager.h"
#include "widgets/rocprofvis_widget.h"

namespace RocProfVis
{
namespace View
{

class DataProvider;
class ComputeSelection;
class Roofline;

class ComputeSummaryView : public RocWidget
{
public:
    ComputeSummaryView(DataProvider&                     data_provider,
                       std::shared_ptr<ComputeSelection> compute_selection);
    ~ComputeSummaryView();

    void Update() override;
    void Render() override;

private:
    DataProvider& m_data_provider;

    std::shared_ptr<ComputeSelection> m_compute_selection;
    std::unique_ptr<Roofline>         m_roofline;

    uint64_t m_client_id;

    EventManager::SubscriptionToken m_workload_selection_changed_token;
    EventManager::SubscriptionToken m_metrics_fetched_token;
};

}  // namespace View
}  // namespace RocProfVis
