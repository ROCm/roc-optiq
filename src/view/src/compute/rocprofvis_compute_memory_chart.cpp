// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_compute_memory_chart.h"
#include "rocprofvis_data_provider.h"
#include "rocprofvis_settings_manager.h"
#include <algorithm>
#include <cstdio>
#include <imgui.h>

namespace RocProfVis
{
namespace View
{

// =============================================================================
// Layout constants — tweak these to adjust the entire diagram
// =============================================================================
static constexpr float PAD       = 14.0f;  // padding from edges
static constexpr float BLOCK_GAP = 12.0f;  // gap between adjacent blocks
static constexpr float ARROW_GAP =
    105.0f;  // gap between columns (room for arrows + labels)

// Category and table IDs for the memory chart metrics
static constexpr uint32_t MEMCHART_CATEGORY_ID = 3;
static constexpr uint32_t MEMCHART_TABLE_ID    = 1;

// Visual styling constants
static constexpr float BLOCK_ROUNDING  = 8.0f;
static constexpr float ARROW_THICKNESS = 2.0f;
static constexpr float ARROW_HEAD_SZ   = 5.0f;

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
    if(value != value) return "-";  // NaN check
    int64_t ival = static_cast<int64_t>(value);
    if(value == static_cast<double>(ival))
    {
        return std::to_string(ival);
    }
    char buf[32];
    snprintf(buf, sizeof(buf), "%.1f", value);
    return buf;
}

// Styled metric row: dim label, bright value
static void
MetricRow(const char* label, const char* value, const char* unit = "")
{
    ImU32 dimCol  = Settings().GetColor(Colors::kTextDim);
    ImU32 mainCol = Settings().GetColor(Colors::kTextMain);

    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::PushStyleColor(ImGuiCol_Text, dimCol);
    ImGui::TextUnformatted(label);
    ImGui::PopStyleColor();

    ImGui::TableNextColumn();
    ImGui::PushStyleColor(ImGuiCol_Text, mainCol);
    if(unit[0] != '\0')
    {
        ImGui::Text("%s %s", value, unit);
    }
    else
    {
        ImGui::TextUnformatted(value);
    }
    ImGui::PopStyleColor();
}

// Push themed visual style for chart blocks
static void
PushBlockStyle()
{
    ImGui::PushStyleColor(ImGuiCol_ChildBg, Settings().GetColor(Colors::kBgPanel));
    ImGui::PushStyleColor(ImGuiCol_Border, Settings().GetColor(Colors::kBorderGray));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, BLOCK_ROUNDING);
}

static void
PopBlockStyle()
{
    ImGui::PopStyleVar(1);
    ImGui::PopStyleColor(2);
}

// Section header: bold title with a thin separator below
static void
BlockHeader(const char* title)
{
    ImGui::PushStyleColor(ImGuiCol_Text, Settings().GetColor(Colors::kTextMain));
    ImGui::TextUnformatted(title);
    ImGui::PopStyleColor();

    ImDrawList* dl  = ImGui::GetWindowDrawList();
    ImVec2      pos = ImGui::GetCursorScreenPos();
    float       w   = ImGui::GetContentRegionAvail().x;
    dl->AddLine(pos, ImVec2(pos.x + w, pos.y), Settings().GetColor(Colors::kBorderGray),
                1.0f);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 5);
}

// Centered text helper for simple label blocks
static void
CenteredText(const char* text)
{
    float avail = ImGui::GetContentRegionAvail().x;
    float textW = ImGui::CalcTextSize(text).x;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (avail - textW) * 0.5f);
    ImGui::TextUnformatted(text);
}

// Themed arrow drawn between blocks
static void
DrawArrow(ImDrawList* dl, float x1, float y1, float x2, float y2, ImVec2 origin)
{
    ImVec2 from(origin.x + x1, origin.y + y1);
    ImVec2 to(origin.x + x2, origin.y + y2);
    ImU32  color = Settings().GetColor(Colors::kArrowColor);
    dl->AddLine(from, to, color, ARROW_THICKNESS);
    float sz = ARROW_HEAD_SZ;
    dl->AddTriangleFilled(to, ImVec2(to.x - sz, to.y - sz * 0.6f),
                          ImVec2(to.x - sz, to.y + sz * 0.6f), color);
}

// =============================================================================
// Core
// =============================================================================

