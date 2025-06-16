// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_events_view.h"
#include "imgui.h"
#include "rocprofvis_data_provider.h"
#include <algorithm>
#include <iostream>
#include <limits>
#include <map>
#include <string>
#include <vector>

namespace RocProfVis
{
namespace View
{

EventsView::EventsView(DataProvider& dp)
: RocCustomWidget([] {})
, m_data_provider(dp)  // Initialize with DataProvider reference
{}

EventsView::~EventsView() {}

void
EventsView::Render()
{
    ImGui::BeginChild("EventsView", ImVec2(0, 0), true);
    if(m_data_provider.GetSelectedEvent() == std::numeric_limits<uint64_t>::max())
    {
        ImGui::Text("No event selected.");
    }
    else
    {
        ImGui::Text("Event ID: %llu", m_data_provider.GetSelectedEvent());
        if(ImGui::Button("Find More Info"))
        {
            m_data_provider.GetEventInfo(m_data_provider.m_selected_event, m_data_provider.m_selected_event_start, m_data_provider.m_selected_event_end);
        }
    }
    ImGui::EndChild();
}

}  // namespace View
}  // namespace RocProfVis