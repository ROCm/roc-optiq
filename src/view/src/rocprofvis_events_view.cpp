// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_events_view.h"
#include "imgui.h"
#include "rocprofvis_data_provider.h"
#include "rocprofvis_event_manager.h"
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
: m_data_provider(dp)
, m_last_selected_event(std::numeric_limits<uint64_t>::max())
{
    std::shared_ptr<RocWidget> left_panel =
        std::make_shared<RocCustomWidget>([this]() { this->RenderLeftPanel(); });
    std::shared_ptr<RocWidget> right_panel =
        std::make_shared<RocCustomWidget>([this]() { this->RenderRightPanel(); });

    LayoutItem left;
    left.m_item           = left_panel;
    left.m_window_padding = ImVec2(4, 4);
    LayoutItem right;
    right.m_item           = right_panel;
    right.m_window_padding = ImVec2(4, 4);

    m_h_spilt_container = std::make_shared<HSplitContainer>(left, right);
    m_h_spilt_container->SetMinLeftWidth(10.0f);
    m_h_spilt_container->SetMinRightWidth(10.0f);
    m_h_spilt_container->SetSplit(0.6f);
}

EventsView::~EventsView() {}

void
EventsView::Render()
{
    m_h_spilt_container->Render();
}

void
EventsView::RenderLeftPanel()
{
    if(ImGui::BeginChild("LeftPanel", ImVec2(0, 0), ImGuiChildFlags_None,
                         ImGuiWindowFlags_NoMove))
    {
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
                    m_data_provider.FetchEventExtData(
                        m_data_provider.GetSelectedEventId());
                    m_data_provider.FetchEventFlowDetails(
                        m_data_provider.GetSelectedEventId());
                    m_data_provider.FetchEventCallStackData(
                        m_data_provider.GetSelectedEventId());
                }
                m_last_selected_event = selected_event;
            }

            const event_info_t& eventInfo = m_data_provider.GetEventInfoStruct();
            if(!eventInfo.ext_data.empty())
            {
                ImGui::Separator();
                ImGui::NewLine();

                RenderEventExtData(eventInfo.ext_data);
            }

            const flow_info_t& flowInfo = m_data_provider.GetFlowInfo();

            ImGui::NewLine();
            ImGui::Separator();
            RenderEventFlowInfo(flowInfo.flow_data);
            // Run arrows after flow because all info needed is now there.
            auto evt = std::make_shared<CreateArrowsViewEvent>(
                static_cast<int>(RocEvents::kHandleUserArrowCreationEvent));
            EventManager::GetInstance()->AddEvent(evt);
        }
    }
    ImGui::EndChild();
}

void
EventsView::RenderRightPanel()
{
    if(ImGui::BeginChild("RightPanel", ImVec2(0, 0), ImGuiChildFlags_None,
                         ImGuiWindowFlags_NoMove))
    {
        const call_stack_info_t& call_stack = m_data_provider.GetCallStackInfo();
        if(!call_stack.call_stack_data.empty())
        {
            RenderCallStackData(call_stack.call_stack_data);
        }
        else
        {
            ImGui::Text("Call stack data not available");
        }
    }
    ImGui::EndChild();
}

void
EventsView::RenderEventExtData(const std::vector<event_ext_data_t>& ext_data)
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
EventsView::RenderEventFlowInfo(const std::vector<event_flow_data_t>& flow_data)
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

void
EventsView::RenderCallStackData(const std::vector<call_stack_data_t>& call_stack_data)
{
    ImGui::TextUnformatted("Call Stack Data");
    ImGui::Separator();

    if(ImGui::BeginTable("CallStackTable", 5,
                         ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                             ImGuiTableFlags_Resizable))
    {
        ImGui::TableSetupColumn("Line");
        ImGui::TableSetupColumn("Function");
        ImGui::TableSetupColumn("Arguments");
        ImGui::TableHeadersRow();

        for(const auto& item : call_stack_data)
        {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(item.line.c_str());
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(item.function.c_str());
            ImGui::TableSetColumnIndex(2);
            ImGui::TextUnformatted(item.arguments.c_str());
        }
        ImGui::EndTable();
    }
}

}  // namespace View
}  // namespace RocProfVis
