#include "imgui.h"
#include "main_view.h"
#include <algorithm>
#include <iostream>
#include <vector>
#include <string>

#include "line_chart.h"


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


main_view::main_view()
 
{
    
        this->minValue        = 0.0f;
        this->maxValue        = 0.0f;
        this->zoom            = 1.0f;
        this->movement        = 0.0f;
        this->hasZoomHappened = false;
        this->id              = 0;
        this->minX            = 0.0f;
        this->maxX            = 0.0f;
        this->minY            = 0.0f;
        this->maxY            = 0.0f;
        data   = { { 1.0, 2.0 }, { 2.0, 3.0 }, { 3.0, 4.0 }, { 5.0, 9.0 },
                                  { 6.0, 2.0 }, { 7.0, 3.0 }, { 8.0, 9.0 }, { 10.0, 9.0 } };
        findMaxMin();
}
void
main_view::findMaxMin()
{
    std::cout << id;
    minX = data[0].xValue;
    maxX = data[0].xValue;
    minY = data[0].yValue;
    maxY = data[0].yValue;

    for(const auto& point : data)
    {
        if(point.xValue < minX)
        {
            minX = point.xValue;
        }
        if(point.xValue > maxX)
        {
            maxX = point.xValue;
        }
        if(point.yValue < minY)
        {
            minY = point.yValue;
        }
        if(point.yValue > maxY)
        {
            maxY = point.yValue;
        }
    }
}
main_view::~main_view() {}


void
main_view::handleTouch() {
    // Handle Zoom
    if(ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows))
    {
        float scrollWheel = ImGui::GetIO().MouseWheel;
        if(scrollWheel != 0.0f)
        {  // Stops this from constantly running
            zoom *= (scrollWheel > 0) ? 1.1f : 0.9f;
            zoom = clamp(zoom, 0.1f, 9.0f);
        hasZoomHappened:
            true;
        }
    }

    // Handle Panning
    if(ImGui::IsMouseDragging(ImGuiMouseButton_Left))
    {
        float drag      = ImGui::GetIO().MouseDelta.x;
        float viewWidth = (maxX - minX) / zoom;
        movement -= (drag / ImGui::GetContentRegionAvail().x) * viewWidth;
        movement = clamp(movement, 0.0f, (maxX - minX) - viewWidth);
    }

    ImDrawList* drawList = ImGui::GetWindowDrawList();

    ImVec2 cPosition = ImGui::GetCursorScreenPos();
    ImVec2 cSize     = ImGui::GetContentRegionAvail();

    
}
void main_view::renderMain()
{
    ImGui::BeginChild("ScrollableArea", ImVec2(0, 0), true,
                      ImGuiWindowFlags_AlwaysVerticalScrollbar  );
    handleTouch();


 
    // Add boxes (child views)
    for(int i = 0; i < 2; ++i)
    {
        ImGui::BeginChild((std::to_string(i)).c_str(), ImVec2(0, 150), true,
                          ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoMove);
        ImGui::Text("Box %d", i);
         line_chart line = line_chart(i, minValue, maxValue, zoom, movement, hasZoomHappened, minX, maxX, minY, maxY);
        line.render();
        ImGui::EndChild();
        ImGui::Spacing();  // Add some spacing between boxes
    }
   
    ImGui::SetItemAllowOverlap();  // Allow input to pass through

    ImGui::EndChild();
}