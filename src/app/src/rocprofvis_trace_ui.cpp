// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_trace.h"
#include "ImGuiFileDialog.h"
#include "imgui.h"
#include "imgui_widget_flamegraph.h"
#include "implot.h"
#include "json.h"
#include "rocprofvis_main_view.h"
#include "rocprofvis_controller.h"
#include <fstream>
#include <future>
#include <iostream>

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

static void
rocprofvis_trace_event_flame_graph_getter(float* start, float* end, ImU8* level,
                                          const char** caption, const void* data, int idx)
{
    float       start_val = 0.f;
    float       end_val   = 0.f;
    ImU8        out_level = 0;
    char const* label     = "";
    if(data)
    {
        std::vector<rocprofvis_trace_event_t> const* thread_data =
            (std::vector<rocprofvis_trace_event_t> const*) data;
        if(thread_data->size() > idx)
        {
            // rocprofvis_trace_event_t const& first_event = thread_data->events[0];
            rocprofvis_trace_event_t const& event = (*thread_data)[idx];
            start_val = event.m_start_ts;                       // -first_event.start_ts;
            end_val   = (event.m_start_ts + event.m_duration);  // -first_event.start_ts;
            label     = event.m_name.c_str();
        }
    }
    if(start) *start = start_val;
    if(end) *end = end_val;
    if(level) *level = out_level;
    if(caption) *caption = label;
}

static ImPlotPoint
rocprofvis_trace_counter_plot_getter(int idx, void* user_data)
{
    ImPlotPoint point = ImPlotPoint(0, 0);
    if(user_data)
    {
        std::vector<rocprofvis_trace_counter_t>* thread_data =
            (std::vector<rocprofvis_trace_counter_t>*) user_data;
        if(thread_data->size() > idx)
        {
            rocprofvis_trace_counter_t const& counter = (*thread_data)[idx];
            point.x                                   = counter.m_start_ts;
            point.y                                   = counter.m_value;
        }
    }
    return point;
}

static rocprofvis_trace_data_t trace_object;
static rocprofvis_controller_future_t* trace_future = nullptr;
static rocprofvis_controller_t* trace_controller = nullptr;
static rocprofvis_controller_timeline_t* trace_timeline = nullptr;
static rocprofvis_controller_array_t* graph_data_array = nullptr;
static rocprofvis_controller_array_t* graph_futures = nullptr;

void
rocprofvis_trace_setup()
{
    ImPlot::CreateContext();

    trace_object.m_min_ts          = DBL_MAX;
    trace_object.m_max_ts          = 0.0;
    trace_object.m_is_trace_loaded = false;
}

