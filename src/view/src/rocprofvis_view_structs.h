// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once
#include "imgui.h"

namespace RocProfVis
{
namespace View
{

class TrackItem;

typedef struct HighlightYRange
{
    float max_limit;
    float min_limit;

} rocprofvis_color_by_value_t;

typedef struct rocprofvis_data_point_t
{
    double x_value;
    double y_value;
    double x2_value;
} rocprofvis_data_point_t;

typedef struct rocprofvis_graph_t
{
    enum
    {
        TYPE_LINECHART,
        TYPE_FLAMECHART
    } graph_type;
    bool                        display;
    bool                        display_changed;
    TrackItem*                  chart;
    bool                        selected;

} rocprofvis_graph_t;

}  // namespace View
}  // namespace RocProfVis
