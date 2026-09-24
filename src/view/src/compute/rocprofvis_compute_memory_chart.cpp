// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_compute_memory_chart.h"

#include "rocprofvis_compute_selection.h"
#include "rocprofvis_data_provider.h"
#include "rocprofvis_memory_chart_layouts_generated.h"
#include "rocprofvis_requests.h"
#include "rocprofvis_event_manager.h"
#include "rocprofvis_settings_manager.h"
#include "rocprofvis_utils.h"
#include "model/compute/rocprofvis_compute_data_model.h"
#include "widgets/rocprofvis_gui_helpers.h"

#include "spdlog/spdlog.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <functional>
#include <iterator>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <vector>

#include <imgui.h>

namespace RocProfVis
{
namespace View
{

// Filename of an optional runtime override dropped at <config-dir>/.
static constexpr const char* OVERRIDE_FILE_NAME = "memory_chart.json";

// Layout constants.
static constexpr float CHART_PADDING     = 20.0f;
static constexpr float LEFT_MARGIN       = 60.0f;   // Lane for arrows entering column 0 from the left.
static constexpr float BLOCK_GAP         = 18.0f;
static constexpr float COLUMN_GAP        = 130.0f;  // Base gap between columns that labeled arrows cross.
static constexpr float COLUMN_GAP_UNLABELED = 60.0f; // Gap crossed only by unlabeled arrows: heads + a short run.
static constexpr float COLUMN_GAP_NO_ARROW = 40.0f; // Tight gap between columns with no arrow crossing.
static constexpr float MAX_COLUMN_GAP    = 200.0f;  // Cap when spreading to fill wide panels.
static constexpr float GAP_LABEL_CLEARANCE = 12.0f; // Space kept between a gap's label box and each column edge.
static constexpr float ROW_HEIGHT        = 26.0f;
static constexpr float BLOCK_BODY_TOP    = 6.0f;
static constexpr float MIN_BLOCK_WIDTH   = 240.0f;
static constexpr float MIN_TITLE_ONLY_WIDTH = 96.0f;  // Blocks with no metric rows only need to fit the title.
static constexpr float MAX_BLOCK_WIDTH   = 400.0f;
static constexpr float MIN_BLOCK_HEIGHT  = 96.0f;
static constexpr float EMPTY_BODY_H      = 40.0f;
static constexpr float LABEL_ONLY_PAD_Y  = 14.0f;   // Title padding of a label-only block outside row 0.
static constexpr float MIN_TARGET_HEIGHT = 460.0f;  // Floor for the common column height (row 0 only).
static constexpr float INTER_ROW_MIN_GAP = 40.0f;   // Corridor between row bands with no labels in it.
static constexpr float INTER_ROW_LABEL_PAD = 12.0f; // Corridor space above/below its label stack.
static constexpr float ROW_VERT_STEP     = 16.0f;   // Horizontal pitch between fanned vertical inter-row connectors.
static constexpr float ROW_ELBOW_STUB    = 18.0f;   // Vertical stub off a block before a cross-column inter-row elbow turns.
static constexpr float GROUP_HEADER      = 26.0f;  // Title band of a group box.
static constexpr float GROUP_PAD         = 9.0f;   // Inset of blocks inside a group box.
static constexpr float GROUP_INNER_GAP   = 12.0f;  // Gap between blocks inside a group.

// Drawing constants.
static constexpr float BLOCK_ROUNDING   = 8.0f;
static constexpr float BLOCK_TEXT_PAD   = 10.0f;
static constexpr float HEADER_SEP_GAP   = 6.0f;
static constexpr float METRIC_VALUE_GAP = 14.0f;
static constexpr float LEGEND_HEIGHT    = 28.0f;

// Block frame drawing (DrawBlockRect).
static constexpr float BLOCK_SHADOW_OFFSET_X    = 3.0f;   // Drop-shadow horizontal offset.
static constexpr float BLOCK_SHADOW_OFFSET_Y    = 4.0f;   // Drop-shadow vertical offset.
static constexpr float BLOCK_HIGHLIGHT_INSET    = 1.0f;   // Inset of the top highlight band from the block edge.
static constexpr float BLOCK_HIGHLIGHT_BOTTOM   = 3.0f;   // Bottom offset of the top highlight band.
static constexpr float BLOCK_HIGHLIGHT_ROUNDING = 2.0f;   // Corner rounding of the highlight band.
static constexpr float BLOCK_HIGHLIGHT_ALPHA    = 0.55f;  // Alpha applied to the hot-border highlight.
static constexpr float BLOCK_BORDER_THICKNESS   = 1.0f;   // Outline thickness of the block frame.

// Block header drawing (DrawBlockHeader / HeaderHeight).
static constexpr float HEADER_ACCENT_INSET    = 2.0f;   // Vertical inset of the title accent bar.
static constexpr float HEADER_ACCENT_WIDTH    = 3.0f;   // Width of the title accent bar.
static constexpr float HEADER_ACCENT_ROUNDING = 2.0f;   // Corner rounding of the accent bar.
static constexpr float HEADER_TITLE_INDENT    = 9.0f;   // Title text indent from the block text pad.
static constexpr float HEADER_TITLE_GAP       = 5.0f;   // Gap between the title text and the separator line.
static constexpr float HEADER_SEP_ALPHA       = 0.55f;  // Alpha of the header separator line.
static constexpr float HEADER_SEP_THICKNESS   = 1.0f;   // Thickness of the header separator line.

static constexpr float ARROW_THICKNESS   = 2.5f;
static constexpr float ARROW_HEAD_SIZE   = 8.0f;
static constexpr float ARROW_LABEL_ABOVE = 4.0f;
static constexpr float LABEL_STACK_GAP   = 4.0f;   // Gap between fanned/stacked label boxes.
static constexpr float ARROW_DASH_LENGTH = 6.0f;
static constexpr float ARROW_DASH_GAP    = 4.0f;
static constexpr float LANE_GAP          = 20.0f;
static constexpr int   MAX_DASH_ITERS    = 20000;
static constexpr int   MAX_LABEL_PASSES  = 200;

static constexpr float ARROW_GLOW_ALPHA      = 0.22f;  // Alpha of the soft underglow behind a dashed line.
static constexpr float ARROW_GLOW_EXTRA      = 3.0f;   // Extra thickness of the underglow vs. the line.
static constexpr float ARROW_HEAD_HALF_RATIO = 0.6f;   // Arrow-head half-width as a fraction of its length.

// Arrow routing (BuildArrowRoutes).
static constexpr float SAME_COL_LANE_BASE  = 12.0f;  // First same-column lane offset past the block edge.
static constexpr float SAME_COL_LANE_STEP  = 14.0f;  // Horizontal pitch between stacked same-column lanes.
static constexpr float SAME_COL_LANE_PAD   = 10.0f;  // Vertical clearance between spans sharing a same-column lane.
static constexpr float SAME_COL_LABEL_GAP  = 6.0f;   // Label offset past a same-column lane.
static constexpr float SKIP_MARGIN_RATIO   = 0.35f;  // Fraction of LEFT_MARGIN used by left-going skip routes.
static constexpr float SKIP_LANE_PAD       = 24.0f;  // Clearance between spans sharing a highway lane.
static constexpr float SKIP_LANE_LABEL_PAD = 12.0f;  // Extra pitch between highway lanes for the label.
static constexpr float SKIP_HIGHWAY_DROP   = 30.0f;  // Drop below the blocks to the first highway lane.
static constexpr float PORT_STEP           = 16.0f;  // Horizontal pitch between a block's bottom ports.
static constexpr float PORT_EDGE_PAD       = 14.0f;  // Keep bottom ports this far from the block edges.

// Floating label (DrawFloatingLabel / ResolveLabelOverlaps).
static constexpr float LABEL_PAD_X            = 6.0f;   // Horizontal padding inside a floating label.
static constexpr float LABEL_PAD_Y            = 4.0f;   // Vertical padding inside a floating label.
static constexpr float LABEL_ROUNDING         = 4.0f;   // Corner rounding of a floating label.
static constexpr float LABEL_BORDER_ALPHA     = 0.7f;   // Alpha of a floating label's border.
static constexpr float LABEL_BORDER_THICKNESS = 1.0f;   // Border thickness of a floating label.
static constexpr float LABEL_OVERLAP_NUDGE    = 7.0f;   // Vertical breathing room between stacked labels.
static constexpr float LABEL_MARKER_SIZE      = 8.0f;   // Size of the flow-direction marker drawn in a label.
static constexpr float LABEL_MARKER_GAP       = 5.0f;   // Gap between the direction marker and the label text.

// Group box (DrawGroupBox).
static constexpr float GROUP_FILL_ALPHA       = 0.35f;  // Alpha of a group box's fill.
static constexpr float GROUP_BORDER_ALPHA     = 0.9f;   // Alpha of a group box's border.
static constexpr float GROUP_BORDER_THICKNESS = 1.5f;   // Border thickness of a group box.
static constexpr float GROUP_TITLE_TOP        = 6.0f;   // Title inset from the group box top.

// Legend (DrawLegend).
static constexpr float LEGEND_LABEL_GAP       = 12.0f;  // Gap after the "Legend:" caption.
static constexpr float LEGEND_SWATCH_TOP      = 4.0f;   // Swatch top inset within the legend row.
static constexpr float LEGEND_SWATCH_WIDTH    = 12.0f;  // Swatch width.
static constexpr float LEGEND_SWATCH_BOTTOM   = 10.0f;  // Swatch bottom inset within the legend row.
static constexpr float LEGEND_SWATCH_ROUNDING = 2.0f;   // Swatch corner rounding.
static constexpr float LEGEND_TEXT_GAP        = 17.0f;  // Swatch-left to label-left (and swatch advance).
static constexpr float LEGEND_ITEM_GAP        = 16.0f;  // Gap between legend items.

// Metric row (DrawLeaf).
static constexpr float ROW_INSET_X         = 6.0f;   // Horizontal inset of a metric row's hover rect.
static constexpr float ROW_HOVER_INSET     = 2.0f;   // Vertical inset of a metric row's hover rect.
static constexpr float ROW_HOVER_ALPHA     = 0.16f;  // Alpha of a metric row's hover highlight.
static constexpr float ROW_HOVER_ROUNDING  = 4.0f;   // Corner rounding of a metric row's hover highlight.
static constexpr float ROW_ACCENT_TOP      = 4.0f;   // Metric-row accent bar top offset.
static constexpr float ROW_ACCENT_WIDTH    = 3.0f;   // Metric-row accent bar width.
static constexpr float ROW_ACCENT_BOTTOM   = 12.0f;  // Metric-row accent bar bottom offset.
static constexpr float ROW_ACCENT_ALPHA    = 0.85f;  // Metric-row accent bar alpha.
static constexpr float ROW_ACCENT_ROUNDING = 2.0f;   // Metric-row accent bar corner rounding.
static constexpr float ROW_LABEL_INDENT    = 7.0f;   // Metric-row label indent past the accent bar.

// Block sizing / canvas / tooltip.
static constexpr float BLOCK_CONTENT_EXTRA_W = 12.0f;   // Slack added to a leaf block's content width.
static constexpr float CANVAS_BOTTOM_PAD     = 6.0f;    // Padding below the lowest route/label.
static constexpr float TOOLTIP_MAX_WIDTH     = 300.0f;  // Max width of a metric tooltip.

// Missing `order` sorts after any explicit value; stable_sort keeps declaration
// order among siblings that omit the field.
static constexpr int32_t UNSET_ORDER_SORT_KEY = 0x7fffffff;

static constexpr const char* UNAVAILABLE_METRIC_TEXT = "N/A";

static int32_t
BlockOrderKey(int32_t order)
{
    return order < 0 ? UNSET_ORDER_SORT_KEY : order;
}

static bool
BlockOrderLess(const MemChartBlock& a, const MemChartBlock& b)
{
    return BlockOrderKey(a.order) < BlockOrderKey(b.order);
}

static bool
TopLevelBlockLess(const MemChartBlock& a, const MemChartBlock& b)
{
    if(a.column != b.column)
    {
        return a.column < b.column;
    }
    return BlockOrderLess(a, b);
}

static void
SortNestedChildrenByOrder(std::vector<MemChartBlock>& blocks)
{
    for(MemChartBlock& block : blocks)
    {
        std::stable_sort(block.children.begin(), block.children.end(), BlockOrderLess);
        SortNestedChildrenByOrder(block.children);
    }
}

// Sort top-level blocks by (column, order) and nested children by `order`.
// Must run before the id -> block index is built: in-place sort moves the
// objects. ComputeLayout then only buckets pointers; it does not re-sort.
static void
SortLayoutBlocks(std::vector<MemChartBlock>& blocks)
{
    std::stable_sort(blocks.begin(), blocks.end(), TopLevelBlockLess);
    SortNestedChildrenByOrder(blocks);
}

// Depth-first walk of a (possibly nested) block list. Layouts nest metric
// leaves inside container boxes, so callers cannot just iterate m_layout.blocks.
template <typename Blocks, typename Fn>
static void
ForEachBlock(Blocks& blocks, Fn&& fn)
{
    for(auto& block : blocks)
    {
        fn(block);
        ForEachBlock(block.children, fn);
    }
}

static bool
StartsWith(const std::string& text, const char* prefix)
{
    return std::strncmp(text.c_str(), prefix, std::strlen(prefix)) == 0;
}

// Color slot from the label's leading word so unlabeled older layouts still
// distinguish read/write/atomic. Unrecognized prefixes (including a blank
// label) map to the chart's main text color.
static MemChartColorKind
ColorKindForLabel(const std::string& label)
{
    if(StartsWith(label, "Wr")) return MemChartColorKind::kWrite;
    if(StartsWith(label, "Rd") || StartsWith(label, "Read")) return MemChartColorKind::kRead;
    if(StartsWith(label, "Atomic")) return MemChartColorKind::kAtomic;
    if(StartsWith(label, "Util")) return MemChartColorKind::kUtil;
    if(StartsWith(label, "Hit")) return MemChartColorKind::kHit;
    if(StartsWith(label, "Stall")) return MemChartColorKind::kStall;
    return MemChartColorKind::kNeutral;
}

// Slot from an explicit data-driven category. Falls back to the label
// heuristic when no category is given, so older layouts keep working.
static MemChartColorKind
ColorKindForCategory(const std::string& category, const std::string& label)
{
    if(category.empty()) return ColorKindForLabel(label);
    if(category == "read") return MemChartColorKind::kRead;
    if(category == "write") return MemChartColorKind::kWrite;
    if(category == "atomic") return MemChartColorKind::kAtomic;
    if(category == "util") return MemChartColorKind::kUtil;
    if(category == "hit") return MemChartColorKind::kHit;
    if(category == "stall") return MemChartColorKind::kStall;
    return MemChartColorKind::kNeutral;  // "misc" / anything else.
}

static bool
IsAvailableMetricText(const std::string& text)
{
    return text != UNAVAILABLE_METRIC_TEXT && text != "-";
}

static std::string
FormatMetricValue(double value)
{
    if(std::isnan(value)) return UNAVAILABLE_METRIC_TEXT;
    return compact_number_format(value);
}

static std::string
FormatMetricValueRaw(double value)
{
    if(std::isnan(value)) return UNAVAILABLE_METRIC_TEXT;
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.2f", value);
    return std::string(buf);
}

void
ComputeMemoryChartView::DrawBlockRect(ImDrawList* draw_list, ImVec2 top_left, ImVec2 bottom_right)
{
    draw_list->AddRectFilled({top_left.x + BLOCK_SHADOW_OFFSET_X, top_left.y + BLOCK_SHADOW_OFFSET_Y},
                             {bottom_right.x + BLOCK_SHADOW_OFFSET_X, bottom_right.y + BLOCK_SHADOW_OFFSET_Y},
                             m_colors.shadow, BLOCK_ROUNDING);
    draw_list->AddRectFilled(top_left, bottom_right, m_colors.panel, BLOCK_ROUNDING);
    draw_list->AddRectFilled({top_left.x + BLOCK_HIGHLIGHT_INSET, top_left.y + BLOCK_HIGHLIGHT_INSET},
                             {bottom_right.x - BLOCK_HIGHLIGHT_INSET, top_left.y + BLOCK_HIGHLIGHT_BOTTOM},
                             ApplyAlpha(m_colors.border_hot, BLOCK_HIGHLIGHT_ALPHA), BLOCK_HIGHLIGHT_ROUNDING);
    draw_list->AddRect(top_left, bottom_right, m_colors.border, BLOCK_ROUNDING, 0, BLOCK_BORDER_THICKNESS);
}

float
ComputeMemoryChartView::DrawBlockHeader(ImDrawList* draw_list, const char* title, float block_x,
                                        float block_y, float block_w)
{
    float text_y = block_y + BLOCK_TEXT_PAD;
    float text_h = ImGui::CalcTextSize(title).y;
    draw_list->AddRectFilled({block_x + BLOCK_TEXT_PAD, text_y + HEADER_ACCENT_INSET},
                             {block_x + BLOCK_TEXT_PAD + HEADER_ACCENT_WIDTH, text_y + text_h - HEADER_ACCENT_INSET},
                             m_colors.read, HEADER_ACCENT_ROUNDING);
    draw_list->AddText(ImVec2(block_x + BLOCK_TEXT_PAD + HEADER_TITLE_INDENT, text_y), m_colors.text_main, title);

    float line_y = text_y + text_h + HEADER_TITLE_GAP;
    draw_list->AddLine(ImVec2(block_x + BLOCK_TEXT_PAD, line_y),
                       ImVec2(block_x + block_w - BLOCK_TEXT_PAD, line_y),
                       ApplyAlpha(m_colors.border, HEADER_SEP_ALPHA), HEADER_SEP_THICKNESS);
    return line_y + HEADER_SEP_GAP;
}

// Height reserved for a block header, matching DrawBlockHeader.
static float
HeaderHeight()
{
    return BLOCK_TEXT_PAD + ImGui::CalcTextSize("X").y + HEADER_TITLE_GAP + HEADER_SEP_GAP;
}

static void
DrawDashedLine(ImDrawList* draw_list, ImVec2 from, ImVec2 to, ImU32 color)
{
    ImVec2 delta(to.x - from.x, to.y - from.y);
    float  length = std::sqrt(delta.x * delta.x + delta.y * delta.y);
    if(!std::isfinite(length) || length <= 0.0f) return;

    ImVec2 dir(delta.x / length, delta.y / length);
    float  cursor = 0.0f;
    int    guard  = 0;
    while(cursor < length && guard++ < MAX_DASH_ITERS)
    {
        float  dash_end = std::min(cursor + ARROW_DASH_LENGTH, length);
        ImVec2 dash_from(from.x + dir.x * cursor, from.y + dir.y * cursor);
        ImVec2 dash_to(from.x + dir.x * dash_end, from.y + dir.y * dash_end);
        draw_list->AddLine(dash_from, dash_to, ApplyAlpha(color, ARROW_GLOW_ALPHA),
                           ARROW_THICKNESS + ARROW_GLOW_EXTRA);
        draw_list->AddLine(dash_from, dash_to, color, ARROW_THICKNESS);
        cursor = dash_end + ARROW_DASH_GAP;
    }
}

// Filled arrow head at `tip`, pointing along the unit vector `dir`.
static void
DrawArrowHead(ImDrawList* draw_list, ImVec2 tip, ImVec2 dir, ImU32 color)
{
    float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
    if(!std::isfinite(len) || len <= 0.0001f) return;
    ImVec2 unit(dir.x / len, dir.y / len);
    ImVec2 perp(-unit.y, unit.x);
    float  head = ARROW_HEAD_SIZE;
    float  half = head * ARROW_HEAD_HALF_RATIO;
    ImVec2 base(tip.x - unit.x * head, tip.y - unit.y * head);
    ImVec2 a(base.x + perp.x * half, base.y + perp.y * half);
    ImVec2 b(base.x - perp.x * half, base.y - perp.y * half);
    draw_list->AddTriangleFilled(tip, a, b, color);
}

void
ComputeMemoryChartView::DrawFloatingLabel(ImDrawList* draw_list, ImVec2 pos, const char* text,
                                          uint32_t accent_color, float dir_x, float dir_y,
                                          bool bidir)
{
    ImVec2 text_size = ImGui::CalcTextSize(text);
    ImVec2 pad(LABEL_PAD_X, LABEL_PAD_Y);

    bool   has_marker = (dir_x != 0.0f || dir_y != 0.0f);
    float  marker_w   = has_marker ? (LABEL_MARKER_SIZE + LABEL_MARKER_GAP) : 0.0f;
    ImVec2 text_pos(pos.x + marker_w, pos.y);

    ImVec2 min(pos.x - pad.x, pos.y - pad.y);
    ImVec2 max(text_pos.x + text_size.x + pad.x, pos.y + text_size.y + pad.y);
    draw_list->AddRectFilled(min, max, m_colors.bg, LABEL_ROUNDING);
    draw_list->AddRect(min, max, ApplyAlpha(accent_color, LABEL_BORDER_ALPHA), LABEL_ROUNDING, 0,
                       LABEL_BORDER_THICKNESS);

    if(has_marker)
    {
        // Flow-direction glyph so same-color arrows pointing opposite ways differ.
        // Bidirectional draws two half-length heads meeting tip-out at the centre.
        ImVec2 center(pos.x + LABEL_MARKER_SIZE * 0.5f, pos.y + text_size.y * 0.5f);
        float  half = LABEL_MARKER_SIZE * 0.5f;
        auto   head = [&](float sx, float sy, float len) {
            ImVec2 tip(center.x + sx * half, center.y + sy * half);
            ImVec2 base(tip.x - sx * len, tip.y - sy * len);
            ImVec2 perp(-sy * len * 0.5f, sx * len * 0.5f);
            draw_list->AddTriangleFilled(tip, {base.x + perp.x, base.y + perp.y},
                                         {base.x - perp.x, base.y - perp.y}, accent_color);
        };
        if(bidir)
        {
            head(dir_x, dir_y, half);
            head(-dir_x, -dir_y, half);
        }
        else
        {
            head(dir_x, dir_y, LABEL_MARKER_SIZE);
        }
    }

    draw_list->AddText(text_pos, accent_color, text);
}

void
ComputeMemoryChartView::DrawGroupBox(ImDrawList* draw_list, ImVec2 top_left, float w, float h,
                                     const char* title)
{
    ImVec2 bottom_right(top_left.x + w, top_left.y + h);
    draw_list->AddRectFilled(top_left, bottom_right, ApplyAlpha(m_colors.panel_alt, GROUP_FILL_ALPHA),
                             BLOCK_ROUNDING);
    draw_list->AddRect(top_left, bottom_right, ApplyAlpha(m_colors.border, GROUP_BORDER_ALPHA),
                       BLOCK_ROUNDING, 0, GROUP_BORDER_THICKNESS);
    if(title && title[0] != '\0')
    {
        draw_list->AddText({top_left.x + BLOCK_TEXT_PAD, top_left.y + GROUP_TITLE_TOP},
                           m_colors.text_dim, title);
    }
}

void
ComputeMemoryChartView::DrawLegend(ImDrawList* draw_list, ImVec2 origin, float y)
{
    struct LegendItem
    {
        const char* text;
        uint32_t    color;
    };
    const LegendItem legend[] = {
        {"Read", m_colors.read}, {"Write", m_colors.write}, {"Atomic", m_colors.atomic},
        {"Util", m_colors.util}, {"Hit", m_colors.hit},     {"Stall", m_colors.stall}};

    ImVec2 pos(origin.x + CHART_PADDING, origin.y + y);
    draw_list->AddText(pos, m_colors.text_dim, "Legend:");
    pos.x += ImGui::CalcTextSize("Legend:").x + LEGEND_LABEL_GAP;
    for(const LegendItem& item : legend)
    {
        draw_list->AddRectFilled({pos.x, pos.y + LEGEND_SWATCH_TOP},
                                 {pos.x + LEGEND_SWATCH_WIDTH, pos.y + LEGEND_SWATCH_BOTTOM},
                                 item.color, LEGEND_SWATCH_ROUNDING);
        draw_list->AddText({pos.x + LEGEND_TEXT_GAP, pos.y}, m_colors.text_dim, item.text);
        pos.x += LEGEND_TEXT_GAP + ImGui::CalcTextSize(item.text).x + LEGEND_ITEM_GAP;
    }
}

ComputeMemoryChartView::ComputeMemoryChartView(
    DataProvider& data_provider, std::shared_ptr<ComputeSelection> compute_selection)
: m_data_provider(data_provider)
, m_compute_selection(compute_selection)
, m_client_id(IdGenerator::GetInstance().GenerateId())
{
    RefreshPalette();
    m_theme_changed_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kThemeChanged),
        [this](std::shared_ptr<RocEvent> e) {
            (void) e;
            RefreshPalette();
            RefreshCachedColors();
        });
    LoadLayout();
}

