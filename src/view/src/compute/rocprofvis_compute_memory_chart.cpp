// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_compute_memory_chart.h"
#include "rocprofvis_compute_selection.h"
#include "rocprofvis_data_provider.h"
#include "rocprofvis_requests.h"
#include "rocprofvis_settings_manager.h"
#include "rocprofvis_utils.h"
#include "widgets/rocprofvis_gui_helpers.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <imgui.h>

namespace RocProfVis
{
namespace View
{

// Layout constants
static constexpr float CHART_PADDING     = 18.0f;
static constexpr float BLOCK_GAP         = 14.0f;
static constexpr float ARROW_COLUMN_GAP  = 160.0f;

static constexpr uint32_t MEMCHART_CATEGORY_ID = 3;
static constexpr uint32_t MEMCHART_TABLE_ID    = 1;

static constexpr float BLOCK_ROUNDING    = 8.0f;
static constexpr float BLOCK_TEXT_PAD    = 10.0f;
static constexpr float ROW_HEIGHT        = 20.0f;
static constexpr float HEADER_SEP_GAP    = 6.0f;
static constexpr float METRIC_VALUE_GAP  = 10.0f;

static constexpr float ARROW_THICKNESS   = 2.0f;
static constexpr float ARROW_HEAD_SIZE   = 5.0f;
static constexpr float ARROW_LABEL_ABOVE = 21.0f;
static constexpr float ARROW_VERT_SPACE  = 22.0f;
static constexpr float ARROW_DASH_LENGTH = 6.0f;
static constexpr float ARROW_DASH_GAP    = 4.0f;
static constexpr float LEGEND_HEIGHT     = 28.0f;

// Helpers
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

static constexpr ChartColors DARK_CHART_COLORS = {
    IM_COL32(29, 30, 38, 255), IM_COL32(34, 37, 48, 245),
    IM_COL32(39, 43, 56, 245), IM_COL32(62, 116, 168, 220),
    IM_COL32(78, 152, 220, 255), IM_COL32(238, 243, 255, 255),
    IM_COL32(145, 156, 174, 255), IM_COL32(47, 214, 220, 235),
    IM_COL32(225, 203, 78, 235), IM_COL32(184, 139, 226, 235),
    IM_COL32(129, 231, 79, 255), IM_COL32(231, 196, 65, 255),
    IM_COL32(235, 82, 98, 255), IM_COL32(0, 0, 0, 85)};

static constexpr ChartColors LIGHT_CHART_COLORS = {
    IM_COL32(248, 251, 255, 255), IM_COL32(255, 255, 255, 246),
    IM_COL32(241, 247, 255, 246), IM_COL32(91, 139, 184, 205),
    IM_COL32(38, 132, 214, 255), IM_COL32(25, 38, 56, 255),
    IM_COL32(92, 106, 126, 255), IM_COL32(0, 132, 155, 235),
    IM_COL32(168, 128, 0, 235), IM_COL32(124, 78, 190, 235),
    IM_COL32(58, 145, 26, 255), IM_COL32(177, 130, 0, 255),
    IM_COL32(204, 55, 70, 255), IM_COL32(76, 95, 128, 35)};

static const ChartColors&
C()
{
    return SettingsManager::GetInstance().GetUserSettings().display_settings.use_dark_mode
               ? DARK_CHART_COLORS
               : LIGHT_CHART_COLORS;
}

static bool
StartsWith(const char* text, const char* prefix)
{
    return text && prefix &&
           std::strncmp(text, prefix, std::strlen(prefix)) == 0;
}

static ImU32
ColorForFlowLabel(const char* text)
{
    if(StartsWith(text, "Wr:")) return C().write;
    if(StartsWith(text, "Atomic:")) return C().atomic;
    return C().read;
}

static ImU32
ColorForMetricLabel(const char* label)
{
    if(StartsWith(label, "Util")) return C().util;
    if(StartsWith(label, "Hit")) return C().hit;
    if(StartsWith(label, "Stall")) return C().stall;
    if(StartsWith(label, "Coalescing")) return C().read;
    if(StartsWith(label, "Rd")) return C().read;
    if(StartsWith(label, "Wr")) return C().write;
    if(StartsWith(label, "Atomic")) return C().atomic;
    return C().text_main;
}

static bool
IsAvailableMetricText(const char* text)
{
    return text && std::strcmp(text, UNAVAILABLE_METRIC_TEXT) != 0 &&
           std::strcmp(text, "-") != 0;
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
    draw_list->AddRect(top_left, bottom_right,
                       C().border, BLOCK_ROUNDING, 0, 1.0f);
    draw_list->AddRect({top_left.x + 1.0f, top_left.y + 1.0f},
                       {bottom_right.x - 1.0f, bottom_right.y - 1.0f},
                       ApplyAlpha(C().border_hot, 0.25f), BLOCK_ROUNDING, 0, 1.0f);
}

static float
DrawBlockHeader(ImDrawList* draw_list, const char* title,
                float block_x, float block_y, float block_w)
{
    float text_y = block_y + BLOCK_TEXT_PAD;
    float text_h       = ImGui::CalcTextSize(title).y;
    draw_list->AddRectFilled({block_x + BLOCK_TEXT_PAD, text_y + 2.0f},
                             {block_x + BLOCK_TEXT_PAD + 3.0f,
                              text_y + text_h - 2.0f},
                             C().read, 2.0f);
    draw_list->AddText(ImVec2(block_x + BLOCK_TEXT_PAD + 9.0f, text_y),
                       C().text_main, title);

    float line_y = text_y + text_h + 5.0f;
    draw_list->AddLine(ImVec2(block_x + BLOCK_TEXT_PAD, line_y),
                       ImVec2(block_x + block_w - BLOCK_TEXT_PAD, line_y),
                       ApplyAlpha(C().border, 0.55f), 1.0f);
    return line_y + HEADER_SEP_GAP;
}

static void
DrawDashedFlowLine(ImDrawList* draw_list, ImVec2 from, ImVec2 to, ImU32 color)
{
    ImVec2 delta(to.x - from.x, to.y - from.y);
    float  length = std::sqrt(delta.x * delta.x + delta.y * delta.y);
    if(length <= 0.0f) return;

    ImVec2 dir(delta.x / length, delta.y / length);
    float  cursor = 0.0f;
    while(cursor < length)
    {
        float dash_end = std::min(cursor + ARROW_DASH_LENGTH, length);
        ImVec2 dash_from(from.x + dir.x * cursor, from.y + dir.y * cursor);
        ImVec2 dash_to(from.x + dir.x * dash_end, from.y + dir.y * dash_end);
        draw_list->AddLine(dash_from, dash_to,
                           ApplyAlpha(color, 0.22f), ARROW_THICKNESS + 3.0f);
        draw_list->AddLine(dash_from, dash_to, color, ARROW_THICKNESS);
        cursor = dash_end + ARROW_DASH_GAP;
    }
}

static void
DrawHorizontalArrow(ImDrawList* draw_list, ImVec2 from, ImVec2 to, ImU32 color)
{
    if(from.x == to.x) return;

    const float dir  = (to.x > from.x) ? 1.0f : -1.0f;
    const float head = ARROW_HEAD_SIZE;

    DrawDashedFlowLine(draw_list, from, {to.x - dir * head, to.y}, color);
    draw_list->AddCircleFilled(from, 2.25f, color);
    draw_list->AddTriangleFilled(
        to, ImVec2(to.x - dir * head, to.y - head * 0.6f),
        ImVec2(to.x - dir * head, to.y + head * 0.6f), color);
}

static void
DrawCenteredText(ImDrawList* draw_list, const char* text,
                 float region_x, float text_y, float region_w, ImU32 color)
{
    float text_w = ImGui::CalcTextSize(text).x;
    draw_list->AddText(ImVec2(region_x + (region_w - text_w) * 0.5f, text_y),
                       color, text);
}

static void
DrawFloatingTextBackground(ImDrawList* draw_list, ImVec2 pos, const char* text,
                           ImU32 accent_color)
{
    ImVec2 text_size = ImGui::CalcTextSize(text);
    ImVec2 pad(5.0f, 2.0f);
    ImVec2 min(pos.x - pad.x, pos.y - pad.y);
    ImVec2 max(pos.x + text_size.x + pad.x, pos.y + text_size.y + pad.y);

    draw_list->AddRectFilled(min, max, ApplyAlpha(C().bg, 0.92f), 4.0f);
    draw_list->AddRect(min, max, ApplyAlpha(accent_color, 0.7f), 4.0f, 0, 1.0f);
}

static void
DrawInsetCard(ImDrawList* draw_list, float card_x, float card_y,
              float card_w, float card_h, const char* text)
{
    ImVec2 top_left(card_x, card_y);
    ImVec2 bottom_right(card_x + card_w, card_y + card_h);
    draw_list->AddRectFilled(top_left, bottom_right, C().panel_alt, 4.0f);
    draw_list->AddRect(top_left, bottom_right,
                       ApplyAlpha(C().border, 0.75f), 4.0f, 0, 1.0f);

    float text_h = ImGui::CalcTextSize(text).y;
    draw_list->AddText(ImVec2(card_x + BLOCK_TEXT_PAD,
                              card_y + (card_h - text_h) * 0.5f),
                       C().text_dim, text);
}

static void
DrawChartBackdrop(ImDrawList* draw_list, ImVec2 origin, float canvas_w, float canvas_h)
{
    ImVec2 min = origin;
    ImVec2 max(origin.x + canvas_w, origin.y + canvas_h);
    draw_list->AddRectFilled(min, max, C().bg);

    for(float x = min.x + CHART_PADDING; x < max.x; x += 36.0f)
    {
        draw_list->AddLine({x, min.y}, {x, max.y},
                           ApplyAlpha(C().border, 0.08f), 1.0f);
    }
    for(float y = min.y + CHART_PADDING; y < max.y; y += 36.0f)
    {
        draw_list->AddLine({min.x, y}, {max.x, y},
                           ApplyAlpha(C().border, 0.08f), 1.0f);
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
        {"Util", C().util}, {"Hit", C().hit}, {"Stall", C().stall}};

    ImVec2 pos(origin.x + CHART_PADDING, origin.y + y);
    draw_list->AddText(pos, C().text_dim, "Legend:");
    pos.x += ImGui::CalcTextSize("Legend:").x + 12.0f;

    for(const auto& item : legend)
    {
        draw_list->AddRectFilled({pos.x, pos.y + 4.0f},
                                 {pos.x + 12.0f, pos.y + 10.0f},
                                 item.color, 2.0f);
        draw_list->AddText({pos.x + 17.0f, pos.y}, C().text_dim, item.text);
        pos.x += 17.0f + ImGui::CalcTextSize(item.text).x + 16.0f;
    }
}

ComputeMemoryChartView::ComputeMemoryChartView(DataProvider& data_provider, std::shared_ptr<ComputeSelection> compute_selection)
: m_data_provider(data_provider)
, m_compute_selection(compute_selection)
, m_client_id(IdGenerator::GetInstance().GenerateId())
{
    m_values.fill(UNAVAILABLE_METRIC_TEXT);
    m_metric_ptrs.resize(MEMCHART_METRIC_COUNT, nullptr);
}

ComputeMemoryChartView::~ComputeMemoryChartView() {}

const char*
ComputeMemoryChartView::GetMetricText(MemChartMetric metric) const
{
    if(metric == MEMCHART_METRIC_NA) return UNAVAILABLE_METRIC_TEXT;
    if(metric >= 0 && metric < MEMCHART_METRIC_COUNT)
        return m_values[metric].c_str();
    return UNAVAILABLE_METRIC_TEXT;
}

void
ComputeMemoryChartView::FetchMemChartMetrics()
{
    m_values.fill(UNAVAILABLE_METRIC_TEXT);
    m_metric_ptrs.assign(MEMCHART_METRIC_COUNT, nullptr);

    m_data_provider.ComputeModel().ClearKernelMetricValues(m_client_id);

    if(m_compute_selection)
    {
        uint32_t workload_id = m_compute_selection->GetSelectedWorkload();
        uint32_t kernel_id = m_compute_selection->GetSelectedKernel();

        std::vector<uint32_t> kernel_ids = {kernel_id};
        std::vector<MetricsRequestParams::MetricID> metric_ids;
        metric_ids.push_back({MEMCHART_CATEGORY_ID, MEMCHART_TABLE_ID, std::nullopt});

        m_data_provider.FetchMetrics(
            MetricsRequestParams(workload_id, kernel_ids, metric_ids, m_client_id));
    }
}

void 
ComputeMemoryChartView::UpdateMetrics()
{
    m_metric_ptrs.assign(MEMCHART_METRIC_COUNT, nullptr);

    if(!m_compute_selection) return;

    uint32_t kernel_id = m_compute_selection->GetSelectedKernel();
    if(kernel_id == ComputeSelection::INVALID_SELECTION_ID) return;

    const std::vector<std::shared_ptr<MetricValue>>* metrics =
        m_data_provider.ComputeModel().GetKernelMetricsData(m_client_id, kernel_id);
    
    if(!metrics) return;

    for(const std::shared_ptr<MetricValue>& metric : *metrics)
    {
        if(!metric || !metric->entry) continue;
        if(metric->entry->category_id != MEMCHART_CATEGORY_ID) continue;
        if(metric->entry->table_id != MEMCHART_TABLE_ID) continue;

        uint32_t id = metric->entry->id;
        if(id < MEMCHART_METRIC_COUNT)
        {
            m_metric_ptrs[id] = metric.get();
        }
    }

    for(size_t i = 0; i < MEMCHART_METRIC_COUNT; ++i)
    {
        if(m_metric_ptrs[i] && !m_metric_ptrs[i]->values.empty())
        {
            m_values[i] = FormatMetricValue(m_metric_ptrs[i]->values.begin()->second);
        }
        else
        {
            m_values[i] = UNAVAILABLE_METRIC_TEXT;
        }
    }
}

void
ComputeMemoryChartView::Render()
{
    // Compute block positions first so we know the canvas dimensions before
    // creating the child window (required for auto-height parent containers).
    ComputeLayout();

    float canvas_w = m_hbm_block.Right() + CHART_PADDING;
    float canvas_h = std::max({m_instr_buff_block.Bottom(),
                               m_instr_dispatch_block.Bottom(),
                               m_instr_l1_block.Bottom(),
                               m_gmi_block.Bottom()}) + CHART_PADDING * 2 +
                     LEGEND_HEIGHT;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, C().bg);

