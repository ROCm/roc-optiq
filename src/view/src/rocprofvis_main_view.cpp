// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_main_view.h"
#include "imgui.h"
#include "rocprofvis_controller.h"
#include "rocprofvis_flame_chart.h"
#include "rocprofvis_grid.h"
#include "rocprofvis_line_chart.h"
#include <iostream>
#include <map>
#include <string>
#include <tuple>
#include <vector>

namespace RocProfVis
{
namespace View
{
float
clamp(float min, float max, float value)
{
    if(value < min)
    {
        return min;
    }
    else if(value > max)
    {
        return max;
    }
    else
    {
        return value;
    }
}
MainView::MainView()
: m_min_value(0.0f)
, m_max_value(0.0f)
, m_zoom(1.0f)
, m_movement(0.0f)
, m_min_x(0.0f)
, m_max_x(0.0f)  // use FLT_MAX and FLT_MIN?
, m_min_y(0.0f)
, m_max_y(0.0f)
, m_scroll_position(0.0f)
, m_scrubber_position(0.0f)
, m_v_min_x()
, m_v_max_x()
, m_scale_x()
, m_sidebar_size(500.f)
, m_meta_map_made(false)
, m_meta_map({})
, m_user_adjusting_graph_height(false)
, m_previous_scroll_position(0)
, m_show_graph_customization_window(false)
, m_graph_map({})
, min_max_x_init(false)
, m_is_control_held(false)
, m_original_v_max_x(-100)
{}

MainView::~MainView() {}

void
MainView::Render()
{
    if(m_meta_map_made)
    {
        RenderGraphPoints();
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

    // overlayed windows need to have fully trasparent bg otherwise they will overlay
    // (with no alpha) over their predecessors
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
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

    ImVec2 display_size = ImGui::GetWindowSize();

    ImGui::SetNextWindowSize(ImVec2(display_size.x, display_size.y), ImGuiCond_Always);

    ImGui::SetCursorPos(ImVec2(0, 0));
    // overlayed windows need to have fully trasparent bg otherwise they will overlay
    // (with no alpha) over their predecessors
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(220, 0, 0, 0));

    ImDrawList*            draw_list = ImGui::GetWindowDrawList();
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
MainView::RenderGraphView()
{
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                                    ImGuiWindowFlags_NoScrollWithMouse;

    ImVec2 display_size = ImGui::GetWindowSize();

    ImGui::SetNextWindowSize(ImVec2(display_size.x, display_size.y - 60.0f),
                             ImGuiCond_Always);
    ImGui::SetCursorPos(ImVec2(0, 0));

    // overlayed windows need to have fully trasparent bg otherwise they will overlay
    // (with no alpha) over their predecessors
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
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
MainView::MakeGraphView(rocprofvis_controller_timeline_t* timeline,
                        rocprofvis_controller_array_t* array, float scale_x)
{
    /*This section makes the charts both line and flamechart are constructed here*/
    uint64_t            num_graphs = 0;
    rocprofvis_result_t result     = rocprofvis_controller_get_uint64(
        timeline, kRPVControllerTimelineNumGraphs, 0, &num_graphs);
    assert(result == kRocProfVisResultSuccess);

    int graph_id = 0;
    for(uint64_t i = 0; i < num_graphs; i++)
    {
        rocprofvis_handle_t* graph = nullptr;
        result                     = rocprofvis_controller_get_object(
            timeline, kRPVControllerTimelineGraphIndexed, i, &graph);
        assert(result == kRocProfVisResultSuccess && graph);

        rocprofvis_handle_t* track = nullptr;
        result =
            rocprofvis_controller_get_object(graph, kRPVControllerGraphTrack, 0, &track);
        assert(result == kRocProfVisResultSuccess && track);

        rocprofvis_controller_array_t* track_data = nullptr;
        result = rocprofvis_controller_get_object(array, kRPVControllerArrayEntryIndexed,
                                                  i, &track_data);
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
                    std::string                   name = buffer;
                    RocProfVis::View::FlameChart* flame =
                        new RocProfVis::View::FlameChart(graph_id, name, m_zoom,
                                                         m_movement, m_min_x, m_max_x,
                                                         scale_x);

                    flame->ExtractFlamePoints(track_data);
                    std::tuple<float, float> temp_min_max_flame =
                        flame->FindMaxMinFlame();

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
                    temp_flame.chart          = flame;
                    temp_flame.graph_type     = rocprofvis_graph_map_t::TYPE_FLAMECHART;
                    temp_flame.display        = true;
                    temp_flame.selected       = ImVec4(0, 0, 0, 0);
                    temp_flame.color_by_value = false;
                    rocprofvis_color_by_value_t temp_color = {};
                    temp_flame.color_by_value_digits       = temp_color;
                    m_graph_map[graph_id]                  = temp_flame;

                    graph_id = graph_id + 1;

                    break;
                }
                case kRPVControllerTrackTypeSamples:
                {
                    // Linechart
                    std::string name = buffer;

                    void* datap = nullptr;

                    RocProfVis::View::LineChart* line = new RocProfVis::View ::LineChart(
                        graph_id, name, m_zoom, m_movement, m_min_x, m_max_x, m_scale_x,
                        datap);

                    line->ExtractPointsFromData(track_data);
                    std::tuple<float, float> temp_min_max = line->FindMaxMin();

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
                    temp.chart          = line;
                    temp.graph_type     = rocprofvis_graph_map_t::TYPE_LINECHART;
                    temp.display        = true;
                    temp.selected       = ImVec4(0, 0, 0, 0);
                    temp.color_by_value = false;
                    rocprofvis_color_by_value_t temp_color_line = {};
                    temp.color_by_value_digits                  = temp_color_line;
                    m_graph_map[graph_id]                       = temp;

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
MainView::RenderGraphPoints()
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

        if(m_original_v_max_x == -100)
        {
            m_original_v_max_x = m_v_max_x;  // Used to set bounds
        }

        RenderGrid();

        if(m_meta_map_made)
        {
            RenderGraphView();
        }

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
    ImGui::EndChild();
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
                m_zoom = clamp(0.9, 200, m_zoom);
                m_movement += m_v_width - ((m_max_x - m_min_x) / m_zoom);
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
            if((10000 * ((m_min_x / m_v_min_x) - 1)) > 1.6 * (1 / (m_scale_x * 1000)))
            {
                if((drag / ImGui::GetContentRegionAvail().x) * view_width < 0)
                {
                    m_movement -= (drag / ImGui::GetContentRegionAvail().x) * view_width;
                }
            }
            else if((100000 * ((m_original_v_max_x / m_v_max_x) - 1)) <
                    1.6 * (1 / (m_scale_x * 1000)))
            {
                ////THIS ONE IS STILL PROBLEMATIC.
                if((drag / ImGui::GetContentRegionAvail().x) * view_width > 0)
                {
                    m_movement -= (drag / ImGui::GetContentRegionAvail().x) * view_width;
                }
            }
            else
            {
                m_movement -= (drag / ImGui::GetContentRegionAvail().x) * view_width;
            }
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
