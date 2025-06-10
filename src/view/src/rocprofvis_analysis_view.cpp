// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_analysis_view.h"
#include "rocprofvis_controller_enums.h"
#include "rocprofvis_data_provider.h"
#include "rocprofvis_events_view.h"
#include "spdlog/spdlog.h"

#include "widgets/rocprofvis_debug_window.h"

using namespace RocProfVis::View;

AnalysisView::AnalysisView(DataProvider& dp)
: m_data_provider(dp)
, m_event_table(std::make_shared<InfiniteScrollTable>(dp, TableType::kEventTable))
, m_sample_table(std::make_shared<InfiniteScrollTable>(dp, TableType::kSampleTable))
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

    m_events_view = std::make_shared<EventsView>(m_data_provider);

    // Add EventsView tab
    tab_item.m_label     = "Events View";
    tab_item.m_id        = "events_view";
    tab_item.m_can_close = false;
    tab_item.m_widget    = m_events_view;
    m_tab_container->AddTab(tab_item);

    m_tab_container->SetAllowToolTips(false);
    m_tab_container->SetActiveTab(0);

    auto time_line_selection_changed_handler = [this](std::shared_ptr<RocEvent> e) {
        this->HandleTimelineSelectionChanged(e);
    };
    // Subscribe to timeline selection changed event
    m_time_line_selection_changed_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kTimelineSelectionChanged),
        time_line_selection_changed_handler);
}

AnalysisView::~AnalysisView()
{
    // Unsubscribe from the timeline selection changed event
    EventManager::GetInstance()->Unsubscribe(
        static_cast<int>(RocEvents::kTimelineSelectionChanged),
        m_time_line_selection_changed_token);
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
    if(e && e->GetType() == RocEventType::kTimelineSelectionChangedEvent)
    {
        std::shared_ptr<TrackSelectionChangedEvent> selection_changed_event =
            std::static_pointer_cast<TrackSelectionChangedEvent>(e);
        if(selection_changed_event)
        {
            if(m_event_table)
            {
                m_event_table->HandleTrackSelectionChanged(selection_changed_event);
            }
            if(m_sample_table)
            {
                m_sample_table->HandleTrackSelectionChanged(selection_changed_event);
            }
        }
    }
}