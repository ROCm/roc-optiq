// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_infinite_scroll_table.h"
#include "rocprofvis_settings_manager.h"
#include "rocprofvis_font_manager.h"
#include "icons/rocprovfis_icon_defines.h"
#include "rocprofvis_timeline_selection.h"
#include "rocprofvis_event_manager.h"

#include "spdlog/spdlog.h"
#include <sstream>

#include "widgets/rocprofvis_debug_window.h"
#include "widgets/rocprofvis_gui_helpers.h"
#include "widgets/rocprofvis_notification_manager.h"

namespace RocProfVis
{
namespace View
{

constexpr const char* ROWCONTEXTMENU_POPUP_NAME = "RowContextMenu";
constexpr uint64_t INVALID_UINT64_INDEX = std::numeric_limits<uint64_t>::max();

const std::string TRACK_ID_COLUMN_NAME = "__trackId";
const std::string STREAM_ID_COLUMN_NAME = "__streamTrackId";
const std::string ID_COLUMN_NAME = "id";
const std::string NAME_COLUMN_NAME = "name";
const std::string START_TS_COLUMN_NAME = "startTs";
const std::string END_TS_COLUMN_NAME = "endTs";
const std::string DURATION_COLUMN_NAME = "duration";

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
, m_settings(SettingsManager::GetInstance())
, m_req_table_type(table_type == TableType::kEventTable ? kRPVControllerTableTypeEvents
                                                        : kRPVControllerTableTypeSamples)
, m_filter_options({ 0, "", "" })
, m_pending_filter_options({ 0, "", "" })
, m_data_changed(true)
, m_important_column_idxs(std::vector<size_t>(kNumImportantColumns, INVALID_UINT64_INDEX))
{
    m_widget_name = (table_type == TableType::kEventTable)
                        ? GenUniqueName("Event Table")
                        : GenUniqueName("Sample Table");
}

void
InfiniteScrollTable::HandleTrackSelectionChanged(
    std::shared_ptr<TrackSelectionChangedEvent> e)
{
    if(e && e->GetTracePath() == m_data_provider.GetTraceFilePath())
    {
        const std::vector<uint64_t>& tracks   = e->GetSelectedTracks();
        double                       start_ns = e->GetStartNs();
        double                       end_ns   = e->GetEndNs();

        // If no valid time range is provided, use the full trace range
        if(start_ns == TimelineSelection::INVALID_SELECTION_TIME ||
           end_ns == TimelineSelection::INVALID_SELECTION_TIME)
        {
            start_ns = m_data_provider.GetStartTime();
            end_ns   = m_data_provider.GetEndTime();
        }

        // loop trough tracks and filter out ones that don't match the table type
        std::vector<uint64_t> filtered_tracks;
        for(uint64_t track_id : tracks)
        {
            const track_info_t* track_info = m_data_provider.GetTrackInfo(track_id);
            if(track_info)
            {
                if((track_info->track_type == kRPVControllerTrackTypeSamples &&
                    m_table_type == TableType::kSampleTable) ||
                   (track_info->track_type == kRPVControllerTrackTypeEvents &&
                    m_table_type == TableType::kEventTable))
                {
                    filtered_tracks.push_back(track_id);
                }
            }
        }

        bool fetch_result = false;

        uint64_t request_id = (m_table_type == TableType::kEventTable)
                                  ? DataProvider::EVENT_TABLE_REQUEST_ID
                                  : DataProvider::SAMPLE_TABLE_REQUEST_ID;
        // Cancel pending requests.
        if(m_data_provider.IsRequestPending(request_id))
        {
            m_data_provider.CancelRequest(request_id);
        }
        // if no tracks match the table type, clear the table
        if(filtered_tracks.empty())
        {
            m_data_provider.ClearTable(m_table_type);
            fetch_result = true;
        }
        else
        {
            // Fetch table data for the selected tracks
            TableRequestParams event_table_params(
                m_req_table_type, filtered_tracks, start_ns, end_ns,
                m_filter_options.filter,
                (m_filter_options.column_index == 0)
                    ? ""
                    : m_column_names_ptr[m_filter_options.column_index],
                m_filter_options.group_columns, 0, m_fetch_chunk_size);

            fetch_result = m_data_provider.FetchMultiTrackTable(event_table_params);
        }

        if(!fetch_result)
        {
            spdlog::error("Failed to queue table request for tracks: {}",
                          filtered_tracks.size());
            // save this selection event to reprocess it later (it's ok to replace the
            // previous one as the new one reflects the current selection)
            m_track_selection_event_to_handle = e;
        }
        else
        {
            // clear any pending track selection event
            m_track_selection_event_to_handle = nullptr;
        }
        // Update the selected tracks for this table type
        m_selected_tracks = std::move(filtered_tracks);
    }
}

void
InfiniteScrollTable::HandleNewTableData(std::shared_ptr<TableDataEvent> e)
{
    if(e && e->GetTracePath() == m_data_provider.GetTraceFilePath())
    {
        m_data_changed = true;
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
    if(m_data_changed)
    {
        const std::vector<std::string>& column_names =
            m_data_provider.GetTableHeader(m_table_type);

        //remember column index positions
        m_important_column_idxs = std::vector<size_t>(kNumImportantColumns, INVALID_UINT64_INDEX);
        for(size_t i = 0; i < column_names.size(); i++)
        {
            const auto& col = column_names[i];
            if(!col.empty()) 
            {
                if(col == TRACK_ID_COLUMN_NAME)
                {
                    m_important_column_idxs[kTrackId] = i;
                }
                else if(col == STREAM_ID_COLUMN_NAME)
                {
                    m_important_column_idxs[kStreamId] = i;
                }
                else if(col == ID_COLUMN_NAME)
                {
                    m_important_column_idxs[kId] = i;
                }
                else if(col == NAME_COLUMN_NAME)
                {
                    m_important_column_idxs[kName] = i;
                }
                else if(col == START_TS_COLUMN_NAME)
                {
                    m_important_column_idxs[kTimeStartNs] = i;
                }
                else if(col == END_TS_COLUMN_NAME)
                {
                    m_important_column_idxs[kTimeEndNs] = i;
                }
                else if(col == DURATION_COLUMN_NAME)
                {
                    m_important_column_idxs[kDurationNs] = i;
                }
            }
        }

        if(m_table_type == TableType::kEventTable)
        {
            if(m_filter_options.column_index == 0)
            {
                m_column_names.clear();
                // Create a combo box for selecting the group column
                // populate the combo box with column names but filter out empty and
                // internal columns (those starting with '_')
                m_column_names.reserve(column_names.size() + 1);
                m_column_names.push_back("-- None --");

                for(size_t i = 0; i < column_names.size(); i++)
                {
                    const auto& col = column_names[i];
                    if(col.empty() || col[0] == '_') 
                    {
                        continue;  // Skip empty or internal columns
                    }
                    m_column_names.push_back(col);
                }
            }
            else
            {
                std::string selected_option =
                    m_column_names[m_filter_options.column_index];
                m_column_names.resize(2);
                m_column_names[1]                     = selected_option;
                m_filter_options.column_index         = 1;
                m_pending_filter_options.column_index = 1;
            }
            m_column_names_ptr.resize(m_column_names.size());
            for(int i = 0; i < m_column_names.size(); i++)
            {
                m_column_names_ptr[i] = m_column_names[i].c_str();
            }
        }

        m_data_changed = false;
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
        selected_track_count = event_table_params->m_track_ids.size();
    }
    uint64_t end_row = start_row + row_count;
    if(end_row >= total_row_count)
    {
        end_row = total_row_count - 1;  // Ensure we don't exceed the total row count
    }

    float start_row_position = start_row * row_height;
    float end_row_position   = end_row * row_height;

    bool                               sort_requested    = false;
    bool                               filter_requested  = false;
    uint64_t                           sort_column_index = 0;
    rocprofvis_controller_sort_order_t sort_order = kRPVControllerSortOrderAscending;

    ImGuiTableFlags table_flags = ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg |
                                  ImGuiTableFlags_BordersOuter | ImGuiTableFlags_ScrollX |
                                  ImGuiTableFlags_BordersV | ImGuiTableFlags_Resizable |
                                  ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable;

    if(!m_data_provider.IsRequestPending(m_table_type == TableType::kEventTable
                                             ? DataProvider::EVENT_TABLE_REQUEST_ID
                                             : DataProvider::SAMPLE_TABLE_REQUEST_ID))
    {
        // If the request is not pending, we can allow sorting
        table_flags |= ImGuiTableFlags_Sortable;
    }
    else
    {
        show_loading_indicator = true;
    }

    {
        ImGui::Text("Cached %llu to %llu of %llu events for %llu tracks", start_row,
                    end_row, total_row_count, selected_track_count);

        ImGui::BeginGroup();
        if(m_table_type == TableType::kEventTable)
        {
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("Group By");
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("Group Columns");
        }
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Filter");
        ImGui::EndGroup();

        ImGui::SameLine();

        ImGui::BeginGroup();
        if(m_table_type == TableType::kEventTable)
        {
            ImGui::SetNextItemAllowOverlap();
            ImGui::Combo("##group_by", &m_pending_filter_options.column_index,
                         m_column_names_ptr.data(), m_column_names_ptr.size());
            if(m_pending_filter_options.column_index != 0)
            {
                ImGui::SameLine();
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() -
                                     ImGui::GetFrameHeightWithSpacing() -
                                     ImGui::GetFontSize());
                if(XButton("clear_group"))
                {
                    m_pending_filter_options.column_index = 0;
                }
            }

            ImGui::SetNextItemAllowOverlap();
            ImGui::BeginDisabled(m_filter_options.column_index == 0);
            ImGui::InputTextWithHint(
                "##group_columns",
                "name, COUNT(*) as num_invocations, AVG(duration) as avg_duration, "
                "MIN(duration) as min_duration, MAX(duration) as max_duration",
                m_pending_filter_options.group_columns,
                IM_ARRAYSIZE(m_pending_filter_options.group_columns));
            if(strlen(m_pending_filter_options.group_columns) > 0)
            {
                ImGui::SameLine();
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() - ImGui::GetFontSize());
                if(XButton("clear_group_columns"))
                {
                    m_pending_filter_options.group_columns[0] = '\0';
                }
            }
            ImGui::EndDisabled();
        }

        ImGui::SetNextItemAllowOverlap();
        ImGui::InputTextWithHint("##filters", "SQL WHERE comparisons",
                                 m_pending_filter_options.filter,
                                 IM_ARRAYSIZE(m_pending_filter_options.filter));
        if(strlen(m_pending_filter_options.filter) > 0)
        {
            ImGui::SameLine();
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() - ImGui::GetFontSize());
            if(XButton("clear_filters"))
            {
                m_pending_filter_options.filter[0] = '\0';
            }
        }
        ImGui::EndGroup();
        ImGui::SameLine();
        if(ImGui::Button("Submit", ImVec2(0, ImGui::GetItemRectSize().y)))
        {
            filter_requested = true;
        }

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

