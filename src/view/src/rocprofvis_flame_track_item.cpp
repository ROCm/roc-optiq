// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_flame_track_item.h"
#include "rocprofvis_appwindow.h"
#include "rocprofvis_core_assert.h"
#include "rocprofvis_event_manager.h"
#include "rocprofvis_settings_manager.h"
#include "rocprofvis_timeline_selection.h"
#include "rocprofvis_utils.h"
#include "spdlog/spdlog.h"
#include <cmath>
#include <limits>
#include <string>
#include <unordered_set>

namespace RocProfVis
{
namespace View
{

constexpr int   MIN_LABEL_WIDTH          = 40;
constexpr float HIGHLIGHT_THICKNESS      = 4.0f;
constexpr float HIGHLIGHT_THICKNESS_HALF = HIGHLIGHT_THICKNESS / 2;
/*
For IMGUI rectangle borders ANTI_ALIASING_WORKAROUND is needed to avoid anti-aliasing
issues (rectangle being too big or too small).
*/
constexpr int ANTI_ALIASING_WORKAROUND = 1;

FlameTrackItem::FlameTrackItem(DataProvider&                      dp,
                               std::shared_ptr<TimelineSelection> timeline_selection,
                               int id, std::string name, double zoom,
                               double time_offset_ns, double min_x, double max_x,
                               double scale_x, float level_min, float level_max)
: TrackItem(dp, id, name, zoom, time_offset_ns, min_x, max_x, scale_x)
, m_event_color_mode(EventColorMode::kByEventName)
, m_text_padding(SettingsManager::GetInstance().GetDefaultIMGUIStyle().FramePadding)
, m_level_height(SettingsManager::GetInstance().GetEventLevelHeight())
, m_timeline_selection(timeline_selection)
, m_min_level(level_min)
, m_max_level(level_max)
, m_selection_changed(false)
, m_has_drawn_tool_tip(false)
, m_project_settings(dp.GetTraceFilePath(), *this)
, m_selected_chart_items({})
{
    auto time_line_selection_changed_handler = [this](std::shared_ptr<RocEvent> e) {
        this->HandleTimelineSelectionChanged(e);
    };
    // Subscribe to timeline selection changed event
    m_timeline_event_selection_changed_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kTimelineEventSelectionChanged),
        time_line_selection_changed_handler);

    if(m_project_settings.Valid())
    {
        m_event_color_mode = m_project_settings.ColorEvents();
    }
}

void
FlameTrackItem::RenderMetaAreaExpand()
{
    ImGui::PushStyleColor(ImGuiCol_Button, m_settings.GetColor(Colors::kTransparent));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                          m_settings.GetColor(Colors::kTransparent));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                          m_settings.GetColor(Colors::kTransparent));
    ImGui::SetCursorPos(
        ImVec2(ImGui::GetContentRegionMax() - m_metadata_padding -
               ImVec2(ImGui::GetTextLineHeight(), ImGui::GetTextLineHeight())));

    int visible_levels = static_cast<int>(std::ceil(m_track_height / m_level_height));

    if(visible_levels <= m_max_level + 1)
    {
        if(ImGui::ArrowButton("##expand", ImGuiDir_Down))
        {
            m_track_height         = m_max_level * m_level_height + m_level_height + 2.0f;
            m_track_height_changed = true;
        }
        if(ImGui::IsItemHovered()) ImGui::SetTooltip("Expand track to see all events");
    }
    else if(m_track_height >
            std::max(m_max_level * m_level_height + m_level_height,
                     m_track_default_height))  // stand-in for default height..
    {
        if(ImGui::ArrowButton("##contract", ImGuiDir_Up))
        {
            m_track_height =
                m_track_default_height;  // Default track height defined in parent class.
            m_track_height_changed = true;
        }
        if(ImGui::IsItemHovered()) ImGui::SetTooltip("Contract track to default height");
    }
    ImGui::PopStyleColor(3);
}
FlameTrackItem::~FlameTrackItem()
{
    EventManager::GetInstance()->Unsubscribe(
        static_cast<int>(RocEvents::kTimelineEventSelectionChanged),
        m_timeline_event_selection_changed_token);
}

bool
FlameTrackItem::ReleaseData()
{
    if(TrackItem::ReleaseData())
    {
        m_chart_items.clear();
        return true;
    }
    return false;
}

bool
FlameTrackItem::ExtractPointsFromData()
{
    const RawTrackData* rtd = m_data_provider.GetRawTrackData(m_id);

    // If no raw track data is found, this means the track was unloaded before the
    // response was processed
    if(!rtd)
    {
        spdlog::error("No raw track data found for track {}", m_id);
        // Reset the request state to idle
        m_request_state = TrackDataRequestState::kIdle;
        return false;
    }

    const RawTrackEventData* event_track = dynamic_cast<const RawTrackEventData*>(rtd);

    if(!event_track)
    {
        spdlog::debug("Invalid track data type for track {}", m_id);
        m_request_state = TrackDataRequestState::kError;
        return false;
    }

    if(event_track->AllDataReady())
    {
        m_request_state = TrackDataRequestState::kIdle;
    }

    if(event_track->GetData().empty())
    {
        spdlog::debug("No data for track {}", m_id);
        return false;
    }

    // Update selection state cache.
    const std::vector<rocprofvis_trace_event_t>& events_data = event_track->GetData();
    m_chart_items.resize(events_data.size());
    for(int i = 0; i < events_data.size(); i++)
    {
        const rocprofvis_trace_event_t& event = events_data[i];
        m_chart_items[i].event                = event;
        m_chart_items[i].selected  = m_timeline_selection->EventSelected(event.m_id);
        m_chart_items[i].name_hash = std::hash<std::string>{}(event.m_name);
    }

    return true;
}