ComputeMemoryChartView::ComputeMemoryChartView(DataProvider& data_provider)
: m_data_provider(data_provider)
{
    m_values.fill("-");
}

ComputeMemoryChartView::~ComputeMemoryChartView() {}

const char*
ComputeMemoryChartView::Val(MemChartMetric m) const
{
    if(m >= 0 && m < MEMCHART_METRIC_COUNT)
    {
        return m_values[m].c_str();
    }
    return "-";
}

void
ComputeMemoryChartView::FetchMemChartMetrics(uint32_t                     workload_id,
                                             const std::vector<uint32_t>& kernel_ids)
{
    m_values.fill("-");
    m_data_provider.ComputeModel().ClearMetricValues();

    std::vector<MetricsRequestParams::MetricID> metric_ids;
    metric_ids.push_back({ MEMCHART_CATEGORY_ID, MEMCHART_TABLE_ID, std::nullopt });

    m_data_provider.FetchMetrics(
        MetricsRequestParams(workload_id, kernel_ids, metric_ids));
}

void
ComputeMemoryChartView::Update()
{
    const std::vector<std::unique_ptr<MetricValue>>& metrics =
        m_data_provider.ComputeModel().GetMetricsData();

    for(const std::unique_ptr<MetricValue>& metric : metrics)
    {
        if(!metric) continue;
        if(metric->entry.category_id != MEMCHART_CATEGORY_ID) continue;
        if(metric->entry.table_id != MEMCHART_TABLE_ID) continue;

        uint32_t id = metric->entry.id;
        if(id < MEMCHART_METRIC_COUNT)
        {
            m_values[id] = FormatMetricValue(metric->value);
        }
    }
}

// =============================================================================
// Render — each block is positioned relative to the previous one
// =============================================================================

void
ComputeMemoryChartView::Render()
{
    // Outer container with themed background
    ImGui::PushStyleColor(ImGuiCol_ChildBg, Settings().GetColor(Colors::kBgMain));
    ImGui::PushStyleColor(ImGuiCol_Border, Settings().GetColor(Colors::kBorderGray));


      ImVec2 avail   = ImGui::GetContentRegionAvail();
    float  offsetX = std::max(0.0f, (avail.x - 1500.0f) * 0.5f);  // 1500 = diagram width
    ImGui::SetCursorPosX(offsetX);


    ImGui::BeginChild("MemoryChart", ImVec2(0, 0), ImGuiChildFlags_Borders,
                      ImGuiWindowFlags_HorizontalScrollbar);
    ImGui::PopStyleColor(2);

    // --- Pipeline column ---
    m_instrBuff     = RenderInstrBuff(PAD, PAD);
    m_instrDispatch = RenderInstrDispatch(m_instrBuff.Right() + BLOCK_GAP, PAD);
    m_activeCUs     = RenderActiveCUs(m_instrDispatch.Right() + BLOCK_GAP, PAD + 15);

    // --- Cache column (stacked vertically) ---
    float cacheX = m_activeCUs.Right() + ARROW_GAP;
    m_lds        = RenderLDS(cacheX, PAD);
    m_vectorL1   = RenderVectorL1Cache(cacheX, m_lds.Bottom() + BLOCK_GAP);
    m_scalarL1D  = RenderScalarL1DCache(cacheX, m_vectorL1.Bottom() + BLOCK_GAP);
    m_instrL1    = RenderInstrL1Cache(cacheX, m_scalarL1D.Bottom() + BLOCK_GAP);

    // --- L2 column (spans full cache height) ---
    float l2H = m_instrL1.Bottom() - PAD;
    m_l2      = RenderL2Cache(m_lds.Right() + ARROW_GAP, PAD, l2H);

    // --- Fabric column (stacked vertically) ---
    float fabricX = m_l2.Right() + ARROW_GAP;
    m_xgmiPcie    = RenderXGMIPCIe(fabricX, PAD);
    m_fabric      = RenderFabricBlock(fabricX, m_xgmiPcie.Bottom() + BLOCK_GAP);
    m_gmi         = RenderGMI(fabricX, m_fabric.Bottom() + BLOCK_GAP);

    // --- HBM (right of fabric) ---
    m_hbm = RenderHBM(m_fabric.Right() + ARROW_GAP / 2, m_fabric.y);

    // Arrows and labels between blocks
    RenderConnections();

    // Ensure scroll region covers the entire diagram
    float canvasW = m_hbm.Right() + PAD;
    float canvasH = m_instrBuff.Bottom();
    canvasH       = std::max(canvasH, m_instrDispatch.Bottom());
    canvasH       = std::max(canvasH, m_instrL1.Bottom());
    canvasH       = std::max(canvasH, m_gmi.Bottom());
    canvasH += PAD;
    ImGui::SetCursorPos(ImVec2(canvasW, canvasH));
    ImGui::Dummy(ImVec2(1, 1));

    ImGui::EndChild();
}

