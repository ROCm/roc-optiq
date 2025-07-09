// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_timeline_arrow.h"
#include "spdlog/spdlog.h"

namespace RocProfVis
{
namespace View
{

void
TimelineArrow::Draw(ImDrawList* draw_list, double v_min_x, double pixels_per_ns,
                    ImVec2 window, ImU32 color, float thickness, float head_size)
{
   
    float scroll_y = ImGui::GetScrollY();
    for(const auto& arrow : m_arrows_to_render)
    {
        const ImVec2 p_start =
            ImVec2(window.x + (arrow.start.x - v_min_x) * pixels_per_ns,
                   window.y + arrow.start.y - scroll_y);
        const ImVec2 p_end = ImVec2(window.x + (arrow.end.x - v_min_x) * pixels_per_ns,
                                    window.y + arrow.end.y - scroll_y);

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
{
    // Constructor implementation (if needed)
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

TimelineArrow::~TimelineArrow()
{
    // Destructor implementation (if needed)
}

void
TimelineArrow::Update()
{
    // Update logic for TimelineArrow
}

void
TimelineArrow::Render()
{}

}  // namespace View
}  // namespace RocProfVis