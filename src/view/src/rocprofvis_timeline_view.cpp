// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_timeline_view.h"
#include "imgui.h"
#include "rocprofvis_controller.h"
#include "rocprofvis_core_assert.h"
#include "rocprofvis_flame_track_item.h"
#include "rocprofvis_line_track_item.h"
#include "rocprofvis_settings.h"
#include "rocprofvis_utils.h"
#include "spdlog/spdlog.h"
#include "widgets/rocprofvis_debug_window.h"
#include <GLFW/glfw3.h>
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

constexpr double INVALID_SELECTION_TIME = std::numeric_limits<double>::lowest();

TimelineView::TimelineView(DataProvider& dp)
: m_data_provider(dp)
, m_zoom(1.0f)
, m_view_time_offset_ns(0.0f)
, m_min_x(std::numeric_limits<double>::max())
, m_max_x(std::numeric_limits<double>::lowest())
, m_min_y(std::numeric_limits<double>::max())
, m_max_y(std::numeric_limits<double>::lowest())
, m_scroll_position(0.0f)
, m_content_max_y_scoll(0.0f)
, m_scrubber_position(0.0f)
, m_v_min_x(0.0f)
, m_v_max_x(0.0f)
, m_pixels_per_ns(0.0f)
, m_stop_zooming(false)
, m_meta_map_made(false)
, m_previous_scroll_position(0.0f)
, m_graph_map({})
, m_is_control_held(false)
, m_original_v_max_x(0.0f)
, m_grid_size(50)
, m_unload_track_distance(1000.0f)
, m_sidebar_size(400)
, m_resize_activity(false)
, m_scroll_position_x(0)
, m_scrollbar_location_as_percentage(0)
, m_artifical_scrollbar_active(false)
, m_highlighted_region({ INVALID_SELECTION_TIME, INVALID_SELECTION_TIME })
, m_new_track_token(-1)
, m_settings(Settings::GetInstance())
, m_v_past_width(0)
, m_v_width(0)
, m_viewport_past_position(0)
, m_graph_size()
, m_range_x(0.0f)
, m_can_drag_to_pan(false)
, m_region_selection_changed(false)
, m_artificial_scrollbar_size(30)
{
    auto new_track_data_handler = [this](std::shared_ptr<RocEvent> e) {
        this->HandleNewTrackData(e);
    };
    m_new_track_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kNewTrackData), new_track_data_handler);
}

int
TimelineView::FindTrackIdByName(const std::string& name)
{
    for(const auto& pair : m_graph_map)
    {
        if(pair.second.chart && pair.second.chart->GetName() == name)
        {
            return pair.first;
        }
    }
    return -1;
}

float
TimelineView::CalculateTrackOffsetY(int chart_id)
{
    float offset = 0.0f;
    for(const auto& pair : m_graph_map)
    {
        if(pair.first == chart_id) break;
        if(pair.second.display && pair.second.chart)
        {
            offset += pair.second.chart->GetTrackHeight();
        }
    }
    return offset;
}
void
TimelineView::ScrollToTrack(uint64_t position)
{
    float offset      = CalculateTrackOffsetY(position);
    m_scroll_position = offset;
    ImGui::SetScrollY(m_scroll_position);
}

void
TimelineView::ScrollToTrackByName(const std::string& name)
{
    int chart_id = FindTrackIdByName(name);
    if(chart_id == -1) return;

    float offset      = CalculateTrackOffsetY(chart_id);
    m_scroll_position = offset;
    ImGui::SetScrollY(m_scroll_position);
}
void
TimelineView::SetViewTimePosition(double time_pos_ns, bool center)
{
    if(center)
    {
        // Center the movement value in the current view
        // m_v_width is the width of the visible window in timeline units
        m_view_time_offset_ns = time_pos_ns - (m_v_width / 2.0);
    }
    else
    {
        m_view_time_offset_ns = time_pos_ns;
    }
}
TimelineView::~TimelineView()
{
    DestroyGraphs();
    EventManager::GetInstance()->Unsubscribe(static_cast<int>(RocEvents::kNewTrackData),
                                             m_new_track_token);
}

