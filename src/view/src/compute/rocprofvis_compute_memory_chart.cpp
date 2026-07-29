// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_compute_memory_chart.h"

#include "rocprofvis_compute_selection.h"
#include "rocprofvis_data_provider.h"
#include "rocprofvis_memory_chart_default_layout.h"
#include "rocprofvis_requests.h"
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
static constexpr float COLUMN_GAP        = 130.0f;  // Base gap between columns.
static constexpr float MAX_COLUMN_GAP    = 200.0f;  // Cap when spreading to fill wide panels.
static constexpr float ROW_HEIGHT        = 26.0f;
static constexpr float BLOCK_BODY_TOP    = 6.0f;
static constexpr float MIN_BLOCK_WIDTH   = 240.0f;
static constexpr float MAX_BLOCK_WIDTH   = 400.0f;
static constexpr float MIN_BLOCK_HEIGHT  = 96.0f;
static constexpr float EMPTY_BODY_H      = 40.0f;
static constexpr float MIN_TARGET_HEIGHT = 460.0f;  // Floor for the common column height.
static constexpr float MAX_TARGET_HEIGHT = 900.0f;
static constexpr float GROUP_HEADER      = 26.0f;  // Title band of a group box.
static constexpr float GROUP_PAD         = 9.0f;   // Inset of blocks inside a group box.
static constexpr float GROUP_INNER_GAP   = 12.0f;  // Gap between blocks inside a group.

// Drawing constants.
static constexpr float BLOCK_ROUNDING   = 8.0f;
static constexpr float BLOCK_TEXT_PAD   = 10.0f;
static constexpr float HEADER_SEP_GAP   = 6.0f;
static constexpr float METRIC_VALUE_GAP = 14.0f;
static constexpr float LEGEND_HEIGHT    = 28.0f;

static constexpr float ARROW_THICKNESS   = 2.5f;
static constexpr float ARROW_HEAD_SIZE   = 8.0f;
static constexpr float ARROW_LABEL_ABOVE = 4.0f;
static constexpr float ARROW_VERT_SPACE  = 26.0f;
static constexpr float ARROW_DASH_LENGTH = 6.0f;
static constexpr float ARROW_DASH_GAP    = 4.0f;
static constexpr float LANE_GAP          = 20.0f;
static constexpr int   MAX_DASH_ITERS    = 20000;
static constexpr int   MAX_LABEL_PASSES  = 200;

static constexpr const char* UNAVAILABLE_METRIC_TEXT = "N/A";

struct ChartColors
{
    ImU32 bg;
    ImU32 panel;
    ImU32 panel_alt;
    ImU32 border;
    ImU32 border_hot;
    ImU32 text_main;
    ImU32 text_dim;
    ImU32 read;
    ImU32 write;
    ImU32 atomic;
    ImU32 util;
    ImU32 hit;
    ImU32 stall;
    ImU32 shadow;
};

static ChartColors
C()
{
    const SettingsManager& s = SettingsManager::GetInstance();
    return ChartColors{
        s.GetColor(Colors::kMemChartBg),        s.GetColor(Colors::kMemChartPanel),
        s.GetColor(Colors::kMemChartPanelAlt),  s.GetColor(Colors::kMemChartBorder),
        s.GetColor(Colors::kMemChartBorderHot), s.GetColor(Colors::kMemChartTextMain),
        s.GetColor(Colors::kMemChartTextDim),   s.GetColor(Colors::kMemChartRead),
        s.GetColor(Colors::kMemChartWrite),     s.GetColor(Colors::kMemChartAtomic),
        s.GetColor(Colors::kMemChartUtil),      s.GetColor(Colors::kMemChartHit),
        s.GetColor(Colors::kMemChartStall),     s.GetColor(Colors::kMemChartShadow),
    };
}

static bool
StartsWith(const std::string& text, const char* prefix)
{
    return std::strncmp(text.c_str(), prefix, std::strlen(prefix)) == 0;
}

// Color picked from the label's leading word so read/write/atomic flows are
// visually distinct regardless of which metric backs them.
static ImU32
ColorForLabel(const std::string& label)
{
    if(StartsWith(label, "Wr")) return C().write;
    if(StartsWith(label, "Atomic")) return C().atomic;
    if(StartsWith(label, "Util")) return C().util;
    if(StartsWith(label, "Hit")) return C().hit;
    if(StartsWith(label, "Stall")) return C().stall;
    return C().read;
}