ComputeMemoryChartView::~ComputeMemoryChartView()
{
    EventManager::GetInstance()->Unsubscribe(
        static_cast<int>(RocEvents::kThemeChanged), m_theme_changed_token);
}

// Embedded layout by exact registry key ("default", "gfx950", ...).
static const char*
FindEmbeddedLayout(const char* key)
{
    for(const MemChartEmbeddedLayout& entry : kMemChartEmbeddedLayouts)
    {
        if(std::strcmp(entry.key, key) == 0) return entry.json;
    }
    return nullptr;
}

static const char*
DefaultEmbeddedLayout()
{
    const char* def = FindEmbeddedLayout("default");
    return def ? def : kMemChartEmbeddedLayouts[0].json;
}

// Exact arch key first, then family (gfx94x/gfx950), then default.
static const char*
EmbeddedLayoutForArch(const std::string& arch)
{
    if(!arch.empty())
    {
        if(const char* exact = FindEmbeddedLayout(arch.c_str())) return exact;
        if(arch.rfind("gfx94", 0) == 0)
        {
            if(const char* fam = FindEmbeddedLayout("gfx94x")) return fam;
        }
        else if(arch.rfind("gfx95", 0) == 0)
        {
            if(const char* fam = FindEmbeddedLayout("gfx950")) return fam;
        }
    }
    return DefaultEmbeddedLayout();
}

