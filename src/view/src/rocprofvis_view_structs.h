// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once
#include "imgui.h"

namespace RocProfVis
{
namespace View
{

class TrackItem;

typedef struct rocprofvis_color_by_value_t
{
    float interest_1_max;
    float interest_1_min;

} rocprofvis_color_by_value_t;

typedef struct rocprofvis_data_point_t
{
    double x_value;
    double y_value;
} rocprofvis_data_point_t;

typedef struct rocprofvis_graph_t
{
    enum
    {
        TYPE_LINECHART,
        TYPE_FLAMECHART
    } graph_type;
    bool                        display;
    TrackItem*                  chart;
    bool                        selected;
    bool                        color_by_value;
    bool                        colorful_flamechart;
    bool                        make_boxplot;
    rocprofvis_color_by_value_t color_by_value_digits;

} rocprofvis_graph_t;

}  // namespace View
}  // namespace RocProfVis
