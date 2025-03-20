// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_main_view.h"
#include "imgui.h"
#include "rocprofvis_flame_chart.h"
#include "rocprofvis_graph_view_metadata.h"
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
, min_max_x_init(false)
, m_is_control_held(false)
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
MainView::ContinueGraphView()
{
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
    for(const auto& graph_objects : m_graph_map)
    {
        ImGuiIO& io         = ImGui::GetIO();
        m_is_control_held =   io.KeyCtrl;

        if(m_is_control_held)
        {
            ImGui::Selectable(
                ("Move Position " + std::to_string(graph_objects.first)).c_str(), false,
                ImGuiSelectableFlags_AllowDoubleClick, ImVec2(0, 20.0f));

        }

      
     
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

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
                    (rocprofvis_graph_map_t*) payload->Data;  // incoming (being dragged)
                rocprofvis_graph_map_t payload_data_copy = *payload_data;
                int payload_position = payload_data_copy.chart->ReturnChartID();
                rocprofvis_graph_map_t outgoing_chart =
                    m_graph_map[graph_objects.first];  // outgoing (getting replaced)
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

        ImGui::BeginChild(
            (std::to_string(graph_objects.first)).c_str(),
            ImVec2(0, m_graph_map[graph_objects.first].chart->ReturnSize()), false,
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoScrollbar);
        m_graph_map[graph_objects.first].chart->UpdateMovement(
            m_zoom, m_movement, m_min_x, m_max_x, m_scale_x);

        m_graph_map[graph_objects.first].chart->Render();

        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::Spacing();
        ImGui::Separator();
        HandleGraphResize(graph_objects.first);
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

                ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
                ImGui::BeginChild((std::to_string(graph_id)).c_str(), ImVec2(0, 75.0f),
                                  false,
                                  ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                                      ImGuiWindowFlags_NoScrollWithMouse |
                                      ImGuiWindowFlags_NoScrollbar);

                RocProfVis::View::FlameChart* flame = new RocProfVis::View::FlameChart(
                    graph_id, thread.first, m_zoom, m_movement, m_min_x, m_max_x, scale_x,
                    events);

                flame->ExtractFlamePoints();
                std::tuple<float, float> temp_min_max_flame = flame->FindMaxMinFlame();

                flame->Render();
                if(min_max_x_init == false)
                {
                    // Without this 0 would be lowest which is not correct behavior.
                    m_min_x        = std::get<0>(temp_min_max_flame);
                    m_max_x        = std::get<1>(temp_min_max_flame);
                    min_max_x_init = true;
                }

                if(std::get<0>(temp_min_max_flame) < m_min_x)
                {
                    m_min_x = std::get<0>(temp_min_max_flame);
                }
                if(std::get<1>(temp_min_max_flame) > m_max_x)
                {
                    m_max_x = std::get<1>(temp_min_max_flame);
                }

                rocprofvis_graph_map_t temp_flame;
                temp_flame.chart      = flame;
                temp_flame.graph_type = rocprofvis_graph_map_t::TYPE_FLAMECHART;

                m_graph_map[graph_id] = temp_flame;

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

                ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

                ImGui::BeginChild((std::to_string(graph_id)).c_str(), ImVec2(0, 300.0f),
                                  false,
                                  ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                                      ImGuiWindowFlags_NoScrollWithMouse |
                                      ImGuiWindowFlags_NoScrollbar);

                RocProfVis::View::LineChart* line = new RocProfVis::View ::LineChart(
                    graph_id, thread.first, m_zoom, m_movement, m_min_x, m_max_x,
                    m_scale_x, datap);
                line->ExtractPointsFromData();
                std::tuple<float, float> temp_min_max = line->FindMaxMin();
                line->Render();

                if(min_max_x_init == false)
                {
                    // Without this 0 would be lowest which is not correct behavior.
                    m_min_x        = std::get<0>(temp_min_max);
                    m_max_x        = std::get<1>(temp_min_max);
                    min_max_x_init = true;
                }

                if(std::get<0>(temp_min_max) < m_min_x)
                {
                    m_min_x = std::get<0>(temp_min_max);
                }
                if(std::get<1>(temp_min_max) > m_max_x)
                {
                    m_max_x = std::get<1>(temp_min_max);
                }

                rocprofvis_graph_map_t temp;
                temp.chart            = line;
                temp.graph_type       = rocprofvis_graph_map_t::TYPE_LINECHART;
                m_graph_map[graph_id] = temp;

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
MainView::GenerateGraphPoints(
    std::map<std::string, rocprofvis_trace_process_t>& trace_data)

{
    ImVec2 screen_pos = ImGui::GetCursorScreenPos();

    ImVec2 display_size_main = ImGui::GetIO().DisplaySize;

    ImGui::SetNextWindowPos(ImVec2(0, 20.0f));
    ImGui::SetNextWindowSize(ImVec2(display_size_main.x, display_size_main.y - 20.0f),
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
         // and
        //  relayed into subcomponents as needed.
        if(m_meta_map_made == false)
        {
            MakeGraphView(trace_data, m_scale_x);
        }
        else
        {
            ContinueGraphView();
        }

        if (!m_is_control_held) {
            //Disable when user wants to reposition graphs.
            MakeScrubber(display_size_main_graphs, screen_pos);
            HandleTopSurfaceTouch();  // Funtion enables user interactions to be captured

        }
 

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

}  // namespace View
}  // namespace RocProfVis