// GPU arch (e.g. "gfx950") from the workload's system info. The controller
// surfaces keys with underscores as spaces ("gpu_arch" -> "gpu arch").
static std::string
WorkloadArch(const WorkloadInfo* workload)
{
    if(!workload || workload->system_info.size() < 2) return "";
    const std::vector<std::string>& names  = workload->system_info[0];
    const std::vector<std::string>& values = workload->system_info[1];
    for(size_t i = 0; i < names.size() && i < values.size(); ++i)
    {
        std::string key = names[i];
        std::replace(key.begin(), key.end(), ' ', '_');
        if(key == "gpu_arch") return values[i];
    }
    return "";
}

bool
ComputeMemoryChartView::TryLoadOverrideFile()
{
    std::string config_dir = get_application_config_path(false);
    if(config_dir.empty()) return false;

    std::string   override_path = config_dir + "/" + OVERRIDE_FILE_NAME;
    std::ifstream file(override_path);
    if(!file.good()) return false;

    std::stringstream buffer;
    buffer << file.rdbuf();

    MemChartLayout override_layout;
    std::string    override_error;
    if(MemChartLayout::ParseFromString(buffer.str(), override_layout, &override_error))
    {
        m_layout = std::move(override_layout);
        spdlog::info("Memory chart: loaded override layout from {}", override_path);
        return true;
    }
    spdlog::warn("Memory chart: ignoring invalid override {} ({})", override_path,
                 override_error);
    return false;
}

void
ComputeMemoryChartView::LoadLayout()
{
    // Dev override wins if present; otherwise the embedded default layout.
    if(!TryLoadOverrideFile())
    {
        std::string error;
        if(!MemChartLayout::ParseFromString(DefaultEmbeddedLayout(), m_layout, &error))
        {
            spdlog::error("Memory chart: failed to parse embedded default layout: {}", error);
        }
    }
    OnLayoutLoaded();
}

void
ComputeMemoryChartView::LoadWorkloadLayout(uint32_t workload_id)
{
    // Priority: dev override file -> per-workload DB layout (parsed once by the
    // data provider) -> architecture-specific embedded layout -> embedded default.
    if(TryLoadOverrideFile())
    {
        OnLayoutLoaded();
        return;
    }

    const WorkloadInfo* workload = m_data_provider.ComputeModel().GetWorkload(workload_id);

    if(workload && !workload->memory_chart_layout.blocks.empty())
    {
        m_layout = workload->memory_chart_layout;
        spdlog::info("Memory chart: using layout from workload {} database blob", workload_id);
        OnLayoutLoaded();
        return;
    }

    const std::string arch   = WorkloadArch(workload);
    const char*       layout = EmbeddedLayoutForArch(arch);
    std::string       error;
    if(MemChartLayout::ParseFromString(layout, m_layout, &error))
    {
        spdlog::info("Memory chart: using embedded layout for arch '{}' (workload {})",
                     arch.empty() ? "default" : arch, workload_id);
    }
    else
    {
        spdlog::error("Memory chart: failed to parse embedded layout for arch '{}': {}", arch,
                      error);
    }
    OnLayoutLoaded();
}

void
ComputeMemoryChartView::OnLayoutLoaded()
{
    // Sort by column/order before resolving: ComputeLayout and PositionBlock must
    // not move these objects, or the arrows' endpoint pointers would dangle.
    SortLayoutBlocks(m_layout.blocks);

    // Resolve each arrow's endpoint ids to blocks once (pointers into m_layout,
    // valid until the next layout load), so layout and routing never look ids
    // up. Nested ids are first-class arrow endpoints. The parser already
    // rejected unknown ids; a miss here just leaves the pointer null.
    std::unordered_map<std::string, const MemChartBlock*> block_by_id;
    ForEachBlock(m_layout.blocks, [&block_by_id](MemChartBlock& block) {
        block_by_id[block.id] = &block;
    });
    auto find_block = [&block_by_id](const std::string& id) -> const MemChartBlock* {
        std::unordered_map<std::string, const MemChartBlock*>::const_iterator it =
            block_by_id.find(id);
        return it != block_by_id.end() ? it->second : nullptr;
    };
    for(MemChartArrow& arrow : m_layout.arrows)
    {
        arrow.from_block = find_block(arrow.from);
        arrow.to_block   = find_block(arrow.to);
    }

    // Children inherit their top-level ancestor's column/row. PositionBlock does
    // this too, but RebuildColumnGaps and the ComputeLayout pre-pass run first.
    for(MemChartBlock& block : m_layout.blocks)
    {
        ForEachBlock(block.children, [&block](MemChartBlock& child) {
            child.column = block.column;
            child.row    = block.row;
        });
    }

    // Strings first: RebuildColumnGaps reads cached_label to tell labeled gaps
    // from unlabeled ones.
    RefreshMetricStrings();
    RebuildColumnGaps();
    m_layout_dirty = true;
}

void
ComputeMemoryChartView::RebuildColumnGaps()
{
    // Mark each inter-column gap that an arrow crosses. Adjacent-/skip-column
    // arrows are drawn across the gap; same-column arrows use the gutter just
    // right of their column. Runs on layout load only (columns and arrows are
    // static until the next load), so ComputeLayout can reuse the result.
    m_gap_kinds.clear();

    std::set<int32_t> column_set;
    for(const MemChartBlock& block : m_layout.blocks) column_set.insert(block.column);
    std::vector<int32_t> col_keys(column_set.begin(), column_set.end());  // ascending

    int num_gaps = static_cast<int>(col_keys.size()) - 1;
    if(num_gaps <= 0) return;
    m_gap_kinds.assign(static_cast<size_t>(num_gaps), GapKind::kEmpty);

    for(const MemChartArrow& arrow : m_layout.arrows)
    {
        const MemChartBlock* from = arrow.from_block;
        const MemChartBlock* to   = arrow.to_block;
        if(!from || !to) continue;

        // A gap takes the roomiest kind of any arrow crossing it.
        GapKind kind = arrow.cached_label.empty() ? GapKind::kUnlabeledArrow
                                                  : GapKind::kLabeledArrow;
        auto mark = [&](int g) {
            if(g >= 0 && g < num_gaps && m_gap_kinds[g] < kind) m_gap_kinds[g] = kind;
        };

        if(from->row != to->row)
        {
            // Inter-row arrows between columns run their vertical in the gap
            // beside the target, on the source's side; same-column ones need none.
            if(from->column == to->column) continue;
            int t = static_cast<int>(std::lower_bound(col_keys.begin(), col_keys.end(), to->column) -
                                     col_keys.begin());
            mark(from->column < to->column ? t - 1 : t);
            continue;
        }
        int32_t lo = std::min(from->column, to->column);
        int32_t hi = std::max(from->column, to->column);
        for(int g = 0; g < num_gaps; ++g)
        {
            if(lo == hi)
            {
                if(col_keys[g] == lo) mark(g);
            }
            else if(lo <= col_keys[g] && hi >= col_keys[g + 1])
            {
                mark(g);
            }
        }
    }
}

void
ComputeMemoryChartView::RefreshMetricStrings()
{
    ForEachBlock(m_layout.blocks, [this](MemChartBlock& block) {
        for(MemChartContentItem& item : block.content)
        {
            item.cached_label = MetricLabel(item.metric, item.title);
            item.cached_value = MetricValueText(item.metric, true, item.unit);
            item.cached_color_kind =
                ColorKindForCategory(item.category, item.cached_label);
        }
    });

    for(MemChartArrow& arrow : m_layout.arrows)
    {
        arrow.cached_label = arrow.title.empty() ? MetricLabel(arrow.metric, "") : arrow.title;
        // Arrow labels omit the unit (kept compact; full value + unit is in the tooltip).
        arrow.cached_value = MetricValueText(arrow.metric, false);
        const std::string& color_key =
            arrow.cached_label.empty() ? arrow.cached_value : arrow.cached_label;
        arrow.cached_color_kind = ColorKindForCategory(arrow.category, color_key);
    }
    RefreshCachedColors();
}

void
ComputeMemoryChartView::RefreshPalette()
{
    const SettingsManager& s = SettingsManager::GetInstance();
    m_colors.bg         = s.GetColor(Colors::kMemChartBg);
    m_colors.panel      = s.GetColor(Colors::kMemChartPanel);
    m_colors.panel_alt  = s.GetColor(Colors::kMemChartPanelAlt);
    m_colors.border     = s.GetColor(Colors::kMemChartBorder);
    m_colors.border_hot = s.GetColor(Colors::kMemChartBorderHot);
    m_colors.text_main  = s.GetColor(Colors::kMemChartTextMain);
    m_colors.text_dim   = s.GetColor(Colors::kMemChartTextDim);
    m_colors.read       = s.GetColor(Colors::kMemChartRead);
    m_colors.write      = s.GetColor(Colors::kMemChartWrite);
    m_colors.atomic     = s.GetColor(Colors::kMemChartAtomic);
    m_colors.util       = s.GetColor(Colors::kMemChartUtil);
    m_colors.hit        = s.GetColor(Colors::kMemChartHit);
    m_colors.stall      = s.GetColor(Colors::kMemChartStall);
    m_colors.shadow     = s.GetColor(Colors::kMemChartShadow);
}

void
ComputeMemoryChartView::RefreshCachedColors()
{
    ForEachBlock(m_layout.blocks, [this](MemChartBlock& block) {
        for(MemChartContentItem& item : block.content)
        {
            item.cached_color = ColorFromKind(item.cached_color_kind);
        }
    });

    for(MemChartArrow& arrow : m_layout.arrows)
    {
        arrow.cached_color = ColorFromKind(arrow.cached_color_kind);
    }

    // Cached routes hold copied colors, so they must be rebuilt. This also covers
    // metric-string refreshes (which call here) and layout loads.
    m_layout_dirty = true;
}

uint32_t
ComputeMemoryChartView::ColorFromKind(MemChartColorKind kind) const
{
    switch(kind)
    {
    case MemChartColorKind::kRead:   return m_colors.read;
    case MemChartColorKind::kWrite:  return m_colors.write;
    case MemChartColorKind::kAtomic: return m_colors.atomic;
    case MemChartColorKind::kUtil:   return m_colors.util;
    case MemChartColorKind::kHit:    return m_colors.hit;
    case MemChartColorKind::kStall:  return m_colors.stall;
    case MemChartColorKind::kNeutral:
    default:                        return m_colors.text_main;
    }
}

// Category and table ids (leading segments) of a dotted metric id
// "category.table.entry".
static bool
MetricCategoryTable(const MemChartMetricRef& ref, uint32_t& category, uint32_t& table)
{
    if(!ref.valid) return false;
    size_t d1 = ref.name.find('.');
    if(d1 == 0 || d1 == std::string::npos) return false;
    size_t d2 = ref.name.find('.', d1 + 1);
    if(d2 == d1 + 1 || d2 == std::string::npos) return false;

    auto parse = [](const std::string& s, size_t begin, size_t end, uint32_t& out) -> bool {
        out = 0;
        for(size_t i = begin; i < end; ++i)
        {
            char c = s[i];
            if(c < '0' || c > '9') return false;
            out = out * 10 + static_cast<uint32_t>(c - '0');
        }
        return end > begin;
    };
    return parse(ref.name, 0, d1, category) && parse(ref.name, d1 + 1, d2, table);
}

static void
CollectTables(const std::vector<MemChartBlock>&        blocks,
              std::set<std::pair<uint32_t, uint32_t>>& out)
{
    for(const MemChartBlock& block : blocks)
    {
        uint32_t category = 0, table = 0;
        for(const MemChartContentItem& item : block.content)
        {
            if(MetricCategoryTable(item.metric, category, table)) out.insert({category, table});
        }
        CollectTables(block.children, out);
    }
}