// =============================================================================
// Pipeline blocks
// =============================================================================

ChartBlock
ComputeMemoryChartView::RenderInstrBuff(float x, float y)
{
    const float w = 195, h = 460;
    ImGui::SetCursorPos(ImVec2(x, y));
    PushBlockStyle();
    ImGui::BeginChild("InstrBuff", ImVec2(w, h), ImGuiChildFlags_Borders);

    BlockHeader("Instr Buff");

    // Wave buffers — subtle inset cards
    ImGui::PushStyleColor(ImGuiCol_ChildBg, Settings().GetColor(Colors::kBgFrame));
    ImGui::PushStyleColor(ImGuiCol_Border, Settings().GetColor(Colors::kBorderGray));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 4.0f);

    ImGui::BeginChild("Wave0", ImVec2(-1, 30), ImGuiChildFlags_Borders);
    ImGui::PushStyleColor(ImGuiCol_Text, Settings().GetColor(Colors::kTextDim));
    ImGui::TextUnformatted("Wave 0 Instr buff");
    ImGui::PopStyleColor();
    ImGui::EndChild();

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 5);

    ImGui::BeginChild("WaveN", ImVec2(-1, 30), ImGuiChildFlags_Borders);
    ImGui::PushStyleColor(ImGuiCol_Text, Settings().GetColor(Colors::kTextDim));
    ImGui::TextUnformatted("Wave N-1 Instr buff");
    ImGui::PopStyleColor();
    ImGui::EndChild();

    ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 10);

    // Metrics
    ImGui::PushStyleColor(ImGuiCol_Text, Settings().GetColor(Colors::kTextDim));
    ImGui::TextUnformatted("Wave Occupancy:");
    ImGui::PopStyleColor();
    ImGui::PushStyleColor(ImGuiCol_Text, Settings().GetColor(Colors::kTextMain));
    ImGui::Text("%s per-GCD", Val(WAVEFRONT_OCCUPANCY));
    ImGui::PopStyleColor();

    ImGui::Spacing();

    ImGui::PushStyleColor(ImGuiCol_Text, Settings().GetColor(Colors::kTextDim));
    ImGui::TextUnformatted("Wave Life:");
    ImGui::PopStyleColor();
    ImGui::PushStyleColor(ImGuiCol_Text, Settings().GetColor(Colors::kTextMain));
    ImGui::Text("%s cycles", Val(WAVE_LIFE));
    ImGui::PopStyleColor();

    ImGui::EndChild();
    PopBlockStyle();
    return { x, y, w, h };
}

