// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_loading_slots.h"

#include <algorithm>
#include <cmath>

#include "imgui.h"

#include "rocprofvis_render_scheduler.h"
#include "rocprofvis_settings_manager.h"
#include "widgets/rocprofvis_gui_helpers.h"

namespace RocProfVis
{
namespace View
{

// One sweep end to end: the playhead crosses, the blocks it laid down hold, and
// they fade out so the next sweep starts on an empty track.
constexpr float LOADING_SLOT_CYCLE_SECONDS = 1.8f;
// Shares of that cycle spent crossing, and spent fading at the end of it. The
// hold is whatever is left between the two.
constexpr float LOADING_SLOT_SWEEP_SHARE = 0.6f;
constexpr float LOADING_SLOT_FADE_SHARE  = 0.2f;
static_assert(LOADING_SLOT_SWEEP_SHARE + LOADING_SLOT_FADE_SHARE <= 1.0f,
              "The sweep and the fade have to fit inside one cycle.");
// How much later each slot starts than the one above it.
constexpr float LOADING_SLOT_STAGGER_SECONDS = 0.18f;

// Blocks sit across the middle of the slot, the way events sit on a track.
constexpr float LOADING_SLOT_BLOCK_HEIGHT_SHARE = 0.34f;
constexpr float LOADING_SLOT_BLOCK_ROUNDING     = 2.0f;
// Neighbouring blocks alternate between two strengths of the accent, so a lane
// reads as different kinds of event rather than one bar with gaps cut in it.
constexpr float LOADING_SLOT_BLOCK_ALPHA     = 0.75f;
constexpr float LOADING_SLOT_BLOCK_ALT_ALPHA = 0.45f;

// The playhead: a thin accent line in a faint band, kept clear of the slot's
// top and bottom edges.
constexpr float LOADING_SLOT_PLAYHEAD_WIDTH      = 2.0f;
constexpr float LOADING_SLOT_PLAYHEAD_BAND_WIDTH = 8.0f;
constexpr float LOADING_SLOT_PLAYHEAD_BAND_ALPHA = 0.2f;
constexpr float LOADING_SLOT_PLAYHEAD_INSET      = 4.0f;

// Block and gap widths, as shares of the track. Each slot and each sweep starts
// reading them at a different point, and the two lengths share no factor, so
// the lanes a column lays down keep changing.
constexpr float  LOADING_SLOT_BLOCK_WIDTHS[] = { 0.14f, 0.06f, 0.21f, 0.09f, 0.05f, 0.17f,
                                                 0.11f, 0.07f, 0.19f, 0.08f, 0.12f };
constexpr float  LOADING_SLOT_GAP_WIDTHS[]   = { 0.015f, 0.03f,  0.012f, 0.045f,
                                                 0.02f,  0.025f, 0.01f };
constexpr size_t LOADING_SLOT_BLOCK_WIDTH_COUNT =
    sizeof(LOADING_SLOT_BLOCK_WIDTHS) / sizeof(LOADING_SLOT_BLOCK_WIDTHS[0]);
constexpr size_t LOADING_SLOT_GAP_WIDTH_COUNT =
    sizeof(LOADING_SLOT_GAP_WIDTHS) / sizeof(LOADING_SLOT_GAP_WIDTHS[0]);
// Where in those lists the next slot down, and the next sweep, start reading.
constexpr size_t LOADING_SLOT_PATTERN_SLOT_STEP  = 4;
constexpr size_t LOADING_SLOT_PATTERN_SWEEP_STEP = 3;

// Eases in and out, so the playhead gathers speed and settles rather than
// crossing at a constant rate.
static float
EaseInOut(float t)
{
    return t * t * (3.0f - 2.0f * t);
}

// One slot: its track, the blocks this sweep has laid down so far, and the
// playhead while it is still crossing. local_seconds is the slot's own clock,
// already offset by its stagger, so it stays at or below zero until the slot
// starts.
static void
DrawSlot(ImDrawList* draw_list, const ImVec2& min, const ImVec2& max, size_t slot,
         float local_seconds, ImU32 track_color, ImU32 accent_color)
{
    const ImGuiStyle& style = ImGui::GetStyle();
    draw_list->AddRectFilled(min, max, track_color, style.FrameRounding);
    if(local_seconds <= 0.0f)
    {
        return;
    }

    const float sweep = std::floor(local_seconds / LOADING_SLOT_CYCLE_SECONDS);
    const float phase =
        (local_seconds - sweep * LOADING_SLOT_CYCLE_SECONDS) / LOADING_SLOT_CYCLE_SECONDS;
    const float reveal =
        EaseInOut(std::clamp(phase / LOADING_SLOT_SWEEP_SHARE, 0.0f, 1.0f));
    const float fade = std::clamp((1.0f - phase) / LOADING_SLOT_FADE_SHARE, 0.0f, 1.0f);

    const float height      = max.y - min.y;
    const float track_left  = min.x + style.FramePadding.x;
    const float track_width = std::max(0.0f, max.x - min.x - style.FramePadding.x * 2.0f);
    const float head_x      = track_left + track_width * reveal;
    const float block_height = height * LOADING_SLOT_BLOCK_HEIGHT_SHARE;
    const float block_top    = min.y + (height - block_height) * 0.5f;

    const size_t pattern = slot * LOADING_SLOT_PATTERN_SLOT_STEP +
                           static_cast<size_t>(sweep) * LOADING_SLOT_PATTERN_SWEEP_STEP;
    float        offset  = 0.0f;
    for(size_t block = pattern; offset < 1.0f; ++block)
    {
        const float left = track_left + track_width * offset;
        if(left >= head_x)
        {
            break;
        }
        const float width =
            LOADING_SLOT_BLOCK_WIDTHS[block % LOADING_SLOT_BLOCK_WIDTH_COUNT];
        const float right =
            std::min(head_x, track_left + track_width * std::min(1.0f, offset + width));
        const float alpha =
            (block % 2 == 0 ? LOADING_SLOT_BLOCK_ALPHA : LOADING_SLOT_BLOCK_ALT_ALPHA) *
            fade;
        draw_list->AddRectFilled(
            ImVec2(left, block_top), ImVec2(right, block_top + block_height),
            ApplyAlpha(accent_color, alpha), LOADING_SLOT_BLOCK_ROUNDING);
        offset += width + LOADING_SLOT_GAP_WIDTHS[block % LOADING_SLOT_GAP_WIDTH_COUNT];
    }

    if(reveal < 1.0f)
    {
        const float top       = min.y + LOADING_SLOT_PLAYHEAD_INSET;
        const float bottom    = max.y - LOADING_SLOT_PLAYHEAD_INSET;
        const float half_band = LOADING_SLOT_PLAYHEAD_BAND_WIDTH * 0.5f;
        draw_list->AddRectFilled(
            ImVec2(head_x - half_band, top), ImVec2(head_x + half_band, bottom),
            ApplyAlpha(accent_color, LOADING_SLOT_PLAYHEAD_BAND_ALPHA), half_band);
        draw_list->AddLine(ImVec2(head_x, top), ImVec2(head_x, bottom), accent_color,
                           LOADING_SLOT_PLAYHEAD_WIDTH);
    }
}

void
RenderLoadingSlots(size_t slot_count, float elapsed_seconds, SettingsManager* settings)
{
    if(settings == nullptr)
    {
        settings = &SettingsManager::GetInstance();
    }
    const ImU32 track_color  = settings->GetColor(Colors::kButton);
    const ImU32 accent_color = settings->GetColor(Colors::kAccent);
    const float width        = std::max(0.0f, ImGui::GetContentRegionAvail().x);
    const float height       = ImGui::GetFrameHeight();
    ImDrawList* draw_list    = ImGui::GetWindowDrawList();

    for(size_t slot = 0; slot < slot_count; ++slot)
    {
        const ImVec2 min = ImGui::GetCursorScreenPos();
        ImGui::Dummy(ImVec2(width, height));
        DrawSlot(draw_list, min, ImVec2(min.x + width, min.y + height), slot,
                 elapsed_seconds -
                     static_cast<float>(slot) * LOADING_SLOT_STAGGER_SECONDS,
                 track_color, accent_color);
    }
    RenderScheduler::GetInstance().RequestRender();
}

}  // namespace View
}  // namespace RocProfVis
