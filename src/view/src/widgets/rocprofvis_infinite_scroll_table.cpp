// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_infinite_scroll_table.h"

#include "spdlog/spdlog.h"

#include "widgets/rocprofvis_gui_helpers.h"
#include "widgets/rocprofvis_debug_window.h"

using namespace RocProfVis::View;

InfiniteScrollTable::InfiniteScrollTable(DataProvider& dp, TableType table_type)
: m_data_provider(dp)
, m_skip_data_fetch(false)
, m_table_type(table_type)
, m_last_total_row_count(0)
, m_fetch_chunk_size(100)      // Number of items to fetch in one go
, m_fetch_pad_items(30)        // Number of items to pad the fetch range
, m_fetch_threshold_items(10)  // Number of items from the edge to trigger a fetch
, m_last_table_size(0, 0)
, m_track_selection_event_to_handle(nullptr)
, m_settings(Settings::GetInstance())
, m_req_table_type(table_type == TableType::kEventTable ? kRPVControllerTableTypeEvents
                                                        : kRPVControllerTableTypeSamples)
{
    m_widget_name = (table_type == TableType::kEventTable)
                        ? GenUniqueName("Event Table")
                        : GenUniqueName("Sample Table");
}

void
InfiniteScrollTable::HandleTrackSelectionChanged(
    std::shared_ptr<TrackSelectionChangedEvent> selection_changed_event)
{
    if(selection_changed_event)
    {
        const std::vector<uint64_t>& tracks =
            selection_changed_event->GetSelectedTracks();
        double start_ns = selection_changed_event->GetStartNs();
        double end_ns   = selection_changed_event->GetEndNs();

        // loop trough tracks and filter out ones that don't match the table type
        std::vector<uint64_t> filtered_tracks;
        for(uint64_t track_index : tracks)
        {
            const track_info_t* track_info = m_data_provider.GetTrackInfo(track_index);
            if(track_info)
            {
                if((track_info->track_type == kRPVControllerTrackTypeSamples &&
                    m_table_type == TableType::kSampleTable) ||
                   (track_info->track_type == kRPVControllerTrackTypeEvents &&
                    m_table_type == TableType::kEventTable))
                {
                    filtered_tracks.push_back(track_index);
                }
            }
        }

        // if no tracks match the table type, clear the table
        if(filtered_tracks.empty())
        {
            m_data_provider.ClearTable(m_table_type);
            // Todo: Clear any pending requests for this table type?
            // clear any pending track selection event
            m_track_selection_event_to_handle = nullptr;
        }
        else
        {
            // Fetch table data for the selected tracks
            TableRequestParams event_table_params(m_req_table_type, filtered_tracks,
                                                  start_ns, end_ns, 0,
                                                  m_fetch_chunk_size);

            bool result = m_data_provider.FetchMultiTrackTable(event_table_params);
            if(!result)
            {
                spdlog::error("Failed to fetch table data for tracks: {}",
                              filtered_tracks.size());
                // save this selection event to reprocess it later (it's ok to replace the
                // previous one as the new one reflects the current selection)
                m_track_selection_event_to_handle = selection_changed_event;
            }
            else
            {
                // clear any pending track selection event
                m_track_selection_event_to_handle = nullptr;
            }
        }
        // Update the selected tracks for this table type
        m_selected_tracks = std::move(filtered_tracks);
    }
}

void
InfiniteScrollTable::Update()
{
    // Handle track selection changed event
    if(m_track_selection_event_to_handle)
    {
        if(!m_data_provider.IsRequestPending(m_table_type == TableType::kEventTable
                                                 ? DataProvider::EVENT_TABLE_REQUEST_ID
                                                 : DataProvider::SAMPLE_TABLE_REQUEST_ID))
        {
            // try to repocess the deferred track selection event
            spdlog::debug(
                "Reprocessing deferred track selection changed event for table type: {}",
                m_table_type == TableType::kEventTable ? "Event Table" : "Sample Table");
            HandleTrackSelectionChanged(m_track_selection_event_to_handle);
        }
    }
}

