// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_main_view.h"
#include "imgui.h"
#include "rocprofvis_boxplot.h"
#include "rocprofvis_controller.h"
#include "rocprofvis_core_assert.h"
#include "rocprofvis_flame_chart.h"
#include "rocprofvis_grid.h"
#include "rocprofvis_line_chart.h"
#include "rocprofvis_utils.h"
#include "spdlog/spdlog.h"
#include <iostream>
#include <map>
#include <string>
#include <tuple>
#include <vector>

#include "widgets/rocprofvis_debug_window.h"

namespace RocProfVis
{
namespace View
{

MainView::MainView(DataProvider& dp)
: m_data_provider(dp)
, m_zoom(1.0f)
, m_movement(0.0f)
, m_min_x(FLT_MAX)
, m_max_x(FLT_MIN)
, m_min_y(FLT_MAX)
, m_max_y(FLT_MIN)
, m_scroll_position(0.0f)
, m_content_max_y_scoll(0.0f)
, m_scrubber_position(0.0f)
, m_v_min_x(0.0f)
, m_v_max_x(0.0f)
, m_scale_x(0.0f)
, m_meta_map_made(false)
, m_user_adjusting_graph_height(false)
, m_previous_scroll_position(0.0f)
, m_show_graph_customization_window(false)
, m_graph_map({})
, m_is_control_held(false)
, m_can_drag_to_pan(false)
, m_original_v_max_x(0.0f)
, m_capture_og_v_max_x(true)
, m_grid_size(50)
{
    m_new_track_data_handler = [this](std::shared_ptr<RocEvent> e) {
        this->HandleNewTrackData(e);
    };
    EventManager::GetInstance()->Subscribe(static_cast<int>(RocEvents::kNewTrackData),
                                           m_new_track_data_handler);
}

MainView::~MainView()
{
    DestroyGraphs();
    EventManager::GetInstance()->Unsubscribe(static_cast<int>(RocEvents::kNewTrackData),
                                             m_new_track_data_handler);
}

void
MainView::ResetView()
{
    m_zoom               = 1.0f;
    m_movement           = 0.0f;
    m_min_x              = FLT_MAX;
    m_max_x              = FLT_MIN;
    m_min_y              = FLT_MAX;
    m_max_y              = FLT_MIN;
    m_scroll_position    = 0.0f;
    m_scrubber_position  = 0.0f;
    m_v_min_x            = 0.0f;
    m_v_max_x            = 0.0f;
    m_scale_x            = 0.0f;
    m_original_v_max_x   = 0.0f;
    m_capture_og_v_max_x = true;
}

void
MainView::HandleNewTrackData(std::shared_ptr<RocEvent> e)
{
    if(!e)
    {
        spdlog::debug("Null event, cannot process new track data");
        return;
    }

    std::shared_ptr<TrackDataEvent> tde = std::dynamic_pointer_cast<TrackDataEvent>(e);
    if(!tde)
    {
        spdlog::debug("Invalid event type {}, cannot process new track data",
                      static_cast<int>(e->GetType()));
    }
    else
    {
        uint64_t            track_index = tde->GetTrackIndex();
        const RawTrackData* rtd         = m_data_provider.GetRawTrackData(track_index);

        if(m_graph_map[track_index].chart->SetRawData(rtd))
        {
            auto min_max = m_graph_map[track_index].chart->GetMinMax();

            if(std::get<0>(min_max) < m_min_x)
            {
                m_min_x = std::get<0>(min_max);
            }
            if(std::get<1>(min_max) > m_max_x)
            {
                m_max_x = std::get<1>(min_max);
            }

            spdlog::debug("min max is now {},{}", m_min_x, m_max_x);
        }
    }
}

void
MainView::Update()
{
    if(m_meta_map_made)
    {
        // nothing for now
    }
}

void
MainView::Render()
{
    if(m_meta_map_made)
    {
        RenderGraphPoints();
    }
}

void
MainView::RenderScrubber(ImVec2 screen_pos)
{
    // Scrubber Line
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                                    ImGuiWindowFlags_NoScrollWithMouse;

    ImVec2      display_size         = ImGui::GetWindowSize();
    float       scrollbar_width      = ImGui::GetStyle().ScrollbarSize;
    const float metadata_area_offset = 400.0f;
    ImGui::SetNextWindowSize(
        ImVec2(display_size.x - scrollbar_width - metadata_area_offset, display_size.y),
        ImGuiCond_Always);
    ImGui::SetCursorPos(
        ImVec2(metadata_area_offset, 0));  // Meta Data size will be universal next PR.

    // overlayed windows need to have fully trasparent bg otherwise they will overlay
    // (with no alpha) over their predecessors
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));

