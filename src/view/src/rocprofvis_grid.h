// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include "imgui.h"
#include "rocprofvis_settings.h"
#include <utility>
namespace RocProfVis
{
namespace View
{

class Grid
{
public:
    Grid();
    ~Grid();

    void RenderGrid(double min_x, double max_x, double movement, float zoom,
                    float scale_x, double v_max_x, double v_min_x, int grid_size,
                    int sidebar_size, ImVec2 graph_size);

    void SetHighlightedRegion(std::pair<float, float> region);

private:
    std::pair<float, float> m_highlighted_region;
    Settings&               m_settings;
};

}  // namespace View
}  // namespace RocProfVis
