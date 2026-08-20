// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_timeline_track_options.h"
#include "rocprofvis_data_provider.h"
#include "rocprofvis_event_manager.h"
#include "rocprofvis_events.h"
#include "rocprofvis_flame_track_item.h"
#include "rocprofvis_line_track_item.h"
#include "rocprofvis_settings_manager.h"
#include "rocprofvis_timeline_selection.h"
#include "rocprofvis_track_item.h"
#include "widgets/rocprofvis_gui_helpers.h"
#include <algorithm>
#include <cstring>

namespace RocProfVis
{
namespace View
{

constexpr std::array<const char*, TrackInfo::Count>
    DISPLAY_STRINGS_TOPOLOGY_TRACK_TYPES = { "##Unknown",         "Queue Tracks",
                                             "Stream Tracks",     "Thread Tracks",
                                             "Thread (S) Tracks", "Counter Tracks" };

// Fraction of the viewport work area that a "Show Hidden Tracks" category
// submenu may occupy before it starts scrolling.
constexpr float HIDDEN_TRACKS_MENU_MAX_HEIGHT_FRACTION = 0.5f;
constexpr std::array<const char*, 2> DISPLAY_STRINGS_TRACK_DATA_TYPES = {
    "Counter Tracks", "Event Tracks"
};

TrackOptions::TrackOptions(const TrackItem& track, TimelineTrackOptions& ctx,
                           const std::string& project_id)
: m_updated(false)
, m_display(true)
, m_height(DEFAULT_TRACK_HEIGHT)
, m_track_item(track)
, m_ctx(ctx)
, m_settings(SettingsManager::GetInstance())
, m_project_settings(project_id.empty()
                         ? nullptr
                         : std::make_unique<TrackProjectSetting>(project_id, *this))
{
    m_type_mask.set(TrackOptions::kTrack);
}

TrackOptions::TrackOptions(const TrackOptions& other)
: m_updated(other.m_updated)
, m_display(other.m_display)
, m_height(other.m_height)
, m_track_item(other.m_track_item)
, m_ctx(other.m_ctx)
, m_settings(other.m_settings)
, m_project_settings(other.m_project_settings ? std::make_unique<TrackProjectSetting>(
                                                    *other.m_project_settings)
                                              : nullptr)
{
    m_type_mask.set(TrackOptions::kTrack);
}

TrackOptions&
TrackOptions::operator&=(const TrackOptions& other)
{
    m_display &= other.m_display;
    m_height = other.m_height;
    return *this;
}

void
TrackOptions::ToJson()
{
    if(m_project_settings)
    {
        jt::Json& track =
            m_project_settings->GetJson()[JSON_KEY_GROUP_TIMELINE]
                                         [JSON_KEY_TIMELINE_TRACK][m_track_item.GetID()];
        track[JSON_KEY_TIMELINE_TRACK_DISPLAY] = m_display;
        track[JSON_KEY_TIMELINE_TRACK_HEIGHT]  = m_height;
    }
}

bool
TrackOptions::Valid() const
{
    if(m_project_settings)
    {
        jt::Json& track =
            m_project_settings->GetJson()[JSON_KEY_GROUP_TIMELINE]
                                         [JSON_KEY_TIMELINE_TRACK][m_track_item.GetID()];
        return track[JSON_KEY_TIMELINE_TRACK_DISPLAY].isBool() &&
               track[JSON_KEY_TIMELINE_TRACK_HEIGHT].isNumber();
    }
    return false;
}

void
TrackOptions::FromJson()
{
    if(m_project_settings)
    {
        jt::Json& track =
            m_project_settings->GetJson()[JSON_KEY_GROUP_TIMELINE]
                                         [JSON_KEY_TIMELINE_TRACK][m_track_item.GetID()];
        m_display = track[JSON_KEY_TIMELINE_TRACK_DISPLAY].getBool();
        m_height  = static_cast<float>(track[JSON_KEY_TIMELINE_TRACK_HEIGHT].getNumber());
    }
}

void
TrackOptions::Render()
{
    if(m_track_item.GetTrackInfo())
    {
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));
        bool display = m_display;
        if(ImGui::Checkbox("Track Visibility", &display))
        {
            if(m_ctx.m_propagate == TimelineTrackOptions::kNone)
            {
                m_display = display;
                m_updated = true;
            }
            else
            {
                for(TrackOptions* sibling :
                    m_ctx.Siblings(m_track_item.GetTrackInfo()->topology.type,
                                   m_track_item.GetTrackInfo()->track_type))
                {
                    if(sibling && sibling->m_display != display)
                    {
                        sibling->m_display = display;
                        sibling->m_updated = true;
                    }
                }
            }
            m_ctx.m_update_aggregates = true;
            EventManager::GetInstance()->AddEvent(std::make_shared<RocEvent>(
                static_cast<int>(RocEvents::kTrackVisibilityChanged),
                m_track_item.m_data_provider.GetTraceFilePath()));
        }
        ImGui::PopStyleVar();
    }
}

bool
TrackOptions::Updated()
{
    if(m_updated)
    {
        m_updated = false;
        return true;
    }
    return false;
}

const std::bitset<TrackOptions::kNumTypes>&
TrackOptions::TypeMask() const
{
    return m_type_mask;
}

TrackOptions::TrackProjectSetting::TrackProjectSetting(const std::string& project_id,
                                                       TrackOptions&      options)
: ProjectSetting(project_id)
, m_options(options)
{}

void
TrackOptions::TrackProjectSetting::ToJson()
{
    m_options.ToJson();
}

bool
TrackOptions::TrackProjectSetting::Valid() const
{
    return m_options.Valid();
}

jt::Json&
TrackOptions::TrackProjectSetting::GetJson()
{
    return m_settings_json;
}

