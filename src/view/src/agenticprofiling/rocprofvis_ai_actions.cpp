// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_ai_actions.h"

#include <cctype>
#include <cmath>
#include <memory>

#include "compute/rocprofvis_compute_selection.h"
#include "rocprofvis_appwindow.h"
#include "rocprofvis_data_provider.h"
#include "rocprofvis_event_manager.h"
#include "rocprofvis_events.h"
#include "rocprofvis_settings_manager.h"
#include "rocprofvis_timeline_selection.h"
#include "rocprofvis_trace_view.h"
#include "widgets/rocprofvis_log_viewer.h"
#include "widgets/rocprofvis_notification_manager.h"

namespace RocProfVis
{
namespace View
{

namespace
{

// True when a time range is one we can act on.
//
// The finiteness test is the point of this function. Every comparison against
// NaN is false, so a bare "end_ns <= start_ns" guard lets NaN and infinity
// straight through, and these numbers come from the model.
bool
IsUsableRange(double start_ns, double end_ns)
{
    return std::isfinite(start_ns) && std::isfinite(end_ns) && end_ns > start_ns;
}

// True when a single point in time is one we can act on. Same reasoning as
// IsUsableRange, for the arguments that are an instant rather than a span.
bool
IsUsableInstant(double time_ns)
{
    return std::isfinite(time_ns);
}

// Reduces a panel name to lowercase letters and digits, so "Mini Map",
// "mini-map", and "minimap" all arrive as the same key.
std::string
NormalizePanelKey(const std::string& name)
{
    std::string key;
    key.reserve(name.size());
    for(char c : name)
    {
        if(std::isalnum(static_cast<unsigned char>(c)) != 0)
        {
            key += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
    }
    return key;
}

// Drops the filler users wrap a panel name in - "the topology view", "details
// panel" - leaving the alias itself. Worth trying only after an exact match has
// failed, otherwise "overview" would be cut down to "over".
std::string
StripPanelFiller(const std::string& key)
{
    static const std::string k_prefixes[] = { "the", "my" };
    static const std::string k_suffixes[] = { "view", "panel", "window", "pane",
                                              "tab",  "area",  "section" };
    std::string stripped = key;
    for(const std::string& prefix : k_prefixes)
    {
        if(stripped.size() > prefix.size() &&
           stripped.compare(0, prefix.size(), prefix) == 0)
        {
            stripped.erase(0, prefix.size());
            break;
        }
    }
    for(const std::string& suffix : k_suffixes)
    {
        if(stripped.size() > suffix.size() &&
           stripped.compare(stripped.size() - suffix.size(), suffix.size(),
                            suffix) == 0)
        {
            stripped.erase(stripped.size() - suffix.size());
            break;
        }
    }
    return stripped;
}

// Every name a user might reach for, keyed on the normalized form. No alias is
// repeated across panels, so a name can never resolve two ways.
OptiqPanel
PanelFromKey(const std::string& key)
{
    if(key == "minimap" || key == "map" || key == "overview" ||
       key == "birdseye" || key == "thumbnail")
    {
        return OptiqPanel::kMinimap;
    }
    if(key == "histogram" || key == "activity" || key == "activitybar" ||
       key == "activitygraph" || key == "activitychart" || key == "density")
    {
        return OptiqPanel::kHistogram;
    }
    // The tall tree down the left, which users call the sidebar, the navbar,
    // the track list, or just "the tracks".
    if(key == "topology" || key == "sidebar" || key == "side" ||
       key == "leftsidebar" || key == "left" || key == "navbar" || key == "nav" ||
       key == "navigation" || key == "navigator" || key == "tree" ||
       key == "hierarchy" || key == "track" || key == "tracks" ||
       key == "tracklist" || key == "project" || key == "projecttree")
    {
        return OptiqPanel::kTopology;
    }
    if(key == "details" || key == "detail" || key == "advanced" ||
       key == "analysis" || key == "table" || key == "tables" ||
       key == "eventtable" || key == "eventtables" || key == "bottom" ||
       key == "inspector")
    {
        return OptiqPanel::kDetails;
    }
    if(key == "summary" || key == "stats" || key == "statistics")
    {
        return OptiqPanel::kSummary;
    }
    if(key == "log" || key == "logs" || key == "logviewer" || key == "console" ||
       key == "output" || key == "messages")
    {
        return OptiqPanel::kLogViewer;
    }
    if(key == "toolbar" || key == "tools" || key == "topbar" || key == "top" ||
       key == "buttons" || key == "controls")
    {
        return OptiqPanel::kToolbar;
    }
    if(key == "annotations" || key == "annotation" || key == "notes" ||
       key == "note" || key == "sticky" || key == "stickynote" ||
       key == "stickynotes" || key == "comments")
    {
        return OptiqPanel::kAnnotations;
    }
    return OptiqPanel::kUnknown;
}

}  // namespace

// Binds to one trace's view objects. Any of them may be null.
OptiqActions::OptiqActions(DataProvider*      data_provider,
                           TimelineSelection* timeline_selection,
                           ComputeSelection*  compute_selection, TraceView* trace_view)
: m_data_provider(data_provider)
, m_timeline_selection(timeline_selection)
, m_compute_selection(compute_selection)
, m_trace_view(trace_view)
{}

// Resolves whatever the model called a panel to one of ours.
OptiqPanel
OptiqActions::PanelFromName(const std::string& name)
{
    const std::string key   = NormalizePanelKey(name);
    const OptiqPanel  exact = PanelFromKey(key);
    if(exact != OptiqPanel::kUnknown)
    {
        return exact;
    }
    // Users rarely name a panel bare: "close the topology view" and "hide the
    // details panel" both carry a word we do not care about.
    return PanelFromKey(StripPanelFiller(key));
}

// The canonical name of a panel, for reporting back what was changed.
const char*
OptiqActions::PanelName(OptiqPanel panel)
{
    switch(panel)
    {
        case OptiqPanel::kMinimap:   return "minimap";
        case OptiqPanel::kHistogram: return "histogram";
        case OptiqPanel::kTopology:  return "topology";
        case OptiqPanel::kDetails:   return "details";
        case OptiqPanel::kSummary:   return "summary";
        case OptiqPanel::kLogViewer: return "log";
        case OptiqPanel::kToolbar:     return "toolbar";
        case OptiqPanel::kAnnotations: return "annotations";
        default:                       return "unknown";
    }
}

// Every panel name, for the tool schema and for bad-argument replies.
std::string
OptiqActions::PanelNameList()
{
    return "minimap (map, overview), histogram (activity), topology (sidebar, "
           "navbar, tree, tracks, left panel), details (analysis, tables, event "
           "table, bottom), summary (stats), log (console, output), toolbar (top "
           "bar), annotations (notes, sticky notes). Whatever the user called it "
           "is fine: matching ignores case, spacing, and trailing words like "
           "\"view\" or \"panel\"";
}

// Where a panel's visibility actually lives, which is what decides whether a
// live TraceView is needed to change it.
//
//  - kTraceViewOnly       drawn by a TraceView widget, nothing persisted.
//  - kSettingAndTraceView persisted in AppWindowSettings and mirrored on a
//                         widget that was seeded from it at construction, so
//                         both have to move: the setting is what survives a
//                         restart, the widget is what redraws now.
//  - kSettingAndAppWindow persisted and mirrored on the AppWindow layout.
//  - kSettingOnly         persisted only; whoever draws it re-reads the flag
//                         every frame, so there is no widget to miss.
//  - kLogViewer           its own singleton, independent of any trace.
//
// Only trace-local panels need the active tab to be a TraceView. Setting-backed
// panels follow the View menu: they update every open system trace even when a
// compute tab is in front.
namespace
{

enum class PanelBacking
{
    kTraceViewOnly,
    kSettingAndTraceView,
    kSettingAndAppWindow,
    kSettingOnly,
    kLogViewer
};

PanelBacking
BackingOf(OptiqPanel panel)
{
    switch(panel)
    {
        case OptiqPanel::kMinimap:
        case OptiqPanel::kAnnotations: return PanelBacking::kTraceViewOnly;
        case OptiqPanel::kHistogram:
        case OptiqPanel::kTopology:
        case OptiqPanel::kDetails:     return PanelBacking::kSettingAndTraceView;
        case OptiqPanel::kToolbar:     return PanelBacking::kSettingAndAppWindow;
        case OptiqPanel::kSummary:     return PanelBacking::kSettingOnly;
        case OptiqPanel::kLogViewer:   return PanelBacking::kLogViewer;
        default:                       return PanelBacking::kSettingOnly;
    }
}

// True when this panel cannot be touched without the active TraceView.
bool
NeedsActiveTraceView(OptiqPanel panel)
{
    return BackingOf(panel) == PanelBacking::kTraceViewOnly;
}

}  // namespace

// Opens or closes a panel, the same as ticking its View-menu item.
bool
OptiqActions::ShowPanel(OptiqPanel panel, bool visible)
{
    if(NeedsActiveTraceView(panel) && m_trace_view == nullptr)
    {
        return false;
    }

    AppWindowSettings& app_settings =
        SettingsManager::GetInstance().GetAppWindowSettings();

    switch(panel)
    {
        case OptiqPanel::kMinimap:
        {
            m_trace_view->SetMinimapVisibility(visible);
            return true;
        }
        case OptiqPanel::kAnnotations:
        {
            m_trace_view->SetAnnotationsVisible(visible);
            return true;
        }
        case OptiqPanel::kHistogram:
        {
            app_settings.show_histogram = visible;
            AppWindow::GetInstance()->ApplyPanelVisibilitySettings();
            return true;
        }
        case OptiqPanel::kTopology:
        {
            app_settings.show_sidebar = visible;
            AppWindow::GetInstance()->ApplyPanelVisibilitySettings();
            return true;
        }
        case OptiqPanel::kDetails:
        {
            app_settings.show_details_panel = visible;
            AppWindow::GetInstance()->ApplyPanelVisibilitySettings();
            return true;
        }
        case OptiqPanel::kSummary:
        {
            app_settings.show_summary = visible;
            return true;
        }
        case OptiqPanel::kToolbar:
        {
            app_settings.show_toolbar = visible;
            AppWindow::GetInstance()->ApplyPanelVisibilitySettings();
            return true;
        }
        case OptiqPanel::kLogViewer:
        {
            LogViewer* log_viewer = LogViewer::GetInstance();
            if(log_viewer == nullptr)
            {
                return false;
            }
            *log_viewer->VisiblePtr() = visible;
            return true;
        }
        default: return false;
    }
}

// Reads a panel's current visibility, so a tool can report it without toggling.
// Fails wherever ShowPanel would, so the two never disagree about whether this
// panel is reachable at all.
bool
OptiqActions::IsPanelVisible(OptiqPanel panel, bool& visible_out) const
{
    if(NeedsActiveTraceView(panel) && m_trace_view == nullptr)
    {
        return false;
    }

    const AppWindowSettings& app_settings =
        SettingsManager::GetInstance().GetAppWindowSettings();

    switch(panel)
    {
        case OptiqPanel::kMinimap:
        {
            visible_out = m_trace_view->IsMinimapVisible();
            return true;
        }
        case OptiqPanel::kAnnotations:
        {
            visible_out = m_trace_view->AreAnnotationsVisible();
            return true;
        }
        case OptiqPanel::kHistogram:
        {
            visible_out = app_settings.show_histogram;
            return true;
        }
        case OptiqPanel::kTopology:
        {
            visible_out = app_settings.show_sidebar;
            return true;
        }
        case OptiqPanel::kDetails:
        {
            visible_out = app_settings.show_details_panel;
            return true;
        }
        case OptiqPanel::kSummary:
        {
            visible_out = app_settings.show_summary;
            return true;
        }
        case OptiqPanel::kToolbar:
        {
            visible_out = app_settings.show_toolbar;
            return true;
        }
        case OptiqPanel::kLogViewer:
        {
            LogViewer* log_viewer = LogViewer::GetInstance();
            if(log_viewer == nullptr)
            {
                return false;
            }
            visible_out = *log_viewer->VisiblePtr();
            return true;
        }
        default: return false;
    }
}

// True when the timeline actions are usable, i.e. a system trace is in front.
bool
OptiqActions::HasTimeline() const
{
    return m_data_provider != nullptr && m_timeline_selection != nullptr;
}

// True when the compute actions are usable, i.e. a compute trace is in front.
bool
OptiqActions::HasCompute() const
{
    return m_data_provider != nullptr && m_compute_selection != nullptr;
}

// The event source id the rest of the app filters on, which is the trace path.
std::string
OptiqActions::SourceId() const
{
    return m_data_provider != nullptr ? m_data_provider->GetTraceFilePath()
                                      : std::string();
}

// The open traces, by the tab label the user sees.
std::vector<std::string>
OptiqActions::ListTabs() const
{
    std::vector<std::string> names;
    AppWindow*               app = AppWindow::GetInstance();
    if(app == nullptr || !app->GetTabContainer())
    {
        return names;
    }
    for(const TabItem* tab : app->GetTabContainer()->GetTabs())
    {
        if(tab != nullptr)
        {
            names.push_back(tab->m_label);
        }
    }
    return names;
}

// The label of the trace currently in front.
std::string
OptiqActions::ActiveTab() const
{
    AppWindow* app = AppWindow::GetInstance();
    if(app == nullptr || !app->GetTabContainer())
    {
        return std::string();
    }
    const TabItem* active = app->GetTabContainer()->GetActiveTab();
    return active != nullptr ? active->m_label : std::string();
}

// Brings a trace tab to the front by name.
bool
OptiqActions::SelectTab(const std::string& name)
{
    AppWindow* app = AppWindow::GetInstance();
    if(app == nullptr || !app->GetTabContainer() || name.empty())
    {
        return false;
    }

    // Matching by label, including the ambiguity rules, belongs to the
    // container that owns the tabs; the details panel resolves its inner tabs
    // through the same call.
    std::shared_ptr<TabContainer> container = app->GetTabContainer();
    const TabItem*                match    = container->FindTabByLabel(name);
    if(match == nullptr)
    {
        return false;
    }
    container->SetActiveTab(match->m_id);
    return true;
}

// The details panel's inner tabs, by label.
std::vector<std::string>
OptiqActions::ListAnalysisTabs() const
{
    return m_trace_view != nullptr ? m_trace_view->ListAnalysisTabs()
                                   : std::vector<std::string>();
}

// The label of the details tab currently showing.
std::string
OptiqActions::ActiveAnalysisTab() const
{
    return m_trace_view != nullptr ? m_trace_view->ActiveAnalysisTab() : std::string();
}

// Selects a details tab, opening the panel if it was hidden.
bool
OptiqActions::SelectAnalysisTab(const std::string& name)
{
    return m_trace_view != nullptr && m_trace_view->SelectAnalysisTab(name);
}

// Shows or hides the arrows linking an event to what it launched or waited on.
bool
OptiqActions::SetFlowArrowsVisible(bool visible)
{
    if(m_trace_view == nullptr)
    {
        return false;
    }
    m_trace_view->SetFlowArrowsVisible(visible);
    return true;
}

// Reads whether the flow arrows are currently drawn.
bool
OptiqActions::AreFlowArrowsVisible(bool& visible_out) const
{
    if(m_trace_view == nullptr)
    {
        return false;
    }
    visible_out = m_trace_view->AreFlowArrowsVisible();
    return true;
}

// Switches the flow arrows between fanning out and chaining through the sequence.
bool
OptiqActions::SetFlowRenderChained(bool chained)
{
    if(m_trace_view == nullptr)
    {
        return false;
    }
    m_trace_view->SetFlowRenderChained(chained);
    return true;
}

// Zooms back out to the whole trace, the same as the Reset View button.
bool
OptiqActions::ResetView()
{
    if(m_trace_view == nullptr)
    {
        return false;
    }
    m_trace_view->ResetView();
    return true;
}

// The bookmark slots that currently hold a view.
std::vector<int>
OptiqActions::ListBookmarks() const
{
    return m_trace_view != nullptr ? m_trace_view->ListBookmarks() : std::vector<int>();
}

// Stores the current zoom and scroll position in a numbered slot.
bool
OptiqActions::SaveBookmark(int slot)
{
    return m_trace_view != nullptr && m_trace_view->SaveBookmark(slot);
}

// Restores the view saved in a slot.
bool
OptiqActions::GotoBookmark(int slot)
{
    return m_trace_view != nullptr && m_trace_view->GotoBookmark(slot);
}

// Empties a bookmark slot.
bool
OptiqActions::RemoveBookmark(int slot)
{
    return m_trace_view != nullptr && m_trace_view->RemoveBookmark(slot);
}

// Drops the two measurement pins on a span, the same as the Measure tool.
bool
OptiqActions::MeasureRange(double start_ns, double end_ns)
{
    if(m_trace_view == nullptr || !IsUsableRange(start_ns, end_ns))
    {
        return false;
    }
    return m_trace_view->MeasureRange(start_ns, end_ns);
}

// Takes the measurement pins back off the timeline.
bool
OptiqActions::ClearMeasurement()
{
    if(m_trace_view == nullptr)
    {
        return false;
    }
    m_trace_view->ClearMeasurement();
    return true;
}

// Zooms the visible window to a range, rather than only selecting it.
bool
OptiqActions::ZoomToRange(double start_ns, double end_ns)
{
    if(m_trace_view == nullptr || !IsUsableRange(start_ns, end_ns))
    {
        return false;
    }
    m_trace_view->ZoomToRange(start_ns, end_ns);
    return true;
}

// Pins a sticky note, which is saved with the project and outlives the chat.
bool
OptiqActions::AddNote(double time_ns, const std::string& title, const std::string& text,
                      double v_min, double v_max, uint64_t track_id)
{
    if(m_trace_view == nullptr || !IsUsableInstant(time_ns) ||
       !IsUsableRange(v_min, v_max))
    {
        return false;
    }
    return m_trace_view->AddNote(time_ns, title, text, v_min, v_max, track_id);
}

// Raises a toast, for telling the user something without them reading the panel.
void
OptiqActions::Notify(const std::string& message, bool is_warning)
{
    NotificationManager::GetInstance().Show(
        message, is_warning ? NotificationLevel::Warning : NotificationLevel::Info);
}

// Drag-selects a time range, the same as dragging on the ruler.
bool
OptiqActions::SelectRange(double start_ns, double end_ns)
{
    if(!HasTimeline() || !IsUsableRange(start_ns, end_ns))
    {
        return false;
    }
    m_timeline_selection->SelectTimeRange(start_ns, end_ns);
    return true;
}

// Drops the range selection.
bool
OptiqActions::ClearRange()
{
    if(!HasTimeline())
    {
        return false;
    }
    m_timeline_selection->ClearTimeRange();
    return true;
}

// Moves the visible window without changing the selection.
bool
OptiqActions::ShowRange(double start_ns, double end_ns)
{
    // The timeline is what listens for this event, so require it rather than
    // just a data provider: a compute trace has the latter but not the former.
    if(!HasTimeline() || !IsUsableRange(start_ns, end_ns))
    {
        return false;
    }
    EventManager::GetInstance()->AddEvent(std::make_shared<RangeEvent>(
        static_cast<int>(RocEvents::kSetViewRange), start_ns, end_ns, SourceId()));
    return true;
}

// Scrolls a track into view on the timeline.
bool
OptiqActions::ScrollToTrack(uint64_t track_id)
{
    if(m_data_provider == nullptr)
    {
        return false;
    }
    EventManager::GetInstance()->AddEvent(std::make_shared<ScrollToTrackEvent>(
        static_cast<int>(RocEvents::kHandleUserGraphNavigationEvent), track_id,
        SourceId()));
    return true;
}

// Expands and scrolls to a track in the topology sidebar.
bool
OptiqActions::RevealTrackInTopology(uint64_t track_id)
{
    if(m_data_provider == nullptr)
    {
        return false;
    }
    EventManager::GetInstance()->AddEvent(std::make_shared<ScrollToTrackEvent>(
        static_cast<int>(RocEvents::kRevealTrackInTopology), track_id, SourceId()));
    return true;
}

// The literal equivalent of clicking an event, which is what loads its details,
// flow arrows, and call stack.
bool
OptiqActions::ClickEvent(uint64_t track_id, uint64_t event_uuid)
{
    if(!HasTimeline() || event_uuid == TimelineSelection::INVALID_SELECTION_ID)
    {
        return false;
    }
    m_timeline_selection->UnselectAllEvents();
    m_timeline_selection->SelectTrackEvent(track_id, event_uuid);
    return true;
}

// Adds an event to the selection, the same as clicking with multi-select held.
bool
OptiqActions::ShiftClickEvent(uint64_t track_id, uint64_t event_uuid)
{
    if(!HasTimeline() || event_uuid == TimelineSelection::INVALID_SELECTION_ID)
    {
        return false;
    }
    m_timeline_selection->SelectTrackEvent(track_id, event_uuid);
    return true;
}

// Deselects every event.
bool
OptiqActions::ClearEventSelection()
{
    if(!HasTimeline())
    {
        return false;
    }
    m_timeline_selection->UnselectAllEvents();
    return true;
}

// Makes an event glow until something clears it.
bool
OptiqActions::HighlightEvent(uint64_t track_id, uint64_t event_uuid)
{
    if(!HasTimeline() || event_uuid == TimelineSelection::INVALID_SELECTION_ID)
    {
        return false;
    }
    m_timeline_selection->HighlightTrackEventPersistent(track_id, event_uuid);
    return true;
}

// Turns off every persistent highlight.
bool
OptiqActions::ClearHighlights()
{
    if(!HasTimeline())
    {
        return false;
    }
    m_timeline_selection->UnhighlightPersistentEvents();
    return true;
}

// Scrolls to an event and frames it, the same as jumping from a table row.
bool
OptiqActions::NavigateToEvent(uint64_t track_id, uint64_t event_uuid, double start_ns,
                              double duration_ns)
{
    // A zero-length event is real - an instantaneous marker - so only the sign
    // and finiteness are checked here, not that the duration is positive.
    if(!HasTimeline() || !IsUsableInstant(start_ns) ||
       !IsUsableInstant(duration_ns) || duration_ns < 0.0)
    {
        return false;
    }
    m_timeline_selection->NavigateToEvent(track_id, event_uuid, start_ns, duration_ns);
    return true;
}

// Selects a kernel in the compute views.
bool
OptiqActions::SelectKernel(uint32_t kernel_id)
{
    if(!HasCompute())
    {
        return false;
    }
    m_compute_selection->SelectKernel(kernel_id);
    return true;
}

// Selects a workload in the compute views.
bool
OptiqActions::SelectWorkload(uint32_t workload_id)
{
    if(!HasCompute())
    {
        return false;
    }
    m_compute_selection->SelectWorkload(workload_id);
    return true;
}

}  // namespace View
}  // namespace RocProfVis
