// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_sidebar.h"
#include "imgui.h"
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
            ImGui::PushStyleColor(ImGuiCol_Header, IM_COL32(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, IM_COL32(0, 0, 0, 0));

            ImGui::PushStyleColor(ImGuiCol_HeaderActive, IM_COL32(0, 0, 0, 0));

            if(ImGui::CollapsingHeader(("Chart #" + std::to_string(tree_item.first) +
                                        ": " + tree_item.second.chart->GetName())
                                           .c_str()))
            {
                tree_item.second.selected = true;

                if(ImGui::Checkbox(
                       (" Enable/Disable Chart #" + std::to_string((tree_item.first)))
                           .c_str(),
                       &tree_item.second.display))
                {
                }
                if(tree_item.second.graph_type == rocprofvis_graph_map_t::TYPE_FLAMECHART)
                {
                    if(ImGui::Checkbox("Turn off color",
                                       &tree_item.second.colorful_flamechart))

                    {
                        static_cast<FlameChart*>(tree_item.second.chart)
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
                    ImGui::TextColored(ImVec4(0, 1, 0, 1), "Component Is: ");

                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(0, 1, 0, 1), "In Frame ");
                }
                else
                {
                    ImGui::TextColored(ImVec4(0, 1, 0, 1), "Component Is: ");

                    ImGui::SameLine();
                    std::string temp_movement_value =
                        "Not In Frame by: " +
                        std::to_string(tree_item.second.chart->GetDistanceToView()) +
                        " units.";
                    ImGui::TextColored(ImVec4(1, 0, 0, 1), temp_movement_value.c_str());
                }
            }
            else
            {
                tree_item.second.selected = false;
            }
            ImGui::PopStyleColor(3);
        }
    }
}
