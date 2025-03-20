// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_line_chart.h"
#include "imgui.h"
#include "rocprofvis_grid.h"
#include "rocprofvis_structs.h"
#include <algorithm>
#include <iostream>
#include <string>
#include <vector>
#include "rocprofvis_charts.h"

namespace RocProfVis
{
namespace View
{

LineChart::LineChart(int id, std::string name, float zoom, float movement, float& min_x,
                     float& max_x, float scale_x, void* datap)
: m_id(id)
, m_zoom(zoom)
, m_movement(movement)
, m_min_x(min_x)
, m_max_x(max_x)
, m_min_y(0)
, m_max_y(0)
, m_scale_x(scale_x)
, m_data({})
, datap(datap)
, m_name(name)
, size(290.0f)
{}

LineChart::~LineChart() {}
float
LineChart::ReturnSize(){return size;}



int
LineChart::ReturnChartID()
{
    return m_id;
}
void 
LineChart::SetID(int id)
{
    m_id = id;
}


std::vector<rocprofvis_data_point_t>
LineChart::ExtractPointsFromData()
{
    auto* counters_vector = static_cast<std::vector<rocprofvis_trace_counter_t>*>(datap);

    std::vector<rocprofvis_data_point_t> aggregated_points;

    ImVec2 display_size = ImGui::GetIO().DisplaySize;
    int    screen_width = static_cast<int>(display_size.x);

    float effectiveWidth = screen_width / m_zoom;
    float bin_size       = (m_max_x - m_min_x) / effectiveWidth;

    double bin_sum_x         = 0.0;
    double bin_sum_y         = 0.0;
    int    bin_count         = 0;
    double current_bin_start = counters_vector->at(0).m_start_ts;

    for(const auto& counter : *counters_vector)
    {
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
                binned_point.xValue = bin_sum_x / bin_count;
                binned_point.yValue = bin_sum_y / bin_count;
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
        binned_point.xValue = bin_sum_x / bin_count;
        binned_point.yValue = bin_sum_y / bin_count;
        aggregated_points.push_back(binned_point);
    }

    m_data = aggregated_points;
    return aggregated_points;
}

std::tuple<float, float>
LineChart::FindMaxMin()
{
    m_min_y = m_data[0].yValue;
    m_max_y = m_data[0].yValue;
    m_min_x = m_data[0].xValue;
    m_max_x = m_data[0].xValue;

    for(const auto& point : m_data)
    {
        if(point.xValue < m_min_x)
        {
            m_min_x = point.xValue;
        }
        if(point.xValue > m_max_x)
        {
            m_max_x = point.xValue;
        }
        if(point.yValue < m_min_y)
        {
            m_min_y = point.yValue;
        }
        if(point.yValue > m_max_y)
        {
            m_max_y = point.yValue;
        }
    }
    std::cout << m_min_x << " line " << m_max_x << std::endl;

    return std::make_tuple(m_min_x, m_max_x);
}

void
LineChart::UpdateMovement(float zoom, float movement, float& min_x, float& max_x,
                          float scale_x)
{
    m_zoom     = zoom;
    m_movement = movement;
    m_scale_x  = scale_x;
    m_min_x    = min_x;
    m_max_x    = max_x;
}

void
LineChart::Render()
{
    ImGuiWindowFlags window_flags =
        ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoMove;

    if(ImGui::BeginChild((std::to_string(m_id)).c_str()), true, window_flags)
    {
        ImVec2 parent_size   = ImGui::GetContentRegionAvail();
        float  metadata_size = 300.0f;
        float  graph_size    = parent_size.x - metadata_size;
      
        ImGui::BeginChild("MetaData View", ImVec2(metadata_size,size), false);
        ImGui::EndChild();
      

        ImGui::SameLine();
        ImGui::BeginChild("Graph View", ImVec2(graph_size, size), false);
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
        ImGui::EndChild();
        
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
