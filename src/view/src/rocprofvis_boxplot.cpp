// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_boxplot.h"
#include "imgui.h"
#include "rocprofvis_controller.h"
#include <algorithm>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace RocProfVis
{
namespace View
{

BoxPlot::BoxPlot(int id, std::string name, float zoom, float movement, float& min_x,
                 float& max_x, float scale_x)
: m_id(id)
, m_zoom(zoom)
, m_movement(movement)
, m_min_x(min_x)
, m_max_x(max_x)
, m_min_y(0)
, m_max_y(0)
, m_scale_x(scale_x)
, m_data({})
, m_name(name)
, m_track_height(290.0f)
, m_color_by_value_digits()
, m_is_color_value_existant(false)
{}

BoxPlot::~BoxPlot() {}
float
BoxPlot::GetTrackHeight()
{
    return m_track_height;  // Create an invisible button with a more area
}

std::string&
BoxPlot::GetName()
{
    return m_name;
}
int
BoxPlot::ReturnChartID()
{
    return m_id;
}
void
BoxPlot::SetID(int id)
{
    m_id = id;
}
void
BoxPlot::SetColorByValue(rocprofvis_color_by_value_t color_by_value_digits)
{
    m_color_by_value_digits = color_by_value_digits;
    m_is_color_value_existant = true;
}

std::vector<rocprofvis_data_point_t>
BoxPlot::ExtractPointsFromData(rocprofvis_controller_array_t* track_data)
{
    std::vector<rocprofvis_data_point_t> aggregated_points;

    ImVec2 display_size = ImGui::GetIO().DisplaySize;
    int    screen_width = static_cast<int>(display_size.x);

    float effectiveWidth = screen_width / m_zoom;
    float bin_size       = (m_max_x - m_min_x) / effectiveWidth;

    double bin_sum_x         = 0.0;
    double bin_sum_y         = 0.0;
    int    bin_count         = 0;
    double current_bin_start = DBL_MAX;

    uint64_t            count  = 0;
    rocprofvis_result_t result = rocprofvis_controller_get_uint64(
        track_data, kRPVControllerArrayNumEntries, 0, &count);
    assert(result == kRocProfVisResultSuccess);

    rocprofvis_trace_counter_t counter;

    for(uint64_t i = 0; i < count; i++)
    {
        rocprofvis_controller_sample_t* sample = nullptr;
        result                                 = rocprofvis_controller_get_object(
            track_data, kRPVControllerArrayEntryIndexed, i, &sample);
        assert(result == kRocProfVisResultSuccess && sample);

        double start_ts = 0;
        result = rocprofvis_controller_get_double(sample, kRPVControllerSampleTimestamp,
                                                  0, &start_ts);
        assert(result == kRocProfVisResultSuccess);

        double value = 0;
        result = rocprofvis_controller_get_double(sample, kRPVControllerSampleValue, 0,
                                                  &value);
        assert(result == kRocProfVisResultSuccess);

        counter.m_start_ts = start_ts;
        counter.m_value    = value;

        if(i == 0)
        {
            current_bin_start = start_ts;
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
BoxPlot::FindMaxMin()
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
BoxPlot::UpdateMovement(float zoom, float movement, float& min_x, float& max_x,
                        float scale_x)
{
    m_zoom     = zoom;
    m_movement = movement;
    m_scale_x  = scale_x;
    m_min_x    = min_x;
    m_max_x    = max_x;
}

void
BoxPlot::Render()
{
    ImGuiWindowFlags window_flags =
        ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoMove;

    if(ImGui::BeginChild((std::to_string(m_id)).c_str()), true, window_flags)
    {
        ImVec2 parent_size   = ImGui::GetContentRegionAvail();
        float  metadata_size = 400.0f;
        float  graph_size    = parent_size.x - metadata_size;

        ImGui::BeginChild("MetaData View", ImVec2(metadata_size, m_track_height), false);

        ImGui::BeginChild("MetaData Content",
                          ImVec2(metadata_size - 70.0f, m_track_height),
                          false);
        ImGui::Text(m_name.c_str());
        ImGui::EndChild();

        ImGui::SameLine();

        ImGui::BeginChild("MetaData Scale", ImVec2(70.0f, m_track_height), false);
        ImGui::Text((std::to_string(m_max_y)).c_str());

        ImVec2 child_window_size = ImGui::GetWindowSize();
        ImVec2 text_size         = ImGui::CalcTextSize("Scale Size");
        ImGui::SetCursorPos(ImVec2(0, child_window_size.y - text_size.y -
                                          ImGui::GetStyle().WindowPadding.y));

        ImGui::Text((std::to_string(m_min_y)).c_str());

        ImGui::EndChild();

        ImGui::EndChild();

        ImGui::SameLine();
        ImGui::BeginChild("Graph View", ImVec2(graph_size, m_track_height), false);
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
                ImGui::Text(std::to_string(m_data[i - 1].xValue - m_min_x).c_str());
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

    // Controls for graph resize.
    ImGuiIO& io              = ImGui::GetIO();
    bool     is_control_held = io.KeyCtrl;
    if(is_control_held)
    {
        ImGui::Selectable(("Move Position Line " + std::to_string(m_id)).c_str(), false,
                          ImGuiSelectableFlags_AllowDoubleClick, ImVec2(0, 20.0f));

        if(ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
        {
            ImVec2 drag_delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);
            m_track_height    = m_track_height + (drag_delta.y);
            ImGui::ResetMouseDragDelta();
            ImGui::EndDragDropSource();
        }
        if(ImGui::BeginDragDropTarget())
        {
            ImGui::EndDragDropTarget();
        }
    }

    ImGui::EndChild();
}

ImVec2
BoxPlot::MapToUI(rocprofvis_data_point_t& point, ImVec2& cursor_position,
                 ImVec2& content_size, float scaleX, float scaleY)
{
    float x = (point.xValue - (m_min_x + m_movement)) * scaleX;
    float y = cursor_position.y + content_size.y - (point.yValue - m_min_y) * scaleY;

    return ImVec2(x, y);
}

}  // namespace View
}  // namespace RocProfVis