    ImGui::BeginChild("MemoryChart", ImVec2(0, canvas_h), ImGuiChildFlags_None,
                      ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleColor(1);

    ImDrawList* draw_list       = ImGui::GetWindowDrawList();
    ImVec2      window_position = ImGui::GetCursorScreenPos();
    float       backdrop_w =
        std::max(canvas_w, ImGui::GetContentRegionAvail().x + ImGui::GetScrollX());

    DrawChartBackdrop(draw_list, window_position, backdrop_w, canvas_h);

    DrawInstrBuff(draw_list, window_position);
    DrawInstrDispatch(draw_list, window_position);
    DrawActiveCUs(draw_list, window_position);

    DrawLDS(draw_list, window_position);
    DrawVectorL1(draw_list, window_position);
    DrawScalarL1D(draw_list, window_position);
    DrawInstrL1(draw_list, window_position);

    DrawL2(draw_list, window_position);

    DrawXGMIPCIe(draw_list, window_position);
    DrawFabric(draw_list, window_position);
    DrawGMI(draw_list, window_position);
    DrawHBM(draw_list, window_position);

    DrawConnections(draw_list, window_position);
    DrawLegend(draw_list, window_position, canvas_h - CHART_PADDING - LEGEND_HEIGHT);

    // Compute and reserve space for horizontal scrollbar if needed
    const float h_scrollbar_size = (ImGui::GetScrollMaxX() > 0.0f)
                                       ? ImGui::GetStyle().ScrollbarSize
                                       : 0.0f;
    ImGui::SetCursorPos(ImVec2(canvas_w, canvas_h - 1.0f - h_scrollbar_size));
    ImGui::Dummy(ImVec2(1, 1));

    ImGui::EndChild();
}

void
ComputeMemoryChartView::ComputeLayout()
{
    // Pipeline column
    m_instr_buff_block     = {CHART_PADDING, CHART_PADDING, 210, 480};
    m_instr_dispatch_block = {m_instr_buff_block.Right() + BLOCK_GAP + 30,
                              CHART_PADDING, 360, 480};
    m_active_cus_block     = {m_instr_dispatch_block.Right() - 60,
                              CHART_PADDING + 15, 250, 450};

    // Cache column
    float cache_column_x = m_active_cus_block.Right() + ARROW_COLUMN_GAP;
    m_lds_block        = {cache_column_x, CHART_PADDING, 240, 105};
    m_vector_l1_block  = {cache_column_x, m_lds_block.Bottom() + BLOCK_GAP, 240, 160};
    m_scalar_l1d_block = {cache_column_x, m_vector_l1_block.Bottom() + BLOCK_GAP, 240, 105};
    m_instr_l1_block   = {cache_column_x, m_scalar_l1d_block.Bottom() + BLOCK_GAP, 240, 105};

    // L2 column (spans full cache height)
    float l2_height = m_instr_l1_block.Bottom() - CHART_PADDING;
    m_l2_block      = {m_lds_block.Right() + ARROW_COLUMN_GAP,
                       CHART_PADDING, 200, l2_height};

    // Fabric column
    float fabric_column_x = m_l2_block.Right() + ARROW_COLUMN_GAP;
    m_xgmi_pcie_block = {fabric_column_x, CHART_PADDING, 160, 95};
    m_fabric_block    = {fabric_column_x, m_xgmi_pcie_block.Bottom() + BLOCK_GAP, 250, 280};
    m_gmi_block       = {fabric_column_x, m_fabric_block.Bottom() + BLOCK_GAP, 160, 80};

    // HBM (right of fabric)
    m_hbm_block = {m_fabric_block.Right() + ARROW_COLUMN_GAP,
                   m_fabric_block.y, 160, 140};
}

float
ComputeMemoryChartView::DrawMetricRow(ImDrawList* draw_list, float block_x, float cursor_y, float block_w,
              const char* label, MemChartMetric metric_id, const char* unit /*= ""*/)
{
    ImVec2 row_min(block_x + 6.0f, cursor_y - 2.0f);
    ImVec2 row_max(block_x + block_w - 6.0f, cursor_y + ROW_HEIGHT - 2.0f);
    if(ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows |
                              ImGuiHoveredFlags_NoPopupHierarchy) &&
       ImGui::IsMouseHoveringRect(row_min, row_max))
    {
        draw_list->AddRectFilled(row_min, row_max,
                                 ApplyAlpha(C().border_hot, 0.16f),
                                 4.0f);
    }

