// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_flame_track_item.h"
#include "rocprofvis_appwindow.h"
#include "rocprofvis_click_manager.h"
#include "rocprofvis_core_assert.h"
#include "rocprofvis_event_manager.h"
#include "rocprofvis_hotkey_manager.h"
#include "rocprofvis_measurement_controller.h"
#include "rocprofvis_settings_manager.h"
#include "rocprofvis_timeline_selection.h"
#include "rocprofvis_timeline_track_options.h"
#include "rocprofvis_utils.h"
#include "spdlog/spdlog.h"
#include "widgets/rocprofvis_gui_helpers.h"
#ifdef IMGUI_ENABLE_TEST_ENGINE
#include "imgui_internal.h"
#endif
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <sstream>
#include <string>

namespace RocProfVis
{
namespace View
{

inline constexpr float MIN_LABEL_WIDTH          = 40.0f;
inline constexpr float HIGHLIGHT_THICKNESS      = 4.0f;
inline constexpr float HIGHLIGHT_THICKNESS_HALF = HIGHLIGHT_THICKNESS / 2;
inline constexpr float TOOLTIP_OFFSET           = 16.0f;
inline constexpr int   MAX_CHARACTERS_PER_LINE  = 40;
inline constexpr float MAX_TABLE_HEIGHT         = 300.0f;
inline constexpr float FLAME_MIN_TRACK_HEIGHT   = 28.0f;
inline constexpr float MIN_EVENT_BOX_HEIGHT     = 1.0f;
inline constexpr float DEFAULT_VISIBLE_LEVELS   = 2.0f;
inline constexpr float TRACK_HEIGHT_EPSILON     = 1.0f;

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

FlameTrackItem::FlameTrackItem(DataProvider& dp, uint64_t track_id,
                               TimelineTrackOptions&                  track_options,
                               std::shared_ptr<TimePixelTransform>    tpt,
                               std::shared_ptr<TimelineSelection>     timeline_selection,
                               std::shared_ptr<MeasurementController> measurement)
: TrackItem(dp, track_id, track_options, tpt, timeline_selection)
, m_text_padding(SettingsManager::GetInstance().GetDefaultIMGUIStyle().FramePadding)
, m_level_height(SettingsManager::GetInstance().GetEventLevelHeight())
, m_measurement(measurement)
, m_min_level(0.0f)
, m_max_level(0.0f)
, m_deferred_click_handled(false)
, m_has_drawn_tool_tip(false)
, m_selected_chart_items({})
, m_tooltip_size(0.0f, 0.0f)
, m_pill_analysis_queue(nullptr)
, m_event_options(nullptr)
, m_queue_options(nullptr)
{
    if(!m_tpt)
    {
        spdlog::error("FlameTrackItem: m_tpt shared_ptr is null, cannot construct");
        return;
    }

    if(m_track_metadata)
    {
        m_min_level = static_cast<float>(m_track_metadata->min_value);
        m_max_level = static_cast<float>(m_track_metadata->max_value);
        m_track_statistics =
            m_data_provider.DataModel().GetAnalysis().RegisterTrack(*m_track_metadata);
        if(m_track_statistics &&
           m_track_metadata->topology.type == TrackInfo::TrackType::Queue)
        {
            m_pill_analysis_queue = AddPill();
            m_pill_analysis_queue->SetAccentColor(
                m_track_statistics
                    ->stats[AnalysisTrackStatistics::Queue::kQueueUtilization]
                    .accent_color);
        }
    }

    auto time_line_selection_changed_handler = [this](std::shared_ptr<RocEvent> e) {
        this->HandleTimelineSelectionChanged(e);
    };

    // Subscribe to timeline selection changed event
    m_timeline_event_selection_changed_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kTimelineEventSelectionChanged),
        time_line_selection_changed_handler);

    auto timeline_highlight_changed_handler = [this](std::shared_ptr<RocEvent> e) {
        this->HandleTimelineHighlightChanged(e);
    };

