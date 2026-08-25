// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_track_item.h"
#include "icons/rocprovfis_icon_defines.h"
#include "rocprofvis_font_manager.h"
#include "rocprofvis_settings_manager.h"
#include "rocprofvis_timeline_selection.h"
#include "rocprofvis_timeline_track_options.h"
#include "rocprofvis_utils.h"
#include "spdlog/spdlog.h"
#include "widgets/rocprofvis_gui_helpers.h"
#include "widgets/rocprofvis_notification_manager.h"
#include "widgets/rocprofvis_widget.h"
#include <algorithm>
#include <memory>

namespace RocProfVis
{
namespace View
{

inline constexpr float    DEFAULT_MIN_TRACK_HEIGHT       = 10.0f;
inline constexpr float    DEFAULT_GRIP_WIDTH             = 20.0f;
inline constexpr uint64_t DEFAULT_CHUNK_DURATION         = TimeConstants::ns_per_s * 30;
inline constexpr float    META_TOOLTIP_MAX_WIDTH         = 320.0f;
inline constexpr uint64_t META_TOOLTIP_COMPACT_COUNT_MIN = 1000;
inline constexpr float    NAME_LABEL_HITBOX_PADDING_X    = 4.0f;
inline constexpr float    NAME_LABEL_HITBOX_PADDING_Y    = 3.0f;
inline constexpr float    META_WINDOW_PADDING_Y          = 2.0f;
inline constexpr float    META_FRAME_PADDING_Y           = 4.0f;
inline constexpr float    META_ITEM_SPACING_Y            = 3.0f;
constexpr const char*     TRACK_COPY_MENU_POPUP_NAME     = "TrackCopyMenu";

float TrackItem::s_metadata_width = 400.0f;

static ImU32
CompareSourceColor(size_t source_index, SettingsManager& settings)
{
    const std::vector<ImU32>& wheel = settings.GetColorWheel();
    if(wheel.empty())
    {
        return settings.GetColor(Colors::kTabAccent);
    }
    // Color by the source's file index so every merged file gets a distinct swatch
    // (not just an A/B two-way split).
    return wheel[source_index % wheel.size()];
}

float
CompareSourceBadgeWidth(const TrackInfo* track_info)
{
    if(!track_info || track_info->compare_source.id.empty())
    {
        return 0.0f;
    }
    return ImGui::CalcTextSize(track_info->compare_source.id.c_str()).x +
           2.0f * ImGui::GetStyle().FramePadding.x;
}

void
RenderCompareSourceBadge(const TrackInfo* track_info, SettingsManager& settings)
{
    if(!track_info || track_info->compare_source.id.empty())
    {
        return;
    }

    const CompareSourceInfo& source = track_info->compare_source;
    ImU32                    color  = CompareSourceColor(track_info->file_id, settings);
    float                    width  = CompareSourceBadgeWidth(track_info);

    ImGui::PushID("compare_source_badge");
    ImGui::PushStyleColor(ImGuiCol_Button, color);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, color);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, color);
    ImGui::PushStyleColor(ImGuiCol_Text, settings.GetColor(Colors::kTextOnAccent));
    ImGui::Button(source.id.c_str(), ImVec2(width, 0.0f));
    ImGui::PopStyleColor(4);
    if(ImGui::IsItemHovered())
    {
        std::string tooltip = "Source";
        if(!source.name.empty())
        {
            tooltip += ": " + source.name;
        }
        if(!source.path.empty())
        {
            tooltip += "\n" + source.path;
        }
        SetTooltipStyled("%s", tooltip.c_str());
    }
    ImGui::PopID();
}

TrackItem::TrackItem(DataProvider& dp, uint64_t id, TimelineTrackOptions& track_options,
                     std::shared_ptr<TimePixelTransform> tpt,
                     std::shared_ptr<TimelineSelection>  timeline_selection)
: m_track_metadata(nullptr)
, m_track_statistics(nullptr)
// Start dirty so a freshly-created track item applies an already-kReady analysis stat on its
// first Update (after a rebuild the stat survives but the pill label must be re-applied).
, m_track_statistics_dirty(true)
, m_track_id(id)
, m_track_content_height(0.0f)
, m_min_track_height(DEFAULT_MIN_TRACK_HEIGHT)
, m_track_height_changed(false)
, m_is_in_view_vertical(false)
, m_distance_to_view_y(0.0f)
, m_metadata_padding(ImVec2(8.0f, 4.0f))
, m_resize_grip_thickness(4.0f)
, m_data_provider(dp)
, m_request_state(TrackDataRequestState::kIdle)
, m_settings(SettingsManager::GetInstance())
, m_meta_area_clicked(false)
, m_meta_area_scale_width(0.0f)
, m_max_meta_area_scale_width(0.0f)
, m_reorder_grip_width(DEFAULT_GRIP_WIDTH)
, m_tpt(tpt)
, m_timeline_selection(timeline_selection)
, m_chunk_duration_ns(DEFAULT_CHUNK_DURATION)
, m_group_id_counter(0)
, m_meta_area_label("")
, m_has_node_color(false)
, m_node_color_index(0)
, m_node_display_index(0)
, m_node_pill(nullptr)
, m_options(nullptr)
, m_timeline_track_options(track_options)
, m_selected(false)
, m_selected_changed_token(EventManager::InvalidSubscriptionToken)
{
    const TrackInfo* track_info =
        m_data_provider.DataModel().GetTimeline().GetTrack(m_track_id);

    if(track_info == nullptr)
    {
        spdlog::error("TrackItem: failed to get TrackInfo for track_id {}", m_track_id);
        return;
    }
    m_track_metadata = track_info;
    m_name           = m_data_provider.DataModel().BuildTrackName(m_track_id);
    SetMetaAreaLabel(track_info);
    SetNodeColor(track_info);
    SetDefaultPillLabel(track_info);

    EventManager::EventHandler selected_changed_handler =
        [this](std::shared_ptr<RocEvent> e) {
            std::shared_ptr<TrackSelectionChangedEvent> evt =
                std::dynamic_pointer_cast<TrackSelectionChangedEvent>(e);
            if(evt && evt->GetSourceId() == m_data_provider.GetTraceFilePath() &&
               (evt->GetTrackID() == m_track_id ||
                evt->GetTrackID() == TimelineSelection::INVALID_SELECTION_ID))
            {
                m_selected = evt->TrackSelected();
            }
        };
    m_selected_changed_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kTimelineTrackSelectionChanged),
        selected_changed_handler);

    m_options = m_timeline_track_options.InitTrack(*this);
    ROCPROFVIS_ASSERT(m_options);
}