void
ComputeMemoryChartView::FetchMemChartMetrics()
{
    m_ptr_by_metric_id.clear();
    RefreshMetricStrings();  // values -> N/A until the fetch completes

    m_data_provider.ComputeModel().ClearKernelMetricValues(m_client_id);

    if(!m_compute_selection) return;

    uint32_t workload_id = m_compute_selection->GetSelectedWorkload();
    uint32_t kernel_id   = m_compute_selection->GetSelectedKernel();

    // Fetch only the specific (category, table) pairs the layout references, not
    // whole categories - these charts are sparse (a handful of metrics per table).
    std::set<std::pair<uint32_t, uint32_t>> tables;
    CollectTables(m_layout.blocks, tables);
    for(const MemChartArrow& arrow : m_layout.arrows)
    {
        uint32_t category = 0, table = 0;
        if(MetricCategoryTable(arrow.metric, category, table)) tables.insert({category, table});
    }
    if(tables.empty()) return;

    std::vector<uint32_t>                       kernel_ids = {kernel_id};
    std::vector<MetricsRequestParams::MetricID> metric_ids;
    for(const std::pair<uint32_t, uint32_t>& ct : tables)
    {
        metric_ids.push_back({ct.first, ct.second, std::nullopt});
    }

    m_data_provider.FetchMetrics(
        MetricsRequestParams(workload_id, kernel_ids, metric_ids, m_client_id));
}

void
ComputeMemoryChartView::UpdateMetrics()
{
    m_ptr_by_metric_id.clear();

    if(m_compute_selection)
    {
        uint32_t kernel_id = m_compute_selection->GetSelectedKernel();
        if(kernel_id != ComputeSelection::INVALID_SELECTION_ID)
        {
            const std::vector<std::shared_ptr<MetricValue>>* metrics =
                m_data_provider.ComputeModel().GetKernelMetricsData(m_client_id, kernel_id);
            if(metrics)
            {
                for(const std::shared_ptr<MetricValue>& metric : *metrics)
                {
                    if(!metric || !metric->entry) continue;
                    // Index each fetched metric by its full dotted id.
                    std::string full_id = std::to_string(metric->entry->category_id) + "." +
                                          std::to_string(metric->entry->table_id) + "." +
                                          std::to_string(metric->entry->id);
                    m_ptr_by_metric_id[full_id] = metric.get();
                }
            }
        }
    }

    // Refresh the cached display strings once now that values are in, instead of
    // recomputing them for every block/arrow on every frame.
    RefreshMetricStrings();
}

const MetricValue*
ComputeMemoryChartView::ResolveMetric(const MemChartMetricRef& ref) const
{
    if(!ref.valid) return nullptr;
    // Layouts address metrics by their full dotted id ("category.table.entry").
    std::unordered_map<std::string, const MetricValue*>::const_iterator it =
        m_ptr_by_metric_id.find(ref.name);
    return it != m_ptr_by_metric_id.end() ? it->second : nullptr;
}

std::string
ComputeMemoryChartView::MetricLabel(const MemChartMetricRef& ref,
                                    const std::string&       title_override) const
{
    if(!title_override.empty()) return title_override;
    const MetricValue* metric = ResolveMetric(ref);
    if(metric && metric->entry) return metric->entry->name;
    if(!ref.name.empty()) return ref.name;
    return "";
}

std::string
ComputeMemoryChartView::MetricValueText(const MemChartMetricRef& ref, bool include_unit,
                                        const std::string& unit_override) const
{
    const MetricValue* metric = ResolveMetric(ref);
    if(!metric || metric->values.empty()) return UNAVAILABLE_METRIC_TEXT;
    std::string text = FormatMetricValue(metric->values.begin()->second);
    if(include_unit && IsAvailableMetricText(text))
    {
        // The layout's unit takes priority; otherwise use the metric entry's unit
        // (the curated Memory Chart table stores % metrics without a unit).
        std::string unit = !unit_override.empty()
                               ? unit_override
                               : (metric->entry ? metric->entry->unit : std::string());
        if(!unit.empty())
        {
            text += " ";
            text += unit;
        }
    }
    return text;
}

void
ComputeMemoryChartView::MeasureBlock(MemChartBlock& block) const
{
    // Container: size from its children stacked inside a titled box.
    if(block.IsContainer())
    {
        float inner_w = 0.0f;
        float inner_h = 0.0f;
        for(size_t k = 0; k < block.children.size(); ++k)
        {
            MeasureBlock(block.children[k]);
            inner_w = std::max(inner_w, block.children[k].w);
            inner_h += block.children[k].h;
            if(k + 1 < block.children.size()) inner_h += GROUP_INNER_GAP;
        }
        float title_w = ImGui::CalcTextSize(block.title.c_str()).x + BLOCK_TEXT_PAD * 2.0f;
        block.w       = std::max(inner_w + GROUP_PAD * 2.0f, title_w);
        block.h       = GROUP_HEADER + GROUP_PAD * 2.0f + inner_h;
        return;
    }

    // Leaf: size from its metric rows.
    float width = ImGui::CalcTextSize(block.title.c_str()).x;
    for(const MemChartContentItem& item : block.content)
    {
        float row_w = ImGui::CalcTextSize(item.cached_label.c_str()).x + METRIC_VALUE_GAP +
                      ImGui::CalcTextSize(item.cached_value.c_str()).x;
        width = std::max(width, row_w);
    }

    width += BLOCK_TEXT_PAD * 2.0f + BLOCK_CONTENT_EXTRA_W;
    // A title-only block has no label/value columns to line up with its
    // neighbors, so hold it to the narrower minimum instead of padding it out
    // to the metric-row width.
    float min_width = block.content.empty() ? MIN_TITLE_ONLY_WIDTH : MIN_BLOCK_WIDTH;
    block.w         = std::min(std::max(width, min_width), MAX_BLOCK_WIDTH);

    // Only row 0 stretches, so a label-only block elsewhere just fits its title.
    if(block.content.empty() && block.row != 0)
    {
        block.h = ImGui::GetTextLineHeight() + LABEL_ONLY_PAD_Y * 2.0f;
        return;
    }

    float body = block.content.empty()
                     ? EMPTY_BODY_H
                     : static_cast<float>(block.content.size()) * ROW_HEIGHT;
    block.h = std::max(HeaderHeight() + BLOCK_BODY_TOP + body + BLOCK_TEXT_PAD,
                       MIN_BLOCK_HEIGHT);
}

void
ComputeMemoryChartView::PositionBlock(MemChartBlock& block, float x, float y, float w,
                                      float h, float conn_l, float conn_r, int32_t column,
                                      int32_t row)
{
    block.x          = x;
    block.y          = y;
    block.w          = w;
    block.h          = h;
    block.conn_left  = conn_l;   // top-level ancestor box edges (passed unchanged)
    block.conn_right = conn_r;
    block.column     = column;   // propagate row/column so arrow routing sees nested blocks
    block.row        = row;

    if(!block.IsContainer()) return;

    MemChartGroupBox box;
    box.title = block.title;
    box.x     = x;
    box.y     = y;
    box.w     = w;
    box.h     = h;
    m_group_boxes.push_back(box);

    // Children were sorted once in OnLayoutLoaded; do not reorder the vector
    // here — the arrows' resolved endpoints point into it.
    float inner_x       = x + GROUP_PAD;
    float inner_w       = w - GROUP_PAD * 2.0f;
    float inner_room    = std::max(h - GROUP_HEADER - GROUP_PAD * 2.0f, 1.0f);
    int   n             = static_cast<int>(block.children.size());
    float inner_gaps    = static_cast<float>(std::max(n - 1, 0)) * GROUP_INNER_GAP;
    float inner_natural = 0.0f;
    for(const MemChartBlock& child : block.children)
    {
        inner_natural += child.h;
    }
    float inner_scale =
        inner_natural > 0.0f ? std::max(inner_room - inner_gaps, 1.0f) / inner_natural : 1.0f;

    float cy = y + GROUP_HEADER + GROUP_PAD;
    for(MemChartBlock& child : block.children)
    {
        float ch = child.h * inner_scale;
        PositionBlock(child, inner_x, cy, inner_w, ch, conn_l, conn_r, column, row);
        cy += ch + GROUP_INNER_GAP;
    }
}

// Caption for an arrow, from its cached label/value (refreshed on load and on
// metric fetch, not per frame). An unnamed arrow (no title and no resolvable
// metric) has nothing to caption, so it stays unlabelled instead of reading "N/A".
static std::string
ArrowLabelText(const MemChartArrow& arrow)
{
    return arrow.cached_label.empty() ? std::string()
                                      : arrow.cached_label + ": " + arrow.cached_value;
}

// A highway label sits above its lane's line, so the pitch between lanes must
// exceed the label height - otherwise a lane's label lands on the line above it.
static float
SkipLanePitch()
{
    return std::max(LANE_GAP,
                    ImGui::GetTextLineHeight() + ARROW_LABEL_ABOVE + SKIP_LANE_LABEL_PAD);
}

// Pitch between fanned arrows or stacked labels: one label box plus a gap.
static float
ArrowFanPitch()
{
    return ImGui::GetTextLineHeight() + LABEL_PAD_Y * 2.0f + LABEL_STACK_GAP;
}

using BlockPair = std::pair<const MemChartBlock*, const MemChartBlock*>;

// Order-independent key for the pair of blocks an arrow joins. std::less gives
// a total order even for blocks in different (nested) child vectors.
static BlockPair
BlockPairKey(const MemChartBlock& a, const MemChartBlock& b)
{
    return std::less<const MemChartBlock*>()(&a, &b) ? BlockPair(&a, &b) : BlockPair(&b, &a);
}

// Vertical room `lanes` highway lanes occupy below their row band.
static float
SkipLanesReserve(int lanes)
{
    return lanes > 0 ? SKIP_HIGHWAY_DROP + static_cast<float>(lanes) * SkipLanePitch() : 0.0f;
}

void
ComputeMemoryChartView::CollectSkipSpans(
    const std::map<int32_t, float>& col_mid_x,
    std::map<int32_t, std::vector<SkipSpan>>& spans_by_row) const
{
    const float margin_x = CHART_PADDING + LEFT_MARGIN * SKIP_MARGIN_RATIO;
    auto mid_x = [&col_mid_x](int32_t column) -> float {
        std::map<int32_t, float>::const_iterator it = col_mid_x.find(column);
        return it != col_mid_x.end() ? it->second : 0.0f;
    };

    for(size_t i = 0; i < m_layout.arrows.size(); ++i)
    {
        const MemChartArrow& arrow = m_layout.arrows[i];
        const MemChartBlock* from  = arrow.from_block;
        const MemChartBlock* to    = arrow.to_block;
        if(!from || !to || from->row != to->row) continue;
        int32_t dcol = to->column - from->column;
        if(dcol >= -1 && dcol <= 1) continue;
        float a = mid_x(from->column);
        float b = to->column < from->column ? margin_x : mid_x(to->column);
        spans_by_row[from->row].push_back({i, std::min(a, b), std::max(a, b), 0});
    }
}

int
ComputeMemoryChartView::PackSkipLanes(std::vector<SkipSpan>& spans)
{
    // Shortest spans first so they take the inner lanes; ties left-to-right.
    std::stable_sort(spans.begin(), spans.end(), [](const SkipSpan& a, const SkipSpan& b) {
        float wa = a.hi - a.lo;
        float wb = b.hi - b.lo;
        if(wa != wb) return wa < wb;
        return a.lo < b.lo;
    });

    // First-fit packing: reuse the lowest lane clear of this span, else open one.
    std::vector<std::vector<const SkipSpan*>> lanes;
    for(SkipSpan& span : spans)
    {
        int lane = 0;
        for(; lane < static_cast<int>(lanes.size()); ++lane)
        {
            bool clear = true;
            for(const SkipSpan* other : lanes[lane])
            {
                if(span.lo < other->hi + SKIP_LANE_PAD && other->lo < span.hi + SKIP_LANE_PAD)
                {
                    clear = false;
                    break;
                }
            }
            if(clear) break;
        }
        if(lane == static_cast<int>(lanes.size())) lanes.emplace_back();
        span.lane = lane;
        lanes[lane].push_back(&span);
    }
    return static_cast<int>(lanes.size());
}

