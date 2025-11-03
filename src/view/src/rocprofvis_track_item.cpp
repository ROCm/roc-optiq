// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_track_item.h"
#include "icons/rocprovfis_icon_defines.h"
#include "rocprofvis_font_manager.h"
#include "rocprofvis_settings_manager.h"
#include "rocprofvis_utils.h"
#include "spdlog/spdlog.h"
#include "widgets/rocprofvis_gui_helpers.h"

namespace RocProfVis
{
namespace View
{

float            TrackItem::s_metadata_width = 400.0f;

TrackItem::TrackItem(DataProvider& dp, uint64_t id, std::string name, float zoom,
                     double time_offset_ns, double& min_x, double& max_x, double scale_x)
: m_data_provider(dp)
, m_id(id)
, m_zoom(zoom)
, m_time_offset_ns(time_offset_ns)
, m_min_x(min_x)
, m_max_x(max_x)
, m_scale_x(scale_x)
, m_name(name)
, m_track_height(75.0f)
, m_track_default_height(75.0f)
, m_track_content_height(0.0f)
, m_min_track_height(10.0f)
, m_is_in_view_vertical(false)
, m_metadata_padding(ImVec2(4.0f, 4.0f))
, m_resize_grip_thickness(4.0f)
, m_request_state(TrackDataRequestState::kIdle)
, m_track_height_changed(false)
, m_meta_area_clicked(false)
, m_meta_area_scale_width(0.0f)
, m_settings(SettingsManager::GetInstance())
, m_selected(false)
, m_reorder_grip_width(20.0f)
, m_group_id_counter(0)
, m_chunk_duration_ns(TimeConstants::ns_per_s *
                      30)  // Default chunk duration
, m_project_settings(m_data_provider.GetTraceFilePath(), *this)
{
    if(m_project_settings.Valid())
    {
        m_track_height = m_project_settings.Height();
    }
}

bool
TrackItem::TrackHeightChanged()
{
    bool height_changed    = m_track_height_changed;
    m_track_height_changed = false;
    return height_changed;
}
float
TrackItem::GetTrackHeight()
{
    return m_track_height;
}

const std::string&
TrackItem::GetName()
{
    return m_name;
}

uint64_t
TrackItem::GetID()
{
    return m_id;
}

void
TrackItem::SetSidebarSize(int sidebar_size)
{
    s_metadata_width = static_cast<float>(sidebar_size);
}

bool
TrackItem::IsInViewVertical()
{
    return m_is_in_view_vertical;
}

void
TrackItem::SetDistanceToView(float distance)
{
    m_distance_to_view_y = distance;
}

float
TrackItem::GetDistanceToView()
{
    return m_distance_to_view_y;
}

void
TrackItem::SetInViewVertical(bool in_view)
{
    m_is_in_view_vertical = in_view;
}

void
TrackItem::SetID(uint64_t id)
{
    m_id = id;
}

std::tuple<double, double>
TrackItem::GetMinMax()
{
    return std::make_tuple(m_min_x, m_max_x);
}

bool
TrackItem::IsSelected() const
{
    return m_selected;
}

void
TrackItem::SetSelected(bool selected)
{
    m_selected = selected;
}

void
TrackItem::UpdateMovement(float zoom, double time_offset_ns, double& min_x, double& max_x,
                          double scale_x, float y_scroll_position)
{
    m_zoom           = zoom;
    m_time_offset_ns = time_offset_ns;
    m_scale_x        = scale_x;
    m_min_x          = min_x;
    m_max_x          = max_x;
    (void) y_scroll_position;
}

void
TrackItem::Render(float width)
{
    ImGui::BeginGroup();

    RenderMetaArea();
    ImGui::SameLine();

    RenderChart(width);
    RenderResizeBar(ImVec2(width + s_metadata_width, m_track_height));

    ImGui::EndGroup();

    if(ImGui::IsItemVisible())
    {
        m_is_in_view_vertical = true; 
    }
    else
    {
        m_is_in_view_vertical = false;
    }
}

float
TrackItem::GetReorderGripWidth()
{
    return m_reorder_grip_width;
}

void 
TrackItem::UpdateMaxMetaAreaSize(float new_size)
{
    m_meta_area_scale_width = std::max(CalculateNewMetaAreaSize(), new_size);
}

float
TrackItem::CalculateNewMetaAreaSize()
{
    return m_meta_area_scale_width;
}

void
TrackItem::RenderMetaAreaExpand()
{
    // no-op
}

void
TrackItem::RenderMetaArea()
{
    // Shrink the meta data content area by one unit in the vertical direction so that the
    // borders rendered by the parent are visible other wise the bg fill will cover them
    // up.
    ImVec2 metadata_shrink_padding(0.0f, 1.0f);
    ImVec2 outer_container_size = ImGui::GetContentRegionAvail();
    m_track_content_height      = m_track_height - metadata_shrink_padding.y * 2.0f;

    // Global padding is too large for metadata must compress.
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(1, 2));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(1, 2));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(1, 2));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(2, 4));

    ImGui::PushStyleColor(ImGuiCol_ChildBg,
                          m_selected
                              ? m_settings.GetColor(Colors::kMetaDataColorSelected)
                              : (m_request_state == TrackDataRequestState::kError
                                     ? m_settings.GetColor(Colors::kGridRed)
                                     : m_settings.GetColor(Colors::kMetaDataColor)));
    ImGui::SetCursorPos(metadata_shrink_padding);
    if(ImGui::BeginChild("MetaData Area",
                         ImVec2(s_metadata_width, outer_container_size.y -
                                                      metadata_shrink_padding.y * 2.0f),
                         ImGuiChildFlags_None,
                         ImGuiWindowFlags_NoScrollbar |
                             ImGuiWindowFlags_NoScrollWithMouse))
    {
        ImVec2 content_size = ImGui::GetContentRegionAvail();

        // handle mouse click
        ImVec2 container_pos  = ImGui::GetWindowPos() + ImVec2(m_reorder_grip_width, 0);
        ImVec2 container_size = ImGui::GetWindowSize();

        // Reordering grip decoration
        ImGui::SetCursorPos(
            ImVec2((m_reorder_grip_width - ImGui::CalcTextSize(ICON_GRID).x) / 2,
                   (container_size.y - ImGui::GetTextLineHeightWithSpacing()) / 2));
        ImGui::PushFont(m_settings.GetFontManager().GetIconFont(FontType::kDefault));
        ImGui::TextUnformatted(ICON_GRID);

        float menu_button_width = ImGui::CalcTextSize(ICON_GEAR).x;
        ImGui::PopFont();

        ImGui::SetCursorPos(m_metadata_padding + ImVec2(m_reorder_grip_width, 0));
        // Adjust content size to account for padding
        content_size.x -= m_metadata_padding.x * 2;
        content_size.y -= m_metadata_padding.x * 2;

        // TODO: For testing and debugging request cancellation on the backend
        // Remove once this feature is stable
        // if(HasPendingRequests())
        // {
        //     if(ImGui::Button("Cancel Request"))
        //     {
        //         for(const auto& [request_id, req] : m_pending_requests)
        //         {
        //             m_data_provider.CancelRequest(request_id);
        //         }
        //     }
        // }

        ImGui::PushTextWrapPos(content_size.x - m_meta_area_scale_width -
                               (menu_button_width + 2 * m_metadata_padding.x));

            ImFont* large_font = m_settings.GetFontManager().GetFont(FontType::kLarge);

        ImGui::PushFont(large_font);

        ImGui::TextUnformatted(m_name.c_str());

        ImGui::PopFont();   

        ImGui::PopTextWrapPos();

        if(m_request_state != TrackDataRequestState::kIdle)
        {
            ImGuiStyle& style = ImGui::GetStyle();

            float  dot_radius  = 10.0f;
            int    num_dots    = 3;
            float  dot_spacing = 5.0f;
            float  anim_speed  = 7.0f;
            ImVec2 dot_size =
                MeasureLoadingIndicatorDots(dot_radius, num_dots, dot_spacing);

            ImVec2 cursor_pos = ImGui::GetCursorPos();
            ImGui::SetCursorPos(
                ImVec2(cursor_pos.x + (content_size.x - dot_size.x) * 0.5f,
                       cursor_pos.y + style.ItemSpacing.y));

            RenderLoadingIndicatorDots(dot_radius, num_dots, dot_spacing,
                                       m_settings.GetColor(Colors::kScrollBarColor),
                                       anim_speed);
        }

        ImGui::SetCursorPos(ImVec2(m_metadata_padding.x + content_size.x -
                                       m_meta_area_scale_width - menu_button_width,
                                   0));
        ImGui::PushStyleColor(ImGuiCol_Button, m_settings.GetColor(Colors::kTransparent));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                              m_settings.GetColor(Colors::kTransparent));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                              m_settings.GetColor(Colors::kTransparent));
        ImGui::PushFont(m_settings.GetFontManager().GetIconFont(FontType::kDefault));
        ImGui::Button(ICON_GEAR);
        ImGui::PopFont();
        ImGui::PopStyleColor(3);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, 
                            m_settings.GetDefaultStyle().WindowPadding);
        if(ImGui::BeginItemTooltip())
        {
            ImGui::TextUnformatted("Track Options");
            ImGui::EndTooltip();
        }
        ImGui::SetNextWindowPos(ImGui::GetCursorScreenPos() +
                                ImVec2(content_size.x - m_meta_area_scale_width -
                                           menu_button_width -
                                           ImGui::GetStyle().FramePadding.x,
                                       0));
        if(ImGui::BeginPopupContextItem("", ImGuiPopupFlags_MouseButtonLeft))
        {
            RenderMetaAreaOptions();
            ImGui::EndPopup();
        }
        ImGui::PopStyleVar();
        RenderMetaAreaScale();
        RenderMetaAreaExpand();
    }
    ImGui::EndChild();  // end metadata area
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(4);
    if(ImGui::IsItemClicked(ImGuiMouseButton_Left))
    {
        m_meta_area_clicked = true;
    }
    else
    {
        m_meta_area_clicked = false;
    }
}

