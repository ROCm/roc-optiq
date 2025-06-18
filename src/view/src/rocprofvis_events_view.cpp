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
    ImVec2 avail      = ImGui::GetContentRegionAvail();
    float  left_width = avail.x * 0.33f;  // 33% for left, 67% for right

    ImGui::BeginChild("EventsViewParent", avail, true);

    // Split horizontally: left (EventExtData), right (other content)
    ImGui::BeginChild("LeftPanel", ImVec2(left_width, 0), true,
                      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar);

    // --- Left Panel: Event Extended Data ---
    if(m_data_provider.GetSelectedEvent() == std::numeric_limits<uint64_t>::max())
    {
        ImGui::Text("No event selected.");
    }
    else
    {
        ImGui::Text("Event ID: %llu", m_data_provider.GetSelectedEvent());
        if(ImGui::Button("Find More Info"))
        {
            m_data_provider.GetEventInfo(m_data_provider.m_selected_event,
                                         m_data_provider.m_selected_event_start,
                                         m_data_provider.m_selected_event_end);
        }

        // Show the extended data table if available
        if(!m_data_provider.m_event_info.ext_data.empty())
        {
            ShowEventExtDataPanel(m_data_provider.m_event_info.ext_data);
        }
    }

    ImGui::EndChild();

    ImGui::SameLine();

    // --- Right Panel: Placeholder for other event details or UI ---
    ImGui::BeginChild("RightPanel", ImVec2(0, 0), true,
                      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar);
    ImGui::Text("Event Details / Timeline / Other Content Here");
    // ... add your right-side content here ...
    ImGui::EndChild();

    ImGui::EndChild();

}

void
EventsView::ShowEventExtDataPanel(const std::vector<event_ext_data>& ext_data)
{
    // Create a distinct child region within LeftPanel for Event Extended Data
    ImGui::BeginChild("EventExtDataChild",
                      ImVec2(0, ImGui::GetContentRegionAvail().y * 0.6f), true);

    // Title
    ImGui::TextUnformatted("Event Extended Data");
    ImGui::Separator();

    // Table for the data
    if(ImGui::BeginTable("ExtDataTable", 3,
                         ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                             ImGuiTableFlags_Resizable))
    {
        ImGui::TableSetupColumn("Category");
        ImGui::TableSetupColumn("Name");
        ImGui::TableSetupColumn("Value");
        ImGui::TableHeadersRow();

        for(const auto& item : ext_data)
        {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(item.category.c_str());
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(item.name.c_str());
            ImGui::TableSetColumnIndex(2);
            ImGui::TextUnformatted(item.value.c_str());
        }
        ImGui::EndTable();
    }

    ImGui::EndChild();
}

}  // namespace View
}  // namespace RocProfVis