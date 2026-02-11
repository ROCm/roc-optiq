// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once
#include <array>
#include <string>
#include <vector>
#include <cstdint>

namespace RocProfVis
{
namespace View
{

class DataProvider;

// Simple rectangle returned by each block render function.
// Position the next block relative to this one using Right(), Bottom(), MidY(), etc.
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
    MFMA,
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

    MEMCHART_METRIC_COUNT  // sentinel: total number of metrics (52)
};

class ComputeMemoryChartView
{
public:
    ComputeMemoryChartView(DataProvider& data_provider);
    ~ComputeMemoryChartView();

    void Render();
    void Update();

    // Fetch all 3.1.x metrics for the given workload and kernels
    void FetchMemChartMetrics(uint32_t workload_id, const std::vector<uint32_t>& kernel_ids);

private:
    // Each block render takes a position and returns its bounds.
    // The next block is positioned relative to the previous one.

    // Pipeline stage blocks
    ChartBlock RenderInstrBuff(float x, float y);
    ChartBlock RenderInstrDispatch(float x, float y);
    ChartBlock RenderActiveCUs(float x, float y);

    // Cache hierarchy blocks
    ChartBlock RenderLDS(float x, float y);
    ChartBlock RenderVectorL1Cache(float x, float y);
    ChartBlock RenderScalarL1DCache(float x, float y);
    ChartBlock RenderInstrL1Cache(float x, float y);

    // Memory subsystem blocks
    ChartBlock RenderL2Cache(float x, float y, float h);
    ChartBlock RenderXGMIPCIe(float x, float y);
    ChartBlock RenderFabricBlock(float x, float y);
    ChartBlock RenderGMI(float x, float y);
    ChartBlock RenderHBM(float x, float y);

    // Connection arrows and labels (uses stored block positions)
    void RenderConnections();

    // Get the display string for a metric (returns "-" if not yet populated)
    const char* Val(MemChartMetric m) const;

    DataProvider& m_data_provider;
    std::array<std::string, MEMCHART_METRIC_COUNT> m_values;

    // Block positions stored during Render() for use by RenderConnections()
    ChartBlock m_instrBuff, m_instrDispatch, m_activeCUs;
    ChartBlock m_lds, m_vectorL1, m_scalarL1D, m_instrL1;
    ChartBlock m_l2;
    ChartBlock m_xgmiPcie, m_fabric, m_gmi;
    ChartBlock m_hbm;
};

}  // namespace View
}  // namespace RocProfVis
