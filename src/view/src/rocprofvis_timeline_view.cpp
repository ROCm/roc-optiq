// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_timeline_view.h"
#include "imgui.h"
#include "rocprofvis_annotations.h"
#include "rocprofvis_controller.h"
#include "rocprofvis_core_assert.h"
#include "rocprofvis_flame_track_item.h"
#include "rocprofvis_line_track_item.h"
#include "rocprofvis_settings_manager.h"
#include "rocprofvis_timeline_selection.h"
#include "rocprofvis_utils.h"
#include "spdlog/spdlog.h"
#include "widgets/rocprofvis_debug_window.h"

#include <algorithm>
#include <GLFW/glfw3.h>
#include "rocprofvis_font_manager.h"
namespace RocProfVis
{
namespace View
{

// 20% top and bottom of the window size
constexpr float REORDER_AUTO_SCROLL_THRESHOLD = 0.2f;

TimelineView::TimelineView(DataProvider&                       dp,
                           std::shared_ptr<TimelineSelection>  timeline_selection,
                           std::shared_ptr<AnnotationsManager> annotations)
: m_data_provider(dp)
, m_zoom(1.0f)
, m_view_time_offset_ns(0.0f)
, m_min_x(std::numeric_limits<double>::max())
, m_max_x(std::numeric_limits<double>::lowest())
, m_min_y(std::numeric_limits<double>::max())
, m_max_y(std::numeric_limits<double>::lowest())
, m_scroll_position_y(0.0f)
, m_content_max_y_scroll(0.0f)
, m_v_min_x(0.0f)
, m_v_max_x(0.0f)
, m_pixels_per_ns(0.0f)
, m_meta_map_made(false)
, m_previous_scroll_position(0.0f)
, m_ruler_height(ImGui::GetTextLineHeightWithSpacing())
, m_ruler_padding(4)
, m_unload_track_distance(1000.0f)
, m_sidebar_size(400)
, m_resize_activity(false)
, m_highlighted_region({ TimelineSelection::INVALID_SELECTION_TIME,
                         TimelineSelection::INVALID_SELECTION_TIME })
, m_new_track_token(static_cast<uint64_t>(-1))
, m_scroll_to_track_token(static_cast<uint64_t>(-1))
, m_font_changed_token(static_cast<uint64_t>(-1))
, m_set_view_range_token(static_cast<uint64_t>(-1))
, m_settings(SettingsManager::GetInstance())
, m_last_data_req_v_width(0)
, m_v_width(0)
, m_last_data_req_view_time_offset_ns(0)
, m_graph_size()
, m_range_x(0.0f)
, m_can_drag_to_pan(false)
, m_artificial_scrollbar_height(30)
, m_grid_interval_ns(0.0)
, m_recalculate_grid_interval(true)
, m_last_zoom(1.0f)
, m_last_graph_size(0.0f, 0.0f)
, m_reorder_request({ true, 0, 0 })
, m_track_height_sum(0)
, m_arrow_layer(m_data_provider, timeline_selection)
, m_stop_user_interaction(false)
, m_timeline_selection(timeline_selection)
, m_project_settings(m_data_provider.GetTraceFilePath(), *this)
, m_annotations(annotations)
, m_dragged_sticky_id(-1)
, m_histogram(nullptr)
, m_pseudo_focus(false)
, m_histogram_pseudo_focus(false)
{
    auto new_track_data_handler = [this](std::shared_ptr<RocEvent> e) {
        this->HandleNewTrackData(e);
    };
    m_new_track_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kNewTrackData), new_track_data_handler);

    // Used to move to track when tree view clicks on it.
    auto scroll_to_track_handler = [this](std::shared_ptr<RocEvent> e) {
        auto evt = std::dynamic_pointer_cast<ScrollToTrackEvent>(e);
        if(evt)
        {
            if(evt->GetSourceId() == m_data_provider.GetTraceFilePath())
            {
                this->ScrollToTrack(evt->GetTrackID());
            }
        }
    };
    m_scroll_to_track_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kHandleUserGraphNavigationEvent),
        scroll_to_track_handler);

    auto set_view_range_handle = [this](std::shared_ptr<RocEvent> e) {
        auto evt = std::dynamic_pointer_cast<RangeEvent>(e);
        if(evt)
        {
            if(evt->GetSourceId() == m_data_provider.GetTraceFilePath())
            {
                this->SetViewableRangeNS(evt->GetStartNs(), evt->GetEndNs());
            }
        }
    };
    m_set_view_range_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kSetViewRange), set_view_range_handle);

    auto font_changed_handler = [this](std::shared_ptr<RocEvent> e) {
        m_recalculate_grid_interval = true;
        m_ruler_height              = ImGui::GetTextLineHeightWithSpacing();
    };
    m_font_changed_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kFontSizeChanged), font_changed_handler);

    // This is used for navigation from other views like the annotation view.
    auto navigation_handler = [this](std::shared_ptr<RocEvent> e) {
        auto evt = std::dynamic_pointer_cast<NavigationEvent>(e);
        MoveToPosition(evt->GetVMin(), evt->GetVMax(), evt->GetYPosition(),
                       evt->GetCenter());
    };
    m_navigation_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kGoToTimelineSpot), navigation_handler);

    m_graphs = std::make_shared<std::vector<rocprofvis_graph_t>>();
}

void
TimelineView::RenderInteractiveUI(ImVec2 screen_pos)
{
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                                    ImGuiWindowFlags_NoScrollWithMouse |
                                    ImGuiWindowFlags_NoInputs;

    ImGui::SetNextWindowSize(ImVec2(m_graph_size.x, m_graph_size.y - m_ruler_height -
                                                        m_artificial_scrollbar_height),
                             ImGuiCond_Always);
    ImGui::SetCursorPos(ImVec2(m_sidebar_size, 0));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(0, 0, 0, 0));
    ImGui::BeginChild("UI Interactive Overlay", ImVec2(m_graph_size.x, m_graph_size.y),
                      false, window_flags);

    ImGui::SetScrollY(static_cast<float>(m_scroll_position_y));
    ImGui::BeginChild(
        "UI Interactive Content", ImVec2(m_graph_size.x, m_track_height_sum), false,
        window_flags | ImGuiWindowFlags_NoScrollbar);

    ImDrawList* draw_list       = ImGui::GetWindowDrawList();
    ImVec2      window_position = ImGui::GetWindowPos();

    m_arrow_layer.Render(draw_list, m_v_min_x, m_pixels_per_ns, window_position,
                         m_track_position_y, m_graphs);

    RenderAnnotations(draw_list, window_position);

    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::EndChild();
    bool overlay_hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_None);
     bool overlay_clicked =
        overlay_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left);
    std::cout << overlay_clicked<<std::endl;
}

