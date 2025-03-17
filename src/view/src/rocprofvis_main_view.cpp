// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_main_view.h"
#include "imgui.h"
#include "rocprofvis_flame_chart.h"
#include "rocprofvis_graph_view_metadata.h"
#include "rocprofvis_grid.h"
#include "rocprofvis_line_chart.h"
#include "rocprofvis_structs.h"
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
, m_scrubber_position(0.0f)
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
{}

MainView::~MainView() {}
void
MainView::MakeScrubber(ImVec2 display_size_main_graphs, ImVec2 screen_pos)
{
    ImVec2 windowPos  = ImGui::GetWindowPos();
    ImVec2 windowSize = ImGui::GetWindowSize();

    // Scrubber Line

    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                                    ImGuiWindowFlags_NoScrollWithMouse;

    ImVec2 display_size = ImGui::GetWindowSize();

    ImGui::SetNextWindowSize(ImVec2(display_size.x - 20.0f, display_size.y),
                             ImGuiCond_Always);

    ImGui::SetCursorPos(ImVec2(0, 0));

    ImGui::BeginChild("Scrubber View", ImVec2(0, 0), false, window_flags);
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    if(ImGui::IsMouseHoveringRect(
           ImVec2(m_sidebar_size, 00),
           ImVec2(display_size_main_graphs.x, display_size_main_graphs.y)))
    {
        ImVec2 mPos = ImGui::GetMousePos();
        draw_list->AddLine(ImVec2(mPos.x, screen_pos.y),
                           ImVec2(mPos.x, screen_pos.y + display_size_main_graphs.y),
                           IM_COL32(0, 0, 0, 255), 2.0f);
    }

    ImGui::EndChild();
}
void
MainView::MakeGrid()
{
    /*This section makes the grid for the charts*/

    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                                    ImGuiWindowFlags_NoScrollWithMouse;

    ImVec2      display_size = ImGui::GetWindowSize();
    ImDrawList* draw_list    = ImGui::GetWindowDrawList();

    ImGui::SetNextWindowSize(ImVec2(display_size.x, display_size.y), ImGuiCond_Always);

    ImGui::SetCursorPos(ImVec2(0, 0));

    ImGui::BeginChild("Grid View", ImVec2(0, 0), false, window_flags);

    RocProfVis::View::Grid main_grid = RocProfVis::View::Grid();

    main_grid.RenderGrid(m_min_x, m_max_x, m_movement, m_zoom, draw_list, m_scale_x,
                         m_v_max_x, m_v_min_x);

    ImGui::EndChild();
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
MainView::MakeGraphMetadataView(
    std::map<std::string, rocprofvis_trace_process_t>& trace_data)
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
MainView::MakeGraphView(std::map<std::string, rocprofvis_trace_process_t>& trace_data,
                        float                                              scale_x)
{
    /*This section makes the charts both line and flamechart are constructed here*/

    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                                    ImGuiWindowFlags_NoScrollWithMouse;

    ImVec2 display_size = ImGui::GetWindowSize();

    ImGui::SetNextWindowSize(ImVec2(display_size.x, display_size.y - 60.0f),
                             ImGuiCond_Always);
    ImGui::SetCursorPos(ImVec2(0, 0));

    ImGui::BeginChild("Graph View Main", ImVec2(0, 0), false, window_flags);

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

    int graph_id = 0;
    for(auto& process : trace_data)
    {
        for(auto& thread : process.second.m_threads)
        {
            auto& events   = thread.second.m_events;
            auto& counters = thread.second.m_counters;
            if(events.size())
            {
                // Create FlameChart
                m_flame_event = ExtractFlamePoints(events);
                FindMaxMinFlame();
                if(!m_meta_map_made)
                {
                    // Create FlameChart title and info panel
                    rocprofvis_meta_map_struct_t temp_meta_map = {};

                    temp_meta_map.type       = "flame";
                    temp_meta_map.chart_name = thread.first;
                    temp_meta_map.size       = 75;

                    m_meta_map[graph_id] = temp_meta_map;
                }

                ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
                ImGui::BeginChild((std::to_string(graph_id)).c_str(),
                                  ImVec2(0, m_meta_map[graph_id].size), false,
                                  ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                                      ImGuiWindowFlags_NoScrollWithMouse |
                                      ImGuiWindowFlags_NoScrollbar);
                RocProfVis::View::FlameChart flame = RocProfVis::View::FlameChart(
                    graph_id, m_zoom, m_movement, m_min_x, m_max_x, scale_x, events);
                flame.ExtractFlamePoints();
                flame.render();

                ImGui::EndChild();
                ImGui::PopStyleVar();
                ImGui::Spacing();
                ImGui::Separator();

                HandleGraphResize(graph_id);

                graph_id = graph_id + 1;
            }

            else if(counters.size())
            {
                // Linechart
                void* datap = (void*) &thread.second.m_counters;
                int   count = counters.size();

                if(!m_meta_map_made)
                {
                    rocprofvis_meta_map_struct_t temp_meta_map = {};
                    temp_meta_map.type                         = "line";
                    temp_meta_map.chart_name                   = thread.first;
                    temp_meta_map.max                          = m_max_y;
                    temp_meta_map.min                          = m_min_y;
                    temp_meta_map.size                         = 300;
                    m_meta_map[graph_id]                       = temp_meta_map;
                }
                ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

                ImGui::BeginChild((std::to_string(graph_id)).c_str(),
                                  ImVec2(0, m_meta_map[graph_id].size), false,
                                  ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                                      ImGuiWindowFlags_NoScrollWithMouse |
                                      ImGuiWindowFlags_NoScrollbar);

                if(m_graph_map.find(graph_id) != m_graph_map.end())
                {
                    m_graph_map[graph_id].graph.line_chart->UpdateMovement(
                        m_zoom, m_movement, m_min_x, m_max_x, m_scale_x);
                    m_graph_map[graph_id].graph.line_chart->Render();
                }
                else
                {
                    RocProfVis::View::LineChart* line = new RocProfVis::View ::LineChart(
                        graph_id, m_zoom, m_movement, m_min_x, m_max_x, m_scale_x, datap);
                    line->ExtractPointsFromData();
                    std::tuple<float, float> temp_min_max = line->FindMaxMin();
                    line->Render();

                    if(std::get<0>(temp_min_max) < m_min_x)
                    {
                        m_min_x = std::get<0>(temp_min_max);
                    }
                    if(std::get<1>(temp_min_max) > m_max_x)
                    {
                        m_max_x = std::get<1>(temp_min_max);
                    }

                    rocprofvis_graph_map_t temp;
                    temp.graph.line_chart = line;
                    m_graph_map[graph_id] = temp;
                }

                ImGui::EndChild();
                ImGui::PopStyleVar();
                ImGui::Spacing();
                ImGui::Separator();
                HandleGraphResize(graph_id);
                graph_id = graph_id + 1;
            }
        }
    }

    m_meta_map_made = true;
    ImGui::EndChild();
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

std::vector<rocprofvis_trace_event_t>
MainView::ExtractFlamePoints(const std::vector<rocprofvis_trace_event_t>& traceEvents)
{
    std::vector<rocprofvis_trace_event_t> entries;

    ImVec2 display_size = ImGui::GetIO().DisplaySize;
    int    screen_width = static_cast<int>(display_size.x);

    float effective_width = screen_width / m_zoom;
    float bin_size        = ((m_max_x - m_min_x) / effective_width);

    double bin_sum_x         = 0.0;
    int    bin_count         = 0;
    double current_bin_start = traceEvents[0].m_start_ts;
    float  largest_duration  = 0;
    for(const auto& counter : traceEvents)
    {
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
        binned_point.m_name     = traceEvents.back().m_name;

        entries.push_back(binned_point);
    }

    return entries;
}

void
MainView::GenerateGraphPoints(
    std::map<std::string, rocprofvis_trace_process_t>& trace_data)

{
    ImVec2 screen_pos = ImGui::GetCursorScreenPos();

    ImVec2 display_size_main = ImGui::GetIO().DisplaySize;

    ImGui::SetNextWindowPos(ImVec2(m_sidebar_size, 20.0f));
    ImGui::SetNextWindowSize(
        ImVec2(display_size_main.x - m_sidebar_size, display_size_main.y - 20.0f),
        ImGuiCond_Always);

    if(ImGui::Begin("Main Graphs", nullptr,
                    ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollWithMouse |
                        ImGuiWindowFlags_HorizontalScrollbar |
                        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoResize))
    {
        ImVec2 display_size_main = ImGui::GetIO().DisplaySize;

        ImVec2 display_size_main_graphs = ImGui::GetIO().DisplaySize;

        ImVec2 subcomponent_size_main = ImGui::GetContentRegionAvail();

        ImGui::BeginChild(
            "Grid View", ImVec2(subcomponent_size_main.x, subcomponent_size_main.y),
            false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        ImVec2 content_size = ImGui::GetContentRegionAvail();

        // Scale used in all graphs computer here.
        m_v_width = (m_max_x - m_min_x) / m_zoom;
        m_v_min_x = m_min_x + m_movement;
        m_v_max_x = m_v_min_x + m_v_width;
        m_scale_x = content_size.x / (m_v_max_x - m_v_min_x);

        MakeGrid();
        HandleTopSurfaceTouch();  // Funtion enables user interactions to be captured and
                                  // relayed into subcomponents as needed.

        MakeGraphView(trace_data, m_scale_x);

        MakeScrubber(display_size_main_graphs, screen_pos);

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

        MakeGraphMetadataView(trace_data);

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
    if(m_user_adjusting_graph_height == false)
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
                m_zoom = clamp(m_zoom, 1.0f, 1000.0f);

                m_v_width = (m_max_x - m_min_x) / m_zoom;
                m_v_min_x = m_min_x + m_movement;
                m_v_max_x = m_v_min_x + m_v_width;
                m_movement += view_width - (m_v_max_x - m_v_min_x);
            }
        }

        // Handle Panning
        if(ImGui::IsMouseDragging(ImGuiMouseButton_Left))
        {
            float drag       = ImGui::GetIO().MouseDelta.x;
            float view_width = (m_max_x - m_min_x) / m_zoom;
            m_movement -= (drag / ImGui::GetContentRegionAvail().x) * view_width;
            m_scrubber_position -= (drag / ImGui::GetContentRegionAvail().x) * view_width;
            float drag_y = ImGui::GetIO().MouseDelta.y;

            m_scroll_position = static_cast<int>(m_scroll_position - drag_y);
        }
    }
}
void
MainView::HandleGraphResize(int chart_id)
{
    // Create an invisible button with a more area
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 5);
    ImGui::InvisibleButton(std::to_string(chart_id).c_str(),
                           ImVec2(ImGui::GetContentRegionAvail().x, 10));
    if(ImGui::IsItemActive() && ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        m_user_adjusting_graph_height = true;
        ImVec2 drag_delta             = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);
        rocprofvis_meta_map_struct_t temp_meta_map = m_meta_map[chart_id];
        temp_meta_map.size                         = temp_meta_map.size + (drag_delta.y);
        m_meta_map[chart_id]                       = temp_meta_map;
        ImGui::ResetMouseDragDelta();
    }
    else if(!ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        m_user_adjusting_graph_height = false;
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

void
MainView::RenderGraphMetadata(int graph_id, float size, std::string type,
                              rocprofvis_meta_map_struct_t data)
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::BeginChild((std::to_string(graph_id)).c_str(), ImVec2(0, data.size), false,
                      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                          ImGuiWindowFlags_NoScrollWithMouse |
                          ImGuiWindowFlags_NoScrollbar);

    ImVec2 childSize        = ImGui::GetContentRegionAvail();
    float  splitRatio       = 0.8f;
    float  firstChildWidth  = childSize.x * splitRatio;
    float  secondChildWidth = childSize.x * (1.0f - splitRatio);

    ImGui::BeginChild("MetaData Content", ImVec2(childSize.x - 50.0f, childSize.y),
                      false);
    RocProfVis::View::GraphViewMetadata metaData =
        RocProfVis::View::GraphViewMetadata(graph_id, size, type, data);
    metaData.renderData();

    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("MetaData Scale", ImVec2(50.0f, childSize.y), false);

    ImGui::Text((std::to_string(data.max)).c_str());

    ImVec2 child_window_size = ImGui::GetWindowSize();
    ImVec2 text_size         = ImGui::CalcTextSize("Scale Size");
    ImGui::SetCursorPos(
        ImVec2(0, child_window_size.y - text_size.y - ImGui::GetStyle().WindowPadding.y));

    ImGui::Text((std::to_string(data.min)).c_str());

    ImGui::EndChild();

    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::Spacing();
    ImGui::Separator();
    HandleGraphResize(graph_id);
}

}  // namespace View
}  // namespace RocProfVis
