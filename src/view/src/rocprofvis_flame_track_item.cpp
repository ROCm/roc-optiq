// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_flame_track_item.h"
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

constexpr int MIN_LABEL_WIDTH = 40;

FlameTrackItem::FlameTrackItem(DataProvider& dp, int id, std::string name, double zoom,
                               double time_offset_ns, double min_x, double max_x,
                               double scale_x)
: TrackItem(dp, id, name, zoom, time_offset_ns, min_x, max_x, scale_x)
, m_request_random_color(true)
, m_text_padding(ImVec2(4.0f, 2.0f))
, m_flame_height(40.0f)
, m_selected_event_id(std::numeric_limits<uint64_t>::max())
, m_dp(dp)
{
    m_meta_area_scale_width = 0.0f;
}

void
FlameTrackItem::SetRandomColorFlag(bool set_color)
{
    m_request_random_color = set_color;
}

std::tuple<double, double>
FlameTrackItem::FindMaxMinFlame()
{
    return std::make_tuple(m_min_x, m_max_x);
}

void
FlameTrackItem::SetColorByValue(rocprofvis_color_by_value_t color_by_value_digits)
{
    (void) color_by_value_digits;
}

bool
FlameTrackItem::HasData()
{
    return m_data_provider.GetRawTrackData(m_id) != nullptr;
}

void
FlameTrackItem::ReleaseData()
{
    m_flames.clear();
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

    if(event_track->GetData().empty())
    {
        spdlog::debug("No data for track {}", m_id);
        return false;
    }

    m_flames = event_track->GetData();
    return true;
}

void
FlameTrackItem::DrawBox(ImVec2 start_position, int color_index,
                        rocprofvis_trace_event_t const& flame, double duration,
                        ImDrawList* draw_list, double raw_start_time)
{
    ImVec2 cursor_position = ImGui::GetCursorScreenPos();
    ImVec2 content_size    = ImGui::GetContentRegionAvail();

    ImVec2 rectMin = ImVec2(start_position.x, start_position.y + cursor_position.y);
    ImVec2 rectMax = ImVec2(start_position.x + duration,
                            start_position.y + m_flame_height + cursor_position.y);

    ImU32 rectColor;
    if(m_request_random_color)
    {
        rectColor = m_settings.GetColorWheel()[color_index];
    }
    else
    {
        rectColor = m_settings.GetColor(static_cast<int>(Colors::kFlameChartColor));
    }

    float rounding = 2.0f;
    draw_list->AddRectFilled(rectMin, rectMax, rectColor, rounding);

    if(flame.m_id == m_selected_event_id)
    {
        if(m_selected_event_id == m_dp.GetSelectedEventId())
        {
            float highlight_thickness = 3.0f;
            draw_list->AddRect(
                rectMin - ImVec2(2, 2), rectMax + ImVec2(2, 2),
                m_settings.GetColor(static_cast<int>(Colors::kEventHighlight)),
                rounding + 2, 0, highlight_thickness + 2);
        }
    }

    if(rectMax.x - rectMin.x > MIN_LABEL_WIDTH)
    {
        draw_list->PushClipRect(rectMin, rectMax, true);
        ImVec2 textPos =
            ImVec2(rectMin.x + m_text_padding.x, rectMin.y + m_text_padding.y);
        draw_list->AddText(textPos,
                           m_settings.GetColor(static_cast<int>(Colors::kTextMain)),
                           flame.m_name.c_str());
        draw_list->PopClipRect();
    }
    if(ImGui::IsMouseHoveringRect(rectMin, rectMax))
    {
        // Select on click
        if(ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            if(m_selected_event_id != flame.m_id ||
               m_dp.GetSelectedEventId() != flame.m_id)
            {
                selected_event_t selected_event = { flame.m_id, raw_start_time, m_id };
                m_dp.SetSelectedEvent(selected_event);
                m_selected_event_id = flame.m_id;
            }
            else
            {
                m_dp.SetSelectedEvent({});
                m_selected_event_id = std::numeric_limits<uint64_t>::max();
            }
        }

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, m_text_padding);
        ImGui::BeginTooltip();
        ImGui::Text("%s", flame.m_name.c_str());
        ImGui::Text("Start: %.2f", flame.m_start_ts - m_min_x);
        ImGui::Text("Duration: %.2f", flame.m_duration);
        ImGui::Text("Id: %llu", flame.m_id);
        ImGui::EndTooltip();
        ImGui::PopStyleVar();
    }
}

void
FlameTrackItem::RenderChart(float graph_width)
{
    ImGui::BeginChild("FV", ImVec2(graph_width, m_track_content_height), false);
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    auto colorCount = m_settings.GetColorWheel().size();
    ROCPROFVIS_ASSERT(colorCount > 0);

    int color_index = 0;
    for(const auto& flame : m_flames)
    {
        ImVec2 container_pos = ImGui::GetWindowPos();

        double normalized_start =
            container_pos.x +
            (flame.m_start_ts - (m_min_x + m_time_offset_ns)) * m_scale_x;

        double normalized_duration = std::max(flame.m_duration * m_scale_x, 1.0);
        double normalized_end      = normalized_start + normalized_duration;

        ImVec2 start_position;

        // Calculate the start position based on the normalized start time and level
        start_position = ImVec2(normalized_start, flame.m_level * m_flame_height);

        if(normalized_end < container_pos.x ||
           normalized_start > container_pos.x + graph_width)
        {
            continue;  // Skip if the flame is not visible in the current view
        }

        color_index = static_cast<long long>(flame.m_start_ts) % colorCount;
        DrawBox(start_position, color_index, flame, normalized_duration, draw_list,
                flame.m_start_ts + flame.m_duration);
    }

    ImGui::EndChild();
}

void
FlameTrackItem::RenderMetaAreaScale()
{}

void
FlameTrackItem::RenderMetaAreaOptions()
{
    ImGui::Checkbox("Color Events", &m_request_random_color);
}

}  // namespace View
}  // namespace RocProfVis