    ImU32 label_accent = ColorForMetricLabel(label);
    draw_list->AddRectFilled({block_x + BLOCK_TEXT_PAD, cursor_y + 4.0f},
                             {block_x + BLOCK_TEXT_PAD + 3.0f, cursor_y + 12.0f},
                             ApplyAlpha(label_accent, 0.85f), 2.0f);
    DrawTextWithTooltip(draw_list, {block_x + BLOCK_TEXT_PAD + 7.0f, cursor_y},
                        C().text_dim,
                        label, metric_id, true, false);

    const char* metric_text = GetMetricText(metric_id);
    std::string value_text  = metric_text;
    if(unit[0] != '\0' && IsAvailableMetricText(metric_text))
    {
        value_text += " ";
        value_text += unit;
    }

    float label_w = ImGui::CalcTextSize(label).x;
    float value_w = ImGui::CalcTextSize(value_text.c_str()).x;
    float value_x = block_x + block_w - BLOCK_TEXT_PAD - value_w;
    value_x       = std::max(value_x,
                             block_x + BLOCK_TEXT_PAD + 7.0f + label_w +
                                 METRIC_VALUE_GAP);

    ImU32 value_color = IsAvailableMetricText(metric_text)
                            ? label_accent
                            : C().text_dim;
    DrawTextWithTooltip(draw_list, {value_x, cursor_y},
                        value_color, value_text.c_str(), metric_id, false, true);
    return cursor_y + ROW_HEIGHT;
}

void
ComputeMemoryChartView::ShowMetricTooltip(ImVec2 hover_min, ImVec2 hover_max,
                                          MemChartMetric metric_id,
                                          bool show_description, bool show_raw_value)
{
    if(!ImGui::IsMouseHoveringRect(hover_min, hover_max) ||
       !ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows |
                               ImGuiHoveredFlags_NoPopupHierarchy))
        return;
    if(metric_id < 0 || metric_id >= MEMCHART_METRIC_COUNT) return;
    if(!m_metric_ptrs[metric_id]) return;

