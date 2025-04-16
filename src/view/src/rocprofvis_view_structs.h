#pragma once
#include "imgui.h"

namespace RocProfVis
{
namespace View
{

class Charts;

typedef struct rocprofvis_color_by_value_t
{
    float interest_1_max;
    float interest_1_min;

} rocprofvis_color_by_value_t;

typedef struct rocprofvis_data_point_t
{
    double xValue;
    double yValue;
} rocprofvis_data_point_t;

typedef struct rocprofvis_graph_map_t
{
    enum
    {
        TYPE_LINECHART,
        TYPE_FLAMECHART
    } graph_type;
    bool                        display;
    Charts*                     chart;
    bool                        selected;
    bool                        color_by_value;
    bool                        make_boxplot;
    rocprofvis_color_by_value_t color_by_value_digits;

} rocprofvis_graph_map_t;

}  // namespace View
}  // namespace RocProfVis