void
ComputeMemoryChartView::ComputeLayout(float available_width)
{
    m_group_boxes.clear();

    for(MemChartBlock& block : m_layout.blocks)
    {
        MeasureBlock(block);
    }

    // Grow each block so its fanned connectors and labels fit. Entry- and
    // exit-side arrows sit in separate corridors, so a block only needs to fit
    // the busier side. Counts must match the ports BuildArrowRoutes actually
    // fans, or fan_y silently compresses them. Top-level blocks only.
    {
        // "Stacked" is judged within a row band, so count blocks per (row, column).
        std::map<std::pair<int32_t, int32_t>, int> cell_counts;
        ForEachBlock(m_layout.blocks, [&](const MemChartBlock& block) {
            cell_counts[{block.row, block.column}]++;
        });

        std::map<const MemChartBlock*, int> entry_anchored;  // arrows entering from the left
        std::map<const MemChartBlock*, int> exit_anchored;   // arrows leaving to the right
        for(const MemChartArrow& arrow : m_layout.arrows)
        {
            const MemChartBlock* from = arrow.from_block;
            const MemChartBlock* to   = arrow.to_block;
            if(!from || !to) continue;
            if(from->row != to->row) continue;    // inter-row arrows fan vertically, not here
            int32_t dcol = to->column - from->column;
            // Same column: the arrow leaves and re-enters the right edge, so
            // both of its ends claim an exit port on their own block.
            if(dcol == 0)
            {
                exit_anchored[from]++;
                exit_anchored[to]++;
                continue;
            }
            if(dcol != 1 && dcol != -1) continue;  // skip-column arrows use the highways
            const MemChartBlock* left  = from->column < to->column ? from : to;
            const MemChartBlock* right = from->column < to->column ? to : from;
            if(cell_counts[{right->row, right->column}] > 1)
                entry_anchored[right]++;
            else
                exit_anchored[left]++;
        }

        // Height to fan n arrows, one label slot each.
        const float fan_pitch = ArrowFanPitch();
        auto required_arrow_height = [fan_pitch](int n) -> float {
            if(n < 2) return 0.0f;
            return HeaderHeight() + BLOCK_BODY_TOP + static_cast<float>(n) * fan_pitch +
                   BLOCK_TEXT_PAD;
        };

        for(MemChartBlock& block : m_layout.blocks)
        {
            int n    = std::max(entry_anchored[&block], exit_anchored[&block]);
            block.h  = std::max(block.h, required_arrow_height(n));
        }
    }

    // Bucket by column (for the shared width) and by (row, column) cell (for
    // stacking). Sibling order was fixed in OnLayoutLoaded, so push_back order
    // is already the stack order.
    std::map<int32_t, std::vector<MemChartBlock*>>                    columns;
    std::map<int32_t, std::map<int32_t, std::vector<MemChartBlock*>>> cells;  // cells[row][column]
    for(MemChartBlock& block : m_layout.blocks)
    {
        columns[block.column].push_back(&block);
        cells[block.row][block.column].push_back(&block);
    }

    // Column width = widest block in the column across ALL rows, so the column
    // keeps one x-span in every row (columns align even at different densities).
    std::vector<int32_t> col_keys;
    std::vector<float>   column_widths;
    for(std::pair<const int32_t, std::vector<MemChartBlock*>>& column : columns)
    {
        std::vector<MemChartBlock*>& blocks = column.second;
        float col_width = 0.0f;
        for(const MemChartBlock* block : blocks)
        {
            col_width = std::max(col_width, block->w);
        }
        col_keys.push_back(column.first);
        column_widths.push_back(col_width);
    }

    int num_gaps = static_cast<int>(col_keys.size()) - 1;

    // Per-gap spacing: gaps with labeled arrows get the full width; gaps crossed
    // only by unlabeled arrows just need room for the heads; arrow-free gaps stay
    // tightest so unconnected columns don't leave a large empty corridor. The
    // gap kinds are precomputed on layout load (m_gap_kinds), in the same
    // ascending column order rebuilt here.
    auto gap_kind = [&](int g) {
        return g >= 0 && g < static_cast<int>(m_gap_kinds.size()) ? m_gap_kinds[g]
                                                                  : GapKind::kEmpty;
    };

    std::vector<float> col_gaps(static_cast<size_t>(std::max(num_gaps, 0)), COLUMN_GAP_NO_ARROW);
    for(int g = 0; g < num_gaps; ++g)
    {
        switch(gap_kind(g))
        {
        case GapKind::kLabeledArrow:   col_gaps[g] = COLUMN_GAP; break;
        case GapKind::kUnlabeledArrow: col_gaps[g] = COLUMN_GAP_UNLABELED; break;
        case GapKind::kEmpty:          break;
        }
    }

    // Labels are centred in the gap their arrow runs through (adjacent-column
    // arrows, and inter-row elbows beside their target), so a gap must fit its
    // widest label box - which scales with the font - plus clearance for heads.
    for(const MemChartArrow& arrow : m_layout.arrows)
    {
        const MemChartBlock* from = arrow.from_block;
        const MemChartBlock* to   = arrow.to_block;
        if(!from || !to || from->column == to->column) continue;
        int32_t dcol = to->column - from->column;
        if(from->row == to->row && dcol != 1 && dcol != -1) continue;  // highway label

        int g = -1;
        if(from->row == to->row)
        {
            g = static_cast<int>(std::lower_bound(col_keys.begin(), col_keys.end(),
                                                  std::min(from->column, to->column)) -
                                 col_keys.begin());
        }
        else
        {
            int t = static_cast<int>(std::lower_bound(col_keys.begin(), col_keys.end(), to->column) -
                                     col_keys.begin());
            g     = from->column < to->column ? t - 1 : t;
        }
        if(g < 0 || g >= num_gaps) continue;

        std::string text = ArrowLabelText(arrow);
        if(text.empty()) continue;
        float box_w = ImGui::CalcTextSize(text.c_str()).x + LABEL_MARKER_SIZE + LABEL_MARKER_GAP +
                      LABEL_PAD_X * 2.0f;
        col_gaps[g] = std::max(col_gaps[g], box_w + GAP_LABEL_CLEARANCE * 2.0f);
    }

    float blocks_w = 0.0f;
    for(float w : column_widths)
    {
        blocks_w += w;
    }
    float base_gap_sum = 0.0f;
    for(float gw : col_gaps)
    {
        base_gap_sum += gw;
    }
    float natural_w = CHART_PADDING + LEFT_MARGIN + blocks_w + base_gap_sum + CHART_PADDING;

    // Spread leftover width only into labeled-arrow gaps (capped) so the chart
    // fills wide panels without stretching the tight gaps back open.
    int labeled_gap_count = 0;
    for(int g = 0; g < num_gaps; ++g)
    {
        if(gap_kind(g) == GapKind::kLabeledArrow) ++labeled_gap_count;
    }
    if(labeled_gap_count > 0 && available_width > natural_w)
    {
        float extra = (available_width - natural_w) / static_cast<float>(labeled_gap_count);
        for(int g = 0; g < num_gaps; ++g)
        {
            if(gap_kind(g) == GapKind::kLabeledArrow)
                col_gaps[g] = std::max(col_gaps[g], std::min(col_gaps[g] + extra, MAX_COLUMN_GAP));
        }
    }

    // Left x + width per column id, shared by every row so columns stay aligned.
    std::map<int32_t, float> col_x;
    std::map<int32_t, float> col_w;
    {
        float cursor_x = CHART_PADDING + LEFT_MARGIN;
        for(size_t i = 0; i < col_keys.size(); ++i)
        {
            col_x[col_keys[i]] = cursor_x;
            col_w[col_keys[i]] = column_widths[i];
            cursor_x += column_widths[i] + (i < col_gaps.size() ? col_gaps[i] : 0.0f);
        }
    }

    // Natural stacked height of one (row, column) cell.
    auto cell_stack_height = [](const std::vector<MemChartBlock*>& blocks) -> float {
        float sum = static_cast<float>(std::max<int>(blocks.size(), 1) - 1) * BLOCK_GAP;
        for(const MemChartBlock* block : blocks)
        {
            sum += block->h;
        }
        return sum;
    };

    // Row 0 is the tall main band (floored at MIN_TARGET_HEIGHT, later stretched
    // to fill); other rows hug their natural content height.
    std::map<int32_t, float> row_height;
    for(std::pair<const int32_t, std::map<int32_t, std::vector<MemChartBlock*>>>& row : cells)
    {
        float natural = 0.0f;
        for(std::pair<const int32_t, std::vector<MemChartBlock*>>& cell : row.second)
        {
            natural = std::max(natural, cell_stack_height(cell.second));
        }
        row_height[row.first] = (row.first == 0) ? std::max(natural, MIN_TARGET_HEIGHT) : natural;
    }

    // Skip-column highways run just below their own row band, so the corridor
    // under a band must also hold that band's highway lanes.
    std::map<int32_t, float> row_skip_reserve;
    {
        std::map<int32_t, float> col_mid_x;
        for(const std::pair<const int32_t, float>& column : col_x)
        {
            col_mid_x[column.first] = column.second + col_w[column.first] * 0.5f;
        }
        std::map<int32_t, std::vector<SkipSpan>> spans_by_row;
        CollectSkipSpans(col_mid_x, spans_by_row);
        for(std::pair<const int32_t, std::vector<SkipSpan>>& row : spans_by_row)
        {
            row_skip_reserve[row.first] = SkipLanesReserve(PackSkipLanes(row.second));
        }
    }

    // Corridor below each band: fits the label stack of same-column links to the
    // next band and the turns of cross-column elbows leaving through it.
    std::vector<int32_t> row_keys;  // ascending, one per band
    for(const std::pair<const int32_t, float>& row : row_height)
    {
        row_keys.push_back(row.first);
    }
    std::vector<float> corridor_h(row_keys.size(), INTER_ROW_MIN_GAP);
    {
        auto band_of = [&row_keys](int32_t row) -> int {
            return static_cast<int>(std::lower_bound(row_keys.begin(), row_keys.end(), row) -
                                    row_keys.begin());
        };

        std::map<BlockPair, int>                             stack_counts;  // labelled links per block pair
        std::map<std::pair<const MemChartBlock*, bool>, int> side_counts;   // elbow ports per (target, enters-right)
        for(const MemChartArrow& arrow : m_layout.arrows)
        {
            const MemChartBlock* from = arrow.from_block;
            const MemChartBlock* to   = arrow.to_block;
            if(!from || !to || from->row == to->row) continue;
            if(from->column != to->column)
            {
                side_counts[{to, from->column > to->column}]++;
            }
            else if(!arrow.cached_label.empty())
            {
                stack_counts[BlockPairKey(*from, *to)]++;
            }
        }

        const float fan_pitch = ArrowFanPitch();
        for(const std::pair<const BlockPair, int>& stack : stack_counts)
        {
            int band_a = band_of(stack.first.first->row);
            int band_b = band_of(stack.first.second->row);
            int upper  = std::min(band_a, band_b);
            if(std::max(band_a, band_b) != upper + 1) continue;  // label lands in a band, not a corridor
            float stack_h = static_cast<float>(stack.second) * fan_pitch - LABEL_STACK_GAP;
            corridor_h[upper] = std::max(corridor_h[upper], stack_h + INTER_ROW_LABEL_PAD * 2.0f);
        }

        // Elbow runs stub out one ROW_VERT_STEP further per port on their target.
        for(const MemChartArrow& arrow : m_layout.arrows)
        {
            const MemChartBlock* from = arrow.from_block;
            const MemChartBlock* to   = arrow.to_block;
            if(!from || !to || from->row == to->row || from->column == to->column) continue;
            int   ports    = side_counts[{to, from->column > to->column}];
            int   src      = band_of(from->row);
            int   corridor = to->row > from->row ? src : src - 1;
            float run_h    = ROW_ELBOW_STUB * 2.0f + static_cast<float>(ports - 1) * ROW_VERT_STEP;
            corridor_h[corridor] = std::max(corridor_h[corridor], run_h);
        }
    }

    // Stack bands top-to-bottom (negative rows above row 0): each band, then its
    // highway lanes, then its corridor.
    std::map<int32_t, float> row_y;
    {
        float cursor_y = CHART_PADDING;
        for(size_t band = 0; band < row_keys.size(); ++band)
        {
            int32_t row = row_keys[band];
            row_y[row]  = cursor_y;
            cursor_y += row_height[row] + row_skip_reserve[row] + corridor_h[band];
        }
    }

    // Row 0 stretches its stack to fill the band (so horizontal arrows stay
    // level); other rows keep natural heights, anchored to the band top.
    for(std::pair<const int32_t, std::map<int32_t, std::vector<MemChartBlock*>>>& row : cells)
    {
        int32_t r      = row.first;
        float   band_y = row_y[r];
        float   band_h = row_height[r];
        for(std::pair<const int32_t, std::vector<MemChartBlock*>>& cell : row.second)
        {
            int32_t                      c      = cell.first;
            std::vector<MemChartBlock*>& blocks = cell.second;
            float                        cx     = col_x[c];
            float                        cw     = col_w[c];

            float gaps = static_cast<float>(std::max<int>(blocks.size(), 1) - 1) * BLOCK_GAP;
            float natural_sum = 0.0f;
            for(const MemChartBlock* block : blocks)
            {
                natural_sum += block->h;
            }

            float scale = 1.0f;
            if(r == 0 && natural_sum > 0.0f)
            {
                float room = std::max(band_h - gaps, 1.0f);
                scale      = room / natural_sum;
            }

            float y = band_y;
            for(MemChartBlock* block : blocks)
            {
                float bh = block->h * scale;
                PositionBlock(*block, cx, y, cw, bh, cx, cx + cw, c, r);
                y += bh + BLOCK_GAP;
            }
        }
    }
}

