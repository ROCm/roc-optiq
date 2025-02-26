#include "grid.h"
#include "imgui.h"
#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

Grid::Grid() {}
Grid::~Grid() {}

void
Grid::RenderGrid(float min_x, float max_x, float movement, float zoom, ImDrawList* draw_list)
{
    ImVec2 cursor_position = ImGui::GetCursorScreenPos();
    ImVec2 content_size     = ImGui::GetContentRegionAvail();
    float  v_width    = (max_x - min_x) / zoom;
    float  v_min_x     = min_x + movement;
    float  v_max_x            = v_min_x + v_width;
    float  scale_x           = content_size.x / (v_max_x - v_min_x);

    const int num_marks  = 40;
    float     range     = (v_max_x + movement) - (v_min_x + movement);
    float     increment  = range / num_marks;
 
    float  steps       = (max_x - min_x) / 50;
    ImVec2 displaySize = ImGui::GetIO().DisplaySize;

    ImGuiWindowFlags window_flags =
        ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoMove;

 


    if(ImGui::BeginChild("Grid"), ImVec2(displaySize.x, displaySize.y), true,
       window_flags)
    {
    
        for(float i = min_x; i < max_x; i += steps)
        {
            float normalized_start = (i - (min_x + movement)) * scale_x;

            draw_list->AddLine(ImVec2(normalized_start, cursor_position.y),
                              ImVec2(normalized_start, cursor_position.y + content_size.y),
                IM_COL32(100, 100, 100, 128), 1.0f);

            char label[32];
            snprintf(label, sizeof(label), "%.2f", i - min_x);

            ImVec2 labelSize = ImGui::CalcTextSize(label);
            ImVec2 labelPos  = ImVec2(normalized_start - labelSize.x / 2,
                       cursor_position.y + content_size.y - labelSize.y - 5);
            draw_list->AddText(labelPos, IM_COL32(255, 255, 255, 255), label);
         }
    }
    ImGui::EndChild();
}