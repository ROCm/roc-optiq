
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

 

class MainView
{
public:
    float                                 min_value;
    float                                 max_value;
    float                                 zoom;
    float                                 movement;
    bool                                  has_zoom_happened;
    float                                 min_x;
    std::vector<rocprofvis_trace_event_t> flame_event;
    float                                 max_x;
    float                                 min_y;
    float                                 max_y;
    std::vector<dataPoint>                data_arr;
    bool                                  ran_once;
    float                                 scroll_position;
    bool user_adjusting_graph_height;
    bool meta_map_made;
    std::map < int, meta_map_struct> meta_map;
    MainView();
    ~MainView();

    void GenerateGraphPoints(
        std::map<std::string, rocprofvis_trace_process_t>& trace_data);

    void MakeGrid();
    void MakeGraphView(std::map<std::string, rocprofvis_trace_process_t>& trace_data);
    void MakeGraphMetadataView(
        std::map<std::string, rocprofvis_trace_process_t>& trace_data);
    void HandleGraphResize(int chart_id);
    void FindMaxMin();
    void FindMaxMinFlame();
    void RenderLineCharts(int chart_id);
    void RenderFlameCharts(int chart_id);
    void RenderGraphMetadata(int graph_id, float size, std::string type,
                             meta_map_struct data);

    void                                  HandleTopSurfaceTouch();
    std::vector<dataPoint>                ExtractPointsFromData(void* data);
    std::vector<rocprofvis_trace_event_t> ExtractFlamePoints(
        const std::vector<rocprofvis_trace_event_t>& traceEvents);
};

#endif