void
TimelineView::RenderAnnotations(ImDrawList* draw_list, ImVec2 window_position)
{
    bool movement_drag   = false;
    bool movement_resize = false;
    // m_visible_center     = current_center;

    if(m_annotations->IsVisibile())
    {
        // Interaction --> top-most gets priority
        for(int i = static_cast<int>(m_annotations->GetStickyNotes().size()) - 1; i >= 0;
            --i)
        {
            if(!m_annotations->GetStickyNotes()[i].IsVisible()) continue;

            movement_drag |= m_annotations->GetStickyNotes()[i].HandleDrag(
                window_position, m_v_min_x, m_v_max_x, m_pixels_per_ns,
                m_dragged_sticky_id);
            movement_resize |= m_annotations->GetStickyNotes()[i].HandleResize(
                window_position, m_v_min_x, m_v_max_x, m_pixels_per_ns);
        }

        // Rendering --> based on added order (old bottom new on top)
        for(size_t i = 0; i < m_annotations->GetStickyNotes().size(); ++i)
        {
            if(!m_annotations->GetStickyNotes()[i].IsVisible()) continue;

            m_annotations->GetStickyNotes()[i].Render(draw_list, window_position,
                                                      m_v_min_x, m_pixels_per_ns);
        }
    }
    m_stop_user_interaction |= movement_drag || movement_resize;

    RenderTimelineViewOptionsMenu(window_position);
    m_annotations->ShowStickyNotePopup();
    m_annotations->ShowStickyNoteEditPopup();
}
ImVec2
TimelineView::GetGraphSize()
{
    return m_graph_size;
}
void
TimelineView::RenderTimelineViewOptionsMenu(ImVec2 window_position)
{
    ImVec2 mouse_pos = ImGui::GetMousePos();
    // Mouse position relative without adjusting for user scroll.
    ImVec2 rel_mouse_pos =
        ImVec2(mouse_pos.x - window_position.x, mouse_pos.y - window_position.y);

    // Use the visible area for hover detection adjusted for user scroll.
    ImVec2 win_min = window_position;
    ImVec2 win_max = ImVec2(window_position.x + m_graph_size.x,
                            window_position.y + m_graph_size.y + m_scroll_position_y);

    if(ImGui::IsMouseClicked(ImGuiMouseButton_Right) &&
       ImGui::IsMouseHoveringRect(win_min, win_max) &&
       !ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopup))
    {
        ImGui::OpenPopup("StickyNoteContextMenu");
    }

    if(!ImGui::IsPopupOpen("StickyNoteContextMenu"))
    {
        return;
    }
    auto style = m_settings.GetDefaultStyle();
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, style.WindowPadding);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, style.ItemSpacing);
    if(ImGui::BeginPopup("StickyNoteContextMenu"))
    {
        if(ImGui::MenuItem("Add Annotation"))
        {
            float  x_in_chart = rel_mouse_pos.x;
            double time_ns =
                m_v_min_x + (x_in_chart / m_graph_size.x) * (m_v_max_x - m_v_min_x);
            float y_offset = rel_mouse_pos.y;
            m_annotations->OpenStickyNotePopup(time_ns, y_offset, m_v_min_x, m_v_max_x,
                                               m_graph_size);
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar(2);
}

void
TimelineView::ScrollToTrack(const uint64_t& track_id)
{
    if(m_track_position_y.count(track_id) > 0)
    {
        m_scroll_position_y =
            std::min(m_content_max_y_scroll, m_track_position_y[track_id]);
        ImGui::SetScrollY(m_scroll_position_y);
    }
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
float
TimelineView::GetScrollPosition()
{
    return m_scroll_position_y;
}

void
TimelineView::MoveToPosition(double start_ns, double end_ns, double y_position,
                             bool center)
{
    /*
    Use this funtion for all future navigation requests that do not need to scroll to a
    particular track. Ex) Annotation and Bookmarks.
    */

    SetViewableRangeNS(start_ns, end_ns);

    if(center)
    {
        m_scroll_position_y =
            std::clamp(static_cast<float>(y_position) - m_graph_size.y * 0.5f, 0.0f,
                       m_content_max_y_scroll);
    }
    else
    {
        m_scroll_position_y =
            std::clamp(static_cast<float>(y_position), 0.0f, m_content_max_y_scroll);
    }

    ImGui::SetScrollY(m_scroll_position_y);
}

void
TimelineView::SetViewableRangeNS(double start_ns, double end_ns)
{
    // Configure the timeline view so that the visible horizontal range is
    // [start_ns, end_ns] in absolute timestamp units.
    // Guard against invalid inputs.
    if(end_ns <= start_ns) return;

    // Clamp requested range to known data bounds when available.
    start_ns = std::max(start_ns, m_min_x);
    end_ns   = std::min(end_ns, m_max_x);
    if(end_ns <= start_ns) return;  // Fully outside bounds after clamping.

    double new_width_ns = end_ns - start_ns;
    // Prevent division by zero and overly small widths.
    const double kMinWidth = 10.0;  // 10 ns minimum span.
    if(new_width_ns < kMinWidth) new_width_ns = kMinWidth;

    // Compute zoom: m_v_width = m_range_x / m_zoom  =>  m_zoom = m_range_x / m_v_width
    if(m_range_x > 0.0)
    {
        m_zoom = std::max(0.000001, m_range_x / new_width_ns);
    }

    // view_time_offset is relative to m_min_x
    m_view_time_offset_ns = start_ns - m_min_x;

    // Update derived viewport values.
    m_v_width = new_width_ns;
    m_v_min_x = (m_min_x < m_max_x) ? start_ns : m_view_time_offset_ns;
    m_v_max_x = m_v_min_x + m_v_width;

    ROCPROFVIS_ASSERT(m_v_max_x > m_v_min_x);
    m_pixels_per_ns = (m_graph_size.x) / (m_v_max_x - m_v_min_x);

    // Ensure offsets remain within global bounds (after potential zoom computation).
    if(m_range_x > 0.0 && m_view_time_offset_ns + m_v_width > m_range_x)
    {
        m_view_time_offset_ns = std::max(0.0, m_range_x - m_v_width);
        m_v_min_x             = m_min_x + m_view_time_offset_ns;
        m_v_max_x             = m_v_min_x + m_v_width;
    }

    // Mark grid for recalculation since scale changed.
    m_recalculate_grid_interval = true;
}

TimelineView::~TimelineView()
{
    DestroyGraphs();
    EventManager::GetInstance()->Unsubscribe(static_cast<int>(RocEvents::kNewTrackData),
                                             m_new_track_token);
    EventManager::GetInstance()->Unsubscribe(
        static_cast<int>(RocEvents::kHandleUserGraphNavigationEvent),
        m_scroll_to_track_token);
    EventManager::GetInstance()->Unsubscribe(
        static_cast<int>(RocEvents::kFontSizeChanged), m_new_track_token);
    EventManager::GetInstance()->Unsubscribe(static_cast<int>(RocEvents::kSetViewRange),
                                             m_set_view_range_token);
    EventManager::GetInstance()->Unsubscribe(
        static_cast<int>(RocEvents::kGoToTimelineSpot), m_navigation_token);
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
    m_scroll_position_y   = 0.0f;
    m_v_min_x             = 0.0f;
    m_v_max_x             = 0.0f;
    m_pixels_per_ns       = 0.0f;
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
        const std::string& trace_path = tde->GetSourceId();
        // check if event trace path matches the current our data provider's trace path
        // since events are global for all views
        if(m_data_provider.GetTraceFilePath() != trace_path)
        {
            spdlog::debug("Trace path {} does not match current trace path {}",
                          trace_path, m_data_provider.GetTraceFilePath());
            return;
        }

        const track_info_t* metadata = m_data_provider.GetTrackInfo(tde->GetTrackID());
        if(!metadata)
        {
            spdlog::error(
                "No metadata found for track id {}, cannot process new track data",
                tde->GetTrackID());
            return;
        }

        uint64_t track_index = metadata->index;
        if(track_index < m_graphs->size())
        {
            if((*m_graphs)[track_index].chart)
            {
                (*m_graphs)[track_index].chart->HandleTrackDataChanged(
                    tde->GetRequestID(), tde->GetResponseCode());
            }
            else
            {
                spdlog::error("Chart object for track index {} is null. Cannot handle "
                              "track data changed.",
                              track_index);
            }
        }
        else
        {
            spdlog::warn("Track index {} not found in graph_map. Cannot handle track "
                         "data changed.",
                         track_index);
        }
    }
}

void
TimelineView::Update()
{
    if(m_meta_map_made)
    {
        if(!m_reorder_request.handled)
        {
            if(m_data_provider.SetGraphIndex(m_reorder_request.track_id,
                                             m_reorder_request.new_index))
            {
                std::vector<rocprofvis_graph_t> m_graphs_reordered;
                m_graphs_reordered.resize(m_data_provider.GetTrackCount());
                for(rocprofvis_graph_t& graph : *m_graphs)
                {
                    const track_info_t* metadata =
                        m_data_provider.GetTrackInfo(graph.chart->GetID());
                    ROCPROFVIS_ASSERT(metadata);
                    m_graphs_reordered[metadata->index] = std::move(graph);
                }
                *m_graphs = std::move(m_graphs_reordered);
            }
        }
        // Rebuild the positioning map.
        if(m_resize_activity || !m_reorder_request.handled)
        {
            m_track_position_y.clear();
            m_track_height_sum = 0;
            for(int i = 0; i < m_graphs->size(); i++)
            {
                m_track_position_y[(*m_graphs)[i].chart->GetID()] = m_track_height_sum;
                m_track_height_sum +=
                    (*m_graphs)[i].display
                        ? (*m_graphs)[i]
                              .chart->GetTrackHeight()  // Get the height of the track.
                        : 0;
                (*m_graphs)[i].display_changed = false;
            }
        }
        m_reorder_request.handled = true;
        m_resize_activity         = false;
    }
}

void
TimelineView::Render()
{
    if(m_meta_map_made)
    {
        RenderGraphPoints();
    }
    if(m_graph_size.x != m_last_graph_size.x || m_zoom != m_last_zoom)
    {
        m_recalculate_grid_interval = true;
    }

    m_last_zoom       = m_zoom;
    m_last_graph_size = m_graph_size;
}

void
TimelineView::RenderSplitter(ImVec2 screen_pos)
{
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                                    ImGuiWindowFlags_NoScrollWithMouse;

    ImVec2 display_size = ImGui::GetWindowSize();

    ImGui::SetNextWindowSize(ImVec2(1.0f, display_size.y), ImGuiCond_Always);
    ImGui::SetCursorPos(ImVec2(m_sidebar_size, 0));

    ImGui::PushStyleColor(ImGuiCol_ChildBg, m_settings.GetColor(Colors::kSplitterColor));

    ImGui::BeginChild("Splitter View", ImVec2(0, 0), ImGuiChildFlags_None, window_flags);

    ImGui::Selectable("##MovePositionLineVert", false,
                      ImGuiSelectableFlags_AllowDoubleClick,
                      ImVec2(5.0f, display_size.y));

    if(ImGui::IsItemHovered())
    {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
    }

    if(ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
    {
        ImVec2 drag_delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);
        m_sidebar_size    = std::clamp(m_sidebar_size + drag_delta.x, 100.0f, 600.0f);
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
    ImGui::SetCursorPos(
        ImVec2(0, m_graph_size.y - m_ruler_height - m_artificial_scrollbar_height));

    ImGui::PushStyleColor(ImGuiCol_ChildBg, m_settings.GetColor(Colors::kSplitterColor));

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

    ImVec2 container_size  = ImGui::GetWindowSize();
    float  scrollbar_width = ImGui::GetStyle().ScrollbarSize;
    ImGui::SetNextWindowSize(m_graph_size, ImGuiCond_Always);
    ImGui::SetCursorPos(ImVec2(m_sidebar_size, 0));

    // overlayed windows need to have fully trasparent bg otherwise they will overlay
    // (with no alpha) over their predecessors
    ImGui::PushStyleColor(ImGuiCol_ChildBg, m_settings.GetColor(Colors::kTransparent));

    ImGui::BeginChild(
        "Scrubber View",
        ImVec2(m_graph_size.x, m_graph_size.y - m_artificial_scrollbar_height),
        ImGuiChildFlags_None, window_flags);

    ImGui::SetItemAllowOverlap();

    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    ImVec2 window_position = ImGui::GetWindowPos();
    ImVec2 mouse_position  = ImGui::GetMousePos();

    ImVec2 relative_mouse_pos = ImVec2(mouse_position.x - window_position.x,
                                       mouse_position.y - window_position.y);

    // Render range selction box
    ImVec2 cursor_position = screen_pos;

    if(m_highlighted_region.first != TimelineSelection::INVALID_SELECTION_TIME)
    {
        double normalized_start_box_highlighted =
            window_position.x +
            (m_highlighted_region.first - m_view_time_offset_ns) * m_pixels_per_ns;

        draw_list->AddLine(ImVec2(normalized_start_box_highlighted, cursor_position.y),
                           ImVec2(normalized_start_box_highlighted,
                                  cursor_position.y + container_size.y - m_ruler_height),
                           m_settings.GetColor(Colors::kSelectionBorder), 3.0f);
    }
    if(m_highlighted_region.first != TimelineSelection::INVALID_SELECTION_TIME)
    {
        double normalized_start_box_highlighted_end =
            window_position.x +
            (m_highlighted_region.second - m_view_time_offset_ns) * m_pixels_per_ns;

        draw_list->AddLine(
            ImVec2(normalized_start_box_highlighted_end, cursor_position.y),
            ImVec2(normalized_start_box_highlighted_end,
                   cursor_position.y + container_size.y - m_ruler_height),
            m_settings.GetColor(Colors::kSelectionBorder), 3.0f);
    }
    if(m_highlighted_region.first != TimelineSelection::INVALID_SELECTION_TIME &&
       m_highlighted_region.second != TimelineSelection::INVALID_SELECTION_TIME)
    {
        double normalized_start_box_highlighted =
            window_position.x +
            (m_highlighted_region.first - m_view_time_offset_ns) * m_pixels_per_ns;
        double normalized_start_box_highlighted_end =
            window_position.x +
            (m_highlighted_region.second - m_view_time_offset_ns) * m_pixels_per_ns;
        draw_list->AddRectFilled(
            ImVec2(normalized_start_box_highlighted, cursor_position.y),
            ImVec2(normalized_start_box_highlighted_end,
                   cursor_position.y + container_size.y - m_ruler_height),
            m_settings.GetColor(Colors::kSelection));
    }

    // IsMouseHoveringRect check in screen coordinates
    if(ImGui::IsMouseHoveringRect(
           window_position, ImVec2(window_position.x + m_graph_size.x - scrollbar_width,
                                   window_position.y + m_graph_size.y)) &&
       !m_stop_user_interaction)
    {
        float cursor_screen_percentage =
            (mouse_position.x - window_position.x) / m_graph_size.x;
        double scrubber_position =
            m_view_time_offset_ns + (cursor_screen_percentage * m_v_width);

        std::string label = nanosecond_to_formatted_str(
            scrubber_position, m_settings.GetUserSettings().unit_settings.time_format,
            true);

        ImVec2 label_size = ImGui::CalcTextSize(label.c_str());

        constexpr float label_padding = 4.0f;
        ImVec2 rect_pos1 = ImVec2(mouse_position.x, screen_pos.y + container_size.y -
                                                        label_size.y - m_ruler_padding);
        ImVec2 rect_pos2 = ImVec2(mouse_position.x + label_size.x + label_padding * 2,
                                  screen_pos.y + container_size.y - m_ruler_padding);
        ImVec2 text_pos  = ImVec2(rect_pos1.x + label_padding, rect_pos1.y);

        draw_list->AddRectFilled(rect_pos1, rect_pos2,
                                 m_settings.GetColor(Colors::kScrubberNumberColor));
        draw_list->AddText(text_pos, m_settings.GetColor(Colors::kFillerColor),
                           label.c_str());
        draw_list->AddLine(
            ImVec2(mouse_position.x, screen_pos.y),
            ImVec2(mouse_position.x, screen_pos.y + container_size.y - m_ruler_padding),
            m_settings.GetColor(Colors::kGridColor), 2.0f);

        // Code below is for detecting range selection by double clicking
        if(ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        {
            if(m_highlighted_region.first == TimelineSelection::INVALID_SELECTION_TIME)
            {
                m_highlighted_region.first =
                    m_view_time_offset_ns + (cursor_screen_percentage * m_v_width);
            }
            else if(m_highlighted_region.second ==
                    TimelineSelection::INVALID_SELECTION_TIME)
            {
                m_highlighted_region.second =
                    m_view_time_offset_ns + (cursor_screen_percentage * m_v_width);
                m_timeline_selection->SelectTimeRange(
                    std::min(m_highlighted_region.first, m_highlighted_region.second) +
                        m_min_x,
                    std::max(m_highlighted_region.first, m_highlighted_region.second) +
                        m_min_x);
            }
            else
            {
                m_highlighted_region.first  = TimelineSelection::INVALID_SELECTION_TIME;
                m_highlighted_region.second = TimelineSelection::INVALID_SELECTION_TIME;
                m_timeline_selection->ClearTimeRange();
            }
        }
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

std::shared_ptr<std::vector<rocprofvis_graph_t>>
TimelineView::GetGraphs()
{
    return m_graphs;
}

void
TimelineView::CalculateGridInterval()
{
    // measure the size of the label to determine the step size
    std::string label =
        nanosecond_to_formatted_str(
            m_max_x - m_min_x, m_settings.GetUserSettings().unit_settings.time_format,
            true) +
        "gap";
    ImVec2 label_size = ImGui::CalcTextSize(label.c_str());

    // calculate the number of intervals based on the graph width and label width
    int interval_count = m_graph_size.x / label_size.x;

    double interval_ns  = calculate_nice_interval(m_v_width, interval_count);
    double step_size_px = interval_ns * m_pixels_per_ns;

    int pad_amount = 2;  // +2 for the first and last label

    // If the step size is smaller than the label size, try to adjust the interval count
    while(step_size_px < label_size.x)
    {
        interval_count--;
        interval_ns  = calculate_nice_interval(m_v_width, interval_count);
        step_size_px = interval_ns * m_pixels_per_ns;
        // If the interval count is too small break out and pad it
        if(interval_count <= 3)
        {
            pad_amount++;
            break;
        }
    }

    m_grid_interval_ns    = interval_ns;
    m_grid_interval_count = interval_count + pad_amount;
}

void
TimelineView::RenderGrid()
{
   
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                                    ImGuiWindowFlags_NoScrollWithMouse;

    ImVec2 container_pos =
        ImVec2(ImGui::GetWindowPos().x + m_sidebar_size, ImGui::GetWindowPos().y);

    ImVec2 container_size  = ImGui::GetWindowSize();
    ImVec2 cursor_position = ImGui::GetCursorScreenPos();
    ImVec2 content_size    = ImVec2(container_size.x - m_sidebar_size, container_size.y);

    if(m_recalculate_grid_interval)
    {
        CalculateGridInterval();
        m_recalculate_grid_interval = false;
    }

    constexpr float tick_height = 10.0f;
    double          start_ns    = m_view_time_offset_ns;
    double          grid_line_start_ns =
        std::floor(start_ns / m_grid_interval_ns) * m_grid_interval_ns;

    ImGui::SetCursorPos(ImVec2(m_sidebar_size, 0));

    if(ImGui::BeginChild("Grid"), content_size, true, window_flags)
    {
        ImDrawList* draw_list = ImGui::GetWindowDrawList();

        ImVec2 child_win  = ImGui::GetWindowPos();
        ImVec2 child_size = ImGui::GetWindowSize();

        // Background for the ruler area
        draw_list->AddRectFilled(
            ImVec2(container_pos.x, cursor_position.y + content_size.y - m_ruler_height),
            ImVec2(container_pos.x + m_graph_size.x, cursor_position.y + content_size.y),
            m_settings.GetColor(Colors::kRulerBgColor));

        // Detect right mouse click in the ruler area
        if(ImGui::IsMouseClicked(ImGuiMouseButton_Right) &&
           ImGui::IsMouseHoveringRect(
               ImVec2(container_pos.x,
                      cursor_position.y + content_size.y - m_ruler_height),
               ImVec2(container_pos.x + m_graph_size.x,
                      cursor_position.y + content_size.y)))
        {
            // Show context menu for time format selection
            ImGui::OpenPopup("Time Format Selection");
        }

        std::string label;
        for(auto i = 0; i < m_grid_interval_count; i++)
        {
            double grid_line_ns = grid_line_start_ns + (i * m_grid_interval_ns);
            float  normalized_start =
                child_win.x + (grid_line_ns - m_view_time_offset_ns) * m_pixels_per_ns;

            draw_list->AddLine(
                ImVec2(normalized_start, cursor_position.y),
                ImVec2(normalized_start,
                       cursor_position.y + content_size.y + tick_height - m_ruler_height),
                m_settings.GetColor(Colors::kBoundBox), 0.5f);

            label = nanosecond_to_formatted_str(
                grid_line_ns, m_settings.GetUserSettings().unit_settings.time_format,
                true);

            ImVec2 label_size = ImGui::CalcTextSize(label.c_str());

            ImVec2 label_pos;
            if(grid_line_start_ns == 0)
            {
                label_pos = ImVec2(normalized_start + ImGui::CalcTextSize("0").x,
                                   cursor_position.y + content_size.y - label_size.y -
                                       m_ruler_padding);
            }
            else
            {
                label_pos = ImVec2(normalized_start - label_size.x / 2,
                                   cursor_position.y + content_size.y - label_size.y -
                                       m_ruler_padding);
            }

            draw_list->AddText(label_pos, m_settings.GetColor(Colors::kRulerTextColor),
                               label.c_str());
        }
    }

    ImGui::EndChild();
}

void
TimelineView::RenderGraphView()
{
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                                    ImGuiWindowFlags_NoScrollWithMouse;

    ImVec2 container_size = ImGui::GetWindowSize();
    ImGui::SetCursorPos(ImVec2(0, 0));

    // overlayed windows need to have fully trasparent bg otherwise they will overlay
    // (with no alpha) over their predecessors
    ImGui::PushStyleColor(ImGuiCol_ChildBg, m_settings.GetColor(Colors::kTransparent));
    ImGui::BeginChild("Graph View Main",
                      ImVec2(container_size.x, container_size.y - m_ruler_height), false,
                      window_flags);
    m_content_max_y_scroll = ImGui::GetScrollMaxY();

    // Prevent choppy behavior by preventing constant rerender.
    float temp_scroll_position = ImGui::GetScrollY();
    if(m_previous_scroll_position != temp_scroll_position)
    {
        m_previous_scroll_position = temp_scroll_position;
        m_scroll_position_y        = temp_scroll_position;
    }
    else if(m_scroll_position_y != temp_scroll_position)
    {
        ImGui::SetScrollY(m_scroll_position_y);
    }

    ImVec2 window_size  = m_graph_size;
    bool   request_data = false;

    // for zooming out
    if(m_v_width - m_last_data_req_v_width > m_last_data_req_v_width)
    {
        spdlog::debug("Zooming out: m_last_data_req_v_width: {}, m_v_width: {}, "
                      "m_last_data_req_view_time_offset_ns: {}",
                      m_last_data_req_v_width, m_v_width,
                      m_last_data_req_view_time_offset_ns);

        m_last_data_req_v_width             = m_v_width;
        m_last_data_req_view_time_offset_ns = m_view_time_offset_ns;
        request_data                        = true;
    }
    // zooming in
    else if(m_last_data_req_v_width > m_v_width * 2.0f)
    {
        spdlog::debug("Zooming in: m_last_data_req_v_width: {}, m_v_width: {}, "
                      "m_last_data_req_view_time_offset_ns: {}",
                      m_last_data_req_v_width, m_v_width,
                      m_last_data_req_view_time_offset_ns);

        m_last_data_req_v_width             = m_v_width;
        m_last_data_req_view_time_offset_ns = m_view_time_offset_ns;
        request_data                        = true;
    }
    // for panning
    else if(std::abs(m_view_time_offset_ns - m_last_data_req_view_time_offset_ns) >
            m_v_width)
    {
        spdlog::debug("Panning: m_last_data_req_v_width: {}, m_v_width: {}, "
                      "m_last_data_req_view_time_offset_ns: {}",
                      m_last_data_req_v_width, m_v_width,
                      m_last_data_req_view_time_offset_ns);

        m_last_data_req_view_time_offset_ns = m_view_time_offset_ns;
        request_data                        = true;
    }

    for(int i = 0; i < m_graphs->size(); i++)
    {
        rocprofvis_graph_t& track_item = (*m_graphs)[i];

        m_resize_activity |= track_item.display_changed;

        if(track_item.display)
        {
            ROCPROFVIS_ASSERT(track_item.chart);
            // Get track height and position to check if the track is in view
            float  track_height = track_item.chart->GetTrackHeight();
            ImVec2 track_pos    = ImGui::GetCursorPos();

            // Calculate the track's position in the scrollable area
            float track_top    = track_pos.y;
            float track_bottom = track_top + track_height;

            // Calculate deltas for out-of-view tracks
            float delta_top = m_scroll_position_y -
                              track_bottom;  // Positive if the track is above the view
            float delta_bottom =
                track_top - (m_scroll_position_y +
                             m_graph_size.y);  // Positive if the track is below the view

            // Save distance for book keeping
            track_item.chart->SetDistanceToView(
                std::max(std::max(delta_bottom, delta_top), 0.0f));

            // This item is being reordered if there is an active payload and its id
            // matches the payload's id.
            bool is_reordering = ImGui::GetDragDropPayload() &&
                                 m_reorder_request.track_id == track_item.chart->GetID();

            // Check if the track is visible
            bool is_visible = (track_bottom >= m_scroll_position_y &&
                               track_top <= m_scroll_position_y + m_graph_size.y) ||
                              is_reordering;

            track_item.chart->SetInViewVertical(is_visible);

            m_resize_activity |= track_item.chart->TrackHeightChanged();

            if(is_visible ||
               track_item.chart->GetDistanceToView() <= m_unload_track_distance)
            {
                // Request data for the chart if it doesn't have data.
                if((!track_item.chart->HasData() && track_item.chart->GetRequestState() ==
                                                        TrackDataRequestState::kIdle) ||
                   request_data)

                {
                    // Request one viewport worth of data on each side of the current
                    // view.
                    double buffer_distance = m_v_width;
                    track_item.chart->RequestData(
                        (m_view_time_offset_ns - buffer_distance) + m_min_x,
                        (m_view_time_offset_ns + m_v_width + buffer_distance) + m_min_x,
                        m_graph_size.x * 3);
                }
            }

            if(is_visible)
            {
                ImU32 selection_color = m_settings.GetColor(Colors::kTransparent);
                if(track_item.selected)
                {
                    selection_color = m_settings.GetColor(Colors::kHighlightChart);
                }

                ImGui::PushStyleColor(ImGuiCol_ChildBg, selection_color);
                ImGui::PushID(i);
                if(ImGui::BeginChild("", ImVec2(0, track_height), false,
                                     window_flags | ImGuiWindowFlags_NoScrollbar))
                {
                    // call update function (TODO: move this to timeline's update
                    // function?)
                    track_item.chart->Update();

                    track_item.chart->UpdateMovement(m_zoom, m_view_time_offset_ns,
                                                     m_min_x, m_max_x, m_pixels_per_ns,
                                                     m_scroll_position_y);

                    if(is_reordering)
                    {
                        // Empty space if the track is being reordered
                        ImGui::Dummy(ImVec2(0, track_height));
                    }
                    else
                    {
                        track_item.chart->Render(m_graph_size.x);
                    }

                    // Region for recieving reordering request.
                    if(ImGui::BeginDragDropTarget())
                    {
                        if(ImGui::AcceptDragDropPayload("reorder_request"))
                        {
                            m_reorder_request.handled   = false;
                            m_reorder_request.new_index = i;
                        }
                        ImGui::EndDragDropTarget();
                    }

                    ImGui::PopStyleColor();  // ImGuiCol_ChildBg

                    ImGui::SetCursorPos(ImVec2(0, 0));
                    // Transparent region where reordering can be initiated.
                    if(ImGui::BeginChild(
                           "reorder_handle",
                           ImVec2(track_item.chart->GetReorderGripWidth(), 0), false,
                           window_flags | ImGuiWindowFlags_NoScrollbar))
                    {
                        // Check if the resize grip area is hovered to change the cursor
                        ImVec2 cursor_pos             = ImGui::GetCursorPos();
                        ImVec2 invisible_hotspot_size = ImGui::GetContentRegionAvail();
                        ImVec2 invisible_hotspot_pos  = cursor_pos;
                        ImGui::SetCursorPos(invisible_hotspot_pos);
                        ImGui::InvisibleButton("##InvisibleHotspot",
                                               invisible_hotspot_size,
                                               ImGuiButtonFlags_None);
                        if(ImGui::IsItemHovered())
                        {
                            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
                        }
                        ImGui::SetCursorPos(cursor_pos);  // Reset cursor position

                        if(ImGui::BeginDragDropSource(
                               ImGuiDragDropFlags_SourceNoPreviewTooltip))
                        {
                            char dummy_payload;
                            ImGui::SetDragDropPayload("reorder_request", &dummy_payload,
                                                      sizeof(char));
                            m_reorder_request.track_id = track_item.chart->GetID();
                            ImGui::EndDragDropSource();
                        }
                    }
                    ImGui::EndChild();
                    ImGui::PushStyleColor(ImGuiCol_ChildBg, selection_color);

                    // check for mouse click
                    if(track_item.chart->IsMetaAreaClicked())
                    {
                        m_timeline_selection->ToggleSelectTrack(track_item);
                    }
                }
                ImGui::EndChild();
                ImGui::PopID();
                ImGui::PopStyleColor();

                // Draw border around the track
                // This is done after the child window to ensure it is on top
                ImVec2 p_min = ImGui::GetItemRectMin();
                ImVec2 p_max = ImGui::GetItemRectMax();
                ImGui::GetWindowDrawList()->AddRect(
                    p_min, p_max, m_settings.GetColor(Colors::kBorderColor), 0.0f, 0,
                    1.0f);
            }
            else
            {
                // If the track is not visible past a certain distance, release its
                // data to free up memory
                if(track_item.chart->GetDistanceToView() > m_unload_track_distance &&
                   (track_item.chart->HasData() ||
                    track_item.chart->HasPendingRequests()))
                {
                    track_item.chart->ReleaseData();
                }

                // Render dummy to maintain layout
                ImGui::Dummy(ImVec2(0, track_height));
            }

            if(is_reordering)
            {
                // Show the track as tooltip while being reordered.
                ImVec2 graph_view_pos     = ImGui::GetWindowPos();
                ImVec2 mouse_pos          = ImGui::GetMousePos();
                ImVec2 mouse_relative_pos = mouse_pos - graph_view_pos;

                ImGui::SetNextWindowPos(
                    ImVec2(graph_view_pos.x, mouse_pos.y - ImGui::GetFrameHeight() / 2),
                    ImGuiCond_Always);
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
                if(ImGui::Begin("##ReorderPreview", nullptr,
                                ImGuiWindowFlags_Tooltip | ImGuiWindowFlags_NoInputs |
                                    ImGuiWindowFlags_NoNav |
                                    ImGuiWindowFlags_NoDecoration |
                                    ImGuiWindowFlags_AlwaysAutoResize |
                                    ImGuiWindowFlags_NoSavedSettings |
                                    ImGuiWindowFlags_NoFocusOnAppearing |
                                    ImGuiWindowFlags_NoBringToFrontOnFocus))
                {
                    track_item.chart->Render(m_graph_size.x);
                }
                ImGui::End();
                ImGui::PopStyleVar();

                // Scroll the view if the mouse is near the top or bottom of the window.
                // Speed is proportional to frame height and depth of mouse inside
                // auto-scroll zone
                if(mouse_relative_pos.y <
                   container_size.y * REORDER_AUTO_SCROLL_THRESHOLD)
                {
                    ImGui::SetScrollY(
                        m_scroll_position_y -
                        ImGui::GetFrameHeight() *
                            std::min(
                                1.0f,
                                (container_size.y * REORDER_AUTO_SCROLL_THRESHOLD -
                                 mouse_relative_pos.y) /
                                    (container_size.y * REORDER_AUTO_SCROLL_THRESHOLD)));
                }
                else if(mouse_relative_pos.y >
                        container_size.y * (1 - REORDER_AUTO_SCROLL_THRESHOLD))
                {
                    ImGui::SetScrollY(
                        m_scroll_position_y +
                        ImGui::GetFrameHeight() *
                            std::min(
                                1.0f,
                                (mouse_relative_pos.y -
                                 container_size.y * (1 - REORDER_AUTO_SCROLL_THRESHOLD)) /
                                    (container_size.y * REORDER_AUTO_SCROLL_THRESHOLD)));
                }
                m_scroll_position_y = ImGui::GetScrollY();
            }
        }
    }

    TrackItem::SetSidebarSize(m_sidebar_size);
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void
TimelineView::DestroyGraphs()
{
    if (m_graphs)
    {
        for(rocprofvis_graph_t& graph : *m_graphs)
        {
            delete graph.chart;
        }

        m_graphs->clear();
    }
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

    m_v_width               = (m_range_x) / m_zoom;
    m_last_data_req_v_width = m_v_width;

    /*This section makes the charts both line and flamechart are constructed here*/
    uint64_t num_graphs = m_data_provider.GetTrackCount();
    int      scale_x    = 1;
    m_graphs->resize(num_graphs);

    std::vector<const track_info_t*> track_list    = m_data_provider.GetTrackInfoList();
    bool                             project_valid = m_project_settings.Valid();

    for(int i = 0; i < track_list.size(); i++)
    {
        const track_info_t* track_info = track_list[i];
        bool                display    = true;

        if(project_valid)
        {
            uint64_t            track_id_at_index = m_project_settings.TrackID(i);
            const track_info_t* track_at_index_info =
                m_data_provider.GetTrackInfo(track_id_at_index);
            if(track_at_index_info && track_at_index_info->index != i)
            {
                ROCPROFVIS_ASSERT(m_data_provider.SetGraphIndex(track_id_at_index, i));
            }
            track_info = track_at_index_info;
            display    = m_project_settings.DisplayTrack(track_id_at_index);
        }

        if(!track_info)
        {
            // log warning (should this be an error?)
            spdlog::warn("Missing track meta data for track id {}", i);
            continue;
        }

        rocprofvis_graph_t graph = { rocprofvis_graph_t::TYPE_FLAMECHART, display, false,
                                     nullptr, false };
        switch(track_info->track_type)
        {
            case kRPVControllerTrackTypeEvents:
            {
                // Create FlameChart

                graph.chart = new FlameTrackItem(
                    m_data_provider, m_timeline_selection, track_info->id,
                    track_info->name, m_zoom, m_view_time_offset_ns, m_min_x, m_max_x,
                    scale_x, track_info->min_value, track_info->max_value);
                graph.graph_type = rocprofvis_graph_t::TYPE_FLAMECHART;
                break;
            }
            case kRPVControllerTrackTypeSamples:
            {
                // Linechart
                graph.chart = new LineTrackItem(
                    m_data_provider, track_info->id, track_info->name, m_zoom,
                    m_view_time_offset_ns, m_min_x, m_max_x, m_pixels_per_ns);
                graph.graph_type = rocprofvis_graph_t::TYPE_LINECHART;
                break;
            }
            default:
            {
                break;
            }
        }
        if(graph.chart)
        {
            m_min_x = std::min(track_info->min_ts, m_min_x);
            m_max_x = std::max(track_info->max_ts, m_max_x);

            (*m_graphs)[track_info->index] = std::move(graph);
        }
    }
    m_histogram       = &m_data_provider.GetHistogram();
    m_meta_map_made   = true;
    m_resize_activity = true;
}

void
TimelineView::RenderHistogram()
{
    if(!m_histogram || m_histogram->empty()) return;

    const float     kHistogramTotalHeight = ImGui::GetContentRegionAvail().y;
    const float     kHistogramBarHeight   = kHistogramTotalHeight - m_ruler_height;
    ImVec2          window_size           = ImGui::GetWindowSize();

    // Outer container
    ImGui::PushStyleColor(ImGuiCol_ChildBg, m_settings.GetColor(Colors::kBgMain));
    ImGui::BeginChild("Histogram", ImVec2(window_size.x, kHistogramTotalHeight), false,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    // Histogram bars area
    ImGui::BeginChild("Histogram Bars", ImVec2(window_size.x, kHistogramBarHeight), false,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    ImDrawList* draw_list   = ImGui::GetWindowDrawList();
    ImVec2      bars_pos    = ImGui::GetCursorScreenPos();
    float       bars_width  = window_size.x;
    float       bars_height = kHistogramBarHeight;
    size_t      bin_count   = m_histogram->size();

    // Draw histogram bars
    if(bin_count > 0)
    {
        uint64_t max_bin_value =
            *std::max_element(m_histogram->begin(), m_histogram->end());
        float bin_width = bars_width / static_cast<float>(bin_count);

        for(size_t i = 0; i < bin_count; ++i)
        {
            float x0 = bars_pos.x + i * bin_width;
            float x1 = x0 + bin_width;
            float y0 = bars_pos.y;
            float y1 = y0 + bars_height;
            float bar_height =
                (max_bin_value > 0)
                    ? (static_cast<float>((*m_histogram)[i]) / max_bin_value) *
                          bars_height
                    : 0.0f;
            float y_bar = y1 - bar_height;
            draw_list->AddRectFilled(ImVec2(x0, y_bar), ImVec2(x1, y1),
                                     i % 2 == 0
                                         ? m_settings.GetColor(Colors::kAccentRedActive)
                                         : m_settings.GetColor(Colors::kAccentRed));
        }
    }

    // Draw view range overlays and labels
    float view_start_frac = m_view_time_offset_ns / m_range_x;
    float view_end_frac   = (m_view_time_offset_ns + m_v_width) / m_range_x;
    view_start_frac       = std::clamp(view_start_frac, 0.0f, 1.0f);
    view_end_frac         = std::clamp(view_end_frac, 0.0f, 1.0f);

    float x_view_start = bars_pos.x + view_start_frac * bars_width;
    float x_view_end   = bars_pos.x + view_end_frac * bars_width;
    float y0           = bars_pos.y;
    float y1           = bars_pos.y + bars_height;

    // Left overlay
    if(x_view_start > bars_pos.x)
    {
        draw_list->AddRectFilled(ImVec2(bars_pos.x, y0), ImVec2(x_view_start, y1),
                                 m_settings.GetColor(Colors::kGridColor));
        std::string vmin_label = nanosecond_to_formatted_str(
            m_v_min_x - m_min_x, m_settings.GetUserSettings().unit_settings.time_format);
        ImVec2 vmin_label_size = ImGui::CalcTextSize(vmin_label.c_str());
        float  vmin_label_x =
            std::max(x_view_start - vmin_label_size.x - 6, bars_pos.x + 2);
        ImVec2 vmin_label_pos(vmin_label_x, y0);
        draw_list->AddText(vmin_label_pos, m_settings.GetColor(Colors::kRulerTextColor),
                           vmin_label.c_str());
    }

    // Right overlay
    if(x_view_end < bars_pos.x + bars_width)
    {
        draw_list->AddRectFilled(ImVec2(x_view_end, y0),
                                 ImVec2(bars_pos.x + bars_width, y1),
                                 m_settings.GetColor(Colors::kGridColor));
        std::string vmax_label = nanosecond_to_formatted_str(
            m_v_max_x - m_min_x, m_settings.GetUserSettings().unit_settings.time_format);
        ImVec2 vmax_label_size = ImGui::CalcTextSize(vmax_label.c_str());
        float  vmax_label_x =
            std::min(x_view_end + 6, bars_pos.x + bars_width - vmax_label_size.x - 2);
        ImVec2 vmax_label_pos(vmax_label_x, y0 + (bars_height - vmax_label_size.y));
        draw_list->AddText(vmax_label_pos, m_settings.GetColor(Colors::kRulerTextColor),
                           vmax_label.c_str());
    }

    if(!m_resize_activity && !m_stop_user_interaction) HandleHistogramTouch();

    ImGui::EndChild();  // Histogram Bars

    // Ruler area
    ImGui::PushStyleColor(ImGuiCol_ChildBg, m_settings.GetColor(Colors::kRulerBgColor));
    ImGui::BeginChild("Histogram Ruler", ImVec2(window_size.x, m_ruler_height), false,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    ImDrawList* draw_list_ruler = ImGui::GetWindowDrawList();
    ImVec2      ruler_pos       = ImGui::GetCursorScreenPos();
    float       ruler_width     = window_size.x;
    float       tick_top        = ruler_pos.y + 2.0f;
    float       tick_bottom     = ruler_pos.y + 7.0f;
    ImFont*     font            = m_settings.GetFontManager().GetFont(FontType::kSmall);
    float       label_font_size = font->FontSize;

    // Interval calculation
    // measure the size of the label to determine the step size
    std::string label =
        nanosecond_to_formatted_str(
            m_range_x, m_settings.GetUserSettings().unit_settings.time_format,
            true) +
        "gap";
    ImVec2 label_size = ImGui::CalcTextSize(label.c_str());

    // calculate the number of intervals based on the graph width and label width
    // reserve space for first and last label
    int interval_count = static_cast<int>((ruler_width - label_size.x * 2.0f) / label_size.x);
    if(interval_count < 1) interval_count = 1;

    double pixels_per_ns = window_size.x / m_range_x;
    double interval_ns = calculate_nice_interval(m_range_x, interval_count);
    double step_size_px = interval_ns * pixels_per_ns;
    int pad_amount = 2;  // +2 for the first and last label

    // If the step size is smaller than the label size, try to adjust the interval count
    while(step_size_px < label_size.x)
    {
        interval_count--;
        interval_ns  = calculate_nice_interval(m_range_x, interval_count);
        step_size_px = interval_ns * pixels_per_ns;
        // If the interval count is too small break out
        if(interval_count <= 0)
        {
            break;
        }
    }

    const int num_ticks          = interval_count + pad_amount;
    double    grid_line_start_ns = 0;
    ImVec2    window_pos         = ImGui::GetWindowPos();

    for(int i = 0; i < num_ticks; i++)
    {
        double tick_ns = grid_line_start_ns + (i * interval_ns);
        float  tick_x  = window_pos.x + tick_ns * pixels_per_ns;
        draw_list_ruler->AddLine(ImVec2(tick_x, tick_top), ImVec2(tick_x, tick_bottom),
                                 m_settings.GetColor(Colors::kRulerTextColor), 1.0f);

        std::string tick_label = nanosecond_to_formatted_str(
            tick_ns, m_settings.GetUserSettings().unit_settings.time_format);
        label_size = ImGui::CalcTextSize(tick_label.c_str());

        float label_x;
        if(i == 0)
        {
            label_x = tick_x + ImGui::CalcTextSize("0").x;
        }
        else
        {
            label_x = tick_x;
        }

        ImVec2 label_pos(label_x, tick_bottom + 1.0f);
        draw_list_ruler->AddText(font, label_font_size, label_pos,
                                 m_settings.GetColor(Colors::kRulerTextColor),
                                 tick_label.c_str());
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::EndChild();
    ImGui::PopStyleColor();

    // Check if mouse is inside histogram area
    window_pos         = ImGui::GetWindowPos();
    ImGuiIO& io        = ImGui::GetIO();
    bool mouse_any = io.MouseDown[ImGuiMouseButton_Left] ||
                     io.MouseDown[ImGuiMouseButton_Right] || io.MouseDown[ImGuiMouseButton_Middle];

    ImVec2 mouse_position = io.MousePos;
    bool   mouse_inside   = mouse_position.x >= window_pos.x && mouse_position.x <= window_pos.x + window_size.x &&
                          mouse_position.y >= window_pos.y &&
                          mouse_position.y <= window_pos.y + window_size.y;

    // Update pseudo focus state based on mouse interaction
    if(mouse_any)
    {
        if( mouse_inside )
            m_histogram_pseudo_focus = true;
        else
            m_histogram_pseudo_focus = false;
    }

}

void
TimelineView::RenderTraceView()
{
    ImVec2 screen_pos             = ImGui::GetCursorScreenPos();
    ImVec2 subcomponent_size_main = ImGui::GetWindowSize();

    ImGuiStyle& style             = ImGui::GetStyle();
    float       fontHeight        = ImGui::GetFontSize();
    m_artificial_scrollbar_height = fontHeight + style.FramePadding.y * 2.0f;
    m_graph_size =
        ImVec2(subcomponent_size_main.x - m_sidebar_size, subcomponent_size_main.y);

    ImGui::BeginChild("Grid View 2",
                      ImVec2(subcomponent_size_main.x,
                             subcomponent_size_main.y - m_artificial_scrollbar_height),
                      false,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    // Scale used in all graphs computed here
    m_v_width       = (m_range_x) / m_zoom;
    m_v_min_x       = m_min_x + m_view_time_offset_ns;
    m_v_max_x       = m_v_min_x + m_v_width;
    m_pixels_per_ns = (m_graph_size.x) / (m_v_max_x - m_v_min_x);

    m_stop_user_interaction |= ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopup);

    RenderGrid();
    if (!m_stop_user_interaction) {
        RenderGraphView();
        RenderSplitter(screen_pos);
    }
     
    RenderInteractiveUI(screen_pos);

    RenderScrubber(screen_pos);

    if(!m_resize_activity && !m_stop_user_interaction)
    {
        // Funtion enables user interactions to be captured
        HandleTopSurfaceTouch();
    }

    ImGui::EndChild();  // End of Grid View 2

    ImGui::PushStyleColor(ImGuiCol_ChildBg, m_settings.GetColor(Colors::kTransparent));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));

    ImGui::BeginChild("scrollbar",
                      ImVec2(subcomponent_size_main.x, m_artificial_scrollbar_height),
                      true, ImGuiWindowFlags_NoScrollbar);

    // ImGui::SameLine();
    ImGui::Dummy(ImVec2(m_sidebar_size, 0));
    ImGui::SameLine();

    float available_width = subcomponent_size_main.x - m_sidebar_size;

    ImGui::PushStyleVar(ImGuiStyleVar_GrabMinSize,
                        available_width > 0
                            ? std::clamp((subcomponent_size_main.x * (1.0f / m_zoom)),
                                         (available_width * 0.05f),
                                         (available_width * 0.90f))
                            : 4.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_GrabRounding, 3.0f);

    ImU32 scroll_color = m_settings.GetColor(Colors::kFillerColor);
    ImU32 grab_color   = m_settings.GetColor(Colors::kScrollBarColor);

    ImGui::PushStyleColor(ImGuiCol_SliderGrab, grab_color);
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, grab_color);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, scroll_color);
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, scroll_color);
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, scroll_color);

    ImGui::PushItemWidth(subcomponent_size_main.x - m_sidebar_size);

    m_v_width = std::min(m_v_width,
                         m_range_x);  // Ensure view width does not exceed total
                                      // range. Prevents jitter and assertion errors.
    float max_offset  = static_cast<float>((m_range_x - m_v_width) + m_v_width * 0.10f);
    float min_offset  = 0.0f;
    float view_offset = static_cast<float>(m_view_time_offset_ns);

    ImGui::SliderFloat("##scrollbar", &view_offset, min_offset, max_offset, "%.5f");
    m_view_time_offset_ns = static_cast<double>(view_offset);

    // Clamp the view offset to prevent scrolling out of bounds
    m_view_time_offset_ns = std::clamp(static_cast<double>(view_offset), 0.0,
                                       (m_range_x - m_v_width) + m_v_width * 0.10);

    ImGui::PopStyleColor(5);  // Pop the colors we pushed above
    ImGui::PopStyleVar(2);    // Pop both style variables
    ImGui::PopItemWidth();

    m_stop_user_interaction = false;

    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
}
void
TimelineView::RenderGraphPoints()
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
    ImGuiChildFlags flags = ImGuiChildFlags_None;

    if(ImGui::BeginChild("Main Trace", ImVec2(0, 0), flags))
    {
        RenderTraceView();
    }

    ImGui::EndChild();
    ImGui::PopStyleVar(2);
}

void
TimelineView::HandleHistogramTouch()
{
    ImVec2 container_pos  = ImGui::GetWindowPos();
    ImVec2 container_size = ImGui::GetWindowSize();

    ImVec2 histogram_area_min = ImVec2(container_pos.x, container_pos.y);
    ImVec2 histogram_area_max =
        ImVec2(container_pos.x + m_sidebar_size + m_graph_size.x, container_pos.y + 100);

    bool is_mouse_in_graph =
        ImGui::IsMouseHoveringRect(histogram_area_min, histogram_area_max);

    ImGuiIO& io = ImGui::GetIO();

    // Histogram area: allow full interaction
    if(is_mouse_in_graph)
    {
        if(ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            m_can_drag_to_pan = true;
        }
    }
    if(ImGui::IsMouseReleased(ImGuiMouseButton_Left))
    {
        m_can_drag_to_pan = false;
    }

    // Handle Panning (but only if in Histogram area)
    if(m_can_drag_to_pan && ImGui::IsMouseDragging(ImGuiMouseButton_Left) &&
       is_mouse_in_graph)
    {
        float  drag       = io.MouseDelta.x;
        double view_width = (m_range_x) / m_zoom;

        float user_requested_move = (drag / m_graph_size.x) * m_range_x;

        if(user_requested_move <= 0)
        {
            if(m_view_time_offset_ns <= (m_range_x))
            {
                m_view_time_offset_ns += user_requested_move;
            }
        }
        else
        {
            if(m_view_time_offset_ns >= 0)
            {
                m_view_time_offset_ns += user_requested_move;
            }
        }
        double max_offset =
            std::max(0.0, m_range_x - view_width);  // If zoomed out too much can trigger
                                                    // failure without this line.
        m_view_time_offset_ns = std::clamp(m_view_time_offset_ns, 0.0, max_offset);
    }
}

void
TimelineView::HandleTopSurfaceTouch()
{
    ImVec2 container_pos  = ImGui::GetWindowPos();
    ImVec2 container_size = ImGui::GetWindowSize();

    // Define sidebar and graph areas
    ImVec2 sidebar_min = container_pos;
    ImVec2 sidebar_max =
        ImVec2(container_pos.x + m_sidebar_size, container_pos.y + m_graph_size.y);

    ImVec2 graph_area_min = ImVec2(container_pos.x + m_sidebar_size, container_pos.y);
    ImVec2 graph_area_max = ImVec2(container_pos.x + m_sidebar_size + m_graph_size.x,
                                   container_pos.y + m_graph_size.y);

    bool is_mouse_in_sidebar = ImGui::IsMouseHoveringRect(sidebar_min, sidebar_max);
    bool is_mouse_in_graph   = ImGui::IsMouseHoveringRect(graph_area_min, graph_area_max);

    ImGuiIO&    io         = ImGui::GetIO();
    const float zoom_speed = 0.1f;

    bool mouse_any = io.MouseDown[ImGuiMouseButton_Left] ||
                     io.MouseDown[ImGuiMouseButton_Right] || io.MouseDown[ImGuiMouseButton_Middle];

    // Sidebar: scroll wheel pans vertically
    if(is_mouse_in_sidebar)
    {
        if(mouse_any && !m_pseudo_focus)
        {
            m_pseudo_focus = true;
        }

        float scroll_wheel = io.MouseWheel;
        if(scroll_wheel != 0.0f)
        {
            // Adjust scroll speed as needed (here, 40.0f per scroll step)
            float scroll_speed = 100.0f;
            m_scroll_position_y =
                std::clamp(m_scroll_position_y - scroll_wheel * scroll_speed, 0.0f,
                           m_content_max_y_scroll);
        }
    }
    // Graph area: allow full interaction
    else if(is_mouse_in_graph)
    {
        if(mouse_any && !m_pseudo_focus)
        {
            m_pseudo_focus = true;
        }

        if(ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            m_can_drag_to_pan = true;
        }

        // Enables horizontal scrolling using mouse.
        float scroll_wheel_h = io.MouseWheelH;
        if(scroll_wheel_h != 0.0f)
        {
            float move_amount = scroll_wheel_h * m_v_width * zoom_speed;
            m_view_time_offset_ns -= move_amount;

            if(m_view_time_offset_ns < 0.0f) m_view_time_offset_ns = 0.0f;
            if(m_view_time_offset_ns > m_range_x - m_v_width)
                m_view_time_offset_ns = m_range_x - m_v_width;
        }

        // Handle Zoom at Cursor
        float scroll_wheel = io.MouseWheel;
        if(scroll_wheel != 0.0f)
        {
            // 1. Get mouse position relative to graph area
            ImVec2 mouse_pos        = ImGui::GetMousePos();
            ImVec2 graph_pos        = graph_area_min;
            float  mouse_x_in_graph = mouse_pos.x - graph_pos.x;

            // 2. Calculate the world coordinate under the cursor before zoom
            float  cursor_screen_percentage = mouse_x_in_graph / m_graph_size.x;
            double x_under_cursor =
                m_view_time_offset_ns + cursor_screen_percentage * m_v_width;

            // 3. Apply zoom

            float new_zoom = m_zoom;
            if(scroll_wheel > 0)
            {
                if(m_pixels_per_ns < 1.0)
                {
                    new_zoom *= 1.0f + zoom_speed;
                }
            }
            else
            {
                new_zoom *= 1.0f - zoom_speed;
            }
            new_zoom = std::max(new_zoom, 0.9f);

            // 4. Calculate new view width
            double new_v_width = m_range_x / new_zoom;

            // 5. Adjust m_movement so the world_x_under_cursor stays under the cursor
            m_view_time_offset_ns =
                x_under_cursor - cursor_screen_percentage * new_v_width;

            // 6. Update zoom and view width
            m_zoom    = new_zoom;
            m_v_width = new_v_width;
            m_v_min_x = m_min_x + m_view_time_offset_ns;
            m_v_max_x = m_v_min_x + m_v_width;
        }
    }
    else if(mouse_any)
    {
        //mouse activity outside the graph area removes pseudo focus
        m_pseudo_focus = false;
    }

    // Only handle keyboard input if not typing in a text input and no item is active
    // and this view has focus
    if(m_pseudo_focus || m_histogram_pseudo_focus && !io.WantTextInput && !ImGui::IsAnyItemActive())
    {
        // WASD and Arrow key panning
        float pan_speed_sped_up = 2;
        bool  is_shift_down     = ImGui::GetIO().KeyShift;

        float pan_speed = is_shift_down ? pan_speed_sped_up : 1.0f;

        float region_moved_per_click_x = 0.01 * m_graph_size.x;
        float region_moved_per_click_y = 0.01 * m_content_max_y_scroll;

        //A, D, left arrow, right arrow go left and right
        if(ImGui::IsKeyPressed(ImGuiKey_A) || ImGui::IsKeyPressed(ImGuiKey_LeftArrow))
        {
            m_view_time_offset_ns -=
                pan_speed * ((region_moved_per_click_x / m_graph_size.x) * m_v_width);
        }
        if(ImGui::IsKeyPressed(ImGuiKey_D) || ImGui::IsKeyPressed(ImGuiKey_RightArrow))
        {
            m_view_time_offset_ns -=
                pan_speed * ((-region_moved_per_click_x / m_graph_size.x) * m_v_width);
        }

        float new_zoom = 0.0f;
        // W/S for zoom in/out
        if(ImGui::IsKeyPressed(ImGuiKey_W))
        {
            // Zoom in
            new_zoom = m_zoom * (1.0f + zoom_speed * pan_speed);
            new_zoom = std::max(new_zoom, 0.9f);
        }
        if(ImGui::IsKeyPressed(ImGuiKey_S))
        {
            // Zoom out
            new_zoom = m_zoom * (1.0f - zoom_speed * pan_speed);
            new_zoom = std::max(new_zoom, 0.9f);
        }
        if(new_zoom > 0.0f)
        {
            // Center zoom at current view center
            double center_ns      = m_view_time_offset_ns + m_v_width * 0.5;
            double new_v_width    = m_range_x / new_zoom;
            m_view_time_offset_ns = center_ns - new_v_width * 0.5;
            m_zoom                = new_zoom;
            m_v_width             = new_v_width;
            m_v_min_x             = m_min_x + m_view_time_offset_ns;
            m_v_max_x             = m_v_min_x + m_v_width;
        }

        // Up/Down arrows for vertical scroll
        if(ImGui::IsKeyPressed(ImGuiKey_UpArrow))
        {
            m_scroll_position_y =
                std::clamp(m_scroll_position_y - pan_speed * region_moved_per_click_y, 0.0f,
                    m_content_max_y_scroll);
        }
        if(ImGui::IsKeyPressed(ImGuiKey_DownArrow))
        {
            m_scroll_position_y =
                std::clamp(m_scroll_position_y + pan_speed * region_moved_per_click_y, 0.0f,
                    m_content_max_y_scroll);
        }
    }

    // Stop panning if mouse released
    if(ImGui::IsMouseReleased(ImGuiMouseButton_Left))
    {
        m_can_drag_to_pan = false;
    }

    // Handle Panning (but only if in graph area)
    if(m_can_drag_to_pan && ImGui::IsMouseDragging(ImGuiMouseButton_Left) &&
       is_mouse_in_graph)
    {
        float drag_y = io.MouseDelta.y;
        m_scroll_position_y =
            std::clamp(m_scroll_position_y - drag_y, 0.0f, m_content_max_y_scroll);
        float  drag       = io.MouseDelta.x;
        double view_width = (m_range_x) / m_zoom;

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

ViewCoords
TimelineView::GetViewCoords() const
{
    return { m_scroll_position_y, m_zoom, m_v_min_x, m_v_max_x };
}

TimelineArrow&
TimelineView::GetArrowLayer()
{
    return m_arrow_layer;
}

TimelineViewProjectSettings::TimelineViewProjectSettings(const std::string& project_id,
                                                         TimelineView&      timeline_view)
: ProjectSetting(project_id)
, m_timeline_view(timeline_view)
{}

TimelineViewProjectSettings::~TimelineViewProjectSettings() {}

void
TimelineViewProjectSettings::ToJson()
{
    const std::vector<rocprofvis_graph_t>& graphs = *m_timeline_view.GetGraphs();
    for(int i = 0; i < graphs.size(); i++)
    {
        uint64_t id = graphs[i].chart->GetID();
        m_settings_json[JSON_KEY_GROUP_TIMELINE][JSON_KEY_TIMELINE_TRACK_ORDER][i] = id;
        m_settings_json[JSON_KEY_GROUP_TIMELINE][JSON_KEY_TIMELINE_TRACK][id]
                       [JSON_KEY_TIMELINE_TRACK_DISPLAY] = graphs[i].display;
    }
}

bool
TimelineViewProjectSettings::Valid() const
{
    bool valid = false;
    if(m_settings_json[JSON_KEY_GROUP_TIMELINE][JSON_KEY_TIMELINE_TRACK_ORDER].isArray())
    {
        std::vector<jt::Json>& track_order =
            m_settings_json[JSON_KEY_GROUP_TIMELINE][JSON_KEY_TIMELINE_TRACK_ORDER]
                .getArray();
        if(track_order.size() == m_timeline_view.m_graphs->size())
        {
            int valid_count = 0;
            for(jt::Json& track_id : track_order)
            {
                if(track_id.isLong())
                {
                    valid_count++;
                }
                else
                {
                    break;
                }
            }
            valid = (valid_count == track_order.size());
        }
    }
    if(valid)
    {
        valid = false;
        if(m_settings_json[JSON_KEY_GROUP_TIMELINE][JSON_KEY_TIMELINE_TRACK].isArray())
        {
            std::vector<jt::Json>& tracks =
                m_settings_json[JSON_KEY_GROUP_TIMELINE][JSON_KEY_TIMELINE_TRACK]
                    .getArray();
            if(tracks.size() == m_timeline_view.m_graphs->size())
            {
                int valid_count = 0;
                for(jt::Json& track_id : tracks)
                {
                    if(track_id[JSON_KEY_TIMELINE_TRACK_DISPLAY].isBool())
                    {
                        valid_count++;
                    }
                    else
                    {
                        break;
                    }
                }
                valid = (valid_count == tracks.size());
            }
        }
    }
    return valid;
}

uint64_t
TimelineViewProjectSettings::TrackID(int index) const
{
    return m_settings_json[JSON_KEY_GROUP_TIMELINE][JSON_KEY_TIMELINE_TRACK_ORDER][index]
        .getLong();
}

bool
TimelineViewProjectSettings::DisplayTrack(uint64_t track_id) const
{
    return m_settings_json[JSON_KEY_GROUP_TIMELINE][JSON_KEY_TIMELINE_TRACK][track_id]
                          [JSON_KEY_TIMELINE_TRACK_DISPLAY]
                              .getBool();
}

}  // namespace View
}  // namespace RocProfVis