    m_timeline_event_highlight_changed_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kTimelineEventHighlightChanged),
        timeline_highlight_changed_handler);

    auto font_size_changed_handler = [this](std::shared_ptr<RocEvent> e) {
        this->HandleFontSizeChanged(e);
    };

    m_font_size_changed_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kFontSizeChanged), font_size_changed_handler);

    if(m_options)
    {
        m_event_options = dynamic_cast<EventTrackOptions*>(m_options.get());
        m_queue_options = dynamic_cast<QueueTrackOptions*>(m_options.get());
        if(m_queue_options && m_pill_analysis_queue)
        {
            m_pill_analysis_queue->SetVisible(m_queue_options->m_show_queue_utilization);
        }
    }
    ROCPROFVIS_ASSERT(m_event_options);

    RefreshLevelHeight();

    if(m_options && !HasSavedTrackHeight())
    {
        m_options->m_height = DefaultTrackHeight();
    }
}

float
FlameTrackItem::GetMetaAreaTrailingWidth() const
{
    return ImGui::GetFrameHeight() + ImGui::GetStyle().ItemSpacing.x;
}

void
FlameTrackItem::RenderMetaAreaExpand()
{
    if(m_event_options)
    {
        ImGui::PushStyleColor(ImGuiCol_Button, m_settings.GetColor(Colors::kTransparent));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                              m_settings.GetColor(Colors::kTransparent));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                              m_settings.GetColor(Colors::kTransparent));
        // ArrowButton uses the full frame height, not the text line height.
        const float control_size = ImGui::GetFrameHeight();
        ImVec2      button_pos   = ImGui::GetContentRegionMax() - m_metadata_padding -
                            ImVec2(control_size + m_meta_area_scale_width, control_size);
        const float default_height = DefaultTrackHeight();
        const float target_height  = std::max(ExpandedTrackHeight(), default_height);

        if(m_options->m_height + TRACK_HEIGHT_EPSILON < target_height)
        {
            ImGui::SetCursorPos(button_pos);
            if(ImGui::ArrowButton("##expand", ImGuiDir_Down))
            {
                RecalculateTrackHeight();
                m_event_options->m_expand = true;
            }
            if(ImGui::IsItemHovered()) SetTooltipStyled("Expand track to see all events");
        }
        else if(m_options->m_height > default_height + TRACK_HEIGHT_EPSILON)
        {
            ImGui::SetCursorPos(button_pos);
            if(ImGui::ArrowButton("##contract", ImGuiDir_Up))
            {
                m_options->m_height       = default_height;
                m_track_height_changed    = true;
                m_event_options->m_expand = false;
            }
            if(ImGui::IsItemHovered()) SetTooltipStyled("Contract track to default height");
        }
        ImGui::PopStyleColor(3);
    }
}

FlameTrackItem::~FlameTrackItem()
{
    EventManager::GetInstance()->Unsubscribe(
        static_cast<int>(RocEvents::kTimelineEventSelectionChanged),
        m_timeline_event_selection_changed_token);
    EventManager::GetInstance()->Unsubscribe(
        static_cast<int>(RocEvents::kTimelineEventHighlightChanged),
        m_timeline_event_highlight_changed_token);
    EventManager::GetInstance()->Unsubscribe(
        static_cast<int>(RocEvents::kFontSizeChanged), m_font_size_changed_token);
}

