// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_main_view.h"
#include "imgui.h"
#include "rocprofvis_flame_chart.h"
#include "rocprofvis_grid.h"
#include "rocprofvis_line_chart.h"
#include "rocprofvis_structs.h"
#include <iostream>
#include <map>
#include <string>
#include <tuple>
#include <vector>

namespace RocProfVis
{
namespace View
{

MainView::MainView()
: m_min_value(0.0f)
, m_max_value(0.0f)
, m_zoom(1.0f)
, m_movement(0.0f)
, m_min_x(0.0f)
, m_max_x(0.0f)
, m_min_y(0.0f)
, m_max_y(0.0f)
, m_scroll_position(0.0f)
 , m_v_min_x()
, m_v_max_x()
, m_scale_x()
, m_data_arr()
, m_sidebar_size(500.f)
, m_ran_once(false)
, m_meta_map_made(false)
, m_meta_map({})
, m_user_adjusting_graph_height(false)
, m_flame_event({})
, m_previous_scroll_position(0)
, m_show_graph_customization_window(false)
, m_graph_map({})
, min_max_x_init(false)
, m_is_control_held(false)
, m_trace_data_ptr(nullptr)
{}

MainView::~MainView() {}

void
MainView::Render()
{
    if(m_trace_data_ptr)
    {
        std::map<std::string, rocprofvis_trace_process_t>& trace_data = *m_trace_data_ptr;
        RenderGraphPoints(trace_data);
    }
}

void
MainView::RenderScrubber(ImVec2 display_size_main_graphs, ImVec2 screen_pos)
{
    ImVec2 windowPos  = ImGui::GetWindowPos();
    ImVec2 windowSize = ImGui::GetWindowSize();

    // Scrubber Line
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                                    ImGuiWindowFlags_NoScrollWithMouse;

    ImVec2 display_size = ImGui::GetWindowSize();
    ImGui::SetNextWindowSize(ImVec2(display_size.x - 20.0f, display_size.y),
                             ImGuiCond_Always);

    ImGui::SetCursorPos(ImVec2(400, 0));

    //overlayed windows need to have fully trasparent bg otherwise they will overlay (with no alpha) over their predecessors
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0,0,0,0));  
    ImGui::BeginChild("Scrubber View", ImVec2(0, 0), ImGuiChildFlags_None, window_flags);
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    if(ImGui::IsMouseHoveringRect(
           ImVec2(m_sidebar_size, 0),
           ImVec2(display_size_main_graphs.x, display_size_main_graphs.y)))
    {
        ImVec2 mPos = ImGui::GetMousePos();
        draw_list->AddLine(ImVec2(mPos.x, screen_pos.y),
                           ImVec2(mPos.x, screen_pos.y + display_size_main_graphs.y),
                           IM_COL32(0, 0, 0, 255), 2.0f);
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

std::map<int, rocprofvis_graph_map_t>*
MainView::GetGraphMap()
{
    return &m_graph_map;
}

void
MainView::RenderGrid()
{
    /*This section makes the grid for the charts*/

    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                                    ImGuiWindowFlags_NoScrollWithMouse;

    ImVec2      display_size = ImGui::GetWindowSize();

    ImGui::SetNextWindowSize(ImVec2(display_size.x, display_size.y), ImGuiCond_Always);

    ImGui::SetCursorPos(ImVec2(0, 0));
    //overlayed windows need to have fully trasparent bg otherwise they will overlay (with no alpha) over their predecessors
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(220,0,0,0));  

     ImDrawList* draw_list    = ImGui::GetWindowDrawList();
    RocProfVis::View::Grid main_grid = RocProfVis::View::Grid();

    main_grid.RenderGrid(m_min_x, m_max_x, m_movement, m_zoom, draw_list, m_scale_x,
                         m_v_max_x, m_v_min_x);

     ImGui::PopStyleColor();
}

void
MainView::RenderGraphCustomizationWindow(int graph_id)
{
    if(ImGui::Begin((std::to_string(graph_id)).c_str(), nullptr,
                    ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("This is a popout window!");
        if(ImGui::Button((std::to_string(graph_id)).c_str()))
        {
            m_show_graph_customization_window = false;
        }
        ImGui::End();
    }
}

void
MainView::MakeGraphMetadataView()
{
    /*This section makes the charts both line and flamechart are constructed here*/

    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                                    ImGuiWindowFlags_NoScrollWithMouse |
                                    ImGuiWindowFlags_NoScrollbar;

    ImVec2 display_size = ImGui::GetWindowSize();

    ImGui::SetNextWindowSize(ImVec2(display_size.x, display_size.y - 60.0f),
                             ImGuiCond_Always);
    ImGui::SetCursorPos(ImVec2(0, 0));

    ImGui::BeginChild("Graph MetaData View", ImVec2(0, 0), false, window_flags);

    if(m_scroll_position != ImGui::GetScrollY())
    {
        ImGui::SetScrollY(m_scroll_position);
    }

    for(const auto& pair : m_meta_map)
    {
        if(pair.second.type == "flame")
        {
            RenderGraphMetadata(pair.first, 250, "Flame", pair.second);
        }

        else if(pair.second.type == "line")
        {
            RenderGraphMetadata(pair.first, 300, "Line", pair.second);
        }
    }

    ImGui::EndChild();
}

void
MainView::MakeGraphView(rocprofvis_controller_timeline_t*                  timeline,
                        rocprofvis_controller_array_t*                     array,
                        float                                              scale_x)
{
    /*This section makes the charts both line and flamechart are constructed here*/

    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                                    ImGuiWindowFlags_NoScrollWithMouse;

    ImVec2 display_size = ImGui::GetWindowSize();

    ImGui::SetNextWindowSize(ImVec2(display_size.x, display_size.y - 60.0f),
                             ImGuiCond_Always);
    ImGui::SetCursorPos(ImVec2(0, 0));

    //overlayed windows need to have fully trasparent bg otherwise they will overlay (with no alpha) over their predecessors
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0,0,0,0));      
    ImGui::BeginChild("Graph View Main", ImVec2(0, 0), false, window_flags);
    ImGuiIO& io       = ImGui::GetIO();
    m_is_control_held = io.KeyCtrl;

