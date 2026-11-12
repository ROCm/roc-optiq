// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

// The Ask Optiq panel's drawing half: the dock and its splitter, the
// transcript and its cards, the wait animation, the composer, the activity
// strip, and the toolbar button that opens it. Everything here reads panel
// state and emits ImGui; the turn machinery that fills that state lives in
// rocprofvis_ai_assistant.cpp, and the two only meet through AssistantPanel
// members.

#include "rocprofvis_ai_assistant.h"

#include <algorithm>
#include <cfloat>
#include <chrono>
#include <cmath>
#include <cstdint>
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
    if(kind == AssistantFetchKind::kTabSwitch)
    {
        words.title  = "Switching traces";
        words.detail = "Bringing the other trace to the front.";
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

}  // namespace

void
AssistantPanel::RenderActivityChart(uint64_t track_id)
{
    const AssistantToolContext context = MakeToolContext();
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

    // A width persisted below the current minimum would otherwise stick until
    // the splitter was dragged.
    m_dock_width = std::max(m_dock_width, ASSISTANT_MIN_WIDTH);

    SettingsManager& settings = SettingsManager::GetInstance();

    RenderSplitter();
    ImGui::SameLine(0.0f, 0.0f);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ASSISTANT_WINDOW_PADDING);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10.0f, 10.0f));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, settings.GetColor(Colors::kBgMain));
    // Only the transcript scrolls; the header and composer are pinned.
    ImGui::BeginChild("##assistant_dock", ImVec2(m_dock_width, 0.0f),
                      ImGuiChildFlags_None,
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

    const AssistantProvider* provider = settings.GetActiveAssistantProvider();
    const bool configured = provider != nullptr && !provider->endpoint_url.empty();

    const float close_width = ImGui::GetFrameHeight();
    ImGui::BeginGroup();
    PanelIcon(ICON_COMPASS, Colors::kAccent, &settings);
    ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
    ImGui::BeginGroup();
    ImGui::PushFont(nullptr, settings.GetFontManager().GetFontSize(FontSize::kMedLarge));
    ImGui::TextUnformatted("Ask Optiq");
    ImGui::PopFont();
    ImGui::PushStyleColor(ImGuiCol_Text, settings.GetColor(Colors::kTextDim));
    if(!configured)
    {
        ImGui::TextUnformatted("Not configured");
    }
    else if(!provider->model.empty())
    {
        ImGui::TextUnformatted(provider->model.c_str());
    }
    else
    {
        ImGui::TextUnformatted("Ready");
    }
    ImGui::PopStyleColor();
    ImGui::EndGroup();
    ImGui::EndGroup();
    if(ImGui::IsItemHovered())
    {
        if(!configured)
        {
            SetTooltipStyled("Not configured. Edit > Preferences > Assistant.");
        }
        else
        {
            SetTooltipStyled("%s\n%s",
                             provider->model.empty() ? "(no model set)"
                                                     : provider->model.c_str(),
                             provider->endpoint_url.c_str());
        }
    }

    ImGui::SameLine(0.0f, 0.0f);
    const float leftover = ImGui::GetContentRegionAvail().x - close_width;
    if(leftover > 0.0f)
    {
        ImGui::Dummy(ImVec2(leftover, 0.0f));
        ImGui::SameLine(0.0f, 0.0f);
    }
    if(XButton("##assistant_close", "Close the assistant", &settings))
    {
        m_visible = false;
    }

    if(!configured)
    {
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, settings.GetColor(Colors::kTextWarning));
        ImGui::TextWrapped("Set the URL, model, and key in Edit > Preferences > Assistant.");
        ImGui::PopStyleColor();
    }

    EndPanelCard();
}

void
AssistantPanel::RenderActivityCard()
{
    if(!Busy() || m_status.empty())
    {
        m_activity_height = 0.0f;
        return;
    }

    SettingsManager&  settings = SettingsManager::GetInstance();
    const ImGuiStyle& style    = settings.GetDefaultStyle();
    const float       start_y  = ImGui::GetCursorPosY();

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
                           kind == AssistantFetchKind::kTraceLoading ||
                           kind == AssistantFetchKind::kTabSwitch;
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

    // EndChild already moved past one ItemSpacing, which the transcript has to
    // reserve as well or the card collides with the composer.
    m_activity_height = ImGui::GetCursorPosY() - start_y + style.ItemSpacing.y;
}

