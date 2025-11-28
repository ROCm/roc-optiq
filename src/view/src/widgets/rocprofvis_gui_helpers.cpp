// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_gui_helpers.h"
#include "icons/rocprovfis_icon_defines.h"
#include "rocprofvis_utils.h"
#include <cmath>
#include <algorithm>

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
        alpha_multiplier = std::clamp(alpha_multiplier * 1.5f - 0.25f, 0.0f, 1.0f);

        ImU32        current_color = color;
        unsigned int alpha         = (current_color >> IM_COL32_A_SHIFT) & 0xFF;
        alpha                      = static_cast<unsigned int>(alpha * alpha_multiplier);
        current_color = (current_color & ~IM_COL32_A_MASK) | (alpha << IM_COL32_A_SHIFT);

        draw_list->AddCircleFilled(ImVec2(current_dot_x, current_dot_y), dot_radius,
                                   current_color, 12);
    }
}

bool
RocProfVis::View::IconButton(const char* icon, ImFont* icon_font, ImVec2 size,
                             const char* tooltip, ImVec2 tooltip_padding, bool frameless,
                             ImVec2 frame_padding, ImU32 bg_color, ImU32 bg_color_hover,
                             ImU32 bg_color_active, const char* id)
{
    if(id && strlen(id) > 0)
    {
        ImGui::PushID(id);
    }
    else
    {
        ImGui::PushID(icon);
    }
    if(frameless)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
        ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(0, 0, 0, 0));
    }
    else
    {
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, frame_padding);
        ImGui::PushStyleColor(ImGuiCol_Button, bg_color);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, bg_color_hover);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, bg_color_active);
    }
    ImGui::PushFont(icon_font);
    bool clicked = ImGui::Button(icon, size);
    ImGui::PopFont();
    if(tooltip && strlen(tooltip) > 0)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, tooltip_padding);
        if(ImGui::BeginItemTooltip())
        {
            ImGui::TextUnformatted(tooltip);
            ImGui::EndTooltip();
        }
        ImGui::PopStyleVar();
    }
    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar();
    ImGui::PopID();
    return clicked;
}

std::pair<bool, bool>
RocProfVis::View::InputTextWithClear(const char* id, const char* hint, char* buf,
                                     size_t buf_size, ImFont* icon_font, ImU32 bg_color,
                                     const ImGuiStyle& style, float width)
{
    bool input_cleared = false;
    ImGui::BeginGroup();
    ImGui::SetNextItemAllowOverlap();
    ImGui::PushID(id);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, bg_color);
    ImGui::SetNextItemWidth(width);
    bool input_changed =
        ImGui::InputTextWithHint("##input_text_with_clear", hint, buf, buf_size);
    ImGui::PopStyleColor();
    if(strlen(buf) > 0)
    {
        ImGui::PushFont(icon_font);
        if(width >= ImGui::CalcTextSize(ICON_X_CIRCLED).x + 2 * style.FramePadding.x)
        {
            ImGui::SameLine();
            ImGui::SetCursorPosX(ImGui::GetItemRectMax().x - 2 * style.FramePadding.x -
                                 ImGui::CalcTextSize(ICON_X_CIRCLED).x);
            ImGui::PopFont();
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, style.FramePadding);
            input_cleared = IconButton(ICON_X_CIRCLED, icon_font, ImVec2(0, 0), "Clear",
                                       style.WindowPadding, false, style.FramePadding,
                                       bg_color, bg_color, bg_color);
            ImGui::PopStyleVar();
        }
        else
        {
            ImGui::PopFont();
        }
    }
    ImGui::PopID();
    ImGui::EndGroup();
    return std::make_pair(input_changed, input_cleared);
}

#ifdef ROCPROFVIS_ENABLE_INTERNAL_BANNER

void
RocProfVis::View::DrawInternalBuildBanner(const char* text /*= "Internal Build"*/)
{
    if(!text || !*text) return;

    ImDrawList*   dl   = ImGui::GetForegroundDrawList();
    const ImVec2& disp = ImGui::GetIO().DisplaySize;

    // Parameters
    static constexpr float ribbon_thickness = 20.0f;
    static constexpr float min_base_length  = 150.0f;
    static constexpr ImU32 col_fill         = IM_COL32(200, 16, 32, 150);
    static constexpr ImU32 col_border       = IM_COL32(255, 255, 255, 40);
    static constexpr ImU32 col_text         = IM_COL32(255, 255, 255, 255);

    // use precomputed cos/sin for 45 degrees to avoid trig calls
    static constexpr float c_45 = 0.70710678118f;
    static constexpr float s_45 = 0.70710678118f;

    // Measure text first
    ImVec2 ts = ImGui::CalcTextSize(text);

    // Required ribbon length so text fits
    const float desired_length = ts.x * 2.0f;
    const float ribbon_length =
        (desired_length > min_base_length) ? desired_length : min_base_length;

    const float half_len   = ribbon_length * 0.5f;
    const float half_thick = ribbon_thickness * 0.5f;

    // Center a rotated rectangle so it visually emerges from the top-right corner
    ImVec2 center = ImVec2(disp.x - half_len * 0.5f, half_len * 0.5f);

    // Axis‑aligned rect (local space before rotation)
    ImVec2 local[4] = { ImVec2(-half_len, -half_thick), ImVec2(half_len, -half_thick),
                        ImVec2(half_len, half_thick), ImVec2(-half_len, half_thick) };

    ImVec2 quad[4];
    for(int i = 0; i < 4; ++i)
    {
        const ImVec2& p = local[i];
        quad[i].x       = center.x + p.x * c_45 - p.y * s_45;
        quad[i].y       = center.y + p.x * s_45 + p.y * c_45;
    }

    dl->AddConvexPolyFilled(quad, 4, col_fill);
    dl->AddPolyline(quad, 4, col_border, true, 1.0f);

    // Add text at unrotated local position (centered), then rotate vertices
    ImVec2 text_local_pos(-ts.x * 0.5f, -ts.y * 0.5f);
    int    v_start = dl->VtxBuffer.Size;
    dl->AddText(nullptr, ImGui::GetFontSize(),
                ImVec2(center.x + text_local_pos.x, center.y + text_local_pos.y),
                col_text, text);
    int v_end = dl->VtxBuffer.Size;

    for(int i = v_start; i < v_end; ++i)
    {
        ImDrawVert&   v = dl->VtxBuffer[i];
        const ImVec2  p = v.pos - center;
        v.pos.x         = center.x + p.x * c_45 - p.y * s_45;
        v.pos.y         = center.y + p.x * s_45 + p.y * c_45;
    }
}
#endif // ROCPROFVIS_ENABLE_INTERNAL_BANNER