void
InfiniteScrollTable::Render()
{
    float       row_height    = ImGui::GetTextLineHeightWithSpacing();
    ImGuiStyle& style         = ImGui::GetStyle();
    float       row_padding_v = style.CellPadding.y * 2.0f;
    // Adjust row height to include padding
    row_height += row_padding_v;

    // track the frame number for debugging purposes
    static uint64_t frame_count = 0;
    frame_count++;

    // Flag to show loading spinner
    bool show_loading_indicator = false;

    ImGui::BeginChild(m_widget_name.c_str(), ImVec2(0, 0), true);

    const std::vector<std::vector<std::string>>& table_data =
        m_data_provider.GetTableData(m_table_type);
    const std::vector<std::string>& column_names =
        m_data_provider.GetTableHeader(m_table_type);
    auto     event_table_params = m_data_provider.GetTableParams(m_table_type);
    uint64_t total_row_count    = m_data_provider.GetTableTotalRowCount(m_table_type);

    // Skip data fetch for this render cycle if total row count has changed
    // This is so we can recalulate the table size with the new total row count
    if(m_last_total_row_count != total_row_count)
    {
        m_skip_data_fetch      = true;
        m_last_total_row_count = total_row_count;
    }

    uint64_t row_count            = 0;
    uint64_t start_row            = 0;
    uint64_t selected_track_count = 0;
    if(event_table_params)
    {
        row_count            = event_table_params->m_req_row_count;
        start_row            = event_table_params->m_start_row;
        selected_track_count = event_table_params->m_track_indices.size();
    }
    uint64_t end_row = start_row + row_count;
    if(end_row >= total_row_count)
    {
        end_row = total_row_count - 1;  // Ensure we don't exceed the total row count
    }

    float start_row_position = start_row * row_height;
    float end_row_position   = end_row * row_height;

    bool                               sort_requested    = false;
    uint64_t                           sort_colunn_index = 0;
    rocprofvis_controller_sort_order_t sort_order = kRPVControllerSortOrderAscending;

    ImGuiTableFlags table_flags = ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg |
                                  ImGuiTableFlags_BordersOuter |
                                  ImGuiTableFlags_BordersV | ImGuiTableFlags_Resizable |
                                  ImGuiTableFlags_Reorderable;

    if(!m_data_provider.IsRequestPending(m_table_type == TableType::kEventTable
                                             ? DataProvider::EVENT_TABLE_REQUEST_ID
                                             : DataProvider::SAMPLE_TABLE_REQUEST_ID))
    {
        // If the request is not pending, we can allow sorting
        table_flags |= ImGuiTableFlags_Sortable;
    } else {
        show_loading_indicator = true;
    }

    if(table_data.size() > 0 && m_selected_tracks.size() > 0)
    {
        ImGui::Text("Cached %d to %d of %d events for %d tracks", start_row, end_row,
                    total_row_count, selected_track_count);

        ImVec2 outer_size = ImVec2(0.0f, ImGui::GetContentRegionAvail().y);
        if(outer_size.y != m_last_table_size.y)
        {
            // If the outer size has changed, we need to recalulate the number of items to
            // fetch
            int visible_rows   = outer_size.y / row_height;
            m_fetch_chunk_size = std::max(visible_rows * 4, 100);
            m_fetch_pad_items  = clamp(visible_rows / 2, 10, 30);
            m_last_table_size  = outer_size;

            spdlog::debug("Recalculated fetch chunk size: {}, fetch pad items: {}, "
                          "visible rows: {}, outer size: {}",
                          m_fetch_chunk_size, m_fetch_pad_items, visible_rows,
                          outer_size.y);
        }

        if(ImGui::BeginTable("Event Data Table", column_names.size(), table_flags,
                             outer_size))
        {
            if(m_skip_data_fetch && ImGui::GetScrollY() > 0.0f)
            {
                // Reset scroll position if the track selection changed, the
                // m_skip_data_fetch flag indicates this. This is to ensure that the
                // scroll position is reset when we the start row was updated to 0 due to
                // new data selection
                ImGui::SetScrollY(0.0f);
            }

            ImGui::TableSetupScrollFreeze(0, 1);  // Freeze header row
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

            // Calculate the height of the top spacer based on the start row
            float top_spacer_height = (float) start_row * row_height;
            if(top_spacer_height > 0.0f)
            {
                ImGui::TableNextRow(ImGuiTableRowFlags_None, top_spacer_height);
                ImGui::TableNextColumn();  // Declare at least one column
            }

            ImGuiListClipper clipper;
            // Clipper's item count is the size of our current data cache
            clipper.Begin(table_data.size(), row_height);
            while(clipper.Step())
            {
                for(int row_n = clipper.DisplayStart; row_n < clipper.DisplayEnd; ++row_n)
                {
                    if(row_n < 0 || row_n >= table_data.size())
                        continue;  // Should not happen with proper clipper usage

                    ImGui::TableNextRow();
                    int column = 0;
                    for(const auto& col : table_data[row_n])
                    {
                        ImGui::TableSetColumnIndex(column);

                        if(column == 0)
                        {
                            // Handle row selection and click events
                            std::string selectable_label =
                                col + "##" + std::to_string(row_n);

                            bool is_selected = false;
                            // The Selectable spans all columns.
                            if(ImGui::Selectable(
                                   selectable_label.c_str(), &is_selected,
                                   ImGuiSelectableFlags_SpanAllColumns |
                                       ImGuiSelectableFlags_AllowItemOverlap,
                                   ImVec2(0, 0)))
                            {
                                // Todo: Handle row selection
                                // For now, just log the row and column clicked
                                spdlog::info("Row clicked: {} {}", col, row_n);
                            }
                        }
                        else
                        {
                            ImGui::Text(col.c_str());
                        }
                        column++;
                    }
                }
            }
            clipper.End();

            float bottom_spacer_height =
                (float) (total_row_count - (end_row + 1)) * row_height;
            if(bottom_spacer_height > 0.0f)
            {
                ImGui::TableNextRow(ImGuiTableRowFlags_None, bottom_spacer_height);
                ImGui::TableNextColumn();  // Declare at least one column
            }

            float scroll_y     = ImGui::GetScrollY();
            float scroll_max_y = ImGui::GetScrollMaxY();

            // only check if we need to fetch based on the scroll position if we don't
            // have all the data
            if(!m_skip_data_fetch && table_data.size() < total_row_count - 1)
            {
                bool fetching_event_table = m_data_provider.IsRequestPending(
                    DataProvider::EVENT_TABLE_REQUEST_ID);

                if(!fetching_event_table)
                {
                    if(scroll_y <
                           start_row_position + m_fetch_threshold_items * row_height &&
                       start_row > 0)
                    {  // fetch data for the start row
                        uint64_t new_start_pos = scroll_y / row_height;
                        // Ensure start position does not go below zero
                        if(m_fetch_pad_items > new_start_pos)
                        {
                            new_start_pos = 0;
                        }
                        else
                        {
                            new_start_pos -= m_fetch_pad_items;
                        }

                        spdlog::debug("Fetching more data for start row, new start pos: "
                                      "{}, frame count: {}, chunk size: {}, scroll y: {}",
                                      new_start_pos, frame_count, m_fetch_chunk_size,
                                      scroll_y);

                        m_data_provider.FetchMultiTrackTable(TableRequestParams(
                            m_req_table_type, event_table_params->m_track_indices,
                            event_table_params->m_start_ts, event_table_params->m_end_ts,
                            new_start_pos, m_fetch_chunk_size,
                            event_table_params->m_sort_column_index,
                            event_table_params->m_sort_order));
                    }
                    else if((scroll_y + ImGui::GetWindowHeight() >
                             end_row_position - m_fetch_threshold_items * row_height) &&
                            (end_row != total_row_count - 1) && (scroll_max_y > 0.0f))
                    {
                        // fetch data for the end row
                        uint64_t new_start_pos = (scroll_y) / row_height;

                        // Ensure start position does not go below zero
                        // (this can happen if the start_row is close to the beginning of
                        // the table if a previous fetch did not return enough rows)
                        if(m_fetch_pad_items > new_start_pos)
                        {
                            new_start_pos = 0;
                        }
                        else
                        {
                            new_start_pos -= m_fetch_pad_items;
                        }

                        spdlog::debug("Fetching more data for end row, new start pos: "
                                      "{}, frame count: {}, chunk size: {}, scroll y: {}",
                                      new_start_pos, frame_count, m_fetch_chunk_size,
                                      scroll_y);

                        m_data_provider.FetchMultiTrackTable(TableRequestParams(
                            m_req_table_type, event_table_params->m_track_indices,
                            event_table_params->m_start_ts, event_table_params->m_end_ts,
                            new_start_pos, m_fetch_chunk_size,
                            event_table_params->m_sort_column_index,
                            event_table_params->m_sort_order));
                    }
                }
            }
            ImGui::EndTable();
        }  // End BeginTable
    }
    else
    {
        ImGui::Text("No Event Data Available");
    }

    if(show_loading_indicator)
    {
        // Show a loading indicator if the data is being fetched
        RenderLoadingIndicator();
    }

    ImGui::EndChild();

    if(sort_requested)
    {
        if(event_table_params)
        {
            // Update the event table params with the new sort request
            event_table_params->m_sort_column_index = sort_colunn_index;
            event_table_params->m_sort_order        = sort_order;

            spdlog::debug("Fetching data for sort, frame count: {}", frame_count);

            // Fetch the event table with the updated params
            m_data_provider.FetchMultiTrackTable(TableRequestParams(
                m_req_table_type, event_table_params->m_track_indices,
                event_table_params->m_start_ts, event_table_params->m_end_ts,
                event_table_params->m_start_row, event_table_params->m_req_row_count,
                event_table_params->m_sort_column_index,
                event_table_params->m_sort_order));
        }
        else
        {
            spdlog::warn(
                "Warning: Event table params not available, aborting sort request.");
        }
    }

    m_skip_data_fetch = false;  // Reset the skip data fetch flag after rendering
}

void
InfiniteScrollTable::RenderLoadingIndicator()
{
    float dot_radius  = 5.0f;
    int   num_dots    = 3;
    float dot_spacing = 5.0f;
    float anim_speed  = 5.0f;

    ImVec2 dot_size = MeasureLoadingIndicatorDots(dot_radius, num_dots, dot_spacing);

    ImVec2 window_pos = ImGui::GetWindowPos();
    ImVec2 view_rect  = ImGui::GetWindowSize();
    ImVec2 pos        = ImGui::GetCursorPos();
    ImVec2 center_pos = ImVec2(window_pos.x + (view_rect.x - dot_size.x) * 0.5f,
                               window_pos.y + (view_rect.y - dot_size.y) * 0.5f);

    ImGui::SetCursorScreenPos(center_pos);

    RenderLoadingIndicatorDots(dot_radius, num_dots, dot_spacing,
                               m_settings.GetColor(Colors::kScrollBarColor), anim_speed);
                            
    // Reset cursor position after rendering spinner                               
    ImGui::SetCursorPos(pos);
}
