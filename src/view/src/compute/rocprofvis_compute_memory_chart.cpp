// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_compute_memory_chart.h"
#include <imgui.h>

namespace RocProfVis
{
namespace View
{

static constexpr float CANVAS_W = 1450.0f;
static constexpr float CANVAS_H = 490.0f;

// Helper: renders a label-value row inside a 2-column table context
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

// Helper: draw a horizontal arrow on the draw list (local coords + origin)
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

ComputeMemoryChartView::ComputeMemoryChartView(DataProvider& data_provider)
: m_data_provider(data_provider)
{
}

ComputeMemoryChartView::~ComputeMemoryChartView() {}

void ComputeMemoryChartView::Update() {}

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
    ImGui::TextColored(ImVec4(1, 1, 0.2f, 1), "27");       // get WaveOccupancy from DB
    ImGui::TextColored(ImVec4(1, 1, 0.2f, 1), "per-GCD");

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextUnformatted("Wave Life");
    ImGui::TextColored(ImVec4(1, 1, 0.2f, 1), "18926");     // get WaveLife from DB
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
        MetricRow("SALU:", "25");     // get from DB
        MetricRow("SMEM:", "2");      // get from DB
        MetricRow("VALU:", "495");    // get from DB
        MetricRow("MFMA:", "0");      // get from DB
        MetricRow("VMEM:", "8");      // get from DB
        MetricRow("LDS:",  "0");      // get from DB
        MetricRow("GWS:",  "0");      // get from DB
        MetricRow("Br:",   "10");     // get from DB
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
    ImGui::TextColored(ImVec4(1, 1, 0.2f, 1), "103/104");   // get ActiveCUs from DB

    ImGui::Separator();

