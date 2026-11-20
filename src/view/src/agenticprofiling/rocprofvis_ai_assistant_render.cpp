// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

// The Ask Optiq panel's drawing half: the dock and its splitter, the
// transcript and its cards, the wait animation, the composer, the context
// pill, the activity strip, and the toolbar button that opens it. Everything
// here reads panel state and emits ImGui; the turn machinery that fills that
// state lives in rocprofvis_ai_assistant.cpp, and the two only meet through
// AssistantPanel members.

#include "rocprofvis_ai_assistant.h"

#include <algorithm>
#include <cfloat>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

#include "imgui.h"
#include "textselect.hpp"

#include "icons/rocprovfis_icon_defines.h"
#include "model/rocprofvis_summary_model.h"
#include "rocprofvis_ai_tool_schema.h"
#include "rocprofvis_appwindow.h"
#include "rocprofvis_data_provider.h"
#include "rocprofvis_render_scheduler.h"
#include "rocprofvis_settings_manager.h"
#include "rocprofvis_timeline_selection.h"
#include "widgets/rocprofvis_gui_helpers.h"
#include "widgets/rocprofvis_notification_manager.h"
#include "widgets/rocprofvis_tab_container.h"

namespace RocProfVis
{
namespace View
{

namespace
{

constexpr float  ASSISTANT_MIN_WIDTH      = 440.0f;
constexpr float  ASSISTANT_MAX_WIDTH      = 900.0f;
constexpr float  ASSISTANT_SPLITTER_WIDTH = 6.0f;
// Same padding the remote/profiler dialogs use.
constexpr ImVec2 ASSISTANT_WINDOW_PADDING = ImVec2(14.0f, 12.0f);
constexpr ImVec2 ASSISTANT_CARD_PADDING   = ImVec2(14.0f, 10.0f);
constexpr float  ASSISTANT_SEND_WIDTH     = 80.0f;
// Composer row: input, Send, and three icon buttons (copy, export, clear),
// with four gaps between the five items.
constexpr float  ASSISTANT_COMPOSER_MIN_INPUT_WIDTH = 80.0f;
constexpr int    ASSISTANT_COMPOSER_ICON_BUTTONS    = 3;
constexpr int    ASSISTANT_COMPOSER_GAP_COUNT       = 4;
// While a turn runs, the card under the transcript draws the work in Optiq's
// own terms: trace lanes on the right, a compute die on the left, and a bus
// joining them. A read pulls events along the lanes into the die. While the
// model thinks the lanes hold still and flow links draw between events, the way
// the timeline draws flow arrows; writing the answer gathers the links into one
// point; a wait holds everything still and dim. Every colour on it comes from
// the theme's own accent family, so it follows light and dark like the rest of
// the panel.
constexpr float   ASSISTANT_STAGE_HEIGHT           = 108.0f;
constexpr float   ASSISTANT_STAGE_RADIUS           = 8.0f;
constexpr float   ASSISTANT_STAGE_PAD              = 16.0f;
constexpr float   ASSISTANT_ACTIVITY_RESERVE       = 245.0f;
constexpr int64_t ASSISTANT_ACTIVITY_CLOCK_SECONDS = 1;
constexpr int     ASSISTANT_LANES                  = 5;
constexpr int     ASSISTANT_LANE_BLOCKS            = 32;
constexpr float   ASSISTANT_LANE_SPEEDS[]          = { 0.84f, 1.0f, 0.9f, 1.12f, 0.96f };
constexpr float   ASSISTANT_BLOCK_HALF_HEIGHT      = 4.5f;
constexpr float   ASSISTANT_EDGE_FADE              = 48.0f;
constexpr float   ASSISTANT_RULER_STEP             = 22.0f;
constexpr int     ASSISTANT_RULER_MAJOR            = 5;
constexpr float   ASSISTANT_HEAD_CAST              = 76.0f;
constexpr int     ASSISTANT_DIE_TILES              = 4;
constexpr float   ASSISTANT_DIE_SIZE               = 54.0f;
constexpr float   ASSISTANT_DIE_INSET              = 7.0f;
constexpr float   ASSISTANT_DIE_GAP                = 3.0f;
constexpr float   ASSISTANT_DIE_RADIUS             = 7.0f;
constexpr float   ASSISTANT_READ_SPEED             = 72.0f;
constexpr float   ASSISTANT_SPEED_EASE             = 5.0f;
constexpr float   ASSISTANT_FLASH_LENGTH           = 30.0f;
constexpr int     ASSISTANT_FLOW_LINKS             = 3;
constexpr float   ASSISTANT_FLOW_PERIOD            = 2.8f;
constexpr int     ASSISTANT_FLOW_SAMPLES           = 28;
constexpr float   ASSISTANT_SHIMMER_SPEED          = 150.0f;
constexpr float   ASSISTANT_SHIMMER_REST           = 160.0f;
constexpr size_t  ASSISTANT_STEP_CHIPS             = 6;
constexpr float   ASSISTANT_TAU                    = 6.2831853f;
static_assert(sizeof(ASSISTANT_LANE_SPEEDS) / sizeof(ASSISTANT_LANE_SPEEDS[0]) ==
                  ASSISTANT_LANES,
              "one scroll speed per lane");

// Which wait the stage is drawing.
enum class AssistantMotion : uint8_t
{
    kThink,
    kRead,
    kWrite,
    kWait
};

struct ActivityWords
{
    std::string title;
    const char* detail;
};

// One block of a lane's repeating pattern, before it is scrolled.
struct LanePatternBlock
{
    float    x;
    float    width;
    float    weight;
    uint32_t id;
};

// A block wholly on the stage this frame, for a flow link to land on.
struct LaneBlock
{
    float x0;
    float x1;
};

float
Smooth01(float x)
{
    const float c = std::clamp(x, 0.0f, 1.0f);
    return c * c * (3.0f - 2.0f * c);
}

// One peak per cycle. A higher sharpness makes the peak narrower.
float
Wave(float radians, float sharpness)
{
    return std::pow(0.5f + 0.5f * std::sin(radians), sharpness);
}

// An integer hash, so every frame rebuilds the same pattern without storing it.
uint32_t
Mix(uint32_t x)
{
    x ^= x >> 16;
    x *= 0x0be9c77du;
    x ^= x >> 13;
    x *= 0x5cebb869u;
    x ^= x >> 15;
    return x;
}

float
Unit(uint32_t h)
{
    return static_cast<float>(h >> 8) * (1.0f / 16777216.0f);
}

ImVec2
Lerp2(ImVec2 a, ImVec2 b, float u)
{
    return ImVec2(a.x + (b.x - a.x) * u, a.y + (b.y - a.y) * u);
}

ImVec2
Bezier(ImVec2 p0, ImVec2 p1, ImVec2 p2, ImVec2 p3, float u)
{
    const float v = 1.0f - u;
    const float a = v * v * v;
    const float b = 3.0f * v * v * u;
    const float c = 3.0f * v * u * u;
    const float d = u * u * u;
    return ImVec2(a * p0.x + b * p1.x + c * p2.x + d * p3.x,
                  a * p0.y + b * p1.y + c * p2.y + d * p3.y);
}

ImU32
MixColor(ImU32 a, ImU32 b, float u)
{
    const ImVec4 x = ImGui::ColorConvertU32ToFloat4(a);
    const ImVec4 y = ImGui::ColorConvertU32ToFloat4(b);
    return ImGui::ColorConvertFloat4ToU32(
        ImVec4(x.x + (y.x - x.x) * u, x.y + (y.y - x.y) * u, x.z + (y.z - x.z) * u,
               x.w + (y.w - x.w) * u));
}

// The stage's colours. What is lit has to stand out from what is idle in either
// theme, which means a deeper accent on light and a lighter one on dark: a light
// theme gets a white panel with its pressed accent for lit things, a dark theme
// a panel a step below its background with its hover accent lifted toward
// white.
struct StagePalette
{
    ImU32 stage;
    ImU32 edge;
    ImU32 accent;
    ImU32 hot;
};

StagePalette
MakeStagePalette(SettingsManager& settings)
{
    StagePalette palette;
    palette.accent = settings.GetColor(Colors::kAccent);
    if(settings.GetUserSettings().display_settings.use_dark_mode)
    {
        palette.stage =
            MixColor(settings.GetColor(Colors::kBgMain), IM_COL32(0, 0, 0, 255), 0.25f);
        palette.edge = ApplyAlpha(palette.accent, 0.40f);
        palette.hot  = MixColor(settings.GetColor(Colors::kAccentHover),
                                IM_COL32(255, 255, 255, 255), 0.35f);
    }
    else
    {
        palette.stage = settings.GetColor(Colors::kBgPanel);
        palette.edge  = ApplyAlpha(palette.accent, 0.28f);
        palette.hot   = settings.GetColor(Colors::kAccentActive);
    }
    return palette;
}

// A lit point with a soft halo: accent light around a core in the lit colour.
void
Glow(ImDrawList* draw, ImVec2 center, float radius, ImU32 accent, ImU32 bright,
     float strength)
{
    draw->AddCircleFilled(center, radius * 3.2f, ApplyAlpha(accent, 0.10f * strength),
                          24);
    draw->AddCircleFilled(center, radius * 1.9f, ApplyAlpha(accent, 0.28f * strength),
                          20);
    draw->AddCircleFilled(center, radius, ApplyAlpha(bright, strength), 16);
}

int
BuildLanePattern(int lane, float period, LanePatternBlock* out)
{
    uint32_t h     = Mix(static_cast<uint32_t>(lane + 1) * 0x1252973bu);
    float    x     = Unit(h) * 18.0f;
    int      count = 0;
    while(count < ASSISTANT_LANE_BLOCKS)
    {
        h             = Mix(h + 0x756c4116u);
        const float w = 8.0f + 46.0f * Unit(h) * Unit(Mix(h ^ 0x45cc2e0au));
        if(x + w > period - 6.0f)
        {
            break;
        }
        h                 = Mix(h + 0xb0d6b60au);
        out[count].x      = x;
        out[count].width  = w;
        out[count].weight = Unit(Mix(h ^ 0x878050edu));
        out[count].id     = h;
        ++count;
        x += w + 5.0f + 24.0f * Unit(h);
    }
    return count;
}

float
PathLength(const ImVec2* points, int count)
{
    float total = 0.0f;
    for(int i = 0; i + 1 < count; ++i)
    {
        const float dx = points[i + 1].x - points[i].x;
        const float dy = points[i + 1].y - points[i].y;
        total += std::sqrt(dx * dx + dy * dy);
    }
    return total;
}

// The point d pixels along a polyline, held at its far end.
ImVec2
AlongPath(const ImVec2* points, int count, float d)
{
    for(int i = 0; i + 1 < count; ++i)
    {
        const float dx  = points[i + 1].x - points[i].x;
        const float dy  = points[i + 1].y - points[i].y;
        const float len = std::sqrt(dx * dx + dy * dy);
        if(len > 0.0f && d <= len)
        {
            return Lerp2(points[i], points[i + 1], d / len);
        }
        d -= len;
    }
    return points[count - 1];
}

// The block nearest x whose start is at or past min_x0, or null.
const LaneBlock*
NearestBlock(const LaneBlock* blocks, int count, float x, float min_x0)
{
    const LaneBlock* best      = nullptr;
    float            best_dist = FLT_MAX;
    for(int i = 0; i < count; ++i)
    {
        if(blocks[i].x0 < min_x0)
        {
            continue;
        }
        const float dist = std::fabs((blocks[i].x0 + blocks[i].x1) * 0.5f - x);
        if(dist < best_dist)
        {
            best      = &blocks[i];
            best_dist = dist;
        }
    }
    return best;
}

void
LightBlock(ImDrawList* draw, const LaneBlock& block, float y, ImU32 accent, ImU32 bright,
           float alpha)
{
    const ImVec2 a(block.x0, y - ASSISTANT_BLOCK_HALF_HEIGHT);
    const ImVec2 b(block.x1, y + ASSISTANT_BLOCK_HALF_HEIGHT);
    draw->AddRectFilled(ImVec2(a.x - 3.0f, a.y - 3.0f), ImVec2(b.x + 3.0f, b.y + 3.0f),
                        ApplyAlpha(accent, 0.22f * alpha), 5.0f);
    draw->AddRectFilled(a, b, ApplyAlpha(bright, 0.95f * alpha), 2.0f);
}

/*
 * One flow link, drawn the way the timeline draws flow arrows: out of the end
 * of one event and into the start of another. It lifts over the lanes so it
 * reads against them. draw_u grows it in from the source, travel_u runs a spark
 * along it once it is whole (negative for none), and alpha fades it out.
 */
void
DrawFlowLink(ImDrawList* draw, ImVec2 from, ImVec2 to, float lift, float draw_u,
             float travel_u, float alpha, ImU32 accent, ImU32 bright, bool arrow)
{
    const float  dx  = to.x - from.x;
    const float  top = std::min(from.y, to.y) - lift;
    const ImVec2 c1(from.x + dx * 0.32f, top);
    const ImVec2 c2(to.x - dx * 0.32f, top);

    ImVec2 points[ASSISTANT_FLOW_SAMPLES];
    for(int i = 0; i < ASSISTANT_FLOW_SAMPLES; ++i)
    {
        const float u = draw_u * static_cast<float>(i) /
                        static_cast<float>(ASSISTANT_FLOW_SAMPLES - 1);
        points[i] = Bezier(from, c1, c2, to, u);
    }
    draw->AddPolyline(points, ASSISTANT_FLOW_SAMPLES, ApplyAlpha(accent, 0.18f * alpha),
                      ImDrawFlags_None, 6.0f);
    draw->AddPolyline(points, ASSISTANT_FLOW_SAMPLES, ApplyAlpha(accent, 0.55f * alpha),
                      ImDrawFlags_None, 2.6f);
    draw->AddPolyline(points, ASSISTANT_FLOW_SAMPLES, ApplyAlpha(bright, 0.95f * alpha),
                      ImDrawFlags_None, 1.2f);
    if(draw_u < 1.0f)
    {
        Glow(draw, points[ASSISTANT_FLOW_SAMPLES - 1], 2.4f, accent, bright, alpha);
        return;
    }
    if(arrow)
    {
        float       tx  = to.x - c2.x;
        float       ty  = to.y - c2.y;
        const float len = std::sqrt(tx * tx + ty * ty);
        if(len > 0.0f)
        {
            constexpr float HEAD = 5.5f;
            tx /= len;
            ty /= len;
            const ImVec2 base(to.x - tx * HEAD, to.y - ty * HEAD);
            draw->AddTriangleFilled(
                to, ImVec2(base.x - ty * HEAD * 0.55f, base.y + tx * HEAD * 0.55f),
                ImVec2(base.x + ty * HEAD * 0.55f, base.y - tx * HEAD * 0.55f),
                ApplyAlpha(bright, alpha));
        }
    }
    if(travel_u >= 0.0f && travel_u <= 1.0f)
    {
        Glow(draw, Bezier(from, c1, c2, to, travel_u), 2.2f, accent, bright, alpha);
    }
}

void
RenderActivityStage(AssistantMotion motion, float width, double scroll,
                    const StagePalette& palette)
{
    const ImVec2 o = ImGui::GetCursorScreenPos();
    ImGui::Dummy(ImVec2(width, ASSISTANT_STAGE_HEIGHT));
    const ImVec2 e(o.x + width, o.y + ASSISTANT_STAGE_HEIGHT);
    ImDrawList*  draw    = ImGui::GetWindowDrawList();
    const float  t       = static_cast<float>(ImGui::GetTime());
    const bool   reading = motion == AssistantMotion::kRead;
    const float  dim     = motion == AssistantMotion::kWait ? 0.55f : 1.0f;
    const ImU32  accent  = palette.accent;
    const ImU32  bright  = palette.hot;
    const ImU32  stage   = palette.stage;

    draw->AddRectFilled(o, e, stage, ASSISTANT_STAGE_RADIUS);
    draw->AddRect(o, e, palette.edge, ASSISTANT_STAGE_RADIUS, ImDrawFlags_None, 1.0f);
    draw->PushClipRect(ImVec2(o.x + 1.0f, o.y + 1.0f), ImVec2(e.x - 1.0f, e.y - 1.0f),
                       true);

    const float  lane_h = (ASSISTANT_STAGE_HEIGHT - ASSISTANT_STAGE_PAD * 2.0f) /
                         static_cast<float>(ASSISTANT_LANES);
    const float  die_x0 = o.x + 16.0f;
    const float  die_y0 = o.y + (ASSISTANT_STAGE_HEIGHT - ASSISTANT_DIE_SIZE) * 0.5f;
    const float  die_x1 = die_x0 + ASSISTANT_DIE_SIZE;
    const float  die_y1 = die_y0 + ASSISTANT_DIE_SIZE;
    const ImVec2 die_c((die_x0 + die_x1) * 0.5f, (die_y0 + die_y1) * 0.5f);
    const float  bend_x = die_x1 + 7.0f;

    float lane_y[ASSISTANT_LANES];
    float pin_y[ASSISTANT_LANES];
    float reach = 0.0f;
    for(int i = 0; i < ASSISTANT_LANES; ++i)
    {
        const float along =
            static_cast<float>(i) / static_cast<float>(ASSISTANT_LANES - 1);
        lane_y[i] =
            o.y + ASSISTANT_STAGE_PAD + lane_h * (static_cast<float>(i) + 0.5f);
        pin_y[i] = die_y0 + ASSISTANT_DIE_SIZE * (0.2f + 0.6f * along);
        reach    = std::max(reach, std::fabs(lane_y[i] - pin_y[i]));
    }
    const float port_x   = bend_x + reach + 10.0f;
    const float lanes_x1 = e.x - ASSISTANT_STAGE_PAD;
    const float lanes_w  = std::max(60.0f, lanes_x1 - port_x);
    const float period   = std::max(lanes_w * 1.45f, 180.0f);

    // Rails and bus first, so everything that moves draws over them. Each bus
    // runs port to die, with a 45 degree jog onto its pin.
    ImVec2 bus[ASSISTANT_LANES][4];
    float  bus_len[ASSISTANT_LANES];
    for(int i = 0; i < ASSISTANT_LANES; ++i)
    {
        const float dy = lane_y[i] - pin_y[i];
        bus[i][0]      = ImVec2(port_x, lane_y[i]);
        bus[i][1]      = ImVec2(bend_x + std::fabs(dy), lane_y[i]);
        bus[i][2]      = ImVec2(bend_x, pin_y[i]);
        bus[i][3]      = ImVec2(die_x1, pin_y[i]);
        bus_len[i]     = PathLength(bus[i], 4);
        draw->AddRectFilled(ImVec2(port_x, lane_y[i] - 5.5f),
                            ImVec2(lanes_x1, lane_y[i] + 5.5f),
                            ApplyAlpha(accent, 0.08f * dim), 5.5f);
        draw->AddPolyline(bus[i], 4, ApplyAlpha(accent, 0.45f * dim), ImDrawFlags_None,
                          1.3f);
    }

    // A ruler like the timeline's, scrolling with the lanes, so the stage reads
    // as a stretch of trace rather than a pattern.
    const float  ruler_y      = o.y + 5.0f;
    const double ruler_period = static_cast<double>(ASSISTANT_RULER_STEP) *
                                static_cast<double>(ASSISTANT_RULER_MAJOR);
    const float  ruler_shift  = static_cast<float>(std::fmod(scroll, ruler_period));
    for(int k = 0;; ++k)
    {
        const float x =
            port_x - ruler_shift + static_cast<float>(k) * ASSISTANT_RULER_STEP;
        if(x > lanes_x1)
        {
            break;
        }
        if(x < port_x)
        {
            continue;
        }
        const bool  major = k % ASSISTANT_RULER_MAJOR == 0;
        const float fade  = Smooth01((lanes_x1 - x) / ASSISTANT_EDGE_FADE);
        draw->AddLine(ImVec2(x, ruler_y), ImVec2(x, ruler_y + (major ? 5.0f : 2.5f)),
                      ApplyAlpha(bright, (major ? 0.45f : 0.22f) * fade * dim), 1.0f);
    }

    // The read head: the port edge lights up and casts onto the blocks
    // arriving at it.
    if(reading)
    {
        const float head_top    = lane_y[0] - 9.0f;
        const float head_bottom = lane_y[ASSISTANT_LANES - 1] + 9.0f;
        draw->AddRectFilledMultiColor(
            ImVec2(port_x, head_top), ImVec2(port_x + ASSISTANT_HEAD_CAST, head_bottom),
            ApplyAlpha(accent, 0.22f), ApplyAlpha(accent, 0.0f),
            ApplyAlpha(accent, 0.0f), ApplyAlpha(accent, 0.22f));
        draw->AddLine(ImVec2(port_x, head_top), ImVec2(port_x, head_bottom),
                      ApplyAlpha(accent, 0.30f), 6.0f);
        draw->AddLine(ImVec2(port_x, head_top), ImVec2(port_x, head_bottom),
                      ApplyAlpha(bright, 0.85f), 1.2f);
    }

    // The lanes. A read brightens blocks as they near the port, holds the one
    // crossing it at full strength, and sends a spark down the bus for each
    // block that goes in; the spark lights a tile when it reaches the die.
    float            flash[ASSISTANT_DIE_TILES * ASSISTANT_DIE_TILES] = {};
    bool             port_hot[ASSISTANT_LANES]                         = {};
    LaneBlock        live[ASSISTANT_LANES][ASSISTANT_LANE_BLOCKS];
    int              live_count[ASSISTANT_LANES] = {};
    LanePatternBlock pattern[ASSISTANT_LANE_BLOCKS];
    for(int i = 0; i < ASSISTANT_LANES; ++i)
    {
        const int   count = BuildLanePattern(i, period, pattern);
        const float shift = static_cast<float>(
            std::fmod(scroll * ASSISTANT_LANE_SPEEDS[i], static_cast<double>(period)));
        draw->PushClipRect(ImVec2(port_x, o.y), ImVec2(lanes_x1, e.y), true);
        for(int b = 0; b < count; ++b)
        {
            for(int copy = 0; copy < 2; ++copy)
            {
                const float c = pattern[b].x - shift + (copy == 0 ? 0.0f : period);
                if(c >= lanes_w || c + pattern[b].width <= 0.0f)
                {
                    continue;
                }
                const float x0 = port_x + c;
                const float x1 = x0 + pattern[b].width;
                const float fade =
                    Smooth01((lanes_x1 - (x0 + x1) * 0.5f) / ASSISTANT_EDGE_FADE);
                float alpha = (0.30f + 0.42f * pattern[b].weight) * fade;
                ImU32 color = accent;
                if(reading && c <= 0.0f)
                {
                    alpha       = 1.0f;
                    color       = bright;
                    port_hot[i] = true;
                }
                else if(reading)
                {
                    const float near = Smooth01(1.0f - c / ASSISTANT_HEAD_CAST);
                    alpha = std::min(1.0f, alpha * (1.0f + 0.8f * near));
                    color = MixColor(accent, bright, 0.6f * near);
                }
                else
                {
                    alpha *= 0.82f + 0.18f * std::sin(t * 1.3f + Unit(pattern[b].id) *
                                                                      ASSISTANT_TAU);
                }
                draw->AddRectFilled(ImVec2(x0, lane_y[i] - ASSISTANT_BLOCK_HALF_HEIGHT),
                                    ImVec2(x1, lane_y[i] + ASSISTANT_BLOCK_HALF_HEIGHT),
                                    ApplyAlpha(color, alpha * dim), 2.0f);
                if(c >= 4.0f && x1 <= lanes_x1 - 6.0f &&
                   live_count[i] < ASSISTANT_LANE_BLOCKS)
                {
                    live[i][live_count[i]].x0 = x0;
                    live[i][live_count[i]].x1 = x1;
                    ++live_count[i];
                }
            }
        }
        draw->PopClipRect();

        if(!reading)
        {
            continue;
        }
        for(int b = 0; b < count; ++b)
        {
            for(int copy = 0; copy < 2; ++copy)
            {
                const float travelled =
                    shift - pattern[b].x - (copy == 0 ? 0.0f : period);
                if(travelled < 0.0f)
                {
                    continue;
                }
                if(travelled <= bus_len[i])
                {
                    Glow(draw, AlongPath(bus[i], 4, travelled), 2.0f, accent, bright,
                         1.0f);
                }
                else if(travelled <= bus_len[i] + ASSISTANT_FLASH_LENGTH)
                {
                    const uint32_t tile =
                        pattern[b].id % (ASSISTANT_DIE_TILES * ASSISTANT_DIE_TILES);
                    flash[tile] =
                        std::max(flash[tile], 1.0f - (travelled - bus_len[i]) /
                                                         ASSISTANT_FLASH_LENGTH);
                }
            }
        }
    }

    for(int i = 0; i < ASSISTANT_LANES; ++i)
    {
        const ImVec2 port(port_x, lane_y[i]);
        if(port_hot[i])
        {
            Glow(draw, port, 2.6f, accent, bright, 1.0f);
        }
        else
        {
            draw->AddCircleFilled(port, 2.0f, ApplyAlpha(accent, 0.70f * dim), 12);
        }
    }

    // The die: a halo whose strength follows the phase, a solid body over it,
    // pins on the three sides the bus does not use, and a grid of compute
    // units. A read runs a wavefront across the grid, thinking ripples out
    // from the middle, writing glows steadily, and a wait beats one unit.
    float halo = 0.25f;
    if(reading)
    {
        halo = 0.85f + 0.15f * std::sin(t * 3.1f);
    }
    else if(motion == AssistantMotion::kThink)
    {
        halo = 0.45f + 0.30f * Wave(t * 1.6f, 2.0f);
    }
    else if(motion == AssistantMotion::kWrite)
    {
        halo = 0.70f;
    }
    for(int ring = 10; ring >= 0; --ring)
    {
        draw->AddCircleFilled(
            die_c, ASSISTANT_DIE_SIZE * 0.5f + static_cast<float>(ring) * 2.4f,
            ApplyAlpha(accent, 0.032f * halo), 48);
    }
    if(motion == AssistantMotion::kWait)
    {
        const float u    = std::fmod(t * 0.45f, 1.0f);
        const float grow = 2.0f + 12.0f * u;
        draw->AddRect(ImVec2(die_x0 - grow, die_y0 - grow),
                      ImVec2(die_x1 + grow, die_y1 + grow),
                      ApplyAlpha(accent, 0.35f * (1.0f - u)),
                      ASSISTANT_DIE_RADIUS + grow, ImDrawFlags_None, 1.2f);
    }
    draw->AddRectFilled(ImVec2(die_x0, die_y0), ImVec2(die_x1, die_y1), stage,
                        ASSISTANT_DIE_RADIUS);
    draw->AddRectFilled(ImVec2(die_x0, die_y0), ImVec2(die_x1, die_y1),
                        ApplyAlpha(accent, 0.14f), ASSISTANT_DIE_RADIUS);
    draw->AddRect(ImVec2(die_x0, die_y0), ImVec2(die_x1, die_y1),
                  ApplyAlpha(accent, 0.75f * dim), ASSISTANT_DIE_RADIUS,
                  ImDrawFlags_None, 1.2f);
    draw->AddRect(ImVec2(die_x0 + 3.5f, die_y0 + 3.5f),
                  ImVec2(die_x1 - 3.5f, die_y1 - 3.5f), ApplyAlpha(accent, 0.25f * dim),
                  ASSISTANT_DIE_RADIUS - 3.0f, ImDrawFlags_None, 1.0f);

    const float tile = (ASSISTANT_DIE_SIZE - ASSISTANT_DIE_INSET * 2.0f -
                        ASSISTANT_DIE_GAP * static_cast<float>(ASSISTANT_DIE_TILES - 1)) /
                       static_cast<float>(ASSISTANT_DIE_TILES);
    const ImU32 pin = ApplyAlpha(accent, 0.60f * dim);
    for(int k = 0; k < ASSISTANT_DIE_TILES; ++k)
    {
        const float step = ASSISTANT_DIE_INSET + tile * 0.5f +
                           static_cast<float>(k) * (tile + ASSISTANT_DIE_GAP);
        draw->AddLine(ImVec2(die_x0 + step, die_y0 - 4.0f), ImVec2(die_x0 + step, die_y0),
                      pin, 1.4f);
        draw->AddLine(ImVec2(die_x0 + step, die_y1), ImVec2(die_x0 + step, die_y1 + 4.0f),
                      pin, 1.4f);
        draw->AddLine(ImVec2(die_x0 - 4.0f, die_y0 + step), ImVec2(die_x0, die_y0 + step),
                      pin, 1.4f);
    }
    for(int r = 0; r < ASSISTANT_DIE_TILES; ++r)
    {
        for(int c = 0; c < ASSISTANT_DIE_TILES; ++c)
        {
            const float fr  = static_cast<float>(r);
            const float fc  = static_cast<float>(c);
            float       lit = 0.16f;
            if(reading)
            {
                lit = 0.22f + 0.78f * Wave(t * 6.0f - (fc * 0.9f + fr * 0.55f), 3.0f);
            }
            else if(motion == AssistantMotion::kThink)
            {
                const float d =
                    std::sqrt((fr - 1.5f) * (fr - 1.5f) + (fc - 1.5f) * (fc - 1.5f));
                lit = 0.18f + 0.70f * Wave(t * 2.2f - d * 1.6f, 3.0f);
            }
            else if(motion == AssistantMotion::kWrite)
            {
                lit = 0.40f + 0.40f * Wave(t * 1.7f + (fr + fc) * 0.6f, 1.0f);
            }
            else if(r == 1 && c == 2)
            {
                lit += 0.6f * Wave(t * 1.8f, 6.0f);
            }
            lit = std::min(1.0f, std::max(lit, flash[r * ASSISTANT_DIE_TILES + c]));
            const float  pitch = tile + ASSISTANT_DIE_GAP;
            const ImVec2 a(die_x0 + ASSISTANT_DIE_INSET + fc * pitch,
                           die_y0 + ASSISTANT_DIE_INSET + fr * pitch);
            const ImVec2 b(a.x + tile, a.y + tile);
            if(lit > 0.5f)
            {
                draw->AddRectFilled(ImVec2(a.x - 2.0f, a.y - 2.0f),
                                    ImVec2(b.x + 2.0f, b.y + 2.0f),
                                    ApplyAlpha(accent, (lit - 0.5f) * 0.7f), 3.0f);
            }
            const ImU32 unit = MixColor(accent, bright, lit * lit);
            draw->AddRectFilled(a, b, ApplyAlpha(unit, lit), 1.5f);
        }
    }

    // Thinking links one event to another; writing gathers events into one
    // point, the answer. Each link picks its blocks from its own cycle number,
    // so it stays on the same pair for its whole life.
    if(motion == AssistantMotion::kThink || motion == AssistantMotion::kWrite)
    {
        const bool   gather = motion == AssistantMotion::kWrite;
        const ImVec2 sink(lanes_x1 - 20.0f, o.y + ASSISTANT_STAGE_HEIGHT * 0.5f);
        for(int k = 0; k < ASSISTANT_FLOW_LINKS; ++k)
        {
            const float stagger =
                static_cast<float>(k) / static_cast<float>(ASSISTANT_FLOW_LINKS);
            const float    cycle = t / ASSISTANT_FLOW_PERIOD + stagger;
            const float    u     = cycle - std::floor(cycle);
            const uint32_t generation = static_cast<uint32_t>(std::floor(cycle));
            const uint32_t slot       = static_cast<uint32_t>(k);
            const uint32_t h = Mix(generation * 0x230c44d1u + slot * 0xa29f24dfu + 1u);
            const int      from_lane = static_cast<int>(h % ASSISTANT_LANES);
            const LaneBlock* from =
                NearestBlock(live[from_lane], live_count[from_lane],
                             port_x + lanes_w * (0.05f + 0.40f * Unit(Mix(h ^ 0x1u))),
                             -FLT_MAX);
            if(from == nullptr)
            {
                continue;
            }
            const ImVec2     start(from->x1, lane_y[from_lane]);
            ImVec2           end     = sink;
            const LaneBlock* to      = nullptr;
            int              to_lane = from_lane;
            if(gather)
            {
                if(start.x > sink.x - 40.0f)
                {
                    continue;
                }
            }
            else
            {
                to_lane = (from_lane + 1 +
                           static_cast<int>((h >> 12) % (ASSISTANT_LANES - 1))) %
                          ASSISTANT_LANES;
                const float floor_x = from->x1 + 26.0f;
                const float want_x  = floor_x + lanes_w * 0.35f * Unit(Mix(h ^ 0x2u));
                to = NearestBlock(live[to_lane], live_count[to_lane], want_x, floor_x);
                if(to == nullptr)
                {
                    continue;
                }
                end = ImVec2(to->x0, lane_y[to_lane]);
            }
            const float draw_u = Smooth01(u / 0.42f);
            const float alpha  = u < 0.8f ? 1.0f : 1.0f - (u - 0.8f) / 0.2f;
            const float travel = u < 0.42f ? -1.0f : Smooth01((u - 0.42f) / 0.38f);
            LightBlock(draw, *from, lane_y[from_lane], accent, bright, alpha);
            if(to != nullptr && draw_u >= 1.0f)
            {
                LightBlock(draw, *to, lane_y[to_lane], accent, bright, alpha);
            }
            DrawFlowLink(draw, start, end, 14.0f + 8.0f * Unit(h >> 4), draw_u, travel,
                         alpha, accent, bright, !gather);
        }
        if(gather)
        {
            const float ring = std::fmod(t * 0.9f, 1.0f);
            draw->AddCircleFilled(sink, 15.0f, ApplyAlpha(stage, 0.9f), 28);
            draw->AddCircle(sink, 5.0f + 12.0f * ring,
                            ApplyAlpha(accent, 0.45f * (1.0f - ring)), 28, 1.2f);
            Glow(draw, sink, 3.4f + Wave(t * 3.0f, 2.0f), accent, bright, 1.0f);
        }
    }
    draw->PopClipRect();
}

// Draws text with a highlight that sweeps across it and then rests, so a title
// reads as live rather than stuck. Uses the current font.
void
RenderShimmerText(const std::string& text, ImU32 base, ImU32 glint)
{
    const ImVec2 pos  = ImGui::GetCursorScreenPos();
    const ImVec2 size = ImGui::CalcTextSize(text.c_str());
    ImGui::Dummy(size);
    ImDrawList*  draw  = ImGui::GetWindowDrawList();
    ImFont*      font  = ImGui::GetFont();
    const float  fsize = ImGui::GetFontSize();
    const float  band  = std::max(24.0f, fsize * 2.0f);
    const float  cycle = size.x + band * 2.0f + ASSISTANT_SHIMMER_REST;
    const float  sweep =
        std::fmod(static_cast<float>(ImGui::GetTime()) * ASSISTANT_SHIMMER_SPEED, cycle) -
        band;
    const ImVec4 from  = ImGui::ColorConvertU32ToFloat4(base);
    const ImVec4 to    = ImGui::ColorConvertU32ToFloat4(glint);
    const char*  begin = text.c_str();
    const char*  end   = begin + text.size();
    const char*  glyph = begin;
    while(glyph < end)
    {
        const char* next = glyph + 1;
        while(next < end && (static_cast<unsigned char>(*next) & 0xC0u) == 0x80u)
        {
            ++next;
        }
        const float  x0 = font->CalcTextSizeA(fsize, FLT_MAX, 0.0f, begin, glyph).x;
        const float  x1 = font->CalcTextSizeA(fsize, FLT_MAX, 0.0f, begin, next).x;
        const float  k  = Smooth01(1.0f - std::fabs((x0 + x1) * 0.5f - sweep) / band);
        const ImVec4 mix(from.x + (to.x - from.x) * k, from.y + (to.y - from.y) * k,
                         from.z + (to.z - from.z) * k, from.w + (to.w - from.w) * k);
        draw->AddText(font, fsize, ImVec2(pos.x + x0, pos.y),
                      ImGui::ColorConvertFloat4ToU32(mix), glyph, next);
        glyph = next;
    }
}

// Rounded chips that wrap at the card edge. Accent chips describe the read in
// flight; quiet ones list the steps the turn has already taken.
void
RenderChips(const std::vector<std::string>& chips, bool accent_style,
            SettingsManager& settings)
{
    if(chips.empty())
    {
        return;
    }
    constexpr float GAP    = 5.0f;
    const ImVec2    pad    = ImVec2(7.0f, 2.0f);
    const ImU32     accent = settings.GetColor(Colors::kAccent);
    const ImU32     fill   = accent_style ? ApplyAlpha(accent, 0.10f)
                                          : settings.GetColor(Colors::kBgMain);
    const ImU32     edge   = accent_style ? ApplyAlpha(accent, 0.32f)
                                          : settings.GetColor(Colors::kPanelBorderSubtle);
    const ImU32     ink =
        settings.GetColor(accent_style ? Colors::kTextMain : Colors::kTextDim);

    ImGui::PushFont(nullptr, settings.GetFontManager().GetFontSize(FontSize::kSmall));
    const float avail = ImGui::GetContentRegionAvail().x;
    float       used  = 0.0f;
    for(size_t i = 0; i < chips.size(); ++i)
    {
        const ImVec2 text = ImGui::CalcTextSize(chips[i].c_str());
        const ImVec2 size(text.x + pad.x * 2.0f, text.y + pad.y * 2.0f);
        if(i > 0 && used + GAP + size.x <= avail)
        {
            ImGui::SameLine(0.0f, GAP);
            used += GAP;
        }
        else if(i > 0)
        {
            used = 0.0f;
        }
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        ImGui::Dummy(size);
        const ImVec2 max(pos.x + size.x, pos.y + size.y);
        ImDrawList*  draw = ImGui::GetWindowDrawList();
        draw->AddRectFilled(pos, max, fill, size.y * 0.5f);
        draw->AddRect(pos, max, edge, size.y * 0.5f, ImDrawFlags_None, 1.0f);
        draw->AddText(ImVec2(pos.x + pad.x, pos.y + pad.y), ink, chips[i].c_str());
        used += size.x;
    }
    ImGui::PopFont();
}

// The steps a turn has taken, one chip per tool with a count for repeats.
std::vector<std::string>
StepChips(const std::vector<std::string>& steps)
{
    std::vector<std::string> names;
    std::vector<size_t>      counts;
    for(const std::string& step : steps)
    {
        const std::string label = AssistantToolStepLabel(step);
        const std::vector<std::string>::iterator found =
            std::find(names.begin(), names.end(), label);
        if(found == names.end())
        {
            names.push_back(label);
            counts.push_back(1);
        }
        else
        {
            ++counts[static_cast<size_t>(found - names.begin())];
        }
    }
    std::vector<std::string> chips;
    for(size_t i = 0; i < names.size() && i < ASSISTANT_STEP_CHIPS; ++i)
    {
        chips.push_back(counts[i] > 1 ? names[i] + " x" + std::to_string(counts[i])
                                      : names[i]);
    }
    if(names.size() > ASSISTANT_STEP_CHIPS)
    {
        chips.push_back("+" + std::to_string(names.size() - ASSISTANT_STEP_CHIPS) +
                        " more");
    }
    return chips;
}

std::string
FormatClock(int64_t seconds)
{
    if(seconds < 60)
    {
        return std::to_string(seconds) + "s";
    }
    const int64_t rest = seconds % 60;
    return std::to_string(seconds / 60) + "m " + (rest < 10 ? "0" : "") +
           std::to_string(rest) + "s";
}

std::string
PlainStatus(const std::string& status)
{
    if(status.size() >= 3 && status.compare(status.size() - 3, 3, "...") == 0)
    {
        return status.substr(0, status.size() - 3);
    }
    return status;
}

const char*
ToolDetail(const std::string& tool)
{
    struct Blurb
    {
        const char* name;
        const char* detail;
    };
    static const Blurb blurbs[] = {
        { "track_events",
          "The database returns every event in this window, then Optiq filters, "
          "sorts and pages them. Wide windows on large traces take the longest." },
        { "track_samples",
          "The database returns every counter sample in this window. Counters "
          "sample densely, so long windows take the longest." },
        { "top_events", "Ranking event names by their total time across this scope." },
        { "event_instances",
          "Finding the individual events with this name, with the ids the timeline "
          "needs to select one." },
        { "search_events",
          "Matching event names across the trace, the same as the toolbar search." },
        { "event_details", "Loading this event's arguments, flow links and call stack." },
        { "track_statistics",
          "Measuring how busy this queue was, or a counter's range and mean, over "
          "the window." },
        { "get_summary", "Loading the summary Optiq builds for the trace." },
        { "kernel_triage", "Reading the standard diagnostic tables for this kernel." },
        { "get_metrics", "Reading hardware counter values from the database." },
        { "kernel_pc_samples",
          "Reading where this kernel's waves were when the profiler sampled them." },
    };
    for(const Blurb& blurb : blurbs)
    {
        if(tool == blurb.name)
        {
            return blurb.detail;
        }
    }
    return "Reading from the trace.";
}

ActivityWords
DescribeActivity(bool tool_wait, bool force_final, bool warmup, uint32_t round,
                 const std::string& tool, AssistantFetchKind kind, bool contended,
                 const std::string& status)
{
    ActivityWords words;
    if(!tool_wait)
    {
        if(force_final)
        {
            words.title  = "Writing the answer";
            words.detail =
                "Every read is done. The model is turning the results into the answer.";
        }
        else if(round == 0)
        {
            words.title  = "Thinking";
            words.detail =
                "The model is reading your question and the trace briefing. Nothing is "
                "read from the trace until it picks a tool.";
        }
        else
        {
            words.title  = "Thinking about the results";
            words.detail =
                "The model is weighing what the last read returned and choosing the "
                "next one. The trace is idle while it does.";
        }
        return words;
    }
    if(warmup)
    {
        words.title  = "Loading the summary";
        words.detail =
            "Getting the trace summary ready, so the model starts from real numbers.";
        return words;
    }
    if(kind == AssistantFetchKind::kTraceLoading)
    {
        words.title  = "Waiting for the trace";
        words.detail =
            "The trace is still opening. The read starts as soon as it is ready.";
        return words;
    }
    if(kind == AssistantFetchKind::kScript)
    {
        words.title  = "Waiting for you";
        words.detail =
            "A script is ready in the Script tab. It runs only after you press Run.";
        return words;
    }
    words.title = PlainStatus(status.empty() ? AssistantToolStatusLabel(tool) : status);
    if(contended)
    {
        words.detail =
            "Another read is using the same table. This one starts as soon as it "
            "finishes.";
        return words;
    }
    words.detail = ToolDetail(tool);
    return words;
}

constexpr size_t   ASSISTANT_CHART_MAX_BINS        = 64;
constexpr size_t   ASSISTANT_CHART_MIN_BINS        = 16;
constexpr float    ASSISTANT_CHART_PX_PER_BIN      = 5.0f;
constexpr size_t   ASSISTANT_CHART_ROWS            = 5;
constexpr float    ASSISTANT_CHART_HEIGHT          = 52.0f;
constexpr float    ASSISTANT_CHART_ROW_HEIGHT      = 12.0f;
constexpr float    ASSISTANT_CHART_ROW_GAP         = 2.0f;
constexpr float    ASSISTANT_CHART_MIN_ALPHA       = 0.12f;
// The context pill under the title. Its ring turns to a warning, then an error,
// as the history nears the point where older tool results are shortened.
constexpr float ASSISTANT_CONTEXT_WARN_FILL   = 0.70f;
constexpr float ASSISTANT_CONTEXT_HOT_FILL    = 0.90f;
constexpr float ASSISTANT_CONTEXT_RING_STROKE = 2.0f;
// Smallest arc drawn for a conversation that is not empty, so a short chat
// still reads as started rather than as an empty ring.
constexpr float ASSISTANT_CONTEXT_MIN_ARC    = 0.04f;
constexpr float ASSISTANT_CONTEXT_CHIP_PAD_X = 8.0f;
constexpr float ASSISTANT_CONTEXT_CHIP_PAD_Y = 2.0f;
// Space reserved under the pill. The title's line height leaves room above its
// glyphs that the pill's hard edge does not, so without this the header looks
// heavier at the bottom than at the top.
constexpr float ASSISTANT_CONTEXT_CHIP_BELOW    = 2.5f;
constexpr float ASSISTANT_CONTEXT_CHIP_GAP      = 5.0f;
constexpr float ASSISTANT_CONTEXT_DIVIDER_GAP   = 8.0f;
constexpr float ASSISTANT_CONTEXT_TIP_WIDTH     = 300.0f;
constexpr float ASSISTANT_CONTEXT_TIP_BAR_H     = 6.0f;
constexpr float ASSISTANT_CONTEXT_TIP_NUM_WIDTH = 72.0f;
// imgui.h does not publish IM_PI.
constexpr float ASSISTANT_PI = 3.14159265f;

// Groups digits so a token count can be read at a glance. 12481 -> "12,481".
std::string
FormatCount(size_t value)
{
    const std::string digits = std::to_string(value);
    std::string       out;
    out.reserve(digits.size() + digits.size() / 3);
    for(size_t i = 0; i < digits.size(); ++i)
    {
        if(i > 0 && (digits.size() - i) % 3 == 0)
        {
            out += ',';
        }
        out += digits[i];
    }
    return out;
}

// Compact count for the pill: 386, 2.2k, 45.1k, 120k, 1.2M. Full counts stay in
// the tooltip.
std::string
FormatTokensShort(size_t tokens)
{
    if(tokens < 1000)
    {
        return std::to_string(tokens);
    }
    const char* suffix = "k";
    double      value  = static_cast<double>(tokens) / 1000.0;
    if(tokens >= 999500)
    {
        suffix = "M";
        value  = static_cast<double>(tokens) / 1000000.0;
    }
    char buf[32];
    std::snprintf(buf, sizeof(buf), value >= 100.0 ? "%.0f" : "%.1f", value);
    std::string text = buf;
    if(text.size() > 2 && text.compare(text.size() - 2, 2, ".0") == 0)
    {
        text.resize(text.size() - 2);
    }
    return text + suffix;
}

// How full the kept history is, from 0 to 1.
float
ContextFill(size_t used_chars, size_t budget_chars)
{
    if(budget_chars == 0)
    {
        return 0.0f;
    }
    return std::min(1.0f,
                    static_cast<float>(used_chars) / static_cast<float>(budget_chars));
}

ImU32
ContextFillColor(float fill)
{
    SettingsManager& settings = SettingsManager::GetInstance();
    if(fill >= ASSISTANT_CONTEXT_HOT_FILL)
    {
        return settings.GetColor(Colors::kTextError);
    }
    if(fill >= ASSISTANT_CONTEXT_WARN_FILL)
    {
        return settings.GetColor(Colors::kTextWarning);
    }
    return settings.GetColor(Colors::kAccent);
}

// Percent of the history budget, never showing a started chat as 0%.
std::string
FormatFillPercent(float fill)
{
    if(fill <= 0.0f)
    {
        return "0%";
    }
    const long percent = std::lround(fill * 100.0f);
    return percent < 1 ? std::string("<1%") : std::to_string(percent) + "%";
}

// Text that sits right-aligned in the space left on the current line or cell.
void
RightAlignedText(const std::string& text, ImU32 color)
{
    const float width = ImGui::CalcTextSize(text.c_str()).x;
    const float avail = ImGui::GetContentRegionAvail().x;
    if(avail > width)
    {
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + avail - width);
    }
    ImGui::PushStyleColor(ImGuiCol_Text, color);
    ImGui::TextUnformatted(text.c_str());
    ImGui::PopStyleColor();
}

constexpr const char* ASSISTANT_LINK_POPUP = "##assistant_links";

// Header buttons are compact so the title row is barely taller than the title,
// and the lines under it sit close enough to read as its subtitle. An elided
// header line shows in full in a tooltip this many glyphs wide.
constexpr ImVec2 ASSISTANT_HEADER_BUTTON_PADDING = ImVec2(8.0f, 3.0f);
constexpr float  ASSISTANT_HEADER_LINE_GAP       = 4.0f;
constexpr float  ASSISTANT_HEADER_TOOLTIP_GLYPHS = 32.0f;

// A dim header line starting at x, elided to the card's edge with its full
// text in a tooltip. ElidedText draws into a child window with a fixed name,
// so each line needs its own id.
void
HeaderContextLine(const char* id, const std::string& text, float x)
{
    ImGui::SetCursorPosX(x);
    ImGui::PushID(id);
    ImGui::PushStyleColor(ImGuiCol_Text,
                          SettingsManager::GetInstance().GetColor(Colors::kTextDim));
    ElidedText(text.c_str(), std::max(1.0f, ImGui::GetContentRegionAvail().x),
               ImGui::GetFontSize() * ASSISTANT_HEADER_TOOLTIP_GLYPHS);
    ImGui::PopStyleColor();
    ImGui::PopID();
}

// Breaks a button label onto extra lines so a long follow-up still fits.
std::string
WrapButtonLabel(const std::string& text, float wrap_width)
{
    if(text.empty() || wrap_width <= 1.0f)
    {
        return text;
    }
    if(ImGui::CalcTextSize(text.c_str()).x <= wrap_width)
    {
        return text;
    }

    std::string out;
    std::string line;
    size_t      i = 0;
    while(i < text.size())
    {
        const size_t space = text.find(' ', i);
        const size_t next  = (space == std::string::npos) ? text.size() : space;
        const std::string word = text.substr(i, next - i);
        const std::string candidate = line.empty() ? word : line + " " + word;
        if(!line.empty() && ImGui::CalcTextSize(candidate.c_str()).x > wrap_width)
        {
            if(!out.empty())
            {
                out += "\n";
            }
            out += line;
            line = word;
        }
        else
        {
            line = candidate;
        }
        i = (next == text.size()) ? next : next + 1;
    }
    if(!line.empty())
    {
        if(!out.empty())
        {
            out += "\n";
        }
        out += line;
    }
    return out.empty() ? text : out;
}

std::vector<std::string>
SplitMessageLines(const std::string& text)
{
    std::vector<std::string> lines;
    size_t                   start = 0;
    do
    {
        const size_t end = text.find('\n', start);
        if(end == std::string::npos)
        {
            lines.emplace_back(text, start, text.size() - start);
            break;
        }
        lines.emplace_back(text, start, end - start);
        start = end + 1;
    } while(start <= text.size());
    return lines;
}

// A trace the link menu can offer: open, loaded, and read as one trace.
bool
CanLinkTrace(const AssistantToolContext& context)
{
    return context.data_provider != nullptr && !context.is_compute &&
           !context.is_compare &&
           context.data_provider->GetState() == ProviderState::kReady;
}

bool
CanAskTrace(const AssistantToolContext& context)
{
#ifdef ROCPROFVIS_ENABLE_AGENTIC_COMPUTE
    return context.data_provider != nullptr && !context.is_compare;
#else
    return context.data_provider != nullptr && !context.is_compute && !context.is_compare;
#endif
}

TraceLinkMember
LinkMemberFor(const AssistantToolContext& context)
{
    TraceLinkMember member;
    member.path        = context.project_id;
    member.label       = context.trace_name;
    member.fingerprint = AssistantTraceFingerprint(context);
    return member;
}

void
LinkTraces(const AssistantToolContext& baseline, const AssistantToolContext& target)
{
    NotificationManager& notifications = NotificationManager::GetInstance();
    switch(AssistantTraceLinks().Link(LinkMemberFor(baseline), LinkMemberFor(target)))
    {
        case TraceLinkResult::kLinked:
        {
            notifications.Show("Linked " + target.trace_name + " to its baseline " +
                                   baseline.trace_name,
                               NotificationLevel::Success);
            break;
        }
        case TraceLinkResult::kLinkedNotSaved:
        {
            notifications.Show(
                "Linked for this session only: the link could not be saved",
                NotificationLevel::Warning);
            break;
        }
        case TraceLinkResult::kAlreadyLinked:
        {
            notifications.Show("Those traces are already linked",
                               NotificationLevel::Info);
            break;
        }
        case TraceLinkResult::kInvalid:
        {
            notifications.Show("A trace cannot be linked to itself",
                               NotificationLevel::Warning);
            break;
        }
    }
}

}  // namespace

void
AssistantPanel::RenderActivityChart(const std::string& project_id, uint64_t track_id)
{
    const AssistantToolContext context = MakeAssistantToolContext(project_id);
    if(context.data_provider == nullptr)
    {
        return;
    }

    // Bin count follows the column width, so bars stay legible when the dock is
    // dragged narrow.
    const float  avail_width = ImGui::GetContentRegionAvail().x;
    const size_t bin_count   = static_cast<size_t>(
        std::clamp(avail_width / ASSISTANT_CHART_PX_PER_BIN,
                     static_cast<float>(ASSISTANT_CHART_MIN_BINS),
                     static_cast<float>(ASSISTANT_CHART_MAX_BINS)));

    const std::vector<double> bins =
        GetAssistantActivityBins(context, track_id, bin_count);
    if(bins.empty())
    {
        return;
    }

    SettingsManager&  settings = SettingsManager::GetInstance();
    const ImGuiStyle& style    = settings.GetDefaultStyle();
    const ImU32       bg       = settings.GetColor(Colors::kBgFrame);
    const ImU32       bar      = settings.GetColor(Colors::kLineChartColor);
    const ImU32       border   = settings.GetColor(Colors::kBorderColor);
    const ImVec4      accent =
        ImGui::ColorConvertU32ToFloat4(settings.GetColor(Colors::kAccent));

    ImDrawList*  draw  = ImGui::GetWindowDrawList();
    const float  width = ImGui::GetContentRegionAvail().x;
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const float  bar_width = width / static_cast<float>(bins.size());

    draw->AddRectFilled(origin, ImVec2(origin.x + width, origin.y + ASSISTANT_CHART_HEIGHT),
                        bg);
    for(size_t i = 0; i < bins.size(); ++i)
    {
        const float height =
            static_cast<float>(bins[i]) * (ASSISTANT_CHART_HEIGHT - 2.0f);
        if(height <= 0.0f)
        {
            continue;
        }
        const float x0 = origin.x + bar_width * static_cast<float>(i);
        const float y1 = origin.y + ASSISTANT_CHART_HEIGHT - 1.0f;
        draw->AddRectFilled(ImVec2(x0, y1 - height),
                            ImVec2(x0 + std::max(1.0f, bar_width - 1.0f), y1), bar);
    }
    draw->AddRect(origin, ImVec2(origin.x + width, origin.y + ASSISTANT_CHART_HEIGHT),
                  border);
    ImGui::Dummy(ImVec2(width, ASSISTANT_CHART_HEIGHT));

    if(track_id != INVALID_UINT64_INDEX)
    {
        ImGui::TextDisabled("Track %llu", static_cast<unsigned long long>(track_id));
        return;
    }

    const std::vector<AssistantActivityRow> rows =
        GetAssistantActivityRows(context, bin_count, ASSISTANT_CHART_ROWS);
    if(rows.empty())
    {
        return;
    }

    ImGui::Spacing();

    // Size the gutter to the widest id present; a fixed width would eat a chunk
    // of a narrow column.
    float label_width = 0.0f;
    for(const AssistantActivityRow& row : rows)
    {
        char measure[32];
        std::snprintf(measure, sizeof(measure), "t%llu",
                      static_cast<unsigned long long>(row.track_id));
        label_width = std::max(label_width, ImGui::CalcTextSize(measure).x);
    }
    label_width += style.ItemInnerSpacing.x * 2.0f;

    const float strip_width = std::max(1.0f, width - label_width);
    const ImU32 label_color = settings.GetColor(Colors::kTextDim);

    // Rows butt up against each other so they read as one strip; default item
    // spacing would band them.
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                        ImVec2(0.0f, ASSISTANT_CHART_ROW_GAP));
    for(const AssistantActivityRow& row : rows)
    {
        if(row.bins.empty())
        {
            continue;
        }

        const ImVec2 row_origin = ImGui::GetCursorScreenPos();
        const float  row_height =
            std::max(ASSISTANT_CHART_ROW_HEIGHT, ImGui::GetTextLineHeight());

        ImGui::PushID(static_cast<int>(row.track_id));
        ImGui::InvisibleButton("##minimap_row", ImVec2(width, row_height));
        ImGui::PopID();
        if(ImGui::IsItemHovered())
        {
            SetTooltipStyled("%s", row.name.c_str());
        }

        char label[32];
        std::snprintf(label, sizeof(label), "t%llu",
                      static_cast<unsigned long long>(row.track_id));
        draw->AddText(row_origin, label_color, label);

        // Fill the lane first, so idle stretches read as gaps rather than holes.
        const float x_start = row_origin.x + label_width;
        draw->AddRectFilled(ImVec2(x_start, row_origin.y),
                            ImVec2(x_start + strip_width, row_origin.y + row_height),
                            bg);

        const float cell_width = strip_width / static_cast<float>(row.bins.size());
        for(size_t i = 0; i < row.bins.size(); ++i)
        {
            const float value = static_cast<float>(row.bins[i]);
            if(value <= 0.0f)
            {
                continue;
            }
            ImVec4 cell = accent;
            cell.w = ASSISTANT_CHART_MIN_ALPHA + value * (1.0f - ASSISTANT_CHART_MIN_ALPHA);
            const float x0 = x_start + cell_width * static_cast<float>(i);
            draw->AddRectFilled(ImVec2(x0, row_origin.y),
                                ImVec2(x0 + std::max(1.0f, cell_width),
                                       row_origin.y + row_height),
                                ImGui::ColorConvertFloat4ToU32(cell));
        }
    }
    ImGui::PopStyleVar();

