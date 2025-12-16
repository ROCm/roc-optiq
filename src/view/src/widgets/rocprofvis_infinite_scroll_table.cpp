// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_infinite_scroll_table.h"
#include "icons/rocprovfis_icon_defines.h"
#include "rocprofvis_appwindow.h"
#include "rocprofvis_font_manager.h"
#include "rocprofvis_settings_manager.h"
#include "rocprofvis_utils.h"
#include "spdlog/spdlog.h"
#include "widgets/rocprofvis_gui_helpers.h"
#include "widgets/rocprofvis_notification_manager.h"
#include <algorithm>
#include <sstream>

namespace RocProfVis
{
namespace View
{

constexpr uint64_t    FETCH_CHUNK_SIZE               = 1000;
constexpr const char* START_TS_COLUMN_NAME           = "startTs";
constexpr const char* END_TS_COLUMN_NAME             = "endTs";
constexpr const char* DURATION_COLUMN_NAME           = "duration";
constexpr const char* EXPORT_PENDING_NOTIFICATION_ID = "TableExportNotification";

InfiniteScrollTable::InfiniteScrollTable(DataProvider& dp, TableType table_type,
                                         const std::string& no_data_text)
: m_data_provider(dp)
, m_skip_data_fetch(false)
, m_table_type(table_type)
, m_last_total_row_count(0)
, m_fetch_chunk_size(FETCH_CHUNK_SIZE)  // Number of items to fetch in one go
, m_fetch_pad_items(30)                 // Number of items to pad the fetch range
, m_fetch_threshold_items(10)  // Number of items from the edge to trigger a fetch
, m_last_table_size(0, 0)
, m_settings(SettingsManager::GetInstance())
, m_req_table_type(
      table_type == TableType::kEventSearchTable ? kRPVControllerTableTypeSearchResults
      : table_type == TableType::kSummaryKernelTable ? kRPVControllerTableTypeSummaryKernelInstances
      : table_type == TableType::kEventTable         ? kRPVControllerTableTypeEvents
                                                     : kRPVControllerTableTypeSamples)
, m_filter_options({"", "", "", "" })
, m_pending_filter_options({"", "", "", "" })
, m_data_changed(true)
, m_filter_requested(false)
, m_selected_row(-1)
, m_selected_column(-1)
, m_hovered_row(-1)
, m_no_data_text(no_data_text)
, m_horizontal_scroll(0.0f)
, m_time_column_indices(
      { INVALID_UINT64_INDEX, INVALID_UINT64_INDEX, INVALID_UINT64_INDEX })
, m_important_column_idxs(std::vector<size_t>(kNumImportantColumns, INVALID_UINT64_INDEX))
{
    auto new_table_data_handler = [this](std::shared_ptr<RocEvent> e) {
        this->HandleNewTableData(e);
    };
    m_new_table_data_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kNewTableData), new_table_data_handler);

    // subscribe to time format changed event
    auto format_changed_handler = [this](std::shared_ptr<RocEvent> e) {
        // Reformat time columns
        this->FormatData();
    };
    m_format_changed_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kTimeFormatChanged), format_changed_handler);

    m_data_provider.SetExportTableCallback(
        [this](const std::string& file_path, bool success) {
            NotificationManager::GetInstance().Hide(EXPORT_PENDING_NOTIFICATION_ID);
            NotificationManager::GetInstance().Show(
                success ? "Exported: " + file_path : "Failed to export: " + file_path,
                success ? NotificationLevel::Success : NotificationLevel::Error);
        });
}

InfiniteScrollTable::~InfiniteScrollTable()
{
    EventManager::GetInstance()->Unsubscribe(static_cast<int>(RocEvents::kNewTableData),
                                             m_new_table_data_token);
    EventManager::GetInstance()->Unsubscribe(
        static_cast<int>(RocEvents::kTimeFormatChanged), m_format_changed_token);
}

uint64_t
InfiniteScrollTable::GetRequestID() const
{
    switch(m_table_type)
    {
        case TableType::kSampleTable:
        {
            return DataProvider::SAMPLE_TABLE_REQUEST_ID;
        }
        case TableType::kEventTable:
        {
            return DataProvider::EVENT_TABLE_REQUEST_ID;
        }
        case TableType::kEventSearchTable:
        {
            return DataProvider::EVENT_SEARCH_REQUEST_ID;
        }
        case TableType::kSummaryKernelTable:
        {
            return DataProvider::SUMMARY_KERNEL_INSTANCE_TABLE_REQUEST_ID;
        }
        default:
        {
            return INVALID_UINT64_INDEX;
        }
    }
}