    bool has_value = !m_metric_ptrs[metric_id]->values.empty();

    if(show_description)
    {
        constexpr float kTooltipMaxWidth = 300.0f;
        ImGui::SetNextWindowSizeConstraints(ImVec2(0, 0),
                                            ImVec2(kTooltipMaxWidth, FLT_MAX));
        BeginTooltipStyled();
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + kTooltipMaxWidth);
        ImGui::TextUnformatted(m_metric_ptrs[metric_id]->entry->description.c_str());
        if(show_raw_value && has_value)
        {
            ImGui::Spacing();
            ImGui::Text("Value: %s",
                        FormatMetricValueRaw(
                            m_metric_ptrs[metric_id]->values.begin()->second)
                            .c_str());
        }
        ImGui::PopTextWrapPos();
        EndTooltipStyled();
    }
    else if(show_raw_value && has_value)
    {
        ImGui::SetTooltip(
            "%s",
            FormatMetricValueRaw(m_metric_ptrs[metric_id]->values.begin()->second)
                .c_str());
    }
}

void
ComputeMemoryChartView::DrawTextWithTooltip(ImDrawList* draw_list, ImVec2 pos, uint32_t color,
                                            const char* text, MemChartMetric metric_id,
                                            bool show_description, bool show_raw_value)
{
    draw_list->AddText(pos, color, text);
    ImVec2 sz = ImGui::CalcTextSize(text);
    ShowMetricTooltip(pos, {pos.x + sz.x, pos.y + sz.y},
                      metric_id, show_description, show_raw_value);
}

void
ComputeMemoryChartView::DrawInstrBuff(ImDrawList* draw_list, ImVec2 origin)
{
    const auto& block   = m_instr_buff_block;
    float       block_x = origin.x + block.x;
    float       block_y = origin.y + block.y;


    DrawBlockRect(draw_list, { block_x + 16, block_y + 16 },
                  { block_x + block.w + 16, block_y + block.h + 16 });
    DrawBlockRect(draw_list, { block_x + 8, block_y + 8 },
                  { block_x + block.w + 8, block_y + block.h + 8 });
    DrawBlockRect(draw_list, {block_x, block_y},
                  {block_x + block.w, block_y + block.h});
    float cursor_y = DrawBlockHeader(draw_list, "Instr Buff",
                                     block_x, block_y, block.w);

    float card_w = block.w - BLOCK_TEXT_PAD * 2;
    DrawInsetCard(draw_list, block_x + BLOCK_TEXT_PAD, cursor_y, card_w, 32,
                  "Wave 0 Instr buff");
    cursor_y += 38;
    DrawInsetCard(draw_list, block_x + BLOCK_TEXT_PAD, cursor_y, card_w, 32,
                  "Wave N-1 Instr buff");
    cursor_y += 44;

    ImU32 dim_color  = C().text_dim;
    ImU32 main_color = C().text_main;
    char  text_buf[64];

    DrawTextWithTooltip(draw_list, {block_x + BLOCK_TEXT_PAD, cursor_y},
                        dim_color, "Wave Occupancy:",
                        WAVEFRONT_OCCUPANCY, true, false);
    cursor_y += ROW_HEIGHT;
    snprintf(text_buf, sizeof(text_buf), "%s per-GCD",
             GetMetricText(WAVEFRONT_OCCUPANCY));
    DrawTextWithTooltip(draw_list, {block_x + BLOCK_TEXT_PAD, cursor_y},
                        main_color, text_buf,
                        WAVEFRONT_OCCUPANCY, false, true);
    cursor_y += ROW_HEIGHT + 8;

    DrawTextWithTooltip(draw_list, {block_x + BLOCK_TEXT_PAD, cursor_y},
                        dim_color, "Wave Life:",
                        WAVE_LIFE, true, false);
    cursor_y += ROW_HEIGHT;
    snprintf(text_buf, sizeof(text_buf), "%s cycles", GetMetricText(WAVE_LIFE));
    DrawTextWithTooltip(draw_list, {block_x + BLOCK_TEXT_PAD, cursor_y},
                        main_color, text_buf,
                        WAVE_LIFE, false, true);
}

