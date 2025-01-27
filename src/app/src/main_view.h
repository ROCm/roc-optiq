
#pragma once
#ifndef MAIN_VIEW_H
#define MAIN_VIEW_H
#include "imgui.h"
#include <algorithm>
#include <iostream>
#include <vector>
#include <string>
struct dataPoint2
{
    float xValue;
    float yValue;
};

class main_view
{
public:
    float minValue;
    float maxValue;
    float zoom;
    float movement;
    bool  hasZoomHappened;
    int   id;
 
    float minX;
    float maxX;
    float minY;
    float maxY;
    std::vector<dataPoint2> data;

    main_view();
    ~main_view();
    void findMaxMin();

    void renderMain();
    void handleTouch();
 };

#endif
