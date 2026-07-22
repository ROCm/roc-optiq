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

constexpr size_t   COMPARE_SOURCE_A_INDEX         = 0;
constexpr size_t   COMPARE_SOURCE_B_INDEX         = 1;
constexpr uint64_t COMPARE_EVENT_TABLE_A_CLIENT_ID = 1;
constexpr uint64_t COMPARE_EVENT_TABLE_B_CLIENT_ID = 2;
constexpr float    COMPARE_EVENT_TABLE_MIN_WIDTH = 280.0f;
constexpr float    COMPARE_CARD_MARGIN               = 4.0f;
constexpr uint64_t EVENT_TABLE_DEFAULT_SORT_COLUMN = 1;

AnalysisView::AnalysisView(DataProvider& dp, std::shared_ptr<TrackTopology> topology,
                           std::shared_ptr<TimelineSelection>  timeline_selection,
                           std::shared_ptr<AnnotationsManager> annotation_manager)
: m_data_provider(dp)
, m_sample_table(std::make_shared<MultiTrackTable>(
      dp, TableType::kSampleTable, kRPVControllerTableTypeSamples,
      DataProvider::SAMPLE_TABLE_REQUEST_ID,
      [&dp]() -> const TablesModel& { return dp.DataModel().GetTables(); },
      [&dp]() -> TablesModel& { return dp.DataModel().GetTables(); }, true,
      timeline_selection))