    ImGui::TextDisabled("Busiest tracks. Hover for names.");
}

void
AssistantPanel::RenderToolbarButton()
{
    SettingsManager& settings = SettingsManager::GetInstance();
    ImGui::PushStyleColor(ImGuiCol_Button,
                          ImGui::ColorConvertU32ToFloat4(settings.GetColor(Colors::kBgFrame)));
    ImGui::PushStyleColor(
        ImGuiCol_ButtonHovered,
        ImGui::ColorConvertU32ToFloat4(settings.GetColor(Colors::kButtonHovered)));
    ImGui::PushStyleColor(
        ImGuiCol_ButtonActive,
        ImGui::ColorConvertU32ToFloat4(settings.GetColor(Colors::kButtonActive)));
    ImGui::PushStyleColor(ImGuiCol_Text,
                          ImGui::ColorConvertU32ToFloat4(settings.GetColor(Colors::kTextMain)));
    if(ImGui::Button("Ask Optiq"))
    {
        GetInstance()->ToggleVisible();
    }
    ImGui::PopStyleColor(4);
    if(ImGui::IsItemHovered())
    {
        SetTooltipStyled("Open the assistant. Configure the URL and key in "
                         "Edit > Preferences > Assistant.");
    }
}

// Kept for the RocWidget contract; AppWindow draws the panel through RenderDocked.
void
AssistantPanel::Render()
{
    RenderDocked();
}