    // Prevent choppy behavior by preventing constant rerender.
    float temp_scroll_position = ImGui::GetScrollY();
    if(m_previous_scroll_position != temp_scroll_position)
    {
        m_previous_scroll_position = temp_scroll_position;
        m_scroll_position          = temp_scroll_position;
    }
    else if(m_scroll_position != temp_scroll_position)
    {
        ImGui::SetScrollY(m_scroll_position);
    }
    for(const auto& graph_objects : m_graph_map)
    {
        if(graph_objects.second.display == true)
        {
            if(graph_objects.second.color_by_value)
            {
                graph_objects.second.chart->SetColorByValue(
                    graph_objects.second.color_by_value_digits);
            }

            ImGui::PushStyleColor(ImGuiCol_ChildBg, graph_objects.second.selected);
            ImGui::BeginChild(
                (std::to_string(graph_objects.first)).c_str(),
                ImVec2(0, m_graph_map[graph_objects.first].chart->ReturnSize() + 40.0f),
                false,
                ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                    ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoScrollbar);

            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));

            if(m_is_control_held)
            {
                ImGui::Selectable(
                    ("Move Position " + std::to_string(graph_objects.first)).c_str(),
                    false, ImGuiSelectableFlags_AllowDoubleClick, ImVec2(0, 20.0f));

                if(ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
                {
                    ImGui::SetDragDropPayload("MY_PAYLOAD_TYPE", &graph_objects.second,
                                              sizeof(graph_objects.second));
                    ImGui::EndDragDropSource();
                }
                if(ImGui::BeginDragDropTarget())
                {
                    if(const ImGuiPayload* payload =
                           ImGui::AcceptDragDropPayload("MY_PAYLOAD_TYPE"))
                    {
                        // Handle the payload (here we just print it)

                        rocprofvis_graph_map_t* payload_data =
                            (rocprofvis_graph_map_t*)
                                payload->Data;  // incoming (being dragged)
                        rocprofvis_graph_map_t payload_data_copy = *payload_data;
                        int payload_position = payload_data_copy.chart->ReturnChartID();
                        rocprofvis_graph_map_t outgoing_chart =
                            m_graph_map[graph_objects.first];  // outgoing (getting
                                                               // replaced)
                        int outgoing_position = outgoing_chart.chart->ReturnChartID();

                        // Change position in object itself.
                        payload_data_copy.chart->SetID(outgoing_position);
                        outgoing_chart.chart->SetID(payload_position);

                        // Swap Positions.
                        m_graph_map[outgoing_position] = payload_data_copy;
                        m_graph_map[payload_position]  = outgoing_chart;
                    }
                    ImGui::EndDragDropTarget();
                }
            }

            m_graph_map[graph_objects.first].chart->UpdateMovement(
                m_zoom, m_movement, m_min_x, m_max_x, m_scale_x);

            m_graph_map[graph_objects.first].chart->Render();
            ImGui::PopStyleColor();

            ImGui::EndChild();
            ImGui::PopStyleColor();

            ImGui::Separator();
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void
MainView::MakeGraphView(std::map<std::string, rocprofvis_trace_process_t>& trace_data,
                        float                                              scale_x)
{
    m_trace_data_ptr = &trace_data;

    uint64_t num_graphs = 0;
    rocprofvis_result_t result = rocprofvis_controller_get_uint64(timeline, kRPVControllerTimelineNumGraphs, 0, &num_graphs);
    assert(result == kRocProfVisResultSuccess);

    int graph_id = 0;
    for(uint64_t i = 0; i < num_graphs; i++)
    {
        rocprofvis_handle_t* graph = nullptr;
        result = rocprofvis_controller_get_object(timeline, kRPVControllerTimelineGraphIndexed, i, &graph);
        assert(result == kRocProfVisResultSuccess && graph);

        rocprofvis_handle_t* track = nullptr;
        result = rocprofvis_controller_get_object(graph, kRPVControllerGraphTrack, 0, &track);
        assert(result == kRocProfVisResultSuccess && track);

        rocprofvis_controller_array_t* track_data = nullptr;
        result = rocprofvis_controller_get_object(array, kRPVControllerArrayEntryIndexed, i, &track_data);
        if(result == kRocProfVisResultSuccess && track_data)
        {
            uint64_t track_type = 0;
            result = rocprofvis_controller_get_uint64(track, kRPVControllerTrackType, 0,
                                                      &track_type);
            assert(result == kRocProfVisResultSuccess);

            uint32_t length = 0;
            result = rocprofvis_controller_get_string(track, kRPVControllerTrackName, 0,
                                                      nullptr, &length);
            assert(result == kRocProfVisResultSuccess);

            char* buffer = (char*) alloca(length + 1);
            length += 1;
            result = rocprofvis_controller_get_string(track, kRPVControllerTrackName, 0,
                                                      buffer, &length);
            assert(result == kRocProfVisResultSuccess);

            switch(track_type)
            {
                case kRPVControllerTrackTypeEvents:
                {
                    // Create FlameChart
                    m_flame_event = ExtractFlamePoints(track_data);
                    FindMaxMinFlame();
                    if(!m_meta_map_made)
                    {
                        // Create FlameChart title and info panel
                        rocprofvis_meta_map_struct_t temp_meta_map = {};

                        temp_meta_map.type       = "flame";
                        temp_meta_map.chart_name = buffer;
                        temp_meta_map.size       = 75;

                        m_meta_map[graph_id] = temp_meta_map;
                    }
                    RenderFlameCharts(graph_id, scale_x);

                    graph_id = graph_id + 1;
                    break;
                }
                case kRPVControllerTrackTypeSamples:
                {
                    // Linechart
                    m_data_arr = ExtractPointsFromData(track_data);
                    FindMaxMin();

                    if(!m_meta_map_made)
                    {
                        rocprofvis_meta_map_struct_t temp_meta_map = {};
                        temp_meta_map.type                         = "line";
                        temp_meta_map.chart_name                   = buffer;
                        temp_meta_map.max                          = m_max_y;
                        temp_meta_map.min                          = m_min_y;
                        temp_meta_map.size                         = 300;
                        m_meta_map[graph_id]                       = temp_meta_map;
                    }
                    RenderLineCharts(graph_id, scale_x);
                    graph_id = graph_id + 1;
                    break;
                }
                default:
                {
                    break;
                }
            }
        }
    }

    m_meta_map_made = true;
}

void
MainView::FindMaxMin()
{
    if(m_ran_once == false)
    {
        m_min_x    = m_data_arr[0].xValue;
        m_max_x    = m_data_arr[0].xValue;
        m_ran_once = true;
    }

    m_min_y = m_data_arr[0].yValue;
    m_max_y = m_data_arr[0].yValue;

    for(const auto& point : m_data_arr)
    {
        if(point.xValue < m_min_x)
        {
            m_min_x = point.xValue;
        }
        if(point.xValue > m_max_x)
        {
            m_max_x = point.xValue;
        }
        if(point.yValue < m_min_y)
        {
            m_min_y = point.yValue;
        }
        if(point.yValue > m_max_y)
        {
            m_max_y = point.yValue;
        }
    }
}

void
MainView::FindMaxMinFlame()
{
    if(m_ran_once == false)
    {
        m_min_x    = m_flame_event[0].m_start_ts;
        m_max_x    = m_flame_event[0].m_start_ts + m_flame_event[0].m_duration;
        m_ran_once = true;
    }

    for(const auto& point : m_flame_event)
    {
        if(point.m_start_ts < m_min_x)
        {
            m_min_x = point.m_start_ts;
        }
        if(point.m_start_ts + point.m_duration > m_max_x)
        {
            m_max_x = point.m_start_ts + point.m_duration;
        }
    }
}

std::vector<rocprofvis_data_point_t>
MainView::ExtractPointsFromData(rocprofvis_controller_array_t* track_data)
{
    std::vector<rocprofvis_data_point_t> aggregated_points;

    ImVec2 display_size = ImGui::GetIO().DisplaySize;
    int    screen_width = static_cast<int>(display_size.x);

    float effectiveWidth = screen_width / m_zoom;
    float bin_size       = (m_max_x - m_min_x) / effectiveWidth;

    double bin_sum_x         = 0.0;
    double bin_sum_y         = 0.0;
    int    bin_count         = 0;
    double current_bin_start = DBL_MAX;

    uint64_t count  = 0;
    rocprofvis_result_t result = rocprofvis_controller_get_uint64(track_data, kRPVControllerArrayNumEntries, 0, &count);
    assert(result == kRocProfVisResultSuccess);

    rocprofvis_trace_counter_t counter;

    for(uint64_t i = 0; i < count; i++)
    {
        rocprofvis_controller_sample_t* sample = nullptr;
        result = rocprofvis_controller_get_object(track_data, kRPVControllerArrayEntryIndexed, i, &sample);
        assert(result == kRocProfVisResultSuccess && sample);

        double start_ts = 0;
        result = rocprofvis_controller_get_double(sample, kRPVControllerSampleTimestamp, 0, &start_ts);
        assert(result == kRocProfVisResultSuccess);

        double value = 0;
        result = rocprofvis_controller_get_double(sample, kRPVControllerSampleValue, 0, &value);
        assert(result == kRocProfVisResultSuccess);

        counter.m_start_ts = start_ts;
        counter.m_value = value;

        if(i == 0)
        {
            current_bin_start = start_ts;
        }

        if(counter.m_start_ts < current_bin_start + bin_size)
        {
            bin_sum_x += counter.m_start_ts;
            bin_sum_y += counter.m_value;
            bin_count++;
        }
        else
        {
            if(bin_count > 0)
            {
                rocprofvis_data_point_t binned_point;
                binned_point.xValue = bin_sum_x / bin_count;
                binned_point.yValue = bin_sum_y / bin_count;
                aggregated_points.push_back(binned_point);
            }
            current_bin_start += bin_size;
            bin_sum_x = counter.m_start_ts;
            bin_sum_y = counter.m_value;
            bin_count = 1;
        }
    }

    if(bin_count > 0)
    {
        rocprofvis_data_point_t binned_point;
        binned_point.xValue = bin_sum_x / bin_count;
        binned_point.yValue = bin_sum_y / bin_count;
        aggregated_points.push_back(binned_point);
    }

    return aggregated_points;
}

std::vector<rocprofvis_trace_event_t>
MainView::ExtractFlamePoints(rocprofvis_controller_array_t* track_data)
{
    std::vector<rocprofvis_trace_event_t> entries;

    ImVec2 display_size = ImGui::GetIO().DisplaySize;
    int    screen_width = static_cast<int>(display_size.x);

    float effective_width = screen_width / m_zoom;
    float bin_size        = ((m_max_x - m_min_x) / effective_width);

    double bin_sum_x         = 0.0;
    int    bin_count         = 0;
    double current_bin_start = DBL_MAX;
    float  largest_duration  = 0;

    uint64_t count  = 0;
    rocprofvis_result_t result = rocprofvis_controller_get_uint64(track_data, kRPVControllerArrayNumEntries, 0, &count);
    assert(result == kRocProfVisResultSuccess);

    rocprofvis_trace_event_t counter;

    for(uint64_t i = 0; i < count; i++)
    {
        rocprofvis_controller_event_t* event = nullptr;
        result = rocprofvis_controller_get_object(track_data, kRPVControllerArrayEntryIndexed, i, &event);
        assert(result == kRocProfVisResultSuccess && event);

        double start_ts = 0;
        result = rocprofvis_controller_get_double(event, kRPVControllerEventStartTimestamp, 0, &start_ts);
        assert(result == kRocProfVisResultSuccess);

        double end_ts = 0;
        result = rocprofvis_controller_get_double(event, kRPVControllerEventEndTimestamp, 0, &end_ts);
        assert(result == kRocProfVisResultSuccess);

        uint32_t length = 0;
        result = rocprofvis_controller_get_string(event, kRPVControllerEventName, 0, nullptr, &length);
        assert(result == kRocProfVisResultSuccess);
        
        length += 1;
        counter.m_name.resize(length);
        char* buffer = const_cast<char*>(counter.m_name.c_str());
        assert(buffer);
        result = rocprofvis_controller_get_string(event, kRPVControllerEventName, 0, buffer, &length);
        assert(result == kRocProfVisResultSuccess);

        if (i == 0)
        {
            current_bin_start = start_ts;
        }

        counter.m_start_ts = start_ts;
        counter.m_duration = end_ts - start_ts;

        if(counter.m_start_ts < current_bin_start + bin_size)
        {
            if(counter.m_duration > largest_duration)
            {
                largest_duration =
                    counter.m_duration;  // Use the largest duration per bin.
            }
            bin_sum_x += counter.m_start_ts;
            bin_count++;
        }
        else
        {
            if(bin_count > 0)
            {
                rocprofvis_trace_event_t binned_point;
                binned_point.m_start_ts = bin_sum_x / bin_count;
                binned_point.m_duration = largest_duration;
                binned_point.m_name     = counter.m_name;
                entries.push_back(binned_point);
            }

            // Prepare next bin.
            current_bin_start =
                current_bin_start +
                bin_size *
                    static_cast<int>((counter.m_start_ts - current_bin_start) / bin_size);
            bin_sum_x        = counter.m_start_ts;
            largest_duration = counter.m_duration;
            bin_count        = 1;
        }
    }

    if(bin_count > 0)
    {
        rocprofvis_trace_event_t binned_point;
        binned_point.m_start_ts = bin_sum_x / bin_count;
        binned_point.m_duration = largest_duration;
        binned_point.m_name     = counter.m_name;

        entries.push_back(binned_point);
    }

    return entries;
}

void
MainView::GenerateGraphPoints(rocprofvis_controller_timeline_t* object,
                              rocprofvis_controller_array_t*    array)

{
    ImVec2 screen_pos = ImGui::GetCursorScreenPos();

    ImVec2 display_size_main = ImGui::GetWindowSize();



    if(ImGui::BeginChild("Main Graphs"))
    {
        ImVec2 display_size_main_graphs = ImGui::GetIO().DisplaySize;

        ImVec2 subcomponent_size_main = ImGui::GetContentRegionAvail();

        ImGui::BeginChild(
            "Grid View 2", ImVec2(subcomponent_size_main.x, subcomponent_size_main.y),
            false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

        ImVec2 content_size = ImGui::GetContentRegionAvail();

        // Scale used in all graphs computer here.
        m_v_width = (m_max_x - m_min_x) / m_zoom;
        m_v_min_x = m_min_x + m_movement;
        m_v_max_x = m_v_min_x + m_v_width;
        m_scale_x = content_size.x / (m_v_max_x - m_v_min_x);

        RenderGrid();

        MakeGraphView(object, array, m_scale_x);

        ImGuiIO& io       = ImGui::GetIO();
        m_is_control_held = io.KeyCtrl;
        if(!m_is_control_held)
        {
            // Disable when user wants to reposition graphs.
            RenderScrubber(display_size_main_graphs, screen_pos);
            HandleTopSurfaceTouch();  // Funtion enables user interactions to be captured
        }

        ImGui::EndChild();
    }
    ImGui::End();
    ImGui::SetNextWindowPos(ImVec2(0, 20.0f));
    ImGui::SetNextWindowSize(ImVec2(m_sidebar_size, display_size_main.y - 20.0f),
                             ImGuiCond_Always);

    if(ImGui::Begin("Graph MetaData", nullptr,
                    ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollWithMouse |
                        ImGuiWindowFlags_HorizontalScrollbar |
                        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoResize))
    {
        ImVec2 subcomponent_size = ImGui::GetContentRegionAvail();

        ImGui::BeginChild(
            "MetaData Content", ImVec2(subcomponent_size.x - 10.0f, subcomponent_size.y),
            false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

        MakeGraphMetadataView();

        ImGui::EndChild();

        ImGui::SameLine();

        // Second child (20%)
        ImGui::BeginChild("Graph Scale", ImVec2(10.0f, subcomponent_size.y), false,
                          ImGuiWindowFlags_NoScrollbar |
                              ImGuiWindowFlags_NoScrollWithMouse);

        HandleSidebarResize();

        ImGui::EndChild();
    }
    ImGui::End();
}

void
MainView::HandleTopSurfaceTouch()
{
    /*
    This component enables the capture of user inputs and saves them as class variable.
    Enables user interactions please dont touch.
    */
    if(!m_is_control_held)
    {
        // Handle Zoom
        if(ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows))
        {
            float scroll_wheel = ImGui::GetIO().MouseWheel;
            if(scroll_wheel != 0.0f)
            {
                float       view_width = (m_max_x - m_min_x) / m_zoom;
                const float zoom_speed = 0.1f;
                m_zoom *= (scroll_wheel > 0) ? (1.0f + zoom_speed) : (1.0f - zoom_speed);
                m_zoom = m_zoom;

                m_v_width = (m_max_x - m_min_x) / m_zoom;
                m_v_min_x = m_min_x + m_movement;
                m_v_max_x = m_v_min_x + m_v_width;
             }
        }

        // Handle Panning
        if(ImGui::IsMouseDragging(ImGuiMouseButton_Left))
        {
            float drag       = ImGui::GetIO().MouseDelta.x;
            float view_width = (m_max_x - m_min_x) / m_zoom;
            m_movement -= (drag / ImGui::GetContentRegionAvail().x) * view_width;
             float drag_y = ImGui::GetIO().MouseDelta.y;
             m_scroll_position = static_cast<int>(m_scroll_position - drag_y);
        }
    }
}

void
MainView::HandleSidebarResize()
{
    // Create an invisible button with a more area
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 5);
    ImGui::InvisibleButton("Resize Bar", ImVec2(10, ImGui::GetContentRegionAvail().y));
    if(ImGui::IsItemActive() && ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        m_user_adjusting_graph_height = true;
        ImVec2 drag_delta             = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);

        m_sidebar_size = m_sidebar_size + drag_delta.x;
        ImGui::ResetMouseDragDelta();
    }
}

}  // namespace View
}  // namespace RocProfVis
