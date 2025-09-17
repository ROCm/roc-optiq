// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_gui_helpers.h"
#include "rocprofvis_utils.h"
#include <cmath>

constexpr float PI = 3.14159265358979323846f;  // Define PI constant

ImVec2
RocProfVis::View::MeasureLoadingIndicatorDots(float dot_radius, int num_dots,
                                              float spacing)
{
    // Calculate total width needed
    float total_width = (num_dots * (dot_radius * 2.0f)) + ((num_dots - 1) * spacing);
    return ImVec2(total_width, dot_radius * 2.0f);
}

void
RocProfVis::View::RenderLoadingIndicatorDots(float dot_radius, int num_dots,
                                             float spacing, ImU32 color, float speed)
{
    // Calculate total width needed
    float  total_width = MeasureLoadingIndicatorDots(dot_radius, num_dots, spacing).x;
    ImVec2 pos         = ImGui::GetCursorScreenPos();
    ImVec2 size(total_width, dot_radius * 2.0f);

    ImGui::Dummy(size);  // Reserve space

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    const float t         = (float) ImGui::GetTime();

    for(int i = 0; i < num_dots; ++i)
    {
        float current_dot_x = pos.x + dot_radius + i * (dot_radius * 2.0f + spacing);
        float current_dot_y = pos.y + dot_radius;

        // Offset each dot's animation phase
        float phase = (t * speed) - (i * (2.0f * PI / (float) num_dots) *
                                     0.5f);  // Adjust 0.5f for phase spread

        float alpha_multiplier = (sinf(phase) + 1.0f) * 0.5f;

        // Sharpen the pulse a bit
        alpha_multiplier = clamp(alpha_multiplier * 1.5f - 0.25f, 0.0f, 1.0f);

        ImU32        current_color = color;
        unsigned int alpha         = (current_color >> IM_COL32_A_SHIFT) & 0xFF;
        alpha                      = static_cast<unsigned int>(alpha * alpha_multiplier);
        current_color = (current_color & ~IM_COL32_A_MASK) | (alpha << IM_COL32_A_SHIFT);

        draw_list->AddCircleFilled(ImVec2(current_dot_x, current_dot_y), dot_radius,
                                   current_color, 12);
    }
}

#ifdef ROCPROFVIS_ENABLE_INTERNAL_BANNER

#ifndef IM_PI
    #define IM_PI 3.14159265358979323846f
#endif

void
RocProfVis::View::DrawInternalBuildBanner(const char* text /*= "Internal Build"*/)
{
    if(!text || !*text) return;

    ImDrawList* dl   = ImGui::GetForegroundDrawList();
    ImVec2      disp = ImGui::GetIO().DisplaySize;

    // Parameters
    const float angle_rad        = IM_PI * 0.25f;  // 45 degrees (down-left)
    const float ribbon_thickness = 20.0f;              
    const float min_base_length  = 300.0f;         
    const float side_padding     = 64.0f;          
    const ImU32 col_fill         = IM_COL32(200, 16, 32, 150);
    const ImU32 col_border       = IM_COL32(255, 255, 255, 40);
    const ImU32 col_text         = IM_COL32(255, 255, 255, 255);

    // Measure text first
    ImVec2 ts = ImGui::CalcTextSize(text);

    // Required ribbon length so text fits (no scaling unless absolutely necessary)
    float desired_length = ts.x + side_padding;
    float ribbon_length =
        (desired_length > min_base_length) ? desired_length : min_base_length;

    const float half_len   = ribbon_length * 0.5f;
    const float half_thick = ribbon_thickness * 0.5f;

    // Center a rotated rectangle so it visually emerges from the top-right corner
    ImVec2 center = ImVec2(disp.x - half_len * 0.5f, half_thick);

    // Axis‑aligned rect (local space before rotation)
    ImVec2 local[4] = { ImVec2(-half_len, -half_thick), ImVec2(half_len, -half_thick),
                        ImVec2(half_len, half_thick), ImVec2(-half_len, half_thick) };

    // Rotation (same for quad + text)
    const float c = cosf(angle_rad);
    const float s = sinf(angle_rad);

    ImVec2 quad[4];
    for(int i = 0; i < 4; ++i)
    {
        ImVec2 p  = local[i];
        quad[i].x = center.x + p.x * c - p.y * s;
        quad[i].y = center.y + p.x * s + p.y * c;
    }

    dl->AddConvexPolyFilled(quad, 4, col_fill);
    dl->AddPolyline(quad, 4, col_border, true, 1.0f);

    // Text sizing
    float scale = 1.0f;
    if(ts.x > ribbon_length - side_padding) scale = (ribbon_length - side_padding) / ts.x;

    ts.x = 0.0f;  // recalc text size after scaling
    // Add text at unrotated local position (centered), then rotate vertices
    ImVec2 text_local_pos(-ts.x * scale * 0.5f, -ts.y * scale * 0.5f);
    int    v_start = dl->VtxBuffer.Size;
    dl->AddText(nullptr, ImGui::GetFontSize() * scale,
                ImVec2(center.x + text_local_pos.x, center.y + text_local_pos.y),
                col_text, text);
    int v_end = dl->VtxBuffer.Size;

    for(int i = v_start; i < v_end; ++i)
    {
        ImDrawVert& v = dl->VtxBuffer[i];
        ImVec2      p = v.pos - center;
        v.pos.x       = center.x + p.x * c - p.y * s;
        v.pos.y       = center.y + p.x * s + p.y * c;
    }
}
#endif // ROCPROFVIS_ENABLE_INTERNAL_BANNER