TrackItem::~TrackItem()
{
    EventManager::GetInstance()->Unsubscribe(
        static_cast<int>(RocEvents::kTimelineTrackSelectionChanged),
        m_selected_changed_token);
}

bool
TrackItem::HasSavedTrackHeight() const
{
    return m_options && m_options->Valid();
}

float
TrackItem::GetMetaAreaMinHeight() const
{
    const FontManager& fonts  = m_settings.GetFontManager();
    const float        chrome = 2.0f * (META_WINDOW_PADDING_Y + m_metadata_padding.y) +
                         0.5f * m_resize_grip_thickness;
    float min_height  = fonts.GetFontSize(FontSize::kSmall) + chrome;
    float pill_height = 0.0f;

    for(const std::unique_ptr<Pill>& pill : m_pills)
    {
        if(pill->Visible())
        {
            pill_height = std::max(pill_height, pill->Size().y);
        }
    }

    if(pill_height > 0.0f)
    {
        const float title_row_height = ImGui::GetTextLineHeight() +
                                       2.0f * META_FRAME_PADDING_Y + META_ITEM_SPACING_Y;
        min_height = title_row_height + pill_height + chrome;
    }

    return min_height;
}

bool
TrackItem::TrackHeightChanged()
{
    bool height_changed    = m_track_height_changed;
    m_track_height_changed = false;
    return height_changed;
}

float
TrackItem::GetTrackHeight() const
{
    return m_options ? m_options->m_height : DEFAULT_TRACK_HEIGHT;
}

const std::string&
TrackItem::GetName() const
{
    return m_name;
}

uint64_t
TrackItem::GetID() const
{
    return m_track_id;
}

void
TrackItem::SetSidebarSize(float sidebar_size)
{
    s_metadata_width = sidebar_size;
}

bool
TrackItem::IsInViewVertical() const
{
    return m_is_in_view_vertical;
}

void
TrackItem::SetDistanceToView(float distance)
{
    m_distance_to_view_y = distance;
}

float
TrackItem::GetDistanceToView() const
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
    m_track_id = id;
}

bool
TrackItem::IsSelected() const
{
    return m_selected;
}

bool
TrackItem::IsDisplayed() const
{
    return m_options ? m_options->m_display : true;
}

void
TrackItem::SetDisplay(bool display)
{
    if(m_options)
    {
        m_options->m_display = display;
    }
}