void
FlameTrackItem::Update()
{
    if(m_track_statistics && m_pill_analysis_queue)
    {
        // Blue only while a time-range selection narrows the data.
        const bool ranged =
            m_timeline_selection && m_timeline_selection->HasValidTimeRangeSelection();
        m_pill_analysis_queue->SetTextColor(
            ranged ? std::optional<Colors>(Colors::kAccent) : std::nullopt);
        if(m_track_statistics->state == AnalysisTrackStatistics::kReady &&
           m_track_statistics_dirty)
        {
            const AnalysisTrackStatistics::Stat& stat =
                m_track_statistics
                    ->stats[AnalysisTrackStatistics::Queue::kQueueUtilization];
            m_pill_analysis_queue->Activate();
            m_pill_analysis_queue->SetLabel(stat.CompactValue());
            m_pill_analysis_queue->SetExtendedLabel(stat.CompactLabel());
            m_pill_analysis_queue->SetTooltip(stat.FullLabel());
        }
        else if(m_track_statistics->state < AnalysisTrackStatistics::kReady)
        {
            m_pill_analysis_queue->Deactivate();
        }
    }
    if(m_options && m_options->Updated())
    {
        if(m_event_options)
        {
            // Classify the current height before rescaling the rows, so a compact-mode
            // toggle preserves whether the track was at its default or expanded size
            // rather than relying on the transient expand flag alone.
            const float previous_default_height  = DefaultTrackHeight();
            const float previous_expanded_height = ExpandedTrackHeight();
            const float previous_level_height    = m_level_height;
            const bool  was_default =
                std::abs(m_options->m_height - previous_default_height) <=
                TRACK_HEIGHT_EPSILON;
            const bool was_expanded =
                m_event_options->m_expand ||
                m_options->m_height + TRACK_HEIGHT_EPSILON >= previous_expanded_height;

            RefreshLevelHeight();

            if(m_level_height != previous_level_height)
            {
                if(was_expanded)
                {
                    RecalculateTrackHeight();
                    m_event_options->m_expand = true;
                }
                else if(was_default)
                {
                    m_options->m_height    = DefaultTrackHeight();
                    m_track_height_changed = true;
                }
            }
        }
        if(m_queue_options && m_track_statistics && m_pill_analysis_queue)
        {
            m_pill_analysis_queue->SetVisible(m_queue_options->m_show_queue_utilization);
        }
    }

    // Grow when metadata needs more room without shrinking user-resized tracks.
    UpdateMinTrackHeight();
    if(m_options && m_options->m_height < m_min_track_height)
    {
        m_options->m_height    = m_min_track_height;
        m_track_height_changed = true;
    }

    TrackItem::Update();
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
FlameTrackItem::IsCompactMode() const
{
    return m_event_options ? m_event_options->m_compact : TrackItem::IsCompactMode();
}

void
FlameTrackItem::ExtractPointsFromData()
{
    const RawTrackData* rtd =
        m_data_provider.DataModel().GetTimeline().GetTrackData(m_track_id);

    // If no raw track data is found, this means the track was unloaded before the
    // response was processed
    if(!rtd)
    {
        spdlog::debug("No raw track data found for track {}", m_track_id);
        return;
    }

    const RawTrackEventData* event_track = dynamic_cast<const RawTrackEventData*>(rtd);

    if(!event_track)
    {
        spdlog::debug("Invalid track data type for track {}", m_track_id);
        m_request_state = TrackDataRequestState::kError;
        return;
    }

    if(event_track->GetData().empty())
    {
        spdlog::debug("No data for track {}", m_track_id);
        return;
    }

    // Update selection state cache.
    const std::vector<TraceEvent>& events_data = event_track->GetData();
    m_chart_items.resize(events_data.size());
    for(int i = 0; i < events_data.size(); i++)
    {
        const TraceEvent& event   = events_data[i];
        m_chart_items[i].event    = event;
        m_chart_items[i].selected = m_timeline_selection->EventSelected(event.m_id.uuid);
        m_chart_items[i].highlighted =
            m_timeline_selection->EventHighlighted(event.m_id.uuid);
        if(m_chart_items[i].event.m_child_count > 1)
        {
            m_chart_items[i].name_hash =
                std::hash<std::string>{}(event.m_top_combined_name);
        }
        else
        {
            m_chart_items[i].name_hash = std::hash<std::string>{}(event.m_name);
        }
        m_chart_items[i].child_info.clear();
    }
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
                         item.event.m_id.uuid);
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
                size_t count          = std::stoul(combined_name.substr(0, pos1));
                size_t duration_start = pos1 + s_child_info_separator.size();
                size_t duration       = std::stoull(
                    combined_name.substr(duration_start, pos2 - duration_start));
                std::string name =
                    combined_name.substr(pos2 + s_child_info_separator.size());
                out_info = { name, std::hash<std::string>{}(name), count, duration };
                return true;
            } catch(const std::exception&)
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
        for(ChartItem& item : m_chart_items)
        {
            item.selected = m_timeline_selection->EventSelected(item.event.m_id.uuid);
        }
    }
}

