// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_analysis_view.h"
#include "rocprofvis_annotation_view.h"
#include "rocprofvis_annotations.h"
#include "rocprofvis_compare_panes.h"
#include "rocprofvis_data_provider.h"
#include "rocprofvis_events_view.h"
#include "rocprofvis_multi_track_table.h"
#include "rocprofvis_settings_manager.h"
#include "rocprofvis_top_events_view.h"
#include "rocprofvis_track_details.h"
#include "widgets/rocprofvis_gui_helpers.h"
#include "widgets/rocprofvis_split_containers.h"

namespace RocProfVis
{
namespace View
{

constexpr uint64_t TABLE_DEFAULT_SORT_COLUMN = 1;
// Middle dot between the row and track counts of a card summary.
constexpr const char* SUMMARY_SEPARATOR = " \xC2\xB7 ";

AnalysisView::AnalysisView(DataProvider& dp, std::shared_ptr<TrackTopology> topology,
                           std::shared_ptr<TimelineSelection>  timeline_selection,
                           std::shared_ptr<AnnotationsManager> annotation_manager)
: m_data_provider(dp)
, m_compare_mode(false)
, m_events_view(std::make_shared<EventsView>(dp, timeline_selection))
, m_track_details(std::make_shared<TrackDetails>(dp, topology, timeline_selection))
, m_annotation_view(std::make_shared<AnnotationView>(dp, annotation_manager))
, m_top_events_view(std::make_shared<TopEventsView>(dp, timeline_selection))
{
    m_widget_name = GenUniqueName("Analysis View");

    m_compare_mode = IsCompareTrace(dp.DataModel());

    if(m_compare_mode)
    {
        BuildCompareGroup(
            m_event_group,
            { TableType::kCompareEventTableA, TableType::kCompareEventTableB },
            kRPVControllerTableTypeEvents, RequestType::kFetchTrackEventTable,
            "Event Table", "events", "compare_event_tab", timeline_selection);
        BuildCompareGroup(
            m_sample_group,
            { TableType::kCompareSampleTableA, TableType::kCompareSampleTableB },
            kRPVControllerTableTypeSamples, RequestType::kFetchTrackSampleTable,
            "Sample Table", "samples", "compare_sample_tab", timeline_selection);
    }
    else
    {
        m_event_group.tables.push_back(std::make_shared<MultiTrackTable>(
            dp, TableType::kEventTable, kRPVControllerTableTypeEvents,
            DataProvider::EVENT_TABLE_REQUEST_ID,
            [&dp]() -> const TablesModel& { return dp.DataModel().GetTables(); },
            [&dp]() -> TablesModel& { return dp.DataModel().GetTables(); },
            timeline_selection,
            MultiTrackTable::FilterMode::kBasic | MultiTrackTable::FilterMode::kAdvanced));
        m_sample_group.tables.push_back(std::make_shared<MultiTrackTable>(
            dp, TableType::kSampleTable, kRPVControllerTableTypeSamples,
            DataProvider::SAMPLE_TABLE_REQUEST_ID,
            [&dp]() -> const TablesModel& { return dp.DataModel().GetTables(); },
            [&dp]() -> TablesModel& { return dp.DataModel().GetTables(); },
            timeline_selection,
            MultiTrackTable::FilterMode::kBasic | MultiTrackTable::FilterMode::kAdvanced));
    }

    m_tab_container = std::make_shared<TabContainer>();

    TabItem tab_item;
    tab_item.m_label     = "Event Table";
    tab_item.m_id        = "event_table";
    tab_item.m_can_close = false;
    tab_item.m_widget    = TabWidgetFor(m_event_group);
    m_tab_container->AddTab(tab_item);

    tab_item.m_label     = "Sample Table";
    tab_item.m_id        = "sample_table";
    tab_item.m_can_close = false;
    tab_item.m_widget    = TabWidgetFor(m_sample_group);
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
    // Compare tables live in custom layouts, not as tab widgets, so update them here.
    if(m_compare_mode)
    {
        for(std::shared_ptr<MultiTrackTable>& table : m_event_group.tables)
        {
            table->Update();
        }
        for(std::shared_ptr<MultiTrackTable>& table : m_sample_group.tables)
        {
            table->Update();
        }
    }
    m_tab_container->Update();
}

void
AnalysisView::Render()
{
    m_tab_container->Render();
}

std::shared_ptr<RocWidget>
AnalysisView::TabWidgetFor(CompareGroup& group) const
{
    if(m_compare_mode && group.layout)
    {
        return group.layout;
    }
    return group.tables.front();
}

void
AnalysisView::BuildCompareGroup(CompareGroup&                                      group,
                                const std::array<TableType, COMPARE_SOURCE_COUNT>& types,
                                rocprofvis_controller_table_type_t request_table_type,
                                RequestType request_type, const char* friendly_name,
                                const char* noun, const char* child_id,
                                std::shared_ptr<TimelineSelection> timeline_selection)
{
    DataProvider& dp = m_data_provider;
    group.noun       = noun;

    for(size_t source = 0; source < COMPARE_SOURCE_COUNT; source++)
    {
        group.tables.push_back(std::make_shared<MultiTrackTable>(
            dp, types[source], request_table_type,
            RequestIdBuilder::MakeClientRequestId(request_type,
                                                  COMPARE_CLIENT_ID[source]),
            [&dp]() -> const TablesModel& { return dp.DataModel().GetTables(); },
            [&dp]() -> TablesModel& { return dp.DataModel().GetTables(); },
            timeline_selection, MultiTrackTable::FilterMode::kNone,
            TABLE_DEFAULT_SORT_COLUMN, kRPVControllerSortOrderAscending,
            std::string(friendly_name) + " " + COMPARE_SOURCE_LABEL[source], "", source));
        // The card title carries the counts, and the filter form is shared.
        group.tables[source]->SetDisplaySummary(false);
        group.tables[source]->SetHeaderRenderer(
            [this, &group, source]() { RenderCompareSourceTitle(group, source); });
    }

    group.tables[COMPARE_SOURCE_A]->SetFilterSubmitCallback(
        [&group](const MultiTrackTable& source) {
            group.tables[COMPARE_SOURCE_B]->ApplySharedFiltersFrom(source);
        });

    // Either header can drive the sort, so sync both ways.
    group.tables[COMPARE_SOURCE_A]->SetSortSyncCallback(
        [&group](const MultiTrackTable& source) {
            group.tables[COMPARE_SOURCE_B]->ApplySharedSortFrom(source);
        });
    group.tables[COMPARE_SOURCE_B]->SetSortSyncCallback(
        [&group](const MultiTrackTable& source) {
            group.tables[COMPARE_SOURCE_A]->ApplySharedSortFrom(source);
        });

    group.split =
        MakeCompareSplit(group.tables[COMPARE_SOURCE_A], group.tables[COMPARE_SOURCE_B]);

    const std::string tab_id = child_id;
    group.layout             = std::make_shared<RocCustomWidget>(
        [this, &group, tab_id]() { RenderCompareTab(group, tab_id.c_str()); });
}

void
AnalysisView::RenderCompareTab(CompareGroup& group, const char* child_id)
{
    const ImGuiStyle& style = SettingsManager::GetInstance().GetDefaultStyle();
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, style.WindowPadding);
    ImGui::PushID(child_id);
    ImGui::BeginChild("##compare_tab", ImVec2(0.0f, 0.0f),
                      ImGuiChildFlags_AlwaysUseWindowPadding,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    // One filter form above the split drives both tables. Group-by choices are
    // the union of each source's last ungrouped header; Apply still sends a
    // column only if that source has it.
    std::vector<std::string> group_by_names;
    std::vector<std::string> group_by_labels;
    BuildCompareGroupByChoices(group.tables[COMPARE_SOURCE_A]->EligibleGroupByColumns(),
                               group.tables[COMPARE_SOURCE_B]->EligibleGroupByColumns(),
                               group_by_names, group_by_labels);
    group.tables[COMPARE_SOURCE_A]->RenderSharedFilterControls(group_by_names,
                                                               group_by_labels);

    ImGui::Separator();
    ImGui::Spacing();

    group.split->Render();

    ImGui::EndChild();
    ImGui::PopID();
    ImGui::PopStyleVar();
}

void
AnalysisView::RenderCompareSourceTitle(const CompareGroup& group, size_t source_index)
{
    const CompareSourceInfo* source =
        m_data_provider.DataModel().GetCompareSource(source_index);
    if(!source)
    {
        return;
    }

    const MultiTrackTable& table   = *group.tables[source_index];
    const std::string      summary = std::to_string(table.GetTotalRowCount()) + " " +
                                group.noun + SUMMARY_SEPARATOR +
                                std::to_string(table.GetIncludedTrackCount()) + " tracks";
    RenderCompareCardTitle(*source, SettingsManager::GetInstance(), summary);
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
                for(std::shared_ptr<MultiTrackTable>& table : m_event_group.tables)
                {
                    table->HandleTrackSelectionChanged(
                        selection_changed_event->GetTrackID(),
                        selection_changed_event->TrackSelected());
                }
                for(std::shared_ptr<MultiTrackTable>& table : m_sample_group.tables)
                {
                    table->HandleTrackSelectionChanged(
                        selection_changed_event->GetTrackID(),
                        selection_changed_event->TrackSelected());
                }
                if(m_track_details)
                {
                    m_track_details->HandleTrackSelectionChanged(
                        selection_changed_event->GetTrackID(),
                        selection_changed_event->TrackSelected());
                }
                if(m_top_events_view)
                {
                    m_top_events_view->HandleTrackSelectionChanged(
                        selection_changed_event->GetTrackID(),
                        selection_changed_event->TrackSelected());
                }
            }
        }
        else if(event_type == RocEventType::kTimelineTimeRangeChangedEvent)
        {
            std::shared_ptr<TimeRangeSelectionChangedEvent> selection_changed_event =
                std::static_pointer_cast<TimeRangeSelectionChangedEvent>(e);
            if(selection_changed_event)
            {
                for(std::shared_ptr<MultiTrackTable>& table : m_event_group.tables)
                {
                    table->HandleTimeRangeSelectionChanged(
                        selection_changed_event->GetStartNs(),
                        selection_changed_event->GetEndNs());
                }
                for(std::shared_ptr<MultiTrackTable>& table : m_sample_group.tables)
                {
                    table->HandleTimeRangeSelectionChanged(
                        selection_changed_event->GetStartNs(),
                        selection_changed_event->GetEndNs());
                }
                if(m_top_events_view)
                {
                    m_top_events_view->HandleTimeRangeSelectionChanged(
                        selection_changed_event->GetStartNs(),
                        selection_changed_event->GetEndNs());
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