CounterTrackOptions::CounterTrackOptions(const TrackItem&      track,
                                         TimelineTrackOptions& ctx,
                                         const std::string&    project_id)
: TrackOptions(track, ctx, project_id)
, m_show_analysis{ false, false, true, true }
, m_boxplot{ true, false }
, m_highlight{ false, 0.0f, 0.0f }
{
    m_type_mask.set(TrackOptions::kCounter);
}

CounterTrackOptions::CounterTrackOptions(const CounterTrackOptions& other)
: TrackOptions(other)
, m_show_analysis(other.m_show_analysis)
, m_boxplot(other.m_boxplot)
, m_highlight(other.m_highlight)
{
    m_type_mask.set(TrackOptions::kCounter);
}

CounterTrackOptions&
CounterTrackOptions::operator&=(const TrackOptions& other)
{
    TrackOptions::operator&=(other);
    const CounterTrackOptions& counter = static_cast<const CounterTrackOptions&>(other);
    for(size_t i = 0; i < m_show_analysis.size(); i++)
    {
        m_show_analysis[i] &= counter.m_show_analysis[i];
    }
    m_boxplot.enabled &= counter.m_boxplot.enabled;
    m_boxplot.stripes &= counter.m_boxplot.stripes;
    m_highlight.enabled &= counter.m_highlight.enabled;
    m_highlight.range_min = counter.m_highlight.range_min;
    m_highlight.range_max = counter.m_highlight.range_max;
    return *this;
}

void
CounterTrackOptions::ToJson()
{
    if(m_project_settings)
    {
        TrackOptions::ToJson();
        jt::Json& track =
            m_project_settings->GetJson()[JSON_KEY_GROUP_TIMELINE]
                                         [JSON_KEY_TIMELINE_TRACK][m_track_item.GetID()];
        track[JSON_KEY_TIMELINE_TRACK_BOX_PLOT]        = m_boxplot.enabled;
        track[JSON_KEY_TIMELINE_TRACK_COLOR]           = m_highlight.enabled;
        track[JSON_KEY_TIMELINE_TRACK_COLOR_RANGE_MIN] = m_highlight.range_min;
        track[JSON_KEY_TIMELINE_TRACK_COLOR_RANGE_MAX] = m_highlight.range_max;
        track[JSON_KEY_TIMELINE_TRACK_STRIPES]         = m_boxplot.stripes;
        track[JSON_KEY_TIMELINE_TRACK_MIN] =
            m_show_analysis[AnalysisTrackStatistics::Counter::kCounterMin];
        track[JSON_KEY_TIMELINE_TRACK_MAX] =
            m_show_analysis[AnalysisTrackStatistics::Counter::kCounterMax];
        track[JSON_KEY_TIMELINE_TRACK_MEAN] =
            m_show_analysis[AnalysisTrackStatistics::Counter::kCounterMean];
        track[JSON_KEY_TIMELINE_TRACK_STANDARD_DEVIATION] =
            m_show_analysis[AnalysisTrackStatistics::Counter::kCounterStandardDeviation];
    }
}

bool
CounterTrackOptions::Valid() const
{
    if(m_project_settings)
    {
        jt::Json& track =
            m_project_settings->GetJson()[JSON_KEY_GROUP_TIMELINE]
                                         [JSON_KEY_TIMELINE_TRACK][m_track_item.GetID()];
        return track[JSON_KEY_TIMELINE_TRACK_BOX_PLOT].isBool() &&
               track[JSON_KEY_TIMELINE_TRACK_STRIPES].isBool() &&
               track[JSON_KEY_TIMELINE_TRACK_COLOR].isBool() &&
               track[JSON_KEY_TIMELINE_TRACK_COLOR_RANGE_MIN].isNumber() &&
               track[JSON_KEY_TIMELINE_TRACK_COLOR_RANGE_MAX].isNumber() &&
               track[JSON_KEY_TIMELINE_TRACK_MIN].isBool() &&
               track[JSON_KEY_TIMELINE_TRACK_MAX].isBool() &&
               track[JSON_KEY_TIMELINE_TRACK_MEAN].isBool() &&
               track[JSON_KEY_TIMELINE_TRACK_STANDARD_DEVIATION].isBool() &&
               TrackOptions::Valid();
    }
    return false;
}

void
CounterTrackOptions::FromJson()
{
    if(m_project_settings)
    {
        TrackOptions::FromJson();
        jt::Json& track =
            m_project_settings->GetJson()[JSON_KEY_GROUP_TIMELINE]
                                         [JSON_KEY_TIMELINE_TRACK][m_track_item.GetID()];
        m_boxplot.enabled     = track[JSON_KEY_TIMELINE_TRACK_BOX_PLOT].getBool();
        m_boxplot.stripes     = track[JSON_KEY_TIMELINE_TRACK_STRIPES].getBool();
        m_highlight.enabled   = track[JSON_KEY_TIMELINE_TRACK_COLOR].getBool();
        m_highlight.range_min = static_cast<float>(
            track[JSON_KEY_TIMELINE_TRACK_COLOR_RANGE_MIN].getNumber());
        m_highlight.range_max = static_cast<float>(
            track[JSON_KEY_TIMELINE_TRACK_COLOR_RANGE_MAX].getNumber());
        m_show_analysis[AnalysisTrackStatistics::Counter::kCounterMin] =
            track[JSON_KEY_TIMELINE_TRACK_MIN].getBool();
        m_show_analysis[AnalysisTrackStatistics::Counter::kCounterMax] =
            track[JSON_KEY_TIMELINE_TRACK_MAX].getBool();
        m_show_analysis[AnalysisTrackStatistics::Counter::kCounterMean] =
            track[JSON_KEY_TIMELINE_TRACK_MEAN].getBool();
        m_show_analysis[AnalysisTrackStatistics::Counter::kCounterStandardDeviation] =
            track[JSON_KEY_TIMELINE_TRACK_STANDARD_DEVIATION].getBool();
    }
}