void
FlameTrackItem::HandleTimelineHighlightChanged(std::shared_ptr<RocEvent> e)
{
    std::shared_ptr<EventHighlightChangedEvent> highlight_changed_event =
        std::static_pointer_cast<EventHighlightChangedEvent>(e);
    if(highlight_changed_event &&
       highlight_changed_event->GetSourceId() == m_data_provider.GetTraceFilePath())
    {
        for(ChartItem& item : m_chart_items)
        {
            item.highlighted =
                m_timeline_selection->EventHighlighted(item.event.m_id.uuid);
        }
    }
}

void
FlameTrackItem::HandleFontSizeChanged(std::shared_ptr<RocEvent> e)
{
    (void) e;
    if(!m_options)
    {
        return;
    }

    const float previous_default_height  = DefaultTrackHeight();
    const float previous_expanded_height = ExpandedTrackHeight();
    const float previous_level_height    = m_level_height;
    const bool  was_default =
        std::abs(m_options->m_height - previous_default_height) <= TRACK_HEIGHT_EPSILON;
    const bool was_expanded =
        (m_event_options && m_event_options->m_expand) ||
        m_options->m_height + TRACK_HEIGHT_EPSILON >= previous_expanded_height;
    RefreshLevelHeight();

    if(was_default)
    {
        m_options->m_height    = DefaultTrackHeight();
        m_track_height_changed = true;
    }
    else if(was_expanded)
    {
        RecalculateTrackHeight();
        if(m_event_options)
        {
            m_event_options->m_expand = true;
        }
    }
    else if(previous_level_height > 0.0f && m_level_height != previous_level_height)
    {
        const float scale   = m_level_height / previous_level_height;
        m_options->m_height = std::max(m_min_track_height, m_options->m_height * scale);
        m_track_height_changed = true;
    }
}

float
FlameTrackItem::TextGlyphCenter()
{
    // The vertical ink center of the font depends only on the current font size,
    // which is shared by every flame track. Cache it and rescan only when the
    // font size changes, so the glyph lookups run at most once
    // per size change across all tracks rather than per track or per frame.
    static float s_cached_font_size    = -1.0f;
    static float s_cached_glyph_center = 0.0f;

    const float font_size = ImGui::GetFontSize();
    if(font_size == s_cached_font_size)
    {
        return s_cached_glyph_center;
    }

    ImFontBaked* baked = ImGui::GetFontBaked();
    float        min_y = std::numeric_limits<float>::max();
    float        max_y = std::numeric_limits<float>::lowest();

    if(baked != nullptr)
    {
        const float scale = baked->Size > 0.0f ? font_size / baked->Size : 1.0f;

        // Sentinel glyphs spanning the tallest caps and lowest descenders, which
        // bound the ink extents of any typical label.
        static const char* const kSentinel = "AQgjpqy()|";
        for(const char* c = kSentinel; *c != '\0'; ++c)
        {
            ImFontGlyph* glyph =
                baked->FindGlyph(static_cast<unsigned char>(*c));
            if(glyph != nullptr && glyph->Visible)
            {
                min_y = std::min(min_y, glyph->Y0 * scale);
                max_y = std::max(max_y, glyph->Y1 * scale);
            }
        }
    }

    if(min_y == std::numeric_limits<float>::max())
    {
        min_y = 0.0f;
        max_y = ImGui::GetTextLineHeight();
    }

    s_cached_font_size    = font_size;
    s_cached_glyph_center = (min_y + max_y) * 0.5f;
    return s_cached_glyph_center;
}

