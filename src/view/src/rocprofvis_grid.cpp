// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_grid.h"
#include "imgui.h"
#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

namespace RocProfVis
{
namespace View
{

Grid::Grid() 
{
}
Grid::~Grid()
{
}

void
Grid::RenderGrid(float min_x, float max_x, float movement, float zoom,
                 ImDrawList* draw_list, float scale_x, float v_max_x, float v_min_x)
{
    ImVec2        cursor_position = ImGui::GetCursorScreenPos();
    ImVec2        content_size    = ImGui::GetContentRegionAvail();
    float         range           = (v_max_x + movement) - (v_min_x + movement);
    constexpr int number_of_gridlines =
        50;  // determines the number of gridlines in the grid.
    float steps =
        (max_x - min_x) /
        number_of_gridlines;  // amount the loop which generates the grid iterates by.
    ImVec2 displaySize = ImGui::GetIO().DisplaySize;

    ImGuiWindowFlags window_flags =
        ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoMove;

    if(ImGui::BeginChild("Grid"), ImVec2(displaySize.x, displaySize.y), true,
       window_flags)
    {
        for(float raw_position_points_x = min_x; raw_position_points_x < max_x;
            raw_position_points_x += steps)
        {
            // loop through min-max and create appropriate number of scale markers with
            // marker value printed at bottom.
            float normalized_start =
                (raw_position_points_x - (min_x + movement)) *
                scale_x;  // this value takes the raw value of the output and converts
                          // them into positions on the chart which is scaled by scale_x

            draw_list->AddLine(
                ImVec2(normalized_start, cursor_position.y),
                ImVec2(normalized_start, cursor_position.y + content_size.y),
                IM_COL32(0, 0, 0, 128), 1.0f);

            char label[32];
            snprintf(label, sizeof(label), "%.2f", raw_position_points_x - min_x);
            // All though the gridlines are drawn based on where they should be on the
            // scale the raw values are used to represent them.
            ImVec2 labelSize = ImGui::CalcTextSize(label);
            ImVec2 labelPos =
                ImVec2(normalized_start - labelSize.x / 2,
                       cursor_position.y + content_size.y - labelSize.y - 5);
            draw_list->AddText(labelPos, IM_COL32(0, 0, 0, 255), label);
        }
    }
    ImGui::EndChild();
}

}  // namespace View
}  // namespace RocProfVis