void
ComputeMemoryChartView::Render()
{
    float available_width = ImGui::GetContentRegionAvail().x;
    float font_size       = ImGui::GetFontSize();

    // Recompute geometry only when an input to it changes; otherwise reuse the
    // cached blocks/routes/canvas from the last frame and just redraw them.
    if(m_layout_dirty || available_width != m_cached_layout_width ||
       font_size != m_cached_font_size)
    {
        ComputeLayout(available_width);
        BuildArrowRoutes(m_routes);
        ResolveLabelOverlaps(m_routes);

        float max_right  = 0.0f;
        float max_bottom = 0.0f;
        for(const MemChartBlock& block : m_layout.blocks)
        {
            max_right  = std::max(max_right, block.Right());
            max_bottom = std::max(max_bottom, block.Bottom());
        }
        for(const MemChartGroupBox& box : m_group_boxes)
        {
            max_right  = std::max(max_right, box.x + box.w);
            max_bottom = std::max(max_bottom, box.y + box.h);
        }
        // Skip-lanes and labels sit below the blocks, so fold them into the height.
        for(const ArrowRoute& route : m_routes)
        {
            for(const std::pair<float, float>& p : route.points)
            {
                max_bottom = std::max(max_bottom, p.second + CANVAS_BOTTOM_PAD);
            }
            max_bottom = std::max(max_bottom, route.label_y + route.label_h + CANVAS_BOTTOM_PAD);
        }

        m_canvas_w            = max_right + CHART_PADDING;
        m_canvas_h            = max_bottom + CHART_PADDING * 2.0f + LEGEND_HEIGHT;
        m_cached_layout_width = available_width;
        m_cached_font_size    = font_size;
        m_layout_dirty        = false;
    }

    float canvas_w = m_canvas_w;
    float canvas_h = m_canvas_h;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, m_colors.bg);
    ImGui::BeginChild("MemoryChart", ImVec2(0, canvas_h), ImGuiChildFlags_None,
                      ImGuiWindowFlags_HorizontalScrollbar);
    ImGui::PopStyleColor(1);

    ImDrawList* draw_list       = ImGui::GetWindowDrawList();
    ImVec2      window_position = ImGui::GetCursorScreenPos();
    float       backdrop_w =
        std::max(canvas_w, ImGui::GetContentRegionAvail().x + ImGui::GetScrollX());
    draw_list->AddRectFilled(window_position,
                             {window_position.x + backdrop_w, window_position.y + canvas_h},
                             m_colors.bg);

    // Arrows first, then group boxes (containers), then blocks on top.
    DrawArrowRoutes(draw_list, window_position, m_routes);
    for(const MemChartGroupBox& box : m_group_boxes)
    {
        DrawGroupBox(draw_list, {window_position.x + box.x, window_position.y + box.y},
                     box.w, box.h, box.title.c_str());
    }
    for(const MemChartBlock& block : m_layout.blocks)
    {
        DrawBlock(draw_list, window_position, block);
    }

    DrawLegend(draw_list, window_position, canvas_h - CHART_PADDING - LEGEND_HEIGHT);

    const float h_scrollbar_size =
        (ImGui::GetScrollMaxX() > 0.0f) ? ImGui::GetStyle().ScrollbarSize : 0.0f;
    ImGui::SetCursorPos(ImVec2(canvas_w, canvas_h - 1.0f - h_scrollbar_size));
    ImGui::Dummy(ImVec2(1, 1));

    ImGui::EndChild();
}

void
ComputeMemoryChartView::DrawBlock(ImDrawList* draw_list, ImVec2 origin,
                                  const MemChartBlock& block)
{
    // Container: its box is drawn from m_group_boxes; just draw the children.
    if(block.IsContainer())
    {
        for(const MemChartBlock& child : block.children)
        {
            DrawBlock(draw_list, origin, child);
        }
        return;
    }
    DrawLeaf(draw_list, origin, block);
}

void
ComputeMemoryChartView::DrawLeaf(ImDrawList* draw_list, ImVec2 origin,
                                 const MemChartBlock& block)
{
    float block_x = origin.x + block.x;
    float block_y = origin.y + block.y;

    DrawBlockRect(draw_list, {block_x, block_y}, {block_x + block.w, block_y + block.h});

    // Label-only block (no metrics): just center the name in the box.
    if(block.content.empty())
    {
        ImVec2 text_size = ImGui::CalcTextSize(block.title.c_str());
        draw_list->AddText({block_x + (block.w - text_size.x) * 0.5f,
                            block_y + (block.h - text_size.y) * 0.5f},
                           m_colors.text_main, block.title.c_str());
        return;
    }

    float cursor_y =
        DrawBlockHeader(draw_list, block.title.c_str(), block_x, block_y, block.w);
    cursor_y += BLOCK_BODY_TOP;

    for(const MemChartContentItem& item : block.content)
    {
        const std::string& label  = item.cached_label;
        const std::string& value  = item.cached_value;
        ImU32              accent = item.cached_color;

        ImVec2 row_min(block_x + ROW_INSET_X, cursor_y - ROW_HOVER_INSET);
        ImVec2 row_max(block_x + block.w - ROW_INSET_X, cursor_y + ROW_HEIGHT - ROW_HOVER_INSET);
        if(ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows |
                                  ImGuiHoveredFlags_NoPopupHierarchy) &&
           ImGui::IsMouseHoveringRect(row_min, row_max))
        {
            draw_list->AddRectFilled(row_min, row_max, ApplyAlpha(m_colors.border_hot, ROW_HOVER_ALPHA),
                                     ROW_HOVER_ROUNDING);
        }

        draw_list->AddRectFilled(
            {block_x + BLOCK_TEXT_PAD, cursor_y + ROW_ACCENT_TOP},
            {block_x + BLOCK_TEXT_PAD + ROW_ACCENT_WIDTH, cursor_y + ROW_ACCENT_BOTTOM},
            ApplyAlpha(accent, ROW_ACCENT_ALPHA), ROW_ACCENT_ROUNDING);
        std::string label_text = label + ":";
        DrawTextWithTooltip(draw_list, {block_x + BLOCK_TEXT_PAD + ROW_LABEL_INDENT, cursor_y},
                            m_colors.text_dim, label_text.c_str(), item.metric, true, false);

        bool  available = IsAvailableMetricText(value);
        float value_w   = ImGui::CalcTextSize(value.c_str()).x;
        float value_x   = block_x + block.w - BLOCK_TEXT_PAD - value_w;
        ImU32 value_col = available ? accent : m_colors.text_dim;
        DrawTextWithTooltip(draw_list, {value_x, cursor_y}, value_col, value.c_str(),
                            item.metric, false, true);

        cursor_y += ROW_HEIGHT;
    }
}