void
CounterTrackOptions::Render()
{
    TrackOptions::Render();
    if(m_track_item.GetTrackInfo())
    {
        const LineTrackItem& line = static_cast<const LineTrackItem&>(m_track_item);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));
        ImGui::SeparatorText("Sample Appearance");
        bool boxplot = m_boxplot.enabled;
        if(ImGui::Checkbox("Show Counter Boxes", &boxplot))
        {
            if(m_ctx.m_propagate == TimelineTrackOptions::kNone)
            {
                m_boxplot.enabled = boxplot;
                m_updated         = true;
            }
            else
            {
                for(TrackOptions* sibling :
                    m_ctx.Siblings(m_track_item.GetTrackInfo()->topology.type,
                                   m_track_item.GetTrackInfo()->track_type))
                {
                    CounterTrackOptions* counter =
                        static_cast<CounterTrackOptions*>(sibling);
                    if(counter->m_boxplot.enabled != boxplot)
                    {
                        counter->m_boxplot.enabled = boxplot;
                        counter->m_updated         = true;
                    }
                }
            }
            m_ctx.m_update_aggregates = true;
        }
        bool stripes = m_boxplot.stripes;
        if(ImGui::Checkbox("Alternate Counter Coloring", &stripes))
        {
            if(m_ctx.m_propagate == TimelineTrackOptions::kNone)
            {
                m_boxplot.stripes = stripes;
                m_updated         = true;
            }
            else
            {
                for(TrackOptions* sibling :
                    m_ctx.Siblings(m_track_item.GetTrackInfo()->topology.type,
                                   m_track_item.GetTrackInfo()->track_type))
                {
                    CounterTrackOptions* counter =
                        static_cast<CounterTrackOptions*>(sibling);
                    if(counter->m_boxplot.stripes != stripes)
                    {
                        counter->m_boxplot.stripes = stripes;
                        counter->m_updated         = true;
                    }
                }
            }
            m_ctx.m_update_aggregates = true;
        }
        Highlight& highlight           = m_highlight;
        bool       highlight_toggle    = highlight.enabled;
        float      highlight_range_min = highlight.range_min;
        float      highlight_range_max = highlight.range_max;
        float      highlight_min       = static_cast<float>(line.m_min_y.Value());
        float      highlight_max       = static_cast<float>(line.m_max_y.Value());
        bool       highlight_changed   = false;
        bool       highlight_available =
            m_ctx.m_propagate == TimelineTrackOptions::kNone ||
            (m_ctx.m_propagate == TimelineTrackOptions::kSelected &&
             m_ctx.m_siblings_by_selection.size() == 1) ||
            (m_ctx.m_propagate == TimelineTrackOptions::kSiblings &&
             m_ctx.m_siblings_by_topology_type[TrackInfo::Counter].size() == 1);
        ImGui::BeginDisabled(!highlight_available);
        if(ImGui::Checkbox("Highlight Y Range", &highlight_toggle))
        {
            highlight_range_min = highlight_min;
            highlight_range_max = highlight_max;
            highlight_changed   = true;
        }
        ImGui::PopStyleVar();
        if(highlight_toggle && highlight_available)
        {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                                ImVec2(m_settings.GetDefaultStyle().FramePadding.x,
                                       m_settings.GetDefaultStyle().FramePadding.x));
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding,
                                m_settings.GetDefaultStyle().ChildRounding);
            ImGui::PushStyleColor(ImGuiCol_ChildBg,
                                  m_settings.GetColor(Colors::kTransparent));
            ImGui::BeginChild("highlight_limits", ImVec2(0, 0),
                              ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);

            float availalble_width = ImGui::GetContentRegionAvail().x -
                                     m_settings.GetDefaultStyle().ItemSpacing.x;
            ImGui::AlignTextToFramePadding();
            ImGui::BeginGroup();
            ImGui::TextUnformatted("Min Value");
            ImGui::SetNextItemWidth(availalble_width / 2);
            if(ImGui::SliderFloat("##min_drag", &highlight_range_min, highlight_min,
                                  highlight_max, ""))
            {
                highlight_range_max = std::max(highlight_range_min, highlight_range_max);
                highlight_changed   = true;
            }
            ImGui::SetNextItemWidth(availalble_width / 2);
            if(ImGui::InputFloat("##min_input", &highlight_range_min, 0.0f, 0.0f, "%.1f"))
            {
                highlight_range_min =
                    std::clamp(highlight_range_min, highlight_min, highlight_max);
                highlight_range_max = std::max(highlight_range_min, highlight_range_max);
                highlight_changed   = true;
            }
            ImGui::EndGroup();
            ImGui::SameLine();
            ImGui::BeginGroup();
            ImGui::TextUnformatted("Max Value");
            ImGui::SetNextItemWidth(availalble_width / 2);
            if(ImGui::SliderFloat("##max_drag", &highlight_range_max, highlight_min,
                                  highlight_max, ""))
            {
                highlight_range_min = std::min(highlight_range_min, highlight_range_max);
                highlight_changed   = true;
            }
            ImGui::SetNextItemWidth(availalble_width / 2);
            if(ImGui::InputFloat("##max_input", &highlight_range_max, 0.0f, 0.0f, "%.1f"))
            {
                highlight_range_max =
                    std::clamp(highlight_range_max, highlight_min, highlight_max);
                highlight_range_min = std::min(highlight_range_min, highlight_range_max);
                highlight_changed   = true;
            }
            ImGui::EndGroup();
            ImGui::EndChild();
            ImGui::PopStyleColor();
            ImGui::PopStyleVar(2);
        }
        ImGui::EndDisabled();
        if(highlight_changed)
        {
            if(m_ctx.m_propagate == TimelineTrackOptions::kNone)
            {
                highlight.enabled   = highlight_toggle;
                highlight.range_min = highlight_range_min;
                highlight.range_max = highlight_range_max;
                m_updated           = true;
            }
            else
            {
                for(TrackOptions* sibling :
                    m_ctx.Siblings(m_track_item.GetTrackInfo()->topology.type,
                                   m_track_item.GetTrackInfo()->track_type))
                {
                    CounterTrackOptions* counter =
                        static_cast<CounterTrackOptions*>(sibling);
                    counter->m_highlight.enabled   = highlight_toggle;
                    counter->m_highlight.range_min = highlight_range_min;
                    counter->m_highlight.range_max = highlight_range_max;
                    counter->m_updated             = true;
                }
            }
            m_ctx.m_update_aggregates = true;
        }
        const AnalysisTrackStatistics* statistics = m_track_item.m_track_statistics;
        if(statistics)
        {
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                                ImGui::GetStyle().ItemInnerSpacing);
            ImGui::SeparatorText("Sample Metrics");
            for(size_t i = 0; i < AnalysisTrackStatistics::Counter::kCounterCount; i++)
            {
                ImGui::PushID(static_cast<int>(i));
                ImGui::PushStyleColor(
                    ImGuiCol_CheckMark,
                    m_settings.GetColorWheel()[statistics->stats[i].accent_color] |
                        IM_COL32_A_MASK);
                bool show = m_show_analysis[i];
                if(ImGui::Checkbox("", &show))
                {
                    if(m_ctx.m_propagate == TimelineTrackOptions::kNone)
                    {
                        m_show_analysis[i] = show;
                        m_updated          = true;
                    }
                    else
                    {
                        for(TrackOptions* sibling :
                            m_ctx.Siblings(m_track_item.GetTrackInfo()->topology.type,
                                           m_track_item.GetTrackInfo()->track_type))
                        {
                            CounterTrackOptions* counter =
                                static_cast<CounterTrackOptions*>(sibling);
                            if(counter->m_show_analysis[i] != show)
                            {
                                counter->m_show_analysis[i] = show;
                                counter->m_updated          = true;
                            }
                        }
                    }
                    m_ctx.m_update_aggregates = true;
                }
                ImGui::PopStyleColor();
                ImGui::SameLine();
                ImGui::Text("Show %s", statistics->stats[i].name);
                ImGui::PopID();
            }
            ImGui::PopStyleVar(2);
        }
    }
}

