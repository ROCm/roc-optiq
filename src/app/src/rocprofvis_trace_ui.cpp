// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_trace.h"

#include "json.h"
#include <iostream>
#include <fstream>
#include <future>

#include "imgui.h"
#include "imgui_widget_flamegraph.h"
#include "implot.h"
#include "ImGuiFileDialog.h"

static void rocprofvis_trace_event_flame_graph_getter(float* start, float* end, ImU8* level, const char** caption, const void* data, int idx)
{
    float start_val = 0.f;
    float end_val = 0.f;
    ImU8 out_level = 0;
    char const* label = "";
    if (data)
    {
        std::vector<rocprofvis_trace_event_t> const* thread_data = (std::vector<rocprofvis_trace_event_t> const*)data;
        if (thread_data->size() > idx)
        {
            //rocprofvis_trace_event_t const& first_event = thread_data->events[0];
            rocprofvis_trace_event_t const& event = (*thread_data)[idx];
            start_val = event.m_start_ts;// -first_event.start_ts;
            end_val = (event.m_start_ts + event.m_duration);// -first_event.start_ts;
            label = event.m_name.c_str();
        }
    }
    if (start)
        *start = start_val;
    if (end)
        *end = end_val;
    if (level)
        *level = out_level;
    if (caption)
        *caption = label;
}

static ImPlotPoint rocprofvis_trace_counter_plot_getter(int idx, void* user_data)
{
    ImPlotPoint point = ImPlotPoint(0, 0);
    if (user_data)
    {
        std::vector<rocprofvis_trace_counter_t>* thread_data = (std::vector<rocprofvis_trace_counter_t>*)user_data;
        if (thread_data->size() > idx)
        {
            rocprofvis_trace_counter_t const& counter = (*thread_data)[idx];
            point.x = counter.m_start_ts;
            point.y = counter.m_value;
        }
    }
    return point;
}

static rocprofvis_trace_data_t trace_object;

void rocprofvis_trace_setup()
{
    ImPlot::CreateContext();

    trace_object.m_min_ts = DBL_MAX;
    trace_object.m_max_ts = 0.0;
    trace_object.m_is_trace_loaded = false;
}

static void rocprofvis_trace_draw_view()
{
    std::map<std::string, rocprofvis_trace_process_t>& trace_data = trace_object.m_trace_data;

#ifdef IMGUI_HAS_VIEWPORT
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->GetWorkPos());
    ImGui::SetNextWindowSize(viewport->GetWorkSize());
    ImGui::SetNextWindowViewport(viewport->ID);