// Color from an explicit data-driven category. Falls back to the label
// heuristic when no category is given, so older layouts keep working.
static ImU32
ColorForCategory(const std::string& category, const std::string& label)
{
    if(category.empty()) return ColorForLabel(label);
    if(category == "read") return C().read;
    if(category == "write") return C().write;
    if(category == "atomic") return C().atomic;
    if(category == "util") return C().util;
    if(category == "hit") return C().hit;
    if(category == "stall") return C().stall;
    return C().text_main;  // "misc" / anything else: neutral color.
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

static void
DrawBlockRect(ImDrawList* draw_list, ImVec2 top_left, ImVec2 bottom_right)
{
    draw_list->AddRectFilled({top_left.x + 3.0f, top_left.y + 4.0f},
                             {bottom_right.x + 3.0f, bottom_right.y + 4.0f},
                             C().shadow, BLOCK_ROUNDING);
    draw_list->AddRectFilled(top_left, bottom_right, C().panel, BLOCK_ROUNDING);
    draw_list->AddRectFilled({top_left.x + 1.0f, top_left.y + 1.0f},
                             {bottom_right.x - 1.0f, top_left.y + 3.0f},
                             ApplyAlpha(C().border_hot, 0.55f), 2.0f);
    draw_list->AddRect(top_left, bottom_right, C().border, BLOCK_ROUNDING, 0, 1.0f);
}

static float
DrawBlockHeader(ImDrawList* draw_list, const char* title, float block_x, float block_y,
                float block_w)
{
    float text_y = block_y + BLOCK_TEXT_PAD;
    float text_h = ImGui::CalcTextSize(title).y;
    draw_list->AddRectFilled({block_x + BLOCK_TEXT_PAD, text_y + 2.0f},
                             {block_x + BLOCK_TEXT_PAD + 3.0f, text_y + text_h - 2.0f},
                             C().read, 2.0f);
    draw_list->AddText(ImVec2(block_x + BLOCK_TEXT_PAD + 9.0f, text_y), C().text_main, title);

    float line_y = text_y + text_h + 5.0f;
    draw_list->AddLine(ImVec2(block_x + BLOCK_TEXT_PAD, line_y),
                       ImVec2(block_x + block_w - BLOCK_TEXT_PAD, line_y),
                       ApplyAlpha(C().border, 0.55f), 1.0f);
    return line_y + HEADER_SEP_GAP;
}

// Height reserved for a block header, matching DrawBlockHeader.
static float
HeaderHeight()
{
    return BLOCK_TEXT_PAD + ImGui::CalcTextSize("X").y + 5.0f + HEADER_SEP_GAP;
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
        draw_list->AddLine(dash_from, dash_to, ApplyAlpha(color, 0.22f),
                           ARROW_THICKNESS + 3.0f);
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
    float  half = head * 0.6f;
    ImVec2 base(tip.x - unit.x * head, tip.y - unit.y * head);
    ImVec2 a(base.x + perp.x * half, base.y + perp.y * half);
    ImVec2 b(base.x - perp.x * half, base.y - perp.y * half);
    draw_list->AddTriangleFilled(tip, a, b, color);
}

static void
DrawFloatingLabel(ImDrawList* draw_list, ImVec2 pos, const char* text, ImU32 accent_color)
{
    ImVec2 text_size = ImGui::CalcTextSize(text);
    ImVec2 pad(5.0f, 2.0f);
    ImVec2 min(pos.x - pad.x, pos.y - pad.y);
    ImVec2 max(pos.x + text_size.x + pad.x, pos.y + text_size.y + pad.y);
    draw_list->AddRectFilled(min, max, C().bg, 4.0f);
    draw_list->AddRect(min, max, ApplyAlpha(accent_color, 0.7f), 4.0f, 0, 1.0f);
    draw_list->AddText(pos, accent_color, text);
}

static void
DrawGroupBox(ImDrawList* draw_list, ImVec2 top_left, float w, float h, const char* title)
{
    ImVec2 bottom_right(top_left.x + w, top_left.y + h);
    draw_list->AddRectFilled(top_left, bottom_right, ApplyAlpha(C().panel_alt, 0.35f),
                             BLOCK_ROUNDING);
    draw_list->AddRect(top_left, bottom_right, ApplyAlpha(C().border, 0.9f), BLOCK_ROUNDING,
                       0, 1.5f);
    if(title && title[0] != '\0')
    {
        draw_list->AddText({top_left.x + BLOCK_TEXT_PAD, top_left.y + 6.0f}, C().text_dim,
                           title);
    }
}

static void
DrawLegend(ImDrawList* draw_list, ImVec2 origin, float y)
{
    struct LegendItem
    {
        const char* text;
        ImU32       color;
    };
    const LegendItem legend[] = {
        {"Read", C().read}, {"Write", C().write}, {"Atomic", C().atomic},
        {"Util", C().util}, {"Hit", C().hit},     {"Stall", C().stall}};

    ImVec2 pos(origin.x + CHART_PADDING, origin.y + y);
    draw_list->AddText(pos, C().text_dim, "Legend:");
    pos.x += ImGui::CalcTextSize("Legend:").x + 12.0f;
    for(const LegendItem& item : legend)
    {
        draw_list->AddRectFilled({pos.x, pos.y + 4.0f}, {pos.x + 12.0f, pos.y + 10.0f},
                                 item.color, 2.0f);
        draw_list->AddText({pos.x + 17.0f, pos.y}, C().text_dim, item.text);
        pos.x += 17.0f + ImGui::CalcTextSize(item.text).x + 16.0f;
    }
}

ComputeMemoryChartView::ComputeMemoryChartView(
    DataProvider& data_provider, std::shared_ptr<ComputeSelection> compute_selection)
: m_data_provider(data_provider)
, m_compute_selection(compute_selection)
, m_client_id(IdGenerator::GetInstance().GenerateId())
{
    LoadLayout();
}

ComputeMemoryChartView::~ComputeMemoryChartView() {}

void
ComputeMemoryChartView::LoadLayout()
{
    std::string error;
    if(!MemChartLayout::ParseFromString(kDefaultMemoryChartLayout, m_layout, &error))
    {
        spdlog::error("Memory chart: failed to parse embedded layout: {}", error);
    }

    std::string config_dir = get_application_config_path(false);
    if(config_dir.empty()) return;

    std::string   override_path = config_dir + "/" + OVERRIDE_FILE_NAME;
    std::ifstream file(override_path);
    if(!file.good()) return;

    std::stringstream buffer;
    buffer << file.rdbuf();

    MemChartLayout override_layout;
    std::string    override_error;
    if(MemChartLayout::ParseFromString(buffer.str(), override_layout, &override_error))
    {
        m_layout = std::move(override_layout);
        spdlog::info("Memory chart: loaded override layout from {}", override_path);
    }
    else
    {
        spdlog::warn("Memory chart: ignoring invalid override {} ({})", override_path,
                     override_error);
    }
}

void
ComputeMemoryChartView::LoadWorkloadLayout(uint32_t workload_id)
{
    const WorkloadInfo* workload = m_data_provider.ComputeModel().GetWorkload(workload_id);
    if(workload && !workload->memory_chart_layout.empty())
    {
        MemChartLayout db_layout;
        std::string    error;
        if(MemChartLayout::ParseFromString(workload->memory_chart_layout, db_layout, &error))
        {
            m_layout = std::move(db_layout);
            spdlog::info("Memory chart: using layout from workload {} database blob",
                         workload_id);
            return;
        }
        spdlog::warn("Memory chart: workload {} layout blob invalid ({}); using default",
                     workload_id, error);
    }
    // No usable DB layout for this workload: fall back to embedded/override.
    LoadLayout();
}

// Category id (leading segment) of a dotted metric id "category.table.entry".
static bool
MetricCategory(const MemChartMetricRef& ref, uint32_t& category)
{
    if(!ref.valid) return false;
    size_t dot = ref.name.find('.');
    if(dot == 0 || dot == std::string::npos) return false;
    category = 0;
    for(size_t i = 0; i < dot; ++i)
    {
        char c = ref.name[i];
        if(c < '0' || c > '9') return false;
        category = category * 10 + static_cast<uint32_t>(c - '0');
    }
    return true;
}

static void
CollectCategories(const std::vector<MemChartBlock>& blocks, std::set<uint32_t>& out)
{
    for(const MemChartBlock& block : blocks)
    {
        uint32_t category = 0;
        for(const MemChartContentItem& item : block.content)
        {
            if(MetricCategory(item.metric, category)) out.insert(category);
        }
        CollectCategories(block.children, out);
    }
}

void
ComputeMemoryChartView::FetchMemChartMetrics()
{
    m_ptr_by_metric_id.clear();

    m_data_provider.ComputeModel().ClearKernelMetricValues(m_client_id);

    if(!m_compute_selection) return;

    uint32_t workload_id = m_compute_selection->GetSelectedWorkload();
    uint32_t kernel_id   = m_compute_selection->GetSelectedKernel();

    // Metrics may span multiple categories/tables (e.g. 3.1.x and 3.3.x). Fetch
    // each category the layout references, whole (all sub-tables), so every
    // referenced metric resolves.
    std::set<uint32_t> categories;
    CollectCategories(m_layout.blocks, categories);
    for(const MemChartArrow& arrow : m_layout.arrows)
    {
        uint32_t category = 0;
        if(MetricCategory(arrow.metric, category)) categories.insert(category);
    }
    if(categories.empty()) return;

    std::vector<uint32_t>                       kernel_ids = {kernel_id};
    std::vector<MetricsRequestParams::MetricID> metric_ids;
    for(uint32_t category : categories)
    {
        metric_ids.push_back({category, std::nullopt, std::nullopt});
    }

    m_data_provider.FetchMetrics(
        MetricsRequestParams(workload_id, kernel_ids, metric_ids, m_client_id));
}

void
ComputeMemoryChartView::UpdateMetrics()
{
    m_ptr_by_metric_id.clear();

    if(!m_compute_selection) return;

    uint32_t kernel_id = m_compute_selection->GetSelectedKernel();
    if(kernel_id == ComputeSelection::INVALID_SELECTION_ID) return;

    const std::vector<std::shared_ptr<MetricValue>>* metrics =
        m_data_provider.ComputeModel().GetKernelMetricsData(m_client_id, kernel_id);
    if(!metrics) return;

    for(const std::shared_ptr<MetricValue>& metric : *metrics)
    {
        if(!metric || !metric->entry) continue;
        // Index every fetched metric by its full dotted id ("category.table.entry");
        // a layout may reference metrics across categories/tables.
        std::string full_id = std::to_string(metric->entry->category_id) + "." +
                              std::to_string(metric->entry->table_id) + "." +
                              std::to_string(metric->entry->id);
        m_ptr_by_metric_id[full_id] = metric.get();
    }
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
ComputeMemoryChartView::MetricValueText(const MemChartMetricRef& ref,
                                        bool include_unit) const
{
    const MetricValue* metric = ResolveMetric(ref);
    if(!metric || metric->values.empty()) return UNAVAILABLE_METRIC_TEXT;
    std::string text = FormatMetricValue(metric->values.begin()->second);
    if(include_unit && metric->entry && !metric->entry->unit.empty() &&
       IsAvailableMetricText(text))
    {
        text += " ";
        text += metric->entry->unit;
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
        std::string label = MetricLabel(item.metric, item.title);
        std::string value = MetricValueText(item.metric);
        float       row_w = ImGui::CalcTextSize(label.c_str()).x + METRIC_VALUE_GAP +
                      ImGui::CalcTextSize(value.c_str()).x;
        width = std::max(width, row_w);
    }

    width += BLOCK_TEXT_PAD * 2.0f + 12.0f;
    block.w = std::min(std::max(width, MIN_BLOCK_WIDTH), MAX_BLOCK_WIDTH);

    float body = block.content.empty()
                     ? EMPTY_BODY_H
                     : static_cast<float>(block.content.size()) * ROW_HEIGHT;
    block.h = std::max(HeaderHeight() + BLOCK_BODY_TOP + body + BLOCK_TEXT_PAD,
                       MIN_BLOCK_HEIGHT);
}

void
ComputeMemoryChartView::PositionBlock(MemChartBlock& block, float x, float y, float w,
                                      float h, float conn_l, float conn_r, int32_t column)
{
    block.x          = x;
    block.y          = y;
    block.w          = w;
    block.h          = h;
    block.conn_left  = conn_l;   // top-level ancestor box edges (passed unchanged)
    block.conn_right = conn_r;
    block.column     = column;    // propagate so arrow routing sees nested blocks

    if(!block.IsContainer()) return;

    MemChartGroupBox box;
    box.title = block.title;
    box.x     = x;
    box.y     = y;
    box.w     = w;
    box.h     = h;
    m_group_boxes.push_back(box);

    std::stable_sort(block.children.begin(), block.children.end(),
                     [](const MemChartBlock& a, const MemChartBlock& b) {
                         int32_t ka = a.order < 0 ? 0x7fffffff : a.order;
                         int32_t kb = b.order < 0 ? 0x7fffffff : b.order;
                         return ka < kb;
                     });

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
        PositionBlock(child, inner_x, cy, inner_w, ch, conn_l, conn_r, column);
        cy += ch + GROUP_INNER_GAP;
    }
}

void
ComputeMemoryChartView::ComputeLayout(float available_width)
{
    m_group_boxes.clear();

    for(MemChartBlock& block : m_layout.blocks)
    {
        MeasureBlock(block);
    }

    // Group TOP-LEVEL blocks by column, sorted by order. Nested blocks are laid
    // out recursively inside their parent.
    std::map<int32_t, std::vector<MemChartBlock*>> columns;
    for(MemChartBlock& block : m_layout.blocks)
    {
        columns[block.column].push_back(&block);
    }

    std::vector<int32_t> col_keys;
    std::vector<float>   column_widths;
    for(std::pair<const int32_t, std::vector<MemChartBlock*>>& column : columns)
    {
        std::vector<MemChartBlock*>& blocks = column.second;
        std::stable_sort(blocks.begin(), blocks.end(),
                         [](const MemChartBlock* a, const MemChartBlock* b) {
                             int32_t ka = a->order < 0 ? 0x7fffffff : a->order;
                             int32_t kb = b->order < 0 ? 0x7fffffff : b->order;
                             return ka < kb;
                         });
        float col_width = 0.0f;
        for(const MemChartBlock* block : blocks)
        {
            col_width = std::max(col_width, block->w);
        }
        col_keys.push_back(column.first);
        column_widths.push_back(col_width);
    }

    int   num_gaps = static_cast<int>(col_keys.size()) - 1;
    float blocks_w = 0.0f;
    for(float w : column_widths)
    {
        blocks_w += w;
    }
    float natural_w = CHART_PADDING + LEFT_MARGIN + blocks_w +
                      COLUMN_GAP * static_cast<float>(std::max(num_gaps, 0)) + CHART_PADDING;

    // Spread leftover horizontal space into the gaps (capped) so the chart fills
    // wide panels without the columns drifting absurdly far apart.
    float gap = COLUMN_GAP;
    if(num_gaps > 0 && available_width > natural_w)
    {
        gap += (available_width - natural_w) / static_cast<float>(num_gaps);
        gap = std::min(gap, MAX_COLUMN_GAP);
    }

    // Give every column the same height (tallest column's natural height) so
    // columns line up top and bottom and use the vertical space.
    float target_h = MIN_TARGET_HEIGHT;
    for(int32_t key : col_keys)
    {
        std::vector<MemChartBlock*>& blocks = columns[key];
        float sum_h = static_cast<float>(std::max<int>(blocks.size(), 1) - 1) * BLOCK_GAP;
        for(const MemChartBlock* block : blocks)
        {
            sum_h += block->h;
        }
        target_h = std::max(target_h, sum_h);
    }
    target_h = std::min(target_h, MAX_TARGET_HEIGHT);

    // Position and stretch each column's top-level blocks to fill target height.
    float  cursor_x = CHART_PADDING + LEFT_MARGIN;
    size_t col_idx  = 0;
    for(int32_t key : col_keys)
    {
        std::vector<MemChartBlock*>& blocks = columns[key];
        float col_w = column_widths[col_idx];
        float gaps  = static_cast<float>(std::max<int>(blocks.size(), 1) - 1) * BLOCK_GAP;
        float natural_sum = 0.0f;
        for(const MemChartBlock* block : blocks)
        {
            natural_sum += block->h;
        }
        float room  = std::max(target_h - gaps, 1.0f);
        float scale = natural_sum > 0.0f ? room / natural_sum : 1.0f;

        float y = CHART_PADDING;
        for(MemChartBlock* block : blocks)
        {
            float bh = block->h * scale;
            PositionBlock(*block, cursor_x, y, col_w, bh, cursor_x, cursor_x + col_w, key);
            y += bh + BLOCK_GAP;
        }
        cursor_x += col_w + gap;
        ++col_idx;
    }
}

void
ComputeMemoryChartView::Render()
{
    float available_width = ImGui::GetContentRegionAvail().x;
    ComputeLayout(available_width);

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

    // Build the arrow routes now so skip-lanes (below the blocks) contribute to
    // the canvas height.
    std::vector<ArrowRoute> routes;
    BuildArrowRoutes(routes);
    ResolveLabelOverlaps(routes);
    for(const ArrowRoute& route : routes)
    {
        for(const std::pair<float, float>& p : route.points)
        {
            max_bottom = std::max(max_bottom, p.second + 6.0f);
        }
        max_bottom = std::max(max_bottom, route.label_y + route.label_h + 6.0f);
    }

    float canvas_w = max_right + CHART_PADDING;
    float canvas_h = max_bottom + CHART_PADDING * 2.0f + LEGEND_HEIGHT;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, C().bg);
    ImGui::BeginChild("MemoryChart", ImVec2(0, canvas_h), ImGuiChildFlags_None,
                      ImGuiWindowFlags_HorizontalScrollbar |
                          ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleColor(1);

    ImDrawList* draw_list       = ImGui::GetWindowDrawList();
    ImVec2      window_position = ImGui::GetCursorScreenPos();
    float       backdrop_w =
        std::max(canvas_w, ImGui::GetContentRegionAvail().x + ImGui::GetScrollX());
    draw_list->AddRectFilled(window_position,
                             {window_position.x + backdrop_w, window_position.y + canvas_h},
                             C().bg);

    // Arrows first, then group boxes (containers), then blocks on top.
    DrawArrowRoutes(draw_list, window_position, routes);
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
                           C().text_main, block.title.c_str());
        return;
    }

    float cursor_y =
        DrawBlockHeader(draw_list, block.title.c_str(), block_x, block_y, block.w);
    cursor_y += BLOCK_BODY_TOP;

    for(const MemChartContentItem& item : block.content)
    {
        std::string label = MetricLabel(item.metric, item.title);
        std::string value = MetricValueText(item.metric);
        ImU32       accent = ColorForCategory(item.category, label);

        ImVec2 row_min(block_x + 6.0f, cursor_y - 2.0f);
        ImVec2 row_max(block_x + block.w - 6.0f, cursor_y + ROW_HEIGHT - 2.0f);
        if(ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows |
                                  ImGuiHoveredFlags_NoPopupHierarchy) &&
           ImGui::IsMouseHoveringRect(row_min, row_max))
        {
            draw_list->AddRectFilled(row_min, row_max, ApplyAlpha(C().border_hot, 0.16f),
                                     4.0f);
        }

        draw_list->AddRectFilled({block_x + BLOCK_TEXT_PAD, cursor_y + 4.0f},
                                 {block_x + BLOCK_TEXT_PAD + 3.0f, cursor_y + 12.0f},
                                 ApplyAlpha(accent, 0.85f), 2.0f);
        std::string label_text = label + ":";
        DrawTextWithTooltip(draw_list, {block_x + BLOCK_TEXT_PAD + 7.0f, cursor_y},
                            C().text_dim, label_text.c_str(), item.metric, true, false);

        bool  available = IsAvailableMetricText(value);
        float value_w   = ImGui::CalcTextSize(value.c_str()).x;
        float value_x   = block_x + block.w - BLOCK_TEXT_PAD - value_w;
        ImU32 value_col = available ? accent : C().text_dim;
        DrawTextWithTooltip(draw_list, {value_x, cursor_y}, value_col, value.c_str(),
                            item.metric, false, true);

        cursor_y += ROW_HEIGHT;
    }
}

