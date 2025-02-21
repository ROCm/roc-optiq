#include "flame_chart.h"
#include "grid.h"
#include "imgui.h"
#include <algorithm>
#include <iostream>
#include <limits>
#include <map>
FlameChart::FlameChart(int chart_id, double min_value, double max_value, float zoom,
                       float movement, double min_x, double max_x,
                       const std::vector<rocprofvis_trace_event_t>& data_arr)
: min_value(min_value)
, max_value(max_value)
, zoom(zoom)
, movement(movement)
, min_x(min_x)
, chart_id(chart_id)
, max_x(max_x)
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
FlameChart::DrawBox(ImVec2 start_position, ImVec2 end_position, int boxplot_box_id,
                    rocprofvis_trace_event_t flame, float duration)
{
    ImGui::PushStyleColor(ImGuiCol_Button,
                          ImVec4(200.0f / 255.0f, 24.0f / 255.0f, 30.0f / 255.0f,
                                 1.0f));  // Change button color to AMD Red
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                          ImVec4(200.0f / 255.0f, 24.0f / 255.0f, 30.0f / 255.0f,
                                 1.0f));  // Change button hover color to AMD Red
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                          ImVec4(200.0f / 255.0f, 24.0f / 255.0f, 30.0f / 255.0f,
                                 1.0f));  // Change button active color to AMD Red
 

    ImGui::PushID(static_cast<int>(boxplot_box_id));
    ImGui::SetCursorPosX(start_position.x);
    ImGui::SetCursorPosY(start_position.y);
    ImGui::Button(flame.m_name.c_str(), ImVec2(duration, 40));
    if(ImGui::IsItemHovered())
    {

        ImGui::SetTooltip("%s\nStart: %.2f\nDuration: %.2f", flame.m_name.c_str(),
                          flame.m_start_ts, flame.m_duration);
    }
    ImGui::PopID();
    ImGui::PopStyleColor(3);  // Restore previous colors
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
        int                  boxplot_box_id    = 0;
        float                total_width       = 0.0f;
        ImVec2               content_size      = ImGui::GetContentRegionAvail();
        float                v_width           = (max_x - min_x) / zoom;
        float                v_min_x           = min_x + movement;
        float                v_max_x           = v_min_x + v_width;
        float                level             = 0;
        float                previous          = 0;
        float                scale_x           = content_size.x / (v_max_x - v_min_x);
        std::map<int, float> box_at_each_level = {};
        for(const auto& flame : flames)
        {
            float normalized_start = (flame.m_start_ts - (min_x + movement)) * scale_x;

            float duration = static_cast<float>(flame.m_duration * zoom) * scale_x;

            float fullBoxSize = normalized_start + duration;
            ;

            ImVec2 start_position;
            ImVec2 end_position;
        
            if(box_at_each_level.empty())
            {
                box_at_each_level[0] = fullBoxSize;
                start_position =
                    ImVec2(normalized_start,
                           0);  // Scale the start time for better visualization
                end_position = ImVec2((normalized_start + duration),
                                      0);  // Scale and set the height

                DrawBox(start_position, end_position, boxplot_box_id, flame, duration);
            }
            else
            {
                auto iterator = box_at_each_level.rbegin();
                while(iterator != box_at_each_level.rend())
                {
                    if(normalized_start < iterator->second)
                    {
                        // We know the box starts inside the duration.
                        // Create a new level.
                        box_at_each_level[iterator->first + 45] =
                            fullBoxSize;  // Create new
                                          // level.
                        // plot here
                        start_position = ImVec2(
                            normalized_start,
                            iterator->first );  // Scale the start time for better visualization
                        end_position =
                            ImVec2((normalized_start + duration),
                                   iterator->first);  // Scale and set the height

                        DrawBox(start_position, end_position, boxplot_box_id, flame,
                                duration);

                        // Plot here.
                        break;  // Exit the loop after placing the box.
                    }
                    else if(normalized_start > iterator->second)
                    {
                        // New level, pop previous level.
                        if(iterator->first == 0)
                        {
                            // Last level, gotta reset the dictionary.
                            box_at_each_level.clear();
                            // Plot here.

                            box_at_each_level[0] = fullBoxSize;  // Create new level.
                            // plot here
                            start_position = ImVec2(
                                normalized_start,
                                0);  // Scale the start time for better visualization
                            end_position = ImVec2((normalized_start + duration),
                                                  0);  // Scale and set the height

                            DrawBox(start_position, end_position, boxplot_box_id, flame,
                                    duration);

                            break;  // Exit the loop after resetting.
                        }
                      
                    }
                    ++iterator;  // Move to the next element in reverse order.
                }
            }

            previous = fullBoxSize;

            // if the start is in the duraation of the previous box go down a level and
            // append to the level var if the next one is not in the previous ones span
            // drop down a level then check if its in the duration of the previous one if
            // not drop down a var and then repeat until it either finds one in its pan or
            // drop level all the way to 0 again and plot and repeat cycle

            // Calculate total width for the flame chart
            total_width    = std::max(total_width, end_position.x);
            boxplot_box_id = boxplot_box_id + 1;
        }
    }

    ImGui::EndChild();
    graph_depth = box_at_each_level_max_depth;
 }
