// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once
#include "imgui.h"
#include "implot.h"
#include "rocprofvis_compute_data_provider.h"
#include "widgets/rocprofvis_widget.h"

namespace RocProfVis
{
namespace View
{

typedef enum compute_metric_render_modes_t
{
    kComputeMetricTable = 1 << 0,
    kComputeMetricPie = 1 << 1,
    kComputeMetricBar = 1 << 2
} compute_metric_render_modes_t;

typedef int compute_metric_render_flags_t;

class ComputeMetric : public RocWidget
{
public:
    void Render();
    void Update();
    ComputeMetric(std::shared_ptr<ComputeDataProvider> data_provider, std::string metric_category, compute_metric_render_flags_t render_flags = kComputeMetricTable);
    ~ComputeMetric();

private:
    std::shared_ptr<ComputeDataProvider> m_data_provider;
    std::string m_metric_category;
    compute_metric_render_flags_t m_render_flags;
    rocprofvis_compute_metric_group_t* m_metric_data;
};

}  // namespace View
}  // namespace RocProfVis
