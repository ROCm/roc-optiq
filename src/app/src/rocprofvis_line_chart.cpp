#include "rocprofvis_line_chart.h"
#include "rocprofvis_grid.h"
#include "imgui.h"
#include <algorithm>
#include <iostream>
#include <string>
#include <vector>
template <typename T>
T
clamp(const T& value, const T& lower, const T& upper)
{
    if(value < lower)
    {
        return lower;
    }
    else if(value > upper)
    {
        return upper;
    }
    else
    {
        return value;
    }
}

LineChart::LineChart(int id, float min_value, float max_value, float zoom, float movement,
                     bool has_zoom_happened, float& min_x, float& max_x, float& min_y,
                     float& max_y, std::vector<dataPoint> data)

{
    this->id                = id;
    this->min_value         = min_value;
    this->max_value         = max_value;
    this->zoom              = zoom;
    this->movement          = movement;
    this->has_zoom_happened = has_zoom_happened;
    this->min_x             = min_x;
    this->max_x             = max_x;
    this->min_y             = min_y;
    this->max_y             = max_y;
    has_zoom_happened       = false;
    this->data              = data;
}

LineChart::~LineChart() {}

void
LineChart::RenderGrid()
{}

void
LineChart::AddDataPoint(float x, float y)
{
    data.push_back({ x, y });
}

void
LineChart::Render()
{
    ImGuiWindowFlags window_flags =
        ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoMove;

    if(ImGui::BeginChild((std::to_string(id)).c_str()), true, window_flags)
    {
        ImDrawList* draw_list = ImGui::GetWindowDrawList();

        ImVec2 cursor_position = ImGui::GetCursorScreenPos();
        ImVec2 content_size     = ImGui::GetContentRegionAvail();

        float v_width = (max_x - min_x) / zoom;
        float v_min_x  = min_x + movement;
        float v_max_x  = v_min_x + v_width;
        float scale_x   = content_size.x / (v_max_x - v_min_x);
        float scale_y = content_size.y / (max_y - min_y);

        draw_list->AddLine(ImVec2(10, 10), ImVec2(20, 20), IM_COL32(0, 0, 0, 255), 2.0f);

        for(int i = 1; i < data.size(); i++)
        {
            ImVec2 point_1 =
                MapToUI(data[i - 1], cursor_position, content_size, scale_x, scale_y);
            ImVec2 point_2 =
                MapToUI(data[i], cursor_position, content_size, scale_x, scale_y);
            draw_list->AddLine(point_1, point_2, IM_COL32(0, 0, 0, 255), 2.0f);
        }
    }

    ImGui::EndChild();
}

ImVec2
LineChart::MapToUI(dataPoint& point, ImVec2& cursor_position, ImVec2& content_size,
                   float scaleX,
                   float scaleY)
{
    float x = (point.xValue - (min_x + movement)) * scaleX;
    float y = cursor_position.y + content_size.y - (point.yValue - min_y) * scaleY;

    return ImVec2(x, y);
}