float
AssistantPanel::DockedWidth() const
{
    return m_visible ? m_dock_width + ASSISTANT_SPLITTER_WIDTH : 0.0f;
}

// Drag handle between the main view and the panel, matching the topology
// sidebar on the other side of the window.
void
AssistantPanel::RenderSplitter()
{
    SettingsManager& settings = SettingsManager::GetInstance();
    const ImVec2     origin   = ImGui::GetCursorScreenPos();
    const float      height   = ImGui::GetContentRegionAvail().y;

    ImGui::InvisibleButton("##assistant_splitter",
                           ImVec2(ASSISTANT_SPLITTER_WIDTH, height));
    const bool hovered = ImGui::IsItemHovered();
    const bool active  = ImGui::IsItemActive();
    if(hovered || active)
    {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
    }
    if(active)
    {
        // Dragging left widens the panel, so the delta is negated.
        m_dock_width = std::clamp(m_dock_width - ImGui::GetIO().MouseDelta.x,
                                  ASSISTANT_MIN_WIDTH, ASSISTANT_MAX_WIDTH);
    }

    ImGui::GetWindowDrawList()->AddRectFilled(
        origin, ImVec2(origin.x + ASSISTANT_SPLITTER_WIDTH, origin.y + height),
        settings.GetColor(hovered || active ? Colors::kAccent
                                            : Colors::kSplitterColor));
}