void
TrackItem::RenderResizeBar(const ImVec2& parent_size)
{
    ImGui::SetCursorPos(ImVec2(0, parent_size.y - m_resize_grip_thickness));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, m_settings.GetColor(Colors::kTransparent));
    ImGui::BeginChild("Resize Bar", ImVec2(parent_size.x, m_resize_grip_thickness),
                      false);

    ImGui::Selectable(("##MovePositionLine" + std::to_string(m_id)).c_str(), false,
                      ImGuiSelectableFlags_AllowDoubleClick,
                      ImVec2(0, m_resize_grip_thickness));
    if(ImGui::IsItemHovered())
    {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
    }

    if(ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
    {
        ImVec2 drag_delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);
        m_track_height    = m_track_height + (drag_delta.y);
        if(m_track_height < m_min_track_height)
        {
            m_track_height = m_min_track_height;
        }

        ImGui::ResetMouseDragDelta();
        ImGui::EndDragDropSource();
        m_track_height_changed = true;
    }
    if(ImGui::BeginDragDropTarget())
    {
        ImGui::EndDragDropTarget();
    }
    ImGui::EndChild();  // end resize handle
    ImGui::PopStyleColor();
}

void
TrackItem::RequestData(double min, double max, float width)
{
    // create request chunks with ranges of m_chunk_duration_ns  max
    double range       = max - min;
    size_t chunk_count = static_cast<size_t>(std::ceil(range / m_chunk_duration_ns));
    m_group_id_counter++;
    std::deque<TrackRequestParams> temp_request_queue;

    for(size_t i = 0; i < chunk_count; ++i)
    {
        double chunk_start = min + i * TimeConstants::minute_in_ns;
        double chunk_end   = std::min(chunk_start + TimeConstants::minute_in_ns, max);

        double chunk_range = chunk_end - chunk_start;
        float  percentage  = static_cast<float>(chunk_range / range);
        float  chunk_width = width * percentage;

        TrackRequestParams request_params(static_cast<uint32_t>(m_id), chunk_start, chunk_end,
                                          static_cast<uint32_t>(chunk_width),
                                          m_group_id_counter, 
                                          static_cast<uint16_t>(i), chunk_count);

        temp_request_queue.push_back(request_params);
        spdlog::debug("Queueing request for track {}: {} to {} ({} ns) with width {}",
                      m_id, chunk_start, chunk_end, chunk_range, chunk_width);
    }

    if(!m_request_queue.empty())
    {
        m_request_queue.clear();
        spdlog::warn("Overwriting existing request queue for track {}", m_id);
    }
    m_request_queue = std::move(temp_request_queue);

    if(m_request_state == TrackDataRequestState::kIdle)
    {
        FetchHelper();
    }
    else
    {
        spdlog::warn(
            "Fetch request deferred for track {}, requests are already pending...", m_id);

        for(const auto& [request_id, req] : m_pending_requests)
        {
            spdlog::debug("RequestData: Found pending request {} for track {}",
                          request_id, m_id);
            m_data_provider.CancelRequest(request_id);
        }
    }
}

