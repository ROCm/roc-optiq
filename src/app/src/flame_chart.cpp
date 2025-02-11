#include "flame_chart.h"
#include "imgui.h"
#include <algorithm>
#include <limits>

FlameChart::FlameChart(int count, double minValue, double maxValue, float zoom,
                       float movement, double minX, double maxX,
                       const std::vector<rocprofvis_trace_event_t>& data_arr)
: minValue(minValue)
, maxValue(maxValue)
, zoom(zoom)
, movement(movement)
, minX(minX)
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
    ImGui::BeginChild("Parent", ImVec2(0, 150), true,
                      ImGuiWindowFlags_HorizontalScrollbar);
    ImGui::BeginChild("FlameChart", ImVec2(0, 0), true);

    float total_width = 0.0f;
    for(const auto& flame : flames)
    {
        float normalized_start =
            static_cast<float>((flame.m_start_ts - min_start_time) * zoom + movement);
        float duration = static_cast<float>(flame.m_duration * zoom);

        ImVec2 startPosition = ImVec2(
            normalized_start * 0.1f, 0);  // Scale2 the start time for better visualization
        ImVec2 endPosition =
            ImVec2((normalized_start + duration) * 0.1f, 20);  // Scale and set the height

        ImGui::GetWindowDrawList()->AddRectFilled(startPosition, endPosition,
                                                  IM_COL32(255, 100, 100, 255));

        ImGui::SetCursorPosX(startPosition.x);
        ImGui::SetCursorPosY(startPosition.y);
        ImGui::Button(flame.m_name.c_str(), ImVec2(duration * 0.1f, 20));
        if(ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("%s\nStart: %.2f\nDuration: %.2f", flame.m_name.c_str(),
                              flame.m_start_ts, flame.m_duration);
        }

        // Calculate total width for the flame chart
        total_width = std::max(total_width, endPosition.x);
    }

    // Ensure the child window has the appropriate width for horizontal scrolling
    ImGui::Dummy(ImVec2(total_width, 0));  // Add a dummy element to set the total width

    ImGui::EndChild();
    ImGui::EndChild();
}