ChartBlock
ComputeMemoryChartView::RenderInstrDispatch(float x, float y)
{
    const float w = 335, h = 460, trap_w = 50;
    const int   num_traps = 8;
    const float trap_h    = h / static_cast<float>(num_traps);

    ImGui::SetCursorPos(ImVec2(x, y));
    PushBlockStyle();
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));

    ImGui::BeginChild("InstrDispatch_Container", ImVec2(w, h), ImGuiChildFlags_None);

    // Store screen-space positions for cross-child arrow drawing
    ImVec2 trapTopPositions[8];
    ImVec2 trapMainScreenPos;

    // Lambda to draw a single trapezoid
    auto drawTrapezoid = [&](int index) {
        char id[64];
        snprintf(id, sizeof(id), "Trap_%d", index);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
        ImGui::BeginChild(id, ImVec2(trap_w/2, trap_h));
        {
            ImDrawList* dl          = ImGui::GetWindowDrawList();
            ImVec2      pos         = ImGui::GetCursorScreenPos();
            float       narrow_side = trap_h * (150.0f / 460.0f);
            ImVec2      pts[4]      = {
                ImVec2(pos.x, pos.y),
                ImVec2(pos.x + trap_w, pos.y + (trap_h - narrow_side) * 0.5f),
                ImVec2(pos.x + trap_w, pos.y + (trap_h + narrow_side) * 0.5f),
                ImVec2(pos.x, pos.y + trap_h)
            };
            dl->AddConvexPolyFilled(pts, 4, Settings().GetColor(Colors::kBgPanel));
            dl->AddPolyline(pts, 4, Settings().GetColor(Colors::kBorderGray),
                            ImDrawFlags_Closed, 1.0f);
            // Store midpoint of the visible right edge for connecting arrows
            // (child is trap_w/2 wide, so the shape is clipped there)
            trapTopPositions[index] =
                ImVec2(pos.x + trap_w / 2.0f, pos.y + trap_h * 0.5f);
        }
        ImGui::EndChild();
        ImGui::PopStyleColor();
    };

    // Trapezoid column — 8 trapezoids stacked vertically
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
    ImGui::BeginChild("TrapColumn", ImVec2(trap_w , h));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
    for(int i = 0; i < num_traps; ++i)
        drawTrapezoid(i);
    ImGui::PopStyleVar();
    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::SameLine();


     // Trapezoid main — 1 trapezoid
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
    ImGui::BeginChild("TrapMain", ImVec2(trap_w, h));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
    ImDrawList* dl          = ImGui::GetWindowDrawList();
    ImVec2      pos         = ImGui::GetCursorScreenPos();
    float       narrow_side = 390.0f;  // right side (short)
    ImVec2      pts[4]      = {
        ImVec2(pos.x, pos.y),  // top-left  (wide side)
        ImVec2(pos.x + trap_w,
                         pos.y + (h - narrow_side) * 0.5f),  // top-right  (short side)
        ImVec2(pos.x + trap_w,
                         pos.y + (h + narrow_side) * 0.5f),  // bot-right  (short side)
        ImVec2(pos.x, pos.y + h)                   // bot-left  (wide side)
    };
    dl->AddConvexPolyFilled(pts, 4, Settings().GetColor(Colors::kBgPanel));
    dl->AddPolyline(pts, 4, Settings().GetColor(Colors::kBorderGray), ImDrawFlags_Closed,
                    1.0f);
    trapMainScreenPos = pos;
    ImGui::PopStyleVar();
    ImGui::EndChild();
    ImGui::PopStyleColor();

    // Draw connecting arrows from each small trapezoid to TrapMain at the container level
    // so they aren't clipped by the individual child window boundaries
    {
        ImDrawList* containerDl = ImGui::GetWindowDrawList();
        ImU32       arrowColor  = Settings().GetColor(Colors::kArrowColor);
        ImU32       textColor   = Settings().GetColor(Colors::kTextMain);

        // Labels and metrics matching the original dispatch table rows
        const char*          labels[]  = { "SALU", "SMEM", "VALU", "MFMA",
                                           "VMEM", "LDS",  "GWS",  "Br" };
        const MemChartMetric metrics[] = { SALU, SMEM, VALU, MFMA, VMEM, LDS, GWS, BR };

        const float pillPadX = 4.0f;   // horizontal padding inside pillbox
        const float pillPadY = 1.0f;   // vertical padding inside pillbox
        const float pillRound = 4.0f;  // corner rounding
        const float pillGap   = 1.0f;  // gap between title and value pills

        ImU32 borderCol   = IM_COL32(0, 0, 0, 255);
        ImU32 titleBgCol  = Settings().GetColor(Colors::kBorderGray);
        ImU32 valueBgCol  = IM_COL32(255, 255, 255, 255);
        ImU32 titleTxtCol = Settings().GetColor(Colors::kTextMain);
        ImU32 valueTxtCol = IM_COL32(0, 0, 0, 255);

        // Pre-pass: find the widest pill across all entries so they're uniform
        float uniformW = 0.0f;
        float uniformH = 0.0f;
        for(int i = 0; i < num_traps; ++i)
        {
            float tw = ImGui::CalcTextSize(labels[i]).x;
            float vw = ImGui::CalcTextSize(Val(metrics[i])).x;
            uniformW = std::max(uniformW, std::max(tw, vw));
        }
        uniformW += pillPadX * 2.0f;
        uniformH  = ImGui::CalcTextSize("X").y + pillPadY * 2.0f;  // all rows same height



        //Actual rendering of pills. 
        for(int i = 0; i < num_traps; ++i)
        {
            ImVec2 lineStart = trapTopPositions[i];
            ImVec2 lineEnd   = ImVec2(trapMainScreenPos.x + 250, trapTopPositions[i].y);

            containerDl->AddLine(lineStart, lineEnd, arrowColor, 1.0f);

            // Arrowhead at lineEnd pointing right
            float sz = ARROW_HEAD_SZ;
            containerDl->AddTriangleFilled(
                lineEnd,
                ImVec2(lineEnd.x - sz, lineEnd.y - sz * 0.6f),
                ImVec2(lineEnd.x - sz, lineEnd.y + sz * 0.6f),
                arrowColor);

            // Place the pill stack right before the arrowhead
            float pillRight = lineEnd.x - ARROW_HEAD_SZ - 20.0f;
            float midY      = lineStart.y;  // arrow Y

            ImVec2 titleSize = ImGui::CalcTextSize(labels[i]);
            ImVec2 valSize   = ImGui::CalcTextSize(Val(metrics[i]));

            // Title pill: above the arrow line
            ImVec2 tMin(pillRight - uniformW, midY - uniformH - pillGap);
            ImVec2 tMax(pillRight,            midY - pillGap);
            containerDl->AddRectFilled(tMin, tMax, titleBgCol, pillRound);
            containerDl->AddRect(tMin, tMax, borderCol, pillRound, 0, 1.0f);
            containerDl->AddText(
                ImVec2(tMin.x + (uniformW - titleSize.x) * 0.5f, tMin.y + pillPadY),
                titleTxtCol, labels[i]);

            // Value pill: below the arrow line
            ImVec2 vMin(pillRight - uniformW, midY + pillGap);
            ImVec2 vMax(pillRight,            midY + pillGap + uniformH);
            containerDl->AddRectFilled(vMin, vMax, valueBgCol, pillRound);
            containerDl->AddRect(vMin, vMax, borderCol, pillRound, 0, 1.0f);
            containerDl->AddText(
                ImVec2(vMin.x + (uniformW - valSize.x) * 0.5f, vMin.y + pillPadY),
                valueTxtCol, Val(metrics[i]));
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
    PopBlockStyle();
    return { x, y, w, h };
}

ChartBlock
ComputeMemoryChartView::RenderActiveCUs(float x, float y)
{
    const float w = 170, h = 380;
    ImGui::SetCursorPos(ImVec2(x, y));
    PushBlockStyle();
    ImGui::BeginChild("ActiveCUs", ImVec2(w, h), ImGuiChildFlags_Borders);

    BlockHeader("Exec");

    ImGui::PushStyleColor(ImGuiCol_Text, Settings().GetColor(Colors::kTextDim));
    ImGui::TextUnformatted("Active CUs");
    ImGui::PopStyleColor();

    ImGui::PushStyleColor(ImGuiCol_Text, Settings().GetColor(Colors::kTextMain));
    ImGui::Text("%s", Val(NUM_CUS));
    ImGui::PopStyleColor();

    ImDrawList* dl     = ImGui::GetWindowDrawList();
    ImVec2      sepPos = ImGui::GetCursorScreenPos();
    float       sepW   = ImGui::GetContentRegionAvail().x;
    dl->AddLine(sepPos, ImVec2(sepPos.x + sepW, sepPos.y),
                Settings().GetColor(Colors::kBorderGray), 1.0f);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 5);

    if(ImGui::BeginTable("##cus", 2))
    {
        MetricRow("VGPRs:", Val(VGPR));
        MetricRow("SGPRs:", Val(SGPR));
        MetricRow("LDS Alloc:", Val(LDS_ALLOCATION));
        MetricRow("Scratch Alloc:", Val(SCRATCH_ALLOCATION));
        MetricRow("Wavefronts:", Val(WAVEFRONTS));
        MetricRow("Workgroups:", Val(WORKGROUPS));
        ImGui::EndTable();
    }

    ImGui::EndChild();
    PopBlockStyle();
    return { x, y, w, h };
}