EventTrackOptions::EventTrackOptions(const TrackItem& track, TimelineTrackOptions& ctx,
                                     const std::string& project_id)
: TrackOptions(track, ctx, project_id)
, m_color_mode(EventColorMode::kByEventName)
, m_compact(false)
, m_expand(false)
{
    m_type_mask.set(TrackOptions::kEvent);
}

EventTrackOptions::EventTrackOptions(const EventTrackOptions& other)
: TrackOptions(other)
, m_color_mode(other.m_color_mode)
, m_compact(other.m_compact)
, m_expand(other.m_expand)
{
    m_type_mask.set(TrackOptions::kEvent);
}

EventTrackOptions&
EventTrackOptions::operator&=(const TrackOptions& other)
{
    TrackOptions::operator&=(other);
    const EventTrackOptions& event = static_cast<const EventTrackOptions&>(other);
    m_compact &= event.m_compact;
    m_expand &= event.m_expand;
    m_color_mode =
        (m_color_mode == event.m_color_mode) ? m_color_mode : EventColorMode::kMixed;
    return *this;
}

void
EventTrackOptions::ToJson()
{
    if(m_project_settings)
    {
        TrackOptions::ToJson();
        jt::Json& track =
            m_project_settings->GetJson()[JSON_KEY_GROUP_TIMELINE]
                                         [JSON_KEY_TIMELINE_TRACK][m_track_item.GetID()];
        track[JSON_KEY_TIMELINE_TRACK_COLOR]        = static_cast<int>(m_color_mode);
        track[JSON_KEY_TIMELINE_TRACK_COMPACT_MODE] = m_compact;
    }
}

bool
EventTrackOptions::Valid() const
{
    if(m_project_settings)
    {
        jt::Json& track =
            m_project_settings->GetJson()[JSON_KEY_GROUP_TIMELINE]
                                         [JSON_KEY_TIMELINE_TRACK][m_track_item.GetID()];
        return track[JSON_KEY_TIMELINE_TRACK_COLOR].isNumber() &&
               track[JSON_KEY_TIMELINE_TRACK_COMPACT_MODE].isBool() &&
               TrackOptions::Valid();
    }
    return false;
}

void
EventTrackOptions::FromJson()
{
    if(m_project_settings)
    {
        TrackOptions::FromJson();
        jt::Json& track =
            m_project_settings->GetJson()[JSON_KEY_GROUP_TIMELINE]
                                         [JSON_KEY_TIMELINE_TRACK][m_track_item.GetID()];
        double color_mode_raw = track[JSON_KEY_TIMELINE_TRACK_COLOR].getNumber();
        if(color_mode_raw >= 0 &&
           color_mode_raw < static_cast<int>(EventColorMode::__kCount))
        {
            m_color_mode = static_cast<EventColorMode>(color_mode_raw);
        }
        m_compact = track[JSON_KEY_TIMELINE_TRACK_COMPACT_MODE].getBool();
    }
}

