// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_ai_actions.h"

#include <cctype>
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
#include "widgets/rocprofvis_tab_container.h"

namespace RocProfVis
{
namespace View
{

OptiqActions::OptiqActions(DataProvider*      data_provider,
                           TimelineSelection* timeline_selection,
                           ComputeSelection*  compute_selection, TraceView* trace_view)
: m_data_provider(data_provider)
, m_timeline_selection(timeline_selection)
, m_compute_selection(compute_selection)
, m_trace_view(trace_view)
{}

namespace
{

std::string
ToLower(const std::string& value)
{
    std::string lowered;
    lowered.reserve(value.size());
    for(char c : value)
    {
        lowered += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return lowered;
}

}  // namespace

OptiqPanel
OptiqActions::PanelFromName(const std::string& name)
{
    const std::string key = ToLower(name);

    if(key == "minimap" || key == "mini-map" || key == "map" || key == "overview")
    {
        return OptiqPanel::kMinimap;
    }
    if(key == "histogram" || key == "activity" || key == "activity bar")
    {
        return OptiqPanel::kHistogram;
    }
    // The topology panel is the tall tree on the left, which users call the
    // navbar, the sidebar, or just "the tracks".
    if(key == "topology" || key == "sidebar" || key == "tracks" || key == "navbar" ||
       key == "nav" || key == "navigation" || key == "tree" || key == "left" ||
       key == "left panel" || key == "project")
    {
        return OptiqPanel::kTopology;
    }
    if(key == "details" || key == "advanced" || key == "tables" || key == "bottom" ||
       key == "analysis" || key == "event table")
    {
        return OptiqPanel::kDetails;
    }
    if(key == "summary" || key == "stats")
    {
        return OptiqPanel::kSummary;
    }
    if(key == "log" || key == "logs" || key == "logviewer" || key == "log viewer")
    {
        return OptiqPanel::kLogViewer;
    }
    if(key == "toolbar" || key == "tools" || key == "top bar")
    {
        return OptiqPanel::kToolbar;
    }
    if(key == "annotations" || key == "notes" || key == "sticky notes")
    {
        return OptiqPanel::kAnnotations;
    }
    return OptiqPanel::kUnknown;
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
    return "minimap, histogram, topology (the left navbar/tree), details, summary, "
           "log, toolbar, annotations";
}

bool
OptiqActions::ShowPanel(OptiqPanel panel, bool visible)
{
    AppWindowSettings& app_settings =
        SettingsManager::GetInstance().GetAppWindowSettings();

    switch(panel)
    {
        case OptiqPanel::kMinimap:
        {
            if(m_trace_view == nullptr)
            {
                return false;
            }
            m_trace_view->SetMinimapVisibility(visible);
            return true;
        }
        case OptiqPanel::kHistogram:
        {
            // The layout item is seeded from the setting at construction, so
            // both have to move for the change to stick and to be visible now.
            app_settings.show_histogram = visible;
            if(m_trace_view != nullptr)
            {
                m_trace_view->SetHistogramVisibility(visible);
            }
            return true;
        }
        case OptiqPanel::kTopology:
        {
            app_settings.show_sidebar = visible;
            if(m_trace_view != nullptr)
            {
                m_trace_view->SetSidebarViewVisibility(visible);
            }
            return true;
        }
        case OptiqPanel::kDetails:
        {
            app_settings.show_details_panel = visible;
            if(m_trace_view != nullptr)
            {
                m_trace_view->SetAnalysisViewVisibility(visible);
            }
            return true;
        }
        case OptiqPanel::kSummary:
        {
            // SummaryView re-reads this every frame, so the setting is enough.
            app_settings.show_summary = visible;
            return true;
        }
        case OptiqPanel::kToolbar:
        {
            app_settings.show_toolbar = visible;
            return true;
        }
        case OptiqPanel::kAnnotations:
        {
            if(m_trace_view == nullptr)
            {
                return false;
            }
            m_trace_view->SetAnnotationsVisible(visible);
            return true;
        }
        case OptiqPanel::kLogViewer:
        {
            bool* log_visible = LogViewer::GetInstance()->VisiblePtr();
            if(log_visible == nullptr)
            {
                return false;
            }
            *log_visible = visible;
            return true;
        }
        default: return false;
    }
}

bool
OptiqActions::IsPanelVisible(OptiqPanel panel, bool& visible_out) const
{
    const AppWindowSettings& app_settings =
        SettingsManager::GetInstance().GetAppWindowSettings();

    switch(panel)
    {
        case OptiqPanel::kMinimap:
        {
            if(m_trace_view == nullptr)
            {
                return false;
            }
            visible_out = m_trace_view->IsMinimapVisible();
            return true;
        }
        case OptiqPanel::kHistogram: visible_out = app_settings.show_histogram; return true;
        case OptiqPanel::kTopology:  visible_out = app_settings.show_sidebar; return true;
        case OptiqPanel::kDetails:
            visible_out = app_settings.show_details_panel;
            return true;
        case OptiqPanel::kSummary:   visible_out = app_settings.show_summary; return true;
        case OptiqPanel::kToolbar:   visible_out = app_settings.show_toolbar; return true;
        case OptiqPanel::kAnnotations:
        {
            if(m_trace_view == nullptr)
            {
                return false;
            }
            visible_out = m_trace_view->AreAnnotationsVisible();
            return true;
        }
        case OptiqPanel::kLogViewer:
        {
            const bool* log_visible = LogViewer::GetInstance()->VisiblePtr();
            if(log_visible == nullptr)
            {
                return false;
            }
            visible_out = *log_visible;
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

bool
OptiqActions::HasCompute() const
{
    return m_data_provider != nullptr && m_compute_selection != nullptr;
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

    // Match on the label the user sees, exact first then as a substring, so
    // "transpose" finds "rocpd-transpose.db".
    const std::string             needle    = ToLower(name);
    std::shared_ptr<TabContainer> container = app->GetTabContainer();
    const std::vector<const TabItem*> tabs  = container->GetTabs();
    for(const TabItem* tab : tabs)
    {
        if(tab != nullptr && ToLower(tab->m_label) == needle)
        {
            container->SetActiveTab(tab->m_id);
            return true;
        }
    }
    for(const TabItem* tab : tabs)
    {
        if(tab != nullptr && ToLower(tab->m_label).find(needle) != std::string::npos)
        {
            container->SetActiveTab(tab->m_id);
            return true;
        }
    }
    return false;
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
OptiqActions::AreFlowArrowsVisible(bool& visible_out) const
{
    if(m_trace_view == nullptr)
    {
        return false;
    }
    visible_out = m_trace_view->AreFlowArrowsVisible();
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
    return m_trace_view != nullptr && m_trace_view->MeasureRange(start_ns, end_ns);
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
    if(m_trace_view == nullptr || end_ns <= start_ns)
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
    if(m_trace_view == nullptr)
    {
        return false;
    }
    return m_trace_view->AddNote(time_ns, title, text, v_min, v_max, track_id);
}

void
OptiqActions::Notify(const std::string& message, bool is_warning)
{
    NotificationManager::GetInstance().Show(
        message, is_warning ? NotificationLevel::Warning : NotificationLevel::Info);
}

bool
OptiqActions::SelectRange(double start_ns, double end_ns)
{
    if(!HasTimeline() || end_ns <= start_ns)
    {
        return false;
    }
    m_timeline_selection->SelectTimeRange(start_ns, end_ns);
    return true;
}

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

bool
OptiqActions::ShowRange(double start_ns, double end_ns)
{
    if(m_data_provider == nullptr || end_ns <= start_ns)
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
OptiqActions::ShiftClickEvent(uint64_t track_id, uint64_t event_uuid)
{
    if(!HasTimeline() || event_uuid == TimelineSelection::INVALID_SELECTION_ID)
    {
        return false;
    }
    m_timeline_selection->SelectTrackEvent(track_id, event_uuid);
    return true;
}

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
    if(!HasTimeline())
    {
        return false;
    }
    m_timeline_selection->NavigateToEvent(track_id, event_uuid, start_ns, duration_ns);
    return true;
}

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
