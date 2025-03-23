#include "rocprofvis_sidebar.h"
#include "imgui.h"
#include "rocprofvis_structs.h"
#include <iostream>
#include <map>
#include <string>
#include <vector>
#include <utility>

SideBar::SideBar()
: m_dropdown_select(0)
{}

SideBar::~SideBar() {}

void
SideBar::Render()
{}
void
SideBar::ConstructTree(std::map<int, rocprofvis_graph_map_t>* tree)
{
    if(!tree) return;
    if(ImGui::CollapsingHeader("Project 1"))
    {
        for(auto& tree_item : *tree)
        {
            if(ImGui::CollapsingHeader(("Chart # " + std::to_string(tree_item.first) +
                                        ": " + tree_item.second.chart->GetName())
                                           .c_str()))
            {
                tree_item.second.selected = ImVec4(0, 0, 1, 0.1);

                if(ImGui::Checkbox(
                       (" Enable/Disable Chart#" + std::to_string((tree_item.first)))
                           .c_str(),
                       &tree_item.second.display))
                {
                }
                if(ImGui::Checkbox(
                       (" Color By Value#" + std::to_string((tree_item.first))).c_str(),
                       &tree_item.second.display))

                {
                    
                }

                static float range_1 = 0.0f;
                ImGui::Text("Interval 1");
                ImGui::InputFloat("Min Value", &range_1); 

                ImGui::SameLine();

                  static float range_2 = 0.0f;
                ImGui::Text("Interval 2");
                ImGui::InputFloat("Max Value", &range_2); 

                if (ImGui::Button("Sub")) {
                    tree_item.second.red_range = {range_1, range_2};
                    tree_item.second.color_by_value = true;
                }
            }
            else
            {
                tree_item.second.selected = ImVec4(0, 0, 0, 0);
            }
        }
    }
}