void
EventTrackOptions::Render()
{
    TrackOptions::Render();
    if(m_track_item.GetTrackInfo())
    {
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));
        ImGui::SeparatorText("Event Appearance");
        EventColorMode color = m_color_mode;
        if(ImGui::RadioButton("Color by Name", color == EventColorMode::kByEventName))
        {
            color = EventColorMode::kByEventName;
            if(m_ctx.m_propagate == TimelineTrackOptions::kNone)
            {
                m_color_mode = color;
                m_updated    = true;
            }
            else
            {
                for(TrackOptions* sibling :
                    m_ctx.Siblings(m_track_item.GetTrackInfo()->topology.type,
                                   m_track_item.GetTrackInfo()->track_type))
                {
                    EventTrackOptions* event = static_cast<EventTrackOptions*>(sibling);
                    if(event->m_color_mode != color)
                    {
                        event->m_color_mode = color;
                        event->m_updated    = true;
                    }
                }
            }
            m_ctx.m_update_aggregates = true;
        }
        if(ImGui::RadioButton("Color by Time Level",
                              color == EventColorMode::kByTimeLevel))
        {
            color = EventColorMode::kByTimeLevel;
            if(m_ctx.m_propagate == TimelineTrackOptions::kNone)
            {
                m_color_mode = color;
                m_updated    = true;
            }
            else
            {
                for(TrackOptions* sibling :
                    m_ctx.Siblings(m_track_item.GetTrackInfo()->topology.type,
                                   m_track_item.GetTrackInfo()->track_type))
                {
                    EventTrackOptions* event = static_cast<EventTrackOptions*>(sibling);
                    if(event->m_color_mode != color)
                    {
                        event->m_color_mode = color;
                        event->m_updated    = true;
                    }
                }
            }
            m_ctx.m_update_aggregates = true;
        }
        if(ImGui::RadioButton("No Color", color == EventColorMode::kNone))
        {
            color = EventColorMode::kNone;
            if(m_ctx.m_propagate == TimelineTrackOptions::kNone)
            {
                m_color_mode = color;
                m_updated    = true;
            }
            else
            {
                for(TrackOptions* sibling :
                    m_ctx.Siblings(m_track_item.GetTrackInfo()->topology.type,
                                   m_track_item.GetTrackInfo()->track_type))
                {
                    EventTrackOptions* event = static_cast<EventTrackOptions*>(sibling);
                    if(event->m_color_mode != color)
                    {
                        event->m_color_mode = color;
                        event->m_updated    = true;
                    }
                }
            }
            m_ctx.m_update_aggregates = true;
        }
        ImGui::Separator();
        bool compact = m_compact;
        if(ImGui::Checkbox("Compact Mode", &compact))
        {
            if(m_ctx.m_propagate == TimelineTrackOptions::kNone)
            {
                m_compact = compact;
                m_updated = true;
            }
            else
            {
                for(TrackOptions* sibling :
                    m_ctx.Siblings(m_track_item.GetTrackInfo()->topology.type,
                                   m_track_item.GetTrackInfo()->track_type))
                {
                    EventTrackOptions* event = static_cast<EventTrackOptions*>(sibling);
                    if(event->m_compact != compact)
                    {
                        event->m_compact = compact;
                        event->m_updated = true;
                    }
                }
            }
            m_ctx.m_update_aggregates = true;
        }
        ImGui::PopStyleVar();
    }
}

QueueTrackOptions::QueueTrackOptions(const TrackItem& track, TimelineTrackOptions& ctx,
                                     const std::string& project_id)
: EventTrackOptions(track, ctx, project_id)
, m_show_queue_utilization(true)
{
    m_type_mask.set(TrackOptions::kQueue);
}

QueueTrackOptions::QueueTrackOptions(const QueueTrackOptions& other)
: EventTrackOptions(other)
, m_show_queue_utilization(other.m_show_queue_utilization)
{
    m_type_mask.set(TrackOptions::kQueue);
}

QueueTrackOptions&
QueueTrackOptions::operator&=(const TrackOptions& other)
{
    EventTrackOptions::operator&=(other);
    m_show_queue_utilization &=
        static_cast<const QueueTrackOptions&>(other).m_show_queue_utilization;
    return *this;
}

void
QueueTrackOptions::ToJson()
{
    if(m_project_settings)
    {
        EventTrackOptions::ToJson();
        m_project_settings
            ->GetJson()[JSON_KEY_GROUP_TIMELINE][JSON_KEY_TIMELINE_TRACK]
                       [m_track_item.GetID()][JSON_KEY_TIMELINE_TRACK_QUEUE_UTILIZATION] =
            m_show_queue_utilization;
    }
}

bool
QueueTrackOptions::Valid() const
{
    if(m_project_settings)
    {
        return m_project_settings
                   ->GetJson()[JSON_KEY_GROUP_TIMELINE][JSON_KEY_TIMELINE_TRACK]
                              [m_track_item.GetID()]
                              [JSON_KEY_TIMELINE_TRACK_QUEUE_UTILIZATION]
                   .isBool() &&
               EventTrackOptions::Valid();
    }
    return false;
}

void
QueueTrackOptions::FromJson()
{
    if(m_project_settings)
    {
        EventTrackOptions::FromJson();
        m_show_queue_utilization =
            m_project_settings
                ->GetJson()[JSON_KEY_GROUP_TIMELINE][JSON_KEY_TIMELINE_TRACK]
                           [m_track_item.GetID()]
                           [JSON_KEY_TIMELINE_TRACK_QUEUE_UTILIZATION]
                .getBool();
    }
}

