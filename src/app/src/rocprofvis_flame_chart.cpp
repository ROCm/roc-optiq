#include "rocprofvis_flame_chart.h"
#include "imgui.h"
#include "rocprofvis_grid.h"
#include <algorithm>
#include <iostream>
#include <limits>
#include <map>
FlameChart::FlameChart(int chart_id, double min_value, double max_value, float zoom,
                       float movement, double min_x, double max_x,
                       const std::vector<rocprofvis_trace_event_t>& data_arr,
                       float                                        scale_x)
: min_value(min_value)
, max_value(max_value)
, zoom(zoom)
, movement(movement)
, min_x(min_x)
, chart_id(chart_id)
, max_x(max_x)
, scale_x(scale_x)
, min_start_time(std::numeric_limits<double>::max())
{
    this->graph_depth = 0;
    if(!data_arr.empty())
    {
        for(const auto& event : data_arr)
        {
            if(event.m_start_ts < min_start_time)
            {
                min_start_time = event.m_start_ts;
            }
        }
        flames.insert(flames.end(), data_arr.begin(), data_arr.end());
    }
}
void
FlameChart::DrawBox(ImVec2 start_position, int boxplot_box_id,
                    rocprofvis_trace_event_t flame, float duration, ImDrawList* draw_list)
{
    ImGui::PushID(static_cast<int>(boxplot_box_id));

    // Get the current window's draw list

    ImVec2 cursor_position = ImGui::GetCursorScreenPos();
    ImVec2 content_size    = ImGui::GetContentRegionAvail();
    // Define the start and end positions for the rectangle
    ImVec2 rectMin =
        ImVec2(start_position.x,
               start_position.y + cursor_position.y);  // Start position (top-left corner)
    ImVec2 rectMax = ImVec2(start_position.x + duration,
                            start_position.y + 40 +
                                cursor_position.y);  // End position (bottom-right corner)

    ImU32 rectColor = IM_COL32(0, 0, 0, 85);  // Red color

    draw_list->AddRectFilled(rectMin, rectMax, rectColor);

    if(ImGui::IsMouseHoveringRect(rectMin, rectMax))
    {
        ImGui::SetTooltip("%s\nStart: %.2f\nDuration: %.2f ",
                          flame.m_name.c_str(), flame.m_start_ts - min_x,
                          flame.m_duration);
    }

    ImGui::PopID();
}

void
FlameChart::render()
{
    ImGuiWindowFlags window_flags =
        ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoMove;
    int box_at_each_level_max_depth = 1;

    if(ImGui::BeginChild((std::to_string(chart_id)).c_str()), ImVec2(0, 50), true,
       window_flags)
    {
        int    boxplot_box_id = 0;
        float  total_width    = 0.0f;
        ImVec2 content_size   = ImGui::GetContentRegionAvail();
        float  level          = 0;
        float  previous       = 0;

        ImDrawList* draw_list = ImGui::GetWindowDrawList();

        std::map<int, float> box_at_each_level = {};
        for(const auto& flame : flames)
        {
            float normalized_start = (flame.m_start_ts - (min_x + movement)) * scale_x;

            // float duration = static_cast<float>(flame.m_duration * zoom) * scale_x;
            float normalized_end =
                 flame.m_duration * scale_x;
 
            float fullBoxSize = normalized_start + normalized_end;

            ImVec2 start_position;
            ImVec2 end_position;

         
                box_at_each_level[0] = fullBoxSize;
                start_position =
                    ImVec2(normalized_start,
                           0);  // Scale the start time for better visualization

                DrawBox(start_position, boxplot_box_id, flame, normalized_end, draw_list);
        
           
 
             boxplot_box_id = boxplot_box_id + 1;
        }
    }

    ImGui::EndChild();
    graph_depth = box_at_each_level_max_depth;
}
