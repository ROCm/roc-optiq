// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once
#include "widgets/rocprofvis_widget.h"

namespace RocProfVis
{
namespace View
{

class ComputeDataProvider;
class ComputePlotRoofline;

class ComputeRooflineView : public RocWidget
{
public:
    void Render();
    void Update();
    ComputeRooflineView(std::string root_id, std::shared_ptr<ComputeDataProvider> data_provider);
    ~ComputeRooflineView();

private:
    void RenderMenuBar();

    std::unique_ptr<ComputePlotRoofline> m_roofline_fp64;
    std::unique_ptr<ComputePlotRoofline> m_roofline_fp32;
    std::unique_ptr<ComputePlotRoofline> m_roofline_fp16;
    std::unique_ptr<ComputePlotRoofline> m_roofline_i8;
    int m_group_mode;
    std::string m_owner_id;
};

}  // namespace View
}  // namespace RocProfVis
