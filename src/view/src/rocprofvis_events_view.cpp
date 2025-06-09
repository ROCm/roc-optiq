// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_events_view.h"
#include "imgui.h"

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

EventsView::EventsView() {}

EventsView::~EventsView() {}

void
EventsView::Render()
{
    ImGui::BeginChild("EventsView", ImVec2(0, 0), true);
    ImGui::Text("Events View Content");
    ImGui::EndChild();
}

}  // namespace View
}  // namespace RocProfVis