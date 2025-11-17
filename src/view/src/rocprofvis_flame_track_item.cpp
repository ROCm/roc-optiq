// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_flame_track_item.h"
#include "rocprofvis_appwindow.h"
#include "rocprofvis_click_manager.h"
#include "rocprofvis_core_assert.h"
#include "rocprofvis_event_manager.h"
#include "rocprofvis_settings_manager.h"
#include "rocprofvis_timeline_selection.h"
#include "rocprofvis_utils.h"
#include "spdlog/spdlog.h"
#include <cmath>
#include <limits>
#include <sstream>
#include <string>
#include <unordered_set>

namespace RocProfVis
{
namespace View
{

constexpr float MIN_LABEL_WIDTH          = 40.0f;
constexpr float HIGHLIGHT_THICKNESS      = 4.0f;
constexpr float HIGHLIGHT_THICKNESS_HALF = HIGHLIGHT_THICKNESS / 2;
constexpr float TOOLTIP_OFFSET           = 16.0f;
constexpr int   MAX_CHARACTERS_PER_LINE  = 40;
constexpr float MAX_TABLE_HEIGHT         = 300.0f;

/*
For IMGUI rectangle borders ANTI_ALIASING_WORKAROUND is needed to avoid anti-aliasing
issues (rectangle being too big or too small).
*/
constexpr float ANTI_ALIASING_WORKAROUND = 1.0f;

const std::string FlameTrackItem::s_child_info_separator  = "|";
float             FlameTrackItem::s_max_event_label_width = 0.0f;

void
FlameTrackItem::CalculateMaxEventLabelWidth()
{
    // Assume max MAX_CHARACTERS_PER_LINE characters for estimation at current font size.
    s_max_event_label_width = ImGui::CalcTextSize("W").x * MAX_CHARACTERS_PER_LINE;
}

FlameTrackItem::FlameTrackItem(DataProvider&                      dp,
                               std::shared_ptr<TimelineSelection> timeline_selection,
                               uint64_t id, std::string name, float zoom,
                               double time_offset_ns, double min_x, double max_x,
                               double scale_x, float level_min, float level_max)
: TrackItem(dp, id, name, zoom, time_offset_ns, min_x, max_x, scale_x)
, m_event_color_mode(EventColorMode::kByEventName)
, m_text_padding(SettingsManager::GetInstance().GetDefaultIMGUIStyle().FramePadding)
, m_level_height(SettingsManager::GetInstance().GetEventLevelHeight())
, m_timeline_selection(timeline_selection)
, m_min_level(level_min)
, m_max_level(level_max)
, m_deferred_click_handled(false)
, m_has_drawn_tool_tip(false)
, m_project_settings(dp.GetTraceFilePath(), *this)
, m_selected_chart_items({})
, m_tooltip_size(0.0f, 0.0f)
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
        if(m_chart_items[i].event.m_child_count > 1)
        {
            m_chart_items[i].name_hash = std::hash<std::string>{}(event.m_top_combined_name);
        }
        else
        {
            m_chart_items[i].name_hash = std::hash<std::string>{}(event.m_name);
        }
        m_chart_items[i].child_info.clear();
    }
    return true;
}

bool
FlameTrackItem::ExtractChildInfo(ChartItem& item)
{
    // Parse name string to extract child event info if this is a combined event
    if(item.event.m_child_count > 1)
    {
        std::stringstream ss(item.event.m_name);
        std::string       line;
        item.child_info.clear();
        item.child_info.reserve(item.event.m_child_count);
        while(std::getline(ss, line))
        {
            ChildEventInfo child_info;
            if(ParseChildInfo(line, child_info))
            {
                item.child_info.push_back(child_info);
            }
        }
        // If parsing failed to extract any child info, fall back to using the full name
        if(item.child_info.empty())
        {
            item.child_info.clear();
            item.child_info.push_back({ item.event.m_name,
                                        std::hash<std::string>{}(item.event.m_name),
                                        item.event.m_child_count,
                                        static_cast<uint64_t>(item.event.m_duration) });
            spdlog::warn("Failed to parse child info for event ID {}. "
                         "Falling back to full event name.",
                         item.event.m_id);
        }
    }
    else
    {
        item.child_info.clear();
        return false;
    }
    return true;
}