void
QueueTrackOptions::Render()
{
    EventTrackOptions::Render();
    if(m_track_item.GetTrackInfo())
    {
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));
        const AnalysisTrackStatistics* statistics = m_track_item.m_track_statistics;
        if(statistics)
        {
            ImGui::SeparatorText("Queue Metrics");
            ImGui::PushStyleColor(
                ImGuiCol_CheckMark,
                m_settings.GetColorWheel()
                        [statistics
                             ->stats[AnalysisTrackStatistics::Queue::kQueueUtilization]
                             .accent_color] |
                    IM_COL32_A_MASK);
            bool util = m_show_queue_utilization;
            if(ImGui::Checkbox("##queue_util", &util))
            {
                if(m_ctx.m_propagate == TimelineTrackOptions::kNone)
                {
                    m_show_queue_utilization = util;
                    m_updated                = true;
                }
                else
                {
                    for(TrackOptions* sibling :
                        m_ctx.Siblings(m_track_item.GetTrackInfo()->topology.type,
                                       m_track_item.GetTrackInfo()->track_type))
                    {
                        QueueTrackOptions* queue =
                            static_cast<QueueTrackOptions*>(sibling);
                        if(queue->m_show_queue_utilization != util)
                        {
                            queue->m_show_queue_utilization = util;
                            queue->m_updated                = true;
                        }
                    }
                }
                m_ctx.m_update_aggregates = true;
            }
            ImGui::PopStyleColor();
            ImGui::SameLine();
            ImGui::Text(
                "Show %s",
                statistics->stats[AnalysisTrackStatistics::Queue::kQueueUtilization]
                    .name);
        }
        ImGui::PopStyleVar();
    }
}

TimelineTrackOptions::TimelineTrackOptions(const TimelineSelection& selection)
: m_propagate(kNone)
, m_context_menu_target(nullptr)
, m_init_context_menu(false)
, m_update_aggregates(false)
, m_siblings_by_topology_type({})
, m_selection(selection)
, m_settings(SettingsManager::GetInstance())
{}

std::unique_ptr<TrackOptions>
TimelineTrackOptions::InitTrack(const TrackItem& track)
{
    std::unique_ptr<TrackOptions> options = nullptr;
    if(track.GetTrackInfo())
    {
        switch(track.GetTrackInfo()->topology.type)
        {
            case TrackInfo::Unknown:
            {
                if(track.GetTrackInfo()->track_type == kRPVControllerTrackTypeSamples)
                {
                    options = std::make_unique<CounterTrackOptions>(
                        track, *this, track.m_data_provider.GetTraceFilePath());
                }
                else if(track.GetTrackInfo()->track_type == kRPVControllerTrackTypeEvents)
                {
                    options = std::make_unique<EventTrackOptions>(
                        track, *this, track.m_data_provider.GetTraceFilePath());
                }
                break;
            }
            case TrackInfo::Queue:
            {
                options = std::make_unique<QueueTrackOptions>(
                    track, *this, track.m_data_provider.GetTraceFilePath());
                break;
            }
            case TrackInfo::Stream:
            case TrackInfo::InstrumentedThread:
            case TrackInfo::SampledThread:
            {
                options = std::make_unique<EventTrackOptions>(
                    track, *this, track.m_data_provider.GetTraceFilePath());
                break;
            }
            case TrackInfo::Counter:
            {
                options = std::make_unique<CounterTrackOptions>(
                    track, *this, track.m_data_provider.GetTraceFilePath());
                break;
            }
            default:
            {
                ROCPROFVIS_ASSERT(false);
                break;
            }
        }
        if(options)
        {
            if(options->Valid())
            {
                options->FromJson();
            }
            m_options_map[track.GetID()] = options.get();
            m_siblings_by_topology_type[track.GetTrackInfo()->topology.type].emplace_back(
                options.get());
            m_siblings_by_data_type[track.GetTrackInfo()->track_type].emplace_back(
                options.get());
        }
    }
    return options;
}

void
TimelineTrackOptions::Update()
{
    if(m_context_menu_target && m_context_menu_target->GetTrackInfo())
    {
        if(m_init_context_menu)
        {
            m_options_aggregate_selected = nullptr;
            m_options_aggregate_type     = nullptr;
            m_siblings_by_selection.clear();
            if(!m_options_aggregate_selected && m_selection.HasSelectedTracks())
            {
                std::vector<uint64_t> selected_track_ids;
                m_selection.GetSelectedTracks(selected_track_ids);
                for(uint64_t track_id : selected_track_ids)
                {
                    if(m_options_map.count(track_id) > 0 && m_options_map.at(track_id))
                    {
                        m_siblings_by_selection.push_back(m_options_map.at(track_id));
                    }
                }
                std::bitset<TrackOptions::kNumTypes> selected_type_mask =
                    std::bitset<TrackOptions::kNumTypes>().set();
                for(TrackOptions* option : m_siblings_by_selection)
                {
                    selected_type_mask &= option->TypeMask();
                }
                m_options_aggregate_selected =
                    CreateAggregate(selected_type_mask, m_siblings_by_selection);
            }
            m_options_aggregate_type = CreateAggregate(
                m_context_menu_target->m_options->TypeMask(),
                m_context_menu_target->GetTrackInfo()->topology.type == TrackInfo::Unknown
                    ? m_siblings_by_data_type[m_context_menu_target->GetTrackInfo()
                                                  ->track_type]
                    : m_siblings_by_topology_type[m_context_menu_target->GetTrackInfo()
                                                      ->topology.type]);
            m_init_context_menu = false;
        }
        if(m_update_aggregates)
        {
            if(m_options_aggregate_selected && !m_siblings_by_selection.empty())
            {
                m_options_aggregate_selected = CreateAggregate(
                    m_options_aggregate_selected->TypeMask(), m_siblings_by_selection);
            }
            if(m_options_aggregate_type)
            {
                m_options_aggregate_type = CreateAggregate(
                    m_options_aggregate_type->TypeMask(),
                    m_context_menu_target->GetTrackInfo()->topology.type ==
                            TrackInfo::Unknown
                        ? m_siblings_by_data_type[m_context_menu_target->GetTrackInfo()
                                                      ->track_type]
                        : m_siblings_by_topology_type
                              [m_context_menu_target->GetTrackInfo()->topology.type]);
            }
            m_update_aggregates = false;
        }
    }
}