        if(column_names.size() &&
           ImGui::BeginTable("Event Data Table", column_names.size(), table_flags,
                             outer_size))
        {
            if(m_skip_data_fetch && ImGui::GetScrollY() > 0.0f)
            {
                // Reset scroll position if the track selection changed, the
                // m_skip_data_fetch flag indicates this. This is to ensure that the
                // scroll position is reset when the start row was updated to 0 due to
                // new data selection
                ImGui::SetScrollY(0.0f);
            }

            ImGui::TableSetupScrollFreeze(0, 1);  // Freeze header row
            for(const auto& col : column_names)
            {
                ImGuiTableColumnFlags col_flags = ImGuiTableColumnFlags_None;
                if(!col.empty() && col[0] == '_')
                {
                    col_flags = ImGuiTableColumnFlags_DefaultHide |
                                ImGuiTableColumnFlags_Disabled;
                }
                ImGui::TableSetupColumn(col.c_str(), col_flags);
            }

            // Get sort specs
            ImGuiTableSortSpecs* sort_specs = ImGui::TableGetSortSpecs();
            if(sort_specs && sort_specs->SpecsDirty)
            {
                sort_requested    = true;
                sort_column_index = sort_specs->Specs->ColumnIndex;
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
                            if(ImGui::IsItemClicked(ImGuiMouseButton_Right))
                            {
                                ImGui::OpenPopup(ROWCONTEXTMENU_POPUP_NAME);
                                m_selected_row = row_n;
                                spdlog::info("Row right-clicked: {} {} {}", col, row_n,
                                             m_selected_row);
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
                            m_req_table_type, event_table_params->m_track_ids,
                            event_table_params->m_start_ts, event_table_params->m_end_ts,
                            event_table_params->m_filter.c_str(),
                            event_table_params->m_group.c_str(),
                            event_table_params->m_group_columns.c_str(), new_start_pos,
                            m_fetch_chunk_size, event_table_params->m_sort_column_index,
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
                            m_req_table_type, event_table_params->m_track_ids,
                            event_table_params->m_start_ts, event_table_params->m_end_ts,
                            event_table_params->m_filter.c_str(),
                            event_table_params->m_group.c_str(),
                            event_table_params->m_group_columns.c_str(), new_start_pos,
                            m_fetch_chunk_size, event_table_params->m_sort_column_index,
                            event_table_params->m_sort_order));
                    }
                }
            }

            // Render context menu for row actions
            RenderContextMenu();

            // Pop the style vars for window padding and item spacing
            ImGui::PopStyleVar(2);
            ImGui::EndTable();  // End BeginTable
        }
        else
        {
            ImGui::Separator();
            ImGui::Text("No data available for the selected tracks or filters.");
        }
    }

    if(show_loading_indicator)
    {
        // Show a loading indicator if the data is being fetched
        // Create an overlay child window to display the loading indicator
        ImGui::SetCursorPos(ImVec2(0, 0));
        // set transparent background for the overlay window
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
        ImGui::BeginChild(m_widget_name.c_str(), ImGui::GetWindowSize(),
                          ImGuiChildFlags_None);
        RenderLoadingIndicator();
        ImGui::EndChild();
        ImGui::PopStyleColor();
    }

    ImGui::EndChild();

    if(sort_requested || filter_requested)
    {
        if(event_table_params)
        {
            FilterOptions& filter =
                filter_requested ? m_pending_filter_options : m_filter_options;
            if(filter.column_index == 0)
            {
                filter.group_columns[0] = '\0';
            }
            // check that sort order and column index actually are different from the
            // current values before fetching
            if(filter_requested || sort_order != event_table_params->m_sort_order ||
               sort_column_index != event_table_params->m_sort_column_index)
            {
                // Update the event table params with the new sort request
                event_table_params->m_sort_column_index = sort_column_index;
                event_table_params->m_sort_order        = sort_order;
                event_table_params->m_filter            = filter.filter;
                event_table_params->m_group =
                    (filter.column_index == 0) ? "" : m_column_names[filter.column_index];
                event_table_params->m_group_columns = filter.group_columns;

                spdlog::debug("Fetching data for sort, frame count: {}", frame_count);

                // Fetch the event table with the updated params
                m_data_provider.FetchMultiTrackTable(TableRequestParams(
                    m_req_table_type, event_table_params->m_track_ids,
                    event_table_params->m_start_ts, event_table_params->m_end_ts,
                    event_table_params->m_filter.c_str(),
                    event_table_params->m_group.c_str(),
                    event_table_params->m_group_columns.c_str(),
                    event_table_params->m_start_row, event_table_params->m_req_row_count,
                    event_table_params->m_sort_column_index,
                    event_table_params->m_sort_order));

                m_filter_options = filter;
            }
        }
        else
        {
            spdlog::warn(
                "Warning: Event table params not available, aborting sort request.");
        }
    }

    m_skip_data_fetch = false;  // Reset the skip data fetch flag after rendering
}

