// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_analysis_view.h"
#include "rocprofvis_annotation_view.h"
#include "rocprofvis_annotations.h"
#include "rocprofvis_data_provider.h"
#include "rocprofvis_events_view.h"
#include "rocprofvis_multi_track_table.h"
#include "rocprofvis_timeline_selection.h"
#include "rocprofvis_top_events_view.h"
#include "rocprofvis_track_details.h"

namespace RocProfVis
{
namespace View
{

AnalysisView::AnalysisView(DataProvider& dp, std::shared_ptr<TrackTopology> topology,
                           std::shared_ptr<TimelineSelection>  timeline_selection,
                           std::shared_ptr<AnnotationsManager> annotation_manager)
: m_data_provider(dp)
, m_event_table(std::make_shared<MultiTrackTable>(
      dp, TableType::kEventTable, kRPVControllerTableTypeEvents,
      DataProvider::EVENT_TABLE_REQUEST_ID,
      [&dp]() -> const TablesModel& { return dp.DataModel().GetTables(); },
      [&dp]() -> TablesModel& { return dp.DataModel().GetTables(); }, true,
      timeline_selection))
, m_sample_table(std::make_shared<MultiTrackTable>(
      dp, TableType::kSampleTable, kRPVControllerTableTypeSamples,
      DataProvider::SAMPLE_TABLE_REQUEST_ID,
      [&dp]() -> const TablesModel& { return dp.DataModel().GetTables(); },
      [&dp]() -> TablesModel& { return dp.DataModel().GetTables(); }, true,
      timeline_selection))
, m_events_view(std::make_shared<EventsView>(dp, timeline_selection))
, m_annotation_view(std::make_shared<AnnotationView>(dp, annotation_manager))
, m_track_details(std::make_shared<TrackDetails>(dp, topology, timeline_selection))
, m_top_events_view(std::make_shared<TopEventsView>(dp, timeline_selection))
{
    m_widget_name = GenUniqueName("Analysis View");

    m_tab_container = std::make_shared<TabContainer>();

    TabItem tab_item;
    tab_item.m_label     = "Event Table";
    tab_item.m_id        = "event_table";
    tab_item.m_can_close = false;
    tab_item.m_widget    = m_event_table;
    m_tab_container->AddTab(tab_item);

    tab_item.m_label     = "Sample Table";
    tab_item.m_id        = "sample_table";
    tab_item.m_can_close = false;
    tab_item.m_widget    = m_sample_table;
    m_tab_container->AddTab(tab_item);

    // Add EventsView tab
    tab_item.m_label     = "Event Details";
    tab_item.m_id        = "event_details";
    tab_item.m_can_close = false;
    tab_item.m_widget    = m_events_view;
    m_tab_container->AddTab(tab_item);

    tab_item.m_label     = "Track Details";
    tab_item.m_id        = "track_details";
    tab_item.m_can_close = false;
    tab_item.m_widget    = m_track_details;
    m_tab_container->AddTab(tab_item);

    tab_item.m_label     = "Top Events";
    tab_item.m_id        = "top_events";
    tab_item.m_can_close = false;
    tab_item.m_widget    = m_top_events_view;
    m_tab_container->AddTab(tab_item);

    // Add Annotation View Tab
    tab_item.m_label     = "Annotations";
    tab_item.m_id        = "annotation_view";
    tab_item.m_can_close = false;
    tab_item.m_widget    = m_annotation_view;
    m_tab_container->AddTab(tab_item);

    m_tab_container->SetAllowToolTips(false);
    m_tab_container->SetActiveTab(0);

    auto time_line_selection_changed_handler = [this](std::shared_ptr<RocEvent> e) {
        this->HandleTimelineSelectionChanged(e);
    };

    // Subscribe to timeline selection changed event
    m_timeline_track_selection_changed_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kTimelineTrackSelectionChanged),
        time_line_selection_changed_handler);
    m_timeline_range_selection_changed_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kTimelineTimeRangeChanged),
        time_line_selection_changed_handler);
    m_timeline_event_selection_changed_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kTimelineEventSelectionChanged),
        time_line_selection_changed_handler);
}

AnalysisView::~AnalysisView()
{
    // Unsubscribe from the timeline timeline_selection changed event
    EventManager::GetInstance()->Unsubscribe(
        static_cast<int>(RocEvents::kTimelineTrackSelectionChanged),
        m_timeline_track_selection_changed_token);
    EventManager::GetInstance()->Unsubscribe(
        static_cast<int>(RocEvents::kTimelineTimeRangeChanged),
        m_timeline_range_selection_changed_token);
    EventManager::GetInstance()->Unsubscribe(
        static_cast<int>(RocEvents::kTimelineEventSelectionChanged),
        m_timeline_event_selection_changed_token);
}

