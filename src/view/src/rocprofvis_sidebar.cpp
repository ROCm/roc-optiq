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
, m_graph_map(nullptr)
, m_settings(Settings::GetInstance())
{}

SideBar::~SideBar() {}

void
SideBar::SetGraphMap(std::map<int, rocprofvis_graph_map_t>* graph_map)
{
    m_graph_map = graph_map;
}

void
SideBar::Render()
{
    ConstructTree(m_graph_map);
}

void
SideBar::ConstructTree(std::map<int, rocprofvis_graph_map_t>* tree)
{
    if(!tree)
    {
        return;
    }

    if(ImGui::CollapsingHeader("Project 1"))
    {
        for(auto& tree_item : *tree)
        {
            ImGui::PushStyleColor(ImGuiCol_Header, m_settings.GetColor(static_cast<int>(
                                                       Colors::kTransparent)));
            ImGui::PushStyleColor(
                ImGuiCol_HeaderHovered,
                m_settings.GetColor(static_cast<int>(Colors::kTransparent)));

            ImGui::PushStyleColor(
                ImGuiCol_HeaderActive,
                m_settings.GetColor(static_cast<int>(Colors::kTransparent)));

            std::string header_label = "Chart #" + std::to_string(tree_item.first) +
                                       ": " + tree_item.second.chart->GetName();
            bool open = ImGui::CollapsingHeader(header_label.c_str());

            if(open)
            {
                std::string btn_id =
                    "Go To Track###GoToTrackBtn" + tree_item.second.chart->GetName();
                if(ImGui::Button(btn_id.c_str()))
                {
                    auto evt = std::make_shared<ScrollToTrackByNameEvent>(
                        static_cast<int>(RocEvents::kHandleUserGraphNavigationEvent),
                        tree_item.second.chart->GetName());
                    EventManager::GetInstance()->AddEvent(evt);
                }
                if(ImGui::Checkbox(
                       (" Enable/Disable Chart #" + std::to_string((tree_item.first)))
                           .c_str(),
                       &tree_item.second.display))
                {
                }
                if(tree_item.second.graph_type == rocprofvis_graph_map_t::TYPE_FLAMECHART)
                {
                    if(ImGui::Checkbox(
                           ("Turn off color #" + std::to_string(tree_item.first)).c_str(),
                           &tree_item.second.colorful_flamechart))

                    {
                        static_cast<FlameTrackItem*>(tree_item.second.chart)
                            ->SetRandomColorFlag(tree_item.second.colorful_flamechart);
                    }
                }
                if(tree_item.second.graph_type == rocprofvis_graph_map_t::TYPE_LINECHART)
                {
                    if(ImGui::Checkbox(
                           (" Color By Value #" + std::to_string((tree_item.first)))
                               .c_str(),
                           &tree_item.second.color_by_value))

                    {
                    }
                    if(ImGui::Checkbox(
                           (" Convert to Boxplot #" + std::to_string((tree_item.first)))
                               .c_str(),
                           &tree_item.second.make_boxplot))

                    {
                    }
                    if(tree_item.second.color_by_value)
                    {
                        ImGui::Text("Color By Value");
                        ImGui::Spacing();
                        ImGui::PushItemWidth(40.0f);
                        // Use this menu to expand user options in the future
                        ImGui::InputFloat(
                            "Region of Interest Min",
                            &tree_item.second.color_by_value_digits.interest_1_min);

                        ImGui::InputFloat(
                            "Region of Interest Max",
                            &tree_item.second.color_by_value_digits.interest_1_max);

                        ImGui::PopItemWidth();
                    }
                }

                // Lets you know if component is in Frame. Dev purpose only.
                if(tree_item.second.chart->IsInViewVertical())
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
                        std::to_string(tree_item.second.chart->GetDistanceToView()) +
                        " units.";
                    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(m_settings.GetColor(
                                           static_cast<int>(Colors::kTextError))),
                                       temp_movement_value.c_str());
                }
            }
            else
            {
                // tree_item.second.selected = false;
            }
            ImGui::PopStyleColor(3);
        }
    }
}
