// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once
#include <array>
#include <memory>
#include <string>
#include <vector>
#include <cstdint>

struct ImDrawList;
struct ImVec2;

namespace RocProfVis
{
namespace View
{

struct MetricValue;
class DataProvider;
class ComputeSelection;

// Simple rectangle used for block positioning.
// Each block stores its local-space position (relative to the chart origin).
struct ChartBlock
{
    float x = 0, y = 0, w = 0, h = 0;

    float Right()  const { return x + w; }
    float Bottom() const { return y + h; }
    float MidX()   const { return x + w * 0.5f; }
    float MidY()   const { return y + h * 0.5f; }
};

// Maps 1:1 to metric entry IDs in table 3.1
enum MemChartMetric
{
    WAVEFRONT_OCCUPANCY = 0,
    WAVE_LIFE,
    SALU,
    SMEM,
    VALU,
    MATRIX_OPS,
    VMEM,
    LDS,
    GWS,
    BR,
    ACTIVE_CUS_DEPRECATED,
    NUM_CUS,
    VGPR,
    SGPR,
    LDS_ALLOCATION,
    SCRATCH_ALLOCATION,
    WAVEFRONTS,
    WORKGROUPS,
    LDS_REQ,
    LDS_UTIL,
    LDS_LATENCY,
    VL1_RD,
    VL1_WR,
    VL1_ATOMIC,
    VL1_HIT,
    VL1_COALESCE,
    VL1_STALL,
    VL1_L2_RD,
    VL1_L2_WR,
    VL1_L2_ATOMIC,
    SL1D_RD,
    SL1D_HIT,
    SL1D_LAT,
    SL1D_L2_RD,
    SL1D_L2_WR,
    SL1D_L2_ATOMIC,
    IL1_FETCH,
    IL1_HIT,
    IL1_LAT,
    IL1_L2_RD,
    L2_RD,
    L2_WR,
    L2_ATOMIC,
    L2_HIT,
    FABRIC_L2_RD,
    FABRIC_L2_WR,
    FABRIC_L2_ATOMIC,
    FABRIC_RD_LAT,
    FABRIC_WR_LAT,
    FABRIC_ATOMIC_LAT,
    HBM_RD,
    HBM_WR,

    // Chart-only placeholder for rows that intentionally display N/A instead
    // of looking up a metric value.
    MEMCHART_METRIC_NA,
    MEMCHART_METRIC_COUNT = MEMCHART_METRIC_NA  // sentinel: total number of chart slots
};

class ComputeMemoryChartView
{
public:
    ComputeMemoryChartView(DataProvider& data_provider, std::shared_ptr<ComputeSelection> compute_selection);
    ~ComputeMemoryChartView();

    void Render();

    // Fetch all 3.1.x metrics for the given workload and kernels
    void FetchMemChartMetrics();

    void UpdateMetrics();

    uint64_t GetClientId() const { return m_client_id; }

private:
    // Compute all block positions (called once at the start of Render)
    void ComputeLayout();

    // Each Draw* method reads its stored block and renders via the draw list.
    // No child windows — everything is flat draw list primitives.
    void DrawInstrBuff(ImDrawList* draw_list, ImVec2 origin);
    void DrawInstrDispatch(ImDrawList* draw_list, ImVec2 origin);
    void DrawActiveCUs(ImDrawList* draw_list, ImVec2 origin);

    void DrawLDS(ImDrawList* draw_list, ImVec2 origin);
    void DrawVectorL1(ImDrawList* draw_list, ImVec2 origin);
    void DrawScalarL1D(ImDrawList* draw_list, ImVec2 origin);
    void DrawInstrL1(ImDrawList* draw_list, ImVec2 origin);

    void DrawL2(ImDrawList* draw_list, ImVec2 origin);
    void DrawXGMIPCIe(ImDrawList* draw_list, ImVec2 origin);
    void DrawFabric(ImDrawList* draw_list, ImVec2 origin);
    void DrawGMI(ImDrawList* draw_list, ImVec2 origin);
    void DrawHBM(ImDrawList* draw_list, ImVec2 origin);

    void DrawConnections(ImDrawList* draw_list, ImVec2 origin);

    float DrawMetricRow(ImDrawList* draw_list, float block_x, float cursor_y,
                        float block_w, const char* label, MemChartMetric metric_id,
                        const char* unit = "");

    void ShowMetricTooltip(ImVec2 hover_min, ImVec2 hover_max,
                           MemChartMetric metric_id,
                           bool show_description, bool show_raw_value);

    void DrawTextWithTooltip(ImDrawList* draw_list, ImVec2 pos, uint32_t color,
                             const char* text, MemChartMetric metric_id,
                             bool show_description, bool show_raw_value);

    // Get the display string for a metric (returns N/A if not yet populated)
    const char* GetMetricText(MemChartMetric metric) const;

    DataProvider& m_data_provider;
    std::shared_ptr<ComputeSelection> m_compute_selection;

    std::array<std::string, MEMCHART_METRIC_COUNT> m_values;

    uint64_t m_client_id;

    // Cache pointers to MetricValue objects to avoid linear search every frame
    std::vector<const MetricValue*> m_metric_ptrs;
    size_t                          m_last_metrics_count;

    // Block positions computed at the start of Render(), used by all Draw* methods
    ChartBlock m_instr_buff_block, m_instr_dispatch_block, m_active_cus_block;
    ChartBlock m_lds_block, m_vector_l1_block, m_scalar_l1d_block, m_instr_l1_block;
    ChartBlock m_l2_block;
    ChartBlock m_xgmi_pcie_block, m_fabric_block, m_gmi_block;
    ChartBlock m_hbm_block;
};

}  // namespace View
}  // namespace RocProfVis