void
AssistantPanel::RenderDocked()
{
    if(!m_visible)
    {
        return;
    }

    // The tab bar has drawn by now, so this is the trace the user is looking at
    // this frame rather than the one Update() saw.
    m_view_project_id = CurrentProjectId();

    // A width persisted below the current minimum would otherwise stick until
    // the splitter was dragged.
    m_dock_width = std::max(m_dock_width, ASSISTANT_MIN_WIDTH);

    SettingsManager& settings = SettingsManager::GetInstance();

    RenderSplitter();
    ImGui::SameLine(0.0f, 0.0f);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ASSISTANT_WINDOW_PADDING);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10.0f, 10.0f));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, settings.GetColor(Colors::kBgMain));
    // Only the transcript scrolls; the header and composer are pinned. ImGui
    // drops WindowPadding on a borderless child unless asked to keep it.
    ImGui::BeginChild("##assistant_dock", ImVec2(m_dock_width, 0.0f),
                      ImGuiChildFlags_AlwaysUseWindowPadding,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    RenderHeaderCard();
    RenderTranscript();
    RenderActivityCard();
    RenderComposer();

    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
}

void
AssistantPanel::RenderHeaderCard()
{
    SettingsManager&  settings = SettingsManager::GetInstance();
    const ImGuiStyle& style    = settings.GetDefaultStyle();

    BeginPanelCard("##assistant_header", PanelCardTone::kFrame, ASSISTANT_CARD_PADDING,
                   true, &settings);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                        ImVec2(style.ItemSpacing.x, ASSISTANT_HEADER_LINE_GAP));

    const AssistantProvider* provider = settings.GetActiveAssistantProvider();
    const bool configured = provider != nullptr && !provider->endpoint_url.empty();
    // An empty Model field sends the default, so name what is actually sent.
    const std::string model = !configured ? std::string()
                              : provider->model.empty()
                                  ? std::string(ASSISTANT_DEFAULT_MODEL)
                                  : provider->model;

    const AssistantToolContext view     = MakeAssistantToolContext(m_view_project_id);
    const bool                 can_link = CanLinkTrace(view);

    // Top row: identity on the left, link and close at the right edge.
    // Aligning to the frame padding centers the icon and title on the buttons.
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ASSISTANT_HEADER_BUTTON_PADDING);
    ImGui::AlignTextToFramePadding();
    PanelIcon(ICON_COMPASS, Colors::kAccent, &settings);
    ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
    const float title_x = ImGui::GetCursorPosX();
    ImGui::PushFont(nullptr, settings.GetFontManager().GetFontSize(FontSize::kMedLarge));
    ImGui::TextUnformatted("Ask Optiq");
    ImGui::PopFont();
    const bool title_hovered = ImGui::IsItemHovered();

    const float link_width =
        ImGui::CalcTextSize("Link runs").x + 2.0f * ASSISTANT_HEADER_BUTTON_PADDING.x;
    ImGui::PushFont(settings.GetFontManager().GetFont(FontType::kIcon), 0.0f);
    const float close_width = ImGui::CalcTextSize(ICON_X_CIRCLED).x;
    ImGui::PopFont();
    ImGui::SameLine(0.0f, 0.0f);
    const float leftover = ImGui::GetContentRegionAvail().x - link_width -
                           style.ItemInnerSpacing.x - close_width;
    if(leftover > 0.0f)
    {
        ImGui::Dummy(ImVec2(leftover, 0.0f));
        ImGui::SameLine(0.0f, 0.0f);
    }
    ImGui::BeginDisabled(!can_link);
    if(ImGui::Button("Link runs", ImVec2(link_width, 0.0f)))
    {
        ImGui::OpenPopup(ASSISTANT_LINK_POPUP);
    }
    ImGui::EndDisabled();
    if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
    {
        SetTooltipStyled(can_link ? "Link this trace to an earlier or later run"
                                  : "Open a loaded system trace to link runs");
    }
    ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
    if(XButton("##assistant_close", "Close the assistant", &settings))
    {
        m_visible = false;
    }
    ImGui::PopStyleVar();
    RenderLinkMenu();

    if(title_hovered)
    {
        if(!configured)
        {
            SetTooltipStyled("Not configured. Edit > Preferences > Assistant.");
        }
        else
        {
            SetTooltipStyled("%s\n%s", model.c_str(), provider->endpoint_url.c_str());
        }
    }

    // The model and context use live in the pill; trace identity and links stay
    // on their own lines because both are per-tab.
    if(configured)
    {
        RenderContextChip(model, provider->endpoint_url);
    }
    if(!view.trace_name.empty())
    {
        HeaderContextLine("trace", view.trace_name, title_x);
    }

    const std::vector<TraceLink> links = AssistantTraceLinks().LinksOf(m_view_project_id);
    if(!links.empty())
    {
        std::string linked = "Linked: ";
        for(size_t i = 0; i < links.size(); ++i)
        {
            linked += (i == 0 ? "" : ", ") + links[i].other.label + " (" +
                      links[i].other.role + ")";
        }
        HeaderContextLine("linked", linked, title_x);
    }
    ImGui::PopStyleVar();

    if(!configured)
    {
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, settings.GetColor(Colors::kTextWarning));
        ImGui::TextWrapped("Set the URL, model, and key in Edit > Preferences > Assistant.");
        ImGui::PopStyleColor();
    }

    EndPanelCard();
}