void
TimelineTrackOptions::InitTrackOptionsSubmenu(const TrackItem& target)
{
    m_context_menu_target = nullptr;
    if(target.GetTrackInfo() && target.m_options)
    {
        m_propagate           = kNone;
        m_context_menu_target = &target;
        m_init_context_menu   = true;
    }
}

void
TimelineTrackOptions::RenderTrackOptionsSubmenu()
{
    if(m_context_menu_target && m_context_menu_target->m_options)
    {
        RenderPropagateControl();
        switch(m_propagate)
        {
            case Propagate::kNone:
            {
                m_context_menu_target->m_options->Render();
                break;
            }
            case Propagate::kSelected:
            {
                if(m_options_aggregate_selected)
                {
                    m_options_aggregate_selected->Render();
                }
                break;
            }
            case Propagate::kSiblings:
            {
                if(m_options_aggregate_type)
                {
                    m_options_aggregate_type->Render();
                }
                break;
            }
            default:
            {
                ROCPROFVIS_ASSERT(false);
                break;
            }
        }
    }
}

bool
TimelineTrackOptions::ShowHiddenTracksSubmenu() const
{
    for(const auto& entry : m_options_map)
    {
        if(entry.second && !entry.second->m_display)
        {
            return true;
        }
    }
    return false;
}

void
TimelineTrackOptions::RenderHiddenTracksSubmenu()
{
    // Group hidden tracks by category (topology-type order; unknown-topology
    // tracks fall back to a data-type label).
    std::vector<std::pair<const char*, std::vector<TrackOptions*>>> categories;
    auto bucket = [&categories](const char* label) -> std::vector<TrackOptions*>& {
        for(auto& category : categories)
        {
            if(std::strcmp(category.first, label) == 0)
            {
                return category.second;
            }
        }
        categories.emplace_back(label, std::vector<TrackOptions*>{});
        return categories.back().second;
    };

    for(size_t type = TrackInfo::Queue; type < static_cast<size_t>(TrackInfo::Count);
        ++type)
    {
        for(TrackOptions* option : m_siblings_by_topology_type[type])
        {
            if(option && !option->m_display)
            {
                bucket(DISPLAY_STRINGS_TOPOLOGY_TRACK_TYPES[type]).push_back(option);
            }
        }
    }
    for(TrackOptions* option : m_siblings_by_topology_type[TrackInfo::Unknown])
    {
        if(!option || option->m_display)
        {
            continue;
        }
        const TrackInfo* info = option->GetTrackItem().GetTrackInfo();
        const char* label = (info && info->track_type == kRPVControllerTrackTypeSamples)
                                ? DISPLAY_STRINGS_TOPOLOGY_TRACK_TYPES[TrackInfo::Counter]
                                : "Event Tracks";
        bucket(label).push_back(option);
    }

    // Null icon keeps rows aligned with the icon-bearing parent menu.
    if(IconMenuItem(nullptr, "Show All Hidden Tracks"))
    {
        ShowAllHiddenTracks();
    }
    ImGui::Separator();

    // Cap category submenu height so long lists scroll instead of filling the screen.
    const float max_submenu_height =
        ImGui::GetMainViewport()->WorkSize.y * HIDDEN_TRACKS_MENU_MAX_HEIGHT_FRACTION;
    for(auto& category : categories)
    {
        ImGui::SetNextWindowSizeConstraints(ImVec2(0.0f, 0.0f),
                                            ImVec2(FLT_MAX, max_submenu_height));
        if(IconBeginMenu(nullptr, category.first))
        {
            if(IconMenuItem(nullptr, "Show All Hidden"))
            {
                ShowTracks(category.second);
            }
            ImGui::Separator();
            for(TrackOptions* option : category.second)
            {
                // Hidden ##id suffix keeps ids unique when names repeat.
                const std::string item = option->GetTrackItem().GetName() + "##" +
                                         std::to_string(option->GetTrackItem().GetID());
                if(IconMenuItem(nullptr, item.c_str()))
                {
                    ShowTracks({ option });
                }
            }
            ImGui::EndMenu();
        }
    }
}

void
TimelineTrackOptions::SetTrackSortSubmenu(std::function<void()> renderer)
{
    m_render_sort_menu = std::move(renderer);
}

bool
TimelineTrackOptions::ShowTrackSortSubmenu() const
{
    return static_cast<bool>(m_render_sort_menu);
}

void
TimelineTrackOptions::RenderTrackSortSubmenu() const
{
    if(m_render_sort_menu)
    {
        m_render_sort_menu();
    }
}

void
TimelineTrackOptions::ShowTracks(const std::vector<TrackOptions*>& options)
{
    // Sourced from a revealed track rather than the context-menu target, so this
    // also works when the menu was opened without one (all tracks hidden).
    const TrackItem* revealed = nullptr;
    for(TrackOptions* option : options)
    {
        if(option && !option->m_display)
        {
            option->m_display = true;
            revealed          = &option->GetTrackItem();
        }
    }
    if(revealed)
    {
        // Aggregated context-menu options track visibility too, so refresh them.
        m_update_aggregates = true;
        EventManager::GetInstance()->AddEvent(std::make_shared<RocEvent>(
            static_cast<int>(RocEvents::kTrackVisibilityChanged),
            revealed->m_data_provider.GetTraceFilePath()));
    }
}

void
TimelineTrackOptions::ShowAllHiddenTracks()
{
    std::vector<TrackOptions*> hidden;
    for(const auto& entry : m_options_map)
    {
        if(entry.second && !entry.second->m_display)
        {
            hidden.push_back(entry.second);
        }
    }
    ShowTracks(hidden);
}