#else 
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
#endif
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::SetNextWindowContentSize(ImVec2((trace_object.m_max_ts - trace_object.m_min_ts) / 1000.0, 0.f));
    ImGui::Begin("Trace", nullptr, ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize);

    if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("Open", "CTRL+O"))
            {
                IGFD::FileDialogConfig config;
                config.path = ".";
                ImGuiFileDialog::Instance()->OpenDialog("ChooseFileDlgKey", "Choose File", ".json", config);
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Edit"))
        {
            if (ImGui::MenuItem("Undo", "CTRL+Z")) {}
            if (ImGui::MenuItem("Redo", "CTRL+Y", false, false)) {}
            ImGui::Separator();
            if (ImGui::MenuItem("Cut", "CTRL+X")) {}
            if (ImGui::MenuItem("Copy", "CTRL+C")) {}
            if (ImGui::MenuItem("Paste", "CTRL+V")) {}
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    if (trace_object.m_is_trace_loaded)
    {
        auto& IO = ImGui::GetIO();
        float mouse_wheel = IO.MouseWheel;
        static float zoom_amount = 0.f;
        zoom_amount += mouse_wheel;

        double zoom_scale = 1000.0;
        if (zoom_amount > 0.f)
        {
            zoom_scale = 1000.0 * (1.0 + zoom_amount);
        }
        else if (zoom_amount < 0.f)
        {
            zoom_scale = 1000.0 / (1.0 + fabs(zoom_amount));
        }

        for (auto& process : trace_data)
        {
            for (auto& thread : process.second.m_threads)
            {
                auto& events = thread.second.m_events;
                auto& counters = thread.second.m_counters;
                if (events.size())
                {
                    const char* label = "##ThreadFrameGraph";
                    const void* data = (const void*)&thread.second.m_events;
                    int values_count = events.size();
                    int values_offset = 0;
                    const char* overlay_text = "";
                    float scale_min = FLT_MAX;
                    float scale_max = FLT_MAX;
                    ImVec2 graph_size = ImVec2((trace_object.m_max_ts - trace_object.m_min_ts) / zoom_scale, 100);

                    if (values_count > graph_size.x)
                    {
                        if (!thread.second.m_has_events_l1)
                        {
                            rocprofvis_trace_event_t new_event;
                            bool is_first = true;
                            for (auto event : thread.second.m_events)
                            {
                                double Gap = (event.m_start_ts - (new_event.m_duration + new_event.m_start_ts));
                                double duration = ((event.m_start_ts + event.m_duration) - new_event.m_start_ts);
                                if (!is_first && Gap < 1000.0 && duration < 1000.0)
                                {
                                    new_event.m_name.clear();
                                    new_event.m_duration = duration;
                                }
                                else
                                {
                                    if (!is_first)
                                        thread.second.m_events_l1.push_back(new_event);

                                    new_event = event;
                                    is_first = false;
                                }
                            }
                            thread.second.m_events_l1.push_back(new_event);
                            thread.second.m_has_events_l1 = true;
                        }
                        if (values_count > thread.second.m_events_l1.size())
                        {
                            values_count = thread.second.m_events_l1.size();
                            data = (const void*)&thread.second.m_events_l1;
                        }
                    }

                    ImGui::LabelText("##FlameGraphLabel", "%s (%s) : %s (%s)", process.second.m_name.c_str(), process.first.c_str(), thread.second.m_name.c_str(), thread.first.c_str());
                    ImGui::SameLine();
                    ImGuiWidgetFlameGraph::PlotFlame(label, &rocprofvis_trace_event_flame_graph_getter, data, values_count, values_offset, overlay_text, scale_min, scale_max, graph_size);
                }
                else if (counters.size())
                {
                    ImGui::LabelText("##PlotLabel", "%s (%s) : %s (%s)", process.second.m_name.c_str(), process.first.c_str(), thread.second.m_name.c_str(), thread.first.c_str());
                    ImGui::SameLine();

                    void* data = (void*)&thread.second.m_counters;
                    int count = counters.size();
                    ImVec2 graph_size = ImVec2((trace_object.m_max_ts - trace_object.m_min_ts) / zoom_scale, 300);
                    if (counters.size() > graph_size.x)
                    {
                        if (!thread.second.m_has_counters_l1)
                        {
                            rocprofvis_trace_counter_t new_counter;
                            bool is_first = true;
                            for (auto counter : thread.second.m_counters)
                            {
                                double Gap = !is_first ? (counter.m_start_ts - new_counter.m_start_ts) : 0.0;
                                if (!is_first && Gap < 1000.0)
                                {
                                    counter.m_value = std::min(counter.m_value, new_counter.m_value);
                                }
                                else
                                {
                                    if (!is_first)
                                        thread.second.m_counters_l1.push_back(new_counter);

                                    new_counter = counter;
                                    is_first = false;
                                }
                            }
                            thread.second.m_counters_l1.push_back(new_counter);
                            thread.second.m_has_counters_l1 = true;
                        }
                        if (count > thread.second.m_counters_l1.size())
                        {
                            count = thread.second.m_counters_l1.size();
                            data = (void*)&thread.second.m_counters_l1;
                        }
                    }

                    if (ImPlot::BeginPlot("##PlotCounters", graph_size))
                    {
                        const char* label_id = "##ThreadCounters";
                        ImPlotLineFlags flags = ImPlotLineFlags_Shaded;
                        ImPlot::SetupAxes("x", "y");
                        ImPlot::PlotLineG(label_id, &rocprofvis_trace_counter_plot_getter, data, count, flags);

                        ImPlot::EndPlot();
                    }
                }
            }
        }
    }

    ImGui::End();
    ImGui::PopStyleVar(1);
}

void rocprofvis_trace_draw()
{
    rocprofvis_trace_draw_view();

    if (ImGuiFileDialog::Instance()->Display("ChooseFileDlgKey")) {
        trace_object.m_is_trace_loaded = false;
        if (ImGuiFileDialog::Instance()->IsOk()) {
            std::string file_path = ImGuiFileDialog::Instance()->GetFilePathName();
            trace_object.m_loading_future = rocprofvis_trace_async_load_json_trace(file_path, trace_object);
        }

        ImGuiFileDialog::Instance()->Close();
    }

    if (rocprofvis_trace_is_loading(trace_object.m_loading_future))
    {
        static bool is_open = false;
        std::chrono::milliseconds timeout = std::chrono::milliseconds::min();
        if (rocprofvis_trace_is_loaded(trace_object.m_loading_future))
        {
            trace_object.m_is_trace_loaded = trace_object.m_loading_future.get();
            is_open = false;
            ImGui::CloseCurrentPopup();
        }
        else
        {
            if (ImGui::BeginPopupModal("Loading"))
            {
                ImGui::EndPopup();
            }

            if (!is_open)
            {
                ImGui::OpenPopup("Loading");
            }
        }
    }
}
