// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.
#pragma once

#include "rocprofvis_timeline_arrow.h"
#include "spdlog/spdlog.h"
#include <iostream>

namespace RocProfVis
{
namespace View
{

void
TimelineArrow::Render(ImDrawList* draw_list, double v_min_x, double pixels_per_ns,
                      ImVec2 window, std::map<uint64_t, float> track_height_total)
{
    ImU32 color     = IM_COL32(0, 0, 210, 80);
    float thickness = 2.0f;
    float head_size = 12.0f;

    float scroll_y = ImGui::GetScrollY();
    for(const auto& arrow : m_arrows_to_render)
    {
        float start_x = (arrow.start_time - v_min_x) * pixels_per_ns;
        float end_x   = (arrow.end_time - v_min_x) * pixels_per_ns;

        float start_y = track_height_total[arrow.start_track];
        float end_y   = track_height_total[arrow.end_track];

        ImVec2 p_start = ImVec2(window.x + start_x, window.y + start_y - scroll_y);
        ImVec2 p_end   = ImVec2(window.x + end_x, window.y + end_y - scroll_y);

        if(p_start.x == p_end.x && p_start.y == p_end.y) continue;

        draw_list->AddLine(p_start, p_end, color, thickness);

        ImVec2 dir = ImVec2(p_end.x - p_start.x, p_end.y - p_start.y);
        float  len = sqrtf(dir.x * dir.x + dir.y * dir.y);
        if(len > 0.0f)
        {
            dir.x /= len;
            dir.y /= len;
        }
        ImVec2 ortho(-dir.y, dir.x);
        ImVec2 p1 = p_end;
        ImVec2 p2 = ImVec2(p_end.x - dir.x * head_size - ortho.x * head_size * 0.5f,
                           p_end.y - dir.y * head_size - ortho.y * head_size * 0.5f);
        ImVec2 p3 = ImVec2(p_end.x - dir.x * head_size + ortho.x * head_size * 0.5f,
                           p_end.y - dir.y * head_size + ortho.y * head_size * 0.5f);
        draw_list->AddTriangleFilled(p1, p2, p3, color);
    }
}

TimelineArrow::TimelineArrow(DataProvider& data_provider)
: m_data_provider(data_provider)
, m_arrows_to_render({})
, m_add_arrow_token(-1)
{
    auto scroll_to_arrow_handler = [this](std::shared_ptr<RocEvent> e) {
        auto evt = std::dynamic_pointer_cast<CreateArrowsView>(e);
        if(evt)
        {
            this->AddArrows();
        }
    };
    m_add_arrow_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kHandleUserArrowCreationEvent),
        scroll_to_arrow_handler);
}

void
TimelineArrow::SetArrows(const std::vector<TimelineArrowData>& arrows)
{
    m_arrows_to_render = arrows;
}

void
TimelineArrow::AddArrow(const TimelineArrowData& arrow)
{
    m_arrows_to_render.push_back(arrow);
}

void
TimelineArrow::AddArrows()
{
    const flow_info_t& flowInfo = m_data_provider.GetFlowInfo();
    if(!flowInfo.flow_data.empty())
    {
        m_arrows_to_render = {};

        double source_time  = m_data_provider.GetEventPosition();
        int    source_track = m_data_provider.GetEventTrackPosition();

        for(const auto& item : flowInfo.flow_data)
        {
            m_arrows_to_render.push_back({ source_time, source_track,
                                           static_cast<double>(item.timestamp),
                                           static_cast<int>(item.track_id) });
        }
    }
}

TimelineArrow::~TimelineArrow()
{
    EventManager::GetInstance()->Unsubscribe(
        static_cast<int>(RocEvents::kHandleUserArrowCreationEvent), m_add_arrow_token);
}

}  // namespace View
}  // namespace RocProfVis