void
TrackItem::Update()
{
    if(m_request_state == TrackDataRequestState::kIdle)
    {
        if(!m_request_queue.empty())
        {
            FetchHelper();
        }
    }
}

void
TrackItem::FetchHelper()
{
    while(!m_request_queue.empty())
    {
        TrackRequestParams&       req    = m_request_queue.front();
        std::pair<bool, uint64_t> result = m_data_provider.FetchTrack(req);
        if(!result.first)
        {
            spdlog::error("Request for track {} failed", m_id);
        }
        else
        {
            spdlog::debug(
                "Fetching from {} to {} ( {} ) at zoom {} for track {} part of group {}",
                req.m_start_ts, req.m_end_ts, req.m_end_ts - req.m_start_ts, m_zoom, m_id,
                req.m_data_group_id);

            m_request_state = TrackDataRequestState::kRequesting;
            // Store the request with its ID
            m_pending_requests.insert({ result.second, req });
        }
        m_request_queue.pop_front();
    }
}

bool
TrackItem::HandleTrackDataChanged(uint64_t request_id, uint64_t response_code)
{
    (void) response_code;  // Unused at the moment
    bool result = false;
    if(!m_pending_requests.erase(request_id))
    {
        spdlog::warn("Failed to erase pending request {}", request_id);
    }

    result = ExtractPointsFromData();

    return result;
}