void
TimelineView::CalibratePosition()
{
    m_scroll_position_x =
        (m_view_time_offset_ns) / (m_range_x);  // Finds where the chart is at.
    double scrollback = (m_range_x) *m_scroll_position_x;

    if(m_artifical_scrollbar_active == true)
    {
        double value_to_begginging =
            m_view_time_offset_ns - scrollback;  // how to get back to initial/first value
                                                 // accounting for current movement.
        m_view_time_offset_ns =
            value_to_begginging +
            ((m_range_x) *m_scrollbar_location_as_percentage);  // initial/first value +
                                                                // position where
                                                                // scrollbar is.
    }
}

void
TimelineView::ResetView()
{
    m_zoom                = 1.0f;
    m_view_time_offset_ns = 0.0f;
    m_min_x               = std::numeric_limits<double>::max();
    m_max_x               = std::numeric_limits<double>::lowest();
    m_min_y               = std::numeric_limits<double>::max();
    m_max_y               = std::numeric_limits<double>::lowest();
    m_scroll_position     = 0.0f;
    m_scrubber_position   = 0.0f;
    m_v_min_x             = 0.0f;
    m_v_max_x             = 0.0f;
    m_pixels_per_ns       = 0.0f;
    m_original_v_max_x    = 0.0f;
}

