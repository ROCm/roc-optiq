#include "line_chart.h"
#include "imgui.h"
#include <iostream>
#include <vector>
#include <algorithm>
#include <string>
template <typename T>
T clamp(const T& value, const T& lower, const T& upper) {
    if (value < lower) {
        return lower;
    } else if (value > upper) {
        return upper;
    } else {
        return value;
    }
}

line_chart::line_chart(int id, float minValue, float maxValue, float zoom, float movement,
                       bool hasZoomHappened, float& minX, float& maxX, float& minY,
                       float& maxY)
   
{
  

    
    this->id = id;
    this->minValue = minValue;
    this->maxValue = maxValue;
    this->zoom = zoom;
    this->movement = movement;
    this->hasZoomHappened = hasZoomHappened;
    this->minX = minX;
    this->maxX = maxX;
    this->minY = minY;
    this->maxY  = maxY;
hasZoomHappened: false;
    data = { { 1.0, 2.0 }, { 2.0, 3.0 }, { 3.0, 4.0 }, { 5.0, 9.0 },
             { 6.0, 2.0 }, { 7.0, 3.0 }, { 8.0, 9.0 }, { 10.0, 9.0 } };
}

line_chart::~line_chart() {}


 


void line_chart::addDataPoint(float x, float y) {
    data.push_back({ x, y });
}

void line_chart::render() {
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoMove;
    // Begin a new ImGui window wi
    if(ImGui::BeginChild((std::to_string(id)).c_str()), ImVec2(0, 150), true,
       window_flags)
    {

   

        ImDrawList* drawList = ImGui::GetWindowDrawList();

        ImVec2 cPosition = ImGui::GetCursorScreenPos();
        ImVec2 cSize = ImGui::GetContentRegionAvail();

         

        float vWidth = (maxX - minX) / zoom;
        float vMinX = minX + movement;
        float vMaxX = vMinX + vWidth;
       
   
        
        for (int i = 1; i < data.size(); i++) {
            ImVec2 point1 = mapToUI(data[i - 1], cPosition, cSize, vMinX, vMaxX, minY, maxY);
            ImVec2 point2 = mapToUI(data[i], cPosition, cSize, vMinX, vMaxX, minY, maxY);
            drawList->AddLine(point1, point2, IM_COL32(0, 0, 0, 255), 2.0f);
        }
    }
    ImGui::EndChild();
}

ImVec2 line_chart::mapToUI(dataPoint& point, ImVec2& cPosition, ImVec2& cSize, float minX,
                           float maxX, float minY, float maxY) {
    float x = cPosition.x + ((point.xValue - minX) / (maxX - minX)) * cSize.x;
    float y = cPosition.y + cSize.y - ((point.yValue - minY) / (maxY - minY)) * cSize.y;
    return ImVec2(x, y);
}