bool
FlameTrackItem::ParseChildInfo(const std::string& combined_name, ChildEventInfo& out_info)
{
    size_t pos1 = combined_name.find(s_child_info_separator);
    if(pos1 != std::string::npos)
    {
        size_t pos2 = combined_name.find(s_child_info_separator, pos1 + 1);
        if(pos2 != std::string::npos)
        {
            try
            {
                // Extract count, duration and name (format: "<count>|<duration>|<name>")
                size_t count = std::stoul(combined_name.substr(0, pos1));
                size_t duration_start = pos1 + s_child_info_separator.size();
                size_t duration = std::stoull(combined_name.substr(duration_start, pos2 - duration_start));
                std::string name = combined_name.substr(pos2 + s_child_info_separator.size());
                out_info = { name, std::hash<std::string>{}(name), count, duration };
                return true;
            }
            catch(const std::exception&)
            {
                spdlog::warn("Failed to parse child event info from string: {}",
                             combined_name);
            }
        }
    }
    out_info = { "", 0, 0, 0 };  // Default if parsing fails
    return false;
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
                        float duration, ImDrawList* draw_list)
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

        if(chart_item.event.m_child_count > 1)
        {
            std::string label =
                std::to_string(chart_item.event.m_child_count) + " events";
            draw_list->AddText(textPos, m_settings.GetColor(Colors::kTextMain),
                               label.c_str());
        }
        else
        {
            draw_list->AddText(textPos, m_settings.GetColor(Colors::kTextMain),
                               chart_item.event.m_name.c_str());
        }
        draw_list->PopClipRect();
    }
    if(ImGui::IsMouseHoveringRect(rectMin, rectMax) &&
       !ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopup))
    {
        // Select on click
        if(ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            // Defer on click execution to next frame if no other layer takes focus
            TimelineFocusManager::GetInstance().RequestLayerFocus(Layer::kGraphLayer);
        }
        // Execute deferred click if layer has focus
        else if(!m_deferred_click_handled &&
                TimelineFocusManager::GetInstance().GetFocusedLayer() ==
                    Layer::kGraphLayer)
        {
            m_deferred_click_handled =
                true;  // Ensure only one click is handled per render cycle
            chart_item.selected = !chart_item.selected;
            chart_item.selected
                ? m_timeline_selection->SelectTrackEvent(m_id, chart_item.event.m_id)
                : m_timeline_selection->UnselectTrackEvent(m_id, chart_item.event.m_id);
            // Always reset layer clicked after handling
            TimelineFocusManager::GetInstance().RequestLayerFocus(Layer::kNone);
        }

        // only show one tooltip per render cycle and if no other layer has focus
        if(!m_has_drawn_tool_tip &&
           TimelineFocusManager::GetInstance().GetFocusedLayer() == Layer::kNone)
        {
            RenderTooltip(chart_item, color_index);
            m_has_drawn_tool_tip = true;
        }
    }

    if(chart_item.selected)
    {
        m_selected_chart_items.push_back(chart_item);
    }
}

