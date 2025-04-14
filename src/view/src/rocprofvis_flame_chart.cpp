// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_flame_chart.h"
#include "imgui.h"
#include "rocprofvis_controller.h"
#include <algorithm>
#include <iostream>
#include <limits>
#include <map>
#include <string>

namespace RocProfVis
{
namespace View
{
FlameChart::FlameChart(int chart_id, std::string name, float zoom, float movement,
                       float min_x, float max_x, float scale_x)
: m_zoom(zoom)
, m_movement(movement)
, m_min_x(min_x)
, m_chart_id(chart_id)
, m_max_x(max_x)
, m_scale_x(scale_x)
, m_name(name)
, m_track_height(75)
, m_is_color_value_existant()
, m_is_chart_visible(true)  // has to be true or nothing will render.
{}

std::tuple<float, float>
FlameChart::FindMaxMinFlame()
{
    m_min_x = flames[0].m_start_ts;
    m_max_x = flames[0].m_start_ts + flames[0].m_duration;

    for(const auto& point : flames)
    {
        if(point.m_start_ts < m_min_x)
        {
            m_min_x = point.m_start_ts;
        }
        if(point.m_start_ts + point.m_duration > m_max_x)
        {
            m_max_x = point.m_start_ts + point.m_duration;
        }
    }
    return std::make_tuple(m_min_x, m_max_x);
}

void
FlameChart::UpdateMovement(float zoom, float movement, float& min_x, float& max_x,
                           float scale_x, float y_scroll_position)
{
    if(m_is_chart_visible)
    {
        // elements has gone off screen for the first time.
        m_movement_since_unload = y_scroll_position;
    }

    m_zoom       = zoom;
    m_movement   = movement;
    m_scale_x    = scale_x;
    m_min_x      = min_x;
    m_max_x      = max_x;
    m_y_movement = y_scroll_position;
}

void
FlameChart::SetColorByValue(rocprofvis_color_by_value_t color_by_value_digits)
{}

std::string&
FlameChart::GetName()
{
    return m_name;
}

float
FlameChart::GetMovement()
{
    return m_movement_since_unload - m_y_movement;
}

float
FlameChart::GetTrackHeight()
{
    return m_track_height;
}

void
FlameChart::SetID(int id)
{
    m_chart_id = id;
}

int
FlameChart::ReturnChartID()
{
    return m_chart_id;
}

bool
FlameChart::GetVisibility()
{
    return m_is_chart_visible;
}
void
FlameChart::ExtractFlamePoints(rocprofvis_controller_array_t* track_data)
{
    std::vector<rocprofvis_trace_event_t> entries;

    ImVec2 display_size = ImGui::GetIO().DisplaySize;
    int    screen_width = static_cast<int>(display_size.x);

    float effective_width = screen_width / m_zoom;
    float bin_size        = ((m_max_x - m_min_x) / effective_width);

    double bin_sum_x         = 0.0;
    int    bin_count         = 0;
    double current_bin_start = DBL_MAX;
    float  largest_duration  = 0;

    uint64_t            count  = 0;
    rocprofvis_result_t result = rocprofvis_controller_get_uint64(
        track_data, kRPVControllerArrayNumEntries, 0, &count);
    assert(result == kRocProfVisResultSuccess);

    rocprofvis_trace_event_t counter;

    for(uint64_t i = 0; i < count; i++)
    {
        rocprofvis_controller_event_t* event = nullptr;
        result                               = rocprofvis_controller_get_object(
            track_data, kRPVControllerArrayEntryIndexed, i, &event);
        assert(result == kRocProfVisResultSuccess && event);

        double start_ts = 0;
        result          = rocprofvis_controller_get_double(
            event, kRPVControllerEventStartTimestamp, 0, &start_ts);
        assert(result == kRocProfVisResultSuccess);

        double end_ts = 0;
        result = rocprofvis_controller_get_double(event, kRPVControllerEventEndTimestamp,
                                                  0, &end_ts);
        assert(result == kRocProfVisResultSuccess);

        uint32_t length = 0;
        result = rocprofvis_controller_get_string(event, kRPVControllerEventName, 0,
                                                  nullptr, &length);
        assert(result == kRocProfVisResultSuccess);

        length += 1;
        counter.m_name.resize(length);
        char* buffer = const_cast<char*>(counter.m_name.c_str());
        assert(buffer);
        result = rocprofvis_controller_get_string(event, kRPVControllerEventName, 0,
                                                  buffer, &length);
        assert(result == kRocProfVisResultSuccess);

        if(i == 0)
        {
            current_bin_start = start_ts;
        }

        counter.m_start_ts = start_ts;
        counter.m_duration = end_ts - start_ts;

        if(counter.m_start_ts < current_bin_start + bin_size)
        {
            if(counter.m_duration > largest_duration)
            {
                largest_duration =
                    counter.m_duration;  // Use the largest duration per bin.
            }
            bin_sum_x += counter.m_start_ts;
            bin_count++;
        }
        else
        {
            if(bin_count > 0)
            {
                rocprofvis_trace_event_t binned_point;
                binned_point.m_start_ts = bin_sum_x / bin_count;
                binned_point.m_duration = largest_duration;
                binned_point.m_name     = counter.m_name;
                entries.push_back(binned_point);
            }

            // Prepare next bin.
            current_bin_start =
                current_bin_start +
                bin_size *
                    static_cast<int>((counter.m_start_ts - current_bin_start) / bin_size);
            bin_sum_x        = counter.m_start_ts;
            largest_duration = counter.m_duration;
            bin_count        = 1;
        }
    }

    if(bin_count > 0)
    {
        rocprofvis_trace_event_t binned_point;
        binned_point.m_start_ts = bin_sum_x / bin_count;
        binned_point.m_duration = largest_duration;
        binned_point.m_name     = counter.m_name;

        entries.push_back(binned_point);
    }

    flames = entries;
}

void
FlameChart::DrawBox(ImVec2 start_position, int boxplot_box_id,
                    rocprofvis_trace_event_t flame, float duration, ImDrawList* draw_list)
{
    ImGui::PushID(static_cast<int>(boxplot_box_id));

    ImVec2 cursor_position = ImGui::GetCursorScreenPos();
    ImVec2 content_size    = ImGui::GetContentRegionAvail();

    // Define the start and end positions for the rectangle
    ImVec2 rectMin =
        ImVec2(start_position.x,
               start_position.y + cursor_position.y);  // Start position (top-left corner)
    ImVec2 rectMax = ImVec2(start_position.x + duration,
                            start_position.y + 40 +
                                cursor_position.y);  // End position (bottom-right corner)

    ImU32 rectColor = IM_COL32(128, 128, 128, 255);  // Black colored box.

    draw_list->AddRectFilled(rectMin, rectMax, rectColor);

    if(ImGui::IsMouseHoveringRect(rectMin, rectMax))
    {
        ImGui::SetTooltip("%s\nStart: %.2f\nDuration: %.2f ", flame.m_name.c_str(),
                          flame.m_start_ts - m_min_x, flame.m_duration);
    }

    ImGui::PopID();
}

void
FlameChart::Render()
{
    ImGuiWindowFlags window_flags =
        ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoMove;

    if(ImGui::BeginChild((std::to_string(m_chart_id)).c_str()), ImVec2(0, 50), true,
       window_flags)
    {
        int    boxplot_box_id = 0;
        ImVec2 parent_size    = ImGui::GetContentRegionAvail();
        float  metadata_size  = 400.0f;
        float  graph_size     = parent_size.x - metadata_size;

        ImGui::BeginChild("MetaData View", ImVec2(metadata_size, m_track_height), false);

        ImGui::BeginChild("MetaData Content",
                          ImVec2(metadata_size - 70.0f, m_track_height), false);
        ImGui::Text(m_name.c_str());
        if(ImGui::IsItemVisible())
        {
            m_is_chart_visible = true;
        }
        else
        {
            m_is_chart_visible = false;
        }

        ImGui::EndChild();

        ImGui::SameLine();

        ImGui::BeginChild("MetaData Scale", ImVec2(70.0f, m_track_height), false);

        ImGui::EndChild();

        ImGui::EndChild();

        ImGui::SameLine();

        ImGui::BeginChild("Graph View", ImVec2(graph_size, m_track_height), false);
        ImDrawList* draw_list = ImGui::GetWindowDrawList();

        for(const auto& flame : flames)
        {
            float normalized_start =
                (flame.m_start_ts - (m_min_x + m_movement)) * m_scale_x;

            // float duration = static_cast<float>(flame.m_duration * zoom) * scale_x;
            float normalized_end = flame.m_duration * m_scale_x;

            float fullBoxSize = normalized_start + normalized_end;

            ImVec2 start_position;
            ImVec2 end_position;

            start_position = ImVec2(normalized_start,
                                    0);  // Scale the start time for better visualization

            DrawBox(start_position, boxplot_box_id, flame, normalized_end, draw_list);

            boxplot_box_id = boxplot_box_id + 1;
        }
        ImGui::EndChild();
    }
    // Controls for graph resize.
    ImGuiIO& io              = ImGui::GetIO();
    bool     is_control_held = io.KeyCtrl;
    if(is_control_held)
    {
        ImGui::Selectable(("Move Position Line " + std::to_string(m_chart_id)).c_str(),
                          false, ImGuiSelectableFlags_AllowDoubleClick, ImVec2(0, 20.0f));

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

}  // namespace View
}  // namespace RocProfVis
