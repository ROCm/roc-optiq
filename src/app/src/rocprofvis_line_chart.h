// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include "imgui.h"
#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

struct dataPoint
{
    float xValue;
    float yValue;
};
class LineChart
{
public:
    LineChart(int id, float min_value, float max_value, float zoom, float movement,
              float& min_x, float& max_x, float& min_y, float& max_y,
              std::vector<dataPoint> data, float scale_x);
    ~LineChart();
    void   Render();
    ImVec2 MapToUI(dataPoint& point, ImVec2& c_position, ImVec2& c_size, float scale_x,
                   float scale_y);

private:
    float m_min_value;
    float m_max_value;
    float m_zoom;
    float m_movement;
    float m_min_x;
    float m_max_x;
    float m_min_y;
    float m_max_y;
    float m_scale_x;
    int   m_id;

    std::vector<dataPoint> m_data;
};

