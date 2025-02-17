#ifndef FLAME_CHART_H
#define FLAME_CHART_H

#include <string>
#include <vector>

#include "structs.h"

class FlameChart
{
public:
    int graph_depth;

    FlameChart(int count, double min_value, double max_value, float zoom, float movement,
               double min_x, double max_x,
               const std::vector<rocprofvis_trace_event_t>& data_arr);
    void render() ;

    std::vector<rocprofvis_trace_event_t> flames;
    double                                min_value;
    double                                max_value;
    float                                 zoom;
    float                                 movement;
    double                                min_x;
    double                                max_x;
    double                                min_start_time;
    int                                   count;
};

#endif
