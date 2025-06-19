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