void
ComputeMemoryChartView::DrawInstrDispatch(ImDrawList* draw_list, ImVec2 origin)
{
    const auto& block       = m_instr_dispatch_block;
    float       block_x     = origin.x + block.x;
    float       block_y     = origin.y + block.y;
    const int   kNumTraps   = 8;
    const float kTrapWidth  = 50.0f;
    const float kTrapHeight = block.h / static_cast<float>(kNumTraps);

    // --- Compute trapezoid positions (math only, no drawing yet) ---
    ImVec2 trap_midpoints[8];
    ImVec2 small_trap_pts[8][4];
    for(int i = 0; i < kNumTraps; ++i)
    {
        float trap_y      = block_y + i * kTrapHeight;
        float narrow_side = kTrapHeight * (150.0f / 480.0f);
        small_trap_pts[i][0] = {block_x,              trap_y};
        small_trap_pts[i][1] = {block_x + kTrapWidth, trap_y + (kTrapHeight - narrow_side) * 0.5f};
        small_trap_pts[i][2] = {block_x + kTrapWidth, trap_y + (kTrapHeight + narrow_side) * 0.5f};
        small_trap_pts[i][3] = {block_x,              trap_y + kTrapHeight};
        trap_midpoints[i]    = {block_x + kTrapWidth * 0.5f,
                                trap_y + kTrapHeight * 0.5f};
    }

    float  big_trap_x = block_x + kTrapWidth + 30;
    float  big_narrow = 400.0f;

    ImVec2 big_pts[4] = {
        {big_trap_x,              block_y},
        {big_trap_x + kTrapWidth, block_y + (block.h - big_narrow) * 0.5f},
        {big_trap_x + kTrapWidth, block_y + (block.h + big_narrow) * 0.5f},
        {big_trap_x,              block_y + block.h},
    };

    // --- Draw arrows + pills FIRST (behind) ---
    const char* pill_labels[] = {
        "SALU", "SMEM", "VALU", "Matrix Ops", "VMEM", "LDS", "GWS", "Br"
    };
    const MemChartMetric pill_metrics[] = {
        SALU, SMEM, VALU, MATRIX_OPS, VMEM, LDS, GWS, BR
    };

    ImU32 arrow_color     = C().read;
    ImU32 pill_border_col = ApplyAlpha(C().border, 0.85f);
    ImU32 title_bg_col    = C().panel_alt;
    ImU32 value_bg_col    = ApplyAlpha(C().read, 0.18f);
    ImU32 title_text_col  = C().text_main;
    ImU32 value_text_col  = C().text_main;

    const float kPillPadH   = 5.0f;
    const float kPillPadV   = 2.0f;
    const float kPillRound  = 4.0f;
    const float kPillGap    = 1.0f;

    // Compute uniform pill width across all entries
    float pill_width = 0.0f;
    for(int i = 0; i < kNumTraps; ++i)
    {
        pill_width = std::max(pill_width, ImGui::CalcTextSize(pill_labels[i]).x);
        pill_width = std::max(pill_width,
                              ImGui::CalcTextSize(GetMetricText(pill_metrics[i])).x);
    }
    pill_width += kPillPadH * 2;
    float pill_height = ImGui::CalcTextSize("X").y + kPillPadV * 2;
    float line_end_x  = block_x + 300.0f;

    for(int i = 0; i < kNumTraps; ++i)
    {
        ImVec2 arrow_from = trap_midpoints[i];
        ImVec2 arrow_to   = {line_end_x, arrow_from.y};

        DrawHorizontalArrow(draw_list, arrow_from, arrow_to, arrow_color);

        float pill_right_edge = arrow_to.x - ARROW_HEAD_SIZE - 22.0f;
        float mid_y           = arrow_from.y;

        // Title pill (above arrow line)
        ImVec2 title_min = {pill_right_edge - pill_width,
                            mid_y - pill_height - kPillGap};
        ImVec2 title_max = {pill_right_edge, mid_y - kPillGap};
        draw_list->AddRectFilled(title_min, title_max, title_bg_col, kPillRound);
        draw_list->AddRect(title_min, title_max, pill_border_col,
                           kPillRound, 0, 1.0f);
        float label_w = ImGui::CalcTextSize(pill_labels[i]).x;
        draw_list->AddText(
            {title_min.x + (pill_width - label_w) * 0.5f, title_min.y + kPillPadV},
            title_text_col, pill_labels[i]);
        ShowMetricTooltip(title_min, title_max, pill_metrics[i], true, false);

        // Value pill (below arrow line)
        ImVec2 value_min = {pill_right_edge - pill_width, mid_y + kPillGap};
        ImVec2 value_max = {pill_right_edge, mid_y + kPillGap + pill_height};
        draw_list->AddRectFilled(value_min, value_max, value_bg_col, kPillRound);
        draw_list->AddRect(value_min, value_max, pill_border_col,
                           kPillRound, 0, 1.0f);
        float val_w = ImGui::CalcTextSize(GetMetricText(pill_metrics[i])).x;
        draw_list->AddText(
            {value_min.x + (pill_width - val_w) * 0.5f, value_min.y + kPillPadV},
            value_text_col, GetMetricText(pill_metrics[i]));
        ShowMetricTooltip(value_min, value_max, pill_metrics[i], false, true);
    }

    // --- Draw trapezoids LAST (on top of arrows) ---
    ImU32 fill_color   = ApplyAlpha(C().panel_alt, 0.92f);
    ImU32 border_color = ApplyAlpha(C().border, 0.75f);
    for(int i = 0; i < kNumTraps; ++i)
    {
        draw_list->AddConvexPolyFilled(small_trap_pts[i], 4, fill_color);
        draw_list->AddPolyline(small_trap_pts[i], 4, border_color,
                               ImDrawFlags_Closed, 1.0f);
    }
    draw_list->AddConvexPolyFilled(big_pts, 4, fill_color);
    draw_list->AddPolyline(big_pts, 4, border_color, ImDrawFlags_Closed, 1.0f);
}

void
ComputeMemoryChartView::DrawActiveCUs(ImDrawList* draw_list, ImVec2 origin)
{
    const auto& block   = m_active_cus_block;
    float       block_x = origin.x + block.x;
    float       block_y = origin.y + block.y;

    DrawBlockRect(draw_list, {block_x, block_y},
                  {block_x + block.w, block_y + block.h});
    float cursor_y = DrawBlockHeader(draw_list, "Exec",
                                     block_x, block_y, block.w);

    DrawTextWithTooltip(draw_list, {block_x + BLOCK_TEXT_PAD, cursor_y},
                        C().text_dim, "Active CUs",
                        NUM_CUS, true, false);
    cursor_y += ROW_HEIGHT;
    DrawTextWithTooltip(draw_list, {block_x + BLOCK_TEXT_PAD, cursor_y},
                        C().text_main,
                        GetMetricText(NUM_CUS), NUM_CUS, false, true);
    cursor_y += ROW_HEIGHT + 5;

    // Thin separator
    draw_list->AddLine({block_x + BLOCK_TEXT_PAD, cursor_y},
                       {block_x + block.w - BLOCK_TEXT_PAD, cursor_y},
                       ApplyAlpha(C().border, 0.55f), 1.0f);
    cursor_y += 8;

    cursor_y = DrawMetricRow(draw_list, block_x, cursor_y, block.w, "VGPRs:", VGPR);
    cursor_y = DrawMetricRow(draw_list, block_x, cursor_y, block.w, "SGPRs:", SGPR);
    cursor_y = DrawMetricRow(draw_list, block_x, cursor_y, block.w,
                             "LDS Alloc:", LDS_ALLOCATION);
    cursor_y = DrawMetricRow(draw_list, block_x, cursor_y, block.w,
                             "Scratch Alloc:", SCRATCH_ALLOCATION);
    cursor_y =
        DrawMetricRow(draw_list, block_x, cursor_y, block.w, "Wavefronts:", WAVEFRONTS);
    cursor_y =
        DrawMetricRow(draw_list, block_x, cursor_y, block.w, "Workgroups:", WORKGROUPS);
}

