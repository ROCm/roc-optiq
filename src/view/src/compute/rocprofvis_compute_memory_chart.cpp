// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_compute_memory_chart.h"
#include "rocprofvis_data_provider.h"
#include <imgui.h>
#include <cstdio>

namespace RocProfVis
{
namespace View
{

static constexpr float CANVAS_W = 1450.0f;
static constexpr float CANVAS_H = 490.0f;

// Category and table IDs for the memory chart metrics
static constexpr uint32_t MEMCHART_CATEGORY_ID = 3;
static constexpr uint32_t MEMCHART_TABLE_ID    = 1;

// Format a metric double for display
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

// Render a label-value row inside a 2-column table context
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

// Draw a horizontal arrow on the draw list (local coords + origin)
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
    // Request entire table 3.1 (all 52 entries)
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
// Render
// =============================================================================

void ComputeMemoryChartView::Render()
{
    ImGui::BeginChild("MemoryChart", ImVec2(0, 0), ImGuiChildFlags_Borders,
                       ImGuiWindowFlags_HorizontalScrollbar);

    // Pipeline
    RenderInstrBuff();
    RenderInstrDispatch();
    RenderActiveCUs();

    // Caches
    RenderLDS();
    RenderVectorL1Cache();
    RenderScalarL1DCache();
    RenderInstrL1Cache();

    // Memory
    RenderL2Cache();
    RenderFabric();
    RenderHBM();

    // Arrows and labels between blocks
    RenderConnections();

    // Ensure scroll area covers the full diagram
    ImGui::SetCursorPos(ImVec2(CANVAS_W, CANVAS_H));
    ImGui::Dummy(ImVec2(1, 1));

    ImGui::EndChild();
}

// =============================================================================
// Pipeline Section (left)
// =============================================================================

void ComputeMemoryChartView::RenderInstrBuff()
{
    ImGui::SetCursorPos(ImVec2(5, 5));
    ImGui::BeginChild("InstrBuff", ImVec2(145, 460), ImGuiChildFlags_Borders);

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
    ImGui::TextColored(ImVec4(1, 1, 0.2f, 1), "%s", Val(WAVEFRONT_OCCUPANCY));
    ImGui::TextColored(ImVec4(1, 1, 0.2f, 1), "per-GCD");

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextUnformatted("Wave Life");
    ImGui::TextColored(ImVec4(1, 1, 0.2f, 1), "%s", Val(WAVE_LIFE));
    ImGui::TextColored(ImVec4(1, 1, 0.2f, 1), "cycles");

    ImGui::EndChild();
}

void ComputeMemoryChartView::RenderInstrDispatch()
{
    ImGui::SetCursorPos(ImVec2(160, 5));
    ImGui::BeginChild("InstrDispatch", ImVec2(120, 460), ImGuiChildFlags_Borders);

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
}

void ComputeMemoryChartView::RenderActiveCUs()
{
    ImGui::SetCursorPos(ImVec2(290, 20));
    ImGui::BeginChild("ActiveCUs", ImVec2(170, 380), ImGuiChildFlags_Borders);

    ImGui::SeparatorText("Exec");
    ImGui::TextUnformatted("Active CUs");
    ImGui::TextColored(ImVec4(1, 1, 0.2f, 1), "%s", Val(NUM_CUS));

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
}

// =============================================================================
// Cache Section (middle)
// =============================================================================

void ComputeMemoryChartView::RenderLDS()
{
    ImGui::SetCursorPos(ImVec2(555, 5));
    ImGui::BeginChild("LDS_Block", ImVec2(195, 100), ImGuiChildFlags_Borders);

    ImGui::SeparatorText("LDS");

    if (ImGui::BeginTable("##lds", 2))
    {
        MetricRow("Util:", Val(LDS_UTIL),    "%");
        MetricRow("Lat:",  Val(LDS_LATENCY), "cycles");
        ImGui::EndTable();
    }

    ImGui::EndChild();
}

void ComputeMemoryChartView::RenderVectorL1Cache()
{
    ImGui::SetCursorPos(ImVec2(555, 115));
    ImGui::BeginChild("VectorL1", ImVec2(195, 155), ImGuiChildFlags_Borders);

    ImGui::SeparatorText("Vector L1 Cache");

    if (ImGui::BeginTable("##vl1", 2))
    {
        MetricRow("Hit:",    Val(VL1_HIT),      "%");
        MetricRow("Coales:", Val(VL1_COALESCE),  "%");
        MetricRow("Stall:",  Val(VL1_STALL),     "%");
        ImGui::EndTable();
    }

    ImGui::EndChild();
}

void ComputeMemoryChartView::RenderScalarL1DCache()
{
    ImGui::SetCursorPos(ImVec2(555, 280));
    ImGui::BeginChild("ScalarL1D", ImVec2(195, 100), ImGuiChildFlags_Borders);

    ImGui::SeparatorText("Scalar L1D Cache");

    if (ImGui::BeginTable("##sl1d", 2))
    {
        MetricRow("Hit:", Val(SL1D_HIT), "%");
        MetricRow("Lat:", Val(SL1D_LAT), "cycles");
        ImGui::EndTable();
    }

    ImGui::EndChild();
}

void ComputeMemoryChartView::RenderInstrL1Cache()
{
    ImGui::SetCursorPos(ImVec2(555, 390));
    ImGui::BeginChild("InstrL1", ImVec2(195, 80), ImGuiChildFlags_Borders);

    ImGui::SeparatorText("Instr L1 Cache");

    if (ImGui::BeginTable("##il1", 2))
    {
        MetricRow("Hit:", Val(IL1_HIT), "%");
        MetricRow("Lat:", Val(IL1_LAT), "cycles");
        ImGui::EndTable();
    }

    ImGui::EndChild();
}

// =============================================================================
// Memory Section (right)
// =============================================================================

void ComputeMemoryChartView::RenderL2Cache()
{
    ImGui::SetCursorPos(ImVec2(850, 5));
    ImGui::BeginChild("L2Cache", ImVec2(175, 465), ImGuiChildFlags_Borders);

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
}

void ComputeMemoryChartView::RenderFabric()
{
    // xGMI / PCIe
    ImGui::SetCursorPos(ImVec2(1120, 5));
    ImGui::BeginChild("xGMI_PCIe", ImVec2(115, 90), ImGuiChildFlags_Borders);
    ImGui::TextUnformatted("xGMI /");
    ImGui::TextUnformatted("PCIe");
    ImGui::EndChild();

    // Fabric
    ImGui::SetCursorPos(ImVec2(1120, 105));
    ImGui::BeginChild("Fabric", ImVec2(175, 270), ImGuiChildFlags_Borders);

    ImGui::SeparatorText("Fabric");

    if (ImGui::BeginTable("##fabric", 2))
    {
        MetricRow("Rd:",     Val(FABRIC_RD_LAT),     "cycles");
        MetricRow("Wr:",     Val(FABRIC_WR_LAT),     "cycles");
        MetricRow("Atomic:", Val(FABRIC_ATOMIC_LAT),  "cycles");
        ImGui::EndTable();
    }

    ImGui::EndChild();

    // GMI
    ImGui::SetCursorPos(ImVec2(1120, 385));
    ImGui::BeginChild("GMI", ImVec2(115, 75), ImGuiChildFlags_Borders);
    ImGui::TextUnformatted("GMI");
    ImGui::EndChild();
}

void ComputeMemoryChartView::RenderHBM()
{
    ImGui::SetCursorPos(ImVec2(1350, 130));
    ImGui::BeginChild("HBM", ImVec2(95, 130), ImGuiChildFlags_Borders);

    ImGui::SeparatorText("HBM");

    if (ImGui::BeginTable("##hbm", 2))
    {
        MetricRow("Rd:", Val(HBM_RD));
        MetricRow("Wr:", Val(HBM_WR));
        ImGui::EndTable();
    }

    ImGui::EndChild();
}

// =============================================================================
// Connection arrows and labels between blocks
// =============================================================================

void ComputeMemoryChartView::RenderConnections()
{
    ImGui::SetCursorPos(ImVec2(0, 0));
    ImVec2 origin = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // --- Pipeline -> Caches ---

    // Active CUs -> LDS
    DrawArrow(dl, 460, 50, 555, 50, origin);
    ImGui::SetCursorPos(ImVec2(478, 38));
    ImGui::Text("Req: %s", Val(LDS_REQ));

    // Active CUs -> Vector L1
    DrawArrow(dl, 460, 155, 555, 155, origin);
    DrawArrow(dl, 460, 175, 555, 175, origin);
    DrawArrow(dl, 460, 195, 555, 195, origin);
    ImGui::SetCursorPos(ImVec2(478, 143));
    ImGui::Text("Rd: %s", Val(VL1_RD));
    ImGui::SetCursorPos(ImVec2(478, 163));
    ImGui::Text("Wr: %s", Val(VL1_WR));
    ImGui::SetCursorPos(ImVec2(478, 183));
    ImGui::Text("Atomic: %s", Val(VL1_ATOMIC));

    // Active CUs -> Scalar L1D
    DrawArrow(dl, 460, 325, 555, 325, origin);
    ImGui::SetCursorPos(ImVec2(478, 313));
    ImGui::Text("Rd: %s", Val(SL1D_RD));

    // Instr Dispatch -> Instr L1 (Fetch)
    DrawArrow(dl, 280, 450, 555, 450, origin);
    ImGui::SetCursorPos(ImVec2(380, 438));
    ImGui::Text("Fetch: %s", Val(IL1_FETCH));

    // --- Caches -> L2 ---

    // Vector L1 -> L2
    DrawArrow(dl, 750, 155, 850, 155, origin);
    DrawArrow(dl, 750, 175, 850, 175, origin);
    DrawArrow(dl, 750, 195, 850, 195, origin);
    ImGui::SetCursorPos(ImVec2(765, 143));
    ImGui::Text("Rd: %s", Val(VL1_L2_RD));
    ImGui::SetCursorPos(ImVec2(765, 163));
    ImGui::Text("Wr: %s", Val(VL1_L2_WR));
    ImGui::SetCursorPos(ImVec2(765, 183));
    ImGui::Text("Atomic: %s", Val(VL1_L2_ATOMIC));

    // Scalar L1D -> L2
    DrawArrow(dl, 750, 315, 850, 315, origin);
    DrawArrow(dl, 750, 335, 850, 335, origin);
    DrawArrow(dl, 750, 355, 850, 355, origin);
    ImGui::SetCursorPos(ImVec2(765, 303));
    ImGui::Text("Rd: %s", Val(SL1D_L2_RD));
    ImGui::SetCursorPos(ImVec2(765, 323));
    ImGui::Text("Wr: %s", Val(SL1D_L2_WR));
    ImGui::SetCursorPos(ImVec2(765, 343));
    ImGui::Text("Atomic: %s", Val(SL1D_L2_ATOMIC));

    // Instr L1 -> L2
    DrawArrow(dl, 750, 425, 850, 425, origin);
    ImGui::SetCursorPos(ImVec2(765, 413));
    ImGui::Text("Req: %s", Val(IL1_L2_RD));

    // --- L2 -> Fabric ---

    DrawArrow(dl, 1025, 190, 1120, 190, origin);
    DrawArrow(dl, 1025, 215, 1120, 215, origin);
    DrawArrow(dl, 1025, 240, 1120, 240, origin);
    ImGui::SetCursorPos(ImVec2(1038, 178));
    ImGui::Text("Rd: %s", Val(FABRIC_L2_RD));
    ImGui::SetCursorPos(ImVec2(1038, 203));
    ImGui::Text("Wr: %s", Val(FABRIC_L2_WR));
    ImGui::SetCursorPos(ImVec2(1038, 228));
    ImGui::Text("Atomic: %s", Val(FABRIC_L2_ATOMIC));

    // --- Fabric -> HBM ---

    DrawArrow(dl, 1295, 185, 1350, 185, origin);
    DrawArrow(dl, 1295, 205, 1350, 205, origin);
    ImGui::SetCursorPos(ImVec2(1300, 173));
    ImGui::Text("Rd: %s", Val(HBM_RD));
    ImGui::SetCursorPos(ImVec2(1300, 193));
    ImGui::Text("Wr: %s", Val(HBM_WR));
}

}  // namespace View
}  // namespace RocProfVis
