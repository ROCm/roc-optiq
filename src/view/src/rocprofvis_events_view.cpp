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
, m_data_provider(dp)
, m_last_selected_event(std::numeric_limits<uint64_t>::max())
{}

EventsView::~EventsView() {}

void
EventsView::Render()
{
    ImVec2 avail      = ImGui::GetContentRegionAvail();
    float  left_width = avail.x * 0.4f;

    ImGui::BeginChild("EventsViewParent", avail, true);

    ImGui::BeginChild("LeftPanel", ImVec2(left_width, 0), true,
                      ImGuiWindowFlags_NoMove );

    uint64_t selected_event = m_data_provider.GetSelectedEventId();
    if(selected_event == std::numeric_limits<uint64_t>::max())
    {
        ImGui::Text("No event selected.");
    }
    else
    {
        ImGui::Text("Event ID: %llu", m_data_provider.GetSelectedEventId());
        if(selected_event != m_last_selected_event)
        {
            if(selected_event != std::numeric_limits<uint64_t>::max())
            {
                m_data_provider.FetchEventExtData(m_data_provider.GetSelectedEventId());
                m_data_provider.FetchEventFlowDetails(m_data_provider.GetSelectedEventId());
            }
            m_last_selected_event = selected_event;
        }



        const event_info_t& eventInfo = m_data_provider.GetEventInfoStruct();
        if(!eventInfo.ext_data.empty())
        {
            ImGui::Separator();
            ImGui::NewLine();

            ShowEventExtDataPanel(eventInfo.ext_data);
        }

        const flow_info_t& flowInfo = m_data_provider.GetFlowInfo();
        if(!flowInfo.flow_data.empty())
        {
            ImGui::NewLine();
            ImGui::Separator();
            ShowEventFlowInfoPanel(flowInfo.flow_data);
        }
    }

    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("RightPanel", ImVec2(0, 0), true,
                      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar);
    ImGui::Text("Event Details / Timeline / Other Content Here");
    ImGui::EndChild();

    ImGui::EndChild();
}

void
EventsView::ShowEventExtDataPanel(const std::vector<event_ext_data_t>& ext_data)
{
    ImGui::TextUnformatted("Event Extended Data");
    ImGui::Separator();

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

}

void
EventsView::ShowEventFlowInfoPanel(const std::vector<event_flow_data_t>& flow_data)
{
    ImGui::TextUnformatted("Event Flow Info");
    ImGui::Separator();

    if(ImGui::BeginTable("FlowInfoTable", 4,
                         ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                             ImGuiTableFlags_Resizable))
    {
        ImGui::TableSetupColumn("ID");
        ImGui::TableSetupColumn("Timestamp");
        ImGui::TableSetupColumn("Track ID");
        ImGui::TableSetupColumn("Direction");
        ImGui::TableHeadersRow();

        for(const auto& item : flow_data)
        {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(std::to_string(item.id).c_str());
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(std::to_string(item.timestamp).c_str());
            ImGui::TableSetColumnIndex(2);
            ImGui::TextUnformatted(std::to_string(item.track_id).c_str());
            ImGui::TableSetColumnIndex(3);
            ImGui::TextUnformatted(std::to_string(item.direction).c_str());
        }
        ImGui::EndTable();
    }
}

}  // namespace View
}  // namespace RocProfVis