void
InfiniteScrollTable::Update()
{
    if(m_data_changed)
    {
        FormatData();
        m_data_changed = false;
    }
}

void
InfiniteScrollTable::FormatData() const
{
    // default implementation does nothing
}

void
InfiniteScrollTable::HandleNewTableData(std::shared_ptr<RocEvent> e)
{
    if(e && e->GetSourceId() == m_data_provider.GetTraceFilePath())
    {
        m_data_changed = true;
        IndexColumns();
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
    auto     table_params    = m_data_provider.GetTableParams(m_table_type);
    uint64_t total_row_count = m_data_provider.GetTableTotalRowCount(m_table_type);

    const std::vector<formatted_column_info_t>& formatted_table_data =
        m_data_provider.GetFormattedTableData(m_table_type);

    // Skip data fetch for this render cycle if total row count has changed
    // This is so we can recalulate the table size with the new total row count
    if(m_last_total_row_count != total_row_count)
    {
        m_skip_data_fetch      = true;
        m_last_total_row_count = total_row_count;
    }

    uint64_t row_count = 0;
    uint64_t start_row = 0;
    if(table_params)
    {
        row_count = table_params->m_req_row_count;
        start_row = table_params->m_start_row;
    }
    uint64_t end_row = start_row + row_count;
    if(end_row >= total_row_count)
    {
        end_row = total_row_count - 1;  // Ensure we don't exceed the total row count
    }

    float start_row_position = start_row * row_height;
    float end_row_position   = end_row * row_height;

    bool                               sort_requested    = false;
    uint64_t                           sort_column_index = 0;
    rocprofvis_controller_sort_order_t sort_order = kRPVControllerSortOrderAscending;

    ImGuiTableFlags table_flags = ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg |
                                  ImGuiTableFlags_BordersOuter | ImGuiTableFlags_ScrollX |
                                  ImGuiTableFlags_BordersV | ImGuiTableFlags_Resizable |
                                  ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable;


    if(!m_data_provider.IsRequestPending(GetRequestID()))
    {
        // If the request is not pending, we can allow sorting
        table_flags |= ImGuiTableFlags_Sortable;
    }
    else
    {
        show_loading_indicator = true;
    }

    {
        ImVec2 outer_size = ImVec2(0.0f, ImGui::GetContentRegionAvail().y);
        if(outer_size.y != m_last_table_size.y)
        {
            // If the outer size has changed, we need to recalulate the number of
            // items to fetch
            int visible_rows   = static_cast<int>(outer_size.y / row_height);
            m_fetch_pad_items  = std::clamp(visible_rows / 2, 10, 30);
            m_fetch_chunk_size = std::max(static_cast<uint64_t>(visible_rows * 4 +
                                                                m_fetch_threshold_items +
                                                                m_fetch_pad_items),
                                          FETCH_CHUNK_SIZE);
            m_last_table_size  = outer_size;

            spdlog::debug("Recalculated fetch chunk size: {}, fetch pad items: {}, "
                          "visible rows: {}, outer size: {}",
                          m_fetch_chunk_size, m_fetch_pad_items, visible_rows,
                          outer_size.y);
        }

        if(column_names.size() &&
           ImGui::BeginTable("Event Data Table", static_cast<int>(column_names.size()),
                             table_flags, outer_size))
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
            int j = 0;
            for(int i = 0; i < column_names.size(); i++)
            {
                ImGuiTableColumnFlags col_flags = ImGuiTableColumnFlags_None;
                if(!column_names[i].empty() && column_names[i][0] == '_')
                {
                    col_flags = ImGuiTableColumnFlags_DefaultHide |
                                ImGuiTableColumnFlags_Disabled;
                }
                if(!m_hidden_column_indices.empty() && i == m_hidden_column_indices[j])
                {
                    col_flags = ImGuiTableColumnFlags_DefaultHide |
                                ImGuiTableColumnFlags_Disabled;
                    if(j < m_hidden_column_indices.size() - 1)
                    {
                        j++;
                    }
                }
                ImGui::TableSetupColumn(column_names[i].c_str(), col_flags);
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
            clipper.Begin(static_cast<int>(table_data.size()), row_height);
            while(clipper.Step())
            {
                for(int row_n = clipper.DisplayStart; row_n < clipper.DisplayEnd; ++row_n)
                {
                    if(row_n < 0 || row_n >= table_data.size())
                        continue;  // Should not happen with proper clipper usage

                    ImGui::TableNextRow();

                    ImGui::TableSetBgColor(
                        ImGuiTableBgTarget_RowBg0,
                        (row_n == m_hovered_row)
                            ? m_settings.GetColor(Colors::kAccentRedHover)
                            : 0);

                    // Render actual cells after the row hit-box
                    int column = 0;
                    for(const auto& col : table_data[row_n])
                    {
                        ImGui::TableSetColumnIndex(column);
                        const std::string* display_value = &col;

                        if(column < formatted_table_data.size())
                        {
                            const auto& col_format_info = formatted_table_data[column];
                            if(col_format_info.needs_formatting &&
                               row_n < col_format_info.formatted_row_value.size())
                            {
                                display_value =
                                    &col_format_info.formatted_row_value[row_n];
                            }
                        }

                        if(column == 0)
                        {
                            RenderFirstColumnCell(display_value, row_n);
                        }
                        else
                        {
                            RenderCell(display_value, row_n, column);
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

            float scroll_y      = ImGui::GetScrollY();
            float scroll_max_y  = ImGui::GetScrollMaxY();
            m_horizontal_scroll = ImGui::GetScrollMaxX() != 0;

            // only check if we need to fetch based on the scroll position if we don't
            // have all the data
            if(!m_skip_data_fetch && table_data.size() < total_row_count - 1)
            {
                if(!m_data_provider.IsRequestPending(GetRequestID()))
                {
                    if(scroll_y <
                           start_row_position + m_fetch_threshold_items * row_height &&
                       start_row > 0)
                    {  // fetch data for the start row
                        uint64_t new_start_pos =
                            static_cast<uint64_t>(scroll_y / row_height);
                        int visible_rows = static_cast<int>(outer_size.y / row_height);
                        int offset =
                            static_cast<int>(m_fetch_chunk_size - m_fetch_pad_items -
                                             m_fetch_threshold_items - visible_rows);
                        // Ensure start position does not go below zero
                        if(offset > new_start_pos)
                        {
                            new_start_pos = 0;
                        }
                        else
                        {
                            new_start_pos -= offset;
                        }

                        spdlog::debug("Fetching more data for start row, new start pos: "
                                      "{}, frame count: {}, chunk size: {}, scroll y: {}",
                                      new_start_pos, frame_count, m_fetch_chunk_size,
                                      scroll_y);

                        m_data_provider.FetchTable(TableRequestParams(
                            m_req_table_type, table_params->m_track_ids,
                            table_params->m_op_types, table_params->m_start_ts,
                            table_params->m_end_ts, table_params->m_where.c_str(),
                            table_params->m_filter.c_str(), table_params->m_group.c_str(),
                            table_params->m_group_columns.c_str(),
                            table_params->m_string_table_filters, new_start_pos,
                            m_fetch_chunk_size, table_params->m_sort_column_index,
                            table_params->m_sort_order));
                    }
                    else if((scroll_y + ImGui::GetWindowHeight() >
                             end_row_position - m_fetch_threshold_items * row_height) &&
                            (end_row != total_row_count - 1) && (scroll_max_y > 0.0f))
                    {
                        // fetch data for the end row
                        uint64_t new_start_pos =
                            static_cast<uint64_t>(scroll_y / row_height);

                        // Ensure start position does not go below zero
                        // (this can happen if the start_row is close to the beginning
                        // of the table if a previous fetch did not return enough
                        // rows)
                        int offset = m_fetch_pad_items + m_fetch_threshold_items;
                        if(offset > new_start_pos)
                        {
                            new_start_pos = 0;
                        }
                        else
                        {
                            new_start_pos -= offset;
                        }

                        spdlog::debug("Fetching more data for end row, new start pos: "
                                      "{}, frame count: {}, chunk size: {}, scroll y: {}",
                                      new_start_pos, frame_count, m_fetch_chunk_size,
                                      scroll_y);

                        m_data_provider.FetchTable(TableRequestParams(
                            m_req_table_type, table_params->m_track_ids,
                            table_params->m_op_types, table_params->m_start_ts,
                            table_params->m_end_ts, table_params->m_where.c_str(),
                            table_params->m_filter.c_str(), table_params->m_group.c_str(),
                            table_params->m_group_columns.c_str(),
                            table_params->m_string_table_filters, new_start_pos,
                            m_fetch_chunk_size, table_params->m_sort_column_index,
                            table_params->m_sort_order));
                    }
                }
            }
            ImGui::EndTable();  // End BeginTable
        }
        else if(!m_no_data_text.empty())
        {
            ImGui::TextUnformatted(m_no_data_text.c_str());
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

    if(sort_requested || m_filter_requested)
    {
        ProcessSortOrFilterRequest(sort_order, sort_column_index, frame_count);
    }

    m_skip_data_fetch  = false;  // Reset the skip data fetch flag after rendering
    m_filter_requested = false;
}

void
InfiniteScrollTable::RenderCell(const std::string* cell_text, int row, int column)
{
    if(CopyableTextUnformatted(cell_text->c_str(),
                               std::to_string(row) + ":" + std::to_string(column),
                               COPY_DATA_NOTIFICATION, false, false))
    {
        m_selected_row    = row;
        m_selected_column = column;
        RowSelected(ImGuiMouseButton_Left);
    }

    if(ImGui::IsItemClicked(ImGuiMouseButton_Right))
    {
        m_selected_row    = row;
        m_selected_column = column;
        RowSelected(ImGuiMouseButton_Right);
    }

    if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem |
                            ImGuiHoveredFlags_AllowWhenOverlappedByItem))
    {
        m_hovered_row = row;
    }
}

void
InfiniteScrollTable::RenderFirstColumnCell(const std::string* cell_text, int row)
{
    std::string selectable_label = *cell_text + "##" + std::to_string(row);
    ImGui::TableSetColumnIndex(0);

    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0, 0, 0, 0));


    bool row_hovered = ImGui::Selectable(selectable_label.c_str(), false,
                                         ImGuiSelectableFlags_SpanAllColumns |
                                             ImGuiSelectableFlags_AllowOverlap,
                                         ImVec2(0.0f, 0.0f));
    if(row_hovered)
    {
        m_selected_row = row;
        RowSelected(ImGuiMouseButton_Left);

    }
    if(row_hovered ||
       ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem |
                            ImGuiHoveredFlags_AllowWhenOverlappedByItem))
    {
        m_hovered_row = row;

    }
    if(ImGui::IsItemClicked(ImGuiMouseButton_Right))
    {
        m_selected_row = row;
        m_selected_column = 0;
        RowSelected(ImGuiMouseButton_Right);
    }
    ImGui::PopStyleColor(3);
}

void
InfiniteScrollTable::ProcessSortOrFilterRequest(
    rocprofvis_controller_sort_order_t sort_order,
                                        uint64_t sort_column_index, uint64_t frame_count)
{
    auto table_params = m_data_provider.GetTableParams(m_table_type);
    if(table_params)
    {
        FilterOptions& filter =
            m_filter_requested ? m_pending_filter_options : m_filter_options;
        if(filter.group_by == "")
        {
            filter.group_columns[0] = '\0';
        }
        // check that sort order and column index actually are different from the
        // current values before fetching
        if(m_filter_requested || sort_order != table_params->m_sort_order ||
            sort_column_index != table_params->m_sort_column_index)
        {
            // Update the event table params with the new sort request
            table_params->m_sort_column_index = sort_column_index;
            table_params->m_sort_order        = sort_order;
            table_params->m_filter            = filter.filter;
            table_params->m_group = filter.group_by;
            table_params->m_group_columns = filter.group_columns;

            // if filtering changed reset the start row as current row
            // may be beyond result length causing an assertion in controller
            if(m_filter_requested)
            {
                table_params->m_start_row = 0;
            }

            spdlog::debug("Fetching data for sort, frame count: {}", frame_count);

                // Fetch the event table with the updated params
            m_data_provider.FetchTable(TableRequestParams(
                m_req_table_type, table_params->m_track_ids, table_params->m_op_types,
                table_params->m_start_ts, table_params->m_end_ts,
                table_params->m_where.c_str(), table_params->m_filter.c_str(),
                table_params->m_group.c_str(), table_params->m_group_columns.c_str(),
                table_params->m_string_table_filters, table_params->m_start_row,
                table_params->m_req_row_count, table_params->m_sort_column_index,
                table_params->m_sort_order));

            m_filter_options = filter;
        }
    }
    else
    {
        spdlog::warn(
            "Warning: Event table params not available, aborting sort request.");
    }
}

void
InfiniteScrollTable::IndexColumns()
{
    m_time_column_indices = { INVALID_UINT64_INDEX, INVALID_UINT64_INDEX,
                              INVALID_UINT64_INDEX };
    const std::vector<std::string>& column_names =
        m_data_provider.GetTableHeader(m_table_type);
    for(int i = 0; i < column_names.size(); i++)
    {
        if(column_names[i] == START_TS_COLUMN_NAME)
        {
            m_time_column_indices[kTimeStartNs] = i;
        }
        else if(column_names[i] == END_TS_COLUMN_NAME)
        {
            m_time_column_indices[kTimeEndNs] = i;
        }
        else if(column_names[i] == DURATION_COLUMN_NAME)
        {
            m_time_column_indices[kDurationNs] = i;
        }
    }
}

void
InfiniteScrollTable::RowSelected(const ImGuiMouseButton mouse_button)
{
    spdlog::info(mouse_button == ImGuiMouseButton_Left ? "Row {} clicked"
                                                       : "Row {} right-clicked",
                 m_selected_row);
}

uint64_t
InfiniteScrollTable::SelectedRowToTrackID(size_t track_id_column_index,
                                          size_t stream_id_column_index) const
{
    uint64_t target_track_id = INVALID_UINT64_INDEX;
    if(m_selected_row >= 0)
    {
        const std::vector<std::vector<std::string>>& table_data =
            m_data_provider.GetTableData(m_table_type);
        uint64_t track_id  = INVALID_UINT64_INDEX;
        uint64_t stream_id = INVALID_UINT64_INDEX;

        // get track id or stream id
        if(track_id_column_index != INVALID_UINT64_INDEX &&
           track_id_column_index < table_data[m_selected_row].size())
        {
            track_id = std::stoull(table_data[m_selected_row][track_id_column_index]);
        }
        else if(stream_id_column_index != INVALID_UINT64_INDEX &&
                stream_id_column_index < table_data[m_selected_row].size())
        {
            stream_id = std::stoull(table_data[m_selected_row][stream_id_column_index]);
        }
        if(track_id != INVALID_UINT64_INDEX)
        {
            target_track_id = track_id;
        }
        else if(stream_id != INVALID_UINT64_INDEX)
        {
            target_track_id = stream_id;
        }
    }
    return target_track_id;
}

std::pair<uint64_t, uint64_t>
InfiniteScrollTable::SelectedRowToTimeRange() const
{
    uint64_t start_time = INVALID_UINT64_INDEX;
    uint64_t end_time   = INVALID_UINT64_INDEX;
    if(m_selected_row >= 0)
    {
        const std::vector<std::vector<std::string>>& table_data =
            m_data_provider.GetTableData(m_table_type);
        if(m_time_column_indices[kTimeStartNs] != INVALID_UINT64_INDEX &&
           m_time_column_indices[kTimeStartNs] < table_data[m_selected_row].size())
        {
            start_time = std::stoull(
                table_data[m_selected_row][m_time_column_indices[kTimeStartNs]]);
        }

        if(m_time_column_indices[kTimeEndNs] != INVALID_UINT64_INDEX &&
           m_time_column_indices[kTimeEndNs] < table_data[m_selected_row].size())
        {
            end_time = std::stoull(
                table_data[m_selected_row][m_time_column_indices[kTimeEndNs]]);
        }
    }
    return std::make_pair(start_time, end_time);
}

void
InfiniteScrollTable::SelectedRowToClipboard() const
{
    const std::vector<std::vector<std::string>>& table_data =
        m_data_provider.GetTableData(m_table_type);
    if(m_selected_row < 0 || m_selected_row >= (int) table_data.size())
    {
        spdlog::warn("Selected row index out of bounds: {}", m_selected_row);
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
        NotificationManager::GetInstance().Show("Row data copied to clipboard",
                                                NotificationLevel::Info, 1.0);
    }
}

void
InfiniteScrollTable::SelectedRowNavigateEvent(size_t track_id_column_index,
                                              size_t stream_id_column_index) const
{
    const std::vector<std::vector<std::string>>& table_data =
        m_data_provider.GetTableData(m_table_type);
    if(m_selected_row < 0 || m_selected_row >= (int) table_data.size())
    {
        spdlog::warn("Selected row index out of bounds: {}", m_selected_row);
    }
    else
    {
        // Handle navigation
        uint64_t target_track_id =
            SelectedRowToTrackID(track_id_column_index, stream_id_column_index);
        if(target_track_id != INVALID_UINT64_INDEX)
        {
            // get start time and duration
            std::pair<uint64_t, uint64_t> time_range = SelectedRowToTimeRange();
            if(time_range.first != INVALID_UINT64_INDEX &&
               time_range.second != INVALID_UINT64_INDEX)
            {
                ViewRangeNS view_range = calculate_adaptive_view_range(
                    static_cast<double>(time_range.first),
                    static_cast<double>(time_range.second - time_range.first));
                spdlog::info("Navigating to track ID: {} from row: {}", target_track_id,
                             m_selected_row);
                EventManager::GetInstance()->AddEvent(
                    std::make_shared<ScrollToTrackEvent>(
                        static_cast<int>(RocEvents::kHandleUserGraphNavigationEvent),
                        target_track_id, m_data_provider.GetTraceFilePath()));
                EventManager::GetInstance()->AddEvent(std::make_shared<RangeEvent>(
                    static_cast<int>(RocEvents::kSetViewRange), view_range.start_ns,
                    view_range.end_ns, m_data_provider.GetTraceFilePath()));
            }
        }
        else
        {
            spdlog::warn("No valid track or stream ID found for row: {}", m_selected_row);
        }
    }
}

void
InfiniteScrollTable::FormatTimeColumns() const
{
    const std::vector<std::vector<std::string>>& table_data =
        m_data_provider.GetTableData(m_table_type);
    std::vector<formatted_column_info_t>& formatted_column_data =
        m_data_provider.GetMutableFormattedTableData(m_table_type);
    auto   time_format = m_settings.GetUserSettings().unit_settings.time_format;
    double start_time  = m_data_provider.GetStartTime();
    for(size_t i : m_time_column_indices)
    {
        if(i < formatted_column_data.size())
        {
            formatted_column_data[i].needs_formatting = true;
            formatted_column_data[i].formatted_row_value.resize(table_data.size());
            for(size_t row_idx = 0; row_idx < table_data.size(); row_idx++)
            {
                const std::string& raw_value = table_data[row_idx][i];
                // Format time values
                try
                {
                    double time_ns = static_cast<double>(std::stoull(raw_value));
                    if(i != m_time_column_indices[kDurationNs])
                    {
                        // time is relative to the trace start time
                        time_ns -= start_time;
                    }
                    formatted_column_data[i].formatted_row_value[row_idx] =
                        nanosecond_to_formatted_str(time_ns, time_format);
                } catch(const std::exception& e)
                {
                    spdlog::warn("Failed to format time value '{}': {}", raw_value,
                                 e.what());
                    formatted_column_data[i].formatted_row_value[row_idx] = raw_value;
                }
            }
        }
    }
}

void
InfiniteScrollTable::ExportToFile() const
{
    std::vector<FileFilter> file_filters;
    FileFilter trace_filter;
    trace_filter.m_name       = "CSV Files";
    trace_filter.m_extensions = { "csv" };
    file_filters.push_back(trace_filter);

    AppWindow::GetInstance()->ShowSaveFileDialog(
        "Export Table", file_filters, "", [this](std::string file_path) -> void {
            std::shared_ptr<TableRequestParams> table_params =
                m_data_provider.GetTableParams(m_table_type);
            if(table_params &&
               m_data_provider.FetchTable(TableRequestParams(
                   m_req_table_type, table_params->m_track_ids, table_params->m_op_types,
                   table_params->m_start_ts, table_params->m_end_ts,
                   table_params->m_where.c_str(), table_params->m_filter.c_str(),
                   table_params->m_group.c_str(), table_params->m_group_columns.c_str(),
                   table_params->m_string_table_filters, INVALID_UINT64_INDEX,
                   INVALID_UINT64_INDEX, table_params->m_sort_column_index,
                   table_params->m_sort_order, file_path)))
            {
                NotificationManager::GetInstance().ShowPersistent(
                    EXPORT_PENDING_NOTIFICATION_ID, "Exporting: " + file_path,
                    NotificationLevel::Info);
            }
        });
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

}  // namespace View
}  // namespace RocProfVis