void
FlameTrackItem::RenderTooltip(ChartItem& chart_item, int color_index)
{
    const auto& time_format = m_settings.GetUserSettings().unit_settings.time_format;
    int         color_count = static_cast<int>(m_settings.GetColorWheel().size());

    ImVec2 mouse_pos      = ImGui::GetMousePos();
    ImVec2 viewport_size  = ImGui::GetMainViewport()->Size;
    ImVec2 estimated_size = m_tooltip_size;

    // Calculate possible tooltip positions and choose the one with more visible content
    float pos_x_right = mouse_pos.x + TOOLTIP_OFFSET;
    float visible_width_right =
        fmax(0.0f, fmin(viewport_size.x, pos_x_right + estimated_size.x) - pos_x_right);
    float pos_x_left = mouse_pos.x - TOOLTIP_OFFSET - estimated_size.x;
    float visible_width_left =
        fmax(0.0f, fmin(viewport_size.x, pos_x_left + estimated_size.x) - pos_x_left);
    float pos_x = (visible_width_left > visible_width_right) ? pos_x_left : pos_x_right;

    float pos_y_bottom = mouse_pos.y + TOOLTIP_OFFSET;
    float visible_height_bottom =
        fmax(0.0f, fmin(viewport_size.y, pos_y_bottom + estimated_size.y) - pos_y_bottom);
    float pos_y_top = mouse_pos.y - TOOLTIP_OFFSET - estimated_size.y;
    float visible_height_top =
        fmax(0.0f, fmin(viewport_size.y, pos_y_top + estimated_size.y) - pos_y_top);
    float pos_y = (visible_height_top > visible_height_bottom) ? pos_y_top : pos_y_bottom;

    ImVec2 pos(pos_x, pos_y);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, m_text_padding);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,
                        m_settings.GetDefaultStyle().FrameRounding);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, m_settings.GetColor(Colors::kBgFrame));
    ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
    ImGui::Begin("FlameTooltip", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoFocusOnAppearing |
                     ImGuiWindowFlags_NoNav | ImGuiWindowFlags_AlwaysAutoResize |
                     ImGuiWindowFlags_NoSavedSettings);

    if(chart_item.event.m_child_count > 1)
    {
        if(chart_item.child_info.empty())
        {
            // Extract child info on demand
            ExtractChildInfo(chart_item);
        }

        ImGui::Text("%u events", chart_item.event.m_child_count);
        ImGui::PushFont(m_settings.GetFontManager().GetFont(FontType::kSmall));
        ImGui::PushStyleVar(ImGuiStyleVar_CellPadding,
                            ImVec2(ImGui::GetStyle().CellPadding.x, 0.0f));
        if(ImGui::BeginTable("ChildEventsTable", 3,
                             ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
        {
            // Calculate max name width for auto-fit up to s_max_event_label_width
            float max_name_width = 0.0f;
            for(int i = 0; i < chart_item.child_info.size(); ++i)
            {
                float text_width =
                    ImGui::CalcTextSize(chart_item.child_info[i].name.c_str()).x;
                if(text_width > max_name_width) max_name_width = text_width;
            }
            float name_col_width = (max_name_width < s_max_event_label_width)
                                       ? max_name_width
                                       : s_max_event_label_width;

            // Table headers with auto-fit width for Name column
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed,
                                    name_col_width);
            ImGui::TableSetupColumn("Count");
            ImGui::TableSetupColumn("Duration");
            ImGui::TableHeadersRow();

            // Table rows
            const size_t size           = chart_item.child_info.size();
            float        current_height = 0.0f;
            int          num_shown      = 0;
            for(int i = 0; i < size; ++i)
            {
                // Calculate actual row height based on wrapped text
                ImVec2 name_size =
                    ImGui::CalcTextSize(chart_item.child_info[i].name.c_str(), nullptr,
                                        false, name_col_width);
                std::string count_str  = std::to_string(chart_item.child_info[i].count);
                ImVec2      count_size = ImGui::CalcTextSize(count_str.c_str());
                float       row_height = fmax(name_size.y, count_size.y) +
                                   ImGui::GetStyle().CellPadding.y * 2.0f;
                if(current_height + row_height > MAX_TABLE_HEIGHT) break;
                ImGui::TableNextRow();

                // Name column
                ImGui::TableNextColumn();
                if(m_event_color_mode != EventColorMode::kNone)
                {
                    auto c_idx =
                        static_cast<uint64_t>(chart_item.child_info[i].name_hash) %
                        color_count;
                    ImU32 cellBgColor = m_settings.GetColorWheel()[c_idx];
                    ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, cellBgColor);
                }
                ImGui::TextWrapped("%s", chart_item.child_info[i].name.c_str());

                // Count column
                ImGui::TableNextColumn();
                const ChildEventInfo& child = chart_item.child_info[i];
                ImGui::Text("%zu", child.count);

                // Duration column
                ImGui::TableNextColumn();
                std::string duration_str = nanosecond_to_formatted_str(
                    static_cast<double>(child.duration), time_format, true);
                ImGui::Text("%s", duration_str.c_str());

                current_height += row_height;
                num_shown++;
            }
            if(num_shown < size)
            {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("(%zu rows hidden)", size - num_shown);
                ImGui::TableNextColumn();
                // Empty for count
                ImGui::TableNextColumn();
                // Empty for duration
            }
            ImGui::EndTable();
        }
        ImGui::PopStyleVar();  // CellPadding
        ImGui::PopFont();

        std::string label = nanosecond_to_formatted_str(
            chart_item.event.m_start_ts - m_min_x, time_format, true);
        ImGui::Text("Start: %s", label.c_str());
        label =
            nanosecond_to_formatted_str(chart_item.event.m_duration, time_format, true);
        ImGui::Text("Combined Range: %s", label.c_str());
    }
    else
    {
        rocprofvis_trace_event_t_id_t event_id{};
        event_id.id = chart_item.event.m_id;
        ImGui::TextUnformatted("Name: ");
        ImGui::SameLine();
        if(m_event_color_mode != EventColorMode::kNone)
        {
            ImVec2 text_size = ImGui::CalcTextSize(
                chart_item.event.m_name.c_str(), nullptr, false, s_max_event_label_width);
            ImVec2      p         = ImGui::GetCursorScreenPos();
            ImDrawList* draw_list = ImGui::GetWindowDrawList();
            ImU32       rectColor = m_settings.GetColorWheel()[color_index];
            draw_list->AddRectFilled(p, ImVec2(p.x + text_size.x, p.y + text_size.y),
                                     rectColor);
        }
        ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + s_max_event_label_width);
        ImGui::TextWrapped("%s", chart_item.event.m_name.c_str());
        ImGui::PopTextWrapPos();
        ImGui::Separator();
        std::string label = nanosecond_to_formatted_str(
            chart_item.event.m_start_ts - m_min_x, time_format, true);
        ImGui::Text("Start: %s", label.c_str());
        label =
            nanosecond_to_formatted_str(chart_item.event.m_duration, time_format, true);
        ImGui::Text("Duration: %s", label.c_str());
        ImGui::Text("Id: %llu", chart_item.event.m_id);
        ImGui::Text("DB Id: %llu", event_id.bitfield.db_event_id);
    }

    m_tooltip_size = ImGui::GetWindowSize();  // save size for positioning
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
    ImGui::End();
}