float
FlameTrackItem::ComputeTextVerticalOffset(float box_height) const
{
    // The label baseline only depends on the shared font metrics and this
    // track's (constant) box height, not on the label's own characters, so every
    // event on a level aligns to the same baseline. Returns the offset from the
    // box top at which text should be drawn.
    return std::max(0.0f, box_height * 0.5f - TextGlyphCenter());
}

void
FlameTrackItem::DrawBox(ImVec2 start_position, ChartItem& chart_item, float duration,
                        ImDrawList* draw_list, bool use_highlight_color)
{
    ImVec2 cursor_position = ImGui::GetCursorScreenPos();
    ImVec2 content_size    = ImGui::GetContentRegionAvail();

    const float box_height = EventBoxHeight();

    ImVec2 rectMin = ImVec2(start_position.x, start_position.y + cursor_position.y);
    ImVec2 rectMax = ImVec2(start_position.x + duration,
                            start_position.y + box_height + cursor_position.y);

#ifdef IMGUI_ENABLE_TEST_ENGINE
    // Bars are raw draw_list rects with no ImGui ID, so the Test Engine can't
    // find them by ref. Register each bar's bounding box with the engine under a
    // stable per-event ID; tests then locate bars via GatherItems/ItemInfo. This
    // compiles out of production and adds no widget.
    {
        ImGuiContext& g           = *GImGui;
        ImGuiWindow*  test_window = ImGui::GetCurrentWindow();
        ImGuiID       bar_id      = test_window->GetID(
            reinterpret_cast<const void*>(
                static_cast<uintptr_t>(chart_item.event.m_id.uuid)));
        IMGUI_TEST_ENGINE_ITEM_ADD(bar_id, ImRect(rectMin, rectMax), nullptr);
    }
#endif

    const std::vector<ImU32>& color_wheel =
        use_highlight_color ? m_settings.GetHighlightedEventColorWheel()
                            : m_settings.GetColorWheel();
    size_t color_index = 0;
    ImU32 rectColor = use_highlight_color ? color_wheel[0]
                                          : m_settings.GetColor(Colors::kFlameChartColor);
    if(m_event_options)
    {
        if(m_event_options->m_color_mode ==
           EventTrackOptions::EventColorMode::kByEventName)
        {
            color_index = static_cast<size_t>(chart_item.name_hash) % color_wheel.size();
            rectColor   = color_wheel[color_index];
        }
        else if(m_event_options->m_color_mode ==
                EventTrackOptions::EventColorMode::kByTimeLevel)
        {
            color_index = static_cast<size_t>(chart_item.event.m_start_ts +
                                              chart_item.event.m_level) %
                          color_wheel.size();
            rectColor   = color_wheel[color_index];
        }
    }
    else
    {
        color_index = static_cast<size_t>(chart_item.name_hash) % color_wheel.size();
        rectColor   = color_wheel[color_index];
    }

    float rounding = 2.0f;
    draw_list->AddRectFilled(rectMin, rectMax, rectColor, rounding);

    if(rectMax.x - rectMin.x > MIN_LABEL_WIDTH &&
       box_height >= ImGui::GetTextLineHeight())
    {
        draw_list->PushClipRect(rectMin, rectMax, true);
        const std::string label =
            (chart_item.event.m_child_count > 1)
                ? std::to_string(chart_item.event.m_child_count) + " events"
                : chart_item.event.m_name;
        const float text_y  = rectMin.y + m_text_vertical_offset;
        ImVec2      textPos = ImVec2(rectMin.x + m_text_padding.x, text_y);

        if(chart_item.event.m_child_count > 1)
        {
            draw_list->AddText(textPos, m_settings.GetColor(Colors::kTextMain),
                               label.c_str());
        }
        else
        {
            if(rectMin.x < draw_list->GetClipRectMin().x &&
               rectMax.x > draw_list->GetClipRectMin().x)
            {
                // If the rectangle is partially outside the viewport then start rendering
                // the text at the viewport edge to maintain readability.
                textPos =
                    ImVec2(draw_list->GetClipRectMin().x + m_text_padding.x, text_y);
                draw_list->AddText(textPos, m_settings.GetColor(Colors::kTextMain),
                                   label.c_str());
            }
            else
            {
                // The rectangle is fully inside the viewport, render text normally.
                draw_list->AddText(textPos, m_settings.GetColor(Colors::kTextMain),
                                   label.c_str());
            }
        }
        draw_list->PopClipRect();
    }
    if(ImGui::IsMouseHoveringRect(rectMin, rectMax) &&
       ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows |
                              ImGuiHoveredFlags_NoPopupHierarchy))
    {
        // Select on click
        if(IsMouseReleasedWithDragCheck(ImGuiMouseButton_Left) &&
           TimelineFocusManager::GetInstance().GetFocusedLayer() !=
               Layer::kInteractiveLayer)
        {
            // Defer on click execution to next frame if no other layer takes focus
            TimelineFocusManager::GetInstance().RequestLayerFocus(Layer::kGraphLayer);
        }
        // Execute deferred click if layer has focus
        else if(!m_deferred_click_handled &&
                TimelineFocusManager::GetInstance().GetFocusedLayer() ==
                    Layer::kGraphLayer)
        {
            m_deferred_click_handled       = true;
            MeasurementController& measure = *m_measurement;

            if(measure.IsMeasurementMode() && !measure.IsFreehandMode())
            {
                // Clicking after a complete measurement starts a new one.
                if(measure.GetMeasurementState() == MeasurementState::kComplete)
                {
                    m_timeline_selection->UnhighlightPersistentEvents();
                    measure.ClearMeasurement();
                }
                measure.SetMeasurementPoint(
                    chart_item.event.m_start_ts, chart_item.event.m_duration, m_track_id,
                    chart_item.event.m_level, chart_item.event.m_name,
                    chart_item.event.m_id.uuid);
                m_timeline_selection->HighlightTrackEventPersistent(
                    m_track_id, chart_item.event.m_id.uuid);
            }
            else if(!measure.IsMeasurementMode())
            {
                chart_item.selected = !chart_item.selected;

                if(!HotkeyManager::GetInstance().IsActionHeld(
                       HotkeyActionId::kMultiSelect))
                {
                    m_timeline_selection->UnselectAllEvents();
                }

                chart_item.selected ? m_timeline_selection->SelectTrackEvent(
                                          m_track_id, chart_item.event.m_id.uuid)
                                    : m_timeline_selection->UnselectTrackEvent(
                                          m_track_id, chart_item.event.m_id.uuid);
            }
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

    if(chart_item.selected || chart_item.highlighted)
    {
        m_selected_chart_items.push_back(chart_item);
    }
}

void
FlameTrackItem::RenderTooltip(ChartItem& chart_item, size_t color_index)
{
    const auto& time_format = m_settings.GetUserSettings().unit_settings.time_format;
    size_t      color_count = m_settings.GetColorWheel().size();

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
        ImGui::PushFont(NULL, m_settings.GetFontManager().GetFontSize(FontSize::kSmall));
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
                if(m_event_options && m_event_options->m_color_mode !=
                                          EventTrackOptions::EventColorMode::kNone)
                {
                    auto c_idx = static_cast<size_t>(chart_item.child_info[i].name_hash) %
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
            chart_item.event.m_start_ts - m_tpt->GetMinX(), time_format, true);
        ImGui::Text("Start: %s", label.c_str());
        label =
            nanosecond_to_formatted_str(chart_item.event.m_duration, time_format, true);
        ImGui::Text("Combined Range: %s", label.c_str());
    }
    else
    {
        TraceEventId event_id{};
        event_id = chart_item.event.m_id;
        ImGui::TextUnformatted("Name: ");
        ImGui::SameLine();
        if(m_event_options &&
           m_event_options->m_color_mode != EventTrackOptions::EventColorMode::kNone)
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
            chart_item.event.m_start_ts - m_tpt->GetMinX(), time_format, true);
        ImGui::Text("Start: %s", label.c_str());
        label =
            nanosecond_to_formatted_str(chart_item.event.m_duration, time_format, true);
        ImGui::Text("Duration: %s", label.c_str());
#ifdef ROCPROFVIS_DEVELOPER_MODE
        ImGui::Text("UUID: %llu", chart_item.event.m_id.uuid);
#endif
        ImGui::Text("ID: %llu", event_id.bitfield.event_id);
    }

    m_tooltip_size = ImGui::GetWindowSize();  // save size for positioning
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
    ImGui::End();
}