void
ComputeMemoryChartView::BuildArrowRoutes(std::vector<ArrowRoute>& routes) const
{
    routes.clear();

    // Row band extents (skip highways run below their band; inter-row elbows turn
    // in the corridor beyond it) and column x-spans (shared by every row).
    std::map<int32_t, float> row_top;
    std::map<int32_t, float> row_bottom;
    std::map<int32_t, float> col_left;
    std::map<int32_t, float> col_right;
    for(const MemChartBlock& block : m_layout.blocks)
    {
        std::map<int32_t, float>::iterator top = row_top.find(block.row);
        row_top[block.row]    = top == row_top.end() ? block.y : std::min(top->second, block.y);
        row_bottom[block.row] = std::max(row_bottom[block.row], block.Bottom());
        std::map<int32_t, float>::iterator left = col_left.find(block.column);
        col_left[block.column] =
            left == col_left.end() ? block.x : std::min(left->second, block.x);
        col_right[block.column] = std::max(col_right[block.column], block.Right());
    }
    std::map<int32_t, float> col_mid_x;
    for(const std::pair<const int32_t, float>& column : col_left)
    {
        col_mid_x[column.first] = (column.second + col_right[column.first]) * 0.5f;
    }

    // Skip highway lanes, packed per row band exactly as ComputeLayout reserved them.
    std::unordered_map<size_t, int> lane_of;
    std::map<int32_t, float>        row_skip_reserve;
    {
        std::map<int32_t, std::vector<SkipSpan>> spans_by_row;
        CollectSkipSpans(col_mid_x, spans_by_row);
        for(std::pair<const int32_t, std::vector<SkipSpan>>& row : spans_by_row)
        {
            row_skip_reserve[row.first] = SkipLanesReserve(PackSkipLanes(row.second));
            for(const SkipSpan& span : row.second)
            {
                lane_of[span.index] = span.lane;
            }
        }
    }

    std::vector<size_t> adjacent;
    std::vector<size_t> same_column;
    std::vector<size_t> inter_row;
    std::vector<size_t> skipping;

    // True when another block sits in the same column between two inter-row
    // endpoints, so a straight vertical would run through it.
    auto blocked_between = [&](const MemChartBlock* a, const MemChartBlock* b) -> bool {
        if(a->column != b->column) return false;
        const MemChartBlock* up = a->y <= b->y ? a : b;
        const MemChartBlock* lo = a->y <= b->y ? b : a;
        for(const MemChartBlock& block : m_layout.blocks)
        {
            if(block.column != a->column || &block == a || &block == b) continue;
            float mid = block.MidY();
            if(mid > up->Bottom() && mid < lo->y) return true;
        }
        return false;
    };

    for(size_t i = 0; i < m_layout.arrows.size(); ++i)
    {
        const MemChartArrow& arrow = m_layout.arrows[i];
        const MemChartBlock* from  = arrow.from_block;
        const MemChartBlock* to    = arrow.to_block;
        if(!from || !to) continue;

        int32_t dcol = to->column - from->column;
        if(from->row != to->row)
        {
            // Different row bands: a clean vertical connector, unless another
            // block sits between them in the column - then detour through the
            // side gutter (same-column routing) so the line doesn't cross it.
            if(blocked_between(from, to))
                same_column.push_back(i);
            else
                inter_row.push_back(i);
        }
        else if(dcol == 1 || dcol == -1)
        {
            adjacent.push_back(i);
        }
        else if(dcol == 0)
        {
            same_column.push_back(i);
        }
        else
        {
            skipping.push_back(i);
        }
    }

    routes.reserve(m_layout.arrows.size());

    // Vertical position of the k-th of `count` ports inside a block's body.
    const float fan_pitch = ArrowFanPitch();
    auto fan_y = [](const MemChartBlock& b, float frac, int count, float pitch) -> float {
        // A label-only block has no header/body split (its title is centred in the
        // whole box), so span the full box; otherwise ports sit in the body below
        // the header.
        bool  label_only = b.content.empty() && !b.IsContainer();
        float lo = b.y + (label_only ? BLOCK_TEXT_PAD : HeaderHeight() + BLOCK_BODY_TOP);
        float hi = b.Bottom() - BLOCK_TEXT_PAD;
        if(hi <= lo) return b.MidY();
        float avail   = hi - lo;
        float desired = static_cast<float>(std::max(count - 1, 0)) * pitch;
        float spread  = std::min(avail, desired);
        float center  = (lo + hi) * 0.5f;
        return center + (frac - 0.5f) * spread;
    };

    auto make_route = [&](const MemChartArrow&                  arrow,
                          std::vector<std::pair<float, float>> pts, float label_x,
                          float label_y) {
        ArrowRoute route;
        route.points = std::move(pts);
        route.metric = arrow.metric;

        route.label = ArrowLabelText(arrow);
        route.color = arrow.cached_color;

        // Which endpoint(s) get a head: the destination of the flow.
        bool head_from = arrow.direction == MemChartArrowDir::kBoth ||
                         arrow.direction == MemChartArrowDir::kBackward;
        bool head_to = arrow.direction == MemChartArrowDir::kBoth ||
                       arrow.direction == MemChartArrowDir::kForward;
        route.head_at_first = head_from;  // points[0] sits at `from`
        route.head_at_last  = head_to;    // points.back() sits at `to`

        // Label marker points along the flow, snapped to the dominant axis.
        if(route.points.size() >= 2 && (head_from || head_to))
        {
            float dx = route.points.back().first - route.points.front().first;
            float dy = route.points.back().second - route.points.front().second;
            if(head_from && !head_to)  // reversed: head sits at points[0]
            {
                dx = -dx;
                dy = -dy;
            }
            if(std::fabs(dx) >= std::fabs(dy))
            {
                route.label_dir_x = dx >= 0.0f ? 1.0f : -1.0f;
                route.label_dir_y = 0.0f;
            }
            else
            {
                route.label_dir_x = 0.0f;
                route.label_dir_y = dy >= 0.0f ? 1.0f : -1.0f;
            }
            route.label_bidir = head_from && head_to;
        }

        ImVec2 size =
            route.label.empty() ? ImVec2(0.0f, 0.0f) : ImGui::CalcTextSize(route.label.c_str());
        bool  has_marker = !route.label.empty() &&
                          (route.label_dir_x != 0.0f || route.label_dir_y != 0.0f);
        float marker_w   = has_marker ? (LABEL_MARKER_SIZE + LABEL_MARKER_GAP) : 0.0f;
        route.label_w = size.x + marker_w;
        route.label_h = size.y;
        route.label_x = label_x - route.label_w * 0.5f;
        route.label_y = label_y - size.y - ARROW_LABEL_ABOVE;
        routes.push_back(std::move(route));
    };

    // Assign ordered connection ports on each block edge to minimize crossings.
    // Every arrow leaving a block's right edge is sorted by its destination's
    // vertical position (and vice-versa for the left edge), so lines fan out in a
    // consistent top-to-bottom order instead of tangling.
    struct Port
    {
        size_t arrow;
        float  key;      // The other endpoint's mid-Y, used for ordering.
        bool   at_from;  // Which end of `arrow` this port belongs to.
    };
    // Keyed by block; a same-column arrow contributes a port at both of its
    // ends, so ports are identified by (arrow, endpoint) rather than arrow alone.
    using PortKey   = std::pair<size_t, bool>;
    using EdgePorts = std::map<const MemChartBlock*, std::vector<Port>>;
    EdgePorts exit_ports;   // right-edge ports
    EdgePorts entry_ports;  // left-edge ports
    for(size_t i : adjacent)
    {
        const MemChartArrow& arrow  = m_layout.arrows[i];
        const MemChartBlock* from   = arrow.from_block;
        const MemChartBlock* to     = arrow.to_block;
        if(!from || !to) continue;
        const MemChartBlock* left_b  = from->column < to->column ? from : to;
        const MemChartBlock* right_b = from->column < to->column ? to : from;
        exit_ports[left_b].push_back({i, right_b->MidY(), from == left_b});
        entry_ports[right_b].push_back({i, left_b->MidY(), from == right_b});
    }
    // Same-column arrows leave and re-enter on the right edge, so both ends
    // compete for right-edge ports alongside the adjacent-column arrows.
    for(size_t i : same_column)
    {
        const MemChartArrow& arrow = m_layout.arrows[i];
        const MemChartBlock* from  = arrow.from_block;
        const MemChartBlock* to    = arrow.to_block;
        if(!from || !to) continue;
        exit_ports[from].push_back({i, to->MidY(), true});
        exit_ports[to].push_back({i, from->MidY(), false});
    }

    std::map<PortKey, float> exit_y;
    std::map<PortKey, float> entry_y;
    auto assign_ports = [&](EdgePorts& edge, std::map<PortKey, float>& out) {
        for(std::pair<const MemChartBlock* const, std::vector<Port>>& kv : edge)
        {
            const MemChartBlock* block = kv.first;
            std::vector<Port>& ports = kv.second;
            std::stable_sort(ports.begin(), ports.end(),
                             [](const Port& a, const Port& b) { return a.key < b.key; });
            int n = static_cast<int>(ports.size());
            for(int k = 0; k < n; ++k)
            {
                float frac = n > 1 ? static_cast<float>(k) / static_cast<float>(n - 1)
                                   : 0.5f;
                out[PortKey(ports[k].arrow, ports[k].at_from)] = fan_y(*block, frac, n, fan_pitch);
            }
        }
    };
    assign_ports(exit_ports, exit_y);
    assign_ports(entry_ports, entry_y);

    auto port_y = [](const std::map<PortKey, float>& ports, const PortKey& key,
                     float fallback) -> float {
        std::map<PortKey, float>::const_iterator it = ports.find(key);
        return it != ports.end() ? it->second : fallback;
    };

    // How many blocks share each (row, column) cell (a "stacked" cell has > 1),
    // counting nested blocks too (their row/column were propagated during layout).
    std::map<std::pair<int32_t, int32_t>, int> cell_counts;
    ForEachBlock(m_layout.blocks, [&](const MemChartBlock& block) {
        cell_counts[{block.row, block.column}]++;
    });

    for(size_t i : adjacent)
    {
        const MemChartArrow& arrow  = m_layout.arrows[i];
        const MemChartBlock* from   = arrow.from_block;
        const MemChartBlock* to     = arrow.to_block;
        if(!from || !to) continue;
        const MemChartBlock* left_b  = from->column < to->column ? from : to;
        const MemChartBlock* right_b = from->column < to->column ? to : from;

        // Since every column shares the same height, we can keep arrows perfectly
        // horizontal: anchor the row on the "stacked" endpoint (the block whose
        // exact vertical position matters); the full-height neighbor accepts any
        // row. Horizontal lines in a corridor are parallel, so they never cross.
        bool  right_stacked = cell_counts[{right_b->row, right_b->column}] > 1;
        float arrow_y =
            right_stacked
                ? port_y(entry_y, PortKey(i, from == right_b), right_b->MidY())
                : port_y(exit_y, PortKey(i, from == left_b), left_b->MidY());

        // Attach at the connection edges (the group box edge for grouped blocks),
        // so arrows stop at the box rather than entering it.
        float lx = left_b->conn_right;
        float rx = right_b->conn_left;

        std::vector<std::pair<float, float>> pts;
        if(from == left_b)
        {
            pts = {{lx, arrow_y}, {rx, arrow_y}};
        }
        else
        {
            pts = {{rx, arrow_y}, {lx, arrow_y}};
        }

        make_route(arrow, std::move(pts), (lx + rx) * 0.5f, arrow_y);
    }

    // Same column: route through the right-hand gutter in stacked lanes.
    struct SameColRoute
    {
        size_t  index;
        int32_t column;
        float   base_x;
        float   from_y;
        float   to_y;
        int     lane;
    };
    std::vector<SameColRoute> same_routes;
    same_routes.reserve(same_column.size());
    for(size_t index : same_column)
    {
        const MemChartArrow& arrow = m_layout.arrows[index];
        const MemChartBlock* from  = arrow.from_block;
        const MemChartBlock* to    = arrow.to_block;
        if(!from || !to) continue;
        same_routes.push_back({index, from->column,
                               std::max(from->conn_right, to->conn_right),
                               port_y(exit_y, PortKey(index, true), from->MidY()),
                               port_y(exit_y, PortKey(index, false), to->MidY()), 0});
    }

    // First-fit packing per gutter: reuse the innermost lane whose occupants
    // clear this arrow vertically, else open a new one. Arrows heading opposite
    // ways out of the same block never overlap, so they share a lane and their
    // verticals stay aligned instead of each claiming its own offset.
    std::map<int32_t, std::vector<std::vector<const SameColRoute*>>> same_lanes;
    for(SameColRoute& route : same_routes)
    {
        std::vector<std::vector<const SameColRoute*>>& lanes = same_lanes[route.column];
        float lo   = std::min(route.from_y, route.to_y);
        float hi   = std::max(route.from_y, route.to_y);
        int   lane = 0;
        for(; lane < static_cast<int>(lanes.size()); ++lane)
        {
            bool clear = true;
            for(const SameColRoute* other : lanes[lane])
            {
                float other_lo = std::min(other->from_y, other->to_y);
                float other_hi = std::max(other->from_y, other->to_y);
                if(lo < other_hi + SAME_COL_LANE_PAD && other_lo < hi + SAME_COL_LANE_PAD)
                {
                    clear = false;
                    break;
                }
            }
            if(clear) break;
        }
        if(lane == static_cast<int>(lanes.size())) lanes.emplace_back();
        route.lane = lane;
        lanes[lane].push_back(&route);
    }

    for(const SameColRoute& route : same_routes)
    {
        const MemChartArrow& arrow = m_layout.arrows[route.index];
        const MemChartBlock* from  = arrow.from_block;
        const MemChartBlock* to    = arrow.to_block;
        if(!from || !to) continue;

        float lane_x = route.base_x + SAME_COL_LANE_BASE +
                       static_cast<float>(route.lane) * SAME_COL_LANE_STEP;
        std::vector<std::pair<float, float>> pts = {{from->conn_right, route.from_y},
                                                    {lane_x, route.from_y},
                                                    {lane_x, route.to_y},
                                                    {to->conn_right, route.to_y}};
        make_route(arrow, std::move(pts), lane_x + SAME_COL_LABEL_GAP,
                   (route.from_y + route.to_y) * 0.5f);
    }

    // Inter-row: connect blocks in different row bands. Every arrow gets a
    // distinct port on its source edge, ordered by the target's x so bundles
    // heading to different targets fan apart instead of stacking on one spot.
    // Cross-column (elbow) arrows also get a fanned port on the target's side.
    {
        struct EdgePort
        {
            size_t index;
            float  key;  // orders ports along the edge
        };
        using SidePortKey = std::pair<const MemChartBlock*, bool>;
        std::map<SidePortKey, std::vector<EdgePort>> src_ports;   // (block, exits-bottom)
        std::map<SidePortKey, std::vector<EdgePort>> side_ports;  // (target, enters-right)
        // Labelled same-column links between the same blocks share a midpoint, so
        // their labels stack there: slot k of n per block pair.
        std::unordered_map<size_t, int> stack_k_of;
        std::map<BlockPair, int>        stack_n_of;

        for(size_t index : inter_row)
        {
            const MemChartArrow& arrow = m_layout.arrows[index];
            const MemChartBlock* from  = arrow.from_block;
            const MemChartBlock* to    = arrow.to_block;
            if(!from || !to) continue;
            bool exits_bottom = to->MidY() > from->MidY();
            src_ports[{from, exits_bottom}].push_back({index, to->MidX()});
            if(from->column != to->column)
            {
                bool enters_right = from->MidX() > to->MidX();
                side_ports[{to, enters_right}].push_back({index, from->MidY()});
            }
            else if(!arrow.cached_label.empty())
            {
                stack_k_of[index] = stack_n_of[BlockPairKey(*from, *to)]++;
            }
        }

        // Assign x positions along each source edge (fanned about the centre).
        std::unordered_map<size_t, float> src_x_of;
        for(std::pair<const SidePortKey, std::vector<EdgePort>>& kv : src_ports)
        {
            const MemChartBlock* block = kv.first.first;
            if(!block) continue;
            std::vector<EdgePort>& ports = kv.second;
            std::stable_sort(ports.begin(), ports.end(),
                             [](const EdgePort& a, const EdgePort& b) { return a.key < b.key; });
            int   n      = static_cast<int>(ports.size());
            float usable = std::max(block->w - PORT_EDGE_PAD * 2.0f, 0.0f);
            float step   = n > 1 ? std::min(PORT_STEP, usable / static_cast<float>(n - 1)) : 0.0f;
            for(int k = 0; k < n; ++k)
            {
                src_x_of[ports[k].index] =
                    block->MidX() +
                    (static_cast<float>(k) - static_cast<float>(n - 1) * 0.5f) * step;
            }
        }

        // Assign y positions on each target side edge, plus a per-target ordinal
        // used to stagger the elbow turns so their horizontal runs don't overlap.
        std::unordered_map<size_t, float> side_y_of;
        std::unordered_map<size_t, int>   side_k_of;
        std::unordered_map<size_t, int>   side_n_of;
        for(std::pair<const SidePortKey, std::vector<EdgePort>>& kv : side_ports)
        {
            const MemChartBlock* block = kv.first.first;
            if(!block) continue;
            std::vector<EdgePort>& ports = kv.second;
            std::stable_sort(ports.begin(), ports.end(),
                             [](const EdgePort& a, const EdgePort& b) { return a.key < b.key; });
            int n = static_cast<int>(ports.size());
            for(int k = 0; k < n; ++k)
            {
                float frac = n > 1 ? static_cast<float>(k) / static_cast<float>(n - 1) : 0.5f;
                side_y_of[ports[k].index] = fan_y(*block, frac, n, ROW_VERT_STEP);
                side_k_of[ports[k].index] = k;
                side_n_of[ports[k].index] = n;
            }
        }

        for(size_t index : inter_row)
        {
            const MemChartArrow& arrow = m_layout.arrows[index];
            const MemChartBlock* from  = arrow.from_block;
            const MemChartBlock* to    = arrow.to_block;
            if(!from || !to) continue;

            float src_x = src_x_of.count(index) ? src_x_of[index] : from->MidX();

            if(from->column == to->column)
            {
                // Same column: a straight vertical at the source port (valid at
                // the target too, since aligned columns share the x-span).
                const MemChartBlock* upper = from->y <= to->y ? from : to;
                const MemChartBlock* lower = from->y <= to->y ? to : from;
                float upper_y = upper->Bottom();
                float lower_y = lower->y;
                std::pair<float, float> from_pt = (from == upper)
                                                      ? std::make_pair(src_x, upper_y)
                                                      : std::make_pair(src_x, lower_y);
                std::pair<float, float> to_pt = (to == upper)
                                                    ? std::make_pair(src_x, upper_y)
                                                    : std::make_pair(src_x, lower_y);
                float mid_y = (upper_y + lower_y) * 0.5f;
                make_route(arrow, {from_pt, to_pt}, src_x, mid_y);
                std::unordered_map<size_t, int>::const_iterator k = stack_k_of.find(index);
                if(k != stack_k_of.end())
                {
                    float       n     = static_cast<float>(stack_n_of[BlockPairKey(*from, *to)]);
                    float       slot  = static_cast<float>(k->second) - (n - 1.0f) * 0.5f;
                    ArrowRoute& route = routes.back();
                    route.label_y     = mid_y + slot * fan_pitch - route.label_h * 0.5f;
                }
                continue;
            }

            // Cross-column: a vertical-first elbow. Drop off the source port into
            // the corridor beyond the source's band (below its highway lanes when
            // heading down), run along it to the gap beside the target, then drop
            // in that gap and jog into the target's side. The corridor and gaps
            // hold no blocks, so the elbow clears intermediate columns.
            bool  down        = to->MidY() > from->MidY();
            bool  target_left = to->MidX() < from->MidX();
            int   j           = side_k_of.count(index) ? side_k_of[index] : 0;
            int   m           = side_n_of.count(index) ? side_n_of[index] : 1;

            float src_edge_y = down ? from->Bottom() : from->y;
            float stub_len   = ROW_ELBOW_STUB + static_cast<float>(j) * ROW_VERT_STEP;
            float stub_y     = down ? row_bottom[from->row] + row_skip_reserve[from->row] + stub_len
                                    : row_top[from->row] - stub_len;

            // Gap on the source's side of the target: between the target column
            // and its neighbour column in that direction.
            float to_near = target_left ? to->conn_right : to->conn_left;
            float gap_far = to_near;
            if(target_left)
            {
                std::map<int32_t, float>::iterator next = col_left.upper_bound(to->column);
                if(next != col_left.end()) gap_far = next->second;
            }
            else
            {
                std::map<int32_t, float>::iterator prev = col_right.lower_bound(to->column);
                if(prev != col_right.begin()) gap_far = std::prev(prev)->second;
            }
            float lane_x = (to_near + gap_far) * 0.5f +
                           (static_cast<float>(j) - static_cast<float>(m - 1) * 0.5f) * ROW_VERT_STEP;

            float to_edge_x = target_left ? to->conn_right : to->conn_left;
            float to_y      = side_y_of.count(index) ? side_y_of[index] : to->MidY();

            std::vector<std::pair<float, float>> pts = {{src_x, src_edge_y},
                                                        {src_x, stub_y},
                                                        {lane_x, stub_y},
                                                        {lane_x, to_y},
                                                        {to_edge_x, to_y}};
            make_route(arrow, std::move(pts), lane_x, (stub_y + to_y) * 0.5f);
        }
    }

    // Skipping columns: route along "highway" lanes just below the arrow's own
    // row band (lanes packed above; disjoint arrows share a lane, and a shorter
    // span nests nearer the blocks). Left-going routes climb the left margin.
    const float margin_x   = CHART_PADDING + LEFT_MARGIN * SKIP_MARGIN_RATIO;
    const float lane_pitch = SkipLanePitch();

    // Spread each block's bottom connectors symmetrically about the centre so
    // drops don't stack. Ordered left-headed by increasing span then right-headed
    // by decreasing span, keeping the longest spans centre-most (no crossings).
    struct BottomPort
    {
        size_t index;
        bool   is_from;  // from-drop vs. to-rise
        int    side;     // -1 = heads left, +1 = heads right
        int    span;     // column distance
    };
    std::map<const MemChartBlock*, std::vector<BottomPort>> block_ports;
    for(size_t index : skipping)
    {
        const MemChartArrow& arrow = m_layout.arrows[index];
        const MemChartBlock* from  = arrow.from_block;
        const MemChartBlock* to    = arrow.to_block;
        if(!from || !to) continue;
        bool right = to->column > from->column;
        int  span  = right ? to->column - from->column : from->column - to->column;
        block_ports[from].push_back({index, true, right ? 1 : -1, span});
        // A left-going arrow enters its destination at the left edge, not the
        // bottom, so only right-going arrows add a to-rise port.
        if(right) block_ports[to].push_back({index, false, -1, span});
    }

    std::unordered_map<size_t, float> from_port_x;
    std::unordered_map<size_t, float> to_port_x;
    for(std::pair<const MemChartBlock* const, std::vector<BottomPort>>& kv : block_ports)
    {
        const MemChartBlock* block = kv.first;
        std::vector<BottomPort>& ports = kv.second;
        std::stable_sort(ports.begin(), ports.end(), [](const BottomPort& a, const BottomPort& b) {
            if(a.side != b.side) return a.side < b.side;
            return a.side < 0 ? a.span < b.span : a.span > b.span;
        });
        int   n      = static_cast<int>(ports.size());
        float usable = std::max(block->w - PORT_EDGE_PAD * 2.0f, 0.0f);
        float step   = n > 1 ? std::min(PORT_STEP, usable / static_cast<float>(n - 1)) : 0.0f;
        for(int k = 0; k < n; ++k)
        {
            float x = block->MidX() +
                      (static_cast<float>(k) - static_cast<float>(n - 1) * 0.5f) * step;
            (ports[k].is_from ? from_port_x : to_port_x)[ports[k].index] = x;
        }
    }

    for(size_t index : skipping)
    {
        const MemChartArrow& arrow = m_layout.arrows[index];
        const MemChartBlock* from  = arrow.from_block;
        const MemChartBlock* to    = arrow.to_block;
        if(!from || !to) continue;

        int   lane      = lane_of.count(index) ? lane_of[index] : 0;
        float highway_y = row_bottom[from->row] + SKIP_HIGHWAY_DROP +
                          static_cast<float>(lane) * lane_pitch;
        float fx        = from_port_x.count(index) ? from_port_x[index] : from->MidX();

        if(to->column < from->column)
        {
            std::vector<std::pair<float, float>> pts = {{fx, from->Bottom()},
                                                        {fx, highway_y},
                                                        {margin_x, highway_y},
                                                        {margin_x, to->MidY()},
                                                        {to->conn_left, to->MidY()}};
            make_route(arrow, std::move(pts), (margin_x + fx) * 0.5f, highway_y);
        }
        else
        {
            float tx = to_port_x.count(index) ? to_port_x[index] : to->MidX();
            std::vector<std::pair<float, float>> pts = {{fx, from->Bottom()},
                                                        {fx, highway_y},
                                                        {tx, highway_y},
                                                        {tx, to->Bottom()}};
            make_route(arrow, std::move(pts), (fx + tx) * 0.5f, highway_y);
        }
    }
}

