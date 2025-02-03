#include "imgui.h"
#include "grid.h"
#include <algorithm>
#include <iostream>
#include <string>
#include <vector>
 

grid::grid() {}

grid::~grid() {}

void
grid::renderGrid()
{
    ImVec2 scaleSize(500.0f, 200.0f); 
    ImGui::BeginChild("Scale", scaleSize, true);

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2      sPos     = ImGui::GetCursorScreenPos();



    if (ImGui::IsWindowHovered()) {
        ImVec2 mPos = ImGui::GetMousePos();
        drawList->AddLine(ImVec2(mPos.x, sPos.y), ImVec2(mPos.x, sPos.y+scaleSize.y), IM_COL32(0,0,0,255), 2.0f);
    } 

ImGui::EndChild();
}

 
