// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.
#pragma once

#include "rocprofvis_timeline_arrow.h"
#include "rocprofvis_data_provider.h"
#include "rocprofvis_settings_manager.h"
#include "rocprofvis_timeline_selection.h"
#include "spdlog/spdlog.h"

namespace RocProfVis
{
namespace View
{

void
TimelineArrow::SetFlowDisplayMode(FlowDisplayMode mode)
{
    m_flow_display_mode = mode;
}

FlowDisplayMode
TimelineArrow::GetFlowDisplayMode() const
{
    return m_flow_display_mode;
}

void
TimelineArrow::Render(ImDrawList* draw_list, double v_min_x, double pixels_per_ns,
                      ImVec2 window, std::map<uint64_t, float>& track_height_total)
{
    ImU32 color     = SettingsManager::GetInstance().GetColor(Colors::kArrowColor);
    float thickness = 2.0f;
    float head_size = 8.0f;
    float scroll_y  = ImGui::GetScrollY();
    for(const event_info_t* event : m_selected_event_data)
    {
        if(event)
        {
            int stride  = 1;
            int starter = 0;

            if(m_flow_display_mode == FlowDisplayMode::kHide)
            {
                starter = event->flow_info.size();
            }

            if(m_flow_display_mode == FlowDisplayMode::kShowAll)
            {
                stride  = 1;  // Show all flows
                starter = 0;  // Start from the first flow
            }
            else if(m_flow_display_mode == FlowDisplayMode::kShowFirstAndLast)
            {
                if(event->flow_info.size() > 1)
                {
                    stride  = event->flow_info.size() - 1;  // Jump from first to last
                    starter = 0;
                }
                else
                {
                    stride  = 1;  // Only one element, just draw it once
                    starter = 0;
                }
            }

            for(int i = starter; i < event->flow_info.size(); i += stride)
            {
                const event_flow_data_t& flow      = event->flow_info[i];
                const uint64_t&          direction = flow.direction;

                double start_time_ns;
                if(direction == 1)
                    start_time_ns =
                        event->basic_info.m_start_ts;  // Use start of event for outflow
                else
                    start_time_ns =
                        event->basic_info.m_start_ts +
                        event->basic_info.m_duration;  // Use end of event for inflow

                const uint64_t& end_time_ns    = flow.timestamp;
                const uint64_t& start_track_id = event->track_id;
                const uint64_t& end_track_id   = flow.track_id;

                float start_x_ns = (start_time_ns - v_min_x) * pixels_per_ns;
                float end_x_ns   = (end_time_ns - v_min_x) * pixels_per_ns;

                float start_y_px = track_height_total[start_track_id];
                float end_y_px   = track_height_total[end_track_id];

                ImVec2 p_start =
                    ImVec2(window.x + start_x_ns, window.y + start_y_px - scroll_y);
                ImVec2 p_end =
                    ImVec2(window.x + end_x_ns, window.y + end_y_px - scroll_y);

                if(p_start.x == p_end.x && p_start.y == p_end.y) continue;

                // Calculate control points for a smooth cubic Bezier curve
                float  curve_offset = 0.25f * (p_end.x - p_start.x);
                ImVec2 p_ctrl1      = ImVec2(p_start.x + curve_offset, p_start.y);
                ImVec2 p_ctrl2      = ImVec2(p_end.x - curve_offset, p_end.y);

                draw_list->AddBezierCubic(p_start, p_ctrl1, p_ctrl2, p_end, color,
                                          thickness, 32);

                // Compute direction at the end of the curve (tangent)
                ImVec2 dir = ImVec2(p_end.x - p_ctrl2.x, p_end.y - p_ctrl2.y);
                float  len = sqrtf(dir.x * dir.x + dir.y * dir.y);
                if(len > 0.0f)
                {
                    dir.x /= len;
                    dir.y /= len;
                }
                ImVec2 ortho(-dir.y, dir.x);

                // Arrowhead points

                // Arrowhead points
                if(direction == 0)
                {
                    ImVec2 p1 = p_end;
                    ImVec2 p2 =
                        ImVec2(p_end.x - dir.x * head_size - ortho.x * head_size * 0.5f,
                               p_end.y - dir.y * head_size - ortho.y * head_size * 0.5f);
                    ImVec2 p3 =
                        ImVec2(p_end.x - dir.x * head_size + ortho.x * head_size * 0.5f,
                               p_end.y - dir.y * head_size + ortho.y * head_size * 0.5f);
                    draw_list->AddTriangleFilled(p1, p2, p3, color);
                }
                else
                {
                    ImVec2 p1 = p_start;
                    ImVec2 p2 = ImVec2(
                        p_start.x - dir.x * head_size - ortho.x * head_size * 0.5f,
                        p_start.y - dir.y * head_size - ortho.y * head_size * 0.5f);
                    ImVec2 p3 = ImVec2(
                        p_start.x - dir.x * head_size + ortho.x * head_size * 0.5f,
                        p_start.y - dir.y * head_size + ortho.y * head_size * 0.5f);
                    draw_list->AddTriangleFilled(p1, p2, p3, color);
                }
            }
        }
    }
}

TimelineArrow::TimelineArrow(DataProvider&                      data_provider,
                             std::shared_ptr<TimelineSelection> selection)
: m_data_provider(data_provider)
, m_timeline_selection(selection)
, m_selection_changed_token(-1)
{
    auto scroll_to_arrow_handler = [this](std::shared_ptr<RocEvent> e) {
        this->HandleEventSelectionChanged(e);
    };

    m_selection_changed_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kTimelineEventSelectionChanged),
        scroll_to_arrow_handler);
}

TimelineArrow::~TimelineArrow()
{
    EventManager::GetInstance()->Unsubscribe(
        static_cast<int>(RocEvents::kTimelineEventSelectionChanged),
        m_selection_changed_token);
}

void
TimelineArrow::HandleEventSelectionChanged(std::shared_ptr<RocEvent> e)
{
    std::shared_ptr<EventSelectionChangedEvent> selection_changed_event =
        std::static_pointer_cast<EventSelectionChangedEvent>(e);
    if(selection_changed_event &&
       selection_changed_event->GetSourceId() == m_data_provider.GetTraceFilePath())
    {
        m_selected_event_data.clear();
        std::vector<uint64_t> selected_event_ids;
        m_timeline_selection->GetSelectedEvents(selected_event_ids);
        m_selected_event_data.resize(selected_event_ids.size());
        for(int i = 0; i < selected_event_ids.size(); i++)
        {
            m_selected_event_data[i] =
                m_data_provider.GetEventInfo(selected_event_ids[i]);
        }
    }
}

}  // namespace View
}  // namespace RocProfVis