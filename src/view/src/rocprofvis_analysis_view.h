// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once
#include "rocprofvis_event_manager.h"
#include "widgets/rocprofvis_widget.h"

namespace RocProfVis
{
namespace View
{

class DataProvider;
class EventsView;
class InfiniteScrollTable;
class TrackTopology;
class TrackDetails;

class AnalysisView : public RocWidget
{
public:
    AnalysisView(DataProvider& dp, std::shared_ptr<TrackTopology> topology);
    ~AnalysisView();
    void Render() override;
    void Update() override;

private:
    void HandleTimelineSelectionChanged(std::shared_ptr<RocEvent> e);

    DataProvider& m_data_provider;

    std::shared_ptr<InfiniteScrollTable> m_event_table;
    std::shared_ptr<InfiniteScrollTable> m_sample_table;

    std::shared_ptr<TabContainer>   m_tab_container;
    EventManager::SubscriptionToken m_time_line_selection_changed_token;
    std::shared_ptr<EventsView>     m_events_view;
    std::shared_ptr<TrackDetails>   m_track_details;
};

}  // namespace View
}  // namespace RocProfVis
