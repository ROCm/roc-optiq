// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_line_chart.h"
#include "imgui.h"
#include "rocprofvis_grid.h"
#include "rocprofvis_structs.h"
#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

namespace RocProfVis
{
namespace View
{

LineChart::LineChart(int id, float min_value, float max_value, float zoom, float movement,
                     float& min_x, float& max_x, float& min_y, float& max_y,
                     std::vector<rocprofvis_data_point_t> data, float scale_x)
: m_id(id)
, m_min_value(min_value)
, m_max_value(max_value)
, m_zoom(zoom)
, m_movement(movement)
, m_min_x(min_x)
, m_max_x(max_x)
, m_min_y(min_y)
, m_max_y(max_y)
, m_scale_x(scale_x)
, m_data(data)
{}

LineChart::~LineChart() 
{
}

void
LineChart::Render()
{
    ImGuiWindowFlags window_flags =
        ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoMove;

    if(ImGui::BeginChild((std::to_string(m_id)).c_str()), true, window_flags)
    {
        ImDrawList* draw_list = ImGui::GetWindowDrawList();

        ImVec2 cursor_position = ImGui::GetCursorScreenPos();
        ImVec2 content_size    = ImGui::GetContentRegionAvail();

        float scale_y = content_size.y / (m_max_y - m_min_y);

        for(int i = 1; i < m_data.size(); i++)
        {
            ImVec2 point_1 =
                MapToUI(m_data[i - 1], cursor_position, content_size, m_scale_x, scale_y);
            ImVec2 point_2 =
                MapToUI(m_data[i], cursor_position, content_size, m_scale_x, scale_y);
            draw_list->AddLine(point_1, point_2, IM_COL32(0, 0, 0, 255), 2.0f);
        }
    }

    ImGui::EndChild();
}

ImVec2
LineChart::MapToUI(rocprofvis_data_point_t& point, ImVec2& cursor_position,
                   ImVec2& content_size, float scaleX, float scaleY)
{
    float x = (point.xValue - (m_min_x + m_movement)) * scaleX;
    float y = cursor_position.y + content_size.y - (point.yValue - m_min_y) * scaleY;

    return ImVec2(x, y);
}

}  // namespace View
}  // namespace RocProfVis
