// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once
#include "rocprofvis_event_manager.h"
#include "widgets/rocprofvis_widget.h"

namespace RocProfVis
{
namespace View
{

class ComputeDataProvider;
class ComputeDataProvider2;

class ComputeRoot : public RocWidget
{
public:
    void Render() override;
    void Update() override;
    void OpenTrace(const std::string& path);
    void SetProfilePath(const std::string& path);
    bool ProfileLoaded();
    ComputeRoot(std::string owner_id);
    ~ComputeRoot();

private:
    std::shared_ptr<TabContainer> m_tab_container;
    std::shared_ptr<ComputeDataProvider> m_compute_data_provider;
    std::shared_ptr<ComputeDataProvider2> m_compute_data_provider2;
    std::string m_owner_id;
    EventManager::SubscriptionToken m_data_dirty_event_token;
    bool m_data_dirty;
};

}  // namespace View
}  // namespace RocProfVis
