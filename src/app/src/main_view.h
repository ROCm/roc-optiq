
#pragma once
#ifndef MAIN_VIEW_H
#    define MAIN_VIEW_H
#    include "imgui.h"
#    include "line_chart.h"
#    include "structs.h"
#    include <algorithm>
#    include <iostream>
#    include <map>
#    include <string>
#    include <vector>

class main_view
{
public:
    bool                                  viewInit;
    float                                 minValue;
    float                                 maxValue;
    float                                 zoom;
    float                                 movement;
    bool                                  hasZoomHappened;
    ImVec2                                mousePosition;
    float                                 minX;
    std::vector<rocprofvis_trace_event_t> flameEvent;
    float                                 maxX;
    float                                 minY;
    float                                 maxY;
    std::vector<dataPoint>                data_arr;
    bool                                  ranOnce;
    bool                                  fullyRenderedPoints;
    bool                                  renderedOnce;
    float scrollPosition; 
    main_view();
    ~main_view();

    void generate_graph_points(
        std::map<std::string, rocprofvis_trace_process_t>& trace_data);

    void make_grid();
    void make_graph_view(std::map<std::string, rocprofvis_trace_process_t>& trace_data);
    void make_graph_metadata_view(std::map<std::string, rocprofvis_trace_process_t>& trace_data);

    void                                  findMaxMin();
    void                                  findMaxMinFlame();
    void                                  renderMain2(int count);
    void                                  renderMain3(int count);
    void renderGraphMetadata(int graphID, float size, std::string type,
                             rocprofvis_metadata_visualization data);

    void                                  handleTouch();
    std::vector<dataPoint>                extractPointsFromData(void* data);
    std::vector<rocprofvis_trace_event_t> extractFlamePoints(
        const std::vector<rocprofvis_trace_event_t>& traceEvents);
};

#endif