void
FlameTrackItem::HandleTimelineSelectionChanged(std::shared_ptr<RocEvent> e)
{
    std::shared_ptr<EventSelectionChangedEvent> selection_changed_event =
        std::static_pointer_cast<EventSelectionChangedEvent>(e);
    if(selection_changed_event &&
       selection_changed_event->GetSourceId() == m_data_provider.GetTraceFilePath())
    {
        // Update selection state cache.
        for(ChartItem& item : m_chart_items)
        {
            item.selected = m_timeline_selection->EventSelected(item.event.m_id);
        }
    }
}

void
FlameTrackItem::DrawBox(ImVec2 start_position, int color_index, ChartItem& chart_item,
                        double duration, ImDrawList* draw_list)
{
    ImVec2 cursor_position = ImGui::GetCursorScreenPos();
    ImVec2 content_size    = ImGui::GetContentRegionAvail();

    ImVec2 rectMin = ImVec2(start_position.x, start_position.y + cursor_position.y);
    ImVec2 rectMax = ImVec2(start_position.x + duration,
                            start_position.y + m_level_height + cursor_position.y);

    ImU32 rectColor;
    if(m_event_color_mode == EventColorMode::kNone)
    {
        rectColor = m_settings.GetColor(Colors::kFlameChartColor);
    }
    else
    {
        rectColor = m_settings.GetColorWheel()[color_index];
    }

    float rounding = 2.0f;
    draw_list->AddRectFilled(rectMin, rectMax, rectColor, rounding);

    if(rectMax.x - rectMin.x > MIN_LABEL_WIDTH)
    {
        draw_list->PushClipRect(rectMin, rectMax, true);
        ImVec2 textPos =
            ImVec2(rectMin.x + m_text_padding.x, rectMin.y + m_text_padding.y);

        draw_list->AddText(textPos, m_settings.GetColor(Colors::kTextMain),
                           chart_item.event.m_name.c_str());
        draw_list->PopClipRect();
    }
    if(ImGui::IsMouseHoveringRect(rectMin, rectMax) &&
       !ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopup))
    {
        // Select on click
        if(!m_selection_changed && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            chart_item.selected = !chart_item.selected;
            chart_item.selected
                ? m_timeline_selection->SelectTrackEvent(m_id, chart_item.event.m_id)
                : m_timeline_selection->UnselectTrackEvent(m_id, chart_item.event.m_id);
            m_selection_changed = true;
            if(chart_item.selected == false)
            {
                for(int select = 0; select < m_selected_chart_items.size(); select++)
                {
                    if(m_selected_chart_items[select].event.m_id == chart_item.event.m_id)
                    {
                        m_selected_chart_items.erase(m_selected_chart_items.begin() +
                                                     select);

                        break;
                    }
                }
            }
            else
            {
                m_selected_chart_items.push_back(chart_item);
            }
        }

        if(!m_has_drawn_tool_tip)
        {
            const auto& time_format =
                m_settings.GetUserSettings().unit_settings.time_format;
            rocprofvis_trace_event_t_id_t event_id{};
            event_id.id = chart_item.event.m_id;
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, m_text_padding);
            ImGui::BeginTooltip();
            ImGui::Text("%s", chart_item.event.m_name.c_str());
            std::string label = nanosecond_to_formatted_str(
                chart_item.event.m_start_ts - m_min_x, time_format);
            ImGui::Text("Start: %s", label.c_str());
            label = nanosecond_to_formatted_str(chart_item.event.m_duration, time_format);
            ImGui::Text("Duration: %s", label.c_str());
            ImGui::Text("Id: %llu", chart_item.event.m_id);
            ImGui::Text("DB Id: %llu", event_id.bitfield.db_event_id);
            ImGui::EndTooltip();
            ImGui::PopStyleVar();
            m_has_drawn_tool_tip = true;
        }
    }
}

