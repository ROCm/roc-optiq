// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_flame_track_item.h"
#include "../src/view/src/rocprofvis_settings.h"
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

FlameTrackItem::FlameTrackItem(DataProvider& dp, int id, std::string name, float zoom,
                               float movement, double min_x, double max_x, float scale_x)
: TrackItem(dp, id, name, zoom, movement, min_x, max_x, scale_x)
, m_is_color_value_existant()
, m_request_random_color(true)
, m_settings(Settings::GetInstance())

{}

void
FlameTrackItem::SetRandomColorFlag(bool set_color)
{
    m_request_random_color = set_color;
}

std::tuple<double, double>
FlameTrackItem::FindMaxMinFlame()
{
    m_min_x = m_flames[0].m_start_ts;
    m_max_x = m_flames[0].m_start_ts + m_flames[0].m_duration;

    for(const auto& point : m_flames)
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
FlameTrackItem::SetColorByValue(rocprofvis_color_by_value_t color_by_value_digits)
{}

bool
FlameTrackItem::HasData()
{
    return !m_flames.empty();
}

void
FlameTrackItem::ReleaseData()
{
    m_flames.clear();
    m_flames = {};
}

bool
FlameTrackItem::HandleTrackDataChanged()
{
    m_request_state = TrackDataRequestState::kIdle;
    bool result     = false;
    result          = ExtractPointsFromData();
    if(result)
    {
        FindMaxMinFlame();
    }
    return result;
}

bool
FlameTrackItem::ExtractPointsFromData()
{
    const RawTrackData*      rtd         = m_data_provider.GetRawTrackData(m_id);
    const RawTrackEventData* event_track = dynamic_cast<const RawTrackEventData*>(rtd);
    if(!event_track)
    {
        spdlog::debug("Invalid track data type for track {}", m_id);
        return false;
    }

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

    m_flames = entries;
    return true;
}

void
FlameTrackItem::DrawBox(ImVec2 start_position, int boxplot_box_id,
                        rocprofvis_trace_event_t flame, float duration,
                        ImDrawList* draw_list)
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

    ImU32 rectColor;
    if(m_request_random_color)
    {
        rectColor = m_settings.GetColorWheel()[boxplot_box_id % 10];
    }
    else
    {
        rectColor = m_settings.GetColor(
            static_cast<int>(Colors::kFlameChartColor));  // Black colored box.
    }

    draw_list->AddRectFilled(rectMin, rectMax, rectColor);

    if(ImGui::IsMouseHoveringRect(rectMin, rectMax))
    {
        ImGui::SetTooltip("%s\nStart: %.2f\nDuration: %.2f ", flame.m_name.c_str(),
                          flame.m_start_ts - m_min_x, flame.m_duration);
    }

    ImGui::PopID();
}

void
FlameTrackItem::RenderMetaArea()
{
    ImGui::PushStyleColor(ImGuiCol_ChildBg, m_metadata_bg_color);
    ImGui::BeginChild("MetaData View", ImVec2(s_metadata_width, m_track_height),
                      ImGuiChildFlags_None);
    ImVec2 content_size = ImGui::GetContentRegionAvail();

    // Set padding for the child window (Note this done using SetCursorPos
    // because ImGuiStyleVar_WindowPadding has no effect on child windows without borders)
    ImGui::SetCursorPos(m_metadata_padding);
    // Adjust content size to account for padding
    content_size.x -= m_metadata_padding.x * 2;
    content_size.y -= m_metadata_padding.x * 2;
    ImGui::BeginChild("MetaData Content", ImVec2(content_size.x - 70.0f, content_size.y),
                      ImGuiChildFlags_None);
    ImGui::Text(m_name.c_str());
    if(ImGui::IsItemVisible())
    {
        m_is_in_view_vertical = true;
    }
    else
    {
        m_is_in_view_vertical = false;
    }

    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("MetaData Scale", ImVec2(70.0f, content_size.y),
                      ImGuiChildFlags_None);

    ImGui::EndChild();

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void
FlameTrackItem::RenderChart(float graph_width)
{
    ImGui::BeginChild("Graph View", ImVec2(graph_width, m_track_height), false);
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    int boxplot_box_id = 0;

    for(const auto& flame : m_flames)
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
FlameTrackItem::Render()
{
    TrackItem::Render();
}

}  // namespace View
}  // namespace RocProfVis