void
TimelineView::HandleNewTrackData(std::shared_ptr<RocEvent> e)
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
        uint64_t           track_index = tde->GetTrackIndex();
        const std::string& trace_path  = tde->GetTracePath();

        if(m_data_provider.GetTraceFilePath() != trace_path)
        {
            spdlog::debug("Trace path {} does not match current trace path {}",
                          trace_path, m_data_provider.GetTraceFilePath());
            return;
        }

        m_graph_map[track_index].chart->HandleTrackDataChanged();
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
        m_view_time_offset_ns -=
            (drag_delta.x / display_size.x) *
            m_v_width;  // Prevents chart from moving in unexpected way.
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

    // Horizontal Splitter
    ImGui::SetNextWindowSize(ImVec2(display_size.x, 1.0f), ImGuiCond_Always);
    ImGui::SetCursorPos(ImVec2(m_sidebar_size, m_graph_size.y - m_grid_size -
                                                   m_artificial_scrollbar_size));

    ImGui::PushStyleColor(ImGuiCol_ChildBg,
                          m_settings.GetColor(static_cast<int>(Colors::kScrollBarColor)));

    ImGui::BeginChild("Splitter View Horizontal", ImVec2(0, 0), ImGuiChildFlags_None,
                      window_flags);

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
    ImGui::SetNextWindowSize(m_graph_size, ImGuiCond_Always);
    ImGui::SetCursorPos(
        ImVec2(m_sidebar_size, 0));  // Meta Data size will be universal next PR.

    // overlayed windows need to have fully trasparent bg otherwise they will overlay
    // (with no alpha) over their predecessors
    ImGui::PushStyleColor(ImGuiCol_ChildBg,
                          m_settings.GetColor(static_cast<int>(Colors::kTransparent)));

    ImGui::BeginChild("Scrubber View", ImVec2(0, 0), ImGuiChildFlags_None, window_flags);

    ImGui::SetItemAllowOverlap();

    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    ImVec2 window_position = ImGui::GetWindowPos();
    ImVec2 mouse_position  = ImGui::GetMousePos();

    ImVec2 relativeMousePos = ImVec2(mouse_position.x - window_position.x,
                                     mouse_position.y - window_position.y);

    // IsMouseHoveringRect check in screen coordinates
    if(ImGui::IsMouseHoveringRect(
           window_position, ImVec2(window_position.x + m_graph_size.x - scrollbar_width,
                                   window_position.y + m_graph_size.y)))
    {
        ImVec2 mouse_position = ImGui::GetMousePos();

        ImVec2 containerPos = ImGui::GetWindowPos();

        float cursor_screen_percentage =
            (mouse_position.x - window_position.x) / m_graph_size.x;
        char   text[20];
        double scrubber_position =
            m_view_time_offset_ns + (cursor_screen_percentage * m_v_width);

        sprintf(text, "%.0f", scrubber_position);
        ImVec2 text_pos = ImVec2(mouse_position.x, screen_pos.y + display_size.y - 28);

        ImVec2 rect_pos =
            ImVec2(mouse_position.x + 100 * Settings::GetInstance().GetDPI(),
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
            if(m_highlighted_region.first == INVALID_SELECTION_TIME)
            {
                m_highlighted_region.first =
                    m_view_time_offset_ns + (cursor_screen_percentage * m_v_width);
            }
            else if(m_highlighted_region.second == INVALID_SELECTION_TIME)
            {
                m_highlighted_region.second =
                    m_view_time_offset_ns + (cursor_screen_percentage * m_v_width);
                m_region_selection_changed = true;
            }
            else
            {
                m_highlighted_region.first  = INVALID_SELECTION_TIME;
                m_highlighted_region.second = INVALID_SELECTION_TIME;
                m_region_selection_changed  = true;
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
TimelineView::RenderGrid()
{
    /*This section makes the grid for the charts*/

    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                                    ImGuiWindowFlags_NoScrollWithMouse;

    ImGui::SetNextWindowSize(m_graph_size, ImGuiCond_Always);

    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    ImVec2 container_pos =
        ImVec2(ImGui::GetWindowPos().x + m_sidebar_size, ImGui::GetWindowPos().y);

    int    ruler_size      = 30;
    ImVec2 cursor_position = ImGui::GetCursorScreenPos();
    ImVec2 content_size    = ImVec2(m_graph_size.x, m_graph_size.y - ruler_size);
    double range =
        (m_v_max_x + m_view_time_offset_ns) - (m_v_min_x + m_view_time_offset_ns);

    double stepSize = 0;
    double steps    = 0;
    {
        char label[32];
        snprintf(label, sizeof(label), "%.0f", m_max_x);
        ImVec2 labelSize = ImGui::CalcTextSize(label);

        // amount the loop which generates the grid iterates by.
        steps = m_graph_size.x / labelSize.x;

        stepSize = labelSize.x;
    }

    ImGui::SetCursorPos(ImVec2(m_sidebar_size, 0));

    if(ImGui::BeginChild("Grid"), ImVec2(m_graph_size.x, m_graph_size.y - ruler_size),
       true, window_flags)
    {
        ImGui::SetCursorPos(ImVec2(0, 0));

        ImGui::BeginChild("main component",
                          ImVec2(m_graph_size.x, m_graph_size.y - ruler_size), false);
        ImVec2 child_win  = ImGui::GetWindowPos();
        ImVec2 child_size = ImGui::GetWindowSize();

        // Define the clipping rectangle to match the child window
        ImVec2 clip_min = child_win;
        ImVec2 clip_max =
            ImVec2(child_win.x + content_size.x, child_win.y + content_size.y);
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        draw_list->PushClipRect(clip_min, clip_max, true);

        double normalized_start_box =
            container_pos.x +
            (m_min_x - (m_min_x + m_view_time_offset_ns)) * m_pixels_per_ns;

        if(m_highlighted_region.first != INVALID_SELECTION_TIME)
        {
            double normalized_start_box_highlighted =
                container_pos.x +
                (m_highlighted_region.first - m_view_time_offset_ns) * m_pixels_per_ns;

            draw_list->AddLine(
                ImVec2(normalized_start_box_highlighted, cursor_position.y),
                ImVec2(normalized_start_box_highlighted,
                       cursor_position.y + content_size.y - m_grid_size),
                m_settings.GetColor(static_cast<int>(Colors::kSelectionBorder)), 3.0f);
        }
        if(m_highlighted_region.first != INVALID_SELECTION_TIME)
        {
            double normalized_start_box_highlighted_end =
                container_pos.x +
                (m_highlighted_region.second - m_view_time_offset_ns) * m_pixels_per_ns;

            draw_list->AddLine(
                ImVec2(normalized_start_box_highlighted_end, cursor_position.y),
                ImVec2(normalized_start_box_highlighted_end,
                       cursor_position.y + content_size.y - m_grid_size),
                m_settings.GetColor(static_cast<int>(Colors::kSelectionBorder)), 3.0f);
        }
        if(m_highlighted_region.first != INVALID_SELECTION_TIME &&
           m_highlighted_region.second != INVALID_SELECTION_TIME)
        {
            double normalized_start_box_highlighted =
                container_pos.x +
                (m_highlighted_region.first - m_view_time_offset_ns) * m_pixels_per_ns;
            double normalized_start_box_highlighted_end =
                container_pos.x +
                (m_highlighted_region.second - m_view_time_offset_ns) * m_pixels_per_ns;
            draw_list->AddRectFilled(
                ImVec2(normalized_start_box_highlighted, cursor_position.y),
                ImVec2(normalized_start_box_highlighted_end,
                       cursor_position.y + content_size.y - m_grid_size),
                m_settings.GetColor(static_cast<int>(Colors::kSelection)));
        }

        int    rectangle_render_count = 0;
        double m_v_width              = (m_max_x - m_min_x) / m_zoom;

        double x_offset = (m_view_time_offset_ns / m_v_width) * m_graph_size.x;
        x_offset        = (int) x_offset % (int) stepSize;

        for(float i = 0; i < steps + 1; i++)
        {
            float linePos = stepSize * i;
            linePos -= x_offset;
            float cursor_screen_percentage = (linePos) / m_graph_size.x;

            double normalized_start = container_pos.x + linePos;

            draw_list->AddLine(ImVec2(normalized_start, cursor_position.y),
                               ImVec2(normalized_start,
                                      cursor_position.y + content_size.y - m_grid_size),
                               m_settings.GetColor(static_cast<int>(Colors::kBoundBox)),
                               0.5f);

            char label[32];
            snprintf(label, sizeof(label), "%.0f",
                     m_view_time_offset_ns + (cursor_screen_percentage * m_v_width));
            // All though the gridlines are drawn based on where they should be on the
            // scale the raw values are used to represent them.
            ImVec2 labelSize = ImGui::CalcTextSize(label);
            ImVec2 labelPos  = ImVec2(normalized_start - labelSize.x / 2,
                                      cursor_position.y + content_size.y - labelSize.y);
            draw_list->AddText(labelPos,
                               m_settings.GetColor(static_cast<int>(Colors::kGridColor)),
                               label);
        }

        draw_list->PopClipRect();
        ImGui::EndChild();  // End of main component
    }
    ImGui::EndChild();  // End of Grid
}

void
TimelineView::RenderGraphView()
{
    CalibratePosition();

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

    ImVec2 window_size             = m_graph_size;
    bool   request_horizontal_data = false;

    if(std::abs(m_view_time_offset_ns - m_viewport_past_position) > m_v_width)
    {
        m_viewport_past_position = m_view_time_offset_ns;
        request_horizontal_data  = true;
    }
    // for zooming out
    if(m_v_width - m_v_past_width > m_v_past_width)
    {
        request_horizontal_data = true;
        m_v_past_width          = m_v_width;
    }
    // zooming in
    else if(m_v_past_width - m_v_width > 0)
    {
        m_v_past_width = m_v_width;
    }

    bool selection_changed = false;
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
                             m_graph_size.y);  // Positive if the track is below the view

            // Save distance for book keeping
            graph_objects.second.chart->SetDistanceToView(
                std::max(std::max(delta_bottom, delta_top), 0.0f));

            // Check if the track is visible
            bool is_visible = (track_bottom >= m_scroll_position &&
                               track_top <= m_scroll_position + m_graph_size.y);

            if(is_visible)
            {
                // Request data for the chart if it doesn't have data
                if(!graph_objects.second.chart->HasData() &&
                   graph_objects.second.chart->GetRequestState() ==
                       TrackDataRequestState::kIdle)
                {
                    double buffer_distance = m_v_width;  // Essentially creates one
                                                         // viewport worth of buffer.
                    graph_objects.second.chart->RequestData(
                        (m_view_time_offset_ns - buffer_distance) + m_min_x,
                        (m_view_time_offset_ns + m_v_width + buffer_distance) + m_min_x,
                        m_graph_size.x * 3);
                    request_horizontal_data =
                        true;  // This is here because as new tracks are loaded all
                               // graphs should have data to fill the viewport.
                }
                if(m_settings.IsHorizontalRender())
                {
                    if(request_horizontal_data &&
                       graph_objects.second.chart->GetRequestState() ==
                           TrackDataRequestState::kIdle)
                    {
                        double buffer_distance = m_v_width;  // Essentially creates one
                                                             // viewport worth of buffer.

                        graph_objects.second.chart->RequestData(
                            (m_view_time_offset_ns - buffer_distance) + m_min_x,
                            (m_view_time_offset_ns + m_v_width + buffer_distance) +
                                m_min_x,
                            m_graph_size.x * 3);
                    }
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
                                m_graph_map[graph_objects.first];  // outgoing
                                                                   // (getting
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
                    m_zoom, m_view_time_offset_ns, m_min_x, m_max_x, m_pixels_per_ns,
                    m_scroll_position);

                m_resize_activity |= graph_objects.second.chart->GetResizeStatus();
                graph_objects.second.chart->Render(m_graph_size.x);

                // check for mouse click
                if(graph_objects.second.chart->IsMetaAreaClicked())
                {
                    m_graph_map[graph_objects.second.chart->GetID()].selected =
                        !graph_objects.second.selected;
                    selection_changed = true;
                }

                ImGui::PopStyleColor();

                ImGui::EndChild();
                ImGui::PopStyleColor();

                ImGui::Separator();
            }
            else
            {
                // If the track is not visible past a certain distance, release its
                // data to free up memory
                if(graph_objects.second.chart->GetDistanceToView() >
                       m_unload_track_distance &&
                   graph_objects.second.chart->HasData())
                {
                    graph_objects.second.chart->ReleaseData();
                    m_data_provider.FreeTrack(graph_objects.second.chart->GetID());
                }

                // Render dummy to maintain layout
                ImGui::Dummy(ImVec2(0, track_height));
            }
        }
    }

    TrackItem::SetSidebarSize(m_sidebar_size);
    ImGui::EndChild();
    ImGui::PopStyleColor();

    if(selection_changed || m_region_selection_changed)
    {
        m_region_selection_changed = false;
        std::vector<uint64_t> selected_graphs;
        for(const auto& graph_objects : m_graph_map)
        {
            if(graph_objects.second.selected)
            {
                selected_graphs.push_back(graph_objects.first);
            }
        }

        double start_ns = m_min_x;
        double end_ns   = m_max_x;
        if(m_highlighted_region.first != INVALID_SELECTION_TIME &&
           m_highlighted_region.second != INVALID_SELECTION_TIME)
        {
            start_ns = std::min(m_highlighted_region.first, m_highlighted_region.second) +
                       m_min_x;
            end_ns = std::max(m_highlighted_region.first, m_highlighted_region.second) +
                     m_min_x;
        }

        // notify the event manager of the section change
        std::shared_ptr<TrackSelectionChangedEvent> e =
            std::make_shared<TrackSelectionChangedEvent>(
                static_cast<int>(RocEvents::kTimelineSelectionChanged), std::move(selected_graphs),
                start_ns, end_ns);
        EventManager::GetInstance()->AddEvent(e);
    }
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

    m_min_x   = m_data_provider.GetStartTime();
    m_max_x   = m_data_provider.GetEndTime();
    m_range_x = m_max_x - m_min_x;

    m_v_width      = (m_range_x) / m_zoom;
    m_v_past_width = m_v_width;

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
                    m_view_time_offset_ns, m_min_x, m_max_x, scale_x);

                std::tuple<float, float> temp_min_max_flame =
                    std::tuple<float, float>(static_cast<float>(track_info->min_ts),
                                             static_cast<float>(track_info->max_ts));

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
                    m_view_time_offset_ns, m_min_x, m_max_x, m_pixels_per_ns);

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
        ImVec2 screen_pos             = ImGui::GetCursorScreenPos();
        ImVec2 subcomponent_size_main = ImGui::GetWindowSize();

        m_graph_size =
            ImVec2(subcomponent_size_main.x - m_sidebar_size, subcomponent_size_main.y);

        ImGui::BeginChild(
            "Grid View 2",
            ImVec2(subcomponent_size_main.x,
                   subcomponent_size_main.y - m_artificial_scrollbar_size),
            false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

        // Scale used in all graphs computer here.
        m_v_width       = (m_range_x) / m_zoom;
        m_v_min_x       = m_min_x + m_view_time_offset_ns;
        m_v_max_x       = m_v_min_x + m_v_width;
        m_pixels_per_ns = (m_graph_size.x) / (m_v_max_x - m_v_min_x);

        if(m_pixels_per_ns > 1)
        {
            m_stop_zooming = true;
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
                          ImVec2(subcomponent_size_main.x, m_artificial_scrollbar_size),
                          true, ImGuiWindowFlags_NoScrollbar);

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

        ImGui::Dummy(ImVec2(m_sidebar_size, 0));
        ImGui::SameLine();
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
    if(!m_is_control_held)
    {
        ImVec2 container_pos  = ImGui::GetWindowPos();
        ImVec2 container_size = ImGui::GetWindowSize();

        bool is_mouse_inside = ImGui::IsMouseHoveringRect(
            container_pos, ImVec2(container_pos.x + container_size.x,
                                  container_pos.y + container_size.y));

        if(is_mouse_inside)
        {
            if(ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            {
                m_can_drag_to_pan = true;
            }

            // Handle Zoom at Cursor
            float scroll_wheel = ImGui::GetIO().MouseWheel;
            if(scroll_wheel != 0.0f)
            {
                // 1. Get mouse position relative to graph area
                ImVec2 mouse_pos        = ImGui::GetMousePos();
                ImVec2 graph_pos        = container_pos;
                float  mouse_x_in_graph = mouse_pos.x - graph_pos.x - m_sidebar_size;

                // 2. Calculate the world coordinate under the cursor before zoom
                float  cursor_screen_percentage = mouse_x_in_graph / m_graph_size.x;
                double x_under_cursor =
                    m_view_time_offset_ns + cursor_screen_percentage * m_v_width;

                // 3. Apply zoom
                const float zoom_speed = 0.1f;
                float       new_zoom   = m_zoom;
                if(scroll_wheel > 0)
                {
                    if(m_pixels_per_ns < 1.0) new_zoom *= 1.0f + zoom_speed;
                }
                else
                {
                    new_zoom *= 1.0f - zoom_speed;
                }
                new_zoom = std::max(new_zoom, 0.9f);

                // 4. Calculate new view width
                float new_v_width = m_range_x / new_zoom;

                // 5. Adjust m_movement so the world_x_under_cursor stays under the cursor
                m_view_time_offset_ns =
                    x_under_cursor - cursor_screen_percentage * new_v_width;

                // 6. Update zoom and view width
                m_zoom    = new_zoom;
                m_v_width = new_v_width;
                m_v_min_x = m_min_x + m_view_time_offset_ns;
                m_v_max_x = m_v_min_x + m_v_width;
            }

            if(ImGui::IsKeyPressed(ImGuiKey_LeftArrow))
            {
                m_view_time_offset_ns -= (1 / m_graph_size.x) * m_v_width;
            }
            if(ImGui::IsKeyPressed(ImGuiKey_RightArrow))
            {
                m_view_time_offset_ns -= (-1 / m_graph_size.x) * m_v_width;
            }
        }

        if(ImGui::IsMouseReleased(ImGuiMouseButton_Left))
        {
            m_can_drag_to_pan = false;
        }

        // Handle Panning (unchanged)
        if(m_can_drag_to_pan && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
        {
            float drag_y = ImGui::GetIO().MouseDelta.y;
            m_scroll_position =
                clamp(m_scroll_position - drag_y, 0.0, m_content_max_y_scoll);
            float drag       = ImGui::GetIO().MouseDelta.x;
            float view_width = (m_range_x) / m_zoom;

            float user_requested_move = (drag / m_graph_size.x) * view_width;

            if(user_requested_move < 0)
            {
                if(m_view_time_offset_ns < (m_range_x))
                {
                    m_view_time_offset_ns -= user_requested_move;
                }
            }
            else
            {
                if(m_view_time_offset_ns > 0)
                {
                    m_view_time_offset_ns -= user_requested_move;
                }
            }
        }
    }
}

}  // namespace View
}  // namespace RocProfVis
