#ifndef FLAME_CHART_H
#define FLAME_CHART_H

#include <string>
#include <vector>

#include "structs.h"

class FlameChart
{
public:
    int graph_depth;

    FlameChart(int chart_id, double min_value, double max_value, float zoom,
               float movement,
               double min_x, double max_x,
               const std::vector<rocprofvis_trace_event_t>& data_arr, float scale_x);
    void render() ;
    void DrawBox(ImVec2 start_position, int boxplot_box_id,
                 rocprofvis_trace_event_t flame, float duration, ImDrawList* draw_list); 

    std::vector<rocprofvis_trace_event_t> flames;
    double                                min_value;
    double                                max_value;
    float                                 zoom;
    float                                 movement;
    double                                min_x;
    double                                max_x;
    double                                min_start_time;
    int                                   chart_id;
    float scale_x;
};

#endif