void
FlameTrackItem::RecalculateTrackHeight()
{
    if(m_options)
    {
        m_options->m_height    = std::max(ExpandedTrackHeight(), DefaultTrackHeight());
        m_track_height_changed = true;
    }
}

void
FlameTrackItem::UpdateMinTrackHeight()
{
    m_min_track_height =
        std::max({ FLAME_MIN_TRACK_HEIGHT, m_level_height, GetMetaAreaMinHeight() });
}

void
FlameTrackItem::RefreshLevelHeight()
{
    const bool compact = m_event_options && m_event_options->m_compact;
    m_level_height     = compact ? m_settings.GetEventLevelCompactHeight()
                                 : m_settings.GetEventLevelHeight();
    UpdateMinTrackHeight();
}

float
FlameTrackItem::DefaultTrackHeight() const
{
    return std::max(DEFAULT_VISIBLE_LEVELS * m_level_height, m_min_track_height);
}

float
FlameTrackItem::ExpandedTrackHeight() const
{
    return (m_max_level + 1.0f) * m_level_height + 0.5f * m_resize_grip_thickness;
}

float
FlameTrackItem::EventBoxHeight() const
{
    return std::max(MIN_EVENT_BOX_HEIGHT,
                    m_level_height - m_settings.GetEventLevelSpacing());
}

