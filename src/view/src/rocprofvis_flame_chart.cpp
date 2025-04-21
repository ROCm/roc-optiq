// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_flame_chart.h"
#include "imgui.h"
#include "rocprofvis_controller.h"
#include "rocprofvis_core_assert.h"
#include <algorithm>
#include <iostream>
#include <limits>
#include <map>
#include <string>

namespace RocProfVis
{
namespace View
{
FlameChart::FlameChart(int id, std::string name, float zoom, float movement, double min_x,
                       double max_x, float scale_x)
: Charts(id, name, zoom, movement, min_x, max_x, scale_x)
, m_is_color_value_existant()
{}

std::tuple<double, double>
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
FlameChart::UpdateMovement(float zoom, float movement, double& min_x, double& max_x,
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

float
FlameChart::GetMovement()
{
    return m_movement_since_unload - m_y_movement;
}

bool
FlameChart::SetRawData(const RawTrackData* raw_data)
{
    if(raw_data == m_raw_data)
    {
        return false;
    }
    else
    {
        m_raw_data = raw_data;
        const RawTrackEventData* event_track =
            dynamic_cast<const RawTrackEventData*>(raw_data);
        if(event_track)
        {
            ExtractPointsFromData(event_track);
            FindMaxMinFlame();
            return true;
        }
    }
    return false;
}

void
FlameChart::ExtractPointsFromData(const RawTrackEventData* event_track)
{
    std::vector<rocprofvis_trace_event_t> entries;

    ImVec2 display_size = ImGui::GetIO().DisplaySize;
    int    screen_width = static_cast<int>(display_size.x);

    double effective_width = screen_width / m_zoom;
    double bin_size        = ((m_max_x - m_min_x) / effective_width);

    double bin_sum_x         = 0.0;
    int    bin_count         = 0;
    double current_bin_start = DBL_MAX;
    float  largest_duration  = 0;

    const std::vector<rocprofvis_trace_event_t> track_data = event_track->GetData();
    uint64_t                                    count      = track_data.size();
    rocprofvis_trace_event_t                    counter;

    for(uint64_t i = 0; i < count; i++)
    {
        if(i == 0)
        {
            current_bin_start = track_data[i].m_start_ts;
        }

        counter.m_name     = track_data[i].m_name;
        counter.m_start_ts = track_data[i].m_start_ts;
        counter.m_duration = track_data[i].m_duration;

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
FlameChart::RenderMetaArea()
{
    ImGui::BeginChild("MetaData View", ImVec2(m_metadata_width, m_track_height), false);

    ImGui::BeginChild("MetaData Content",
                      ImVec2(m_metadata_width - 70.0f, m_track_height), false);
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
}

void
FlameChart::RenderChart(float graph_width)
{
    ImGui::BeginChild("Graph View", ImVec2(graph_width, m_track_height), false);
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    int boxplot_box_id = 0;

    for(const auto& flame : flames)
    {
        double normalized_start = (flame.m_start_ts - (m_min_x + m_movement)) * m_scale_x;

        // float duration = static_cast<float>(flame.m_duration * zoom) * scale_x;
        double normalized_end = flame.m_duration * m_scale_x;

        double fullBoxSize = normalized_start + normalized_end;

        ImVec2 start_position;
        ImVec2 end_position;

        // Scale the start time for better visualization
        start_position = ImVec2(normalized_start, 0);
        DrawBox(start_position, boxplot_box_id, flame, normalized_end, draw_list);

        boxplot_box_id = boxplot_box_id + 1;
    }
    ImGui::EndChild();
}

void
FlameChart::Render()
{
    Charts::Render();
}

}  // namespace View
}  // namespace RocProfVis