void
AssistantPanel::RenderTranscript()
{
    SettingsManager&  settings = SettingsManager::GetInstance();
    const ImGuiStyle& style    = settings.GetDefaultStyle();

    // Last frame's measured heights; the estimates are only for frame one.
    const float composer_height =
        m_composer_height > 0.0f
            ? m_composer_height
            : ImGui::GetFrameHeight() + style.ItemSpacing.y * 2.0f +
                  ASSISTANT_CARD_PADDING.y * 2.0f;
    const bool  show_activity = Busy() && !m_status.empty();
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

    if(m_lines.empty())
    {
        ImGui::PushStyleColor(ImGuiCol_Text, settings.GetColor(Colors::kTextMain));
        ImGui::TextWrapped("Ask about this trace, or press Explain this view below.");
        ImGui::PopStyleColor();
        ImGui::PushStyleColor(ImGuiCol_Text, settings.GetColor(Colors::kTextDim));
        ImGui::TextWrapped(
            "I read the timeline overview first, then dig into whatever looks worst.");
        ImGui::PopStyleColor();
    }

    for(size_t i = 0; i < m_lines.size(); ++i)
    {
        RenderMessageCard(i, m_lines[i]);
    }

    if(m_scroll_to_bottom)
    {
        ImGui::SetScrollHereY(1.0f);
        m_scroll_to_bottom = false;
    }
    ImGui::EndChild();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(3);
}

void
AssistantPanel::RenderMessageCard(size_t index, const ChatLine& line)
{
    SettingsManager&  settings = SettingsManager::GetInstance();
    const ImGuiStyle& style    = settings.GetDefaultStyle();

    ImGui::PushID(static_cast<int>(index));

    if(line.speaker == Speaker::kStatus)
    {
        // A notice that outlived its turn, such as a failed request.
        PanelIcon(ICON_X_CIRCLED, Colors::kTextError, &settings);
        ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
        RenderSelectableMessage(index, line.text, Colors::kTextError);
        ImGui::PopID();
        return;
    }

    if(line.speaker == Speaker::kChart)
    {
        BeginPanelCard("##chart_card", PanelCardTone::kPanel, ASSISTANT_CARD_PADDING,
                       true, &settings);
        PanelIcon(ICON_CHART_BAR, Colors::kAccent, &settings);
        ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
        PanelFieldLabel("Timeline overview", false, &settings);
        RenderActivityChart(line.track_id);
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
    RenderSelectableMessage(index, line.text, Colors::kTextMain);
    EndPanelCard();
    ImGui::PopID();
}

TextSelect*
AssistantPanel::MessageSelect(size_t index)
{
    if(index >= m_lines.size())
    {
        return nullptr;
    }
    if(m_message_select.size() < m_lines.size())
    {
        m_message_select.resize(m_lines.size());
    }
    if(m_message_lines.size() < m_lines.size())
    {
        m_message_lines.resize(m_lines.size());
    }
    if(m_message_select[index] == nullptr)
    {
        m_message_select[index] = std::make_unique<TextSelect>(
            [this, index](size_t line_index) {
                if(index >= m_message_lines.size() ||
                   line_index >= m_message_lines[index].size())
                {
                    return std::string_view();
                }
                return std::string_view(m_message_lines[index][line_index]);
            },
            [this, index]() {
                return index < m_message_lines.size() ? m_message_lines[index].size()
                                                      : size_t{0};
            },
            true);
    }
    return m_message_select[index].get();
}

void
AssistantPanel::RenderSelectableMessage(size_t index, const std::string& text,
                                        Colors text_color)
{
    const ImU32 color  = SettingsManager::GetInstance().GetColor(text_color);
    TextSelect* select = MessageSelect(index);
    if(select == nullptr)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, color);
        ImGui::TextWrapped("%s", text.c_str());
        ImGui::PopStyleColor();
        return;
    }
    if(m_message_lines[index].empty())
    {
        m_message_lines[index] = SplitMessageLines(text);
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
    for(const std::string& line : m_message_lines[index])
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
        for(const std::unique_ptr<TextSelect>& other : m_message_select)
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

    const bool show_explain = m_lines.empty() && m_next_steps.empty() && !Busy();
    if(!show_explain && m_next_steps.empty())
    {
        return;
    }

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12.0f, 8.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f, 0.5f));
    ImGui::BeginDisabled(Busy() || !configured);

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
            SetTooltipStyled(
                "Runs a self-directed investigation: timeline overview, summary, top "
                "events, then drills into the worst offenders and moves the timeline "
                "to what it found.");
        }
        ImGui::Spacing();
        return;
    }

    const float wrap_width = std::max(
        1.0f, ImGui::GetContentRegionAvail().x - ImGui::GetStyle().FramePadding.x * 2.0f);
    int clicked = -1;
    for(size_t i = 0; i < m_next_steps.size(); ++i)
    {
        ImGui::PushID(static_cast<int>(i));
        const std::string label =
            WrapButtonLabel(std::to_string(i + 1) + ". " + m_next_steps[i], wrap_width);
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
        m_input = m_next_steps[static_cast<size_t>(clicked)];
        SendCurrentInput(false);
    }

    ImGui::Spacing();
}