// =============================================================================
// Cache blocks
// =============================================================================

ChartBlock
ComputeMemoryChartView::RenderLDS(float x, float y)
{
    const float w = 195, h = 100;
    ImGui::SetCursorPos(ImVec2(x, y));
    PushBlockStyle();
    ImGui::BeginChild("LDS_Block", ImVec2(w, h), ImGuiChildFlags_Borders);

    BlockHeader("LDS");

    if(ImGui::BeginTable("##lds", 2))
    {
        MetricRow("Util:", Val(LDS_UTIL), "%");
        MetricRow("Lat:", Val(LDS_LATENCY), "cycles");
        ImGui::EndTable();
    }

    ImGui::EndChild();
    PopBlockStyle();
    return { x, y, w, h };
}

ChartBlock
ComputeMemoryChartView::RenderVectorL1Cache(float x, float y)
{
    const float w = 195, h = 155;
    ImGui::SetCursorPos(ImVec2(x, y));
    PushBlockStyle();
    ImGui::BeginChild("VectorL1", ImVec2(w, h), ImGuiChildFlags_Borders);

    BlockHeader("Vector L1 Cache");

    if(ImGui::BeginTable("##vl1", 2))
    {
        MetricRow("Hit:", Val(VL1_HIT), "%");
        MetricRow("Coales:", Val(VL1_COALESCE), "%");
        MetricRow("Stall:", Val(VL1_STALL), "%");
        ImGui::EndTable();
    }

    ImGui::EndChild();
    PopBlockStyle();
    return { x, y, w, h };
}

