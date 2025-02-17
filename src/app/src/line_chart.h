
#pragma once
#ifndef LINE_CHART_H
#    define LINE_CHART_H
#    include "imgui.h"
#    include <algorithm>
#    include <iostream>
#    include <string>
#    include <vector>
struct dataPoint
{
    float xValue;
    float yValue;
};
class LineChart
{
public:
    int                    id;
    float                  min_value;
    float                  max_value;
    float                  zoom;
    float                  movement;
    bool                   has_zoom_happened;
    float                  min_x;
    float                  max_x;
    float                  min_y;
    float                  max_y;
    std::vector<dataPoint> data;

    LineChart(int id, float min_value, float max_value, float zoom, float movement,
              bool has_zoom_happened, float& min_x, float& max_x, float& min_y,
              float& max_y, std::vector<dataPoint> data);
    ~LineChart();
    void   AddDataPoint(float x, float y);
    void   Render();
    ImVec2 MapToUI(dataPoint& point, ImVec2& c_position, ImVec2& c_size, float scale_x,
                   float scale_y);
    void   RenderGrid();
};

#endif