void
FlameTrackItem::RenderChart(float graph_width)
{
    ImGui::BeginChild("FV", ImVec2(graph_width, m_track_content_height), false,
                      ImGuiWindowFlags_NoMouseInputs);
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

#ifdef IMGUI_ENABLE_TEST_ENGINE
    m_test_flame_window_id = ImGui::GetCurrentWindow()->ID;
#endif

    m_has_drawn_tool_tip = false;

    // The label baseline is the same for every box on the track, so compute the
    // vertical offset once per frame instead of per rendered event.
    m_text_vertical_offset = ComputeTextVerticalOffset(EventBoxHeight());

    double     range_start_ns = TimelineSelection::INVALID_SELECTION_TIME;
    double     range_end_ns   = TimelineSelection::INVALID_SELECTION_TIME;
    const bool has_time_range_selection =
        m_timeline_selection->GetSelectedTimeRange(range_start_ns, range_end_ns);

    for(ChartItem& item : m_chart_items)
    {
        ImVec2 container_pos = ImGui::GetWindowPos();

        double normalized_start =
            container_pos.x + m_tpt->RawTimeToPixel(item.event.m_start_ts);

        double normalized_duration =
            std::max(item.event.m_duration * m_tpt->GetPixelsPerNs(), 1.0);
        double normalized_end = normalized_start + normalized_duration;

        ImVec2 start_position;

        // Calculate the start position based on the normalized start time and level
        start_position = ImVec2(static_cast<float>(normalized_start),
                                item.event.m_level * m_level_height);

        if(normalized_end < container_pos.x ||
           normalized_start > container_pos.x + graph_width)
        {
            continue;  // Skip if the item is not visible in the current view
        }

        if(normalized_duration > std::numeric_limits<float>::max())
        {
            normalized_duration = std::numeric_limits<float>::max();
        }

        const bool use_highlight_color =
            has_time_range_selection && item.event.m_start_ts <= range_end_ns &&
            item.event.m_start_ts + item.event.m_duration >= range_start_ns;

        DrawBox(start_position, item, static_cast<float>(normalized_duration), draw_list,
                use_highlight_color);
    }

    for(ChartItem& item : m_selected_chart_items)
    {
        ImVec2 container_pos = ImGui::GetWindowPos();
        double normalized_start =
            container_pos.x + m_tpt->RawTimeToPixel(item.event.m_start_ts);

        double normalized_duration =
            std::max(item.event.m_duration * m_tpt->GetPixelsPerNs(), 1.0);

        float  rounding       = 2.0f;
        ImVec2 start_position = ImVec2(static_cast<float>(normalized_start),
                                       item.event.m_level * m_level_height);

        ImVec2 cursor_position = ImGui::GetCursorScreenPos();

        const float box_height = EventBoxHeight();

        ImVec2 rectMin = ImVec2(start_position.x - HIGHLIGHT_THICKNESS_HALF,
                                start_position.y + cursor_position.y +
                                    HIGHLIGHT_THICKNESS_HALF - ANTI_ALIASING_WORKAROUND);
        ImVec2 rectMax =
            ImVec2(start_position.x + static_cast<float>(normalized_duration) +
                       HIGHLIGHT_THICKNESS_HALF,
                   start_position.y + box_height + cursor_position.y -
                       HIGHLIGHT_THICKNESS_HALF + ANTI_ALIASING_WORKAROUND);

        ImU32 border_color = item.highlighted
                                 ? m_settings.GetColor(Colors::kEventSearchHighlight)
                                 : m_settings.GetColor(Colors::kEventHighlight);

        float thickness = HIGHLIGHT_THICKNESS;
        if(item.highlighted)
        {
            double elapsed =
                m_timeline_selection->GetHighlightElapsedSeconds(item.event.m_id.uuid);
            bool persistent =
                m_timeline_selection->IsHighlightPersistent(item.event.m_id.uuid);
            if(!persistent)
            {
                float pulse = 0.5f + 0.5f * std::sin(static_cast<float>(elapsed) * 6.0f);
                thickness   = HIGHLIGHT_THICKNESS + pulse * 1.5f;

                ImU32 a      = (border_color >> 24) & 0xFF;
                ImU32 new_a  = static_cast<ImU32>(a * (0.5f + 0.5f * pulse));
                border_color = (border_color & 0x00FFFFFF) | (new_a << 24);
            }
        }
        bool is_last_highlight = item.highlighted;

        float  half_t   = thickness / 2.0f;
        ImVec2 pulseMin = ImVec2(start_position.x - half_t,
                                 rectMin.y - half_t + HIGHLIGHT_THICKNESS_HALF);
        ImVec2 pulseMax =
            ImVec2(start_position.x + static_cast<float>(normalized_duration) + half_t,
                   rectMax.y + half_t - HIGHLIGHT_THICKNESS_HALF);

        draw_list->AddRect(is_last_highlight ? pulseMin : rectMin,
                           is_last_highlight ? pulseMax : rectMax, border_color, rounding,
                           0, thickness);
    }

    m_selected_chart_items.clear();
    m_deferred_click_handled = false;

    ImGui::EndChild();
}

}  // namespace View
}  // namespace RocProfVis
