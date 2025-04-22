// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once
#include "imgui.h"
#include "rocprofvis_compute_block.h"
#include "rocprofvis_compute_data_provider.h"
#include "rocprofvis_compute_summary.h"
#include "rocprofvis_compute_table.h"
#include "widgets/rocprofvis_widget.h"

namespace RocProfVis
{
namespace View
{

typedef enum compute_normalization_unit_t
{
    kPerWave = 0,
    kPerCycle = 1,
    kPerKernel = 2,
    kPerSecond = 3
} compute_normalization_unit_t;

class ComputeRoot : public RocWidget
{
public:
    void Render();
    void Update();
    void SetMetricsPath(std::filesystem::path path);
    bool MetricsLoaded();
    ComputeRoot();
    ~ComputeRoot();

private:
    void RenderMenuBar();

    std::shared_ptr<ComputeSummaryView> m_compute_summary_view;
    std::shared_ptr<ComputeBlockView> m_compute_block_view;
    std::shared_ptr<ComputeTableView> m_compute_table_view;
    std::shared_ptr<ComputeDataProvider> m_compute_data_provider;

    compute_normalization_unit_t m_normalization_unit;
    int m_dispatch_filter;
    int m_kernel_filter;
};

}  // namespace View
}  // namespace RocProfVis
