// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once
#include "widgets/rocprofvis_widget.h"

namespace RocProfVis
{
namespace View
{

class ComputeDataProvider;
class ComputePlotRooflineLegacy;

class ComputeRooflineViewLegacy : public RocWidget
{
public:
    void Render();
    void Update();
    ComputeRooflineViewLegacy(std::string root_id, std::shared_ptr<ComputeDataProvider> data_provider);
    ~ComputeRooflineViewLegacy();

private:
    void RenderMenuBar();

    std::unique_ptr<ComputePlotRooflineLegacy> m_roofline_fp64;
    std::unique_ptr<ComputePlotRooflineLegacy> m_roofline_fp32;
    std::unique_ptr<ComputePlotRooflineLegacy> m_roofline_fp16;
    std::unique_ptr<ComputePlotRooflineLegacy> m_roofline_i8;
    int m_group_mode;
    std::string m_owner_id;
};

}  // namespace View
}  // namespace RocProfVis
