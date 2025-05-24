// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once
#include "rocprofvis_compute_data_provider.h"
#include "rocprofvis_compute_metric.h"
#include "widgets/rocprofvis_widget.h"

namespace RocProfVis
{
namespace View
{

class ComputeRooflineView : public RocWidget
{
public:
    void Render();
    void Update();
    ComputeRooflineView(std::shared_ptr<ComputeDataProvider> data_provider);
    ~ComputeRooflineView();

private:
    void RenderMenuBar();

    std::unique_ptr<ComputeMetricRoofline> m_roofline;
};

}  // namespace View
}  // namespace RocProfVis