std::string
AssistantPanel::BuildTranscriptText() const
{
    std::string out = "# Ask Optiq conversation\n\n";
    for(const ChatLine& line : m_lines)
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
    if(m_lines.empty())
    {
        return;
    }
    ImGui::SetClipboardText(BuildTranscriptText().c_str());
    NotificationManager::GetInstance().Show("Conversation copied to clipboard",
                                            NotificationLevel::Success);
}

void
AssistantPanel::ExportTranscript()
{
    if(m_lines.empty())
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
    const std::string text = BuildTranscriptText();
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

void
AssistantPanel::RenderComposer()
{
    SettingsManager&  settings = SettingsManager::GetInstance();
    const ImGuiStyle& style    = settings.GetDefaultStyle();

    // Measured to the end of the card, so RenderTranscript can reserve exactly
    // this much on the next frame.
    const float start_y = ImGui::GetCursorPosY();

    BeginPanelCard("##assistant_composer", PanelCardTone::kFrame, ASSISTANT_CARD_PADDING,
                   true, &settings);

    RenderSuggestedActions();

    ImGuiInputTextFlags input_flags = ImGuiInputTextFlags_None;
    if(Busy())
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
    InputTextStringWithHint("##assistant_input",
                            Busy() ? "Working..." : "Ask about this trace...", m_input,
                            input_flags);
    const bool submitted = ImGui::IsItemFocused() &&
                           ImGui::IsKeyPressed(ImGuiKey_Enter) &&
                           !ImGui::GetIO().KeyShift && !Busy();

    ImGui::SameLine(0.0f, gap);
    ImGui::BeginDisabled(Busy() || m_input.empty());
    const bool send = AccentButton("Send", ImVec2(ASSISTANT_SEND_WIDTH, 0.0f), &settings);
    ImGui::EndDisabled();

    ImGui::SameLine(0.0f, gap);
    ImGui::BeginDisabled(m_lines.empty());
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
    ImGui::BeginDisabled(m_lines.empty() && m_input.empty() && !Busy());
    if(IconButton(ICON_TRASH_CAN, settings.GetFontManager().GetFont(FontType::kIcon),
                  ImVec2(icon_size, icon_size), "Clear the conversation", false,
                  ImVec2(0.0f, 0.0f), settings.GetColor(Colors::kButton),
                  settings.GetColor(Colors::kButtonHovered),
                  settings.GetColor(Colors::kButtonActive)))
    {
        ResetTurn();
        m_lines.clear();
        m_message_select.clear();
        m_message_lines.clear();
        m_input.clear();
        m_conversation.clear();
        m_next_steps.clear();
    }
    ImGui::EndDisabled();

    EndPanelCard();

    // EndChild already advanced the cursor past one ItemSpacing, which the
    // transcript's reservation has to include too.
    m_composer_height = ImGui::GetCursorPosY() - start_y +
                        settings.GetDefaultStyle().ItemSpacing.y;

    if(send || submitted)
    {
        SendCurrentInput(false);
    }
}

}  // namespace View
}  // namespace RocProfVis