void
ComputeMemoryChartView::ResolveLabelOverlaps(std::vector<ArrowRoute>& routes) const
{
    // Greedy: place labels in reading order and nudge any that would overlap a
    // previously placed label downward until clear.
    std::vector<ImVec4> placed;
    placed.reserve(routes.size());

    for(ArrowRoute& route : routes)
    {
        // Unlabelled arrows neither move nor block anyone else.
        if(route.label.empty()) continue;

        int pass = 0;
        bool moved = true;
        while(moved && pass++ < MAX_LABEL_PASSES)
        {
            moved = false;
            for(const ImVec4& rect : placed)
            {
                bool overlap_x = route.label_x < rect.z && rect.x < route.label_x + route.label_w;
                bool overlap_y = route.label_y < rect.w && rect.y < route.label_y + route.label_h;
                if(overlap_x && overlap_y)
                {
                    route.label_y = rect.w + LABEL_OVERLAP_NUDGE;
                    moved         = true;
                }
            }
        }
        placed.push_back(ImVec4(route.label_x, route.label_y,
                                route.label_x + route.label_w,
                                route.label_y + route.label_h));
    }
}

void
ComputeMemoryChartView::DrawArrowRoutes(ImDrawList* draw_list, ImVec2 origin,
                                        const std::vector<ArrowRoute>& routes)
{
    for(const ArrowRoute& route : routes)
    {
        const std::vector<std::pair<float, float>>& pts = route.points;
        if(pts.size() < 2) continue;

        auto screen = [&](size_t i) -> ImVec2 {
            return {origin.x + pts[i].first, origin.y + pts[i].second};
        };

        size_t last = pts.size() - 1;
        for(size_t i = 0; i + 1 < pts.size(); ++i)
        {
            ImVec2 a = screen(i);
            ImVec2 b = screen(i + 1);

            // Pull the very ends back so the dashes don't poke through heads.
            if(i == 0 && route.head_at_first)
            {
                ImVec2 d(b.x - a.x, b.y - a.y);
                float  len = std::sqrt(d.x * d.x + d.y * d.y);
                if(std::isfinite(len) && len > ARROW_HEAD_SIZE)
                {
                    a.x += d.x / len * ARROW_HEAD_SIZE;
                    a.y += d.y / len * ARROW_HEAD_SIZE;
                }
            }
            if(i + 1 == last && route.head_at_last)
            {
                ImVec2 d(b.x - a.x, b.y - a.y);
                float  len = std::sqrt(d.x * d.x + d.y * d.y);
                if(std::isfinite(len) && len > ARROW_HEAD_SIZE)
                {
                    b.x -= d.x / len * ARROW_HEAD_SIZE;
                    b.y -= d.y / len * ARROW_HEAD_SIZE;
                }
            }
            DrawDashedLine(draw_list, a, b, route.color);
        }

        if(route.head_at_first)
        {
            ImVec2 tip  = screen(0);
            ImVec2 next = screen(1);
            DrawArrowHead(draw_list, tip, {tip.x - next.x, tip.y - next.y}, route.color);
        }
        if(route.head_at_last)
        {
            ImVec2 tip  = screen(last);
            ImVec2 prev = screen(last - 1);
            DrawArrowHead(draw_list, tip, {tip.x - prev.x, tip.y - prev.y}, route.color);
        }
    }

    // Labels last (a second pass) so a later arrow's line is never painted over
    // an earlier arrow's label.
    for(const ArrowRoute& route : routes)
    {
        if(route.points.size() < 2 || route.label.empty()) continue;
        ImVec2 label_pos(origin.x + route.label_x, origin.y + route.label_y);
        DrawFloatingLabel(draw_list, label_pos, route.label.c_str(), route.color,
                          route.label_dir_x, route.label_dir_y, route.label_bidir);
        ImVec2 label_size(route.label_w, route.label_h);
        ShowMetricTooltip(label_pos, {label_pos.x + label_size.x, label_pos.y + label_size.y},
                          route.metric, true, true);
    }
}

void
ComputeMemoryChartView::ShowMetricTooltip(ImVec2 hover_min, ImVec2 hover_max,
                                          const MemChartMetricRef& ref,
                                          bool show_description, bool show_raw_value)
{
    if(!ImGui::IsMouseHoveringRect(hover_min, hover_max) ||
       !ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows |
                               ImGuiHoveredFlags_NoPopupHierarchy))
        return;

    const MetricValue* metric = ResolveMetric(ref);
    if(!metric || !metric->entry) return;

    bool has_value = !metric->values.empty();

    if(show_description)
    {
        bool has_desc = !metric->entry->description.empty();
        bool show_val = show_raw_value && has_value;
        // Nothing to show (e.g. a metric with no description and no value): skip
        // the tooltip entirely instead of drawing an empty box.
        if(!has_desc && !show_val) return;

        BeginTooltipStyled();
        if(has_desc)
        {
            // Wrap and auto-size; a window max-width below the wrap pos clips text.
            ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + TOOLTIP_MAX_WIDTH);
            ImGui::TextUnformatted(metric->entry->description.c_str());
            ImGui::PopTextWrapPos();
        }
        if(show_val)
        {
            if(has_desc) ImGui::Spacing();
            ImGui::Text("Value: %s",
                        FormatMetricValueRaw(metric->values.begin()->second).c_str());
        }
        EndTooltipStyled();
    }
    else if(show_raw_value && has_value)
    {
        ImGui::SetTooltip("%s",
                          FormatMetricValueRaw(metric->values.begin()->second).c_str());
    }
}

void
ComputeMemoryChartView::DrawTextWithTooltip(ImDrawList* draw_list, ImVec2 pos,
                                            uint32_t color, const char* text,
                                            const MemChartMetricRef& ref,
                                            bool show_description, bool show_raw_value)
{
    draw_list->AddText(pos, color, text);
    ImVec2 sz = ImGui::CalcTextSize(text);
    ShowMetricTooltip(pos, {pos.x + sz.x, pos.y + sz.y}, ref, show_description,
                      show_raw_value);
}

}  // namespace View
}  // namespace RocProfVis