void
FlameTrackItem::RenderChart(float graph_width)
{
    ImGui::BeginChild("FV", ImVec2(graph_width, m_track_content_height), false);
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    int colorCount = static_cast<int>(m_settings.GetColorWheel().size());
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
        start_position = ImVec2(static_cast<float>(normalized_start),
                                item.event.m_level * m_level_height);

        if(normalized_end < container_pos.x ||
           normalized_start > container_pos.x + graph_width)
        {
            continue;  // Skip if the item is not visible in the current view
        }

        if(m_event_color_mode == EventColorMode::kByTimeLevel)
        {
            color_index =
                static_cast<uint64_t>(item.event.m_start_ts + item.event.m_level) %
                colorCount;
        }
        else if(m_event_color_mode == EventColorMode::kByEventName)
        {
            color_index = static_cast<uint64_t>(item.name_hash) % colorCount;
        }

        if(normalized_duration > std::numeric_limits<float>::max())
        {
            normalized_duration = std::numeric_limits<float>::max();
        }

        DrawBox(start_position, color_index, item,
                static_cast<float>(normalized_duration), draw_list);
    }

    for(ChartItem& item : m_selected_chart_items)
    {
        ImVec2 container_pos = ImGui::GetWindowPos();
        double normalized_start =
            container_pos.x +
            (item.event.m_start_ts - (m_min_x + m_time_offset_ns)) * m_scale_x;

        double normalized_duration = std::max(item.event.m_duration * m_scale_x, 1.0);

        ImVec2 start_position;
        float  rounding = 2.0f;
        // Calculate the start position based on the normalized start time and level
        start_position = ImVec2(static_cast<float>(normalized_start),
                                item.event.m_level * m_level_height);

        ImVec2 cursor_position = ImGui::GetCursorScreenPos();
        ImVec2 content_size    = ImGui::GetContentRegionAvail();

        ImVec2 rectMin = ImVec2(start_position.x - HIGHLIGHT_THICKNESS_HALF,
                                start_position.y + cursor_position.y +
                                    HIGHLIGHT_THICKNESS_HALF - ANTI_ALIASING_WORKAROUND);
        ImVec2 rectMax =
            ImVec2(start_position.x + static_cast<float>(normalized_duration) +
                       HIGHLIGHT_THICKNESS_HALF,
                   start_position.y + m_level_height + cursor_position.y -
                       HIGHLIGHT_THICKNESS_HALF + ANTI_ALIASING_WORKAROUND);
        // Outer border (sticks out by 2)
        draw_list->AddRect(rectMin, rectMax, m_settings.GetColor(Colors::kEventHighlight),
                           rounding, 0, HIGHLIGHT_THICKNESS);
    }

    m_selected_chart_items.clear();
    m_deferred_click_handled = false;

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