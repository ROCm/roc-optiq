// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once
#include "rocprofvis_event_manager.h"
#include "widgets/rocprofvis_widget.h"

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
    bool OpenTrace(const std::string& path);
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