uint64_t
InfiniteScrollTable::GetTrackIdHelper(
    const std::vector<std::vector<std::string>>& table_data) const
{
    uint64_t track_id  = INVALID_UINT64_INDEX;
    uint64_t stream_id = INVALID_UINT64_INDEX;
    
    // get track id or stream id
    if(m_important_column_idxs[kTrackId] != INVALID_UINT64_INDEX &&
       m_important_column_idxs[kTrackId] < table_data[m_selected_row].size())
    {
        track_id = std::stoull(table_data[m_selected_row][m_important_column_idxs[kTrackId]]);
    }
    else if(m_important_column_idxs[kStreamId] != INVALID_UINT64_INDEX &&
            m_important_column_idxs[kStreamId] < table_data[m_selected_row].size())
    {
        stream_id =
            std::stoull(table_data[m_selected_row][m_important_column_idxs[kStreamId]]);
    }

    uint64_t target_track_id = INVALID_UINT64_INDEX;
    if(track_id != INVALID_UINT64_INDEX)
    {
        target_track_id = track_id;
    }
    else if(stream_id != INVALID_UINT64_INDEX)
    {
        target_track_id = stream_id;
    }

    return target_track_id;
}

void 
InfiniteScrollTable::RenderContextMenu() const
{
    auto style =  SettingsManager::GetInstance().GetDefaultStyle();
    
    // Render context menu for row actions
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, style.WindowPadding);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, style.ItemSpacing);
    if(ImGui::BeginPopup(ROWCONTEXTMENU_POPUP_NAME))
    {
        const std::vector<std::vector<std::string>>& table_data =
            m_data_provider.GetTableData(m_table_type);
        uint32_t target_track_id = GetTrackIdHelper(table_data);
                
        if(ImGui::MenuItem("Copy Row Data", nullptr, false))
        {
            if(m_selected_row < 0 || m_selected_row >= (int) table_data.size())
            {
                spdlog::warn("Selected row index out of bounds: {}",
                                m_selected_row);
            }
            else
            {
                // Build and copy the data from the selected row
                std::ostringstream str_collector;
                for(size_t i = 0; i < table_data[m_selected_row].size(); ++i)
                {
                    if(i > 0) str_collector << ",";
                    str_collector << table_data[m_selected_row][i];
                }
                std::string row_data = str_collector.str();
                // Copy the row data to the clipboard
                ImGui::SetClipboardText(row_data.c_str());
                // Show notification that data was copied
                NotificationManager::GetInstance().Show(
                    "Row data copied to clipboard", NotificationLevel::Info, 1.0);
            }
        }
        else if(ImGui::MenuItem("Go to event", nullptr, false, target_track_id != INVALID_UINT64_INDEX)) 
        {
            if(m_selected_row < 0 || m_selected_row >= (int) table_data.size())
            {
                spdlog::warn("Selected row index out of bounds: {}",
                                m_selected_row);
            }
            else
            {
                // Handle navigation
                if(target_track_id != INVALID_UINT64_INDEX) 
                {
                    spdlog::info("Navigating to track ID: {} from row: {}", target_track_id, m_selected_row);
                    EventManager::GetInstance()->AddEvent(std::make_shared<ScrollToTrackEvent>(
                        static_cast<int>(RocEvents::kHandleUserGraphNavigationEvent),
                        target_track_id, m_data_provider.GetTraceFilePath()));
                    //get start time and duration
                    uint64_t start_time = 0;
                    uint64_t duration = 0;
                    if(m_important_column_idxs[kTimeStartNs] != INVALID_UINT64_INDEX &&
                       m_important_column_idxs[kTimeStartNs] < table_data[m_selected_row].size())
                    {
                        start_time = std::stoull(table_data[m_selected_row][m_important_column_idxs[kTimeStartNs]]);
                    }

                    if(m_important_column_idxs[kDurationNs] != INVALID_UINT64_INDEX &&
                       m_important_column_idxs[kDurationNs] < table_data[m_selected_row].size())
                    {
                        duration = std::stoull(table_data[m_selected_row][m_important_column_idxs[kDurationNs]]);
                    }

                    ViewRangeNS view_range = calculate_adaptive_view_range(static_cast<double>(start_time),  static_cast<double>(duration));
                    EventManager::GetInstance()->AddEvent(std::make_shared<RangeEvent>(
                        static_cast<int>(RocEvents::kSetViewRange), view_range.start_ns,
                        view_range.end_ns, m_data_provider.GetTraceFilePath()));
                }
                else
                {
                    spdlog::warn("No valid track or stream ID found for row: {}",
                                  m_selected_row);
                }
            }
        }
        // TODO handle event selection
        // else if(ImGui::MenuItem("Select event", nullptr, false)) 
        // {
        //     uint64_t event_id = INVALID_UINT64_INDEX;

        //     if(m_important_columns[kId] != INVALID_COLUMN_INDEX &&
        //        m_important_columns[kId] < table_data[m_selected_row].size())
        //     {
        //         event_id =
        //             std::stoull(table_data[m_selected_row][m_important_columns[kId]]);
        //     }
        // }

        ImGui::EndPopup();
    }
}

void
InfiniteScrollTable::RenderLoadingIndicator() const
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
                               m_settings.GetColor(Colors::kTextMain), anim_speed);

    // Reset cursor position after rendering spinner
    ImGui::SetCursorPos(pos);
}

bool
InfiniteScrollTable::XButton(const char* id) const
{
    bool clicked = false;
    ImGui::PushStyleColor(ImGuiCol_Button, m_settings.GetColor(Colors::kTransparent));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                          m_settings.GetColor(Colors::kTransparent));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                          m_settings.GetColor(Colors::kTransparent));
    ImGui::PushStyleVarX(ImGuiStyleVar_FramePadding, 0);
    ImGui::PushFont(m_settings.GetFontManager().GetIconFont(FontType::kDefault));
    ImGui::PushID(id);
    clicked = ImGui::SmallButton(ICON_X_CIRCLED);
    ImGui::PopID();
    ImGui::PopFont();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                        m_settings.GetDefaultIMGUIStyle().WindowPadding);
    if(ImGui::BeginItemTooltip())
    {
        ImGui::TextUnformatted("Clear");
        ImGui::EndTooltip();
    }
    ImGui::PopStyleVar();
    return clicked;
}

}  // namespace View
}  // namespace RocProfVis