void
ComputeMemoryChartView::DrawLDS(ImDrawList* draw_list, ImVec2 origin)
{
    const auto& block   = m_lds_block;
    float       block_x = origin.x + block.x;
    float       block_y = origin.y + block.y;

    DrawBlockRect(draw_list, {block_x, block_y},
                  {block_x + block.w, block_y + block.h});
    float cursor_y = DrawBlockHeader(draw_list, "LDS",
                                     block_x, block_y, block.w);

    cursor_y =
        DrawMetricRow(draw_list, block_x, cursor_y, block.w, "Util:", LDS_UTIL, "%");
    cursor_y = DrawMetricRow(draw_list, block_x, cursor_y, block.w, "Latency:", LDS_LATENCY,
                             "cycles");
}

void
ComputeMemoryChartView::DrawVectorL1(ImDrawList* draw_list, ImVec2 origin)
{
    const auto& block   = m_vector_l1_block;
    float       block_x = origin.x + block.x;
    float       block_y = origin.y + block.y;

    DrawBlockRect(draw_list, {block_x, block_y},
                  {block_x + block.w, block_y + block.h});
    float cursor_y =
        DrawBlockHeader(draw_list, "Vector L1 Cache", block_x, block_y, block.w);

    cursor_y = DrawMetricRow(draw_list, block_x, cursor_y, block.w, "Hit:", VL1_HIT, "%");
    cursor_y =
        DrawMetricRow(draw_list, block_x, cursor_y, block.w, "Latency:", MEMCHART_METRIC_NA, "cycles");
    cursor_y = DrawMetricRow(draw_list, block_x, cursor_y, block.w,
                             "Coalescing:", VL1_COALESCE, "%");
    cursor_y =
        DrawMetricRow(draw_list, block_x, cursor_y, block.w, "Stall:", VL1_STALL, "%");
}

void
ComputeMemoryChartView::DrawScalarL1D(ImDrawList* draw_list, ImVec2 origin)
{
    const auto& block   = m_scalar_l1d_block;
    float       block_x = origin.x + block.x;
    float       block_y = origin.y + block.y;

    DrawBlockRect(draw_list, {block_x, block_y},
                  {block_x + block.w, block_y + block.h});
    float cursor_y = DrawBlockHeader(draw_list, "Scalar L1D Cache",
                                     block_x, block_y, block.w);

    cursor_y =
        DrawMetricRow(draw_list, block_x, cursor_y, block.w, "Hit:", SL1D_HIT, "%");
    cursor_y =
        DrawMetricRow(draw_list, block_x, cursor_y, block.w, "Latency:", SL1D_LAT, "cycles");
}

void
ComputeMemoryChartView::DrawInstrL1(ImDrawList* draw_list, ImVec2 origin)
{
    const auto& block   = m_instr_l1_block;
    float       block_x = origin.x + block.x;
    float       block_y = origin.y + block.y;

    DrawBlockRect(draw_list, {block_x, block_y},
                  {block_x + block.w, block_y + block.h});
    float cursor_y = DrawBlockHeader(draw_list, "Instr L1 Cache",
                                     block_x, block_y, block.w);

    cursor_y = DrawMetricRow(draw_list, block_x, cursor_y, block.w, "Hit:", IL1_HIT, "%");
    cursor_y =
        DrawMetricRow(draw_list, block_x, cursor_y, block.w, "Latency:", IL1_LAT, "cycles");
}

void
ComputeMemoryChartView::DrawL2(ImDrawList* draw_list, ImVec2 origin)
{
    const auto& block   = m_l2_block;
    float       block_x = origin.x + block.x;
    float       block_y = origin.y + block.y;

    DrawBlockRect(draw_list, {block_x, block_y},
                  {block_x + block.w, block_y + block.h});
    float cursor_y = DrawBlockHeader(draw_list, "L2 Cache",
                                     block_x, block_y, block.w);

    cursor_y = DrawMetricRow(draw_list, block_x, cursor_y, block.w, "Rd:", L2_RD);
    cursor_y = DrawMetricRow(draw_list, block_x, cursor_y, block.w, "Wr:", L2_WR);
    cursor_y = DrawMetricRow(draw_list, block_x, cursor_y, block.w, "Atomic:", L2_ATOMIC);
    cursor_y = DrawMetricRow(draw_list, block_x, cursor_y, block.w, "Hit:", L2_HIT, "%");

    cursor_y += HEADER_SEP_GAP;
    cursor_y = DrawBlockHeader(draw_list, "Latency",
                               block_x, cursor_y - BLOCK_TEXT_PAD, block.w);
    cursor_y =
        DrawMetricRow(draw_list, block_x, cursor_y, block.w, "Rd:", MEMCHART_METRIC_NA, "cycles");
    cursor_y =
        DrawMetricRow(draw_list, block_x, cursor_y, block.w, "Wr:", MEMCHART_METRIC_NA, "cycles");
}

void
ComputeMemoryChartView::DrawXGMIPCIe(ImDrawList* draw_list, ImVec2 origin)
{
    const auto& block   = m_xgmi_pcie_block;
    float       block_x = origin.x + block.x;
    float       block_y = origin.y + block.y;

    DrawBlockRect(draw_list, {block_x, block_y},
                  {block_x + block.w, block_y + block.h});

    ImU32 text_color = C().text_main;
    float center_y   = block_y + block.h * 0.3f;
    DrawCenteredText(draw_list, "xGMI /", block_x, center_y, block.w, text_color);
    DrawCenteredText(draw_list, "PCIe", block_x, center_y + ROW_HEIGHT,
                     block.w, text_color);
}