void
TrackItem::Render(float width)
{
    ImGui::BeginGroup();

    RenderMetaArea();
    ImGui::SameLine();

    RenderChart(width);
    RenderResizeBar(ImVec2(width + s_metadata_width, GetTrackHeight()));

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

const TrackInfo*
TrackItem::GetTrackInfo() const
{
    return m_track_metadata;
}

void
TrackItem::UpdateMetaScaleAreaSize()
{
    // no-op;
}

void
TrackItem::UpdateMaxMetaScaleAreaSize()
{
    // no-op;
}

void
TrackItem::RenderMetaAreaExpand()
{
    // no-op
}

void
TrackItem::RenderMetaAreaScale()
{
    // no-op
}

float
TrackItem::GetMetaAreaTrailingWidth() const
{
    return 0.0f;
}

void
TrackItem::RenderMetaArea()
{
    ImVec2 outer_container_size = ImGui::GetContentRegionAvail();
    m_track_content_height      = GetTrackHeight() - 0.5f * m_resize_grip_thickness;

    ImVec2 name_label_min(0.0f, 0.0f);
    ImVec2 name_label_max(0.0f, 0.0f);
    bool   name_label_visible = false;

    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(4, 4));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, META_FRAME_PADDING_Y));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, META_ITEM_SPACING_Y));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(5, META_WINDOW_PADDING_Y));
    // Keep the meta-area square so the selection highlight fill reaches the corners
    // instead of bleeding through rounded edges.
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0.0f);

    ImGui::PushStyleColor(ImGuiCol_ChildBg,
                          m_selected
                              ? m_settings.GetColor(Colors::kMetaDataColorSelected)
                              : (m_request_state == TrackDataRequestState::kError
                                     ? m_settings.GetColor(Colors::kGridRed)
                                     : m_settings.GetColor(Colors::kMetaDataColor)));
    if(ImGui::BeginChild(
           "MetaData Area", ImVec2(s_metadata_width, outer_container_size.y),
           ImGuiChildFlags_None,
           ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
    {
        ImVec2 content_size = ImGui::GetContentRegionAvail();

        // handle mouse click
        ImVec2 container_pos  = ImGui::GetWindowPos() + ImVec2(m_reorder_grip_width, 0);
        ImVec2 container_size = ImGui::GetWindowSize();

        if(m_request_state != TrackDataRequestState::kIdle)
        {
            float  dot_radius  = 10.0f;
            int    num_dots    = 3;
            float  dot_spacing = 5.0f;
            float  anim_speed  = 7.0f;
            ImVec2 dot_size =
                MeasureLoadingIndicatorDots(dot_radius, num_dots, dot_spacing);

            ImVec2 cursor_pos = ImGui::GetCursorPos();
            ImGui::SetCursorPos(
                ImVec2(cursor_pos.x + (content_size.x - dot_size.x) * 0.5f,
                       cursor_pos.y + (content_size.y - dot_size.y) * 0.5f));

            RenderLoadingIndicatorDots(dot_radius, num_dots, dot_spacing,
                                       m_settings.GetColor(Colors::kScrollBarColor),
                                       anim_speed);
        }

        // Reordering grip decoration
        float grid_icon_width = ImGui::CalcTextSize(ICON_GRID).x;

        ImGui::SetCursorPos(
            ImVec2((m_reorder_grip_width - grid_icon_width) / 2,
                   (container_size.y - ImGui::GetTextLineHeightWithSpacing()) / 2));
        ImGui::PushFont(m_settings.GetFontManager().GetFont(FontType::kIcon), 0.0f);

        ImGui::TextUnformatted(ICON_GRID);
        ImGui::PopFont();

        ImGui::SetCursorPos(m_metadata_padding + ImVec2(m_reorder_grip_width, 0));
        // Adjust content size to account for padding
        content_size.x -= m_metadata_padding.x * 2;
        content_size.y = std::max(0.0f, content_size.y - m_metadata_padding.y * 2.0f);

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

        UpdateMetaScaleAreaSize();

        float compare_badge_width = CompareSourceBadgeWidth(m_track_metadata);
        if(compare_badge_width > 0.0f)
        {
            compare_badge_width += ImGui::GetStyle().ItemSpacing.x;
        }
        float available_for_text = content_size.x - m_meta_area_scale_width -
                                   m_reorder_grip_width - compare_badge_width -
                                   GetMetaAreaTrailingWidth();

        if(available_for_text < 0.0f) available_for_text = 0.0f;

        // Small text keeps titles readable in compact tracks.
        ImGui::PushFont(m_settings.GetFontManager().GetFont(FontType::kDefault),
                        m_settings.GetFontManager().GetFontSize(FontSize::kSmall));

        ImVec2 track_name_size = ImGui::CalcTextSize(m_meta_area_label.c_str());

        ImGui::BeginGroup();
        ImGui::PushStyleColor(ImGuiCol_Text, m_settings.GetColor(Colors::kTextMain));
        if(compare_badge_width > 0.0f)
        {
            RenderCompareSourceBadge(m_track_metadata, m_settings);
            ImGui::SameLine();
        }
        if(available_for_text > 0.0f)
        {
            const ImVec2 label_start = ImGui::GetCursorScreenPos();
            ImGui::PushID("meta_area_label");
            ElidedText(m_meta_area_label.c_str(), available_for_text);
            ImGui::PopID();

            const float  label_width = std::min(track_name_size.x, available_for_text);
            const ImVec2 hit_padding(NAME_LABEL_HITBOX_PADDING_X,
                                     NAME_LABEL_HITBOX_PADDING_Y);
            name_label_min     = label_start - hit_padding;
            name_label_max     = ImVec2(label_start.x + label_width + hit_padding.x,
                                        label_start.y + track_name_size.y + hit_padding.y);
            name_label_visible = true;
        }
        ImGui::PopStyleColor();
        ImGui::EndGroup();
        ImGui::PopFont();

        // The node pill is toggled from Edit -> Preferences.
        if(m_node_pill)
        {
            m_node_pill->SetVisible(m_has_node_color && m_settings.ShowNodeColors());
        }

        RenderMetaAreaScale();
        RenderMetaAreaExpand();
        RenderPills(ImVec2(available_for_text, content_size.y));

        ImGui::GetWindowDrawList()->AddLine(
            ImGui::GetWindowPos() + ImVec2(0.0f, ImGui::GetWindowSize().y),
            ImGui::GetWindowPos() + ImGui::GetWindowSize(),
            m_settings.GetColor(Colors::kMetaDataSeparator), m_resize_grip_thickness);
    }
    ImGui::EndChild();  // end metadata area
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(5);
    if(ImGui::IsItemClicked(ImGuiMouseButton_Left))
    {
        m_meta_area_clicked = true;
    }
    else
    {
        m_meta_area_clicked = false;
    }

    const bool meta_area_hovered = ImGui::IsItemHovered() && !ImGui::IsAnyItemHovered();
    const bool name_label_hovered =
        name_label_visible && ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows) &&
        ImGui::IsMouseHoveringRect(name_label_min, name_label_max);

    const ImGuiStyle& style = m_settings.GetDefaultStyle();

    if(name_label_hovered && !m_meta_area_tooltip.empty())
    {
        const float wrap_width = META_TOOLTIP_MAX_WIDTH - 2.0f * style.WindowPadding.x;
        ImGui::SetNextWindowSizeConstraints(ImVec2(0, 0),
                                            ImVec2(META_TOOLTIP_MAX_WIDTH, FLT_MAX));
        BeginTooltipStyled();
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + wrap_width);
        ImGui::TextUnformatted(m_meta_area_tooltip.c_str());
        ImGui::PopTextWrapPos();
        EndTooltipStyled();
    }

    if(meta_area_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
    {
        m_timeline_track_options.InitContextMenu(*this);
        ImGui::OpenPopup(TRACK_COPY_MENU_POPUP_NAME);
    }
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, style.WindowPadding);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, style.ItemSpacing);
    if(ImGui::BeginPopup(TRACK_COPY_MENU_POPUP_NAME))
    {
        auto copy_to_clipboard = [](const std::string& text) {
            ImGui::SetClipboardText(text.c_str());
            NotificationManager::GetInstance().Show(COPY_DATA_NOTIFICATION.data(),
                                                    NotificationLevel::Info);
        };
        if(IconMenuItem(ICON_COPY, "Copy track name"))
        {
            copy_to_clipboard(m_meta_area_label);
        }
        if(IconMenuItem(ICON_COPY, "Copy track ID"))
        {
            copy_to_clipboard(std::to_string(m_track_id));
        }
        ImGui::Separator();
        if(IconMenuItem(ICON_TREE, "Reveal in topology"))
        {
            EventManager::GetInstance()->AddEvent(std::make_shared<ScrollToTrackEvent>(
                static_cast<int>(RocEvents::kRevealTrackInTopology), m_track_id,
                m_data_provider.GetTraceFilePath()));
        }
        ImGui::Separator();
        if(IconBeginMenu(ICON_GEAR, "Track Options"))
        {
            m_timeline_track_options.RenderContextMenu();
            ImGui::EndMenu();
        }
        // Restore hidden tracks without needing the topology side bar. The entry
        // is only shown when something is actually hidden.
        if(m_timeline_track_options.HasHiddenTracks())
        {
            if(IconBeginMenu(ICON_EYE, "Show Hidden Tracks"))
            {
                m_timeline_track_options.RenderHiddenTracksSubmenu();
                ImGui::EndMenu();
            }
        }
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar(2);
}

