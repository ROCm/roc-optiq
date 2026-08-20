// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once
#include <array>
#include <cstddef>
#include <string>
#include <vector>

#include "model/rocprofvis_tables_model.h"
#include "rocprofvis_annotation_view.h"
#include "rocprofvis_annotations.h"
#include "rocprofvis_compare_panes.h"
#include "rocprofvis_controller_enums.h"
#include "rocprofvis_event_manager.h"
#include "rocprofvis_requests.h"
#include "widgets/rocprofvis_tab_container.h"

namespace RocProfVis
{
namespace View
{

class DataProvider;
class EventsView;
class MultiTrackTable;
class TopEventsView;
class TrackTopology;
class TrackDetails;
class TimelineSelection;
class HSplitContainer;

class AnalysisView : public RocWidget
{
public:
    AnalysisView(DataProvider& dp, std::shared_ptr<TrackTopology> topology,
                 std::shared_ptr<TimelineSelection>  timeline_selection,
                 std::shared_ptr<AnnotationsManager> annotation_manager);
    ~AnalysisView();
    void Render() override;
    void Update() override;

    friend struct AnalysisViewTestPeer;

private:
    // One pooled table, or an A/B pair sharing a filter, backing a tab.
    struct CompareGroup
    {
        std::vector<std::shared_ptr<MultiTrackTable>> tables;  // 1 pooled, or 2 (A/B)
        std::shared_ptr<HSplitContainer>              split;   // compare only
        std::shared_ptr<RocWidget>                    layout;  // compare only
        std::string                                   noun;    // "events" / "samples"
    };

    void HandleTimelineSelectionChanged(std::shared_ptr<RocEvent> e);
    // Builds the A/B tables of one tab, their shared filter and their layout.
    void BuildCompareGroup(CompareGroup&                                      group,
                           const std::array<TableType, COMPARE_SOURCE_COUNT>& types,
                           rocprofvis_controller_table_type_t request_table_type,
                           RequestType request_type, const char* friendly_name,
                           const char* noun, const char* child_id,
                           std::shared_ptr<TimelineSelection> timeline_selection);
    void RenderCompareTab(CompareGroup& group, const char* child_id);
    void RenderCompareSourceTitle(const CompareGroup& group, size_t source_index);

    std::shared_ptr<RocWidget> TabWidgetFor(CompareGroup& group) const;

    DataProvider& m_data_provider;

    bool         m_compare_mode;
    CompareGroup m_event_group;
    CompareGroup m_sample_group;

    std::shared_ptr<TabContainer>   m_tab_container;
    std::shared_ptr<EventsView>     m_events_view;
    std::shared_ptr<TrackDetails>   m_track_details;
    std::shared_ptr<AnnotationView> m_annotation_view;
    std::shared_ptr<TopEventsView>  m_top_events_view;

    EventManager::SubscriptionToken m_timeline_track_selection_changed_token;
    EventManager::SubscriptionToken m_timeline_range_selection_changed_token;
    EventManager::SubscriptionToken m_timeline_event_selection_changed_token;
};

}  // namespace View
}  // namespace RocProfVis