ChartBlock
ComputeMemoryChartView::RenderScalarL1DCache(float x, float y)
{
    const float w = 195, h = 100;
    ImGui::SetCursorPos(ImVec2(x, y));
    PushBlockStyle();
    ImGui::BeginChild("ScalarL1D", ImVec2(w, h), ImGuiChildFlags_Borders);

    BlockHeader("Scalar L1D Cache");

    if(ImGui::BeginTable("##sl1d", 2))
    {
        MetricRow("Hit:", Val(SL1D_HIT), "%");
        MetricRow("Lat:", Val(SL1D_LAT), "cycles");
        ImGui::EndTable();
    }

    ImGui::EndChild();
    PopBlockStyle();
    return { x, y, w, h };
}

ChartBlock
ComputeMemoryChartView::RenderInstrL1Cache(float x, float y)
{
    const float w = 195, h = 100;
    ImGui::SetCursorPos(ImVec2(x, y));
    PushBlockStyle();
    ImGui::BeginChild("InstrL1", ImVec2(w, h), ImGuiChildFlags_Borders);

    BlockHeader("Instr L1 Cache");

    if(ImGui::BeginTable("##il1", 2))
    {
        MetricRow("Hit:", Val(IL1_HIT), "%");
        MetricRow("Lat:", Val(IL1_LAT), "cycles");
        ImGui::EndTable();
    }

    ImGui::EndChild();
    PopBlockStyle();
    return { x, y, w, h };
}

// =============================================================================
// Memory subsystem blocks
// =============================================================================

ChartBlock
ComputeMemoryChartView::RenderL2Cache(float x, float y, float h)
{
    const float w = 175;
    ImGui::SetCursorPos(ImVec2(x, y));
    PushBlockStyle();
    ImGui::BeginChild("L2Cache", ImVec2(w, h), ImGuiChildFlags_Borders);

    BlockHeader("L2 Cache");

    if(ImGui::BeginTable("##l2", 2))
    {
        MetricRow("Rd:", Val(L2_RD));
        MetricRow("Wr:", Val(L2_WR));
        MetricRow("Atomic:", Val(L2_ATOMIC));
        MetricRow("Hit:", Val(L2_HIT), "%");
        ImGui::EndTable();
    }

    ImGui::EndChild();
    PopBlockStyle();
    return { x, y, w, h };
}

ChartBlock
ComputeMemoryChartView::RenderXGMIPCIe(float x, float y)
{
    const float w = 115, h = 90;
    ImGui::SetCursorPos(ImVec2(x, y));
    PushBlockStyle();
    ImGui::BeginChild("xGMI_PCIe", ImVec2(w, h), ImGuiChildFlags_Borders);

    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Text, Settings().GetColor(Colors::kTextMain));
    CenteredText("xGMI /");
    CenteredText("PCIe");
    ImGui::PopStyleColor();

    ImGui::EndChild();
    PopBlockStyle();
    return { x, y, w, h };
}

ChartBlock
ComputeMemoryChartView::RenderFabricBlock(float x, float y)
{
    const float w = 175, h = 270;
    ImGui::SetCursorPos(ImVec2(x, y));
    PushBlockStyle();
    ImGui::BeginChild("Fabric", ImVec2(w, h), ImGuiChildFlags_Borders);

    BlockHeader("Fabric");

    if(ImGui::BeginTable("##fabric", 2))
    {
        MetricRow("Rd:", Val(FABRIC_RD_LAT), "cycles");
        MetricRow("Wr:", Val(FABRIC_WR_LAT), "cycles");
        MetricRow("Atomic:", Val(FABRIC_ATOMIC_LAT), "cycles");
        ImGui::EndTable();
    }

    ImGui::EndChild();
    PopBlockStyle();
    return { x, y, w, h };
}