void
ComputeMemoryChartView::DrawFabric(ImDrawList* draw_list, ImVec2 origin)
{
    const auto& block   = m_fabric_block;
    float       block_x = origin.x + block.x;
    float       block_y = origin.y + block.y;

    DrawBlockRect(draw_list, {block_x, block_y},
                  {block_x + block.w, block_y + block.h});
    float cursor_y = DrawBlockHeader(draw_list, "Fabric",
                                     block_x, block_y, block.w);

    cursor_y = DrawMetricRow(draw_list, block_x, cursor_y, block.w, "Rd:", FABRIC_RD_LAT,
                             "cycles");
    cursor_y = DrawMetricRow(draw_list, block_x, cursor_y, block.w, "Wr:", FABRIC_WR_LAT,
                             "cycles");
    cursor_y = DrawMetricRow(draw_list, block_x, cursor_y, block.w,
                             "Atomic:", FABRIC_ATOMIC_LAT, "cycles");
}

void
ComputeMemoryChartView::DrawGMI(ImDrawList* draw_list, ImVec2 origin)
{
    const auto& block   = m_gmi_block;
    float       block_x = origin.x + block.x;
    float       block_y = origin.y + block.y;

    DrawBlockRect(draw_list, {block_x, block_y},
                  {block_x + block.w, block_y + block.h});

    float text_h = ImGui::CalcTextSize("GMI").y;
    DrawCenteredText(draw_list, "GMI",
                     block_x, block_y + (block.h - text_h) * 0.5f,
                     block.w, C().text_main);
}

void
ComputeMemoryChartView::DrawHBM(ImDrawList* draw_list, ImVec2 origin)
{
    const auto& block   = m_hbm_block;
    float       block_x = origin.x + block.x;
    float       block_y = origin.y + block.y;

    DrawBlockRect(draw_list, {block_x, block_y},
                  {block_x + block.w, block_y + block.h});
    float cursor_y = DrawBlockHeader(draw_list, "HBM",
                                     block_x, block_y, block.w);

    cursor_y = DrawMetricRow(draw_list, block_x, cursor_y, block.w, "Rd:", HBM_RD);
    cursor_y = DrawMetricRow(draw_list, block_x, cursor_y, block.w, "Wr:", HBM_WR);
}

// =============================================================================
// Connection arrows + labels
// =============================================================================

