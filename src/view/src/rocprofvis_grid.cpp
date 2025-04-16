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
: m_cursor_position(0.0f)
{}
Grid::~Grid() {}

float
Grid::GetCursorPosition()
{
    return m_cursor_position;
}

void
Grid::RenderGrid(double min_x, double max_x, double movement, float zoom,
                 ImDrawList* draw_list, float scale_x, float v_max_x, float v_min_x)
{
    ImVec2 cursor_position = ImGui::GetCursorScreenPos();
    ImVec2 content_size    = ImGui::GetContentRegionAvail();
    double range           = (v_max_x + movement) - (v_min_x + movement);
    ImVec2 displaySize     = ImGui::GetIO().DisplaySize;

    double steps = (max_x - min_x) /
                  (zoom * 20);  // amount the loop which generates the grid iterates by.

    ImGuiWindowFlags window_flags =
        ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoMove;

    float temp_sidebar_size = 400.0f;  // Replace with resizeable sidebar variable.

    ImGui::SetCursorPos(ImVec2(temp_sidebar_size, 0));

    if(ImGui::BeginChild("Grid"),
       ImVec2(displaySize.x - temp_sidebar_size, displaySize.y - 30.0f), true,
       window_flags)
    {
        ImVec2 child_win  = ImGui::GetWindowPos();
        ImVec2 child_size = ImGui::GetWindowSize();

        // Define the clipping rectangle to match the child window
        ImVec2 clip_min = child_win;
        ImVec2 clip_max = ImVec2(child_win.x + child_size.x, child_win.y + child_size.y);
        draw_list->PushClipRect(clip_min, clip_max, true);

        double normalized_start_box = (min_x - (min_x + movement)) * scale_x;
        draw_list->AddRectFilled(ImVec2(normalized_start_box, cursor_position.y),
                                 ImVec2(normalized_start_box - 1500.0f,
                                        cursor_position.y + content_size.y - 50.0f),
                                 IM_COL32(100, 100, 100, 100));

        double normalized_start_box_end = (max_x - (min_x + movement)) * scale_x;
        draw_list->AddRectFilled(ImVec2(normalized_start_box_end, cursor_position.y),
                                 ImVec2(normalized_start_box_end + 1500.0f,
                                        cursor_position.y + content_size.y - 50.0f),
                                 IM_COL32(100, 100, 100, 100));

        for(double raw_position_points_x = min_x - (steps * 5);
            raw_position_points_x < max_x + (steps * 5); raw_position_points_x += steps)
        {
            // loop through min-max and create appropriate number of scale markers with
            // marker value printed at bottom.

            double normalized_start =
                (raw_position_points_x - (min_x + movement)) *
                scale_x;  // this value takes the raw value of the output and converts
                          // them into positions on the chart which is scaled by scale_x

            double normalized_end =
                ((raw_position_points_x + steps) - (min_x + movement)) *
                scale_x;  // this value takes the raw value of the output and converts
                          // them into positions on the chart which is scaled by scale_x

            draw_list->AddRect(
                ImVec2(normalized_start, cursor_position.y),
                ImVec2(normalized_end, cursor_position.y + content_size.y - 50.0f),
                IM_COL32(0, 0, 0, 128), 1.0f);

            // If user hover over grid block.
            if(ImGui::IsMouseHoveringRect(
                   ImVec2(normalized_start, cursor_position.y),
                   ImVec2(normalized_end, cursor_position.y + content_size.y - 50.0f)))
            {
                ImVec2 mouse_position = ImGui::GetMousePos();

                ImVec2 relativeMousePos = ImVec2(mouse_position.x - normalized_start,
                                                 mouse_position.y - cursor_position.y);
                m_cursor_position =
                    (raw_position_points_x - min_x) +
                    ((relativeMousePos.x / (normalized_end - normalized_start)) *
                     (steps));
            }

            char label[32];
            snprintf(label, sizeof(label), "%.0f", raw_position_points_x - min_x);
            // All though the gridlines are drawn based on where they should be on the
            // scale the raw values are used to represent them.
            ImVec2 labelSize = ImGui::CalcTextSize(label);
            ImVec2 labelPos =
                ImVec2(normalized_start - labelSize.x / 2,
                       cursor_position.y + content_size.y - labelSize.y - 5);
            draw_list->AddText(labelPos, IM_COL32(0, 0, 0, 255), label);
        }
        draw_list->AddLine(
            ImVec2(0, cursor_position.y + content_size.y - 50.0f),
            ImVec2(displaySize.x, cursor_position.y + content_size.y - 50.0f),
            IM_COL32(0, 0, 0, 128), 1.0f);

        ImVec2 windowPos  = ImGui::GetWindowPos();
        ImVec2 windowSize = ImGui::GetWindowSize();
        float  boxWidth   = 300.0f;  // Specify the width of the box
        draw_list->PopClipRect();
        ImGui::EndChild();
    }
}

}  // namespace View
}  // namespace RocProfVis
