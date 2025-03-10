// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_trace.h"
#include "json.h"
#include <fstream>
#include <iostream>

std::future<bool>
rocprofvis_trace_async_load_json_trace(std::string              file_path,
                                       rocprofvis_trace_data_t& trace_object)
{
    trace_object.m_min_ts = DBL_MAX;
    trace_object.m_max_ts = 0.0;
    std::map<std::string, rocprofvis_trace_process_t>& trace_data =
        trace_object.m_trace_data;

    std::future<bool> future =
        std::async(std::launch::async, [file_path, &trace_data, &trace_object]() -> bool {
            bool         is_ok = false;
            std::fstream f(file_path, std::fstream::in);
            std::string  s;
            std::getline(f, s, '\0');
            f.close();

            auto result = jt::Json::parse(s);
            if(result.first == jt::Json::Status::success)
            {
                jt::Json& trace        = result.second;
                jt::Json& trace_events = trace["traceEvents"];
                auto&     trace_array  = trace_events.getArray();
                for(auto& trace_event : trace_array)
                {
                    if(trace_event.isObject())
                    {
                        auto&     object_map = trace_event.getObject();
                        jt::Json& type       = object_map["ph"];
                        if(type.isString())
                        {
                            auto& type_string = type.getString();
                            if(type_string == "M")
                            {
                                auto& pid = object_map["pid"].getString();
                                if(object_map.find("tid") == object_map.end())
                                {
                                    auto& args = object_map["args"];
                                    if(args.isObject())
                                    {
                                        auto& args_map = args.getObject();
                                        if(args_map.find("name") != args_map.end())
                                        {
                                            auto& actual_name = args_map["name"];
                                            if(actual_name.isString())
                                            {
                                                auto& actual_name_str =
                                                    actual_name.getString();
                                                rocprofvis_trace_process_t process;
                                                process.m_name  = actual_name_str;
                                                trace_data[pid] = process;
                                            }
                                        }
                                    }
                                }
                                else
                                {
                                    auto& tid  = object_map["tid"].getString();
                                    auto& args = object_map["args"];
                                    if(args.isObject())
                                    {
                                        auto& args_map = args.getObject();
                                        if(args_map.find("name") != args_map.end())
                                        {
                                            auto& actual_name = args_map["name"];
                                            if(actual_name.isString())
                                            {
                                                auto& actual_name_str =
                                                    actual_name.getString();

                                                if(trace_data.find(pid) ==
                                                   trace_data.end())
                                                {
                                                    rocprofvis_trace_process_t process;
                                                    process.m_name  = pid;
                                                    trace_data[pid] = process;
                                                }
                                                {
                                                    rocprofvis_trace_process_t& process =
                                                        trace_data[pid];
                                                    rocprofvis_trace_thread_t thread;
                                                    thread.m_name = actual_name_str;
                                                    process.m_threads[tid] = thread;
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                            else if(type_string == "X")
                            {
                                auto& name     = object_map["name"].getString();
                                auto& pid      = object_map["pid"].getString();
                                auto& tid      = object_map["tid"].getString();
                                auto& ts       = object_map["ts"].getString();
                                auto& duration = object_map["dur"].getString();
                                if(trace_data.find(pid) != trace_data.end())
                                {
                                    rocprofvis_trace_process_t& process = trace_data[pid];
                                    if(process.m_threads.find(tid) ==
                                       process.m_threads.end())
                                    {
                                        rocprofvis_trace_thread_t thread;
                                        thread.m_name          = tid;
                                        process.m_threads[tid] = thread;
                                    }
                                    {
                                        rocprofvis_trace_thread_t& thread =
                                            process.m_threads[tid];
                                        rocprofvis_trace_event_t event;
                                        event.m_name          = name;
                                        event.m_start_ts      = std::stod(ts);
                                        event.m_duration      = std::stod(duration);
                                        trace_object.m_min_ts = std::min(
                                            trace_object.m_min_ts, event.m_start_ts);
                                        trace_object.m_max_ts =
                                            std::max(trace_object.m_max_ts,
                                                     event.m_start_ts + event.m_duration);
                                        thread.m_events.push_back(event);
                                    }
                                }
                            }
                            else if(type_string == "C")
                            {
                                auto& name = object_map["name"].getString();
                                auto& pid  = object_map["pid"].getString();
                                if(trace_data.find(pid) != trace_data.end())
                                {
                                    rocprofvis_trace_process_t& process = trace_data[pid];
                                    if(process.m_threads.find(name) ==
                                       process.m_threads.end())
                                    {
                                        rocprofvis_trace_thread_t thread;
                                        thread.m_name           = name;
                                        process.m_threads[name] = thread;
                                    }
                                    {
                                        rocprofvis_trace_thread_t& thread =
                                            process.m_threads[name];
                                        rocprofvis_trace_counter_t counter;
                                        if(object_map["ts"].isString())
                                        {
                                            auto& ts = object_map["ts"].getString();
                                            counter.m_start_ts = std::stod(ts);
                                        }
                                        else
                                        {
                                            counter.m_start_ts =
                                                object_map["ts"].getDouble();
                                        }

                                        auto& args = object_map["args"];
                                        if(args.isObject())
                                        {
                                            auto& args_map = args.getObject();
                                            if(args_map.size())
                                            {
                                                auto  it    = args_map.begin();
                                                auto& value = it->second;
                                                if(value.isString())
                                                {
                                                    auto& value_str = value.getString();

                                                    counter.m_value =
                                                        std::stod(value_str);
                                                    thread.m_counters.push_back(counter);
                                                    trace_object.m_min_ts =
                                                        std::min(trace_object.m_min_ts,
                                                                 counter.m_start_ts);
                                                    trace_object.m_max_ts =
                                                        std::max(trace_object.m_max_ts,
                                                                 counter.m_value);
                                                }
                                                else if(value.isLong())
                                                {
                                                    counter.m_value = value.getLong();
                                                    thread.m_counters.push_back(counter);
                                                    trace_object.m_min_ts =
                                                        std::min(trace_object.m_min_ts,
                                                                 counter.m_start_ts);
                                                    trace_object.m_max_ts =
                                                        std::max(trace_object.m_max_ts,
                                                                 counter.m_value);
                                                }
                                                else if(value.isDouble())
                                                {
                                                    counter.m_value = value.getDouble();
                                                    thread.m_counters.push_back(counter);
                                                    trace_object.m_min_ts =
                                                        std::min(trace_object.m_min_ts,
                                                                 counter.m_start_ts);
                                                    trace_object.m_max_ts =
                                                        std::max(trace_object.m_max_ts,
                                                                 counter.m_value);
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                            else if(type_string == "f")
                            {
                            }
                            else if(type_string == "s")
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

bool
rocprofvis_trace_is_loading(std::future<bool>& future)
{
    return future.valid();
}

bool
rocprofvis_trace_is_loaded(std::future<bool>& future)
{
    std::chrono::milliseconds timeout = std::chrono::milliseconds::min();
    return (future.wait_for(timeout) == std::future_status::ready);
}
