// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_trace.h"

#include "json.h"
#include <iostream>
#include <fstream>
#include <future>
#include "main_view.h"
#include "imgui.h"
#include "imgui_widget_flamegraph.h"
#include "implot.h"
#include "ImGuiFileDialog.h"

 void
DisableScrollWheelInImGui()
{
    ImGuiIO& io    = ImGui::GetIO();
    io.MouseWheel  = 0.0f;
    io.MouseWheelH = 0.0f;
}

struct Point
{
    float x;
    float y;
};

 
std::vector<Point>
extractPointsFromData(void* data)
{
    // Cast the void* pointer back to the original type
    auto* counters_vector = static_cast<std::vector<rocprofvis_trace_counter_t>*>(data);

    std::vector<Point> points;
    for(const auto& counter : *counters_vector)
    {
        Point point;
        point.x = counter.m_start_ts;
        point.y = counter.m_value;
        points.push_back(point);
    }
    return points;
}


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

static void rocprofvis_trace_draw_view(main_view* main)
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
      
           
       
        // Open ImGui window
           main->generate_graph_points(trace_data);
 
    }

    ImGui::End();
    ImGui::PopStyleVar(1);
}
 
 
void rocprofvis_trace_draw(main_view* main)
{
    rocprofvis_trace_draw_view(main);

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
