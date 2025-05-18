// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_grid.h"
#include "imgui.h"
#include "rocprofvis_settings.h"
#include <algorithm>
#include <iostream>
#include <string>
#include <utility>
#include <vector>
namespace RocProfVis
{
namespace View
{

double
Grid::GetViewportStartPosition()
{
    return m_viewport_start_position;
}
double
Grid::GetViewportEndPosition()
{
    return GetCursorPosition(m_content_size_x);
}
Grid::Grid()
: m_viewport_start_position(std::numeric_limits<double>::lowest())
, m_highlighted_region({ -1.f, -1.f })
, m_settings(Settings::GetInstance())
, m_viewport_end_position(std::numeric_limits<double>::max())
, m_content_size_x()
, m_sidebar_size()
, m_scale_x()
, m_min_x()
{}
Grid::~Grid() {}

double
Grid::GetCursorPosition(float mouse_position)
{
    return (m_viewport_start_position + ((mouse_position) * (1 / m_scale_x)) - m_min_x);
}

void
Grid::SetHighlightedRegion(std::pair<float, float> region)
{
    m_highlighted_region = region;
}

void
Grid::RenderGrid(double min_x, double max_x, double movement, float zoom, float scale_x,
                 float v_max_x, float v_min_x, int grid_size, int sidebar_size)
{
    ImVec2 cursor_position = ImGui::GetCursorScreenPos();
    ImVec2 content_size    = ImGui::GetContentRegionAvail();
    double range           = (v_max_x + movement) - (v_min_x + movement);
    ImVec2 displaySize     = ImGui::GetIO().DisplaySize;

    m_content_size_x = content_size.x;
    m_sidebar_size   = sidebar_size;
    m_scale_x        = scale_x;
    m_min_x          = min_x;

    double steps = (max_x - min_x) /
                   (zoom * 20);  // amount the loop which generates the grid iterates by.

    ImGuiWindowFlags window_flags =
        ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoMove;

    ImGui::SetCursorPos(ImVec2(sidebar_size, 0));

    if(ImGui::BeginChild("Grid"),
       ImVec2(displaySize.x - sidebar_size, displaySize.y - 30.0f), true, window_flags)
    {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, m_settings.GetColor(static_cast<int>(
                                                    Colors::kFillerColor)));
        ImGui::BeginChild("background component",
                          ImVec2(displaySize.x - sidebar_size, displaySize.y - 30.0f),
                          true);

        ImGui::EndChild();
        ImGui::PopStyleColor();

        ImGui::SetCursorPos(ImVec2(0, 0));

        ImGui::BeginChild("main component",
                          ImVec2(displaySize.x - sidebar_size, displaySize.y - 30.0f),
                          false);
        ImVec2 child_win  = ImGui::GetWindowPos();
        ImVec2 child_size = ImGui::GetWindowSize();

        // Define the clipping rectangle to match the child window
        ImVec2 clip_min = child_win;
        ImVec2 clip_max = ImVec2(child_win.x + child_size.x, child_win.y + child_size.y);
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        draw_list->PushClipRect(clip_min, clip_max, true);

        double normalized_start_box = (min_x - (min_x + movement)) * scale_x;

        ///////Code below is for selection visuals.
        if(m_highlighted_region.first != -1)
        {
            double normalized_start_box_highlighted =
                (m_highlighted_region.first - movement) * scale_x;

            draw_list->AddLine(
                ImVec2(normalized_start_box_highlighted, cursor_position.y),
                ImVec2(normalized_start_box_highlighted,
                       cursor_position.y + content_size.y - grid_size),
                m_settings.GetColor(static_cast<int>(Colors::kSelectionBorder)), 3.0f);
        }
        if(m_highlighted_region.first != -1)
        {
            double normalized_start_box_highlighted_end =
                (m_highlighted_region.second - movement) * scale_x;

            draw_list->AddLine(
                ImVec2(normalized_start_box_highlighted_end, cursor_position.y),
                ImVec2(normalized_start_box_highlighted_end,
                       cursor_position.y + content_size.y - grid_size),
                m_settings.GetColor(static_cast<int>(Colors::kSelectionBorder)), 3.0f);
        }
        if(m_highlighted_region.first != -1 && m_highlighted_region.second != -1)
        {
            double normalized_start_box_highlighted =
                (m_highlighted_region.first - movement) * scale_x;
            double normalized_start_box_highlighted_end =
                (m_highlighted_region.second - movement) * scale_x;
            draw_list->AddRectFilled(
                ImVec2(normalized_start_box_highlighted, cursor_position.y),
                ImVec2(normalized_start_box_highlighted_end,
                       cursor_position.y + content_size.y - grid_size),
                m_settings.GetColor(static_cast<int>(Colors::kSelection)));
        }

        draw_list->AddRectFilled(
            ImVec2(normalized_start_box, cursor_position.y),
            ImVec2(normalized_start_box - 1500.0f,
                   cursor_position.y + content_size.y - grid_size),
            m_settings.GetColor(static_cast<int>(Colors::kBoundBox)));

        double normalized_start_box_end = (max_x - (min_x + movement)) * scale_x;
        draw_list->AddRectFilled(
            ImVec2(normalized_start_box_end, cursor_position.y),
            ImVec2(normalized_start_box_end + content_size.x,
                   cursor_position.y + content_size.y - grid_size),
            m_settings.GetColor(static_cast<int>(Colors::kBoundBox)));
        bool   has_been_seen = false;
        double last_been_seen;
        int    rectangle_render_count = 0;
        for(double raw_position_points_x = min_x - (steps);
            raw_position_points_x < max_x + (steps); raw_position_points_x += steps)
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

            // IsRectVisible checks overlaping with windows coordinates. So we have to add
            // child_win.x to normalized_start to see smaller traces.
            if(ImGui::IsRectVisible(
                   ImVec2(child_win.x + normalized_start, cursor_position.y),
                   ImVec2(child_win.x + normalized_end,
                          cursor_position.y + content_size.y - grid_size)))
            {
                if(has_been_seen == false)
                {
                    // First one thats visible.

                    rectangle_render_count = rectangle_render_count + 1;
                    m_viewport_start_position =
                        (raw_position_points_x -
                         (steps * (1 - ((clip_min.x - normalized_start) /
                                        (normalized_end - normalized_start))))) +
                        steps;
                    has_been_seen = true;
                }
                last_been_seen = (raw_position_points_x +
                                  (steps * (1 - ((clip_min.x - normalized_start) /
                                                 (normalized_end - normalized_start))))) +
                                 steps;
            }

            // Only render visible grid lines or the clipping time is excessive when
            // zooming in to large traces
            if(has_been_seen)
            {
                draw_list->AddRect(
                    ImVec2(normalized_start, cursor_position.y),
                    ImVec2(normalized_end,
                           cursor_position.y + content_size.y - grid_size),
                    m_settings.GetColor(static_cast<int>(Colors::kBoundBox)), 0.5f);

                char label[32];
                snprintf(label, sizeof(label), "%.0f", raw_position_points_x - min_x);
                // All though the gridlines are drawn based on where they should be on the
                // scale the raw values are used to represent them.
                ImVec2 labelSize = ImGui::CalcTextSize(label);
                ImVec2 labelPos =
                    ImVec2(normalized_start - labelSize.x / 2,
                           cursor_position.y + content_size.y - labelSize.y - 5);
                draw_list->AddText(
                    labelPos, m_settings.GetColor(static_cast<int>(Colors::kGridColor)),
                    label);
            }
        }

        ImVec2 windowPos  = ImGui::GetWindowPos();
        ImVec2 windowSize = ImGui::GetWindowSize();
        float  boxWidth   = 300.0f;  // Specify the width of the box
        draw_list->PopClipRect();
        ImGui::EndChild();

        ImGui::EndChild();
    }
}

}  // namespace View
}  // namespace RocProfVis
