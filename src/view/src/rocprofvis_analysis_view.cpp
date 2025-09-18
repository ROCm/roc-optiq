// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_analysis_view.h"
#include "rocprofvis_controller_enums.h"
#include "rocprofvis_data_provider.h"
#include "rocprofvis_events_view.h"
#include "rocprofvis_track_details.h"
#include "spdlog/spdlog.h"

#include "widgets/rocprofvis_debug_window.h"
#include "widgets/rocprofvis_infinite_scroll_table.h"

namespace RocProfVis
{
namespace View
{

AnalysisView::AnalysisView(DataProvider& dp, std::shared_ptr<TrackTopology> topology,
                           std::shared_ptr<TimelineSelection> timeline_selection)
: m_data_provider(dp)
, m_event_table(std::make_shared<InfiniteScrollTable>(dp, timeline_selection,
                                                      TableType::kEventTable))
, m_sample_table(std::make_shared<InfiniteScrollTable>(dp, timeline_selection,
                                                       TableType::kSampleTable))
, m_events_view(std::make_shared<EventsView>(dp, timeline_selection))
, m_track_details(std::make_shared<TrackDetails>(dp, topology, timeline_selection))
{
    m_widget_name = GenUniqueName("Analysis View");

    m_tab_container = std::make_shared<TabContainer>();

    TabItem tab_item;
    tab_item.m_label     = "Event Details";
    tab_item.m_id        = "event_details";
    tab_item.m_can_close = false;
    tab_item.m_widget    = m_event_table;
    m_tab_container->AddTab(tab_item);

    tab_item.m_label     = "Sample Details";
    tab_item.m_id        = "sample_details";
    tab_item.m_can_close = false;
    tab_item.m_widget    = m_sample_table;
    m_tab_container->AddTab(tab_item);

    // Add EventsView tab
    tab_item.m_label     = "Events View";
    tab_item.m_id        = "events_view";
    tab_item.m_can_close = false;
    tab_item.m_widget    = m_events_view;
    m_tab_container->AddTab(tab_item);

    tab_item.m_label     = "Track Details";
    tab_item.m_id        = "track_details";
    tab_item.m_can_close = false;
    tab_item.m_widget    = m_track_details;
    m_tab_container->AddTab(tab_item);

    m_tab_container->SetAllowToolTips(false);
    m_tab_container->SetActiveTab(0);

    auto time_line_selection_changed_handler = [this](std::shared_ptr<RocEvent> e) {
        this->HandleTimelineSelectionChanged(e);
    };
    auto new_table_data_handler = [this](std::shared_ptr<RocEvent> e) {
        this->HandleNewTableData(e);
    };

    // Subscribe to timeline selection changed event
    m_timeline_track_selection_changed_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kTimelineTrackSelectionChanged),
        time_line_selection_changed_handler);
    m_timeline_event_selection_changed_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kTimelineEventSelectionChanged),
        time_line_selection_changed_handler);
    m_new_table_data_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kNewTableData), new_table_data_handler);
}

AnalysisView::~AnalysisView()
{
    // Unsubscribe from the timeline timeline_selection changed event
    EventManager::GetInstance()->Unsubscribe(
        static_cast<int>(RocEvents::kTimelineTrackSelectionChanged),
        m_timeline_track_selection_changed_token);
    EventManager::GetInstance()->Unsubscribe(
        static_cast<int>(RocEvents::kTimelineEventSelectionChanged),
        m_timeline_event_selection_changed_token);
    EventManager::GetInstance()->Unsubscribe(static_cast<int>(RocEvents::kNewTableData),
                                             m_new_table_data_token);
}

void
AnalysisView::Update()
{
    m_tab_container->Update();
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
                if(m_event_table)
                {
                    m_event_table->HandleTrackSelectionChanged();
                }
                if(m_sample_table)
                {
                    m_sample_table->HandleTrackSelectionChanged();
                }
                if(m_track_details)
                {
                    m_track_details->HandleTrackSelectionChanged();
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
                    m_events_view->HandleEventSelectionChanged();
                }
            }
        }
    }
}

void
AnalysisView::HandleNewTableData(std::shared_ptr<RocEvent> e)
{
    if(e)
    {
        RocEventType                    event_type = e->GetType();
        std::shared_ptr<TableDataEvent> table_data_event =
            std::static_pointer_cast<TableDataEvent>(e);
        if(table_data_event &&
           table_data_event->GetSourceId() == m_data_provider.GetTraceFilePath())
        {
            const uint64_t& request_id = table_data_event->GetRequestID();
            if(request_id == DataProvider::EVENT_TABLE_REQUEST_ID && m_event_table)
            {
                m_event_table->HandleNewTableData(table_data_event);
            }
            else if(request_id == DataProvider::SAMPLE_TABLE_REQUEST_ID && m_sample_table)
            {
                m_sample_table->HandleNewTableData(table_data_event);
            }
        }
    }
}

}  // namespace View
}  // namespace RocProfVis