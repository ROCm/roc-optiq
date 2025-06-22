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

class EventsView : public RocCustomWidget
{
public:
    EventsView(DataProvider& dp);
    ~EventsView();
    void ShowEventExtDataPanel(const std::vector<event_ext_data_t>& ext_data);
    void ShowEventFlowInfoPanel(const std::vector<event_flow_data_t>& flow_data);
    void Render();

private:
    std::vector<std::string> events_;
    DataProvider&            m_data_provider;
    uint64_t                 m_last_selected_event;
};

}  // namespace View
}  // namespace RocProfVis