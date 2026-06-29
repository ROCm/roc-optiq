// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_track_item.h"
#include "icons/rocprovfis_icon_defines.h"
#include "rocprofvis_font_manager.h"
#include "rocprofvis_settings_manager.h"
#include "rocprofvis_timeline_selection.h"
#include "rocprofvis_utils.h"
#include "spdlog/spdlog.h"
#include "widgets/rocprofvis_gui_helpers.h"
#include "widgets/rocprofvis_notification_manager.h"
#include "widgets/rocprofvis_widget.h"
#include <memory>
#include <algorithm>

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
constexpr const char*     TRACK_COPY_MENU_POPUP_NAME     = "TrackCopyMenu";

float TrackItem::s_metadata_width = 400.0f;

TrackItem::TrackItem(DataProvider& dp, uint64_t id,
                     std::shared_ptr<TimePixelTransform> tpt,
                     std::shared_ptr<TimelineSelection>  timeline_selection)
: m_track_metadata(nullptr)
, m_track_statistics(nullptr)
, m_track_statistics_dirty(false)
, m_track_id(id)
, m_track_height(DEFAULT_TRACK_HEIGHT)
, m_track_content_height(0.0f)
, m_min_track_height(DEFAULT_MIN_TRACK_HEIGHT)
, m_track_height_changed(false)
, m_is_in_view_vertical(false)
, m_distance_to_view_y(0.0f)
, m_metadata_padding(ImVec2(8.0f, 5.0f))
, m_resize_grip_thickness(4.0f)
, m_data_provider(dp)
, m_request_state(TrackDataRequestState::kIdle)
, m_settings(SettingsManager::GetInstance())
, m_meta_area_clicked(false)
, m_meta_area_scale_width(0.0f)
, m_max_meta_area_scale_width(0.0f)
, m_selected(false)
, m_reorder_grip_width(DEFAULT_GRIP_WIDTH)
, m_tpt(tpt)
, m_timeline_selection(timeline_selection)
, m_chunk_duration_ns(DEFAULT_CHUNK_DURATION)
, m_group_id_counter(0)
, m_meta_area_label("")
, m_track_project_settings(m_data_provider.GetTraceFilePath(), *this)
{
    if(m_track_project_settings.Valid())
    {
        m_track_height = m_track_project_settings.Height();
    }

    const TrackInfo* track_info =
        m_data_provider.DataModel().GetTimeline().GetTrack(m_track_id);

    if(track_info == nullptr)
    {
        spdlog::error("TrackItem: failed to get TrackInfo for track_id {}", m_track_id);
        return;
    }
    m_track_metadata = track_info;
    m_name = m_data_provider.DataModel().BuildTrackName(m_track_id);
    SetMetaAreaLabel(track_info);
    SetDefaultPillLabel(track_info);
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
    return m_track_id;
}