std::unique_ptr<TrackOptions>
TimelineTrackOptions::CreateAggregate(
    const std::bitset<TrackOptions::kNumTypes>& type_mask,
    const std::vector<TrackOptions*>&           components)
{
    std::unique_ptr<TrackOptions> aggregate = nullptr;
    if(!components.empty())
    {
        TrackOptions::Type type = TrackOptions::kTrack;
        for(size_t i = type_mask.size() - 1; i > 0; i--)
        {
            if(type_mask.test(i))
            {
                type = static_cast<TrackOptions::Type>(i);
                break;
            }
        }
        switch(type)
        {
            case TrackOptions::kQueue:
            {
                aggregate = std::make_unique<QueueTrackOptions>(
                    static_cast<QueueTrackOptions&>(*components.front()));
                break;
            }
            case TrackOptions::kEvent:
            {
                aggregate = std::make_unique<EventTrackOptions>(
                    static_cast<EventTrackOptions&>(*components.front()));
                break;
            }
            case TrackOptions::kCounter:
            {
                aggregate = std::make_unique<CounterTrackOptions>(
                    static_cast<CounterTrackOptions&>(*components.front()));
                break;
            }
            case TrackOptions::kTrack:
            {
                aggregate = std::make_unique<TrackOptions>(
                    static_cast<TrackOptions&>(*components.front()));
                break;
            }
            default:
            {
                ROCPROFVIS_ASSERT(false);
                break;
            }
        }
        if(aggregate)
        {
            for(TrackOptions* component : components)
            {
                *aggregate &= *component;
            }
        }
    }

    return aggregate;
}

std::vector<TrackOptions*>
TimelineTrackOptions::Siblings(const TrackInfo::TrackType&               topology_type,
                               const rocprofvis_controller_track_type_t& data_type)
{
    if(m_propagate == Propagate::kSelected)
    {
        return m_siblings_by_selection;
    }
    else if(topology_type == TrackInfo::Unknown)
    {
        return m_siblings_by_data_type[data_type];
    }
    else
    {
        return m_siblings_by_topology_type[topology_type];
    }
}

void
TimelineTrackOptions::RenderPropagateControl()
{
    if(m_context_menu_target && m_context_menu_target->GetTrackInfo())
    {
        ImVec2 screen_pos = ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddRectFilled(
            screen_pos,
            screen_pos +
                ImVec2(
                    ImGui::CalcTextSize("Current Track").x +
                        ImGui::CalcTextSize("Selected Track(s)").x +
                        ImGui::CalcTextSize(
                            m_context_menu_target->GetTrackInfo()->topology.type ==
                                    TrackInfo::Unknown
                                ? DISPLAY_STRINGS_TRACK_DATA_TYPES
                                      [m_context_menu_target->GetTrackInfo()->track_type]
                                : DISPLAY_STRINGS_TOPOLOGY_TRACK_TYPES
                                      [m_context_menu_target->GetTrackInfo()
                                           ->topology.type])
                            .x +
                        6.0f * m_settings.GetDefaultStyle().FramePadding.x,
                    ImGui::GetFrameHeight()),
            m_settings.GetColor(Colors::kButton),
            m_settings.GetDefaultStyle().FrameRounding);
        ImGui::SetCursorPos(screen_pos - ImGui::GetWindowPos());
        if(ColoredButton(
               "Current Track",
               m_settings.GetColor(m_propagate == kNone ? Colors::kAccent
                                                        : Colors::kButton),
               m_settings.GetColor(m_propagate == kNone ? Colors::kAccent
                                                        : Colors::kButtonHovered),
               m_settings.GetColor(m_propagate == kNone ? Colors::kAccent
                                                        : Colors::kButtonActive),
               m_settings.GetColor(m_propagate == kNone ? Colors::kTextOnAccent
                                                        : Colors::kTextMain),
               "Apply changes to the current track."))
        {
            m_propagate = kNone;
        }
        ImGui::SameLine(0.0f, 0.0f);
        ImGui::BeginDisabled(!m_options_aggregate_selected);
        if(ColoredButton("Selected Track(s)",
                         m_settings.GetColor(m_propagate == Propagate::kSelected
                                                 ? Colors::kAccent
                                                 : Colors::kButton),
                         m_settings.GetColor(m_propagate == Propagate::kSelected
                                                 ? Colors::kAccent
                                                 : Colors::kButtonHovered),
                         m_settings.GetColor(m_propagate == Propagate::kSelected
                                                 ? Colors::kAccent
                                                 : Colors::kButtonActive),
                         m_settings.GetColor(m_propagate == Propagate::kSelected
                                                 ? Colors::kTextOnAccent
                                                 : Colors::kTextMain),
                         "Apply changes to selected track(s)."))
        {
            m_propagate = Propagate::kSelected;
        }
        ImGui::EndDisabled();
        ImGui::SameLine(0.0f, 0.0f);
        if(ColoredButton(
               m_context_menu_target->GetTrackInfo()->topology.type == TrackInfo::Unknown
                   ? DISPLAY_STRINGS_TRACK_DATA_TYPES
                         [m_context_menu_target->GetTrackInfo()->track_type]
                   : DISPLAY_STRINGS_TOPOLOGY_TRACK_TYPES
                         [m_context_menu_target->GetTrackInfo()->topology.type],
               m_settings.GetColor(m_propagate == Propagate::kSiblings ? Colors::kAccent
                                                                       : Colors::kButton),
               m_settings.GetColor(m_propagate == Propagate::kSiblings
                                       ? Colors::kAccent
                                       : Colors::kButtonHovered),
               m_settings.GetColor(m_propagate == Propagate::kSiblings
                                       ? Colors::kAccent
                                       : Colors::kButtonActive),
               m_settings.GetColor(m_propagate == Propagate::kSiblings
                                       ? Colors::kTextOnAccent
                                       : Colors::kTextMain),
               "Apply changes to all tracks of the same type."))
        {
            m_propagate = Propagate::kSiblings;
        }
    }
}

}  // namespace View
}  // namespace RocProfVis
