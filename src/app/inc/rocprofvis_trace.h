// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include <string>
#include <map>
#include <vector>
#include <future>

typedef struct rocprofvis_trace_event_t
{
    std::string m_name;
    double m_start_ts;
    double m_duration;
} rocprofvis_trace_event_t;

typedef struct rocprofvis_trace_counter_t
{
    double m_start_ts;
    double m_value;
} rocprofvis_trace_counter_t;

typedef struct rocprofvis_trace_thread_t
{
    std::string m_name;
    std::vector<rocprofvis_trace_event_t> m_events;
    std::vector<rocprofvis_trace_event_t> m_events_l1;
    std::vector<rocprofvis_trace_counter_t> m_counters;
    std::vector<rocprofvis_trace_counter_t> m_counters_l1;
    bool m_has_events_l1 = false;
    bool m_has_counters_l1 = false;
} rocprofvis_trace_thread_t;

struct rocprofvis_trace_process_t
{
    std::string m_name;
    std::map<std::string, rocprofvis_trace_thread_t> m_threads;
};

struct rocprofvis_trace_data_t
{
    double m_min_ts;
    double m_max_ts;
    std::map<std::string, rocprofvis_trace_process_t> m_trace_data;
    bool m_is_trace_loaded = false;
    std::future<bool> m_loading_future;
};

std::future<bool> rocprofvis_trace_async_load_json_trace(std::string file_path, rocprofvis_trace_data_t& trace_object);
bool rocprofvis_trace_is_loading(std::future<bool>& future);
bool rocprofvis_trace_is_loaded(std::future<bool>& future);
void rocprofvis_trace_setup();
void rocprofvis_trace_draw();