    ImGui::BeginChild("Scrubber View", ImVec2(0, 0), ImGuiChildFlags_None, window_flags);
    ImDrawList* draw_list      = ImGui::GetWindowDrawList();
    ImVec2      window_size    = ImGui::GetContentRegionAvail();
    float       mouse_relative = window_size.x - ImGui::GetMousePos().x;

    ImVec2 window_position = ImGui::GetWindowPos();
    ImVec2 mouse_position  = ImGui::GetMousePos();

    ImVec2 relativeMousePos = ImVec2(mouse_position.x - window_position.x,
                                     mouse_position.y - window_position.y);

    // IsMouseHoveringRect check in screen coordinates
    if(ImGui::IsMouseHoveringRect(
           window_position,
           ImVec2(window_position.x + window_size.x, window_position.y + window_size.y)))
    {
        ImVec2 mouse_position = ImGui::GetMousePos();

        char text[20];
        sprintf(text, "%.0f", m_grid.GetCursorPosition());
        ImVec2 text_pos = ImVec2(mouse_position.x, screen_pos.y + display_size.y - 18);

        ImVec2 rect_pos =
            ImVec2(mouse_position.x + 50, screen_pos.y + display_size.y - 5);
        draw_list->AddRectFilled(text_pos, rect_pos, IM_COL32(0, 0, 0, 255));
        draw_list->AddText(text_pos, IM_COL32(255, 255, 255, 255), text);
        draw_list->AddLine(ImVec2(mouse_position.x, screen_pos.y),
                           ImVec2(mouse_position.x, screen_pos.y + display_size.y - 18),
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

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(220, 0, 0, 0));

    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    m_grid.RenderGrid(m_min_x, m_max_x, m_movement, m_zoom, draw_list, m_scale_x,
                      m_v_max_x, m_v_min_x, m_grid_size);

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
    ImGui::SetNextWindowSize(ImVec2(display_size.x, display_size.y - m_grid_size),
                             ImGuiCond_Always);
    ImGui::SetCursorPos(ImVec2(0, 0));

    // overlayed windows need to have fully trasparent bg otherwise they will overlay
    // (with no alpha) over their predecessors
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
    ImGui::BeginChild("Graph View Main", ImVec2(0, 0), false, window_flags);
    ImGuiIO& io           = ImGui::GetIO();
    m_is_control_held     = io.KeyCtrl;
    m_content_max_y_scoll = ImGui::GetScrollMaxY();

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

            ImVec4 selection_color = ImVec4(0, 0, 0, 0);
            if(graph_objects.second.selected == true)
            {
                selection_color = ImVec4(0.17, 0.54, 1.0f, 0.3f);
            }
            ImGui::PushStyleColor(ImGuiCol_ChildBg, selection_color);
            ImGui::BeginChild(
                (std::to_string(graph_objects.first)).c_str(),
                ImVec2(0,
                       m_graph_map[graph_objects.first].chart->GetTrackHeight() + 40.0f),
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

            if(m_graph_map[graph_objects.first].chart->GetVisibility() == true ||
               (m_graph_map[graph_objects.first].chart->GetVisibility() == false ||
                m_graph_map[graph_objects.first].chart->GetMovement() < 1000))
            {
                // If the graph can be seen or is less than 1000 units away update
                // movements.
                m_graph_map[graph_objects.first].chart->UpdateMovement(
                    m_zoom, m_movement, m_min_x, m_max_x, m_scale_x, m_scroll_position);
            }

            m_graph_map[graph_objects.first]
                .chart->Render();  // Always render for now. Update component later on and
                                   // move into if above.

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
MainView::DestroyGraphs()
{
    for(const auto& graph_object : m_graph_map)
    {
        if(graph_object.second.chart)
        {
            delete graph_object.second.chart;
        }
    }

    m_graph_map.clear();
    m_meta_map_made = false;
}

void
MainView::MakeGraphView()
{
    // Destroy any existing data
    DestroyGraphs();
    ResetView();

    /*This section makes the charts both line and flamechart are constructed here*/
    uint64_t num_graphs = m_data_provider.GetTrackCount();
    int      scale_x    = 1;
    for(uint64_t i = 0; i < num_graphs; i++)
    {
        const track_info_t* track_info = m_data_provider.GetTrackInfo(i);
        if(!track_info)
        {
            // log warning (should this be an error?)
            spdlog::warn("Missing track meta data for track at index {}", i);
            continue;
        }

        switch(track_info->track_type)
        {
            case kRPVControllerTrackTypeEvents:
            {
                // Create FlameChart
                FlameChart* flame =
                    new FlameChart(track_info->index, track_info->name, m_zoom,
                                   m_movement, m_min_x, m_max_x, scale_x);

                std::tuple<float, float> temp_min_max_flame =
                    std::tuple<float, float>(static_cast<float>(track_info->min_ts),
                                             static_cast<float>(track_info->max_ts));

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
                temp_flame.color_by_value = false;
                temp_flame.selected       = false;
                rocprofvis_color_by_value_t temp_color = {};
                temp_flame.color_by_value_digits       = temp_color;
                m_graph_map[track_info->index]         = temp_flame;

                break;
            }
            case kRPVControllerTrackTypeSamples:
            {
                // Linechart
                LineChart* line =
                    new LineChart(track_info->index, track_info->name, m_zoom, m_movement,
                                  m_min_x, m_max_x, m_scale_x);

                std::tuple<float, float> temp_min_max_flame =
                    std::tuple<float, float>(static_cast<float>(track_info->min_ts),
                                             static_cast<float>(track_info->max_ts));

                if(std::get<0>(temp_min_max_flame) < m_min_x)
                {
                    m_min_x = std::get<0>(temp_min_max_flame);
                }
                if(std::get<1>(temp_min_max_flame) > m_max_x)
                {
                    m_max_x = std::get<1>(temp_min_max_flame);
                }

                rocprofvis_graph_map_t temp;
                temp.chart          = line;
                temp.graph_type     = rocprofvis_graph_map_t::TYPE_LINECHART;
                temp.display        = true;
                temp.color_by_value = false;
                temp.selected       = false;
                rocprofvis_color_by_value_t temp_color_line = {};
                temp.color_by_value_digits                  = temp_color_line;
                m_graph_map[track_info->index]              = temp;

                break;
            }
            default:
            {
                break;
            }
        }

        // TODO: Quick hack Fetch all tracks for now... in future use event system to
        // decide what / when to fetch

        // if(i < 10)
        {
            m_data_provider.FetchTrack(i, m_data_provider.GetStartTime(),
                                       m_data_provider.GetEndTime(), 1000, 0);
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
        ImVec2 subcomponent_size_main = ImGui::GetWindowSize();

        ImGui::BeginChild(
            "Grid View 2", ImVec2(subcomponent_size_main.x, subcomponent_size_main.y),
            false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

        ImVec2 graph_view_size = ImGui::GetContentRegionAvail();

        // Scale used in all graphs computer here.
        m_v_width = (m_max_x - m_min_x) / m_zoom;
        m_v_min_x = m_min_x + m_movement;
        m_v_max_x = m_v_min_x + m_v_width;
        m_scale_x = graph_view_size.x / (m_v_max_x - m_v_min_x);

        if(m_capture_og_v_max_x)
        {
            m_original_v_max_x   = m_v_max_x;  // Used to set bounds
            m_capture_og_v_max_x = false;
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
            RenderScrubber(screen_pos);
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
        ImVec2 container_pos  = ImGui::GetWindowPos();
        ImVec2 container_size = ImGui::GetWindowSize();

        bool is_mouse_inside = ImGui::IsMouseHoveringRect(
            container_pos, ImVec2(container_pos.x + container_size.x,
                                  container_pos.y + container_size.y));

        if(is_mouse_inside)
        {
            // Handle Zoom
            float scroll_wheel = ImGui::GetIO().MouseWheel;
            if(scroll_wheel != 0.0f)
            {
                float       view_width = (m_max_x - m_min_x) / m_zoom;
                const float zoom_speed = 0.1f;
                m_zoom *= (scroll_wheel > 0) ? (1.0f + zoom_speed) : (1.0f - zoom_speed);
                m_zoom = m_zoom;
                m_zoom = clamp(m_zoom, 0.9f, 200.0f);
                m_movement += m_v_width - ((m_max_x - m_min_x) / m_zoom);
                m_v_width = (m_max_x - m_min_x) / m_zoom;
                m_v_min_x = m_min_x + m_movement;
                m_v_max_x = m_v_min_x + m_v_width;
            }

            // Detect drag start
            if(ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            {
                m_can_drag_to_pan = true;
            }
        }

        if(ImGui::IsMouseReleased(ImGuiMouseButton_Left))
        {
            m_can_drag_to_pan = false;
        }

        // Handle Panning
        if(m_can_drag_to_pan && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
        {
            float drag_y      = ImGui::GetIO().MouseDelta.y;
            m_scroll_position = m_scroll_position - drag_y, 0.0f, m_content_max_y_scoll;

            float drag       = ImGui::GetIO().MouseDelta.x;
            float view_width = (m_max_x - m_min_x) / m_zoom;

            // Left side
            if((drag / ImGui::GetContentRegionAvail().x) * view_width < 0)
            {
                m_movement -= (drag / ImGui::GetContentRegionAvail().x) * view_width;
            }

            // Right side
            if((drag / ImGui::GetContentRegionAvail().x) * view_width > 0)
            {
                m_movement -= (drag / ImGui::GetContentRegionAvail().x) * view_width;
            }

            else
            {
                m_movement -= (drag / ImGui::GetContentRegionAvail().x) * view_width;
            }
        }
    }
}

}  // namespace View
}  // namespace RocProfVis
