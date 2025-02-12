#include "imgui.h"
#include "grid.h"
#include <algorithm>
#include <iostream>
#include <string>
#include <vector>
 template <typename T>
T
clamp(const T& value, const T& lower, const T& upper)
{
    if(value < lower)
    {
        return lower;
    }
    else if(value > upper)
    {
        return upper;
    }
    else
    {
        return value;
    }
}

grid::grid() {}

grid::~grid() {}

void
grid::renderGrid(float minX, float maxX, float movement, float zoom, ImDrawList* drawList)
{
    ImVec2 cPosition = ImGui::GetCursorScreenPos();
    ImVec2 cSize     = ImGui::GetContentRegionAvail();
     float  vWidth    = (maxX - minX) / zoom;
    float  vMinX     = minX + movement;
    float  vMaxX     = vMinX + vWidth;
    float  scaleX    = cSize.x / (vMaxX - vMinX);

    const int numMarks  = 40;
    float     range     = (vMaxX + movement) - (vMinX + movement);
    float     increment = range / numMarks;
    float     actValue  = minX + movement;

    float steps = (maxX - minX) / 50;   
    ImVec2 displaySize = ImGui::GetIO().DisplaySize;

    ImGuiWindowFlags window_flags =
        ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoMove;
    if(ImGui::BeginChild("Grid"), ImVec2(displaySize.x, displaySize.y), true,
       window_flags)
    {

 
        for(float i = minX ; i < maxX ;i += steps)
        {
            float normalized_start = (i - (minX + movement)) * scaleX;
           
                drawList->AddLine(ImVec2(normalized_start, cPosition.y),
                              ImVec2(normalized_start, cPosition.y + cSize.y),
                              IM_COL32(10, 10, 10, 255), 0.5f);

            char label[32];
            snprintf(label, sizeof(label), "%.2f", actValue);

            ImVec2 labelSize = ImGui::CalcTextSize(label);
            ImVec2 labelPos = ImVec2(normalized_start - labelSize.x / 2,
                                      cPosition.y + cSize.y - labelSize.y - 5);
            drawList->AddText(labelPos, IM_COL32(0, 0, 0, 255), label);
            actValue = actValue + steps;
        }

 
    }
    ImGui::EndChild();
}