// Links the trace on screen to another open one as baseline and target, or
// removes a link it already has.
void
AssistantPanel::RenderLinkMenu()
{
    PushContextMenuStyles();
    if(ImGui::BeginPopup(ASSISTANT_LINK_POPUP))
    {
        const AssistantToolContext view = MakeAssistantToolContext(m_view_project_id);
        if(!CanLinkTrace(view))
        {
            ImGui::TextDisabled("Open a system trace to link it to another run.");
        }
        else
        {
            TraceLinkStore&              store = AssistantTraceLinks();
            const std::vector<TraceLink> links = store.LinksOf(m_view_project_id);
            if(!links.empty())
            {
                ImGui::TextDisabled("Linked runs");
                for(const TraceLink& link : links)
                {
                    const std::string label =
                        "Unlink " + link.other.label + " (" + link.other.role + ")";
                    ImGui::PushID(link.group_id.c_str());
                    if(ImGui::MenuItem(label.c_str()))
                    {
                        const TraceUnlinkResult result = store.Unlink(link.group_id);
                        if(result == TraceUnlinkResult::kUnlinkedNotSaved)
                        {
                            NotificationManager::GetInstance().Show(
                                "Unlinked for this session only: the change could "
                                "not be saved",
                                NotificationLevel::Warning);
                        }
                    }
                    ImGui::PopID();
                }
                ImGui::Separator();
            }

            ImGui::TextDisabled("Link another open trace");
            bool       offered = false;
            AppWindow* app     = AppWindow::GetInstance();
            if(app != nullptr && app->GetTabContainer())
            {
                for(const TabItem* tab : app->GetTabContainer()->GetTabs())
                {
                    if(tab == nullptr || tab->m_id == m_view_project_id)
                    {
                        continue;
                    }
                    const AssistantToolContext other =
                        MakeAssistantToolContext(tab->m_id);
                    if(!CanLinkTrace(other))
                    {
                        continue;
                    }
                    offered = true;
                    ImGui::PushID(tab->m_id.c_str());
                    if(ImGui::BeginMenu(other.trace_name.c_str()))
                    {
                        if(ImGui::MenuItem("It is the earlier run (baseline)"))
                        {
                            LinkTraces(other, view);
                        }
                        if(ImGui::MenuItem("It is the later run (target)"))
                        {
                            LinkTraces(view, other);
                        }
                        ImGui::EndMenu();
                    }
                    ImGui::PopID();
                }
            }
            if(!offered)
            {
                ImGui::TextDisabled("No other system trace is open.");
            }
        }
        ImGui::EndPopup();
    }
    PopContextMenuStyles();
}

