// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_compute_memory_chart.h"
#include "rocprofvis_data_provider.h"
#include <imgui.h>
#include <cstdio>
#include <algorithm>

namespace RocProfVis
{
namespace View
{

// =============================================================================
// Layout constants — tweak these three values to adjust the entire diagram
// =============================================================================
static constexpr float PAD       = 10.0f;   // padding from edges
static constexpr float BLOCK_GAP = 10.0f;   // gap between adjacent blocks
static constexpr float ARROW_GAP = 100.0f;  // gap between columns (room for arrows + labels)

// Category and table IDs for the memory chart metrics
static constexpr uint32_t MEMCHART_CATEGORY_ID = 3;
static constexpr uint32_t MEMCHART_TABLE_ID    = 1;

// =============================================================================
// Helpers
// =============================================================================

static std::string FormatMetricValue(double value)
{
    if (value != value) return "-";  // NaN check
    int64_t ival = static_cast<int64_t>(value);
    if (value == static_cast<double>(ival))
    {
        return std::to_string(ival);
    }
    char buf[32];
    snprintf(buf, sizeof(buf), "%.1f", value);
    return buf;
}

static void MetricRow(const char* label, const char* value, const char* unit = "")
{
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(label);
    ImGui::TableNextColumn();
    if (unit[0] != '\0')
    {
        ImGui::Text("%s %s", value, unit);
    }
    else
    {
        ImGui::TextUnformatted(value);
    }
}

static void DrawArrow(ImDrawList* dl, float x1, float y1, float x2, float y2, ImVec2 origin)
{
    ImVec2 from(origin.x + x1, origin.y + y1);
    ImVec2 to(origin.x + x2, origin.y + y2);
    ImU32 color = IM_COL32(255, 128, 0, 200);
    dl->AddLine(from, to, color, 1.5f);
    float sz = 4.0f;
    dl->AddTriangleFilled(
        to,
        ImVec2(to.x - sz, to.y - sz * 0.6f),
        ImVec2(to.x - sz, to.y + sz * 0.6f),
        color);
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

const char* ComputeMemoryChartView::Val(MemChartMetric m) const
{
    if (m >= 0 && m < MEMCHART_METRIC_COUNT)
    {
        return m_values[m].c_str();
    }
    return "-";
}

void ComputeMemoryChartView::FetchMemChartMetrics(uint32_t workload_id,
                                                    const std::vector<uint32_t>& kernel_ids)
{
    m_values.fill("-");
    m_data_provider.ComputeModel().ClearMetricValues();

    std::vector<MetricsRequestParams::MetricID> metric_ids;
    metric_ids.push_back({MEMCHART_CATEGORY_ID, MEMCHART_TABLE_ID, std::nullopt});

    m_data_provider.FetchMetrics(
        MetricsRequestParams(workload_id, kernel_ids, metric_ids));
}

void ComputeMemoryChartView::Update()
{
    const std::vector<std::unique_ptr<MetricValue>>& metrics =
        m_data_provider.ComputeModel().GetMetricsData();

    for (const std::unique_ptr<MetricValue>& metric : metrics)
    {
        if (!metric) continue;
        if (metric->entry.category_id != MEMCHART_CATEGORY_ID) continue;
        if (metric->entry.table_id    != MEMCHART_TABLE_ID)    continue;

        uint32_t id = metric->entry.id;
        if (id < MEMCHART_METRIC_COUNT)
        {
            m_values[id] = FormatMetricValue(metric->value);
        }
    }
}

// =============================================================================
// Render — each block is positioned relative to the previous one
// =============================================================================

void ComputeMemoryChartView::Render()
{
    ImGui::BeginChild("MemoryChart", ImVec2(0, 0), ImGuiChildFlags_Borders,
                       ImGuiWindowFlags_HorizontalScrollbar);

    // --- Pipeline column ---
    m_instrBuff     = RenderInstrBuff(PAD, PAD);
    m_instrDispatch = RenderInstrDispatch(m_instrBuff.Right() + BLOCK_GAP, PAD);
    m_activeCUs     = RenderActiveCUs(m_instrDispatch.Right() + BLOCK_GAP, PAD + 15);

    // --- Cache column (stacked vertically) ---
    float cacheX = m_activeCUs.Right() + ARROW_GAP;
    m_lds       = RenderLDS(cacheX, PAD);
    m_vectorL1  = RenderVectorL1Cache(cacheX, m_lds.Bottom() + BLOCK_GAP);
    m_scalarL1D = RenderScalarL1DCache(cacheX, m_vectorL1.Bottom() + BLOCK_GAP);
    m_instrL1   = RenderInstrL1Cache(cacheX, m_scalarL1D.Bottom() + BLOCK_GAP);

    // --- L2 column (spans full cache height) ---
    float l2H = m_instrL1.Bottom() - PAD;
    m_l2 = RenderL2Cache(m_lds.Right() + ARROW_GAP, PAD, l2H);

    // --- Fabric column (stacked vertically) ---
    float fabricX = m_l2.Right() + ARROW_GAP;
    m_xgmiPcie = RenderXGMIPCIe(fabricX, PAD);
    m_fabric   = RenderFabricBlock(fabricX, m_xgmiPcie.Bottom() + BLOCK_GAP);
    m_gmi      = RenderGMI(fabricX, m_fabric.Bottom() + BLOCK_GAP);

    // --- HBM (right of fabric) ---
    m_hbm = RenderHBM(m_fabric.Right() + ARROW_GAP / 2, m_fabric.y);

    // Arrows and labels between blocks
    RenderConnections();

    // Ensure scroll region covers the entire diagram
    float canvasW = m_hbm.Right() + PAD;
    float canvasH = m_instrBuff.Bottom();
    canvasH = std::max(canvasH, m_instrDispatch.Bottom());
    canvasH = std::max(canvasH, m_instrL1.Bottom());
    canvasH = std::max(canvasH, m_gmi.Bottom());
    canvasH += PAD;
    ImGui::SetCursorPos(ImVec2(canvasW, canvasH));
    ImGui::Dummy(ImVec2(1, 1));

    ImGui::EndChild();
}

// =============================================================================
// Pipeline blocks
// =============================================================================

ChartBlock ComputeMemoryChartView::RenderInstrBuff(float x, float y)
{
    const float w = 145, h = 460;
    ImGui::SetCursorPos(ImVec2(x, y));
    ImGui::BeginChild("InstrBuff", ImVec2(w, h), ImGuiChildFlags_Borders);

    ImGui::SeparatorText("Instr Buff");

    ImGui::BeginChild("Wave0", ImVec2(-1, 30), ImGuiChildFlags_Borders);
    ImGui::TextUnformatted("Wave 0 Instr buff");
    ImGui::EndChild();

    ImGui::Spacing();

    ImGui::BeginChild("WaveN", ImVec2(-1, 30), ImGuiChildFlags_Borders);
    ImGui::TextUnformatted("Wave N-1 Instr buff");
    ImGui::EndChild();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextUnformatted("Wave Occupancy");
    ImGui::Text("%s", Val(WAVEFRONT_OCCUPANCY));
    ImGui::TextUnformatted("per-GCD");

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextUnformatted("Wave Life");
    ImGui::Text("%s", Val(WAVE_LIFE));
    ImGui::TextUnformatted("cycles");

    ImGui::EndChild();
    return {x, y, w, h};
}

ChartBlock ComputeMemoryChartView::RenderInstrDispatch(float x, float y)
{
    const float w = 120, h = 460;
    ImGui::SetCursorPos(ImVec2(x, y));
    ImGui::BeginChild("InstrDispatch", ImVec2(w, h), ImGuiChildFlags_Borders);

    ImGui::SeparatorText("Instr Dispatch");

    if (ImGui::BeginTable("##dispatch", 2))
    {
        MetricRow("SALU:", Val(SALU));
        MetricRow("SMEM:", Val(SMEM));
        MetricRow("VALU:", Val(VALU));
        MetricRow("MFMA:", Val(MFMA));
        MetricRow("VMEM:", Val(VMEM));
        MetricRow("LDS:",  Val(LDS));
        MetricRow("GWS:",  Val(GWS));
        MetricRow("Br:",   Val(BR));
        ImGui::EndTable();
    }

    ImGui::EndChild();
    return {x, y, w, h};
}

ChartBlock ComputeMemoryChartView::RenderActiveCUs(float x, float y)
{
    const float w = 170, h = 380;
    ImGui::SetCursorPos(ImVec2(x, y));
    ImGui::BeginChild("ActiveCUs", ImVec2(w, h), ImGuiChildFlags_Borders);

    ImGui::SeparatorText("Exec");
    ImGui::TextUnformatted("Active CUs");
    ImGui::Text("%s", Val(NUM_CUS));

    ImGui::Separator();

    if (ImGui::BeginTable("##cus", 2))
    {
        MetricRow("VGPRs:",         Val(VGPR));
        MetricRow("SGPRs:",         Val(SGPR));
        MetricRow("LDS Alloc:",     Val(LDS_ALLOCATION));
        MetricRow("Scratch Alloc:", Val(SCRATCH_ALLOCATION));
        MetricRow("Wavefronts:",    Val(WAVEFRONTS));
        MetricRow("Workgroups:",    Val(WORKGROUPS));
        ImGui::EndTable();
    }

    ImGui::EndChild();
    return {x, y, w, h};
}

// =============================================================================
// Cache blocks
// =============================================================================

ChartBlock ComputeMemoryChartView::RenderLDS(float x, float y)
{
    const float w = 195, h = 100;
    ImGui::SetCursorPos(ImVec2(x, y));
    ImGui::BeginChild("LDS_Block", ImVec2(w, h), ImGuiChildFlags_Borders);

    ImGui::SeparatorText("LDS");

    if (ImGui::BeginTable("##lds", 2))
    {
        MetricRow("Util:", Val(LDS_UTIL),    "%");
        MetricRow("Lat:",  Val(LDS_LATENCY), "cycles");
        ImGui::EndTable();
    }

    ImGui::EndChild();
    return {x, y, w, h};
}

ChartBlock ComputeMemoryChartView::RenderVectorL1Cache(float x, float y)
{
    const float w = 195, h = 155;
    ImGui::SetCursorPos(ImVec2(x, y));
    ImGui::BeginChild("VectorL1", ImVec2(w, h), ImGuiChildFlags_Borders);

    ImGui::SeparatorText("Vector L1 Cache");

    if (ImGui::BeginTable("##vl1", 2))
    {
        MetricRow("Hit:",    Val(VL1_HIT),      "%");
        MetricRow("Coales:", Val(VL1_COALESCE),  "%");
        MetricRow("Stall:",  Val(VL1_STALL),     "%");
        ImGui::EndTable();
    }

    ImGui::EndChild();
    return {x, y, w, h};
}

ChartBlock ComputeMemoryChartView::RenderScalarL1DCache(float x, float y)
{
    const float w = 195, h = 100;
    ImGui::SetCursorPos(ImVec2(x, y));
    ImGui::BeginChild("ScalarL1D", ImVec2(w, h), ImGuiChildFlags_Borders);

    ImGui::SeparatorText("Scalar L1D Cache");

    if (ImGui::BeginTable("##sl1d", 2))
    {
        MetricRow("Hit:", Val(SL1D_HIT), "%");
        MetricRow("Lat:", Val(SL1D_LAT), "cycles");
        ImGui::EndTable();
    }

    ImGui::EndChild();
    return {x, y, w, h};
}

ChartBlock ComputeMemoryChartView::RenderInstrL1Cache(float x, float y)
{
    const float w = 195, h = 80;
    ImGui::SetCursorPos(ImVec2(x, y));
    ImGui::BeginChild("InstrL1", ImVec2(w, h), ImGuiChildFlags_Borders);

    ImGui::SeparatorText("Instr L1 Cache");

    if (ImGui::BeginTable("##il1", 2))
    {
        MetricRow("Hit:", Val(IL1_HIT), "%");
        MetricRow("Lat:", Val(IL1_LAT), "cycles");
        ImGui::EndTable();
    }

    ImGui::EndChild();
    return {x, y, w, h};
}

// =============================================================================
// Memory subsystem blocks
// =============================================================================

ChartBlock ComputeMemoryChartView::RenderL2Cache(float x, float y, float h)
{
    const float w = 175;
    ImGui::SetCursorPos(ImVec2(x, y));
    ImGui::BeginChild("L2Cache", ImVec2(w, h), ImGuiChildFlags_Borders);

    ImGui::SeparatorText("L2 Cache");

    if (ImGui::BeginTable("##l2", 2))
    {
        MetricRow("Rd:",     Val(L2_RD));
        MetricRow("Wr:",     Val(L2_WR));
        MetricRow("Atomic:", Val(L2_ATOMIC));
        MetricRow("Hit:",    Val(L2_HIT), "%");
        ImGui::EndTable();
    }

    ImGui::EndChild();
    return {x, y, w, h};
}

ChartBlock ComputeMemoryChartView::RenderXGMIPCIe(float x, float y)
{
    const float w = 115, h = 90;
    ImGui::SetCursorPos(ImVec2(x, y));
    ImGui::BeginChild("xGMI_PCIe", ImVec2(w, h), ImGuiChildFlags_Borders);
    ImGui::TextUnformatted("xGMI /");
    ImGui::TextUnformatted("PCIe");
    ImGui::EndChild();
    return {x, y, w, h};
}

ChartBlock ComputeMemoryChartView::RenderFabricBlock(float x, float y)
{
    const float w = 175, h = 270;
    ImGui::SetCursorPos(ImVec2(x, y));
    ImGui::BeginChild("Fabric", ImVec2(w, h), ImGuiChildFlags_Borders);

    ImGui::SeparatorText("Fabric");

    if (ImGui::BeginTable("##fabric", 2))
    {
        MetricRow("Rd:",     Val(FABRIC_RD_LAT),     "cycles");
        MetricRow("Wr:",     Val(FABRIC_WR_LAT),     "cycles");
        MetricRow("Atomic:", Val(FABRIC_ATOMIC_LAT),  "cycles");
        ImGui::EndTable();
    }

    ImGui::EndChild();
    return {x, y, w, h};
}

ChartBlock ComputeMemoryChartView::RenderGMI(float x, float y)
{
    const float w = 115, h = 75;
    ImGui::SetCursorPos(ImVec2(x, y));
    ImGui::BeginChild("GMI", ImVec2(w, h), ImGuiChildFlags_Borders);
    ImGui::TextUnformatted("GMI");
    ImGui::EndChild();
    return {x, y, w, h};
}

ChartBlock ComputeMemoryChartView::RenderHBM(float x, float y)
{
    const float w = 95, h = 130;
    ImGui::SetCursorPos(ImVec2(x, y));
    ImGui::BeginChild("HBM", ImVec2(w, h), ImGuiChildFlags_Borders);

    ImGui::SeparatorText("HBM");

    if (ImGui::BeginTable("##hbm", 2))
    {
        MetricRow("Rd:", Val(HBM_RD));
        MetricRow("Wr:", Val(HBM_WR));
        ImGui::EndTable();
    }

    ImGui::EndChild();
    return {x, y, w, h};
}

// =============================================================================
// Connection arrows and labels — all positions derived from stored blocks
// =============================================================================

void ComputeMemoryChartView::RenderConnections()
{
    ImGui::SetCursorPos(ImVec2(0, 0));
    ImVec2 origin = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    constexpr float SP    = 20.0f;  // vertical spacing between arrows in a group
    constexpr float LBL_Y = 12.0f;  // label sits this far above the arrow line

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
}

}  // namespace View
}  // namespace RocProfVis
