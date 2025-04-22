// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once
#include "imgui.h"
#include "rocprofvis_compute_data_provider.h"
#include "rocprofvis_compute_metric.h"
#include "widgets/rocprofvis_widget.h"

namespace RocProfVis
{
namespace View
{

class ComputeTableView : public RocWidget
{
public:
    void Render();
    ComputeTableView(std::shared_ptr<ComputeDataProvider> data_provider);
    ~ComputeTableView();

private:
    void RenderMenuBar();
};

}  // namespace View
}  // namespace RocProfVis