/*
 * The pill under the title: the model, a ring for how full the kept history is,
 * the conversation's size against that budget, and - once the endpoint has
 * reported any - the tokens this conversation has used.
 */
void
AssistantPanel::RenderContextChip(const std::string& model, const std::string& endpoint)
{
    Conversation&     conversation = ViewConversation();
    const ContextLoad load         = MeasureContext();
    const float       fill = ContextFill(load.history_chars, load.history_budget_chars);

    SettingsManager& settings = SettingsManager::GetInstance();
    const ImU32      main     = settings.GetColor(Colors::kTextMain);
    const ImU32      dim      = settings.GetColor(Colors::kTextDim);

    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const float  avail  = ImGui::GetContentRegionAvail().x;

    ImGui::PushFont(nullptr, settings.GetFontManager().GetFontSize(FontSize::kSmall));
    const float text_h   = ImGui::GetTextLineHeight();
    const float chip_h   = text_h + ASSISTANT_CONTEXT_CHIP_PAD_Y * 2.0f;
    const float ring_d   = std::max(8.0f, text_h - 3.0f);
    const float word_gap = ImGui::CalcTextSize(" ").x * 2.0f;

    const std::string used   = FormatTokensShort(load.history_tokens);
    const std::string budget = " / " + FormatTokensShort(load.history_budget_tokens);
    const std::string in_n =
        FormatTokensShort(static_cast<size_t>(conversation.session_prompt_tokens));
    const std::string out_n =
        FormatTokensShort(static_cast<size_t>(conversation.session_completion_tokens));
    const char* in_label  = " in";
    const char* out_label = " out";

    const float divider_w = ASSISTANT_CONTEXT_DIVIDER_GAP * 2.0f + 1.0f;
    const float context_w =
        ring_d + ASSISTANT_CONTEXT_CHIP_GAP + ImGui::CalcTextSize(used.c_str()).x;
    const float budget_w = ImGui::CalcTextSize(budget.c_str()).x;
    const float usage_w  = divider_w + ImGui::CalcTextSize(in_n.c_str()).x +
                          ImGui::CalcTextSize(in_label).x + word_gap +
                          ImGui::CalcTextSize(out_n.c_str()).x +
                          ImGui::CalcTextSize(out_label).x;
    const float pads = ASSISTANT_CONTEXT_CHIP_PAD_X * 2.0f;

    // A narrow dock drops the usage first, then the budget, then shortens the
    // model name. The ring and the conversation size always stay.
    std::string label       = model;
    bool        show_usage  = conversation.usage_reported;
    bool        show_budget = true;
    auto        total       = [&]() {
        return pads + ImGui::CalcTextSize(label.c_str()).x + divider_w + context_w +
               (show_budget ? budget_w : 0.0f) + (show_usage ? usage_w : 0.0f);
    };
    if(show_usage && total() > avail)
    {
        show_usage = false;
    }
    if(total() > avail)
    {
        show_budget = false;
    }
    if(total() > avail)
    {
        label = ElideWithEllipsis(model,
                                  std::max(0.0f, avail - (pads + divider_w + context_w)));
    }
    const float width = total();

    ImGui::InvisibleButton("##assistant_context_chip",
                           ImVec2(width, chip_h + ASSISTANT_CONTEXT_CHIP_BELOW));
    const bool hovered = ImGui::IsItemHovered();

    ImDrawList*  draw     = ImGui::GetWindowDrawList();
    const float  center_y = origin.y + chip_h * 0.5f;
    const ImVec2 chip_min(origin);
    const ImVec2 chip_max(origin.x + width, origin.y + chip_h);
    // Hover changes only the outline: the ring's track and the dividers share
    // the fill's tone, so a tinted fill would hide them.
    draw->AddRectFilled(chip_min, chip_max, settings.GetColor(Colors::kBgPanel),
                        chip_h * 0.5f);
    draw->AddRect(
        chip_min, chip_max,
        settings.GetColor(hovered ? Colors::kBorderColor : Colors::kPanelBorderSubtle),
        chip_h * 0.5f);

    const float text_y  = center_y - text_h * 0.5f;
    const ImU32 divider = settings.GetColor(Colors::kBorderColor);
    float       x       = origin.x + ASSISTANT_CONTEXT_CHIP_PAD_X;
    auto        run     = [&](const char* text, ImU32 color) {
        draw->AddText(ImVec2(x, text_y), color, text);
        x += ImGui::CalcTextSize(text).x;
    };
    auto rule = [&]() {
        x += ASSISTANT_CONTEXT_DIVIDER_GAP;
        draw->AddLine(ImVec2(x, center_y - text_h * 0.35f),
                      ImVec2(x, center_y + text_h * 0.35f), divider);
        x += 1.0f + ASSISTANT_CONTEXT_DIVIDER_GAP;
    };

    run(label.c_str(), main);
    rule();

    const ImVec2 ring_center(x + ring_d * 0.5f, center_y);
    const float  ring_r = ring_d * 0.5f - ASSISTANT_CONTEXT_RING_STROKE * 0.5f;
    draw->AddCircle(ring_center, ring_r, settings.GetColor(Colors::kBorderColor), 0,
                    ASSISTANT_CONTEXT_RING_STROKE);
    if(fill >= 0.995f)
    {
        draw->AddCircle(ring_center, ring_r, ContextFillColor(fill), 0,
                        ASSISTANT_CONTEXT_RING_STROKE);
    }
    else if(load.history_chars > 0)
    {
        // Zero radians points right, so start at the top and sweep clockwise.
        const float start = -ASSISTANT_PI * 0.5f;
        const float sweep =
            std::max(fill, ASSISTANT_CONTEXT_MIN_ARC) * ASSISTANT_PI * 2.0f;
        draw->PathArcTo(ring_center, ring_r, start, start + sweep, 0);
        draw->PathStroke(ContextFillColor(fill), ASSISTANT_CONTEXT_RING_STROKE);
    }
    x += ring_d + ASSISTANT_CONTEXT_CHIP_GAP;

    run(used.c_str(), load.history_chars > 0 ? main : dim);
    if(show_budget)
    {
        run(budget.c_str(), dim);
    }
    if(show_usage)
    {
        rule();
        run(in_n.c_str(), main);
        run(in_label, dim);
        x += word_gap;
        run(out_n.c_str(), main);
        run(out_label, dim);
    }
    ImGui::PopFont();

    if(hovered)
    {
        BeginTooltipStyled();
        RenderContextTooltip(conversation, load, fill, model, endpoint);
        EndTooltipStyled();
    }
}

// The pill's hover card: which endpoint answers, the history against its
// budget, what rides along on every request, and the reported token counts.
void
AssistantPanel::RenderContextTooltip(const Conversation& conversation,
                                     const ContextLoad& load, float fill,
                                     const std::string& model,
                                     const std::string& endpoint) const
{
    SettingsManager& settings = SettingsManager::GetInstance();
    const ImU32      main     = settings.GetColor(Colors::kTextMain);
    const ImU32      dim      = settings.GetColor(Colors::kTextDim);
    const float      width    = ASSISTANT_CONTEXT_TIP_WIDTH;
    // A tooltip sizes itself to its content, so alignment is measured against
    // this fixed width rather than against the space the window reports.
    const float start_x = ImGui::GetCursorPosX();

    ImGui::PushTextWrapPos(start_x + width);

    ImGui::TextUnformatted(model.c_str());
    ImGui::PushStyleColor(ImGuiCol_Text, dim);
    ImGui::TextWrapped("%s", endpoint.c_str());
    ImGui::PopStyleColor();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    const std::string percent = FormatFillPercent(fill) + " used";
    ImGui::TextUnformatted("Context");
    ImGui::SameLine(start_x + width - ImGui::CalcTextSize(percent.c_str()).x);
    ImGui::PushStyleColor(ImGuiCol_Text, dim);
    ImGui::TextUnformatted(percent.c_str());
    ImGui::PopStyleColor();

    ImDrawList*  draw   = ImGui::GetWindowDrawList();
    const ImVec2 bar    = ImGui::GetCursorScreenPos();
    const float  radius = ASSISTANT_CONTEXT_TIP_BAR_H * 0.5f;
    draw->AddRectFilled(bar, ImVec2(bar.x + width, bar.y + ASSISTANT_CONTEXT_TIP_BAR_H),
                        settings.GetColor(Colors::kBorderColor), radius);
    if(load.history_chars > 0)
    {
        const float filled =
            std::min(width, std::max(ASSISTANT_CONTEXT_TIP_BAR_H, width * fill));
        draw->AddRectFilled(bar,
                            ImVec2(bar.x + filled, bar.y + ASSISTANT_CONTEXT_TIP_BAR_H),
                            ContextFillColor(fill), radius);
    }
    ImGui::Dummy(ImVec2(width, ASSISTANT_CONTEXT_TIP_BAR_H));

    const std::string about =
        FormatCount(load.history_tokens) + " of " +
        FormatCount(load.history_budget_tokens) +
        " tokens of conversation. Past that, Optiq shortens older tool results. The "
        "prompt and tool list add about " +
        FormatCount(load.overhead_tokens) + " to every request.";
    ImGui::PushStyleColor(ImGuiCol_Text, dim);
    ImGui::TextWrapped("%s", about.c_str());
    ImGui::PopStyleColor();

    // Stays in the context block. The usage table below is what the endpoint
    // reported, and the disclaimer does not cover it.
    ImGui::Spacing();
    ImGui::PushFont(nullptr, settings.GetFontManager().GetFontSize(FontSize::kSmall));
    ImGui::PushStyleColor(ImGuiCol_Text, dim);
    ImGui::TextWrapped("Context figures are estimates. Actual values may vary.");
    ImGui::PopStyleColor();
    ImGui::PopFont();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::TextUnformatted("Token usage");
    if(!conversation.usage_reported)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, dim);
        ImGui::TextWrapped("%s", conversation.messages.empty()
                                     ? "Counts appear after the first reply."
                                     : "This endpoint did not report token counts.");
        ImGui::PopStyleColor();
        ImGui::PopTextWrapPos();
        return;
    }

    if(ImGui::BeginTable("##assistant_usage", 3, ImGuiTableFlags_None,
                         ImVec2(width, 0.0f)))
    {
        ImGui::TableSetupColumn("##what", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("##in", ImGuiTableColumnFlags_WidthFixed,
                                ASSISTANT_CONTEXT_TIP_NUM_WIDTH);
        ImGui::TableSetupColumn("##out", ImGuiTableColumnFlags_WidthFixed,
                                ASSISTANT_CONTEXT_TIP_NUM_WIDTH);

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(1);
        RightAlignedText("In", dim);
        ImGui::TableSetColumnIndex(2);
        RightAlignedText("Out", dim);

        auto row = [&](const char* label, int in_tokens, int out_tokens) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::PushStyleColor(ImGuiCol_Text, dim);
            ImGui::TextUnformatted(label);
            ImGui::PopStyleColor();
            ImGui::TableSetColumnIndex(1);
            RightAlignedText(FormatCount(static_cast<size_t>(in_tokens)), main);
            ImGui::TableSetColumnIndex(2);
            RightAlignedText(FormatCount(static_cast<size_t>(out_tokens)), main);
        };
        row("Last reply", conversation.last_prompt_tokens,
            conversation.last_completion_tokens);
        row("This conversation", conversation.session_prompt_tokens,
            conversation.session_completion_tokens);
        ImGui::EndTable();
    }

    std::string note = "Over " + std::to_string(conversation.session_rounds) +
                       (conversation.session_rounds == 1 ? " request" : " requests") +
                       ". Input counts the whole history again on every request.";
    if(conversation.last_cached_known && conversation.last_cached_tokens > 0)
    {
        note += " " + FormatCount(static_cast<size_t>(conversation.last_cached_tokens)) +
                " of the last reply's input came from the endpoint's cache.";
    }
    ImGui::PushStyleColor(ImGuiCol_Text, dim);
    ImGui::TextWrapped("%s", note.c_str());
    ImGui::PopStyleColor();

    ImGui::PopTextWrapPos();
}