static void
rocprofvis_trace_draw_view(RocProfVis::View::MainView* main)
{
    std::map<std::string, rocprofvis_trace_process_t>& trace_data =
        trace_object.m_trace_data;

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
    ImGui::SetNextWindowContentSize(
        ImVec2((trace_object.m_max_ts - trace_object.m_min_ts) / 1000.0, 0.f));
    ImGui::Begin("Trace", nullptr,
                 ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoTitleBar |
                     ImGuiWindowFlags_NoResize);

    if(ImGui::BeginMenuBar())
    {
        if(ImGui::BeginMenu("File"))
        {
            if(ImGui::MenuItem("Open", "CTRL+O"))
            {
                IGFD::FileDialogConfig config;
                config.path = ".";
                ImGuiFileDialog::Instance()->OpenDialog("ChooseFileDlgKey", "Choose File",
                                                        ".json", config);
            }
            ImGui::EndMenu();
        }
        if(ImGui::BeginMenu("Edit"))
        {
            if(ImGui::MenuItem("Undo", "CTRL+Z"))
            {
            }
            if(ImGui::MenuItem("Redo", "CTRL+Y", false, false))
            {
            }
            ImGui::Separator();
            if(ImGui::MenuItem("Cut", "CTRL+X"))
            {
            }
            if(ImGui::MenuItem("Copy", "CTRL+C"))
            {
            }
            if(ImGui::MenuItem("Paste", "CTRL+V"))
            {
            }
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    if(trace_object.m_is_trace_loaded)
    {
        // Open ImGui window......
        main->GenerateGraphPoints(trace_timeline, graph_data_array);
    }

    ImGui::End();
    ImGui::PopStyleVar(1);
}

void
rocprofvis_trace_draw(RocProfVis::View::MainView* main)
{
    rocprofvis_trace_draw_view(main);

    if(ImGuiFileDialog::Instance()->Display("ChooseFileDlgKey"))
    {
        trace_object.m_is_trace_loaded = false;
        if(ImGuiFileDialog::Instance()->IsOk())
        {
            std::string file_path = ImGuiFileDialog::Instance()->GetFilePathName();
            trace_controller = rocprofvis_controller_alloc();
            if (trace_controller)
            {
                rocprofvis_result_t result = kRocProfVisResultUnknownError;
                trace_future = rocprofvis_controller_future_alloc();
                if(trace_future)
                {
                    result = rocprofvis_controller_load_async(trace_controller, file_path.c_str(), trace_future);
                    assert(result == kRocProfVisResultSuccess);

                    if(result != kRocProfVisResultSuccess)
                    {
                        rocprofvis_controller_future_free(trace_future);
                        trace_future = nullptr;
                    }
                }
                if(result != kRocProfVisResultSuccess)
                {
                    rocprofvis_controller_free(trace_controller);
                    trace_controller = nullptr;
                }
            }
        }

        ImGuiFileDialog::Instance()->Close();
    }

    static bool is_open = false;
    if(trace_future || graph_futures)
    {
        if(trace_future)
        {
            rocprofvis_result_t result =
                rocprofvis_controller_future_wait(trace_future, 0);
            assert(result == kRocProfVisResultSuccess ||
                   result == kRocProfVisResultTimeout);
            if(result == kRocProfVisResultSuccess)
            {
                uint64_t uint64_result = 0;
                result                 = rocprofvis_controller_get_uint64(
                    trace_future, kRPVControllerFutureResult, 0, &uint64_result);
                assert(result == kRocProfVisResultSuccess &&
                       uint64_result == kRocProfVisResultSuccess);

                result = rocprofvis_controller_get_object(
                    trace_controller, kRPVControllerTimeline, 0, &trace_timeline);

                if(result == kRocProfVisResultSuccess && trace_timeline)
                {
                    uint64_t num_graphs = 0;
                    result              = rocprofvis_controller_get_uint64(
                        trace_timeline, kRPVControllerTimelineNumGraphs, 0, &num_graphs);

                    double min_ts = 0;
                    result        = rocprofvis_controller_get_double(
                        trace_timeline, kRPVControllerTimelineMinTimestamp, 0, &min_ts);

                    double max_ts = 0;
                    result        = rocprofvis_controller_get_double(
                        trace_timeline, kRPVControllerTimelineMaxTimestamp, 0, &max_ts);

                    graph_data_array = rocprofvis_controller_array_alloc(num_graphs);
                    graph_futures    = rocprofvis_controller_array_alloc(num_graphs);

                    for(uint32_t i = 0;
                        i < num_graphs && result == kRocProfVisResultSuccess; i++)
                    {
                        rocprofvis_controller_future_t* graph_future =
                            rocprofvis_controller_future_alloc();
                        rocprofvis_controller_array_t* graph_array =
                            rocprofvis_controller_array_alloc(32);
                        rocprofvis_handle_t* graph = nullptr;
                        result                     = rocprofvis_controller_get_object(
                            trace_timeline, kRPVControllerTimelineGraphIndexed, i,
                            &graph);
                        if(result == kRocProfVisResultSuccess && graph && graph_future &&
                           graph_array)
                        {
                            rocprofvis_handle_t* track = nullptr;
                            result                     = rocprofvis_controller_get_object(
                                graph, kRPVControllerGraphTrack, 0, &track);
                            if(result == kRocProfVisResultSuccess)
                            {
                                result = rocprofvis_controller_graph_fetch_async(
                                    trace_controller, graph, min_ts, max_ts, 1000,
                                    graph_future, graph_array);
                                if(result == kRocProfVisResultSuccess)
                                {
                                    result = rocprofvis_controller_set_object(
                                        graph_data_array, kRPVControllerArrayEntryIndexed,
                                        i, graph_array);
                                    assert(result == kRocProfVisResultSuccess);

                                    result = rocprofvis_controller_set_object(
                                        graph_futures, kRPVControllerArrayEntryIndexed, i,
                                        graph_future);
                                    assert(result == kRocProfVisResultSuccess);
                                }
                            }
                        }
                    }
                }

                rocprofvis_controller_future_free(trace_future);
                trace_future = nullptr;
            }
        }
        else if(graph_futures)
        {
            uint64_t            num_tracks = 0;
            rocprofvis_result_t result     = rocprofvis_controller_get_uint64(
                graph_futures, kRPVControllerArrayNumEntries, 0, &num_tracks);
            assert(result == kRocProfVisResultSuccess);

            for(uint32_t i = 0; i < num_tracks && result == kRocProfVisResultSuccess; i++)
            {
                rocprofvis_handle_t* future = nullptr;
                result                      = rocprofvis_controller_get_object(
                    graph_futures, kRPVControllerArrayEntryIndexed, i, &future);
                assert(result == kRocProfVisResultSuccess && future);

                rocprofvis_result_t result = rocprofvis_controller_future_wait(
                    (rocprofvis_controller_future_t*) future, 0);
                assert(result == kRocProfVisResultSuccess ||
                       result == kRocProfVisResultTimeout);

                if(result != kRocProfVisResultSuccess)
                {
                    break;
                }
            }

            if(result != kRocProfVisResultTimeout)
            {
                for(uint32_t i = 0; i < num_tracks && result == kRocProfVisResultSuccess;
                    i++)
                {
                    rocprofvis_handle_t* future = nullptr;
                    result                      = rocprofvis_controller_get_object(
                        graph_futures, kRPVControllerArrayEntryIndexed, i, &future);
                    assert(result == kRocProfVisResultSuccess && future);

                    rocprofvis_controller_future_free(
                        (rocprofvis_controller_future_t*) future);

                    if(result != kRocProfVisResultSuccess)
                    {
                        rocprofvis_handle_t* array = nullptr;
                        result                     = rocprofvis_controller_get_object(
                            graph_data_array, kRPVControllerArrayEntryIndexed, i,
                            &future);
                        assert(result == kRocProfVisResultSuccess && array);

                        rocprofvis_controller_array_free(
                            (rocprofvis_controller_array_t*) array);
                    }
                }

                rocprofvis_controller_array_free(graph_futures);
                graph_futures = nullptr;

                if(result != kRocProfVisResultSuccess)
                {
                    rocprofvis_controller_array_free(graph_data_array);
                    graph_data_array = nullptr;
                }
            }

            if(result == kRocProfVisResultSuccess)
            {
                trace_object.m_is_trace_loaded = true;
                is_open                        = false;
                ImGui::CloseCurrentPopup();
            }
        }

        if(!trace_object.m_is_trace_loaded)
        {
            if(ImGui::BeginPopupModal("Loading"))
            {
                ImGui::EndPopup();
            }

            if(!is_open)
            {
                ImGui::OpenPopup("Loading");
                is_open = true;
            }
        }
    }
}


