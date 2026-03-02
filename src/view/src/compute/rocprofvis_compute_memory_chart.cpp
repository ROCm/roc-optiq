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
#include <cstdio>
#include <imgui.h>

namespace RocProfVis
{
namespace View
{

// =============================================================================
// Layout constants
// =============================================================================
static constexpr float CHART_PADDING     = 18.0f;
static constexpr float BLOCK_GAP         = 14.0f;
static constexpr float ARROW_COLUMN_GAP  = 160.0f;

static constexpr uint32_t MEMCHART_CATEGORY_ID = 3;
static constexpr uint32_t MEMCHART_TABLE_ID    = 1;

static constexpr float BLOCK_ROUNDING    = 8.0f;
static constexpr float BLOCK_TEXT_PAD    = 10.0f;
static constexpr float ROW_HEIGHT        = 20.0f;
static constexpr float HEADER_SEP_GAP    = 6.0f;

static constexpr float ARROW_THICKNESS   = 2.0f;
static constexpr float ARROW_HEAD_SIZE   = 5.0f;
static constexpr float ARROW_LABEL_ABOVE = 21.0f;
static constexpr float ARROW_VERT_SPACE  = 22.0f;

// =============================================================================
// Helpers
// =============================================================================

static SettingsManager&
Settings()
{
    return SettingsManager::GetInstance();
}

static std::string
FormatMetricValue(double value)
{
    if(value != value) return "-";
    return compact_number_format(value);
}

static std::string
FormatMetricValueRaw(double value)
{
    if(value != value) return "-";
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.2f", value);
    return std::string(buf);
}


// Rounded-rect block with fill + border
static void
DrawBlockRect(ImDrawList* draw_list, ImVec2 top_left, ImVec2 bottom_right)
{
    draw_list->AddRectFilled(top_left, bottom_right,
                             Settings().GetColor(Colors::kBgPanel), BLOCK_ROUNDING);
    draw_list->AddRect(top_left, bottom_right,
                       Settings().GetColor(Colors::kBorderGray), BLOCK_ROUNDING, 0, 1.0f);
}

// Header text + thin separator line. Returns cursor_y below the separator.
static float
DrawBlockHeader(ImDrawList* draw_list, const char* title,
                float block_x, float block_y, float block_w)
{
    float text_y = block_y + BLOCK_TEXT_PAD;
    draw_list->AddText(ImVec2(block_x + BLOCK_TEXT_PAD, text_y),
                       Settings().GetColor(Colors::kTextMain), title);

    float line_y = text_y + ImGui::CalcTextSize(title).y + 3.0f;
    draw_list->AddLine(ImVec2(block_x + BLOCK_TEXT_PAD, line_y),
                       ImVec2(block_x + block_w - BLOCK_TEXT_PAD, line_y),
                       Settings().GetColor(Colors::kBorderGray), 1.0f);
    return line_y + HEADER_SEP_GAP;
}

// Horizontal arrow pointing right
static void
DrawHorizontalArrow(ImDrawList* draw_list, ImVec2 from, ImVec2 to)
{
    ImU32 color = Settings().GetColor(Colors::kArrowColor);
    draw_list->AddLine(from, to, color, ARROW_THICKNESS);
    float head = ARROW_HEAD_SIZE;
    draw_list->AddTriangleFilled(to,
                                 ImVec2(to.x - head, to.y - head * 0.6f),
                                 ImVec2(to.x - head, to.y + head * 0.6f),
                                 color);
}

// Horizontally centered text within [x, x+w]
static void
DrawCenteredText(ImDrawList* draw_list, const char* text,
                 float region_x, float text_y, float region_w, ImU32 color)
{
    float text_w = ImGui::CalcTextSize(text).x;
    draw_list->AddText(ImVec2(region_x + (region_w - text_w) * 0.5f, text_y),
                       color, text);
}

// Small inset card (frame-colored rect + dim text)
static void
DrawInsetCard(ImDrawList* draw_list, float card_x, float card_y,
              float card_w, float card_h, const char* text)
{
    ImVec2 top_left(card_x, card_y);
    ImVec2 bottom_right(card_x + card_w, card_y + card_h);
    draw_list->AddRectFilled(top_left, bottom_right,
                             Settings().GetColor(Colors::kBgFrame), 4.0f);
    draw_list->AddRect(top_left, bottom_right,
                       Settings().GetColor(Colors::kBorderGray), 4.0f, 0, 1.0f);

    float text_h = ImGui::CalcTextSize(text).y;
    draw_list->AddText(ImVec2(card_x + BLOCK_TEXT_PAD,
                              card_y + (card_h - text_h) * 0.5f),
                       Settings().GetColor(Colors::kTextDim), text);
}

// =============================================================================
// Core — data loading
// =============================================================================

ComputeMemoryChartView::ComputeMemoryChartView(DataProvider& data_provider, std::shared_ptr<ComputeSelection> compute_selection)
: m_data_provider(data_provider)
, m_compute_selection(compute_selection)
, m_client_id(IdGenerator::GetInstance().GenerateId())
{
    m_values.fill("-");
    m_metric_ptrs.resize(MEMCHART_METRIC_COUNT, nullptr);
}

ComputeMemoryChartView::~ComputeMemoryChartView() {}

const char*
ComputeMemoryChartView::GetMetricText(MemChartMetric metric) const
{
    if(metric >= 0 && metric < MEMCHART_METRIC_COUNT)
        return m_values[metric].c_str();
    return "-";
}

void
ComputeMemoryChartView::FetchMemChartMetrics()
{
    m_values.fill("-");
    m_metric_ptrs.assign(MEMCHART_METRIC_COUNT, nullptr);

    m_data_provider.ComputeModel().ClearMetricValues(m_client_id);

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
        m_data_provider.ComputeModel().GetMetricsData(m_client_id, kernel_id);
    
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
            m_values[i] = "-";
        }
    }
}

