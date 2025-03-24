#include "rocprofvis_sidebar.h"
#include "imgui.h"
#include "rocprofvis_structs.h"
#include <iostream>
#include <map>
#include <string>
#include <utility>
#include <vector>

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
                    if(tree_item.second.color_by_value)
                    {
                        ImGui::Text("Color By Value");
                        ImGui::Spacing();
                        ImGui::PushItemWidth(40.0f);

                        ImGui::InputFloat(
                            "Critical Min",
                            &tree_item.second.color_by_value_digits.upper_min);

                        ImGui::InputFloat(
                            "Critical Max",
                            &tree_item.second.color_by_value_digits.upper_max);

                        ImGui::InputFloat(
                            "Warning Min",
                            &tree_item.second.color_by_value_digits.middle_min);

                        ImGui::InputFloat(
                            "Warning Max",
                            &tree_item.second.color_by_value_digits.middle_max);

                        ImGui::InputFloat(
                            "Acceptable Min",
                            &tree_item.second.color_by_value_digits.lower_min);

                        ImGui::InputFloat(
                            "Acceptable Max",
                            &tree_item.second.color_by_value_digits.lower_max);
                        ImGui::PopItemWidth();
                    }
                }
            }
            else
            {
                tree_item.second.selected = ImVec4(0, 0, 0, 0);
            }
        }
    }
}