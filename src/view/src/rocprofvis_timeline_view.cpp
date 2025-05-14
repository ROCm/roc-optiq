// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_timeline_view.h"
#include "imgui.h"
#include "rocprofvis_controller.h"
#include "rocprofvis_core_assert.h"
#include "rocprofvis_flame_track_item.h"
#include "rocprofvis_grid.h"
#include "rocprofvis_line_track_item.h"
#include "rocprofvis_settings.h"
#include "rocprofvis_utils.h"
#include "spdlog/spdlog.h"
#include "widgets/rocprofvis_debug_window.h"
#include <GLFW/glfw3.h>
#include <cstdlib>
#include <iostream>
#include <map>
#include <string>
#include <tuple>
#include <utility>
#include <vector>
namespace RocProfVis
{
namespace View
{

TimelineView::TimelineView(DataProvider& dp)
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
, m_unload_track_distance(1000.0f)
, m_sidebar_size(400)
, m_resize_activity(false)
, m_scroll_position_x(FLT_MAX)
, m_calibrated(true)
, m_scrollbar_location_as_percentage(FLT_MIN)
, m_artifical_scrollbar_active(false)
, m_highlighted_region({ -1, -1 })
, m_buffer_left_hit(false)
, m_buffer_right_hit(false)
, m_settings(Settings::GetInstance())
, m_viewport_start(0)
, m_viewport_end(0)
, m_viewport_past_position(0)
{
    m_new_track_data_handler = [this](std::shared_ptr<RocEvent> e) {
        this->HandleNewTrackData(e);
    };
    EventManager::GetInstance()->Subscribe(static_cast<int>(RocEvents::kNewTrackData),
                                           m_new_track_data_handler);
}

TimelineView::~TimelineView()
{
    DestroyGraphs();
    EventManager::GetInstance()->Unsubscribe(static_cast<int>(RocEvents::kNewTrackData),
                                             m_new_track_data_handler);
}

void
TimelineView::CalibratePosition()
{
    double current_position = m_grid.GetViewportStartPosition();
    double end_position     = m_grid.GetViewportEndPosition();

    m_viewport_start = current_position;
    m_viewport_end   = end_position + m_min_x;

    m_scroll_position_x = (current_position - m_min_x) /
                          (m_max_x - m_min_x);  // Finds where the chart is at.

    double scrollback =
        (m_max_x - m_min_x) *
        m_scroll_position_x;  // Moves the graph back to start at the beggining.
                              // Represents how much to go back to starting

    double value_to_begginging =
        m_movement - scrollback;  // how to get back to initial/first value accounting for
                                  // current movement.

    if(m_calibrated)
    {
        // This is used to start the chart at the beggining on initial load.
        m_movement   = m_movement - scrollback;
        m_calibrated = false;
    }
    if(m_artifical_scrollbar_active == true)
    {
        m_movement =
            value_to_begginging +
            ((m_max_x - m_min_x) *
             m_scrollbar_location_as_percentage);  // initial/first value + position where
                                                   // scrollbar is.
    }
}

void
TimelineView::ResetView()
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
TimelineView::HandleNewTrackData(std::shared_ptr<RocEvent> e)
{
    spdlog::debug("new track data event fired!");

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
        uint64_t track_index = tde->GetTrackIndex();
        spdlog::debug("Got new track data for track: {}", track_index);

        if(m_graph_map[track_index].chart->HandleTrackDataChanged())
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
TimelineView::Update()
{
    if(m_meta_map_made)
    {
        // nothing for now
    }
}

void
TimelineView::Render()
{
    m_resize_activity = false;
    if(m_meta_map_made)
    {
        RenderGraphPoints();
    }
}

void
TimelineView::RenderSplitter(ImVec2 screen_pos)
{
    // Scrubber Line
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                                    ImGuiWindowFlags_NoScrollWithMouse;

    ImVec2 display_size = ImGui::GetWindowSize();

    ImGui::SetNextWindowSize(ImVec2(1.0f, display_size.y), ImGuiCond_Always);
    ImGui::SetCursorPos(ImVec2(m_sidebar_size, 0));

    ImGui::PushStyleColor(ImGuiCol_ChildBg,
                          m_settings.GetColor(static_cast<int>(Colors::kScrollBarColor)));

    ImGui::BeginChild("Splitter View", ImVec2(0, 0), ImGuiChildFlags_None, window_flags);

    ImGui::Selectable("##MovePositionLineVert", false,
                      ImGuiSelectableFlags_AllowDoubleClick,
                      ImVec2(5.0f, display_size.y));

    if(ImGui::IsItemHovered())
    {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        m_resize_activity |= true;
    }

    if(ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
    {
        ImVec2 drag_delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);
        m_sidebar_size    = clamp(m_sidebar_size + drag_delta.x, 100.0f, 600.0f);
        ImGui::ResetMouseDragDelta();
        ImGui::EndDragDropSource();
        m_resize_activity |= true;
    }
    if(ImGui::BeginDragDropTarget())
    {
        ImGui::EndDragDropTarget();
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void
TimelineView::RenderScrubber(ImVec2 screen_pos)
{
    // Scrubber Line
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                                    ImGuiWindowFlags_NoScrollWithMouse |
                                    ImGuiWindowFlags_NoInputs;

    ImVec2 display_size    = ImGui::GetWindowSize();
    float  scrollbar_width = ImGui::GetStyle().ScrollbarSize;
    ImGui::SetNextWindowSize(ImVec2(display_size.x - scrollbar_width, display_size.y),
                             ImGuiCond_Always);
    ImGui::SetCursorPos(
        ImVec2(m_sidebar_size, 0));  // Meta Data size will be universal next PR.

    // overlayed windows need to have fully trasparent bg otherwise they will overlay
    // (with no alpha) over their predecessors
    ImGui::PushStyleColor(ImGuiCol_ChildBg,
                          m_settings.GetColor(static_cast<int>(Colors::kTransparent)));

    ImGui::BeginChild("Scrubber View", ImVec2(0, 0), ImGuiChildFlags_None, window_flags);

    ImGui::SetItemAllowOverlap();

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

        ImVec2 containerPos = ImGui::GetWindowPos();

        char text[20];
        sprintf(text, "%.0f",
                m_grid.GetCursorPosition(mouse_position.x - containerPos.x + 2));
        ImVec2 text_pos = ImVec2(mouse_position.x, screen_pos.y + display_size.y - 28);

        ImVec2 rect_pos = ImVec2(mouse_position.x + 50 * Settings::GetInstance().GetDPI(),
                                 screen_pos.y + display_size.y - 5);
        draw_list->AddRectFilled(
            text_pos, rect_pos,
            m_settings.GetColor(static_cast<int>(Colors::kGridColor)));
        draw_list->AddText(
            text_pos, m_settings.GetColor(static_cast<int>(Colors::kFillerColor)), text);
        draw_list->AddLine(ImVec2(mouse_position.x, screen_pos.y),
                           ImVec2(mouse_position.x, screen_pos.y + display_size.y - 28),
                           m_settings.GetColor(static_cast<int>(Colors::kGridColor)),
                           2.0f);

        // Code below is for select
        if(ImGui::IsMouseDoubleClicked(0))
        {  // 0 is for the left mouse button
            if(m_highlighted_region.first == -1)
            {
                m_highlighted_region.first =
                    m_grid.GetCursorPosition(mouse_position.x - containerPos.x);
                m_grid.SetHighlightedRegion(m_highlighted_region);
            }
            else if(m_highlighted_region.second == -1)
            {
                m_highlighted_region.second =
                    m_grid.GetCursorPosition(mouse_position.x - containerPos.x);
                m_grid.SetHighlightedRegion(m_highlighted_region);
            }
            else
            {
                m_highlighted_region.first  = -1;
                m_highlighted_region.second = -1;
                m_grid.SetHighlightedRegion(m_highlighted_region);
            }
        }
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

std::map<int, rocprofvis_graph_map_t>*
TimelineView::GetGraphMap()
{
    return &m_graph_map;
}

void
TimelineView::RenderGrid(float width)
{
    /*This section makes the grid for the charts*/

    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                                    ImGuiWindowFlags_NoScrollWithMouse;

    ImVec2 display_size = ImGui::GetWindowSize();

    ImGui::SetNextWindowSize(ImVec2(display_size.x, display_size.y), ImGuiCond_Always);

    ImGui::SetCursorPos(ImVec2(0, 0));

    ImGui::PushStyleColor(ImGuiCol_ChildBg,
                          m_settings.GetColor(static_cast<int>(Colors::kTransparent)));

    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    m_grid.RenderGrid(m_min_x, m_max_x, m_movement, m_zoom, m_scale_x, m_v_max_x,
                      m_v_min_x, m_grid_size, m_sidebar_size);

    ImGui::PopStyleColor();
}

void
TimelineView::RenderGraphCustomizationWindow(int graph_id)
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
TimelineView::RenderGraphView()
{
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                                    ImGuiWindowFlags_NoScrollWithMouse;

    ImVec2 display_size = ImGui::GetWindowSize();
    ImGui::SetNextWindowSize(ImVec2(display_size.x, display_size.y - m_grid_size),
                             ImGuiCond_Always);
    ImGui::SetCursorPos(ImVec2(0, 0));

    // overlayed windows need to have fully trasparent bg otherwise they will overlay
    // (with no alpha) over their predecessors
    ImGui::PushStyleColor(ImGuiCol_ChildBg,
                          m_settings.GetColor(static_cast<int>(Colors::kTransparent)));
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

    ImVec2 window_size = ImGui::GetWindowSize();

    DebugWindow::GetInstance()->AddDebugMessage(
        "Window Height: " + std::to_string(window_size.y) +
        "Parent Height: " + std::to_string(display_size.y) +
        " Scroll Position: " + std::to_string(m_scroll_position) +
        " Content Max Y: " + std::to_string(m_content_max_y_scoll) +
        " Previous Scroll Position: " + std::to_string(m_previous_scroll_position));
    bool request = false;

    if(std::abs(m_movement - m_viewport_past_position) > m_v_width * 0.50)
    {
        m_viewport_past_position = m_movement;
        request                  = true;
        std::cout << "ran" << std::endl;
    }

    for(const auto& graph_objects : m_graph_map)
    {
        if(graph_objects.second.display)
        {
            // Get track height and position to check if the track is in view
            float  track_height = graph_objects.second.chart->GetTrackHeight();
            ImVec2 track_pos    = ImGui::GetCursorPos();

            // Calculate the track's position in the scrollable area
            float track_top    = track_pos.y;
            float track_bottom = track_top + track_height;

            // Calculate deltas for out-of-view tracks
            float delta_top = m_scroll_position -
                              track_bottom;  // Positive if the track is above the view
            float delta_bottom =
                track_top - (m_scroll_position +
                             window_size.y);  // Positive if the track is below the view

            // Save distance for book keeping
            graph_objects.second.chart->SetDistanceToView(
                std::max(std::max(delta_bottom, delta_top), 0.0f));

            // Check if the track is visible
            bool is_visible = (track_bottom >= m_scroll_position &&
                               track_top <= m_scroll_position + window_size.y);

            if(is_visible)
            {
                if(!graph_objects.second.chart->HasData() &&
                   graph_objects.second.chart->GetRequestState() ==
                       TrackDataRequestState::kIdle)
                {
                    graph_objects.second.chart->RequestData(m_min_x,
                                                            m_max_x);
                    std::cout << "no data " << m_max_x << " "
                              << (int)graph_objects.second.chart->GetRequestState() << std::endl;
                }

                if(request && graph_objects.second.chart->GetRequestState() ==
                                  TrackDataRequestState::kIdle)
                {
                  std::cout << "requested from request" << std::endl;

                 graph_objects.second.chart->RequestData(m_viewport_start,
                                                           m_viewport_end);
              }

                if(ImGui::Button(
                       std::to_string(graph_objects.second.chart->GetID()).c_str()))
                {
                    if(graph_objects.second.chart->HasData())
                    {
                        graph_objects.second.chart->ReleaseData();
                    }
                    graph_objects.second.chart->RequestData(m_viewport_start,
                                                            m_viewport_end);
                }
                if(graph_objects.second.color_by_value)
                {
                    graph_objects.second.chart->SetColorByValue(
                        graph_objects.second.color_by_value_digits);
                }

                if(graph_objects.second.graph_type == graph_objects.second.TYPE_LINECHART)
                {
                    static_cast<LineTrackItem*>(graph_objects.second.chart)
                        ->SetShowBoxplot(graph_objects.second.make_boxplot);
                }

                ImVec4 selection_color = ImGui::ColorConvertU32ToFloat4(
                    m_settings.GetColor(static_cast<int>(Colors::kTransparent)));
                if(graph_objects.second.selected)
                {
                    // TODO: move somewhere else don't need to create each loop
                    selection_color = ImGui::ColorConvertU32ToFloat4(
                        m_settings.GetColor(static_cast<int>(Colors::kHighlightChart)));
                }
                ImGui::PushStyleColor(ImGuiCol_ChildBg, selection_color);
                ImGui::BeginChild(
                    (std::to_string(graph_objects.first)).c_str(),
                    ImVec2(0,
                           track_height + 0.0f),  // TODO: magic number was 40.0f
                    false,
                    ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                        ImGuiWindowFlags_NoScrollWithMouse |
                        ImGuiWindowFlags_NoScrollbar);

                ImGui::PushStyleColor(ImGuiCol_ChildBg,
                                      ImGui::ColorConvertU32ToFloat4(m_settings.GetColor(
                                          static_cast<int>(Colors::kTransparent))));

                if(m_is_control_held)
                {
                    ImGui::Selectable(
                        ("Move Position " + std::to_string(graph_objects.first)).c_str(),
                        false, ImGuiSelectableFlags_AllowDoubleClick, ImVec2(0, 20.0f));

                    if(ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
                    {
                        ImGui::SetDragDropPayload("MY_PAYLOAD_TYPE",
                                                  &graph_objects.second,
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
                            int payload_position = payload_data_copy.chart->GetID();
                            rocprofvis_graph_map_t outgoing_chart =
                                m_graph_map[graph_objects.first];  // outgoing (getting
                                                                   // replaced)
                            int outgoing_position = outgoing_chart.chart->GetID();

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

                graph_objects.second.chart->UpdateMovement(
                    m_zoom, m_movement, m_min_x, m_max_x, m_scale_x, m_scroll_position);

                m_resize_activity |= graph_objects.second.chart->GetResizeStatus();
                graph_objects.second.chart->Render();
                ImGui::PopStyleColor();

                ImGui::EndChild();
                ImGui::PopStyleColor();

                ImGui::Separator();
            }
            else
            {
                // If the track is not visible past a certain distance, release its data
                // to free up memory
                if(graph_objects.second.chart->GetDistanceToView() >
                       m_unload_track_distance &&
                   graph_objects.second.chart->HasData())
                {
                    graph_objects.second.chart->ReleaseData();
                    m_data_provider.FreeTrack(graph_objects.second.chart->GetID());
                }

                // Render dummy to maintain layout
                ImGui::Dummy(ImVec2(0, track_height));
                DebugWindow::GetInstance()->AddDebugMessage(
                    "Dummy for: " + std::to_string(graph_objects.second.chart->GetID()) +
                    " " +
                    std::to_string(graph_objects.second.chart->GetDistanceToView()) +
                    " " + std::to_string(m_scroll_position));
            }
        }
    }
    request = false;

    CalibratePosition();

    // Set the sidebar size at the end of render loop.

    TrackItem::SetSidebarSize(m_sidebar_size);
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void
TimelineView::DestroyGraphs()
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
TimelineView::MakeGraphView()
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
                FlameTrackItem* flame = new FlameTrackItem(
                    m_data_provider, track_info->index, track_info->name, m_zoom,
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
                LineTrackItem* line = new LineTrackItem(
                    m_data_provider, track_info->index, track_info->name, m_zoom,
                    m_movement, m_min_x, m_max_x, m_scale_x);

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
                temp.make_boxplot   = false;
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
    }

    m_meta_map_made = true;
}

void
TimelineView::RenderGraphPoints()
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
    if(ImGui::BeginChild("Main Graphs"))
    {
        ImVec2 screen_pos                = ImGui::GetCursorScreenPos();
        ImVec2 subcomponent_size_main    = ImGui::GetWindowSize();
        int    artificial_scrollbar_size = 30;

        ImGui::BeginChild(
            "Grid View 2",
            ImVec2(subcomponent_size_main.x,
                   subcomponent_size_main.y - artificial_scrollbar_size),
            false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

        ImVec2 graph_view_size = ImGui::GetContentRegionAvail();

        DebugWindow::GetInstance()->AddDebugMessage(
            "graph_view_size: " + std::to_string(graph_view_size.y) +
            " Grid View 2 Height raw: " + std::to_string(subcomponent_size_main.y) +
            " Grid View 2 Height: " + std::to_string(ImGui::GetWindowSize().y));

        // Scale used in all graphs computer here.
        m_v_width = (m_max_x - m_min_x) / m_zoom;

        m_v_min_x = m_min_x + m_movement;
        m_v_max_x = m_v_min_x + m_v_width;
        m_scale_x = (graph_view_size.x - m_sidebar_size) / (m_v_max_x - m_v_min_x);

        if(m_capture_og_v_max_x)
        {
            m_original_v_max_x   = m_v_max_x;  // Used to set bounds
            m_capture_og_v_max_x = false;
        }

        RenderGrid(subcomponent_size_main.x);

        if(m_meta_map_made)
        {
            RenderGraphView();
        }

        ImGuiIO& io       = ImGui::GetIO();
        m_is_control_held = io.KeyCtrl;
        if(!m_is_control_held)
        {
            RenderSplitter(screen_pos);
            RenderScrubber(screen_pos);

            if(m_resize_activity == false)
            {
                HandleTopSurfaceTouch();  // Funtion enables user interactions to be
                                          // captured
            }
        }

        ImGui::EndChild();

        ImGui::PushStyleColor(ImGuiCol_ChildBg, m_settings.GetColor(static_cast<int>(
                                                    Colors::kTransparent)));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));

        ImGui::BeginChild("scrollbar",
                          ImVec2(subcomponent_size_main.x, artificial_scrollbar_size),
                          true, ImGuiWindowFlags_NoScrollbar);

        ImGui::Dummy(ImVec2(m_sidebar_size, 10));
        ImGui::SameLine();

        float current_pos = m_scroll_position_x * (subcomponent_size_main.x * m_zoom);

        ImGuiStyle& style = ImGui::GetStyle();

        float available_width = subcomponent_size_main.x - m_sidebar_size;

        float original_grab_min_size = style.GrabMinSize;
        style.GrabMinSize            = clamp((subcomponent_size_main.x * (1 / m_zoom)),
                                             static_cast<float>(available_width * 0.05),
                                             static_cast<float>(available_width * 0.90));
        style.GrabRounding           = 3.0f;

        ImVec4 scroll_color = ImGui::ColorConvertU32ToFloat4(
            m_settings.GetColor(static_cast<int>(Colors::kFillerColor)));
        ImVec4 grab_color = ImGui::ColorConvertU32ToFloat4(
            m_settings.GetColor(static_cast<int>(Colors::kScrollBarColor)));

        style.Colors[ImGuiCol_SliderGrab]       = grab_color;
        style.Colors[ImGuiCol_SliderGrabActive] = grab_color;
        style.Colors[ImGuiCol_FrameBg]          = scroll_color;
        style.Colors[ImGuiCol_FrameBgHovered]   = scroll_color;
        style.Colors[ImGuiCol_FrameBgActive]    = scroll_color;

        ImGui::PushItemWidth(subcomponent_size_main.x - m_sidebar_size);

        ImGui::SliderFloat("##scrollbar", &current_pos, 0.0f,
                           subcomponent_size_main.x * m_zoom, "%.5f");

        ImGui::PopItemWidth();
        style.GrabMinSize = original_grab_min_size;

        m_scrollbar_location_as_percentage =
            current_pos / (subcomponent_size_main.x * m_zoom);

        if(ImGui::IsItemActive())
        {
            m_artifical_scrollbar_active = true;
        }
        else
        {
            m_artifical_scrollbar_active = false;
            if(m_scrollbar_location_as_percentage > 1)
            {
                m_buffer_right_hit = true;
            }
            else if(m_scrollbar_location_as_percentage < -0.10)
            {
                m_buffer_left_hit = true;
            }
            else
            {
                m_buffer_right_hit = false;
                m_buffer_left_hit  = false;
            }
        }

        ImGui::EndChild();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(2);
    }

    ImGui::EndChild();
    ImGui::PopStyleVar(2);
}

void
TimelineView::HandleTopSurfaceTouch()
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
                m_zoom = std::max(m_zoom, 0.9f);
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
            float drag_y = ImGui::GetIO().MouseDelta.y;
            m_scroll_position =
                clamp(m_scroll_position - drag_y, 0.0, m_content_max_y_scoll);

            float drag       = ImGui::GetIO().MouseDelta.x;
            float view_width = (m_max_x - m_min_x) / m_zoom;

            // Left side
            if((drag / ImGui::GetContentRegionAvail().x) * view_width < 0)
            {
                if(m_buffer_right_hit == false)
                {
                    m_movement -= (drag / ImGui::GetContentRegionAvail().x) * view_width;
                }
            }

            // Right side
            if((drag / ImGui::GetContentRegionAvail().x) * view_width > 0)
            {
                if(m_buffer_left_hit == false)
                {
                    m_movement -= (drag / ImGui::GetContentRegionAvail().x) * view_width;
                }
            }
        }
    }
}

}  // namespace View
}  // namespace RocProfVis
