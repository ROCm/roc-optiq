// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_analysis_view.h"
#include "rocprofvis_controller_enums.h"
#include "rocprofvis_data_provider.h"
#include "rocprofvis_events_view.h"
#include "spdlog/spdlog.h"

using namespace RocProfVis::View;

AnalysisView::AnalysisView(DataProvider& dp)
: m_data_provider(dp)
, m_max_displayed_rows(100)  // Default max rows to display
{
    m_widget_name = GenUniqueName("Analysis View");

    m_tab_container = std::make_shared<TabContainer>();

    TabItem tab_item;
    tab_item.m_label     = "Event Details";
    tab_item.m_id        = "event_details";
    tab_item.m_can_close = false;
    tab_item.m_widget =
        std::make_shared<RocCustomWidget>([this]() { this->RenderEventTable(); });
    m_tab_container->AddTab(tab_item);

    tab_item.m_label     = "Sample Details";
    tab_item.m_id        = "sample_details";
    tab_item.m_can_close = false;
    tab_item.m_widget =
        std::make_shared<RocCustomWidget>([this]() { this->RenderSampleTable(); });
    m_tab_container->AddTab(tab_item);


     m_events_view = std::make_shared<EventsView>();

      // Add EventsView tab
    tab_item.m_label     = "Events View";
    tab_item.m_id        = "events_view";
    tab_item.m_can_close = false;
    tab_item.m_widget =
        std::make_shared<RocCustomWidget>([this]() { m_events_view->Render(); });
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
AnalysisView::RenderEventTable()
{
    ImGui::BeginChild("Event Data", ImVec2(0, 0), true);

    const std::vector<std::vector<std::string>>& table_data =
        m_data_provider.GetEventTableData();
    const std::vector<std::string>& column_names = m_data_provider.GetEventTableHeader();

    bool                               sort_requested    = false;
    uint64_t                           sort_colunn_index = 0;
    rocprofvis_controller_sort_order_t sort_order = kRPVControllerSortOrderAscending;

    ImGuiTableFlags table_flags = ImGuiTableFlags_RowBg;
    if(!m_data_provider.IsRequestPending(DataProvider::EVENT_TABLE_REQUEST_ID))
    {
        // If the request is not pending, we can allow sorting
        table_flags |= ImGuiTableFlags_Sortable;
    }

    uint64_t total_row_count = m_data_provider.GetEventTableTotalRowCount();
    if(table_data.size() > 0)
    {
        ImGui::Text("Showing %d of %d events",
                    std::min(static_cast<int>(table_data.size()), m_max_displayed_rows),
                    static_cast<int>(total_row_count));

        ImGui::BeginTable("Event Data Table", column_names.size(), table_flags);
        for(const auto& col : column_names)
        {
            ImGui::TableSetupColumn(col.c_str());
        }

        // Get sort specs
        ImGuiTableSortSpecs* sort_specs = ImGui::TableGetSortSpecs();
        if(sort_specs && sort_specs->SpecsDirty)
        {
            sort_requested    = true;
            sort_colunn_index = sort_specs->Specs->ColumnIndex;
            sort_order =
                (sort_specs->Specs->SortDirection == ImGuiSortDirection_Ascending)
                    ? kRPVControllerSortOrderAscending
                    : kRPVControllerSortOrderDescending;

            sort_specs->SpecsDirty = false;
        }

        ImGui::TableHeadersRow();
        int count = 0;
        for(const auto& row : table_data)
        {
            ImGui::TableNextRow();
            int column = 0;
            for(const auto& col : row)
            {
                ImGui::TableSetColumnIndex(column);
                ImGui::Text(col.c_str());
                column++;
            }
            count++;
            if(count > m_max_displayed_rows)
            {
                break;  // Limit the number of displayed rows
            }
        }
        ImGui::EndTable();
    }
    else
    {
        ImGui::Text("No Event Data Available");
    }
    ImGui::EndChild();

    if(sort_requested)
    {
        auto event_table_params = m_data_provider.GetEventTableParams();
        if(event_table_params)
        {
            // Update the event table params with the new sort request
            event_table_params->m_sort_column_index = sort_colunn_index;
            event_table_params->m_sort_order        = sort_order;

            // Fetch the event table with the updated params
            m_data_provider.FetchMultiTrackEventTable(
                event_table_params->m_track_indices, event_table_params->m_start_ts,
                event_table_params->m_end_ts, event_table_params->m_start_row,
                event_table_params->m_req_row_count,
                event_table_params->m_sort_column_index,
                event_table_params->m_sort_order);
        }
        else
        {
            spdlog::warn(
                "Warning: Event table params not available, aborting sort request.");
        }
    }
}

void
AnalysisView::RenderSampleTable()
{
    ImGui::BeginChild("Sample Data", ImVec2(0, 0), true);

    const std::vector<std::vector<std::string>>& table_data =
        m_data_provider.GetSampleTableData();
    const std::vector<std::string>& column_names = m_data_provider.GetSampleTableHeader();

    bool                               sort_requested    = false;
    uint64_t                           sort_colunn_index = 0;
    rocprofvis_controller_sort_order_t sort_order = kRPVControllerSortOrderAscending;

    ImGuiTableFlags table_flags = ImGuiTableFlags_RowBg;
    if(!m_data_provider.IsRequestPending(DataProvider::SAMPLE_TABLE_REQUEST_ID))
    {
        // If the request is not pending, we can allow sorting
        table_flags |= ImGuiTableFlags_Sortable;
    }

    uint64_t total_row_count = m_data_provider.GetSampleTableTotalRowCount();
    if(table_data.size() > 0)
    {
        ImGui::Text("Showing %d of %d samples",
                    std::min(static_cast<int>(table_data.size()), m_max_displayed_rows),
                    static_cast<int>(total_row_count));
        ImGui::BeginTable("Sample Data Table", column_names.size(), table_flags);
        for(const auto& col : column_names)
        {
            ImGui::TableSetupColumn(col.c_str());
        }

        // Get sort specs
        ImGuiTableSortSpecs* sort_specs = ImGui::TableGetSortSpecs();
        if(sort_specs && sort_specs->SpecsDirty)
        {
            sort_requested    = true;
            sort_colunn_index = sort_specs->Specs->ColumnIndex;
            sort_order =
                (sort_specs->Specs->SortDirection == ImGuiSortDirection_Ascending)
                    ? kRPVControllerSortOrderAscending
                    : kRPVControllerSortOrderDescending;

            sort_specs->SpecsDirty = false;
        }

        ImGui::TableHeadersRow();
        int count = 0;
        for(const auto& row : table_data)
        {
            ImGui::TableNextRow();
            int column = 0;
            for(const auto& col : row)
            {
                ImGui::TableSetColumnIndex(column);
                ImGui::Text(col.c_str());
                column++;
            }
            count++;
            if(count > m_max_displayed_rows)
            {
                break;  // Limit the number of displayed rows
            }
        }
        ImGui::EndTable();
    }
    else
    {
        ImGui::Text("No Sample Data Available");
    }
    ImGui::EndChild();

    if(sort_requested)
    {
        auto sample_table_params = m_data_provider.GetSampleTableParams();
        if(sample_table_params)
        {
            // Update the sample table params with the new sort request
            sample_table_params->m_sort_column_index = sort_colunn_index;
            sample_table_params->m_sort_order        = sort_order;

            // Fetch the sample table with the updated params
            m_data_provider.FetchMultiTrackSampleTable(
                sample_table_params->m_track_indices, sample_table_params->m_start_ts,
                sample_table_params->m_end_ts, sample_table_params->m_start_row,
                sample_table_params->m_req_row_count,
                sample_table_params->m_sort_column_index,
                sample_table_params->m_sort_order);
        }
        else
        {
            spdlog::warn(
                "Warning: Sample table params not available, aborting sort request.");
        }
    }
}

void
AnalysisView::HandleTimelineSelectionChanged(std::shared_ptr<RocEvent> e)
{
    if(e && e->GetType() == RocEventType::kTimelineSelectionChangedEvent)
    {
        auto selection_changed_event =
            std::static_pointer_cast<TrackSelectionChangedEvent>(e);
        if(selection_changed_event)
        {
            const std::vector<uint64_t>& tracks =
                selection_changed_event->GetSelectedTracks();
            double start_ns = selection_changed_event->GetStartNs();
            double end_ns   = selection_changed_event->GetEndNs();
            if(!tracks.empty())
            {
                // Fetch event and sample tables for the selected tracks

                // Check if we have a table request params object and use it's parameters
                // if available
                auto event_table_params = m_data_provider.GetEventTableParams();
                if(!event_table_params)
                {
                    // Create a new table request params object if not available
                    event_table_params = std::make_shared<TableRequestParams>(
                        kRPVControllerTableTypeEvents, tracks, start_ns, end_ns);
                }
                m_data_provider.FetchMultiTrackEventTable(
                    tracks, start_ns, end_ns, event_table_params->m_start_row,
                    event_table_params->m_req_row_count,
                    event_table_params->m_sort_column_index,
                    event_table_params->m_sort_order);

                // Fetch sample table for the selected tracks
                auto sample_table_params = m_data_provider.GetSampleTableParams();
                if(!sample_table_params)
                {
                    // Create a new table request params object if not available
                    sample_table_params = std::make_shared<TableRequestParams>(
                        kRPVControllerTableTypeSamples, tracks, start_ns, end_ns);
                }
                m_data_provider.FetchMultiTrackSampleTable(
                    tracks, start_ns, end_ns, sample_table_params->m_start_row,
                    sample_table_params->m_req_row_count,
                    sample_table_params->m_sort_column_index,
                    sample_table_params->m_sort_order);
            }
            else
            {
                // Clear the tables if no tracks are selected
                m_data_provider.ClearEventTable();
                m_data_provider.ClearSampleTable();
            }
        }
    }
}