// =============================================================================
// Render — single draw list, no child windows per block
// =============================================================================

void
ComputeMemoryChartView::Render()
{
    // Outer scrollable container (the only child window in the whole chart)
    ImGui::PushStyleColor(ImGuiCol_ChildBg, Settings().GetColor(Colors::kBgMain));
    ImGui::PushStyleColor(ImGuiCol_Border, Settings().GetColor(Colors::kBorderGray));

    ImGui::BeginChild("MemoryChart", ImVec2(0, 0), ImGuiChildFlags_Borders,
                      ImGuiWindowFlags_HorizontalScrollbar);
    ImGui::PopStyleColor(2);

    ImDrawList* draw_list       = ImGui::GetWindowDrawList();
    ImVec2      window_position = ImGui::GetCursorScreenPos();

    // -----------------------------------------------------------------
    // 1. Compute layout — assign positions to every block
    // -----------------------------------------------------------------
    ComputeLayout();

    // -----------------------------------------------------------------
    // 2. Draw every block
    // -----------------------------------------------------------------
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

    // -----------------------------------------------------------------
    // 2. Draw connection arrows + labels (on top to allow stacks in instr buff to connect)
    // -----------------------------------------------------------------
    DrawConnections(draw_list, window_position);

    // -----------------------------------------------------------------
    // 4. Set canvas size so scrollbars cover the entire diagram
    // -----------------------------------------------------------------
    float canvas_w = m_hbm_block.Right() + CHART_PADDING;
    float canvas_h = std::max({m_instr_buff_block.Bottom(),
                               m_instr_dispatch_block.Bottom(),
                               m_instr_l1_block.Bottom(),
                               m_gmi_block.Bottom()}) + CHART_PADDING;
    ImGui::SetCursorPos(ImVec2(canvas_w, canvas_h));
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

// Dim label on left, bright value on right. Returns cursor_y for next row.
float
ComputeMemoryChartView::DrawMetricRow(ImDrawList* draw_list, float block_x, float cursor_y, float block_w,
              const char* label, MemChartMetric metric_id, const char* unit /*= ""*/)
{
    DrawTextWithTooltip(draw_list, {block_x + BLOCK_TEXT_PAD, cursor_y},
                        Settings().GetColor(Colors::kTextDim),
                        label, metric_id, true, false);

    float value_x = block_x + block_w * 0.48f;
    if(unit[0] != '\0')
    {
        char text_buf[64];
        snprintf(text_buf, sizeof(text_buf), "%s %s", GetMetricText(metric_id), unit);
        DrawTextWithTooltip(draw_list, {value_x, cursor_y},
                            Settings().GetColor(Colors::kTextMain),
                            text_buf, metric_id, false, true);
    }
    else
    {
        DrawTextWithTooltip(draw_list, {value_x, cursor_y},
                            Settings().GetColor(Colors::kTextMain),
                            GetMetricText(metric_id), metric_id, false, true);
    }
    return cursor_y + ROW_HEIGHT;
}

void
ComputeMemoryChartView::ShowMetricTooltip(ImVec2 hover_min, ImVec2 hover_max,
                                          MemChartMetric metric_id,
                                          bool show_description, bool show_raw_value)
{
    if(!ImGui::IsMouseHoveringRect(hover_min, hover_max)) return;
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

// =============================================================================
// Block drawing — each reads its stored ChartBlock and draws via draw list
// =============================================================================

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

    ImU32 dim_color  = Settings().GetColor(Colors::kTextDim);
    ImU32 main_color = Settings().GetColor(Colors::kTextMain);
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
        "SALU", "SMEM", "VALU", "MFMA", "VMEM", "LDS", "GWS", "Br"
    };
    const MemChartMetric pill_metrics[] = {
        SALU, SMEM, VALU, MFMA, VMEM, LDS, GWS, BR
    };

    ImU32 arrow_color     = Settings().GetColor(Colors::kArrowColor);
    ImU32 pill_border_col = IM_COL32(0, 0, 0, 255);
    ImU32 title_bg_col    = Settings().GetColor(Colors::kBorderGray);
    ImU32 value_bg_col    = IM_COL32(255, 255, 255, 255);
    ImU32 title_text_col  = Settings().GetColor(Colors::kTextMain);
    ImU32 value_text_col  = IM_COL32(0, 0, 0, 255);

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

        // Arrow line + head
        draw_list->AddLine(arrow_from, arrow_to, arrow_color, 1.0f);
        float head = ARROW_HEAD_SIZE;
        draw_list->AddTriangleFilled(
            arrow_to,
            {arrow_to.x - head, arrow_to.y - head * 0.6f},
            {arrow_to.x - head, arrow_to.y + head * 0.6f},
            arrow_color);

        float pill_right_edge = arrow_to.x - head - 22.0f;
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
    ImU32 fill_color   = Settings().GetColor(Colors::kBgPanel);
    ImU32 border_color = Settings().GetColor(Colors::kBorderGray);
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
                        Settings().GetColor(Colors::kTextDim), "Active CUs",
                        NUM_CUS, true, false);
    cursor_y += ROW_HEIGHT;
    DrawTextWithTooltip(draw_list, {block_x + BLOCK_TEXT_PAD, cursor_y},
                        Settings().GetColor(Colors::kTextMain),
                        GetMetricText(NUM_CUS), NUM_CUS, false, true);
    cursor_y += ROW_HEIGHT + 5;

    // Thin separator
    draw_list->AddLine({block_x + BLOCK_TEXT_PAD, cursor_y},
                       {block_x + block.w - BLOCK_TEXT_PAD, cursor_y},
                       Settings().GetColor(Colors::kBorderGray), 1.0f);
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
    cursor_y = DrawMetricRow(draw_list, block_x, cursor_y, block.w, "Lat:", LDS_LATENCY,
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
        DrawMetricRow(draw_list, block_x, cursor_y, block.w, "Lat:", VL1_LAT, "cycles");
    cursor_y = DrawMetricRow(draw_list, block_x, cursor_y, block.w,
                             "Coales:", VL1_COALESCE, "%");
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
        DrawMetricRow(draw_list, block_x, cursor_y, block.w, "Lat:", SL1D_LAT, "cycles");
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
        DrawMetricRow(draw_list, block_x, cursor_y, block.w, "Lat:", IL1_LAT, "cycles");
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
        DrawMetricRow(draw_list, block_x, cursor_y, block.w, "Rd:", L2_RD_LAT, "cycles");
    cursor_y =
        DrawMetricRow(draw_list, block_x, cursor_y, block.w, "Wr:", L2_WR_LAT, "cycles");
}

