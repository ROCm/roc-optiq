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

SideBar::SideBar()
: m_dropdown_select(0)
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
        std::cout << "No graph tree!! " << std::endl;
        return;
    }

    if(ImGui::CollapsingHeader("Project 1"))
    {
        for(auto& tree_item : *tree)
        {
            ImGui::PushStyleColor(ImGuiCol_Header, IM_COL32(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, IM_COL32(0, 0, 0, 0));

            ImGui::PushStyleColor(ImGuiCol_HeaderActive, IM_COL32(0, 0, 0, 0));

            if(ImGui::CollapsingHeader(("Chart # " + std::to_string(tree_item.first) +
                                        ": " + tree_item.second.chart->GetName())
                                           .c_str()))
            {
                tree_item.second.selected = ImVec4(0.17, 0.54, 1.0f, 0.3f);

                if(ImGui::Checkbox(
                       (" Enable/Disable Chart #" + std::to_string((tree_item.first)))
                           .c_str(),
                       &tree_item.second.display))
                {
                }
                if(tree_item.second.graph_type == rocprofvis_graph_map_t::TYPE_LINECHART)
                {
                    if(ImGui::Checkbox(
                           (" Color By Value #" + std::to_string((tree_item.first)))
                               .c_str(),
                           &tree_item.second.color_by_value))

                    {
                    }
                    if(ImGui::Checkbox("Convert to Boxplot",
                                       &tree_item.second.make_boxplot))

                    {
                        std::cout << "I convert to boxplot" << std::endl;
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
            }
            else
            {
                tree_item.second.selected = ImVec4(0, 0, 0, 0);
            }
            ImGui::PopStyleColor(3);
        }
    }
}
