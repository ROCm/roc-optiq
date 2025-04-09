//  Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once
#include <future>
#include <map>
#include <string>
#include <vector>
#include "imgui.h"

class Charts;

typedef struct rocprofvis_trace_event_t
{
    std::string m_name;
    double      m_start_ts;
    double      m_duration;
} rocprofvis_trace_event_t;

typedef struct rocprofvis_color_by_value_t
{
    float interest_1_max;
    float interest_1_min;
   
} rocprofvis_color_by_value_t;

typedef struct rocprofvis_data_point_t
{
    float xValue;
    float yValue;
} rocprofvis_data_point_t;

typedef struct rocprofvis_graph_map_t
{
    enum
    {
        TYPE_LINECHART,
        TYPE_FLAMECHART
    } graph_type;
    bool    display;
    Charts* chart;
    ImVec4  selected;

    bool                      color_by_value;
    bool                        make_boxplot;
    rocprofvis_color_by_value_t color_by_value_digits;

} rocprofvis_graph_map_t;

typedef struct rocprofvis_trace_counter_t
{
    double m_start_ts;
    double m_value;
} rocprofvis_trace_counter_t;

typedef struct rocprofvis_trace_thread_t
{
    std::string                             m_name;
    std::vector<rocprofvis_trace_event_t>   m_events;
    std::vector<rocprofvis_trace_event_t>   m_events_l1;
    std::vector<rocprofvis_trace_counter_t> m_counters;
    std::vector<rocprofvis_trace_counter_t> m_counters_l1;
    bool                                    m_has_events_l1   = false;
    bool                                    m_has_counters_l1 = false;
} rocprofvis_trace_thread_t;

struct rocprofvis_trace_process_t
{
    std::string                                      m_name;
    std::map<std::string, rocprofvis_trace_thread_t> m_threads;
};

struct rocprofvis_meta_map_struct_t
{
    float       size;
    float       max;
    float       min;
    std::string type;
    std::string chart_name;
};
