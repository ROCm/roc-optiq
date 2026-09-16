// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_ai_actions.h"

#include <cctype>
#include <cmath>
#include <memory>

#include "rocprofvis_appwindow.h"
#include "rocprofvis_data_provider.h"
#include "rocprofvis_event_manager.h"
#include "rocprofvis_events.h"
#include "rocprofvis_settings_manager.h"
#include "rocprofvis_timeline_selection.h"
#include "rocprofvis_trace_view.h"
#include "rocprofvis_utils.h"
#include "widgets/rocprofvis_log_viewer.h"
#include "widgets/rocprofvis_notification_manager.h"
#ifdef ROCPROFVIS_ENABLE_SCRIPTING
#    include "widgets/rocprofvis_script_editor.h"
#endif

namespace RocProfVis
{
namespace View
{

namespace
{

// True when a single point in time is one we can act on. Same reasoning as
// is_usable_time_range, for the arguments that are an instant rather than a
// span: every comparison against NaN is false, and these numbers come from the
// model.
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

// The minimap and annotations are drawn by a TraceView widget and persisted
// nowhere, so they cannot be touched without the active trace. Every other
// panel is backed by AppWindowSettings and follows the View menu, updating
// every open system trace even when a compute tab is in front.
bool
NeedsActiveTraceView(OptiqPanel panel)
{
    return panel == OptiqPanel::kMinimap || panel == OptiqPanel::kAnnotations;
}

}  // namespace

OptiqActions::OptiqActions(DataProvider*      data_provider,
                           TimelineSelection* timeline_selection,
                           TraceView*         trace_view)
: m_data_provider(data_provider)
, m_timeline_selection(timeline_selection)
, m_trace_view(trace_view)
{}

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

bool
OptiqActions::HasTimeline() const
{
    return m_data_provider != nullptr && m_timeline_selection != nullptr;
}

std::string
OptiqActions::SourceId() const
{
    return m_data_provider != nullptr ? m_data_provider->GetTraceFilePath()
                                      : std::string();
}

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

std::vector<std::string>
OptiqActions::ListAnalysisTabs() const
{
    return m_trace_view != nullptr ? m_trace_view->ListAnalysisTabs()
                                   : std::vector<std::string>();
}

std::string
OptiqActions::ActiveAnalysisTab() const
{
    return m_trace_view != nullptr ? m_trace_view->ActiveAnalysisTab() : std::string();
}

bool
OptiqActions::SelectAnalysisTab(const std::string& name)
{
    return m_trace_view != nullptr && m_trace_view->SelectAnalysisTab(name);
}

#ifdef ROCPROFVIS_ENABLE_SCRIPTING
// Fills the Script tab and brings the user to it. Selecting the tab is part of
// offering: a question the user cannot see is one the assistant would wait on
// forever.
bool
OptiqActions::ProposeScript(const std::string& source)
{
    if(m_trace_view == nullptr || !m_trace_view->ProposeScript(source))
    {
        return false;
    }
    m_trace_view->SelectAnalysisTab("Script");
    return true;
}

ScriptApproval
OptiqActions::ScriptProposalState() const
{
    return m_trace_view != nullptr ? m_trace_view->ScriptProposalState()
                                   : ScriptApproval::kNone;
}

void
OptiqActions::ClearScriptProposal()
{
    if(m_trace_view != nullptr)
    {
        m_trace_view->ClearScriptProposal();
    }
}
#endif

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

std::vector<int>
OptiqActions::ListBookmarks() const
{
    return m_trace_view != nullptr ? m_trace_view->ListBookmarks() : std::vector<int>();
}

bool
OptiqActions::SaveBookmark(int slot)
{
    return m_trace_view != nullptr && m_trace_view->SaveBookmark(slot);
}

bool
OptiqActions::GotoBookmark(int slot)
{
    return m_trace_view != nullptr && m_trace_view->GotoBookmark(slot);
}

bool
OptiqActions::RemoveBookmark(int slot)
{
    return m_trace_view != nullptr && m_trace_view->RemoveBookmark(slot);
}

bool
OptiqActions::MeasureRange(double start_ns, double end_ns)
{
    if(m_trace_view == nullptr || !is_usable_time_range(start_ns, end_ns))
    {
        return false;
    }
    return m_trace_view->MeasureRange(start_ns, end_ns);
}

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

bool
OptiqActions::ZoomToRange(double start_ns, double end_ns)
{
    if(m_trace_view == nullptr || !is_usable_time_range(start_ns, end_ns))
    {
        return false;
    }
    m_trace_view->ZoomToRange(start_ns, end_ns);
    return true;
}

bool
OptiqActions::AddNote(double time_ns, const std::string& title, const std::string& text,
                      double v_min, double v_max, uint64_t track_id)
{
    if(m_trace_view == nullptr || !IsUsableInstant(time_ns) ||
       !is_usable_time_range(v_min, v_max))
    {
        return false;
    }
    return m_trace_view->AddNote(time_ns, title, text, v_min, v_max, track_id);
}

bool
OptiqActions::SelectRange(double start_ns, double end_ns)
{
    if(!HasTimeline() || !is_usable_time_range(start_ns, end_ns))
    {
        return false;
    }
    m_timeline_selection->SelectTimeRange(start_ns, end_ns);
    return true;
}

bool
OptiqActions::ShowRange(double start_ns, double end_ns)
{
    // The timeline is what listens for this event, so require it rather than
    // just a data provider: a compute trace has the latter but not the former.
    if(!HasTimeline() || !is_usable_time_range(start_ns, end_ns))
    {
        return false;
    }
    EventManager::GetInstance()->AddEvent(std::make_shared<RangeEvent>(
        static_cast<int>(RocEvents::kSetViewRange), start_ns, end_ns, SourceId()));
    return true;
}

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

}  // namespace View
}  // namespace RocProfVis
