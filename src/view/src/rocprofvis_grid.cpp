// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_grid.h"
#include "imgui.h"
#include "rocprofvis_settings.h"
#include "widgets/rocprofvis_debug_window.h"
#include <algorithm>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace RocProfVis
{
namespace View
{

Grid::Grid()
: m_highlighted_region({ -1.f, -1.f })
, m_settings(Settings::GetInstance())
{}
Grid::~Grid() {}

void
Grid::SetHighlightedRegion(std::pair<float, float> region)
{
    m_highlighted_region = region;
}

void
Grid::RenderGrid(double min_x, double max_x, double movement, float zoom, float scale_x,
                 double v_max_x, double v_min_x, int grid_size, int sidebar_size,
                 ImVec2 graph_size)
{
    ImVec2 container_pos =
        ImVec2(ImGui::GetWindowPos().x + sidebar_size, ImGui::GetWindowPos().y);

    ImVec2 cursor_position = ImGui::GetCursorScreenPos();
    ImVec2 content_size    = ImVec2(graph_size.x, graph_size.y - 50);
    double range           = (v_max_x + movement) - (v_min_x + movement);
    ImVec2 displaySize     = graph_size;

    double stepSize = 0;
    double steps    = 0;
    {
        char label[32];
        snprintf(label, sizeof(label), "%.0f", max_x);
        ImVec2 labelSize = ImGui::CalcTextSize(label);

        // amount the loop which generates the grid iterates by.
        steps = displaySize.x / labelSize.x;

        stepSize = labelSize.x;
    }

    ImGuiWindowFlags window_flags =
        ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoMove;

    ImGui::SetCursorPos(ImVec2(sidebar_size, 0));

    if(ImGui::BeginChild("Grid"), ImVec2(displaySize.x, displaySize.y - 30.0f), true,
       window_flags)
    {
        ImGui::SetCursorPos(ImVec2(0, 0));

        ImGui::BeginChild("main component", ImVec2(displaySize.x, displaySize.y - 30.0f),
                          false);
        ImVec2 child_win  = ImGui::GetWindowPos();
        ImVec2 child_size = ImGui::GetWindowSize();

        // Define the clipping rectangle to match the child window
        ImVec2 clip_min = child_win;
        ImVec2 clip_max =
            ImVec2(child_win.x + content_size.x, child_win.y + content_size.y);
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        draw_list->PushClipRect(clip_min, clip_max, true);

        double normalized_start_box =
            container_pos.x + (min_x - (min_x + movement)) * scale_x;

        if(m_highlighted_region.first != -1)
        {
            double normalized_start_box_highlighted =
                container_pos.x + (m_highlighted_region.first - movement) * scale_x;

            draw_list->AddLine(
                ImVec2(normalized_start_box_highlighted, cursor_position.y),
                ImVec2(normalized_start_box_highlighted,
                       cursor_position.y + content_size.y - grid_size),
                m_settings.GetColor(static_cast<int>(Colors::kSelectionBorder)), 3.0f);
        }
        if(m_highlighted_region.first != -1)
        {
            double normalized_start_box_highlighted_end =
                container_pos.x + (m_highlighted_region.second - movement) * scale_x;

            draw_list->AddLine(
                ImVec2(normalized_start_box_highlighted_end, cursor_position.y),
                ImVec2(normalized_start_box_highlighted_end,
                       cursor_position.y + content_size.y - grid_size),
                m_settings.GetColor(static_cast<int>(Colors::kSelectionBorder)), 3.0f);
        }
        if(m_highlighted_region.first != -1 && m_highlighted_region.second != -1)
        {
            double normalized_start_box_highlighted =
                container_pos.x + (m_highlighted_region.first - movement) * scale_x;
            double normalized_start_box_highlighted_end =
                container_pos.x + (m_highlighted_region.second - movement) * scale_x;
            draw_list->AddRectFilled(
                ImVec2(normalized_start_box_highlighted, cursor_position.y),
                ImVec2(normalized_start_box_highlighted_end,
                       cursor_position.y + content_size.y - grid_size),
                m_settings.GetColor(static_cast<int>(Colors::kSelection)));
        }

        int    rectangle_render_count = 0;
        double m_v_width              = (max_x - min_x) / zoom;

        double drag = (movement / m_v_width) * displaySize.x;
        drag        = (int) drag % (int) stepSize;

        for(float i = 0; i < steps + 1; i++)
        {
            float linePos = stepSize * i;

            linePos -= drag;

            float cursor_screen_percentage = (linePos) / displaySize.x;

            double normalized_start = container_pos.x + linePos;

            DebugWindow::GetInstance()->AddDebugMessage(
                "what a drag: " + std::to_string(drag) + " movement: " +
                std::to_string(movement) + " zoom: " + std::to_string(zoom));

            draw_list->AddLine(
                ImVec2(normalized_start, cursor_position.y),
                ImVec2(normalized_start, cursor_position.y + content_size.y - grid_size),
                m_settings.GetColor(static_cast<int>(Colors::kBoundBox)), 0.5f);

            char label[32];
            snprintf(label, sizeof(label), "%.0f",
                     movement + (cursor_screen_percentage * m_v_width));
            // All though the gridlines are drawn based on where they should be on the
            // scale the raw values are used to represent them.
            ImVec2 labelSize = ImGui::CalcTextSize(label);
            ImVec2 labelPos =
                ImVec2(normalized_start - labelSize.x / 2,
                       cursor_position.y + content_size.y - labelSize.y - 5);
            draw_list->AddText(labelPos,
                               m_settings.GetColor(static_cast<int>(Colors::kGridColor)),
                               label);
        }

        draw_list->PopClipRect();
        ImGui::EndChild();

        ImGui::EndChild();
    }
}

}  // namespace View
}  // namespace RocProfVis
