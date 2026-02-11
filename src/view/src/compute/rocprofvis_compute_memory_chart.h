// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

namespace RocProfVis
{
namespace View
{

class DataProvider;

class ComputeMemoryChartView
{
public:
    ComputeMemoryChartView(DataProvider& data_provider);
    ~ComputeMemoryChartView();

    void Render();
    void Update();

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

    DataProvider& m_data_provider;
};

}  // namespace View
}  // namespace RocProfVis