void
TrackItem::SetSidebarSize(float sidebar_size)
{
    s_metadata_width = sidebar_size;
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
    m_track_id = id;
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

void
TrackItem::RenderMetaArea()
{
    ImVec2 outer_container_size = ImGui::GetContentRegionAvail();
    m_track_content_height      = m_track_height;

    ImVec2 name_label_min(0.0f, 0.0f);
    ImVec2 name_label_max(0.0f, 0.0f);
    bool   name_label_visible = false;

    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(4, 4));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 4));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 3));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(5, 5));
    // Keep the meta-area square so the selection highlight fill reaches the corners
    // instead of bleeding through rounded edges.
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0.0f);

    ImGui::PushStyleColor(ImGuiCol_ChildBg,
                          m_selected
                              ? m_settings.GetColor(Colors::kMetaDataColorSelected)
                              : (m_request_state == TrackDataRequestState::kError
                                     ? m_settings.GetColor(Colors::kGridRed)
                                     : m_settings.GetColor(Colors::kMetaDataColor)));
    ImGui::SetCursorPos(ImVec2(0.0f, 0.0f));
    if(ImGui::BeginChild("MetaData Area",
                         ImVec2(s_metadata_width, outer_container_size.y),
                         ImGuiChildFlags_None,
                         ImGuiWindowFlags_NoScrollbar |
                             ImGuiWindowFlags_NoScrollWithMouse))
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
        float arrow_width       = ImGui::GetTextLineHeight();

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
        float available_for_text =
            content_size.x - m_meta_area_scale_width - m_reorder_grip_width;

        ImVec2 track_name_size = ImGui::CalcTextSize(m_meta_area_label.c_str());

        ImGui::BeginGroup();
        ImGui::PushStyleColor(ImGuiCol_Text, m_settings.GetColor(Colors::kTextMain));
        if(available_for_text > 0.0f)
        {
            const ImVec2 label_start = ImGui::GetCursorScreenPos();
            ImGui::PushID("meta_area_label");
            ElidedText(m_meta_area_label.c_str(), available_for_text);
            ImGui::PopID();

            const float  label_width = std::min(track_name_size.x, available_for_text);
            const ImVec2 hit_padding(NAME_LABEL_HITBOX_PADDING_X,
                                     NAME_LABEL_HITBOX_PADDING_Y);
            name_label_min = label_start - hit_padding;
            name_label_max = ImVec2(label_start.x + label_width + hit_padding.x,
                                    label_start.y + track_name_size.y + hit_padding.y);
            name_label_visible = true;
        }
        ImGui::PopStyleColor();
        ImGui::EndGroup();

        RenderMetaAreaScale();
        RenderMetaAreaExpand();
        RenderPills(ImVec2(available_for_text, content_size.y));
    }
    ImGui::EndChild();  // end metadata area

    ImVec2      meta_min = ImGui::GetItemRectMin();
    ImVec2      meta_max = ImGui::GetItemRectMax();
    ImDrawList* dl       = ImGui::GetWindowDrawList();
    dl->AddLine(ImVec2(meta_max.x - 1.0f, meta_min.y),
                ImVec2(meta_max.x - 1.0f, meta_max.y),
                m_settings.GetColor(Colors::kMetaDataSeparator), 1.0f);

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
        ImGui::SetNextWindowSizeConstraints(
            ImVec2(0, 0), ImVec2(META_TOOLTIP_MAX_WIDTH, FLT_MAX));
        BeginTooltipStyled();
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + wrap_width);
        ImGui::TextUnformatted(m_meta_area_tooltip.c_str());
        ImGui::PopTextWrapPos();
        EndTooltipStyled();
    }

    if(meta_area_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
    {
        ImGui::OpenPopup(TRACK_COPY_MENU_POPUP_NAME);
    }
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, style.WindowPadding);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, style.ItemSpacing);
    if(ImGui::BeginPopup(TRACK_COPY_MENU_POPUP_NAME))
    {
        auto copy_to_clipboard = [](const std::string& text) {
            ImGui::SetClipboardText(text.c_str());
            NotificationManager::GetInstance().Show(
                COPY_DATA_NOTIFICATION.data(), NotificationLevel::Info);
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
        if(IconBeginMenu(ICON_GEAR, "Track Options"))
        {
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));
            RenderMetaAreaOptions();
            ImGui::PopStyleVar();
            ImGui::EndMenu();
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

    ImGui::Selectable(("##MovePositionLine" + std::to_string(m_track_id)).c_str(), false,
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
            "Fetch request deferred for track {}, requests are already pending...", m_track_id);

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
        if(m_track_statistics->state == AnalysisTrackStatistics::kPending)
        {
            RequestAnalysis();
        }
        if(m_track_statistics->state < AnalysisTrackStatistics::kReady)
        {
            m_track_statistics_dirty = true;
        }
        else
        {
            m_track_statistics_dirty = false;
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
            spdlog::error("Request for track {} failed", m_track_id);
        }
        else
        {
            spdlog::debug(
                "Fetching from {} to {} ( {} ) for track {} part of group {}",
                req.m_start_ts, req.m_end_ts, req.m_end_ts - req.m_start_ts, m_track_id,
                req.m_data_group_id);

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

    std::string node_id_str    = std::to_string(track_info->topology.node_id);
    std::string process_id_str = std::to_string(track_info->topology.process_id);

    bool show_node_id    = tdm.NodeCount() > 1;
    bool show_process_id = tdm.ProcessCount() > 1;

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
            if(show_node_id)
            {
                m_meta_area_label += " (NID: " + node_id_str + ")";
            }
            if(show_process_id)
            {
                m_meta_area_label += " (PID: " + process_id_str + ")";
            }
            // set tooltip to counter description
            const CounterInfo* counter_info = tdm.GetCounter(track_info->topology.id.value);
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

            if(show_node_id)
            {
                m_meta_area_label += " (NID: " + node_id_str + ")";
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

            if(show_node_id)
            {
                m_meta_area_label += " (NID: " + node_id_str + ")";
            }
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

    const bool is_sample_track =
        track_info->track_type == kRPVControllerTrackTypeSamples;
    const char* count_label = is_sample_track ? "Samples" : "Events";

    std::string meta_lines;
    meta_lines += "Track ID: " + std::to_string(track_info->id) + "\n";
    if(show_node_id)
    {
        meta_lines += "Node ID: " + node_id_str + "\n";
    }
    if(show_process_id)
    {
        meta_lines += "Process ID: " + process_id_str + "\n";
    }
    meta_lines += std::string(count_label) + ": ";
#ifdef ROCPROFVIS_DEVELOPER_MODE
    meta_lines += std::to_string(track_info->num_entries);
#else
    if(track_info->num_entries >= META_TOOLTIP_COMPACT_COUNT_MIN)
    {
        meta_lines +=
            compact_number_format(static_cast<double>(track_info->num_entries));
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
    bool result = m_data_provider.DataModel().GetTimeline().FreeTrackData(m_track_id, true);
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
    if(m_track_statistics && m_timeline_selection &&
       (m_track_statistics->state == AnalysisTrackStatistics::kStale ||
        m_track_statistics->state == AnalysisTrackStatistics::kPending))
    {
        uint64_t request_id = RequestIdBuilder::MakeTrackDataRequestId(
            static_cast<uint32_t>(m_track_id), 0, 0,
            RequestType::kFetchAnalysisTrackStatistics);
        if(m_data_provider.IsRequestPending(request_id))
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
            m_track_statistics->state =
                (start_ts < end_ts &&
                 m_data_provider.FetchAnalysisTrackStatistics(
                     AnalysisTrackStatisticsRequestParams(m_track_id, start_ts, end_ts)))
                    ? AnalysisTrackStatistics::kRequested
                    : AnalysisTrackStatistics::kPending;
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
Pill::SetLabel(const std::string& label, Sizing sizing)
{
    switch(sizing)
    {
        case kCompact:
        {
            m_compact_label = label;
            break;
        }
        case kExtended:
        {
            m_ext_label = label;
            break;
        }
    }
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
    m_widths[kElided]  = ImGui::CalcTextSize("...").x + 2 * m_padding_x;
    m_widths[kCompact] = ImGui::CalcTextSize(m_compact_label.c_str()).x + 2 * m_padding_x;
    m_widths[kExtended] =
        m_ext_label.empty()
            ? m_widths[kCompact]
            : ImGui::CalcTextSize(m_ext_label.c_str()).x + 2 * m_padding_x;
    m_height = ImGui::GetTextLineHeight() + 2 * m_padding_y;
}

}  // namespace View
}  // namespace RocProfVis