ChartBlock
ComputeMemoryChartView::RenderGMI(float x, float y)
{
    const float w = 115, h = 75;
    ImGui::SetCursorPos(ImVec2(x, y));
    PushBlockStyle();
    ImGui::BeginChild("GMI", ImVec2(w, h), ImGuiChildFlags_Borders);

    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Text, Settings().GetColor(Colors::kTextMain));
    CenteredText("GMI");
    ImGui::PopStyleColor();

    ImGui::EndChild();
    PopBlockStyle();
    return { x, y, w, h };
}

ChartBlock
ComputeMemoryChartView::RenderHBM(float x, float y)
{
    const float w = 95, h = 130;
    ImGui::SetCursorPos(ImVec2(x, y));
    PushBlockStyle();
    ImGui::BeginChild("HBM", ImVec2(w, h), ImGuiChildFlags_Borders);

    BlockHeader("HBM");

    if(ImGui::BeginTable("##hbm", 2))
    {
        MetricRow("Rd:", Val(HBM_RD));
        MetricRow("Wr:", Val(HBM_WR));
        ImGui::EndTable();
    }

    ImGui::EndChild();
    PopBlockStyle();
    return { x, y, w, h };
}

// =============================================================================
// Connection arrows and labels — all positions derived from stored blocks
// =============================================================================

