// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once
#include "rocprofvis_compute_data_provider.h"
#include "rocprofvis_compute_metric.h"
#include "widgets/rocprofvis_widget.h"

namespace RocProfVis
{
namespace View
{

class ComputeSummaryLeft : public RocWidget
{
public:
    void Render();
    void Update();
    ComputeSummaryLeft(std::shared_ptr<ComputeDataProvider> data_provider);
    ~ComputeSummaryLeft();

private:
    std::shared_ptr<ComputeMetricGroup> m_system_info;
};

class ComputeSummaryRight : public RocWidget
{
public:
    void Render();
    void Update();
    ComputeSummaryRight(std::shared_ptr<ComputeDataProvider> data_provider);
    ~ComputeSummaryRight();

private:
    std::shared_ptr<ComputeMetricGroup> m_kernel_list;
    std::shared_ptr<ComputeMetricGroup> m_dispatch_list;
};

class ComputeSummaryView : public RocWidget
{
public:
    void Render();
    void Update();
    ComputeSummaryView(std::shared_ptr<ComputeDataProvider> data_provider);
    ~ComputeSummaryView(); 

private:
    void RenderMenuBar();

    std::shared_ptr<HSplitContainer> m_container;
    std::shared_ptr<ComputeSummaryLeft> m_left_view;
    std::shared_ptr<ComputeSummaryRight> m_right_view;
};

}  // namespace View
}  // namespace RocProfVis
