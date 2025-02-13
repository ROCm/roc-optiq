#ifndef FLAME_CHART_H
#define FLAME_CHART_H

#include <string>
#include <vector>

#include "structs.h"

class FlameChart
{
public:
    FlameChart(int count, double minValue, double maxValue, float zoom, float movement,
               double minX, double maxX,
               const std::vector<rocprofvis_trace_event_t>& data_arr);
    void render() const;

    std::vector<rocprofvis_trace_event_t> flames;
    double                                minValue;
    double                                maxValue;
    float                                 zoom;
    float                                 movement;
    double                                minX;
    double                                maxX;
    double                                min_start_time;
    int                                   count;
};

#endif   