void
AssistantPanel::RenderActivityCard()
{
    // Only under the conversation the turn is writing to.
    if(!Busy() || m_status.empty() || m_turn_project_id != m_view_project_id)
    {
        m_activity_height = 0.0f;
        return;
    }

    SettingsManager& settings = SettingsManager::GetInstance();
    const float      start_y  = ImGui::GetCursorPosY();

    const bool tool_wait = m_phase == Phase::kToolWait;
    const std::chrono::steady_clock::time_point started =
        (tool_wait && m_fetch_wait.active) ? m_fetch_wait.started : m_activity_started;
    const int64_t raw_elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                                    std::chrono::steady_clock::now() - started)
                                    .count();
    const int64_t elapsed = raw_elapsed < 0 ? 0 : raw_elapsed;

    // A read that found its table taken by another query, as opposed to one
    // whose own query is running. Summary and PC-sample reads park this way on
    // purpose, so they are not counted.
    const AssistantFetchKind kind      = m_fetch_wait.fetch.kind;
    const bool               holding   = kind == AssistantFetchKind::kScript ||
                           kind == AssistantFetchKind::kTraceLoading;
    const bool               contended = tool_wait && !m_fetch_wait.warmup &&
                             !m_fetch_wait.started_fetch &&
                             (kind == AssistantFetchKind::kTopEvents ||
                              kind == AssistantFetchKind::kEventInstances ||
                              kind == AssistantFetchKind::kDataTable ||
                              kind == AssistantFetchKind::kEventDetails ||
                              kind == AssistantFetchKind::kTrackStatistics ||
                              kind == AssistantFetchKind::kComputeMetrics);

    AssistantMotion motion = AssistantMotion::kThink;
    if(tool_wait)
    {
        motion = (holding || contended) ? AssistantMotion::kWait : AssistantMotion::kRead;
    }
    else if(m_force_final)
    {
        motion = AssistantMotion::kWrite;
    }

    const float dt     = std::clamp(ImGui::GetIO().DeltaTime, 0.0f, 0.1f);
    const float target = motion == AssistantMotion::kRead ? ASSISTANT_READ_SPEED : 0.0f;
    const float ease   = 1.0f - std::exp(-dt * ASSISTANT_SPEED_EASE);
    m_stream_speed += (target - m_stream_speed) * ease;
    m_stream_offset += static_cast<double>(m_stream_speed * dt);

    const ActivityWords words =
        DescribeActivity(tool_wait, m_force_final, m_fetch_wait.warmup, m_tool_round,
                         m_fetch_wait.tool_name, kind, contended, m_status);
    const bool show_scope = tool_wait && !holding && !m_fetch_wait.warmup;
    const std::vector<std::string> chips =
        show_scope ? m_activity_scope : StepChips(m_turn_steps);
    const ImU32 accent = settings.GetColor(Colors::kAccent);

    BeginPanelCard("##assistant_activity", PanelCardTone::kFrame, ASSISTANT_CARD_PADDING,
                   true, &settings);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 7.0f));
    RenderActivityStage(motion, std::max(240.0f, ImGui::GetContentRegionAvail().x),
                        m_stream_offset, MakeStagePalette(settings));

    // The clock is drawn rather than laid out, so it sits on the title's row
    // without changing that row's height.
    const ImVec2 row   = ImGui::GetCursorScreenPos();
    const float  row_w = ImGui::GetContentRegionAvail().x;
    ImGui::PushFont(nullptr, settings.GetFontManager().GetFontSize(FontSize::kMedLarge));
    const float title_h = ImGui::GetTextLineHeight();
    RenderShimmerText(words.title, settings.GetColor(Colors::kTextMain), accent);
    ImGui::PopFont();
    if(elapsed >= ASSISTANT_ACTIVITY_CLOCK_SECONDS)
    {
        const std::string clock = FormatClock(elapsed);
        const ImVec2      size  = ImGui::CalcTextSize(clock.c_str());
        ImGui::GetWindowDrawList()->AddText(
            ImVec2(row.x + row_w - size.x, row.y + (title_h - size.y) * 0.5f),
            settings.GetColor(Colors::kTextDim), clock.c_str());
    }
    RenderChips(chips, show_scope, settings);
    ImGui::PushStyleColor(ImGuiCol_Text, settings.GetColor(Colors::kTextDim));
    ImGui::TextWrapped("%s", words.detail);
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    EndPanelCard();

    // EndChild already advanced the cursor past one ItemSpacing, the same gap
    // the transcript has to leave above the card.
    m_activity_height = ImGui::GetCursorPosY() - start_y;
}

void
AssistantPanel::RenderTranscript()
{
    SettingsManager&  settings     = SettingsManager::GetInstance();
    const ImGuiStyle& style        = settings.GetDefaultStyle();
    Conversation&     conversation = ViewConversation();

    // Last frame's measured heights; the estimates are only for frame one.
    const float composer_height =
        m_composer_height > 0.0f
            ? m_composer_height
            : ImGui::GetFrameHeight() + style.ItemSpacing.y * 2.0f +
                  ASSISTANT_CARD_PADDING.y * 2.0f;
    const bool show_activity =
        Busy() && !m_status.empty() && m_turn_project_id == m_view_project_id;
    const float activity_height =
        !show_activity             ? 0.0f
        : m_activity_height > 0.0f ? m_activity_height
                                   : ASSISTANT_ACTIVITY_RESERVE;

    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, PANEL_CARD_ROUNDING);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ASSISTANT_CARD_PADDING);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10.0f, 12.0f));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, settings.GetColor(Colors::kBgPanel));
    ImGui::PushStyleColor(ImGuiCol_Border, settings.GetColor(Colors::kPanelBorderSubtle));
    ImGui::BeginChild("assistant_history",
                      ImVec2(0.0f, -(composer_height + activity_height)),
                      ImGuiChildFlags_Borders);

    if(conversation.lines.empty())
    {
        ImGui::PushStyleColor(ImGuiCol_Text, settings.GetColor(Colors::kTextMain));
        ImGui::TextWrapped("Ask about this trace, or press Explain this view below.");
        ImGui::PopStyleColor();
        ImGui::PushStyleColor(ImGuiCol_Text, settings.GetColor(Colors::kTextDim));
        ImGui::TextWrapped(
            "I read the timeline overview first, then dig into whatever looks worst.");
        ImGui::PopStyleColor();
    }

    for(size_t i = 0; i < conversation.lines.size(); ++i)
    {
        RenderMessageCard(conversation, i, conversation.lines[i]);
    }

    if(conversation.scroll_to_bottom)
    {
        ImGui::SetScrollHereY(1.0f);
        conversation.scroll_to_bottom = false;
    }
    ImGui::EndChild();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(3);
}

void
AssistantPanel::RenderMessageCard(Conversation& conversation, size_t index,
                                  const ChatLine& line)
{
    SettingsManager&  settings = SettingsManager::GetInstance();
    const ImGuiStyle& style    = settings.GetDefaultStyle();

    ImGui::PushID(static_cast<int>(index));

    if(line.speaker == Speaker::kStatus)
    {
        // A notice that outlived its turn, such as a failed request.
        PanelIcon(ICON_X_CIRCLED, Colors::kTextError, &settings);
        ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
        RenderSelectableMessage(conversation, index, line.text, Colors::kTextError);
        ImGui::PopID();
        return;
    }

    if(line.speaker == Speaker::kChart)
    {
        const std::string chart_project =
            line.project_id.empty() ? m_view_project_id : line.project_id;
        std::string title = "Timeline overview";
        if(chart_project != m_view_project_id)
        {
            const AssistantToolContext source = MakeAssistantToolContext(chart_project);
            if(!source.trace_name.empty())
            {
                title += " of " + source.trace_name;
            }
        }
        BeginPanelCard("##chart_card", PanelCardTone::kPanel, ASSISTANT_CARD_PADDING,
                       true, &settings);
        PanelIcon(ICON_CHART_BAR, Colors::kAccent, &settings);
        ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
        PanelFieldLabel(title.c_str(), false, &settings);
        RenderActivityChart(chart_project, line.track_id);
        EndPanelCard();
        ImGui::PopID();
        return;
    }

    const bool user = line.speaker == Speaker::kUser;
    BeginPanelCard("##message_card",
                   user ? PanelCardTone::kFrame : PanelCardTone::kPanel,
                   ASSISTANT_CARD_PADDING, true, &settings);
    if(user)
    {
        PanelFieldLabel("You", false, &settings);
    }
    else
    {
        PanelIcon(ICON_COMPASS, Colors::kAccent, &settings);
        ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
        PanelFieldLabel("Optiq", false, &settings);
    }
    ImGui::Spacing();
    RenderSelectableMessage(conversation, index, line.text, Colors::kTextMain);
    EndPanelCard();
    ImGui::PopID();
}

TextSelect*
AssistantPanel::MessageSelect(Conversation& conversation, size_t index)
{
    if(index >= conversation.lines.size())
    {
        return nullptr;
    }
    if(conversation.message_select.size() < conversation.lines.size())
    {
        conversation.message_select.resize(conversation.lines.size());
    }
    if(conversation.message_lines.size() < conversation.lines.size())
    {
        conversation.message_lines.resize(conversation.lines.size());
    }
    if(conversation.message_select[index] == nullptr)
    {
        // The conversation is heap-held by the map, so its address is stable for
        // as long as these callbacks, which it owns, can run.
        Conversation* owner                = &conversation;
        conversation.message_select[index] = std::make_unique<TextSelect>(
            [owner, index](size_t line_index) {
                if(index >= owner->message_lines.size() ||
                   line_index >= owner->message_lines[index].size())
                {
                    return std::string_view();
                }
                return std::string_view(owner->message_lines[index][line_index]);
            },
            [owner, index]() {
                return index < owner->message_lines.size()
                           ? owner->message_lines[index].size()
                           : size_t{ 0 };
            },
            true);
    }
    return conversation.message_select[index].get();
}

void
AssistantPanel::RenderSelectableMessage(Conversation& conversation, size_t index,
                                        const std::string& text, Colors text_color)
{
    const ImU32 color  = SettingsManager::GetInstance().GetColor(text_color);
    TextSelect* select = MessageSelect(conversation, index);
    if(select == nullptr)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, color);
        ImGui::TextWrapped("%s", text.c_str());
        ImGui::PopStyleColor();
        return;
    }
    if(conversation.message_lines[index].empty())
    {
        conversation.message_lines[index] = SplitMessageLines(text);
    }

    // TextSelect expects a text-only window whose first glyph is at its origin.
    // Splitting on explicit newlines preserves blank paragraphs while its word
    // wrapping handles each visual line.
    ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(0, 0, 0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    const ImVec2 message_spacing = ImGui::GetStyle().ItemSpacing;
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                        ImVec2(message_spacing.x, 0.0f));
    ImGui::BeginChild("##message_text", ImVec2(0.0f, 0.0f), ImGuiChildFlags_AutoResizeY,
                      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                          ImGuiWindowFlags_NoScrollWithMouse);
    // Scoped to the text so the copy menu below keeps the theme's text color.
    ImGui::PushStyleColor(ImGuiCol_Text, color);
    for(const std::string& line : conversation.message_lines[index])
    {
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextUnformatted(line.data(), line.data() + line.size());
        ImGui::PopTextWrapPos();
    }
    ImGui::PopStyleColor();

    // Only one message keeps a highlight, so Ctrl+C copies what is on screen.
    // Ctrl+C reaches only the focused window, and right-click does not focus
    // on its own. The popup flag keeps this working when the click lands while
    // another message's menu is still open.
    if(ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup) &&
       (ImGui::IsMouseClicked(ImGuiMouseButton_Left) ||
        ImGui::IsMouseClicked(ImGuiMouseButton_Right)))
    {
        ImGui::SetWindowFocus();
        for(const std::unique_ptr<TextSelect>& other : conversation.message_select)
        {
            if(other != nullptr && other.get() != select)
            {
                other->clearSelection();
            }
        }
    }
    select->update();
    // update() copies on Ctrl+C but does not report it. The same chord in the
    // same window fires on the same press.
    bool copied_selection =
        ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_C) && select->hasSelection();

    PushContextMenuStyles();
    if(ImGui::BeginPopupContextWindow("##message_copy"))
    {
        if(IconMenuItem(ICON_COPY, "Copy", select->hasSelection()))
        {
            select->copy();
            copied_selection = true;
        }
        if(IconMenuItem(ICON_LIST, "Select all"))
        {
            select->selectAll();
        }
        ImGui::Separator();
        if(IconMenuItem(ICON_DOCUMENT, "Copy message"))
        {
            ImGui::SetClipboardText(text.c_str());
            NotificationManager::GetInstance().Show("Message copied to clipboard",
                                                    NotificationLevel::Info);
        }
        ImGui::EndPopup();
    }
    PopContextMenuStyles();
    if(copied_selection)
    {
        NotificationManager::GetInstance().Show("Selection copied to clipboard",
                                                NotificationLevel::Info);
    }
    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
}