void
ComputeMemoryChartView::BuildArrowRoutes(std::vector<ArrowRoute>& routes) const
{
    routes.clear();

    // Canvas bottom used to place skip "highway" lanes below every block.
    float blocks_bottom = 0.0f;
    for(const MemChartBlock& block : m_layout.blocks)
    {
        blocks_bottom = std::max(blocks_bottom, block.Bottom());
    }

    std::vector<size_t> adjacent;
    std::vector<size_t> same_column;
    std::vector<size_t> skipping;

    for(size_t i = 0; i < m_layout.arrows.size(); ++i)
    {
        const MemChartArrow& arrow = m_layout.arrows[i];
        const MemChartBlock* from  = m_layout.FindBlock(arrow.from);
        const MemChartBlock* to    = m_layout.FindBlock(arrow.to);
        if(!from || !to) continue;

        int32_t dcol = to->column - from->column;
        if(dcol == 1 || dcol == -1)
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
    auto fan_y = [](const MemChartBlock& b, float frac, int count) -> float {
        float lo = b.y + HeaderHeight() + BLOCK_BODY_TOP;
        float hi = b.Bottom() - BLOCK_TEXT_PAD;
        if(hi <= lo) return b.MidY();
        float avail   = hi - lo;
        float desired = static_cast<float>(std::max(count - 1, 0)) * ARROW_VERT_SPACE;
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

        std::string label = arrow.title;
        if(label.empty())
        {
            label = MetricLabel(arrow.metric, "");
        }
        // Arrow labels omit the unit to stay compact (units bloat the corridor
        // and overflow onto the blocks); the full value + unit is in the tooltip.
        std::string value = MetricValueText(arrow.metric, false);
        route.label        = label.empty() ? value : (label + ": " + value);
        route.color        = ColorForCategory(arrow.category, label.empty() ? value : label);

        // Which endpoint(s) get a head: the destination of the flow.
        bool head_from = arrow.direction == MemChartArrowDir::kBoth ||
                         arrow.direction == MemChartArrowDir::kBackward;
        bool head_to = arrow.direction == MemChartArrowDir::kBoth ||
                       arrow.direction == MemChartArrowDir::kForward;
        route.head_at_first = head_from;  // points[0] sits at `from`
        route.head_at_last  = head_to;    // points.back() sits at `to`

        ImVec2 size        = ImGui::CalcTextSize(route.label.c_str());
        route.label_w      = size.x;
        route.label_h      = size.y;
        route.label_x      = label_x - size.x * 0.5f;
        route.label_y      = label_y - size.y - ARROW_LABEL_ABOVE;
        routes.push_back(std::move(route));
    };

    // Adjacent columns: assign ordered connection ports on each block edge to
    // minimize crossings. Every arrow leaving a block's right edge is sorted by
    // its destination's vertical position (and vice-versa for the left edge), so
    // lines fan out in a consistent top-to-bottom order instead of tangling.
    struct Port
    {
        size_t arrow;
        float  key;  // The other endpoint's mid-Y, used for ordering.
    };
    std::map<uint32_t, std::vector<Port>> exit_ports;   // left-block id  -> right-edge ports
    std::map<uint32_t, std::vector<Port>> entry_ports;  // right-block id -> left-edge ports
    for(size_t i : adjacent)
    {
        const MemChartArrow& arrow  = m_layout.arrows[i];
        const MemChartBlock* from   = m_layout.FindBlock(arrow.from);
        const MemChartBlock* to     = m_layout.FindBlock(arrow.to);
        if(!from || !to) continue;
        const MemChartBlock* left_b  = from->column < to->column ? from : to;
        const MemChartBlock* right_b = from->column < to->column ? to : from;
        exit_ports[left_b->id].push_back({i, right_b->MidY()});
        entry_ports[right_b->id].push_back({i, left_b->MidY()});
    }

    std::unordered_map<size_t, float> exit_y;
    std::unordered_map<size_t, float> entry_y;
    auto assign_ports = [&](std::map<uint32_t, std::vector<Port>>&  edge,
                            std::unordered_map<size_t, float>&      out) {
        for(std::pair<const uint32_t, std::vector<Port>>& kv : edge)
        {
            const MemChartBlock* block = m_layout.FindBlock(kv.first);
            if(!block) continue;
            std::vector<Port>& ports = kv.second;
            std::stable_sort(ports.begin(), ports.end(),
                             [](const Port& a, const Port& b) { return a.key < b.key; });
            int n = static_cast<int>(ports.size());
            for(int k = 0; k < n; ++k)
            {
                float frac = n > 1 ? static_cast<float>(k) / static_cast<float>(n - 1)
                                   : 0.5f;
                out[ports[k].arrow] = fan_y(*block, frac, n);
            }
        }
    };
    assign_ports(exit_ports, exit_y);
    assign_ports(entry_ports, entry_y);

    // How many blocks share each column (a "stacked" column has > 1), counting
    // nested blocks too (their column was propagated during layout).
    std::map<int32_t, int> column_counts;
    std::function<void(const std::vector<MemChartBlock>&)> count_columns =
        [&](const std::vector<MemChartBlock>& blocks) {
            for(const MemChartBlock& block : blocks)
            {
                column_counts[block.column]++;
                count_columns(block.children);
            }
        };
    count_columns(m_layout.blocks);

    for(size_t i : adjacent)
    {
        const MemChartArrow& arrow  = m_layout.arrows[i];
        const MemChartBlock* from   = m_layout.FindBlock(arrow.from);
        const MemChartBlock* to     = m_layout.FindBlock(arrow.to);
        if(!from || !to) continue;
        const MemChartBlock* left_b  = from->column < to->column ? from : to;
        const MemChartBlock* right_b = from->column < to->column ? to : from;

        // Since every column shares the same height, we can keep arrows perfectly
        // horizontal: anchor the row on the "stacked" endpoint (the block whose
        // exact vertical position matters); the full-height neighbor accepts any
        // row. Horizontal lines in a corridor are parallel, so they never cross.
        bool  right_stacked = column_counts[right_b->column] > 1;
        float arrow_y       = right_stacked
                                  ? (entry_y.count(i) ? entry_y[i] : right_b->MidY())
                                  : (exit_y.count(i) ? exit_y[i] : left_b->MidY());

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

    // Same column: route through the right-hand gap in stacked lanes.
    int same_lane = 0;
    for(size_t index : same_column)
    {
        const MemChartArrow& arrow = m_layout.arrows[index];
        const MemChartBlock* from  = m_layout.FindBlock(arrow.from);
        const MemChartBlock* to    = m_layout.FindBlock(arrow.to);
        if(!from || !to) continue;

        float lane_x = std::max(from->conn_right, to->conn_right) + 12.0f +
                       static_cast<float>(same_lane++) * 14.0f;
        std::vector<std::pair<float, float>> pts = {{from->conn_right, from->MidY()},
                                                    {lane_x, from->MidY()},
                                                    {lane_x, to->MidY()},
                                                    {to->conn_right, to->MidY()}};
        make_route(arrow, std::move(pts), lane_x + 6.0f,
                   (from->MidY() + to->MidY()) * 0.5f);
    }

    // Skipping columns: route along packed "highway" lanes below the blocks.
    // Disjoint arrows share a lane; a shorter span nests nearer the blocks than
    // the span enclosing it. Left-going routes climb the reserved left margin.
    const float margin_x = CHART_PADDING + LEFT_MARGIN * 0.35f;

    // Horizontal extent each skipping arrow occupies along the highway.
    struct SkipSpan
    {
        size_t index;
        float  lo;
        float  hi;
        int    lane;
    };
    std::vector<SkipSpan> spans;
    spans.reserve(skipping.size());
    for(size_t index : skipping)
    {
        const MemChartArrow& arrow = m_layout.arrows[index];
        const MemChartBlock* from  = m_layout.FindBlock(arrow.from);
        const MemChartBlock* to    = m_layout.FindBlock(arrow.to);
        if(!from || !to) continue;
        float a = from->MidX();
        float b = to->column < from->column ? margin_x : to->MidX();
        spans.push_back({index, std::min(a, b), std::max(a, b), 0});
    }

    // Shortest spans first so they take the inner lanes; ties left-to-right.
    std::stable_sort(spans.begin(), spans.end(), [](const SkipSpan& a, const SkipSpan& b) {
        float wa = a.hi - a.lo;
        float wb = b.hi - b.lo;
        if(wa != wb) return wa < wb;
        return a.lo < b.lo;
    });

    // First-fit packing: reuse the lowest lane clear of this span, else open one.
    constexpr float                           SKIP_LANE_PAD = 24.0f;
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

    std::unordered_map<size_t, int> lane_of;
    for(const SkipSpan& span : spans)
    {
        lane_of[span.index] = span.lane;
    }

    // A label sits above its lane's line, so the pitch between lanes must exceed
    // the label height - otherwise a lane's label lands on the line above it.
    const float lane_pitch =
        std::max(LANE_GAP, ImGui::GetTextLineHeight() + ARROW_LABEL_ABOVE + 12.0f);

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
    std::map<uint32_t, std::vector<BottomPort>> block_ports;
    for(size_t index : skipping)
    {
        const MemChartArrow& arrow = m_layout.arrows[index];
        const MemChartBlock* from  = m_layout.FindBlock(arrow.from);
        const MemChartBlock* to    = m_layout.FindBlock(arrow.to);
        if(!from || !to) continue;
        bool right = to->column > from->column;
        int  span  = right ? to->column - from->column : from->column - to->column;
        block_ports[from->id].push_back({index, true, right ? 1 : -1, span});
        // A left-going arrow enters its destination at the left edge, not the
        // bottom, so only right-going arrows add a to-rise port.
        if(right) block_ports[to->id].push_back({index, false, -1, span});
    }

    std::unordered_map<size_t, float> from_port_x;
    std::unordered_map<size_t, float> to_port_x;
    constexpr float                   PORT_STEP     = 16.0f;
    constexpr float                   PORT_EDGE_PAD = 14.0f;
    for(std::pair<const uint32_t, std::vector<BottomPort>>& kv : block_ports)
    {
        const MemChartBlock* block = m_layout.FindBlock(kv.first);
        if(!block) continue;
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
        const MemChartBlock* from  = m_layout.FindBlock(arrow.from);
        const MemChartBlock* to    = m_layout.FindBlock(arrow.to);
        if(!from || !to) continue;

        int   lane      = lane_of.count(index) ? lane_of[index] : 0;
        float highway_y = blocks_bottom + 30.0f + static_cast<float>(lane) * lane_pitch;
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
                    route.label_y = rect.w + 2.0f;
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
        if(route.points.size() < 2) continue;
        ImVec2 label_pos(origin.x + route.label_x, origin.y + route.label_y);
        DrawFloatingLabel(draw_list, label_pos, route.label.c_str(), route.color);
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

        constexpr float kTooltipMaxWidth = 300.0f;
        ImGui::SetNextWindowSizeConstraints(ImVec2(0, 0),
                                            ImVec2(kTooltipMaxWidth, FLT_MAX));
        BeginTooltipStyled();
        if(has_desc)
        {
            ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + kTooltipMaxWidth);
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