void
ComputeMemoryChartView::DrawConnections(ImDrawList* draw_list, ImVec2 origin)
{
    char  text_buf[64];

    // Local-to-screen coordinate helper
    auto screen = [&](float local_x, float local_y) -> ImVec2 {
        return {origin.x + local_x, origin.y + local_y};
    };

    // Draw horizontal arrow + label (label placed at source x + 5, above arrow).
    // Read and request flows point back toward the consumer (cache -> CU,
    // L2 -> instr L1), so swap the endpoints when the label is a read/request.
    auto ArrowWithLabel = [&](float src_x, float src_y,
                              float dst_x, float dst_y,
                              const char* label_text,
                              MemChartMetric metric_id) {
        ImU32       flow_color = ColorForFlowLabel(label_text);
        const bool  reverse =
            StartsWith(label_text, "Rd:") || StartsWith(label_text, "Req:");
        const ImVec2 tail = reverse ? screen(dst_x, dst_y) : screen(src_x, src_y);
        const ImVec2 tip  = reverse ? screen(src_x, src_y) : screen(dst_x, dst_y);
        DrawHorizontalArrow(draw_list, tail, tip, flow_color);
        ImVec2 label_pos = screen(src_x + 5, src_y - ARROW_LABEL_ABOVE);
        DrawFloatingTextBackground(draw_list, label_pos, label_text, flow_color);
        DrawTextWithTooltip(draw_list, label_pos,
                            flow_color, label_text, metric_id, true, true);
    };

    // --- Instr Buff stacks -> Trapezoids (3 horizontal lines per trap) ---
    {
        ImU32 wire_color = ApplyAlpha(C().read, 0.55f);

        const int   kNumStacks   = 3;
        const int kStackStep   = 8;
        const int   kNumTraps    = 8;
        const float kLineSpacing = 7.0f;

        float stack_right_x[3];
        for(int stack_idx = 0; stack_idx < kNumStacks; ++stack_idx)
        {
            float stack_offset = static_cast<float>(kNumStacks - 1 - stack_idx)
                                 * kStackStep;
            stack_right_x[stack_idx] = m_instr_buff_block.Right() + stack_offset;
        }

        float trap_height       = m_instr_dispatch_block.h
                                  / static_cast<float>(kNumTraps);
        float trap_left_x       = m_instr_dispatch_block.x;
        float line_offset_base  = -(kNumStacks - 1) * kLineSpacing * 0.5f;

        for(int trap_idx = 0; trap_idx < kNumTraps; ++trap_idx)
        {
            float trap_center_y = m_instr_dispatch_block.y
                                  + trap_idx * trap_height + trap_height * 0.5f;
            for(int stack_idx = 0; stack_idx < kNumStacks; ++stack_idx)
            {
                float line_y = trap_center_y
                               + line_offset_base + stack_idx * kLineSpacing;
                draw_list->AddLine(screen(stack_right_x[stack_idx], line_y),
                                   screen(trap_left_x, line_y),
                                   wire_color, 1.0f);
            }
        }
    }

    // --- Pipeline -> Caches ---

    // Active CUs -> LDS
    {
        float arrow_y = m_lds_block.MidY();
        snprintf(text_buf, sizeof(text_buf), "Req: %s", GetMetricText(LDS_REQ));
        ArrowWithLabel(m_active_cus_block.Right(), arrow_y,
                       m_lds_block.x, arrow_y, text_buf, LDS_REQ);
    }

    // Active CUs -> Vector L1 (3 arrows: Rd, Wr, Atomic)
    {
        float mid_y = m_vector_l1_block.MidY();
        float src_x = m_active_cus_block.Right();
        float dst_x = m_vector_l1_block.x;

        snprintf(text_buf, sizeof(text_buf), "Rd: %s", GetMetricText(VL1_RD));
        ArrowWithLabel(src_x, mid_y - ARROW_VERT_SPACE,
                       dst_x, mid_y - ARROW_VERT_SPACE, text_buf, VL1_RD);

        snprintf(text_buf, sizeof(text_buf), "Wr: %s", GetMetricText(VL1_WR));
        ArrowWithLabel(src_x, mid_y, dst_x, mid_y, text_buf, VL1_WR);

        snprintf(text_buf, sizeof(text_buf), "Atomic: %s",
                 GetMetricText(VL1_ATOMIC));
        ArrowWithLabel(src_x, mid_y + ARROW_VERT_SPACE,
                       dst_x, mid_y + ARROW_VERT_SPACE, text_buf, VL1_ATOMIC);
    }

    // Active CUs -> Scalar L1D
    {
        float arrow_y = m_scalar_l1d_block.MidY();
        snprintf(text_buf, sizeof(text_buf), "Rd: %s", GetMetricText(SL1D_RD));
        ArrowWithLabel(m_active_cus_block.Right(), arrow_y,
                       m_scalar_l1d_block.x, arrow_y, text_buf, SL1D_RD);
    }

    // Instr L1 -> Instr Buff (L-shaped: left from Instr L1, then up to Instr Buff)
    {
        float corner_x = m_instr_buff_block.MidX();
        float start_y  = m_instr_buff_block.Bottom();
        float end_y    = m_instr_l1_block.MidY() + 40;

        ImVec2 top_point    = screen(corner_x, start_y);
        ImVec2 corner_point = screen(corner_x, end_y);
        ImVec2 end_point    = screen(m_instr_l1_block.x, end_y);

        ImU32 arrow_color = C().read;
        DrawDashedFlowLine(draw_list, end_point, corner_point, arrow_color);
        DrawDashedFlowLine(draw_list, corner_point, top_point, arrow_color);

        float head = ARROW_HEAD_SIZE;
        draw_list->AddTriangleFilled(
            top_point,
            {top_point.x - head * 0.6f, top_point.y + head},
            {top_point.x + head * 0.6f, top_point.y + head},
            arrow_color);

        snprintf(text_buf, sizeof(text_buf), "Fetch: %s",
                 GetMetricText(IL1_FETCH));
        float label_x = (corner_x + m_instr_l1_block.x) * 0.5f;
        ImVec2 label_pos = screen(label_x, end_y - ARROW_LABEL_ABOVE);
        DrawFloatingTextBackground(draw_list, label_pos, text_buf, arrow_color);
        DrawTextWithTooltip(draw_list, label_pos,
                            arrow_color, text_buf, IL1_FETCH, true, true);
    }

    // --- Caches -> L2 ---

    // Vector L1 -> L2 (3 arrows)
    {
        float mid_y = m_vector_l1_block.MidY();
        float src_x = m_vector_l1_block.Right();
        float dst_x = m_l2_block.x;

        snprintf(text_buf, sizeof(text_buf), "Rd: %s", GetMetricText(VL1_L2_RD));
        ArrowWithLabel(src_x, mid_y - ARROW_VERT_SPACE,
                       dst_x, mid_y - ARROW_VERT_SPACE, text_buf, VL1_L2_RD);

        snprintf(text_buf, sizeof(text_buf), "Wr: %s", GetMetricText(VL1_L2_WR));
        ArrowWithLabel(src_x, mid_y, dst_x, mid_y, text_buf, VL1_L2_WR);

        snprintf(text_buf, sizeof(text_buf), "Atomic: %s",
                 GetMetricText(VL1_L2_ATOMIC));
        ArrowWithLabel(src_x, mid_y + ARROW_VERT_SPACE,
                       dst_x, mid_y + ARROW_VERT_SPACE, text_buf, VL1_L2_ATOMIC);
    }

    // Scalar L1D -> L2 (3 arrows)
    {
        float mid_y = m_scalar_l1d_block.MidY();
        float src_x = m_scalar_l1d_block.Right();
        float dst_x = m_l2_block.x;

        snprintf(text_buf, sizeof(text_buf), "Rd: %s", GetMetricText(SL1D_L2_RD));
        ArrowWithLabel(src_x, mid_y - ARROW_VERT_SPACE,
                       dst_x, mid_y - ARROW_VERT_SPACE, text_buf, SL1D_L2_RD);

        snprintf(text_buf, sizeof(text_buf), "Wr: %s", GetMetricText(SL1D_L2_WR));
        ArrowWithLabel(src_x, mid_y, dst_x, mid_y, text_buf, SL1D_L2_WR);

        snprintf(text_buf, sizeof(text_buf), "Atomic: %s",
                 GetMetricText(SL1D_L2_ATOMIC));
        ArrowWithLabel(src_x, mid_y + ARROW_VERT_SPACE,
                       dst_x, mid_y + ARROW_VERT_SPACE, text_buf, SL1D_L2_ATOMIC);
    }

    // Instr L1 -> L2
    {
        float arrow_y = m_instr_l1_block.MidY();
        snprintf(text_buf, sizeof(text_buf), "Req: %s",
                 GetMetricText(IL1_L2_RD));
        ArrowWithLabel(m_instr_l1_block.Right(), arrow_y,
                       m_l2_block.x, arrow_y, text_buf, IL1_L2_RD);
    }

    // --- L2 -> Fabric ---
    {
        float mid_y = m_fabric_block.MidY();
        float src_x = m_l2_block.Right();
        float dst_x = m_fabric_block.x;

        snprintf(text_buf, sizeof(text_buf), "Rd: %s",
                 GetMetricText(FABRIC_L2_RD));
        ArrowWithLabel(src_x, mid_y - ARROW_VERT_SPACE,
                       dst_x, mid_y - ARROW_VERT_SPACE, text_buf, FABRIC_L2_RD);

        snprintf(text_buf, sizeof(text_buf), "Wr: %s",
                 GetMetricText(FABRIC_L2_WR));
        ArrowWithLabel(src_x, mid_y, dst_x, mid_y, text_buf, FABRIC_L2_WR);

        snprintf(text_buf, sizeof(text_buf), "Atomic: %s",
                 GetMetricText(FABRIC_L2_ATOMIC));
        ArrowWithLabel(src_x, mid_y + ARROW_VERT_SPACE,
                       dst_x, mid_y + ARROW_VERT_SPACE, text_buf, FABRIC_L2_ATOMIC);
    }

    // --- Fabric -> HBM ---
    {
        float mid_y     = m_hbm_block.MidY();
        float src_x     = m_fabric_block.Right();
        float dst_x     = m_hbm_block.x;
        float half_space = ARROW_VERT_SPACE * 0.5f;

        snprintf(text_buf, sizeof(text_buf), "Rd: %s", GetMetricText(HBM_RD));
        ArrowWithLabel(src_x, mid_y - half_space,
                       dst_x, mid_y - half_space, text_buf, HBM_RD);

        snprintf(text_buf, sizeof(text_buf), "Wr: %s", GetMetricText(HBM_WR));
        ArrowWithLabel(src_x, mid_y + half_space,
                       dst_x, mid_y + half_space, text_buf, HBM_WR);
    }
}

}  // namespace View
}  // namespace RocProfVis