void
ComputeMemoryChartView::DrawXGMIPCIe(ImDrawList* draw_list, ImVec2 origin)
{
    const auto& block   = m_xgmi_pcie_block;
    float       block_x = origin.x + block.x;
    float       block_y = origin.y + block.y;

    DrawBlockRect(draw_list, {block_x, block_y},
                  {block_x + block.w, block_y + block.h});

    ImU32 text_color = Settings().GetColor(Colors::kTextMain);
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
                     block.w, Settings().GetColor(Colors::kTextMain));
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
    ImU32 label_color = Settings().GetColor(Colors::kTextDim);
    char  text_buf[64];

    // Local-to-screen coordinate helper
    auto screen = [&](float local_x, float local_y) -> ImVec2 {
        return {origin.x + local_x, origin.y + local_y};
    };

    // Draw horizontal arrow + label (label placed at source x + 5, above arrow)
    auto ArrowWithLabel = [&](float src_x, float src_y,
                              float dst_x, float dst_y,
                              const char* label_text,
                              MemChartMetric metric_id) {
        DrawHorizontalArrow(draw_list, screen(src_x, src_y), screen(dst_x, dst_y));
        DrawTextWithTooltip(draw_list, screen(src_x + 5, src_y - ARROW_LABEL_ABOVE),
                            label_color, label_text, metric_id, true, true);
    };

    // --- Instr Buff stacks -> Trapezoids (3 horizontal lines per trap) ---
    {
        ImU32 wire_color = Settings().GetColor(Colors::kArrowColor);

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

        ImU32 arrow_color = Settings().GetColor(Colors::kArrowColor);
        draw_list->AddLine(end_point, corner_point, arrow_color, ARROW_THICKNESS);
        draw_list->AddLine(corner_point, top_point, arrow_color, ARROW_THICKNESS);

        float head = ARROW_HEAD_SIZE;
        draw_list->AddTriangleFilled(
            top_point,
            {top_point.x - head * 0.6f, top_point.y + head},
            {top_point.x + head * 0.6f, top_point.y + head},
            arrow_color);

        snprintf(text_buf, sizeof(text_buf), "Fetch: %s",
                 GetMetricText(IL1_FETCH));
        float label_x = (corner_x + m_instr_l1_block.x) * 0.5f;
        DrawTextWithTooltip(draw_list, screen(label_x, end_y - ARROW_LABEL_ABOVE),
                            label_color, text_buf, IL1_FETCH, true, true);
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
