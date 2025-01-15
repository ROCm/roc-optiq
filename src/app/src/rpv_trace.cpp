// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rpv_trace.h"

#include "json.h"
#include <iostream>
#include <fstream>

std::future<bool> rpv_trace_async_load_json_trace(std::string file_path, rpvTraceData& trace_object)
{
    trace_object.min_ts = DBL_MAX;
    trace_object.max_ts = 0.0;
    std::map<std::string, rpvTraceProcess>& trace_data = trace_object.trace_data;

    std::future<bool> future = std::async(std::launch::async, [file_path, &trace_data, &trace_object]() -> bool
    {
        bool is_ok = false;
        std::fstream f(file_path, std::fstream::in);
        std::string s;
        std::getline(f, s, '\0');
        f.close();

        auto result = jt::Json::parse(s);
        if (result.first == jt::Json::Status::success)
        {
            jt::Json& trace = result.second;
            jt::Json& trace_events = trace["trace_events"];
            auto& trace_array = trace_events.getArray();
            for (auto& trace_event : trace_array)
            {
                if (trace_event.isObject())
                {
                    auto& object_map = trace_event.getObject();
                    jt::Json& type = object_map["ph"];
                    if (type.isString())
                    {
                        auto& type_string = type.getString();
                        if (type_string == "M")
                        {
                            auto& pid = object_map["pid"].getString();
                            if (object_map.find("tid") == object_map.end())
                            {
                                auto& args = object_map["args"];
                                if (args.isObject())
                                {
                                    auto& args_map = args.getObject();
                                    if (args_map.find("name") != args_map.end())
                                    {
                                        auto& actual_name = args_map["name"];
                                        if (actual_name.isString())
                                        {
                                            auto& actual_name_str = actual_name.getString();
                                            rpvTraceProcess process;
                                            process.name = actual_name_str;
                                            trace_data[pid] = process;
                                        }
                                    }
                                }
                            }
                            else
                            {
                                auto& tid = object_map["tid"].getString();
                                auto& args = object_map["args"];
                                if (args.isObject())
                                {
                                    auto& args_map = args.getObject();
                                    if (args_map.find("name") != args_map.end())
                                    {
                                        auto& actual_name = args_map["name"];
                                        if (actual_name.isString())
                                        {
                                            auto& actual_name_str = actual_name.getString();

                                            if (trace_data.find(pid) == trace_data.end())
                                            {
                                                rpvTraceProcess process;
                                                process.name = pid;
                                                trace_data[pid] = process;
                                            }
                                            {
                                                rpvTraceProcess& process = trace_data[pid];
                                                rpvTraceThread thread;
                                                thread.name = actual_name_str;
                                                process.threads[tid] = thread;
                                            }
                                        }
                                    }
                                }
                            }
                        }
                        else if (type_string == "X")
                        {
                            auto& name = object_map["name"].getString();
                            auto& pid = object_map["pid"].getString();
                            auto& tid = object_map["tid"].getString();
                            auto& ts = object_map["ts"].getString();
                            auto& duration = object_map["dur"].getString();
                            if (trace_data.find(pid) != trace_data.end())
                            {
                                rpvTraceProcess& process = trace_data[pid];
                                if (process.threads.find(tid) == process.threads.end())
                                {
                                    rpvTraceThread thread;
                                    thread.name = tid;
                                    process.threads[tid] = thread;
                                }
                                {
                                    rpvTraceThread& thread = process.threads[tid];
                                    rpvTraceEvent event;
                                    event.name = name;
                                    event.start_ts = std::stod(ts);
                                    event.duration = std::stod(duration);
                                    trace_object.min_ts = std::min(trace_object.min_ts, event.start_ts);
                                    trace_object.max_ts = std::max(trace_object.max_ts, event.start_ts + event.duration);
                                    thread.events.push_back(event);
                                }
                            }
                        }
                        else if (type_string == "C")
                        {
                            auto& name = object_map["name"].getString();
                            auto& pid = object_map["pid"].getString();
                            if (trace_data.find(pid) != trace_data.end())
                            {
                                rpvTraceProcess& process = trace_data[pid];
                                if (process.threads.find(name) == process.threads.end())
                                {
                                    rpvTraceThread thread;
                                    thread.name = name;
                                    process.threads[name] = thread;
                                }
                                {
                                    rpvTraceThread& thread = process.threads[name];
                                    rpvTraceCounter counter;
                                    if (object_map["ts"].isString())
                                    {
                                        auto& ts = object_map["ts"].getString();
                                        counter.start_ts = std::stod(ts);
                                    }
                                    else
                                    {
                                        counter.start_ts = object_map["ts"].getDouble();
                                    }

                                    auto& args = object_map["args"];
                                    if (args.isObject())
                                    {
                                        auto& args_map = args.getObject();
                                        if (args_map.size())
                                        {
                                            auto it = args_map.begin();
                                            auto& value = it->second;
                                            if (value.isString())
                                            {
                                                auto& value_str = value.getString();

                                                counter.value = std::stod(value_str);
                                                thread.counters.push_back(counter);
                                                trace_object.min_ts = std::min(trace_object.min_ts, counter.start_ts);
                                                trace_object.max_ts = std::max(trace_object.max_ts, counter.value);
                                            }
                                            else if (value.isLong())
                                            {
                                                counter.value = value.getLong();
                                                thread.counters.push_back(counter);
                                                trace_object.min_ts = std::min(trace_object.min_ts, counter.start_ts);
                                                trace_object.max_ts = std::max(trace_object.max_ts, counter.value);
                                            }
                                            else if (value.isDouble())
                                            {
                                                counter.value = value.getDouble();
                                                thread.counters.push_back(counter);
                                                trace_object.min_ts = std::min(trace_object.min_ts, counter.start_ts);
                                                trace_object.max_ts = std::max(trace_object.max_ts, counter.value);
                                            }
                                        }
                                    }
                                }
                            }
                        }
                        else if (type_string == "f")
                        {

                        }
                        else if (type_string == "s")
                        {

                        }
                    }
                }
            }

            is_ok = true;
        }
        return is_ok;
    });

    return future;
}

bool rpv_trace_is_loading(std::future<bool>& future)
{
    return future.valid();
}

bool rpv_trace_is_loaded(std::future<bool>& future)
{
    std::chrono::milliseconds timeout = std::chrono::milliseconds::min();
    return (future.wait_for(timeout) == std::future_status::ready);
}