bool
TrackItem::HasData()
{
    return m_data_provider.GetRawTrackData(m_id) != nullptr;
}

bool
TrackItem::ReleaseData()
{
    bool result = m_data_provider.FreeTrack(m_id, true);
    if(!result)
    {
        spdlog::warn("Failed to release data for track {}", m_id);
    }

    // Clear pending requests
    for(auto it = m_pending_requests.begin(); it != m_pending_requests.end();)
    {
        const auto request_id = it->first;
        if(m_data_provider.CancelRequest(request_id))
        {
            it = m_pending_requests.erase(it);
        }
        else
        {
            spdlog::warn("Failed to cancel pending request {} for track {}", request_id,
                         m_id);
            ++it;
        }
    }

    return result;
}

bool
TrackItem::HasPendingRequests() const
{
    return !m_pending_requests.empty();
}

TrackProjectSettings::TrackProjectSettings(const std::string& project_id,
                                           TrackItem&         track_item)
: ProjectSetting(project_id)
, m_track_item(track_item)
{}

TrackProjectSettings::~TrackProjectSettings() {}

void
TrackProjectSettings::ToJson()
{
    m_settings_json[JSON_KEY_GROUP_TIMELINE][JSON_KEY_TIMELINE_TRACK]
                   [m_track_item.GetID()][JSON_KEY_TIMELINE_TRACK_HEIGHT] =
                       m_track_item.GetTrackHeight();
}

bool
TrackProjectSettings::Valid() const
{
    return m_settings_json[JSON_KEY_GROUP_TIMELINE][JSON_KEY_TIMELINE_TRACK]
                          [m_track_item.GetID()][JSON_KEY_TIMELINE_TRACK_HEIGHT]
                              .isNumber();
}

float
TrackProjectSettings::Height() const
{
    return static_cast<float>(
        m_settings_json[JSON_KEY_GROUP_TIMELINE][JSON_KEY_TIMELINE_TRACK]
                       [m_track_item.GetID()][JSON_KEY_TIMELINE_TRACK_HEIGHT]
                           .getNumber());
}

}  // namespace View
}  // namespace RocProfVis
