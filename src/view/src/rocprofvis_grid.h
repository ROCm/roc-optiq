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

    void  RenderGrid(double min_x, double max_x, double movement, float zoom,
                     ImDrawList* draw_list, float scale_x, float v_max_x, float v_min_x,
                     int grid_size, int sidebar_size);
    float GetCursorPosition();
    double Calibrate();

private:
    float m_cursor_position;
    double m_calibrate;
};

}  // namespace View
}  // namespace RocProfVis
