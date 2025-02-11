#include "flame_chart.h"
#include "imgui.h"
#include <algorithm>
#include <limits>
#include <iostream>
 #include "grid.h"

FlameChart::FlameChart(int count, double minValue, double maxValue, float zoom,
                       float movement, double minX, double maxX,
                       const std::vector<rocprofvis_trace_event_t>& data_arr)
: minValue(minValue)
, maxValue(maxValue)
, zoom(zoom)
, movement(movement)
, minX(minX)
,count(count)
, maxX(maxX)
, min_start_time(std::numeric_limits<double>::max())
{
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
FlameChart::render() const
{

        ImGuiWindowFlags window_flags =
        ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoMove;

           if(ImGui::BeginChild((std::to_string(count)).c_str()), ImVec2(0, 50), true,
           window_flags)
        {
            int    count       = 0;
            float  total_width = 0.0f;
            ImVec2 cSize       = ImGui::GetContentRegionAvail();
            ImVec2 cPosition   = ImGui::GetCursorScreenPos();
             float vWidth             = (maxX - minX) / zoom;
            float vMinX              = minX + movement;
            float vMaxX              = vMinX + vWidth;
  
             float scaleX = cSize.x / (vMaxX - vMinX);
 
 
            for(const auto& flame : flames)
            {
                float normalized_start = (flame.m_start_ts - (minX + movement)) * scaleX;

                float duration = static_cast<float>(flame.m_duration * zoom) * scaleX;

                ImVec2 startPosition =
                    ImVec2(normalized_start,
                           0);  // Scale the start time for better visualization
                ImVec2 endPosition = ImVec2((normalized_start + duration),
                                            20);  // Scale and set the height

                ImGui::GetWindowDrawList()->AddRectFilled(startPosition, endPosition,
                                                          IM_COL32(255, 100, 100, 255));
                ImGui::PushID(static_cast<int>(count));
                ImGui::SetCursorPosX(startPosition.x);
                ImGui::SetCursorPosY(startPosition.y);
                ImGui::Button(flame.m_name.c_str(), ImVec2(duration, 20));
                if(ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("%s\nStart: %.2f\nDuration: %.2f",
                                      flame.m_name.c_str(), flame.m_start_ts,
                                      flame.m_duration);
                }
                ImGui::PopID();

                // Calculate total width for the flame chart
                total_width = std::max(total_width, endPosition.x);
                count       = count + 1;
            }

   

             
        }

    ImGui::EndChild();
}
