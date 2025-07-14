// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_sidebar.h"
#include "imgui.h"
#include "rocprofvis_settings.h"
#include "rocprofvis_structs.h"
#include <iostream>
#include <map>
#include <string>
#include <utility>
#include <vector>

using namespace RocProfVis::View;

SideBar::SideBar(DataProvider& dp)
: m_data_provider(dp)
, m_dropdown_select(0)
, m_graphs(nullptr)
, m_settings(Settings::GetInstance())
{}

SideBar::~SideBar() {}

void
SideBar::SetGraphs(std::vector<rocprofvis_graph_t>* graphs)
{
    m_graphs = graphs;
}

void
SideBar::Render()
{
    ConstructTree();
}

void
SideBar::ConstructTree()
{
    
    if(!m_graphs)
    {
        return;
    }

    if(ImGui::CollapsingHeader("Project 1"))
    {
        for (int i = 0; i < (*m_graphs).size(); i ++)
        {
            rocprofvis_graph_t& graph = (*m_graphs)[i];
            ImGui::PushStyleColor(ImGuiCol_Header, m_settings.GetColor(static_cast<int>(
                                                       Colors::kTransparent)));
            ImGui::PushStyleColor(
                ImGuiCol_HeaderHovered,
                m_settings.GetColor(static_cast<int>(Colors::kTransparent)));

            ImGui::PushStyleColor(
                ImGuiCol_HeaderActive,
                m_settings.GetColor(static_cast<int>(Colors::kTransparent)));

            std::string header_label = "Chart #" + std::to_string(i) +
                                       ": " + graph.chart->GetName();
            bool open = ImGui::CollapsingHeader(header_label.c_str());

            if(open)
            {
                std::string btn_id =
                    "Go To Track###GoToTrackBtn" + graph.chart->GetName();
                if(ImGui::Button(btn_id.c_str()))
                {
                    auto evt = std::make_shared<ScrollToTrackByNameEvent>(
                        static_cast<int>(RocEvents::kHandleUserGraphNavigationEvent),
                        graph.chart->GetName());
                    EventManager::GetInstance()->AddEvent(evt);
                }
                if(ImGui::Checkbox(
                       (" Enable/Disable Chart #" + std::to_string(i))
                           .c_str(),
                       &graph.display))
                {
                }
                if(graph.graph_type == rocprofvis_graph_t::TYPE_FLAMECHART)
                {
                    if(ImGui::Checkbox(
                           ("Turn off color #" + std::to_string(i)).c_str(),
                           &graph.colorful_flamechart))

                    {
                        static_cast<FlameTrackItem*>(graph.chart)
                            ->SetRandomColorFlag(graph.colorful_flamechart);
                    }
                }
                if(graph.graph_type == rocprofvis_graph_t::TYPE_LINECHART)
                {
                    if(ImGui::Checkbox(
                           (" Color By Value #" + std::to_string(i))
                               .c_str(),
                           &graph.color_by_value))

                    {
                    }
                    if(ImGui::Checkbox(
                           (" Convert to Boxplot #" + std::to_string(i))
                               .c_str(),
                           &graph.make_boxplot))

                    {
                    }
                    if(graph.color_by_value)
                    {
                        ImGui::Text("Color By Value");
                        ImGui::Spacing();
                        ImGui::PushItemWidth(40.0f);
                        // Use this menu to expand user options in the future
                        ImGui::InputFloat(
                            "Region of Interest Min",
                            &graph.color_by_value_digits.interest_1_min);

                        ImGui::InputFloat(
                            "Region of Interest Max",
                            &graph.color_by_value_digits.interest_1_max);

                        ImGui::PopItemWidth();
                    }
                }

                // Lets you know if component is in Frame. Dev purpose only.
                if(graph.chart->IsInViewVertical())
                {
                    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(m_settings.GetColor(
                                           static_cast<int>(Colors::kTextSuccess))),
                                       "Component Is: ");

                    ImGui::SameLine();
                    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(m_settings.GetColor(
                                           static_cast<int>(Colors::kTextSuccess))),
                                       "In Frame ");
                }
                else
                {
                    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(m_settings.GetColor(
                                           static_cast<int>(Colors::kTextSuccess))),
                                       "Component Is: ");

                    ImGui::SameLine();
                    std::string temp_movement_value =
                        "Not In Frame by: " +
                        std::to_string(graph.chart->GetDistanceToView()) +
                        " units.";
                    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(m_settings.GetColor(
                                           static_cast<int>(Colors::kTextError))),
                                       temp_movement_value.c_str());
                }
            }
            else
            {
                // graph.selected = false;
            }
            ImGui::PopStyleColor(3);
        }
    }
}