void
AnalysisView::Update()
{
    m_tab_container->Update();

    // If a table tab that missed selection changes while hidden has just become active, catch
    // it up now (a single refetch of the current selection) so it shows current data without
    // every table having fetched on every earlier selection change.
    const TabItem*   active        = m_tab_container->GetActiveTab();
    const RocWidget* active_widget = active ? active->m_widget.get() : nullptr;
    if(active_widget != m_last_active_tab_widget)
    {
        m_last_active_tab_widget = active_widget;
        // Compare against the active widget we already have instead of re-querying the tab
        // container per table. The INVALID id triggers a refetch of the live selection + range.
        auto is_active = [active_widget](const RocWidget* widget) {
            return widget != nullptr && widget == active_widget;
        };
        if(m_event_table_needs_refresh && is_active(m_event_table.get()))
        {
            m_event_table_needs_refresh = false;
            m_event_table->HandleTrackSelectionChanged(
                TimelineSelection::INVALID_SELECTION_ID, false);
        }
        if(m_sample_table_needs_refresh && is_active(m_sample_table.get()))
        {
            m_sample_table_needs_refresh = false;
            m_sample_table->HandleTrackSelectionChanged(
                TimelineSelection::INVALID_SELECTION_ID, false);
        }
        if(m_top_events_needs_refresh && is_active(m_top_events_view.get()))
        {
            m_top_events_needs_refresh = false;
            m_top_events_view->HandleTrackSelectionChanged(
                TimelineSelection::INVALID_SELECTION_ID, false);
        }
    }
}

bool
AnalysisView::IsTabActive(const RocWidget* widget) const
{
    const TabItem* active = m_tab_container->GetActiveTab();
    return active != nullptr && widget != nullptr && active->m_widget.get() == widget;
}

void
AnalysisView::Render()
{
    m_tab_container->Render();
}

void
AnalysisView::HandleTimelineSelectionChanged(std::shared_ptr<RocEvent> e)
{
    if(e && e->GetSourceId() == m_data_provider.GetTraceFilePath())
    {
        RocEventType event_type = e->GetType();
        if(event_type == RocEventType::kTimelineTrackSelectionChangedEvent)
        {
            std::shared_ptr<TrackSelectionChangedEvent> selection_changed_event =
                std::static_pointer_cast<TrackSelectionChangedEvent>(e);
            if(selection_changed_event)
            {
                const uint64_t track_id = selection_changed_event->GetTrackID();
                const bool     selected = selection_changed_event->TrackSelected();

                // Only the visible table tab fetches now; hidden ones are flagged and catch up
                // when shown. This keeps a single track click from firing several slow merged
                // table queries at once.
                if(m_event_table)
                {
                    if(IsTabActive(m_event_table.get()))
                    {
                        m_event_table->HandleTrackSelectionChanged(track_id, selected);
                    }
                    else
                    {
                        m_event_table_needs_refresh = true;
                    }
                }
                if(m_sample_table)
                {
                    if(IsTabActive(m_sample_table.get()))
                    {
                        m_sample_table->HandleTrackSelectionChanged(track_id, selected);
                    }
                    else
                    {
                        m_sample_table_needs_refresh = true;
                    }
                }
                if(m_top_events_view)
                {
                    if(IsTabActive(m_top_events_view.get()))
                    {
                        m_top_events_view->HandleTrackSelectionChanged(track_id, selected);
                    }
                    else
                    {
                        m_top_events_needs_refresh = true;
                    }
                }
                // Track Details keeps a per-track list (cheap, no DB query) and relies on the
                // per-track add/remove deltas, so it is always kept in sync.
                if(m_track_details)
                {
                    m_track_details->HandleTrackSelectionChanged(track_id, selected);
                }
            }
        }
        else if(event_type == RocEventType::kTimelineTimeRangeChangedEvent)
        {
            std::shared_ptr<TimeRangeSelectionChangedEvent> selection_changed_event =
                std::static_pointer_cast<TimeRangeSelectionChangedEvent>(e);
            if(selection_changed_event)
            {
                const double start_ns = selection_changed_event->GetStartNs();
                const double end_ns   = selection_changed_event->GetEndNs();

                // Same visible-only policy as track selection: only the active table tab
                // refetches for the new range; hidden ones catch up when shown.
                if(m_event_table)
                {
                    if(IsTabActive(m_event_table.get()))
                    {
                        m_event_table->HandleTimeRangeSelectionChanged(start_ns, end_ns);
                    }
                    else
                    {
                        m_event_table_needs_refresh = true;
                    }
                }
                if(m_sample_table)
                {
                    if(IsTabActive(m_sample_table.get()))
                    {
                        m_sample_table->HandleTimeRangeSelectionChanged(start_ns, end_ns);
                    }
                    else
                    {
                        m_sample_table_needs_refresh = true;
                    }
                }
                if(m_top_events_view)
                {
                    if(IsTabActive(m_top_events_view.get()))
                    {
                        m_top_events_view->HandleTimeRangeSelectionChanged(start_ns, end_ns);
                    }
                    else
                    {
                        m_top_events_needs_refresh = true;
                    }
                }
            }
        }
        else if(event_type == RocEventType::kTimelineEventSelectionChangedEvent)
        {
            std::shared_ptr<EventSelectionChangedEvent> selection_changed_event =
                std::static_pointer_cast<EventSelectionChangedEvent>(e);
            if(selection_changed_event)
            {
                if(m_events_view)
                {
                    m_events_view->HandleEventSelectionChanged(
                        selection_changed_event->GetEventID(),
                        selection_changed_event->EventSelected());
                }
            }
        }
    }
}

}  // namespace View
}  // namespace RocProfVis