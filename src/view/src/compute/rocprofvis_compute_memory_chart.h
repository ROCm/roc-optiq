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
    // Pipeline stages
    void RenderInstrBuff();
    void RenderInstrDispatch();
    void RenderActiveCUs();

    // Cache hierarchy
    void RenderLDS();
    void RenderVectorL1Cache();
    void RenderScalarL1DCache();
    void RenderInstrL1Cache();

    // Memory subsystem
    void RenderL2Cache();
    void RenderFabric();
    void RenderHBM();

    // Connection arrows and labels between blocks
    void RenderConnections();

    // Get the display string for a metric (returns "-" if not yet populated)
    const char* Val(MemChartMetric m) const;

    DataProvider& m_data_provider;
    std::array<std::string, MEMCHART_METRIC_COUNT> m_values;
};

}  // namespace View
}  // namespace RocProfVis