void
ComputeMemoryChartView::RenderConnections()
{
    ImGui::SetCursorPos(ImVec2(0, 0));
    ImVec2      origin = ImGui::GetCursorScreenPos();
    ImDrawList* dl     = ImGui::GetWindowDrawList();

    constexpr float SP    = 20.0f;  // vertical spacing between arrows in a group
    constexpr float LBL_Y = 12.0f;  // label sits this far above the arrow line

    // Use dim text for connection labels
    ImU32 labelCol = Settings().GetColor(Colors::kTextDim);
    ImGui::PushStyleColor(ImGuiCol_Text, labelCol);

    // --- Pipeline -> Caches ---

    // Active CUs -> LDS (1 arrow)
    {
        float ay = m_lds.MidY();
        float lx = m_activeCUs.Right() + 5;
        DrawArrow(dl, m_activeCUs.Right(), ay, m_lds.x, ay, origin);
        ImGui::SetCursorPos(ImVec2(lx, ay - LBL_Y));
        ImGui::Text("Req: %s", Val(LDS_REQ));
    }

    // Active CUs -> Vector L1 (3 arrows)
    {
        float my = m_vectorL1.MidY();
        float lx = m_activeCUs.Right() + 5;
        float sx = m_activeCUs.Right(), dx = m_vectorL1.x;

        DrawArrow(dl, sx, my - SP, dx, my - SP, origin);
        ImGui::SetCursorPos(ImVec2(lx, my - SP - LBL_Y));
        ImGui::Text("Rd: %s", Val(VL1_RD));

        DrawArrow(dl, sx, my, dx, my, origin);
        ImGui::SetCursorPos(ImVec2(lx, my - LBL_Y));
        ImGui::Text("Wr: %s", Val(VL1_WR));

        DrawArrow(dl, sx, my + SP, dx, my + SP, origin);
        ImGui::SetCursorPos(ImVec2(lx, my + SP - LBL_Y));
        ImGui::Text("Atomic: %s", Val(VL1_ATOMIC));
    }

    // Active CUs -> Scalar L1D (1 arrow)
    {
        float ay = m_scalarL1D.MidY();
        float lx = m_activeCUs.Right() + 5;
        DrawArrow(dl, m_activeCUs.Right(), ay, m_scalarL1D.x, ay, origin);
        ImGui::SetCursorPos(ImVec2(lx, ay - LBL_Y));
        ImGui::Text("Rd: %s", Val(SL1D_RD));
    }

    // Instr Dispatch -> Instr L1 (1 arrow)
    {
        float ay = m_instrL1.MidY();
        float lx = (m_instrDispatch.Right() + m_instrL1.x) * 0.5f;
        DrawArrow(dl, m_instrDispatch.Right(), ay, m_instrL1.x, ay, origin);
        ImGui::SetCursorPos(ImVec2(lx, ay - LBL_Y));
        ImGui::Text("Fetch: %s", Val(IL1_FETCH));
    }

    // --- Caches -> L2 ---

    // Vector L1 -> L2 (3 arrows)
    {
        float my = m_vectorL1.MidY();
        float lx = m_vectorL1.Right() + 5;
        float sx = m_vectorL1.Right(), dx = m_l2.x;

        DrawArrow(dl, sx, my - SP, dx, my - SP, origin);
        ImGui::SetCursorPos(ImVec2(lx, my - SP - LBL_Y));
        ImGui::Text("Rd: %s", Val(VL1_L2_RD));

        DrawArrow(dl, sx, my, dx, my, origin);
        ImGui::SetCursorPos(ImVec2(lx, my - LBL_Y));
        ImGui::Text("Wr: %s", Val(VL1_L2_WR));

        DrawArrow(dl, sx, my + SP, dx, my + SP, origin);
        ImGui::SetCursorPos(ImVec2(lx, my + SP - LBL_Y));
        ImGui::Text("Atomic: %s", Val(VL1_L2_ATOMIC));
    }

    // Scalar L1D -> L2 (3 arrows)
    {
        float my = m_scalarL1D.MidY();
        float lx = m_scalarL1D.Right() + 5;
        float sx = m_scalarL1D.Right(), dx = m_l2.x;

        DrawArrow(dl, sx, my - SP, dx, my - SP, origin);
        ImGui::SetCursorPos(ImVec2(lx, my - SP - LBL_Y));
        ImGui::Text("Rd: %s", Val(SL1D_L2_RD));

        DrawArrow(dl, sx, my, dx, my, origin);
        ImGui::SetCursorPos(ImVec2(lx, my - LBL_Y));
        ImGui::Text("Wr: %s", Val(SL1D_L2_WR));

        DrawArrow(dl, sx, my + SP, dx, my + SP, origin);
        ImGui::SetCursorPos(ImVec2(lx, my + SP - LBL_Y));
        ImGui::Text("Atomic: %s", Val(SL1D_L2_ATOMIC));
    }

    // Instr L1 -> L2 (1 arrow)
    {
        float ay = m_instrL1.MidY();
        float lx = m_instrL1.Right() + 5;
        DrawArrow(dl, m_instrL1.Right(), ay, m_l2.x, ay, origin);
        ImGui::SetCursorPos(ImVec2(lx, ay - LBL_Y));
        ImGui::Text("Req: %s", Val(IL1_L2_RD));
    }

    // --- L2 -> Fabric ---
    {
        float my = m_fabric.MidY();
        float lx = m_l2.Right() + 5;
        float sx = m_l2.Right(), dx = m_fabric.x;

        DrawArrow(dl, sx, my - SP, dx, my - SP, origin);
        ImGui::SetCursorPos(ImVec2(lx, my - SP - LBL_Y));
        ImGui::Text("Rd: %s", Val(FABRIC_L2_RD));

        DrawArrow(dl, sx, my, dx, my, origin);
        ImGui::SetCursorPos(ImVec2(lx, my - LBL_Y));
        ImGui::Text("Wr: %s", Val(FABRIC_L2_WR));

        DrawArrow(dl, sx, my + SP, dx, my + SP, origin);
        ImGui::SetCursorPos(ImVec2(lx, my + SP - LBL_Y));
        ImGui::Text("Atomic: %s", Val(FABRIC_L2_ATOMIC));
    }

    // --- Fabric -> HBM ---
    {
        float my = m_hbm.MidY();
        float lx = m_fabric.Right() + 5;
        float sx = m_fabric.Right(), dx = m_hbm.x;
        float hs = SP * 0.5f;

        DrawArrow(dl, sx, my - hs, dx, my - hs, origin);
        ImGui::SetCursorPos(ImVec2(lx, my - hs - LBL_Y));
        ImGui::Text("Rd: %s", Val(HBM_RD));

        DrawArrow(dl, sx, my + hs, dx, my + hs, origin);
        ImGui::SetCursorPos(ImVec2(lx, my + hs - LBL_Y));
        ImGui::Text("Wr: %s", Val(HBM_WR));
    }

    ImGui::PopStyleColor();  // labelCol
}

}  // namespace View
}  // namespace RocProfVis
