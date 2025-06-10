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
    ImGui::Text(std::to_string(m_data_provider.GetSelectedEvent()).c_str());
    ImGui::EndChild();
}

}  // namespace View
}  // namespace RocProfVis