void
TrackItem::RenderResizeBar(const ImVec2& parent_size)
{
    ImGui::SetCursorPos(ImVec2(0, parent_size.y - m_resize_grip_thickness));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, m_settings.GetColor(Colors::kTransparent));
    ImGui::BeginChild("Resize Bar", ImVec2(parent_size.x, m_resize_grip_thickness),
                      false);
    ImGui::InvisibleButton("##MovePositionLine", ImVec2(0, m_resize_grip_thickness));
    if(ImGui::IsItemHovered() || ImGui::IsItemActive())
    {
        ImGui::GetWindowDrawList()->AddLine(
            ImGui::GetItemRectMin(),
            ImVec2(ImGui::GetItemRectMax().x, ImGui::GetItemRectMin().y),
            m_settings.GetColor(Colors::kAccent), m_resize_grip_thickness);
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
    }

    if(ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
    {
        if(m_options)
        {
            m_options->m_height = std::max(
                m_options->m_height + ImGui::GetMouseDragDelta(ImGuiMouseButton_Left).y,
                m_min_track_height);
            m_track_height_changed = true;
        }
        ImGui::ResetMouseDragDelta();
        ImGui::EndDragDropSource();
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
    // Nothing valid to request for an empty/inverted range. Guarding here also prevents a
    // negative range from producing a garbage (huge) chunk_count below.
    if(max <= min)
    {
        return;
    }

    // Clamp the requested window to this track's own data range. In a merged/multinode view
    // each file is normalized to its own start time, so a track only has data in
    // [min_ts, max_ts]; the timeline requests the global viewport range for every track, so
    // for shorter files the tail of that range lies past the track's data. Fetching it just
    // yields OutOfRange chunks (wasted fetches, "code 10" log spam, blank overview). Clamp so
    // only the covered portion is requested; scale the pixel width to the clamped fraction so
    // the level-of-detail resolution still matches the pixels actually covered.
    const double requested_range = max - min;
    if(m_track_metadata)
    {
        min = std::max(min, m_track_metadata->min_ts);
        max = std::min(max, m_track_metadata->max_ts);
    }
    if(max <= min)
    {
        // The visible window does not overlap this track's data at all.
        return;
    }
    if(requested_range > 0.0)
    {
        width *= static_cast<float>((max - min) / requested_range);
    }

    // Skip re-issuing an identical request when we already hold that data; otherwise every
    // pan/zoom re-fetches every visible track's whole chunk set each frame.
    if(min == m_last_requested_min && max == m_last_requested_max && HasData())
    {
        return;
    }
    m_last_requested_min = min;
    m_last_requested_max = max;

    // create request chunks with ranges of m_chunk_duration_ns  max
    double range       = max - min;
    size_t chunk_count = static_cast<size_t>(std::ceil(range / m_chunk_duration_ns));
    m_group_id_counter++;
    std::deque<TrackRequestParams> temp_request_queue;

    for(size_t i = 0; i < chunk_count; ++i)
    {
        // Step by the same duration used to compute chunk_count. (Previously this stepped
        // by a fixed minute while chunk_count used m_chunk_duration_ns, which overshot and
        // produced chunks with start > end when the two differed.)
        double chunk_start = min + i * static_cast<double>(m_chunk_duration_ns);
        double chunk_end =
            std::min(chunk_start + static_cast<double>(m_chunk_duration_ns), max);

        double chunk_range = chunk_end - chunk_start;
        float  percentage  = static_cast<float>(chunk_range / range);
        float  chunk_width = width * percentage;

        TrackRequestParams request_params(static_cast<uint32_t>(m_track_id), chunk_start,
                                          chunk_end, static_cast<uint32_t>(chunk_width),
                                          m_group_id_counter, static_cast<uint16_t>(i),
                                          chunk_count);

        temp_request_queue.push_back(request_params);
        spdlog::debug("Queueing request for track {}: {} to {} ({} ns) with width {}",
                      m_track_id, chunk_start, chunk_end, chunk_range, chunk_width);
    }

    if(!m_request_queue.empty())
    {
        m_request_queue.clear();
        spdlog::warn("Overwriting existing request queue for track {}", m_track_id);
    }
    m_request_queue = std::move(temp_request_queue);

    if(m_request_state == TrackDataRequestState::kIdle)
    {
        FetchHelper();
    }
    else
    {
        spdlog::warn(
            "Fetch request deferred for track {}, requests are already pending...",
            m_track_id);

        for(const auto& [request_id, req] : m_pending_requests)
        {
            spdlog::debug("RequestData: Found pending request {} for track {}",
                          request_id, m_track_id);
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
    if(m_track_statistics)
    {
        const AnalysisTrackStatistics::State prior_state = m_track_statistics->state;
        if(prior_state == AnalysisTrackStatistics::kPending)
        {
            RequestAnalysis();
        }
        // Keep the pill's refresh flag set while the stat is still loading, and also on the one
        // frame it resolves inline - an empty-range stat is set kReady by RequestAnalysis itself
        // here, so without catching that kPending->kReady edge the pill would never repaint and
        // would stay greyed.
        const AnalysisTrackStatistics::State current_state = m_track_statistics->state;
        m_track_statistics_dirty =
            current_state < AnalysisTrackStatistics::kReady ||
            (prior_state < AnalysisTrackStatistics::kReady &&
             current_state == AnalysisTrackStatistics::kReady);
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
            spdlog::error("Request for track {} failed", m_track_id);
        }
        else
        {
            spdlog::debug("Fetching from {} to {} ( {} ) for track {} part of group {}",
                          req.m_start_ts, req.m_end_ts, req.m_end_ts - req.m_start_ts,
                          m_track_id, req.m_data_group_id);

            m_request_state = TrackDataRequestState::kRequesting;
            // Store the request with its ID
            m_pending_requests.insert({ result.second, req });
        }
        m_request_queue.pop_front();
    }
}

void
TrackItem::SetDefaultPillLabel(const TrackInfo* track_info)
{
    TopologyDataModel& tdm = m_data_provider.DataModel().GetTopology();

    // Get Processor (device) type label from using track's agent_or_pid, ex: "GPU0".
    // The associated device in topology is unreliable, so we use agent_or_pid to find the
    // device. This may be empty for some tracks.
    std::string       device_type_label;
    const DeviceInfo* device_info = tdm.GetDevice(track_info->agent_or_pid);
    if(device_info)
    {
        tdm.GetDeviceTypeLabel(*device_info, device_type_label);
    }
    Pill* pill = AddPill(true, false);
    switch(track_info->topology.type)
    {
        case TrackInfo::TrackType::Queue:
        {
            std::string pill_label =
                "QUEUE" + (device_type_label.empty() ? "" : " " + device_type_label);
            pill->SetLabel(pill_label);
            break;
        }
        case TrackInfo::TrackType::Stream:
        {
            std::string pill_label =
                "STREAM" + (device_type_label.empty() ? "" : " " + device_type_label);
            pill->SetLabel(pill_label);
            break;
        }
        case TrackInfo::TrackType::Counter:
        {
            std::string pill_label =
                "COUNTER" + (device_type_label.empty() ? "" : " " + device_type_label);
            pill->SetLabel(pill_label);
            break;
        }
        case TrackInfo::TrackType::InstrumentedThread:
        {
            if(const ThreadInfo* thread_info =
                   tdm.GetInstrumentedThread(track_info->topology.id.value);
               thread_info && thread_info->tid == track_info->topology.process_id)
            {
                pill->Activate();
                pill->SetLabel("MAIN THREAD");
            }
            else
            {
                pill->SetLabel("THREAD");
            }
            break;
        }
        case TrackInfo::TrackType::SampledThread:
        {
            pill->SetLabel("SAMPLED THREAD");
            break;
        }
        default:
        {
            pill->SetVisible(false);
            break;
        }
    }

    // Set pill tooltip label
    switch(track_info->topology.type)
    {
        case TrackInfo::TrackType::Queue:
        case TrackInfo::TrackType::Stream:
        case TrackInfo::TrackType::Counter:
        {
            // Get product label from topology model, ex: "AMD Radeon RX 6800 XT"
            if(device_info)
            {
                pill->SetTooltip(device_info->product_name);
            }
            break;
        }
        case TrackInfo::TrackType::InstrumentedThread:
        case TrackInfo::TrackType::SampledThread:
        default:
        {
            break;
        }
    }
}

void
TrackItem::SetMetaAreaLabel(const TrackInfo* track_info)
{
    TopologyDataModel& tdm = m_data_provider.DataModel().GetTopology();

    std::string process_id_str = std::to_string(track_info->topology.process_id);

    bool show_process_id = tdm.ProcessCount() > 1;

    // Node is conveyed by the colored node pill, so the tooltip carries the
    // human-readable name + stable index instead of the raw node id.
    const size_t      node_index = tdm.GetNodeDisplayIndex(track_info->topology.node_id);
    const NodeInfo*   node_info  = tdm.GetNode(track_info->topology.node_id);
    const std::string node_name  = (node_info && !node_info->host_name.empty())
                                       ? node_info->host_name
                                       : "Node " + std::to_string(node_index);

    switch(track_info->topology.type)
    {
        case TrackInfo::TrackType::InstrumentedThread:
        case TrackInfo::TrackType::SampledThread:
        {
            std::string process_name_path;
            if(const ProcessInfo* process_info =
                   tdm.GetProcess(track_info->topology.process_id);
               process_info)
            {
                process_name_path += process_info->command;
            }

            std::string       thread_id;
            const ThreadInfo* thread_info =
                (track_info->topology.type == TrackInfo::TrackType::SampledThread)
                    ? tdm.GetSampledThread(track_info->topology.id.value)
                    : tdm.GetInstrumentedThread(track_info->topology.id.value);
            if(thread_info)
            {
                thread_id = std::to_string(thread_info->tid);
            }

            m_meta_area_label =
                get_executable_name(process_name_path) + " (TID: " + thread_id + ")";
            if(track_info->topology.type == TrackInfo::TrackType::SampledThread)
            {
                m_meta_area_label += " (S)";
            }

            // set tooltip to full path
            m_meta_area_tooltip = process_name_path;
            break;
        }
        case TrackInfo::TrackType::Counter:
        {
            m_meta_area_label = track_info->sub_name;
            if(show_process_id)
            {
                m_meta_area_label += " (PID: " + process_id_str + ")";
            }
            // set tooltip to counter description
            const CounterInfo* counter_info =
                tdm.GetCounter(track_info->topology.id.value);
            if(counter_info)
            {
                m_meta_area_tooltip = counter_info->description;
            }
            break;
        }
        case TrackInfo::TrackType::Queue:
        {
            if(track_info->category != "GPU Queue")
            {
                m_meta_area_label = track_info->category + ": " + track_info->sub_name;
            }
            else
            {
                m_meta_area_label = track_info->sub_name;
            }

            if(show_process_id)
            {
                m_meta_area_label += " (PID: " + process_id_str + ")";
            }
            break;
        }
        case TrackInfo::TrackType::Stream:
        {
            m_meta_area_label = track_info->main_name;

            if(show_process_id)
            {
                m_meta_area_label += " (PID: " + process_id_str + ")";
            }
            break;
        }
        default:
        {
            m_meta_area_label = m_name;
            break;
        }
    }

    const bool is_sample_track = track_info->track_type == kRPVControllerTrackTypeSamples;
    const char* count_label    = is_sample_track ? "Samples" : "Events";

    std::string meta_lines;
    meta_lines += "Track ID: " + std::to_string(track_info->id) + "\n";
    meta_lines += "Node: " + node_name + " [" + std::to_string(node_index) + "]\n";
    meta_lines += "Process ID: " + process_id_str + "\n";
    meta_lines += std::string(count_label) + ": ";
#ifdef ROCPROFVIS_DEVELOPER_MODE
    meta_lines += std::to_string(track_info->num_entries);
#else
    if(track_info->num_entries >= META_TOOLTIP_COMPACT_COUNT_MIN)
    {
        meta_lines += compact_number_format(static_cast<double>(track_info->num_entries));
    }
    else
    {
        meta_lines += std::to_string(track_info->num_entries);
    }
#endif

    if(m_meta_area_tooltip.empty())
    {
        m_meta_area_tooltip = meta_lines;
    }
    else
    {
        m_meta_area_tooltip += "\n\n" + meta_lines;
    }
}

void
TrackItem::SetNodeColor(const TrackInfo* track_info)
{
    TopologyDataModel& tdm = m_data_provider.DataModel().GetTopology();

    // Node decorations only make sense on multi-node traces; a single-node
    // trace looks exactly as it did before this feature.
    const uint64_t            node_id = track_info->topology.node_id;
    const std::vector<ImU32>& wheel   = m_settings.GetColorWheel();
    m_node_display_index              = tdm.GetNodeDisplayIndex(node_id);
    m_has_node_color = tdm.NodeCount() > 1 && m_node_display_index > 0 && !wheel.empty();
    if(!m_has_node_color)
    {
        return;
    }

    const NodeInfo* node = tdm.GetNode(node_id);
    m_node_name          = (node && !node->host_name.empty())
                               ? node->host_name
                               : "Node " + std::to_string(m_node_display_index);

    // The 1-based display index maps to a stable color-wheel slot shared with
    // the pill accent and the sidebar so every node cue matches.
    m_node_color_index = (m_node_display_index - 1) % wheel.size();

    m_node_pill = AddPill(true, true);
    m_node_pill->SetLabel(std::to_string(m_node_display_index));
    m_node_pill->SetAccentColor(m_node_color_index);
    m_node_pill->SetTooltip("Node " + std::to_string(m_node_display_index) + ": " +
                            m_node_name);
}

Pill*
TrackItem::AddPill(bool shown, bool active)
{
    m_pills.emplace_back(std::make_unique<Pill>(shown, active));
    return m_pills.back().get();
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
    return m_data_provider.DataModel().GetTimeline().GetTrackData(m_track_id) != nullptr;
}

bool
TrackItem::ReleaseData()
{
    bool result =
        m_data_provider.DataModel().GetTimeline().FreeTrackData(m_track_id, true);
    if(!result)
    {
        spdlog::warn("Failed to release data for track {}", m_track_id);
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
                         m_track_id);
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

void
TrackItem::RequestAnalysis()
{
    if(!m_track_statistics || !m_timeline_selection)
    {
        return;
    }
    // Fast path for the common settled case: a kReady stat needs nothing, so skip the
    // request-id build and the pending-request map lookup this would otherwise do per visible
    // track every frame.
    if(m_track_statistics->state == AnalysisTrackStatistics::kReady)
    {
        return;
    }
    const uint64_t request_id = RequestIdBuilder::MakeTrackDataRequestId(
        static_cast<uint32_t>(m_track_id), 0, 0,
        RequestType::kFetchAnalysisTrackStatistics);
    const bool request_pending = m_data_provider.IsRequestPending(request_id);
    const AnalysisTrackStatistics::State state = m_track_statistics->state;
    // (Re)fetch when kStale/kPending, or when kRequested but the request is gone: the add
    // path's FreeRequests() drops in-flight futures without processing them, so a stat could
    // otherwise wedge at kRequested (blank pill) forever. Self-heals without per-frame churn -
    // once re-issued the request is pending again until it resolves.
    const bool needs_request =
        state == AnalysisTrackStatistics::kStale ||
        state == AnalysisTrackStatistics::kPending ||
        (state == AnalysisTrackStatistics::kRequested && !request_pending);
    if(needs_request)
    {
        if(request_pending)
        {
            m_data_provider.CancelRequest(request_id);
            m_track_statistics->state = AnalysisTrackStatistics::kPending;
        }
        else
        {
            double start_ts;
            double end_ts;
            if(m_timeline_selection->HasValidTimeRangeSelection())
            {
                m_timeline_selection->GetSelectedTimeRange(start_ts, end_ts);
            }
            else
            {
                start_ts = m_tpt->GetVMinX();
                end_ts   = m_tpt->GetVMaxX();
            }
            // Clamp to the track's own [min_ts, max_ts]. In a merged view a track only has
            // data in its window; requesting the full global range yields an empty slice
            // (OutOfRange) for shorter files, so the pill never populates.
            if(m_track_metadata)
            {
                start_ts = std::max(start_ts, m_track_metadata->min_ts);
                end_ts   = std::min(end_ts, m_track_metadata->max_ts);
            }
            if(start_ts < end_ts)
            {
                AnalysisTrackStatisticsRequestParams params(m_track_id, start_ts, end_ts);
                params.m_generation =
                    m_data_provider.DataModel().GetAnalysis().GetGeneration();
                m_track_statistics->state =
                    m_data_provider.FetchAnalysisTrackStatistics(params)
                        ? AnalysisTrackStatistics::kRequested
                        : AnalysisTrackStatistics::kPending;
            }
            else
            {
                // The analysis range does not intersect this track's data window (or collapses
                // to a point, e.g. a queue with a single event). There is nothing to fetch, so
                // resolve the stat to zero and mark it ready instead of leaving it kPending -
                // which would loop every frame and leave the pill greyed forever.
                m_data_provider.DataModel().GetAnalysis().SetTrackStatisticsEmpty(m_track_id);
            }
        }
    }
}

void
TrackItem::RenderPills(ImVec2 region)
{
    if(!m_pills.empty() &&
       region.y - ImGui::GetFrameHeightWithSpacing() >= m_pills[0]->Size().y)
    {
        float   pill_x_pos     = 0;
        uint8_t pills_visible  = 0;
        uint8_t pills_extended = 0;
        for(size_t i = 0; i < m_pills.size(); i++)
        {
            if(m_pills[i]->Visible())
            {
                pills_visible++;
                if(pill_x_pos + m_pills[i]->ExtSize().x < region.x)
                {
                    pills_extended++;
                    pill_x_pos += m_pills[i]->ExtSize().x +
                                  m_settings.GetDefaultStyle().ItemSpacing.x;
                }
            }
        }
        pill_x_pos = m_reorder_grip_width;
        for(size_t i = 0; i < m_pills.size(); i++)
        {
            if(m_pills[i]->Visible())
            {
                if(pills_visible == pills_extended)
                {
                    m_pills[i]->Render(
                        ImVec2(pill_x_pos, region.y - m_pills[i]->Size().y), m_settings,
                        Pill::kExtended);
                    pill_x_pos +=
                        m_pills[i]->Size().x + m_settings.GetDefaultStyle().ItemSpacing.x;
                }
                else if(pill_x_pos + m_pills[i]->CompactSize().x <
                        region.x + m_reorder_grip_width)
                {
                    m_pills[i]->Render(
                        ImVec2(pill_x_pos, region.y - m_pills[i]->Size().y), m_settings,
                        Pill::kCompact);
                    pill_x_pos +=
                        m_pills[i]->Size().x + m_settings.GetDefaultStyle().ItemSpacing.x;
                }
                else if(pill_x_pos + m_pills[i]->ElidedSize().x <
                        region.x + m_reorder_grip_width)
                {
                    m_pills[i]->Render(
                        ImVec2(pill_x_pos, region.y - m_pills[i]->Size().y), m_settings,
                        Pill::kElided);
                    break;
                }
                else
                {
                    break;
                }
            }
        }
    }
}

Pill::Pill(bool shown, bool active)
: m_show_pill_label(shown)
, m_active(active)
, m_accent_color(std::nullopt)
, m_sizing(kCompact)
, m_compact_label("")
, m_ext_label("")
, m_tooltip("")
, m_widths({})
, m_height(0.0f)
, m_font_changed_token(EventManager::InvalidSubscriptionToken)
{
    CalculateSize();

    auto font_changed_handler = [this](std::shared_ptr<RocEvent> e) {
        (void) e;
        CalculateSize();
    };
    m_font_changed_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kFontSizeChanged), font_changed_handler);
}

Pill::~Pill()
{
    EventManager::GetInstance()->Unsubscribe(
        static_cast<int>(RocEvents::kFontSizeChanged), m_font_changed_token);
}

void
Pill::SetLabel(const std::string& label)
{
    m_compact_label = label;
    CalculateSize();
}

void
Pill::SetExtendedLabel(const std::string& label)
{
    m_ext_label = label;
    CalculateSize();
}

void
Pill::SetTooltip(std::string label)
{
    m_tooltip = label;
}

void
Pill::SetAccentColor(size_t accent_color)
{
    m_accent_color = accent_color;
}

void
Pill::Activate()
{
    m_active = true;
}

void
Pill::Deactivate()
{
    m_active = false;
}

ImVec2
Pill::Size()
{
    return ImVec2(m_widths[m_sizing], m_height);
}

ImVec2
Pill::CompactSize()
{
    return ImVec2(m_widths[kCompact], m_height);
}

ImVec2
Pill::ExtSize()
{
    return ImVec2(m_widths[kExtended], m_height);
}

ImVec2
Pill::ElidedSize()
{
    return ImVec2(m_widths[kElided], m_height);
}

bool
Pill::Visible() const
{
    return m_show_pill_label;
}

void
Pill::SetVisible(bool visible)
{
    m_show_pill_label = visible;
}

void
Pill::Render(const ImVec2& pos, SettingsManager& settings, Sizing sizing)
{
    if(m_show_pill_label == false)
    {
        return;
    }
    m_sizing = sizing;
    if(m_sizing == kExtended && m_ext_label.empty())
    {
        m_sizing = kCompact;
    }
    ImGui::PushFont(settings.GetFontManager().GetFont(FontType::kDefault),
                    settings.GetFontManager().GetFontSize(FontSize::kSmall));

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2      win_pos   = ImGui::GetWindowPos();
    if(m_active)
    {
        draw_list->AddRectFilled(win_pos + pos, win_pos + pos + Size(),
                                 settings.GetColor(Colors::kBgFrame), m_height * 0.5f);
        ImGui::PushStyleColor(ImGuiCol_Text, settings.GetColor(Colors::kTextMain));
    }
    else
    {
        draw_list->AddRectFilled(win_pos + pos, win_pos + pos + Size(),
                                 settings.GetColor(Colors::kMetaDataColorSelected),
                                 m_height * 0.5f);
        ImGui::PushStyleColor(ImGuiCol_Text, settings.GetColor(Colors::kTextDim));
    }
    draw_list->AddRect(win_pos + pos, win_pos + pos + Size(),
                       m_accent_color ? settings.GetColorWheel()[m_accent_color.value()]
                                      : settings.GetColor(Colors::kTextDim),
                       m_height * 0.5f, 0, 1.0f);

    ImVec2 text_pos = pos + ImVec2(m_padding_x, m_padding_y);
    ImGui::SetCursorPos(text_pos);
    switch(m_sizing)
    {
        case kElided:
        {
            ImGui::TextUnformatted("...");
            break;
        }
        case kCompact:
        {
            ImGui::TextUnformatted(m_compact_label.c_str());
            break;
        }
        case kExtended:
        {
            ImGui::TextUnformatted(m_ext_label.c_str());
            break;
        }
    }
    if(!m_tooltip.empty() && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
    {
        SetTooltipStyled("%s", m_tooltip.c_str());
    }
    ImGui::PopStyleColor();

    ImGui::PopFont();
}

void
Pill::CalculateSize()
{
    // Measure with the same font Render() draws with; otherwise the width is
    // computed from the larger default font and the gap grows with DPI, letting
    // adjacent pills overlap on high-resolution displays.
    FontManager& fonts = SettingsManager::GetInstance().GetFontManager();
    ImGui::PushFont(fonts.GetFont(FontType::kDefault),
                    fonts.GetFontSize(FontSize::kSmall));

    m_widths[kElided]  = ImGui::CalcTextSize("...").x + 2 * m_padding_x;
    m_widths[kCompact] = ImGui::CalcTextSize(m_compact_label.c_str()).x + 2 * m_padding_x;
    m_widths[kExtended] =
        m_ext_label.empty()
            ? m_widths[kCompact]
            : ImGui::CalcTextSize(m_ext_label.c_str()).x + 2 * m_padding_x;
    m_height = ImGui::GetTextLineHeight() + 2 * m_padding_y;

    ImGui::PopFont();
}

}  // namespace View
}  // namespace RocProfVis
