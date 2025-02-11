
#pragma once
#ifndef LINE_CHART_H
#define LINE_CHART_H
#include <vector>
#include <iostream>
#include "imgui.h"
#include <algorithm>
#include <string>
struct dataPoint
{
    float xValue;
    float yValue;
 
    
};

class line_chart
{
public:
    int  id;
    float minValue;
    float maxValue;
    float zoom;
    float movement; 
    bool hasZoomHappened;
    float minX;
    float  maxX;
    float minY;
    float maxY;
    std::vector<dataPoint> data;

    line_chart(int id, float minValue, float maxValue, float zoom, float movement,
               bool hasZoomHappened, float& minX, float& maxX, float& minY, float& maxY,
               std::vector<dataPoint> data);
    ~line_chart();
      void addDataPoint(float x, float y);
    void render(); 
    ImVec2 mapToUI(dataPoint& point, ImVec2& cPosition, ImVec2& cSize, float scaleX,
                   float scaleY );
     
 
 
};

#endif   
