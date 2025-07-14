// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.
#pragma once
#include "rocprofvis_data_provider.h"
#include "widgets/rocprofvis_widget.h"
#include <string>
#include <vector>

namespace RocProfVis
{
namespace View
{

class EventsView : public RocWidget
{
public:
    EventsView(DataProvider& dp);
    ~EventsView();
    void Render() override;

private:
    void RenderEventExtData(const std::vector<event_ext_data_t>& ext_data);
    void RenderEventFlowInfo(const std::vector<event_flow_data_t>& flow_data);
    void RenderCallStackData(const std::vector<call_stack_data_t>& call_stack_data);

    void RenderLeftPanel();
    void RenderRightPanel();
    
    DataProvider&            m_data_provider;
    uint64_t                 m_last_selected_event;
    std::shared_ptr<HSplitContainer> m_h_spilt_container;
};

}  // namespace View
}  // namespace RocProfVis