void
FlameTrackItem::RenderChart(float graph_width)
{
    ImGui::BeginChild("FV", ImVec2(graph_width, m_track_content_height), false);
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    auto colorCount = m_settings.GetColorWheel().size();
    ROCPROFVIS_ASSERT(colorCount > 0);

    int color_index      = 0;
    m_has_drawn_tool_tip = false;
    for(ChartItem& item : m_chart_items)
    {
        ImVec2 container_pos = ImGui::GetWindowPos();

        double normalized_start =
            container_pos.x +
            (item.event.m_start_ts - (m_min_x + m_time_offset_ns)) * m_scale_x;

        double normalized_duration = std::max(item.event.m_duration * m_scale_x, 1.0);
        double normalized_end      = normalized_start + normalized_duration;

        ImVec2 start_position;

        // Calculate the start position based on the normalized start time and level
        start_position = ImVec2(normalized_start, item.event.m_level * m_level_height);

        if(normalized_end < container_pos.x ||
           normalized_start > container_pos.x + graph_width)
        {
            continue;  // Skip if the item is not visible in the current view
        }

        if(m_event_color_mode == EventColorMode::kByTimeLevel)
        {
            color_index =
                static_cast<long long>(item.event.m_start_ts + item.event.m_level) %
                colorCount;
        }
        else if(m_event_color_mode == EventColorMode::kByEventName)
        {
            color_index = static_cast<long long>(item.name_hash) % colorCount;
        }

        DrawBox(start_position, color_index, item, normalized_duration, draw_list);
    }

    // This is here to check for universal event clear.
    if(!m_timeline_selection->HasSelectedEvents())
    {
        m_selected_chart_items.clear();
    }
                   
    for(ChartItem& item : m_selected_chart_items)
    {
        ImVec2 container_pos = ImGui::GetWindowPos();
        double normalized_start =
            container_pos.x +
            (item.event.m_start_ts - (m_min_x + m_time_offset_ns)) * m_scale_x;

        double normalized_duration = std::max(item.event.m_duration * m_scale_x, 1.0);
        double normalized_end      = normalized_start + normalized_duration;

        ImVec2 start_position;
        float  rounding = 2.0f;
        // Calculate the start position based on the normalized start time and level
        start_position = ImVec2(normalized_start, item.event.m_level * m_level_height);

        ImVec2 cursor_position = ImGui::GetCursorScreenPos();
        ImVec2 content_size    = ImGui::GetContentRegionAvail();

        ImVec2 rectMin =
            ImVec2(start_position.x - HIGHLIGHT_THICKNESS_HALF,
                                start_position.y + cursor_position.y +
                                    HIGHLIGHT_THICKNESS_HALF - ANTI_ALIASING_WORKAROUND);
        ImVec2 rectMax =
            ImVec2(start_position.x + normalized_duration + HIGHLIGHT_THICKNESS_HALF,
                   start_position.y + m_level_height + cursor_position.y -
                       HIGHLIGHT_THICKNESS_HALF + ANTI_ALIASING_WORKAROUND);
        // Outer border (sticks out by 2)
        draw_list->AddRect(rectMin, rectMax, m_settings.GetColor(Colors::kEventHighlight),
                           rounding, 0, HIGHLIGHT_THICKNESS);
    }

    m_selection_changed = false;

    ImGui::EndChild();
}

void
FlameTrackItem::RenderMetaAreaScale()
{}

void
FlameTrackItem::RenderMetaAreaOptions()
{
    EventColorMode mode = m_event_color_mode;

    if(ImGui::RadioButton("Color by Name", mode == EventColorMode::kByEventName))
        mode = EventColorMode::kByEventName;
    ImGui::SameLine();
    if(ImGui::RadioButton("Color by Time Level", mode == EventColorMode::kByTimeLevel))
        mode = EventColorMode::kByTimeLevel;
    ImGui::SameLine();
    if(ImGui::RadioButton("No Color", mode == EventColorMode::kNone))
        mode = EventColorMode::kNone;

    m_event_color_mode = mode;
}

FlameTrackProjectSettings::FlameTrackProjectSettings(const std::string& project_id,
                                                     FlameTrackItem&    track_item)
: ProjectSetting(project_id)
, m_track_item(track_item)
{}

FlameTrackProjectSettings::~FlameTrackProjectSettings() {}

void
FlameTrackProjectSettings::ToJson()
{
    m_settings_json[JSON_KEY_GROUP_TIMELINE][JSON_KEY_TIMELINE_TRACK]
                   [m_track_item.GetID()][JSON_KEY_TIMELINE_TRACK_COLOR] =
                       static_cast<int>(m_track_item.m_event_color_mode);
}

bool
FlameTrackProjectSettings::Valid() const
{
    return m_settings_json[JSON_KEY_GROUP_TIMELINE][JSON_KEY_TIMELINE_TRACK]
                          [m_track_item.GetID()][JSON_KEY_TIMELINE_TRACK_COLOR]
                              .isNumber();
}

EventColorMode
FlameTrackProjectSettings::ColorEvents() const
{
    EventColorMode color_mode = EventColorMode::kNone;

    double color_mode_raw =
        m_settings_json[JSON_KEY_GROUP_TIMELINE][JSON_KEY_TIMELINE_TRACK]
                       [m_track_item.GetID()][JSON_KEY_TIMELINE_TRACK_COLOR]
                           .getNumber();
    if(color_mode_raw >= 0 && color_mode_raw < static_cast<int>(EventColorMode::__kCount))
    {
        color_mode = static_cast<EventColorMode>(color_mode_raw);
    }
    return color_mode;
}

}  // namespace View
}  // namespace RocProfVis