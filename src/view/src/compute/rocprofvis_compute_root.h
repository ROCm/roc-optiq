// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once
#include "rocprofvis_controller_types.h"
#include "rocprofvis_event_manager.h"
#include "widgets/rocprofvis_tab_container.h"

namespace RocProfVis
{
namespace View
{

class ComputeDataProvider;

class ComputeRoot : public RocWidget
{
public:
    void Render() override;
    void Update() override;
    bool LoadTrace(rocprofvis_controller_t* controller);
    ComputeRoot();
    ~ComputeRoot();

private:
    std::shared_ptr<TabContainer> m_tab_container;
    std::shared_ptr<ComputeDataProvider> m_compute_data_provider;
    std::string m_id;
    EventManager::SubscriptionToken m_data_dirty_event_token;
    bool m_data_dirty;
    bool m_trace_opened;
};

}  // namespace View
}  // namespace RocProfVis
