// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_infinite_scroll_table.h"
#include "icons/rocprovfis_icon_defines.h"
#include "rocprofvis_event_manager.h"
#include "rocprofvis_font_manager.h"
#include "rocprofvis_settings_manager.h"
#include "rocprofvis_utils.h"
#include "spdlog/spdlog.h"
#include "widgets/rocprofvis_gui_helpers.h"

#include <algorithm>

namespace RocProfVis
{
namespace View
{

constexpr uint64_t FETCH_CHUNK_SIZE = 1000;
constexpr uint64_t INVALID_UINT64_INDEX = std::numeric_limits<uint64_t>::max();

InfiniteScrollTable::InfiniteScrollTable(DataProvider& dp, TableType table_type)
: m_data_provider(dp)
, m_skip_data_fetch(false)
, m_table_type(table_type)
, m_last_total_row_count(0)
, m_fetch_chunk_size(FETCH_CHUNK_SIZE)      // Number of items to fetch in one go
, m_fetch_pad_items(30)        // Number of items to pad the fetch range
, m_fetch_threshold_items(10)  // Number of items from the edge to trigger a fetch
, m_last_table_size(0, 0)
, m_settings(SettingsManager::GetInstance())
, m_req_table_type(
      table_type == TableType::kEventSearchTable ? kRPVControllerTableTypeSearchResults
      : table_type == TableType::kEventTable     ? kRPVControllerTableTypeEvents
                                                 : kRPVControllerTableTypeSamples)
, m_filter_options({ 0, "", "" })
, m_pending_filter_options({ 0, "", "" })
, m_data_changed(true)
, m_filter_requested(false)
{}

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
        const std::vector<std::string>& column_names =
            m_data_provider.GetTableHeader(m_table_type);

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

        FormatData();
        m_data_changed = false;
    }
}

void InfiniteScrollTable::FormatData() {
    // default implementation does nothing
}

void
InfiniteScrollTable::HandleNewTableData(std::shared_ptr<TableDataEvent> e)
{
    if(e && e->GetSourceId() == m_data_provider.GetTraceFilePath())
    {
        m_data_changed = true;
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
           ImGui::BeginTable("Event Data Table", static_cast<int>(column_names.size()), table_flags,
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
            clipper.Begin(static_cast<int>(table_data.size()), row_height);
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
                        const std::string *display_value = &col;
                        // Check if this column needs formatting
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
                            // Handle row selection and click events
                            std::string selectable_label =
                                *display_value + "##" + std::to_string(row_n);

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
                            ImGui::Text(display_value->c_str());
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
                            table_params->m_end_ts, table_params->m_filter.c_str(),
                            table_params->m_group.c_str(),
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
                        uint64_t new_start_pos = static_cast<uint64_t>(scroll_y / row_height);

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
                            table_params->m_end_ts, table_params->m_filter.c_str(),
                            table_params->m_group.c_str(),
                            table_params->m_group_columns.c_str(),
                            table_params->m_string_table_filters, new_start_pos,
                            m_fetch_chunk_size, table_params->m_sort_column_index,
                            table_params->m_sort_order));
                    }
                }
            }

            // Render context menu for row actions
            RenderContextMenu();

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

    if(sort_requested || m_filter_requested)
    {
        if(table_params)
        {
            FilterOptions& filter =
                m_filter_requested ? m_pending_filter_options : m_filter_options;
            if(filter.column_index == 0)
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
                table_params->m_group =
                    (filter.column_index == 0) ? "" : m_column_names[filter.column_index];
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
                    table_params->m_filter.c_str(), table_params->m_group.c_str(),
                    table_params->m_group_columns.c_str(),
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

    m_skip_data_fetch  = false;  // Reset the skip data fetch flag after rendering
    m_filter_requested = false;
}

void
InfiniteScrollTable::RenderContextMenu() const
{}

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
