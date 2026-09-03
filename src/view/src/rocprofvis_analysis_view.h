// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once
#include "rocprofvis_annotation_view.h"
#include "rocprofvis_annotations.h"
#include "rocprofvis_event_manager.h"
#include "widgets/rocprofvis_tab_container.h"

namespace RocProfVis
{
namespace View
{

class DataProvider;
class EventsView;
class MultiTrackTable;
class ScriptEditor;
class TopEventsView;
class TrackTopology;
class TrackDetails;
class TimelineSelection;

class AnalysisView : public RocWidget
{
public:
    AnalysisView(DataProvider& dp, std::shared_ptr<TrackTopology> topology,
                 std::shared_ptr<TimelineSelection>  timeline_selection,
                 std::shared_ptr<AnnotationsManager> annotation_manager);
    ~AnalysisView();
    void Render() override;
    void Update() override;

    // The tab strip along the bottom: Event Table, Sample Table, Event Details,
    // Track Details, Top Events, Annotations, and Script when scripting is
    // built in. Matched on the visible label.
    std::vector<std::string> ListTabs();
    bool                     SelectTab(const std::string& name);
    std::string              ActiveTab();

#ifdef ROCPROFVIS_ENABLE_SCRIPTING
    // The Script tab's editor, for the assistant to offer a script through.
    // Null only if the tab was never built.
    std::shared_ptr<ScriptEditor> GetScriptEditor() const;
#endif

    friend struct AnalysisViewTestPeer;

private:
    void HandleTimelineSelectionChanged(std::shared_ptr<RocEvent> e);

    DataProvider& m_data_provider;

    std::shared_ptr<MultiTrackTable> m_event_table;
    std::shared_ptr<MultiTrackTable> m_sample_table;

    std::shared_ptr<TabContainer>   m_tab_container;
    std::shared_ptr<EventsView>     m_events_view;
    std::shared_ptr<TrackDetails>   m_track_details;
    std::shared_ptr<AnnotationView> m_annotation_view;
    std::shared_ptr<TopEventsView>  m_top_events_view;
#ifdef ROCPROFVIS_ENABLE_SCRIPTING
    std::shared_ptr<ScriptEditor> m_script_editor;
#endif

    EventManager::SubscriptionToken m_timeline_track_selection_changed_token;
    EventManager::SubscriptionToken m_timeline_range_selection_changed_token;
    EventManager::SubscriptionToken m_timeline_event_selection_changed_token;
};

}  // namespace View
}  // namespace RocProfVis