    if (ImGui::BeginTable("##cus", 2))
    {
        MetricRow("VGPRs:",         "12");     // get from DB
        MetricRow("SGPRs:",         "24");     // get from DB
        MetricRow("LDS Alloc:",     "0");      // get from DB
        MetricRow("Scratch Alloc:", "0");      // get from DB
        MetricRow("Wavefronts:",    "67894");  // get from DB
        MetricRow("Workgroups:",    "16973");  // get from DB
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
    ImGui::BeginChild("LDS", ImVec2(195, 100), ImGuiChildFlags_Borders);

    ImGui::SeparatorText("LDS");

    if (ImGui::BeginTable("##lds", 2))
    {
        MetricRow("Util:", "0",  "%");      // get from DB
        MetricRow("Lat:",  "",   "cycles"); // get from DB
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
        MetricRow("Hit:",    "20",   "%");      // get from DB
        MetricRow("Lat:",    "1323", "cycles"); // get from DB
        MetricRow("Coales:", "70",   "%");      // get from DB
        MetricRow("Stall:",  "3",    "%");      // get from DB
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
        MetricRow("Hit:", "98", "%");      // get from DB
        MetricRow("Lat:", "",   "cycles"); // get from DB
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
        MetricRow("Hit:", "99", "%");      // get from DB
        MetricRow("Lat:", "",   "cycles"); // get from DB
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
        MetricRow("Rd:",     "44");    // get from DB
        MetricRow("Wr:",     "0");     // get from DB
        MetricRow("Atomic:", "0");     // get from DB
        MetricRow("Hit:",    "4", "%");// get from DB
        ImGui::EndTable();
    }

    ImGui::Spacing();
    ImGui::SeparatorText("Latency");

    if (ImGui::BeginTable("##l2lat", 2))
    {
        MetricRow("Rd:", "794", "cycles"); // get from DB
        MetricRow("Wr:", "360", "cycles"); // get from DB
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
        MetricRow("Rd:",     "539", "cycles"); // get from DB
        MetricRow("Wr:",     "427", "cycles"); // get from DB
        MetricRow("Atomic:", "",    "cycles"); // get from DB
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
        MetricRow("Rd:", "44"); // get from DB
        MetricRow("Wr:", "0");  // get from DB
        ImGui::EndTable();
    }

    ImGui::EndChild();
}

// =============================================================================
// Connection arrows and labels between blocks
// =============================================================================

void ComputeMemoryChartView::RenderConnections()
{
    // Compute screen-space origin of the canvas content area
    ImGui::SetCursorPos(ImVec2(0, 0));
    ImVec2 origin = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // --- Pipeline -> Caches ---

    // Active CUs -> LDS
    DrawArrow(dl, 460, 50, 555, 50, origin);
    ImGui::SetCursorPos(ImVec2(478, 38));
    ImGui::TextUnformatted("Req: 0");          // get from DB

    // Active CUs -> Vector L1
    DrawArrow(dl, 460, 155, 555, 155, origin);
    DrawArrow(dl, 460, 175, 555, 175, origin);
    DrawArrow(dl, 460, 195, 555, 195, origin);
    ImGui::SetCursorPos(ImVec2(478, 143));
    ImGui::TextUnformatted("Rd: 279");         // get from DB
    ImGui::SetCursorPos(ImVec2(478, 163));
    ImGui::TextUnformatted("Wr: 0");           // get from DB
    ImGui::SetCursorPos(ImVec2(478, 183));
    ImGui::TextUnformatted("Atomic: 0");       // get from DB

    // Active CUs -> Scalar L1D
    DrawArrow(dl, 460, 325, 555, 325, origin);
    ImGui::SetCursorPos(ImVec2(478, 313));
    ImGui::TextUnformatted("Rd: 2");           // get from DB

    // Instr Dispatch -> Instr L1 (Fetch)
    DrawArrow(dl, 280, 450, 555, 450, origin);
    ImGui::SetCursorPos(ImVec2(380, 438));
    ImGui::TextUnformatted("Fetch: 128");      // get from DB

    // --- Caches -> L2 ---

    // Vector L1 -> L2
    DrawArrow(dl, 750, 155, 850, 155, origin);
    DrawArrow(dl, 750, 175, 850, 175, origin);
    DrawArrow(dl, 750, 195, 850, 195, origin);
    ImGui::SetCursorPos(ImVec2(765, 143));
    ImGui::TextUnformatted("Rd: 44");          // get from DB
    ImGui::SetCursorPos(ImVec2(765, 163));
    ImGui::TextUnformatted("Wr: 0");           // get from DB
    ImGui::SetCursorPos(ImVec2(765, 183));
    ImGui::TextUnformatted("Atomic: 0");       // get from DB

    // Scalar L1D -> L2
    DrawArrow(dl, 750, 315, 850, 315, origin);
    DrawArrow(dl, 750, 335, 850, 335, origin);
    DrawArrow(dl, 750, 355, 850, 355, origin);
    ImGui::SetCursorPos(ImVec2(765, 303));
    ImGui::TextUnformatted("Rd: 0");           // get from DB
    ImGui::SetCursorPos(ImVec2(765, 323));
    ImGui::TextUnformatted("Wr: 0");           // get from DB
    ImGui::SetCursorPos(ImVec2(765, 343));
    ImGui::TextUnformatted("Atomic: 0");       // get from DB

    // Instr L1 -> L2
    DrawArrow(dl, 750, 425, 850, 425, origin);
    ImGui::SetCursorPos(ImVec2(765, 413));
    ImGui::TextUnformatted("Req: 0");          // get from DB

    // --- L2 -> Fabric ---

    DrawArrow(dl, 1025, 190, 1120, 190, origin);
    DrawArrow(dl, 1025, 215, 1120, 215, origin);
    DrawArrow(dl, 1025, 240, 1120, 240, origin);
    ImGui::SetCursorPos(ImVec2(1038, 178));
    ImGui::TextUnformatted("Rd: 44");          // get from DB
    ImGui::SetCursorPos(ImVec2(1038, 203));
    ImGui::TextUnformatted("Wr: 0");           // get from DB
    ImGui::SetCursorPos(ImVec2(1038, 228));
    ImGui::TextUnformatted("Atomic: 0");       // get from DB

    // --- Fabric -> HBM ---

    DrawArrow(dl, 1295, 185, 1350, 185, origin);
    DrawArrow(dl, 1295, 205, 1350, 205, origin);
    ImGui::SetCursorPos(ImVec2(1300, 173));
    ImGui::TextUnformatted("Rd: 44");          // get from DB
    ImGui::SetCursorPos(ImVec2(1300, 193));
    ImGui::TextUnformatted("Wr: 0");           // get from DB
}

}  // namespace View
}  // namespace RocProfVis
