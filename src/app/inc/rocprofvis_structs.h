//  Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once
#include "imgui.h"
#include <future>
#include <map>
#include <string>
#include <vector>


typedef struct rocprofvis_trace_event_t
{
    std::string m_name;
    double      m_start_ts;
    double      m_duration;
} rocprofvis_trace_event_t;

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
