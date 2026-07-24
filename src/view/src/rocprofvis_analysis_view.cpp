// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_analysis_view.h"
#include "rocprofvis_annotation_view.h"
#include "rocprofvis_annotations.h"
#include "rocprofvis_data_provider.h"
#include "rocprofvis_events_view.h"
#include "rocprofvis_multi_track_table.h"
#include "rocprofvis_settings_manager.h"
#include "rocprofvis_top_events_view.h"
#include "rocprofvis_track_item.h"
#include "rocprofvis_track_details.h"
#include "widgets/rocprofvis_gui_helpers.h"
#include "widgets/rocprofvis_split_containers.h"

namespace RocProfVis
{
namespace View
{

constexpr size_t   COMPARE_SOURCE_A_INDEX          = 0;
constexpr size_t   COMPARE_SOURCE_B_INDEX          = 1;
constexpr uint64_t COMPARE_TABLE_A_CLIENT_ID       = 1;
constexpr uint64_t COMPARE_TABLE_B_CLIENT_ID       = 2;
constexpr float    COMPARE_TABLE_MIN_WIDTH         = 280.0f;
constexpr float    COMPARE_CARD_MARGIN             = 4.0f;
constexpr float    COMPARE_SOURCE_TITLE_GAP_FACTOR = 2.0f;
constexpr uint64_t TABLE_DEFAULT_SORT_COLUMN       = 1;

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

    const CompareSourceInfo* source_a =
        dp.DataModel().GetCompareSource(COMPARE_SOURCE_A_INDEX);
    const CompareSourceInfo* source_b =
        dp.DataModel().GetCompareSource(COMPARE_SOURCE_B_INDEX);
    m_compare_mode = source_a && source_b;