, m_split_event_tables(false)
, m_events_view(std::make_shared<EventsView>(dp, timeline_selection))
, m_annotation_view(std::make_shared<AnnotationView>(dp, annotation_manager))
, m_track_details(std::make_shared<TrackDetails>(dp, topology, timeline_selection))
, m_top_events_view(std::make_shared<TopEventsView>(dp, timeline_selection))
{
    m_widget_name = GenUniqueName("Analysis View");

    const CompareSourceInfo* source_a =
        dp.DataModel().GetCompareSource(COMPARE_SOURCE_A_INDEX);
    const CompareSourceInfo* source_b =
        dp.DataModel().GetCompareSource(COMPARE_SOURCE_B_INDEX);
    m_split_event_tables = source_a && source_b;
    if(m_split_event_tables)
    {
        m_event_tables.push_back(std::make_shared<MultiTrackTable>(
            dp, TableType::kCompareEventTableA, kRPVControllerTableTypeEvents,
            RequestIdBuilder::MakeClientRequestId(RequestType::kFetchTrackEventTable,
                                                  COMPARE_EVENT_TABLE_A_CLIENT_ID),
            [&dp]() -> const TablesModel& { return dp.DataModel().GetTables(); },
            [&dp]() -> TablesModel& { return dp.DataModel().GetTables(); }, false,
            timeline_selection, EVENT_TABLE_DEFAULT_SORT_COLUMN,
            kRPVControllerSortOrderAscending, "Event Table A", "", COMPARE_SOURCE_A_INDEX));
        m_event_tables.push_back(std::make_shared<MultiTrackTable>(
            dp, TableType::kCompareEventTableB, kRPVControllerTableTypeEvents,
            RequestIdBuilder::MakeClientRequestId(RequestType::kFetchTrackEventTable,
                                                  COMPARE_EVENT_TABLE_B_CLIENT_ID),
            [&dp]() -> const TablesModel& { return dp.DataModel().GetTables(); },
            [&dp]() -> TablesModel& { return dp.DataModel().GetTables(); }, false,
            timeline_selection, EVENT_TABLE_DEFAULT_SORT_COLUMN,
            kRPVControllerSortOrderAscending, "Event Table B", "", COMPARE_SOURCE_B_INDEX));
        m_event_tables[COMPARE_SOURCE_A_INDEX]->SetDisplaySummary(false);
        m_event_tables[COMPARE_SOURCE_B_INDEX]->SetDisplaySummary(false);

        m_event_tables[COMPARE_SOURCE_A_INDEX]->SetHeaderRenderer(
            [this]() { RenderCompareSourceTitle(COMPARE_SOURCE_A_INDEX); });
        m_event_tables[COMPARE_SOURCE_B_INDEX]->SetHeaderRenderer(
            [this]() { RenderCompareSourceTitle(COMPARE_SOURCE_B_INDEX); });

        m_event_tables[COMPARE_SOURCE_A_INDEX]->SetFilterSubmitCallback(
            [this](const MultiTrackTable& source) {
                m_event_tables[COMPARE_SOURCE_B_INDEX]->ApplySharedFiltersFrom(source);
            });

        // Each source renders as its own single-bordered card, so the panes and the
        // container that hold them are borderless. A small margin gives the two cards
        // breathing room around the splitter.
        LayoutItem::Ptr source_a_item =
            LayoutItem::CreateFromWidget(m_event_tables[COMPARE_SOURCE_A_INDEX]);
        source_a_item->m_child_flags    = ImGuiChildFlags_None;
        source_a_item->m_window_padding  = ImVec2(COMPARE_CARD_MARGIN, COMPARE_CARD_MARGIN);
        LayoutItem::Ptr source_b_item =
            LayoutItem::CreateFromWidget(m_event_tables[COMPARE_SOURCE_B_INDEX]);
        source_b_item->m_child_flags    = ImGuiChildFlags_None;
        source_b_item->m_window_padding  = ImVec2(COMPARE_CARD_MARGIN, COMPARE_CARD_MARGIN);
        m_event_table_split =
            std::make_shared<HSplitContainer>(source_a_item, source_b_item);
        m_event_table_split->SetSplit(0.5f);
        m_event_table_split->SetMinLeftWidth(COMPARE_EVENT_TABLE_MIN_WIDTH);
        m_event_table_split->SetMinRightWidth(COMPARE_EVENT_TABLE_MIN_WIDTH);

        // Render the shared filter form and the two source cards directly in the tab's
        // full-width content region so controls size correctly and share one padded frame.
        m_event_table_layout =
            std::make_shared<RocCustomWidget>([this]() { RenderCompareEventTab(); });
    }
    else
    {
        m_event_tables.push_back(std::make_shared<MultiTrackTable>(
            dp, TableType::kEventTable, kRPVControllerTableTypeEvents,
            DataProvider::EVENT_TABLE_REQUEST_ID,
            [&dp]() -> const TablesModel& { return dp.DataModel().GetTables(); },
            [&dp]() -> TablesModel& { return dp.DataModel().GetTables(); }, true,
            timeline_selection));
    }

    m_tab_container = std::make_shared<TabContainer>();

    TabItem tab_item;
    tab_item.m_label     = "Event Table";
    tab_item.m_id        = "event_table";
    tab_item.m_can_close = false;
    if(m_split_event_tables)
    {
        tab_item.m_widget = m_event_table_layout;
    }
    else
    {
        tab_item.m_widget = m_event_tables.front();
    }
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
    if(m_split_event_tables)
    {
        for(std::shared_ptr<MultiTrackTable>& table : m_event_tables)
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

void
AnalysisView::RenderCompareEventTab()
{
    const ImGuiStyle& style = SettingsManager::GetInstance().GetDefaultStyle();
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, style.WindowPadding);
    ImGui::BeginChild("##compare_event_tab", ImVec2(0.0f, 0.0f),
                      ImGuiChildFlags_AlwaysUseWindowPadding,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    // Shared filter form spanning the full content width.
    m_event_tables[COMPARE_SOURCE_A_INDEX]->RenderSharedControls();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Side-by-side source cards fill the remaining height.
    if(m_event_table_split)
    {
        m_event_table_split->Render();
    }

    ImGui::EndChild();
    ImGui::PopStyleVar();
}

void
AnalysisView::RenderCompareSourceTitle(size_t source_index)
{
    if(source_index >= m_event_tables.size())
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
    // Extra gap so the A/B badge is not crowded against the source name.
    ImGui::SameLine(0.0f, ImGui::GetStyle().ItemSpacing.x * 2.0f);

    const std::string& label = source->name.empty() ? source->path : source->name;
    const std::string  summary =
        std::to_string(m_event_tables[source_index]->GetTotalRowCount()) + " events \xC2\xB7 " +
        std::to_string(m_event_tables[source_index]->GetIncludedTrackCount()) + " tracks";
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
                for(std::shared_ptr<MultiTrackTable>& table : m_event_tables)
                {
                    table->HandleTrackSelectionChanged(
                        selection_changed_event->GetTrackID(),
                        selection_changed_event->TrackSelected());
                }
                if(m_sample_table)
                {
                    m_sample_table->HandleTrackSelectionChanged(
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
                for(std::shared_ptr<MultiTrackTable>& table : m_event_tables)
                {
                    table->HandleTimeRangeSelectionChanged(
                        selection_changed_event->GetStartNs(),
                        selection_changed_event->GetEndNs());
                }
                if(m_sample_table)
                {
                    m_sample_table->HandleTimeRangeSelectionChanged(
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