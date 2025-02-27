
#pragma once
#ifndef GRID_H
#    define GRID_H
#    include "imgui.h"
#    include <algorithm>
#    include <iostream>
#    include <string>
#    include <vector>

class Grid
{
public:
    Grid();
    ~Grid();

    void RenderGrid(float min_x, float max_x, float movement, float zoom,
                    ImDrawList* draw_list);
};

#endif
