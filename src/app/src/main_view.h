
#pragma once
#ifndef MAIN_VIEW_H
#    define MAIN_VIEW_H
#    include "imgui.h"
#    include "structs.h"
#    include <algorithm>
#    include <iostream>
#    include <map>
#    include <string>
#    include <vector>
# include "line_chart.h"

 

class main_view
{
public:
    bool                    viewInit;
    float minValue;
    float maxValue;
    float zoom;
    float movement;
    bool  hasZoomHappened;
    int   id;
    ImVec2 mousePosition;
    float                   minX;
    std::vector<rocprofvis_trace_event_t> flameEvent; 
    float                   maxX;
    float                   minY;
    float                   maxY;
    std::vector<dataPoint>  data_arr;
    void                    generate_graph_points(std::map<std::string, rocprofvis_trace_process_t>& trace_data);
    void    generate_graph_graph_maxmin(
        std::map<std::string, rocprofvis_trace_process_t>& trace_data);
    main_view();
    ~main_view();
    void                    findMaxMin();
    void                    findMaxMinForX();
    void                    renderMain2( int count);
    void                    renderMain3(int count);

    void                    handleTouch();
    void                    handleVerticle();
    std::vector<dataPoint>  extractPointsFromData(void* data);
    bool                    ranOnce;
    bool                    fullyRenderedPoints;
};

#endif