    if(m_compare_mode)
    {
        BuildCompareGroup(
            m_event_group, TableType::kCompareEventTableA, TableType::kCompareEventTableB,
            kRPVControllerTableTypeEvents,
            RequestIdBuilder::MakeClientRequestId(RequestType::kFetchTrackEventTable,
                                                  COMPARE_TABLE_A_CLIENT_ID),
            RequestIdBuilder::MakeClientRequestId(RequestType::kFetchTrackEventTable,
                                                  COMPARE_TABLE_B_CLIENT_ID),
            "Event Table A", "Event Table B", "events", "compare_event_tab",
            timeline_selection);
        BuildCompareGroup(
            m_sample_group, TableType::kCompareSampleTableA,
            TableType::kCompareSampleTableB, kRPVControllerTableTypeSamples,
            RequestIdBuilder::MakeClientRequestId(RequestType::kFetchTrackSampleTable,
                                                  COMPARE_TABLE_A_CLIENT_ID),
            RequestIdBuilder::MakeClientRequestId(RequestType::kFetchTrackSampleTable,
                                                  COMPARE_TABLE_B_CLIENT_ID),
            "Sample Table A", "Sample Table B", "samples", "compare_sample_tab",
            timeline_selection);
    }
    else
    {
        m_event_group.tables.push_back(std::make_shared<MultiTrackTable>(
            dp, TableType::kEventTable, kRPVControllerTableTypeEvents,
            DataProvider::EVENT_TABLE_REQUEST_ID,
            [&dp]() -> const TablesModel& { return dp.DataModel().GetTables(); },
            [&dp]() -> TablesModel& { return dp.DataModel().GetTables(); }, true,
            timeline_selection));
        m_sample_group.tables.push_back(std::make_shared<MultiTrackTable>(
            dp, TableType::kSampleTable, kRPVControllerTableTypeSamples,
            DataProvider::SAMPLE_TABLE_REQUEST_ID,
            [&dp]() -> const TablesModel& { return dp.DataModel().GetTables(); },
            [&dp]() -> TablesModel& { return dp.DataModel().GetTables(); }, true,
            timeline_selection));
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
AnalysisView::BuildCompareGroup(CompareGroup& group, TableType type_a, TableType type_b,
                                rocprofvis_controller_table_type_t request_type,
                                uint64_t request_id_a, uint64_t request_id_b,
                                const char* friendly_a, const char* friendly_b,
                                const char* noun, const char* child_id,
                                std::shared_ptr<TimelineSelection> timeline_selection)
{
    DataProvider& dp = m_data_provider;
    group.noun       = noun;

    group.tables.push_back(std::make_shared<MultiTrackTable>(
        dp, type_a, request_type, request_id_a,
        [&dp]() -> const TablesModel& { return dp.DataModel().GetTables(); },
        [&dp]() -> TablesModel& { return dp.DataModel().GetTables(); }, false,
        timeline_selection, TABLE_DEFAULT_SORT_COLUMN, kRPVControllerSortOrderAscending,
        friendly_a, "", COMPARE_SOURCE_A_INDEX));
    group.tables.push_back(std::make_shared<MultiTrackTable>(
        dp, type_b, request_type, request_id_b,
        [&dp]() -> const TablesModel& { return dp.DataModel().GetTables(); },
        [&dp]() -> TablesModel& { return dp.DataModel().GetTables(); }, false,
        timeline_selection, TABLE_DEFAULT_SORT_COLUMN, kRPVControllerSortOrderAscending,
        friendly_b, "", COMPARE_SOURCE_B_INDEX));
    group.tables[COMPARE_SOURCE_A_INDEX]->SetDisplaySummary(false);
    group.tables[COMPARE_SOURCE_B_INDEX]->SetDisplaySummary(false);

    CompareGroup* grp = &group;
    group.tables[COMPARE_SOURCE_A_INDEX]->SetHeaderRenderer(
        [this, grp]() { RenderCompareSourceTitle(*grp, COMPARE_SOURCE_A_INDEX); });
    group.tables[COMPARE_SOURCE_B_INDEX]->SetHeaderRenderer(
        [this, grp]() { RenderCompareSourceTitle(*grp, COMPARE_SOURCE_B_INDEX); });
    group.tables[COMPARE_SOURCE_A_INDEX]->SetFilterSubmitCallback(
        [grp](const MultiTrackTable& source) {
            grp->tables[COMPARE_SOURCE_B_INDEX]->ApplySharedFiltersFrom(source);
        });

    // Each table draws its own card, so the panes stay borderless with a small margin.
    LayoutItem::Ptr source_a_item =
        LayoutItem::CreateFromWidget(group.tables[COMPARE_SOURCE_A_INDEX]);
    source_a_item->m_child_flags    = ImGuiChildFlags_None;
    source_a_item->m_window_padding = ImVec2(COMPARE_CARD_MARGIN, COMPARE_CARD_MARGIN);
    LayoutItem::Ptr source_b_item =
        LayoutItem::CreateFromWidget(group.tables[COMPARE_SOURCE_B_INDEX]);
    source_b_item->m_child_flags    = ImGuiChildFlags_None;
    source_b_item->m_window_padding = ImVec2(COMPARE_CARD_MARGIN, COMPARE_CARD_MARGIN);
    group.split = std::make_shared<HSplitContainer>(source_a_item, source_b_item);
    group.split->SetSplit(0.5f);
    group.split->SetMinLeftWidth(COMPARE_TABLE_MIN_WIDTH);
    group.split->SetMinRightWidth(COMPARE_TABLE_MIN_WIDTH);

    std::string child_id_str = child_id;
    group.layout             = std::make_shared<RocCustomWidget>(
        [this, grp, child_id_str]() { RenderCompareTab(*grp, child_id_str.c_str()); });
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

    group.tables[COMPARE_SOURCE_A_INDEX]->RenderSharedControls();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if(group.split)
    {
        group.split->Render();
    }

    ImGui::EndChild();
    ImGui::PopID();
    ImGui::PopStyleVar();
}

void
AnalysisView::RenderCompareSourceTitle(CompareGroup& group, size_t source_index)
{
    if(source_index >= group.tables.size())
    {
        return;
    }
    const CompareSourceInfo* source =
        m_data_provider.DataModel().GetCompareSource(source_index);
    if(!source)
    {
        return;
    }

    SettingsManager& settings = SettingsManager::GetInstance();

    RenderCompareSourceBadge(*source, settings);
    // Panes run with ItemSpacing 0, so take the badge gap from the default style.
    ImGui::SameLine(
        0.0f, settings.GetDefaultStyle().ItemSpacing.x * COMPARE_SOURCE_TITLE_GAP_FACTOR);

    const std::string& label = source->name.empty() ? source->path : source->name;
    const std::string  summary =
        std::to_string(group.tables[source_index]->GetTotalRowCount()) + " " + group.noun +
        " \xC2\xB7 " +
        std::to_string(group.tables[source_index]->GetIncludedTrackCount()) + " tracks";
    const float summary_width = ImGui::CalcTextSize(summary.c_str()).x;

    float label_width = ImGui::GetContentRegionAvail().x - summary_width -
                        ImGui::GetStyle().ItemSpacing.x;
    if(label_width < 0.0f)
    {
        label_width = 0.0f;
    }
    ElidedText(label.c_str(), label_width, ImGui::GetFontSize() * 24.0f, Alignment_Left,
               true);
    ImGui::SameLine();
    ImGui::AlignTextToFramePadding();
    ImGui::TextDisabled("%s", summary.c_str());
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