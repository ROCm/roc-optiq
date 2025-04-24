// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_boxplot.h"
#include "imgui.h"
#include "rocprofvis_controller.h"
#include "rocprofvis_core_assert.h"
#include <algorithm>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace RocProfVis
{
namespace View
{

BoxPlot::BoxPlot(int id, std::string name, float zoom, float movement, double min_x,
                 double max_x, float scale_x)
: Charts(id, name, zoom, movement, min_x, max_x, scale_x)
, m_min_y(0)
, m_max_y(0)
, m_data({})
, m_color_by_value_digits()
, m_is_color_value_existant(false)
{}

BoxPlot::~BoxPlot() {}

void
BoxPlot::SetColorByValue(rocprofvis_color_by_value_t color_by_value_digits)
{
    m_color_by_value_digits   = color_by_value_digits;
    m_is_color_value_existant = true;
}

void
BoxPlot::ExtractPointsFromData(const RawTrackSampleData* sample_track)
{
    std::vector<rocprofvis_data_point_t> aggregated_points;

    ImVec2 display_size = ImGui::GetIO().DisplaySize;
    int    screen_width = static_cast<int>(display_size.x);

    double effective_width = screen_width / m_zoom;
    double bin_size        = (m_max_x - m_min_x) / effective_width;

    double bin_sum_x         = 0.0;
    double bin_sum_y         = 0.0;
    int    bin_count         = 0;
    double current_bin_start = DBL_MAX;

    const std::vector<rocprofvis_trace_counter_t> track_data = sample_track->GetData();
    uint64_t                                      count      = track_data.size();

    rocprofvis_trace_counter_t counter;

    for(uint64_t i = 0; i < count; i++)
    {
        counter.m_start_ts = track_data[i].m_start_ts;
        counter.m_value    = track_data[i].m_value;

        if(i == 0)
        {
            current_bin_start = counter.m_start_ts;
        }

        if(counter.m_start_ts < current_bin_start + bin_size)
        {
            bin_sum_x += counter.m_start_ts;
            bin_sum_y += counter.m_value;
            bin_count++;
        }
        else
        {
            if(bin_count > 0)
            {
                rocprofvis_data_point_t binned_point;
                binned_point.x_value = bin_sum_x / bin_count;
                binned_point.y_value = bin_sum_y / bin_count;
                aggregated_points.push_back(binned_point);
            }
            current_bin_start += bin_size;
            bin_sum_x = counter.m_start_ts;
            bin_sum_y = counter.m_value;
            bin_count = 1;
        }
    }

    if(bin_count > 0)
    {
        rocprofvis_data_point_t binned_point;
        binned_point.x_value = bin_sum_x / bin_count;
        binned_point.y_value = bin_sum_y / bin_count;
        aggregated_points.push_back(binned_point);
    }

    m_data = aggregated_points;
}

std::tuple<double, double>
BoxPlot::FindMaxMin()
{
    m_min_y = m_data[0].y_value;
    m_max_y = m_data[0].y_value;
    m_min_x = m_data[0].x_value;
    m_max_x = m_data[0].x_value;

    for(const auto& point : m_data)
    {
        if(point.x_value < m_min_x)
        {
            m_min_x = point.x_value;
        }
        if(point.x_value > m_max_x)
        {
            m_max_x = point.x_value;
        }
        if(point.y_value < m_min_y)
        {
            m_min_y = point.y_value;
        }
        if(point.y_value > m_max_y)
        {
            m_max_y = point.y_value;
        }
    }

    return std::make_tuple(m_min_x, m_max_x);
}

float
BoxPlot::CalculateMissingX(float x_1, float y_1, float x_2, float y_2, float known_y)
{
    // Calculate slope (m)
    double m = (y_2 - y_1) / (x_2 - x_1);

    // Calculate y-intercept (b)
    double b = y_1 - m * x_1;

    // Calculate x for the given y
    double x = (known_y - b) / m;

    return x;
}

void
BoxPlot::RenderMetaArea()
{
    ImGui::BeginChild("MetaData View", ImVec2(s_metadata_width, m_track_height), false);

    ImGui::BeginChild("MetaData Content",
                      ImVec2(s_metadata_width - 70.0f, m_track_height), false);
    ImGui::Text(m_name.c_str());
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("MetaData Scale", ImVec2(70.0f, m_track_height), false);
    ImGui::Text((std::to_string(m_max_y)).c_str());

    if(ImGui::IsItemVisible())
    {
        m_is_in_view_vertical = true;
    }
    else
    {
        m_is_in_view_vertical = false;
    }

    ImVec2 child_window_size = ImGui::GetWindowSize();
    ImVec2 text_size         = ImGui::CalcTextSize("Scale Size");
    ImGui::SetCursorPos(
        ImVec2(0, child_window_size.y - text_size.y - ImGui::GetStyle().WindowPadding.y));

    ImGui::Text((std::to_string(m_min_y)).c_str());

    ImGui::EndChild();

    ImGui::EndChild();
}

void
BoxPlot::RenderChart(float graph_width)
{
    ImGui::BeginChild("Graph View", ImVec2(graph_width, m_track_height), false);
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    ImVec2 cursor_position = ImGui::GetCursorScreenPos();
    ImVec2 content_size    = ImGui::GetContentRegionAvail();

    float scale_y = content_size.y / (m_max_y - m_min_y);
    for(int i = 1; i < m_data.size(); i++)
    {
        ImVec2 point_1 =
            MapToUI(m_data[i - 1], cursor_position, content_size, m_scale_x, scale_y);
        if(ImGui::IsMouseHoveringRect(ImVec2(point_1.x - 10, point_1.y - 10),
                                      ImVec2(point_1.x + 10, point_1.y + 10)))
        {
            ImGui::BeginTooltip();
            ImGui::Text(std::to_string(m_data[i - 1].x_value - m_min_x).c_str());
            ImGui::EndTooltip();
        }
        ImVec2 point_2 =
            MapToUI(m_data[i], cursor_position, content_size, m_scale_x, scale_y);
        ImU32 box_color =
            IM_COL32(0, 0, 0, 200);  // Used for color by value (to be added later).

        float bottom_of_chart =
            cursor_position.y + content_size.y - (m_min_y - m_min_y) * scale_y;

        draw_list->AddRectFilled(
            point_1, ImVec2(point_1.x + (point_2.x - point_1.x), bottom_of_chart),
            box_color, 2.0f);
    }
    ImGui::EndChild();
}

void
BoxPlot::Render()
{
    Charts::Render();
}

ImVec2
BoxPlot::MapToUI(rocprofvis_data_point_t& point, ImVec2& cursor_position,
                 ImVec2& content_size, float scaleX, float scaleY)
{
    float x = (point.x_value - (m_min_x + m_movement)) * scaleX;
    float y = cursor_position.y + content_size.y - (point.y_value - m_min_y) * scaleY;

    return ImVec2(x, y);
}

}  // namespace View
}  // namespace RocProfVis