// Stacked follow-ups under the transcript: Explain this view on an empty chat,
// or the model's offered next steps after a turn.
void
AssistantPanel::RenderSuggestedActions()
{
    SettingsManager& settings = SettingsManager::GetInstance();
    const AssistantProvider* provider = settings.GetActiveAssistantProvider();
    const bool configured =
        provider != nullptr && !provider->endpoint_url.empty();
    const AssistantToolContext view         = MakeAssistantToolContext(m_view_project_id);
    const bool                 can_ask      = CanAskTrace(view);
    Conversation&              conversation = ViewConversation();

    const bool show_explain =
        conversation.lines.empty() && conversation.next_steps.empty() && !Busy();
    if(!show_explain && conversation.next_steps.empty())
    {
        return;
    }

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12.0f, 8.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f, 0.5f));
    ImGui::BeginDisabled(Busy() || !configured || !can_ask);

    if(show_explain)
    {
        if(AccentButton("Explain this view", ImVec2(-FLT_MIN, 0.0f), &settings))
        {
            SendCurrentInput(true);
        }
        ImGui::EndDisabled();
        ImGui::PopStyleVar(2);
        if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        {
            if(view.is_compare)
            {
                SetTooltipStyled("Open the two traces on their own and link them; "
                                 "Ask Optiq would add a compare tab's runs together.");
            }
            else if(view.is_compute && !can_ask)
            {
                SetTooltipStyled("Ask Optiq currently reads system traces.");
            }
            else
            {
                SetTooltipStyled(
                    "Runs a self-directed investigation: timeline overview, summary, "
                    "top events, then drills into the worst offenders and moves the "
                    "timeline to what it found.");
            }
        }
        ImGui::Spacing();
        return;
    }

    const float wrap_width = std::max(
        1.0f, ImGui::GetContentRegionAvail().x - ImGui::GetStyle().FramePadding.x * 2.0f);
    int clicked = -1;
    for(size_t i = 0; i < conversation.next_steps.size(); ++i)
    {
        ImGui::PushID(static_cast<int>(i));
        const std::string label = WrapButtonLabel(
            std::to_string(i + 1) + ". " + conversation.next_steps[i], wrap_width);
        bool pressed = false;
        if(i == 0)
        {
            pressed = AccentButton(label.c_str(), ImVec2(-FLT_MIN, 0.0f), &settings);
        }
        else
        {
            pressed = ColoredButton(label.c_str(), settings.GetColor(Colors::kButton),
                                    settings.GetColor(Colors::kButtonHovered),
                                    settings.GetColor(Colors::kButtonActive),
                                    settings.GetColor(Colors::kTextMain), nullptr,
                                    ImVec2(-FLT_MIN, 0.0f));
        }
        if(pressed)
        {
            clicked = static_cast<int>(i);
        }
        ImGui::PopID();
    }

    ImGui::EndDisabled();
    ImGui::PopStyleVar(2);

    if(clicked >= 0)
    {
        conversation.input = conversation.next_steps[static_cast<size_t>(clicked)];
        SendCurrentInput(false);
    }

    ImGui::Spacing();
}

std::string
AssistantPanel::BuildTranscriptText(const Conversation& conversation,
                                    const std::string&  trace_name) const
{
    std::string out = "# Ask Optiq conversation\n\n";
    if(!trace_name.empty())
    {
        out += "Trace: " + trace_name + "\n\n";
    }
    for(const ChatLine& line : conversation.lines)
    {
        switch(line.speaker)
        {
            case Speaker::kUser:
                out += "## You\n\n";
                out += line.text;
                out += "\n\n";
                break;
            case Speaker::kAssistant:
                out += "## Optiq\n\n";
                out += line.text;
                out += "\n\n";
                break;
            case Speaker::kStatus:
                out += "> Notice: ";
                out += line.text;
                out += "\n\n";
                break;
            case Speaker::kChart:
                // Charts redraw from live model data, so note one was shown
                // rather than freezing a snapshot.
                if(line.track_id == INVALID_UINT64_INDEX)
                {
                    out += "_[Timeline overview chart]_\n\n";
                }
                else
                {
                    out += "_[Activity chart for track " +
                           std::to_string(line.track_id) + "]_\n\n";
                }
                break;
        }
    }
    return out;
}

void
AssistantPanel::CopyTranscript()
{
    const Conversation& conversation = ViewConversation();
    if(conversation.lines.empty())
    {
        return;
    }
    const std::string trace_name = MakeAssistantToolContext(m_view_project_id).trace_name;
    ImGui::SetClipboardText(BuildTranscriptText(conversation, trace_name).c_str());
    NotificationManager::GetInstance().Show("Conversation copied to clipboard",
                                            NotificationLevel::Success);
}

void
AssistantPanel::ExportTranscript()
{
    const Conversation& conversation = ViewConversation();
    if(conversation.lines.empty())
    {
        return;
    }
    AppWindow* app = AppWindow::GetInstance();
    if(app == nullptr)
    {
        return;
    }

    FileFilter md_filter;
    md_filter.m_name       = "Markdown";
    md_filter.m_extensions = { "md" };
    FileFilter txt_filter;
    txt_filter.m_name       = "Text";
    txt_filter.m_extensions = { "txt" };

    // Captured by value: the save dialog is async and the conversation may
    // change before the user picks a path.
    const std::string text = BuildTranscriptText(
        conversation, MakeAssistantToolContext(m_view_project_id).trace_name);
    app->ShowSaveFileDialog(
        "Export Conversation", { md_filter, txt_filter }, "ask-optiq-conversation.md",
        [text](std::string path) {
            if(path.empty())
            {
                return;
            }
            std::ofstream file(path, std::ios::binary);
            if(!file.is_open())
            {
                NotificationManager::GetInstance().Show("Could not write " + path,
                                                        NotificationLevel::Error);
                return;
            }
            file.write(text.data(), static_cast<std::streamsize>(text.size()));
            // Close explicitly and check: the write may be buffered, so a full
            // disk or flush failure only surfaces here, not at is_open().
            file.close();
            if(file.fail())
            {
                NotificationManager::GetInstance().Show("Failed to write " + path,
                                                        NotificationLevel::Error);
                return;
            }
            NotificationManager::GetInstance().Show("Conversation exported to " + path,
                                                    NotificationLevel::Success);
        });
}

// Clears the conversation on screen. A turn writing to it is stopped; one
// running on another trace carries on.
void
AssistantPanel::ClearConversation()
{
    if(Busy() && m_turn_project_id == m_view_project_id)
    {
        ResetTurn();
    }
    Conversation& conversation = ViewConversation();
    conversation.lines.clear();
    conversation.message_select.clear();
    conversation.message_lines.clear();
    conversation.input.clear();
    conversation.next_steps.clear();
    ResetHistory(conversation);
    ClearTokenUsage(conversation);
}

void
AssistantPanel::RenderComposer()
{
    SettingsManager&           settings     = SettingsManager::GetInstance();
    const ImGuiStyle&          style        = settings.GetDefaultStyle();
    Conversation&              conversation = ViewConversation();
    const AssistantToolContext view         = MakeAssistantToolContext(m_view_project_id);
    const bool                 can_ask      = CanAskTrace(view);

    // Measured to the end of the card, so RenderTranscript can reserve exactly
    // this much on the next frame.
    const float start_y = ImGui::GetCursorPosY();

    BeginPanelCard("##assistant_composer", PanelCardTone::kFrame, ASSISTANT_CARD_PADDING,
                   true, &settings);

    RenderSuggestedActions();

    // One turn runs at a time, so a question on another trace waits for it,
    // and the hint says which trace it is busy with.
    const bool  busy_here      = Busy() && m_turn_project_id == m_view_project_id;
    const bool  busy_elsewhere = Busy() && !busy_here;
    std::string hint           = "Ask about this trace...";
    if(busy_elsewhere)
    {
        const std::string turn_trace = MakeToolContext().trace_name;
        hint                         = "Working on " +
               (turn_trace.empty() ? std::string("another trace") : turn_trace) + "...";
    }
    else if(busy_here)
    {
        hint = "Working...";
    }
    else if(view.is_compare)
    {
        hint = "Link the two traces from their own tabs...";
    }
    else if(view.is_compute && !can_ask)
    {
        hint = "Ask Optiq currently reads system traces...";
    }
    else if(view.data_provider == nullptr)
    {
        hint = "Open a trace...";
    }

    ImGuiInputTextFlags input_flags = ImGuiInputTextFlags_None;
    if(Busy() || !can_ask)
    {
        input_flags |= ImGuiInputTextFlags_ReadOnly;
    }

    const float gap       = style.ItemInnerSpacing.x;
    const float icon_size = ImGui::GetFrameHeight();
    const float input_width =
        std::max(ASSISTANT_COMPOSER_MIN_INPUT_WIDTH,
                 ImGui::GetContentRegionAvail().x - ASSISTANT_SEND_WIDTH -
                     icon_size * ASSISTANT_COMPOSER_ICON_BUTTONS -
                     gap * ASSISTANT_COMPOSER_GAP_COUNT);

    ImGui::SetNextItemWidth(input_width);
    InputTextStringWithHint("##assistant_input", hint.c_str(), conversation.input,
                            input_flags);
    const bool submitted = ImGui::IsItemFocused() &&
                           ImGui::IsKeyPressed(ImGuiKey_Enter) &&
                           !ImGui::GetIO().KeyShift && !Busy() && can_ask;

    ImGui::SameLine(0.0f, gap);
    ImGui::BeginDisabled(Busy() || !can_ask || conversation.input.empty());
    const bool send = AccentButton("Send", ImVec2(ASSISTANT_SEND_WIDTH, 0.0f), &settings);
    ImGui::EndDisabled();

    ImGui::SameLine(0.0f, gap);
    ImGui::BeginDisabled(conversation.lines.empty());
    if(IconButton(ICON_COPY, settings.GetFontManager().GetFont(FontType::kIcon),
                  ImVec2(icon_size, icon_size), "Copy the conversation", false,
                  ImVec2(0.0f, 0.0f), settings.GetColor(Colors::kButton),
                  settings.GetColor(Colors::kButtonHovered),
                  settings.GetColor(Colors::kButtonActive)))
    {
        CopyTranscript();
    }

    ImGui::SameLine(0.0f, gap);
    // ICON_ARROW_IN_BOX reads as "import" (it is the Import-from-Basic glyph), so
    // a document icon is used here to mean "write the conversation out to a file".
    if(IconButton(ICON_DOCUMENT, settings.GetFontManager().GetFont(FontType::kIcon),
                  ImVec2(icon_size, icon_size), "Export the conversation to a file", false,
                  ImVec2(0.0f, 0.0f), settings.GetColor(Colors::kButton),
                  settings.GetColor(Colors::kButtonHovered),
                  settings.GetColor(Colors::kButtonActive)))
    {
        ExportTranscript();
    }
    ImGui::EndDisabled();

    ImGui::SameLine(0.0f, gap);
    ImGui::BeginDisabled(conversation.lines.empty() && conversation.input.empty() &&
                         !busy_here);
    if(IconButton(ICON_TRASH_CAN, settings.GetFontManager().GetFont(FontType::kIcon),
                  ImVec2(icon_size, icon_size), "Clear the conversation", false,
                  ImVec2(0.0f, 0.0f), settings.GetColor(Colors::kButton),
                  settings.GetColor(Colors::kButtonHovered),
                  settings.GetColor(Colors::kButtonActive)))
    {
        ClearConversation();
    }
    ImGui::EndDisabled();

    EndPanelCard();

    // EndChild already advanced the cursor past one ItemSpacing, the same gap
    // the transcript has to leave above the composer.
    m_composer_height = ImGui::GetCursorPosY() - start_y;

    if(send || submitted)
    {
        SendCurrentInput(false);
    }
}

}  // namespace View
}  // namespace RocProfVis
