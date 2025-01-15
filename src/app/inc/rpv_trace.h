// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include <string>
#include <map>
#include <vector>
#include <future>

typedef struct rpvTraceEvent
{
    std::string name;
    double start_ts;
    double duration;
} rpvTraceEvent;

typedef struct rpvTraceCounter
{
    double start_ts;
    double value;
} rpvTraceCounter;

typedef struct rpvTraceThread
{
    std::string name;
    std::vector<rpvTraceEvent> events;
    std::vector<rpvTraceEvent> events_l1;
    std::vector<rpvTraceCounter> counters;
    std::vector<rpvTraceCounter> counters_l1;
    bool has_events_l1 = false;
    bool has_counters_l1 = false;
} rpvTraceThread;

struct rpvTraceProcess
{
    std::string name;
    std::map<std::string, rpvTraceThread> threads;
};

struct rpvTraceData
{
    double min_ts;
    double max_ts;
    std::map<std::string, rpvTraceProcess> trace_data;
    bool is_trace_loaded = false;
    std::future<bool> loading_future;
};

std::future<bool> rpv_trace_async_load_json_trace(std::string file_path, rpvTraceData& trace_object);
bool rpv_trace_is_loading(std::future<bool>& future);
bool rpv_trace_is_loaded(std::future<bool>& future);
void rpv_trace_setup();
void rpv_trace_draw();
