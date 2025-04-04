// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include "imgui.h"

namespace RocProfVis
{
namespace View
{

class Grid
{
public:
    Grid();
    ~Grid();

    void RenderGrid(double min_x, double max_x, double movement, double zoom,
                    ImDrawList* draw_list, double scale_x, double v_max_x, double v_min_x);
};

}  // namespace View
}  // namespace RocProfVis


