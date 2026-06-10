// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_infinite_scroll_table.h"
#include "rocprofvis_appwindow.h"
#include "rocprofvis_common_defs.h"
#include "rocprofvis_settings_manager.h"
#include "rocprofvis_timeline_selection.h"
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

constexpr const char* ROWCONTEXTMENU_POPUP_NAME = "RowContextMenu";
constexpr uint64_t    FETCH_CHUNK_SIZE          = 1000;
constexpr const char* START_TS_COLUMN_NAME      = "start";
constexpr const char* END_TS_COLUMN_NAME        = "end";
constexpr const char* DURATION_COLUMN_NAME      = "duration";

InfiniteScrollTable::InfiniteScrollTable(
    DataProvider& dp, TableType table_type,
    rocprofvis_controller_table_type_t request_table_type, uint64_t request_id,
    const std::function<const TablesModel&()> table_model,
    const std::function<TablesModel&()>       table_model_mutable,
    std::shared_ptr<TimelineSelection>        timeline_selection,
    uint64_t                                  default_sort_column_index,
    rocprofvis_controller_sort_order_t        default_sort_order,
    const std::string& friendly_name, const std::string& no_data_text)
: m_data_provider(dp)
, m_open_context_menu(false)
, m_skip_data_fetch(false)
, m_table_type(table_type)
, m_request_table_type(request_table_type)
, m_request_id(request_id)
, m_table_model(table_model)
, m_table_model_mutable(table_model_mutable)
, m_last_total_row_count(0)
, m_fetch_chunk_size(FETCH_CHUNK_SIZE)  // Number of items to fetch in one go
, m_fetch_pad_items(30)                 // Number of items to pad the fetch range
, m_fetch_threshold_items(10)  // Number of items from the edge to trigger a fetch
, m_last_table_size(0, 0)
, m_settings(SettingsManager::GetInstance())
, m_filter_options({ "", "", "", "" })
, m_pending_filter_options({ "", "", "", "" })
, m_sort_column_index(default_sort_column_index)
, m_default_sort_column_index(default_sort_column_index)
, m_sort_order(default_sort_order)
, m_default_sort_order(default_sort_order)
, m_data_changed(true)
, m_filter_requested(false)
, m_selected_row(-1)
, m_selected_column(-1)
, m_hovered_row(-1)
, m_no_data_text(no_data_text)
, m_export_notification_id(dp.GetTraceFilePath())
, m_timeline_selection(timeline_selection)
, m_horizontal_scroll(0.0f)
, m_time_column_indices(
      { INVALID_UINT64_INDEX, INVALID_UINT64_INDEX, INVALID_UINT64_INDEX })
, m_important_column_idxs(std::vector<size_t>(kNumImportantColumns, INVALID_UINT64_INDEX))
{
    ROCPROFVIS_ASSERT(m_table_model && m_table_model_mutable);

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
            NotificationManager::GetInstance().Hide(m_export_notification_id);
            NotificationManager::GetInstance().Show(
                success ? "Exported: " + file_path : "Failed to export: " + file_path,
                success ? NotificationLevel::Success : NotificationLevel::Error);
        });

    auto request_progress_update_handler = [this](std::shared_ptr<RocEvent> e) {
        auto event = std::dynamic_pointer_cast<RequestProgressUpdateEvent>(e);
        if(event && event->GetSourceId() == m_data_provider.GetTraceFilePath() &&
           event->GetRequestType() == RequestType::kTableExport)
        {
            NotificationManager::GetInstance().UpdateProgress(m_export_notification_id,
                                                              event->GetProgressPercent(),
                                                              event->GetMessage());
        }
    };
    m_request_progress_update_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kRequestProgressUpdate),
        request_progress_update_handler);
    m_widget_name = GenUniqueName(friendly_name);
}

InfiniteScrollTable::~InfiniteScrollTable()
{
    EventManager::GetInstance()->Unsubscribe(static_cast<int>(RocEvents::kNewTableData),
                                             m_new_table_data_token);
    EventManager::GetInstance()->Unsubscribe(
        static_cast<int>(RocEvents::kTimeFormatChanged), m_format_changed_token);
    EventManager::GetInstance()->Unsubscribe(
        static_cast<int>(RocEvents::kRequestProgressUpdate),
        m_request_progress_update_token);
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
    // Matches the actual table row height; *WithSpacing would over-estimate
    // it and the clipper would drop bottom rows as the panel grows.
    ImGuiStyle& style         = ImGui::GetStyle();
    float       row_padding_v = style.CellPadding.y * 2.0f;
    float       row_height    = ImGui::GetTextLineHeight() + row_padding_v;

    // track the frame number for debugging purposes
    static uint64_t frame_count = 0;
    frame_count++;

    // Flag to show loading spinner
    bool show_loading_indicator = false;

    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding,
                        m_settings.GetDefaultStyle().ChildRounding);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, m_settings.GetColor(Colors::kBgPanel));
    ImGui::PushStyleColor(ImGuiCol_Border, m_settings.GetColor(Colors::kBorderColor));
    ImGui::BeginChild(m_widget_name.c_str(), ImVec2(0, 0), ImGuiChildFlags_Borders);
    const auto& table_model = m_table_model();

    const std::vector<std::vector<std::string>>& table_data =
        table_model.GetTableData(m_table_type);
    const std::vector<std::string>& column_names =
        table_model.GetTableHeader(m_table_type);
    auto     table_params    = table_model.GetTableParams(m_table_type);
    uint64_t total_row_count = table_model.GetTableTotalRowCount(m_table_type);

    const std::vector<FormattedColumnInfo>& formatted_table_data =
        table_model.GetFormattedTableData(m_table_type);

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

    if(!m_data_provider.IsRequestPending(m_request_id))
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

        // TODO: is there a more reliable way to detect Group by changes?
        ImGui::PushID(static_cast<int>(column_names.size())); 
        ImGui::PushStyleColor(ImGuiCol_ChildBg,
                              m_settings.GetColor(Colors::kFillerColor));
        if(!table_data.empty())
        {
            if(ImGui::BeginTable("infinite_scroll_table",
                                 static_cast<int>(column_names.size()), table_flags,
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
                int j = 0;
                for(int i = 0; i < column_names.size(); i++)
                {
                    ImGuiTableColumnFlags col_flags = ImGuiTableColumnFlags_None;
                    if(!column_names[i].empty() && column_names[i][0] == '_')
                    {
                        col_flags = ImGuiTableColumnFlags_DefaultHide |
                                    ImGuiTableColumnFlags_Disabled;
                    }
                    if(!m_hidden_column_indices.empty() &&
                       i == m_hidden_column_indices[j])
                    {
                        col_flags = ImGuiTableColumnFlags_DefaultHide |
                                    ImGuiTableColumnFlags_Disabled;
                        if(j < m_hidden_column_indices.size() - 1)
                        {
                            j++;
                        }
                    }
                    if(i == m_default_sort_column_index)
                    {
                        col_flags |=
                            ImGuiTableColumnFlags_DefaultSort |
                            (m_default_sort_order == kRPVControllerSortOrderAscending
                                 ? ImGuiTableColumnFlags_PreferSortAscending
                                 : ImGuiTableColumnFlags_PreferSortDescending);
                    }
                    ImGui::TableSetupColumn(column_names[i].c_str(), col_flags);
                }

                // Get sort specs
                ImGuiTableSortSpecs* sort_specs = ImGui::TableGetSortSpecs();
                if(sort_specs && (sort_specs->SpecsDirty ||
                                  sort_specs->Specs->ColumnIndex != m_sort_column_index ||
                                  sort_specs->Specs->SortOrder != m_sort_order))
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
                    for(int row_n = clipper.DisplayStart; row_n < clipper.DisplayEnd;
                        ++row_n)
                    {
                        if(row_n < 0 || row_n >= table_data.size())
                            continue;  // Should not happen with proper clipper usage
                        ImGui::PushID(row_n);
                        ImGui::TableNextRow();

                        ImGui::TableSetBgColor(
                            ImGuiTableBgTarget_RowBg0,
                            (row_n == m_hovered_row)
                                ? m_settings.GetColor(Colors::kHighlightChart)
                                : 0);

                        int  column       = 0;
                        bool place_hitbox = true;
                        for(const auto& col : table_data[row_n])
                        {
                            ImGui::TableSetColumnIndex(column);
                            const std::string* display_value = &col;

                            if(column < formatted_table_data.size())
                            {
                                const auto& col_format_info =
                                    formatted_table_data[column];
                                if(col_format_info.needs_formatting &&
                                   row_n < col_format_info.formatted_row_value.size())
                                {
                                    display_value =
                                        &col_format_info.formatted_row_value[row_n];
                                }
                            }

                            if(place_hitbox && (ImGui::TableGetColumnFlags() &
                                                ImGuiTableColumnFlags_IsVisible))
                            {
                                ImGui::PushStyleColor(ImGuiCol_Header,
                                                      ImVec4(0, 0, 0, 0));
                                ImGui::PushStyleColor(ImGuiCol_HeaderHovered,
                                                      ImVec4(0, 0, 0, 0));
                                ImGui::PushStyleColor(ImGuiCol_HeaderActive,
                                                      ImVec4(0, 0, 0, 0));
                                if(ImGui::Selectable(
                                       "", false,
                                       ImGuiSelectableFlags_SpanAllColumns |
                                           ImGuiSelectableFlags_AllowOverlap))
                                {
                                    m_selected_row = row_n;
                                    RowSelected(ImGuiMouseButton_Left);
                                }
                                if(ImGui::IsItemClicked(ImGuiMouseButton_Right))
                                {
                                    m_selected_row = row_n;
                                    RowSelected(ImGuiMouseButton_Right);
                                }
                                if(ImGui::IsItemHovered(
                                       ImGuiHoveredFlags_AllowWhenBlockedByActiveItem |
                                       ImGuiHoveredFlags_AllowWhenOverlappedByItem))
                                {
                                    m_hovered_row = row_n;
                                }
                                ImGui::PopStyleColor(3);
                                ImGui::SameLine(0.0f, 0.0f);
                                place_hitbox = false;
                            }

                            // Render actual cells after the row hit-box
                            RenderCell(display_value, row_n, column);
                            if(m_time_column_indices[kTimeEndNs] == column ||
                               m_time_column_indices[kTimeStartNs] == column ||
                               m_time_column_indices[kDurationNs] == column)
                            {
                                // show raw value as tooltip for time columns if hovered
                                if(ImGui::IsItemHovered())
                                {
                                    SetTooltipStyled("%s ns", col.c_str());
                                }
                            }

                            column++;
                        }
                        ImGui::PopID();
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
                    if(!m_data_provider.IsRequestPending(m_request_id))
                    {
                        if(scroll_y < start_row_position +
                                          m_fetch_threshold_items * row_height &&
                           start_row > 0)
                        {  // fetch data for the start row
                            uint64_t new_start_pos =
                                static_cast<uint64_t>(scroll_y / row_height);
                            int visible_rows =
                                static_cast<int>(outer_size.y / row_height);
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

                            spdlog::debug(
                                "Fetching more data for start row, new start pos: "
                                "{}, frame count: {}, chunk size: {}, scroll y: {}",
                                new_start_pos, frame_count, m_fetch_chunk_size, scroll_y);

                            m_data_provider.FetchTable(TableRequestParams(
                                m_request_table_type, table_params->m_track_ids,
                                table_params->m_op_types, table_params->m_start_ts,
                                table_params->m_end_ts, table_params->m_where.c_str(),
                                table_params->m_filter.c_str(),
                                table_params->m_group.c_str(),
                                table_params->m_group_columns.c_str(),
                                table_params->m_string_table_filters, new_start_pos,
                                m_fetch_chunk_size, table_params->m_sort_column_index,
                                table_params->m_sort_order));
                        }
                        else if((scroll_y + ImGui::GetWindowHeight() >
                                 end_row_position -
                                     m_fetch_threshold_items * row_height) &&
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

                            spdlog::debug(
                                "Fetching more data for end row, new start pos: "
                                "{}, frame count: {}, chunk size: {}, scroll y: {}",
                                new_start_pos, frame_count, m_fetch_chunk_size, scroll_y);

                            m_data_provider.FetchTable(TableRequestParams(
                                m_request_table_type, table_params->m_track_ids,
                                table_params->m_op_types, table_params->m_start_ts,
                                table_params->m_end_ts, table_params->m_where.c_str(),
                                table_params->m_filter.c_str(),
                                table_params->m_group.c_str(),
                                table_params->m_group_columns.c_str(),
                                table_params->m_string_table_filters, new_start_pos,
                                m_fetch_chunk_size, table_params->m_sort_column_index,
                                table_params->m_sort_order));
                        }
                    }
                }
                ImGui::EndTable();  // End BeginTable
            }
            RenderContextMenu();
        }
        else if(!m_no_data_text.empty() && !show_loading_indicator &&
                ImGui::GetContentRegionAvail().y > ImGui::GetTextLineHeight())
        {
            CenterNextTextItem(m_no_data_text.c_str());
            ImGui::SetCursorPosY((ImGui::GetWindowHeight() - ImGui::GetTextLineHeight()) *
                                 0.5f);
            ImGui::TextDisabled("%s", m_no_data_text.c_str());
        }
        ImGui::PopStyleColor();
        ImGui::PopID();
    }

    if(show_loading_indicator)
    {
        RenderLoadingIndicator(m_settings.GetColor(Colors::kTextMain),
                               m_widget_name.c_str());
    }

    ImGui::EndChild();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar();

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
InfiniteScrollTable::RenderContextMenu()
{
    if(m_open_context_menu)
    {
        ImGui::OpenPopup(ROWCONTEXTMENU_POPUP_NAME);
        m_open_context_menu = false;
    }

    auto style = m_settings.GetDefaultStyle();

    // Render context menu for row actions
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, style.WindowPadding);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, style.ItemSpacing);
    if(ImGui::BeginPopup(ROWCONTEXTMENU_POPUP_NAME))
    {
        uint64_t target_track_id = SelectedRowToTrackID(
            m_important_column_idxs[kTrackId], m_important_column_idxs[kStreamId]);
        if(target_track_id != INVALID_UINT64_INDEX)
        {
            if(ImGui::MenuItem(m_table_type == TableType::kSampleTable ? "Go To Sample"
                                                                       : "Go To Event",
                               nullptr, false, target_track_id != INVALID_UINT64_INDEX))
            {
                SelectedRowNavigateEvent(m_important_column_idxs[kTrackId],
                                         m_important_column_idxs[kStreamId]);
            }
            ImGui::Separator();
        }        
        if(ImGui::MenuItem("Copy Row Data", nullptr, false))
        {
            SelectedRowToClipboard();
        }
        else if(ImGui::MenuItem("Copy Cell Data", nullptr, false))
        {
            SelectedCellToClipboard(true);
        }

        // Only show option to copy unformatted cell data if
        // column has formatting applied to it.
        bool show_copy_unformatted = false;
        if(m_selected_column >= 0 &&
           m_selected_column < (int) m_table_model().GetTableHeader(m_table_type).size())
        {
            const auto&                             table_model = m_table_model();
            const std::vector<FormattedColumnInfo>& formatted_table_data =
                table_model.GetFormattedTableData(m_table_type);
            const auto& col_format_info = formatted_table_data[m_selected_column];
            show_copy_unformatted       = col_format_info.needs_formatting;
        }
        if(show_copy_unformatted)
        {
            if(ImGui::MenuItem("Copy Unformatted Cell Data", nullptr, false))
            {
                SelectedCellToClipboard(false);
            }
        }
        ImGui::Separator();
        if(ImGui::MenuItem(
               "Export To File", nullptr, false,
               !m_data_provider.IsRequestPending(DataProvider::TABLE_EXPORT_REQUEST_ID)))
        {
            ExportToFile();
        }
        // TODO handle event selection
        // else if(ImGui::MenuItem("Select event", nullptr, false))
        // {
        //     uint64_t event_id = INVALID_UINT64_INDEX;

        //     if(m_important_columns[kUUId] != INVALID_COLUMN_INDEX &&
        //        m_important_columns[kUUId] < table_data[m_selected_row].size())
        //     {
        //         event_id =
        //             std::stoull(table_data[m_selected_row][m_important_columns[kUUId]]);
        //     }
        // }

        ImGui::EndPopup();
    }
    // Pop the style vars for window padding and item spacing
    ImGui::PopStyleVar(2);
}

void
InfiniteScrollTable::ProcessSortOrFilterRequest(
    rocprofvis_controller_sort_order_t sort_order,
                                        uint64_t sort_column_index, uint64_t frame_count)
{
    auto table_params = m_table_model().GetTableParams(m_table_type);
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
        if(m_filter_requested || sort_order != m_sort_order ||
           sort_column_index != m_sort_column_index)
        {
            m_sort_column_index = sort_column_index;
            m_sort_order        = sort_order;
            // Update the event table params with the new sort request
            table_params->m_sort_column_index = m_sort_column_index;
            table_params->m_sort_order        = m_sort_order;
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
                m_request_table_type, table_params->m_track_ids, table_params->m_op_types,
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
        m_table_model().GetTableHeader(m_table_type);
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
    spdlog::debug(mouse_button == ImGuiMouseButton_Left ? "Row {} clicked"
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
            m_table_model().GetTableData(m_table_type);
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
            m_table_model().GetTableData(m_table_type);
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
        m_table_model().GetTableData(m_table_type);
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
InfiniteScrollTable::SelectedCellToClipboard(bool use_formatted_data) const
{
    const std::vector<std::vector<std::string>>& table_data =
        m_table_model().GetTableData(m_table_type);

    if(m_selected_row < 0 || m_selected_row >= (int) table_data.size())
    {
        spdlog::warn("Selected row index out of bounds: {}", m_selected_row);
        return;
    }

    if(m_selected_column < 0 ||
       m_selected_column >= (int) table_data[m_selected_row].size())
    {
        spdlog::warn("Selected column index out of bounds: {}", m_selected_column);
        return;
    }

    std::string cell_text = table_data[m_selected_row][m_selected_column];

    if(use_formatted_data)
    {
        const auto&                             table_model = m_table_model();
        const std::vector<FormattedColumnInfo>& formatted_table_data =
            table_model.GetFormattedTableData(m_table_type);

        const auto& col_format_info = formatted_table_data[m_selected_column];
        if(col_format_info.needs_formatting &&
           m_selected_row < col_format_info.formatted_row_value.size())
        {
            cell_text = col_format_info.formatted_row_value[m_selected_row];
        }
    }

    ImGui::SetClipboardText(cell_text.c_str());
    NotificationManager::GetInstance().Show("Cell data was copied",
                                            NotificationLevel::Info);
}

void
InfiniteScrollTable::SelectedRowNavigateEvent(size_t track_id_column_index,
                                              size_t stream_id_column_index) const
{
    const std::vector<std::vector<std::string>>& table_data =
        m_table_model().GetTableData(m_table_type);
    if(m_selected_row < 0 || m_selected_row >= (int) table_data.size())
    {
        spdlog::warn("Selected row index out of bounds: {}", m_selected_row);
    }
    else
    {
        uint64_t target_track_id =
            SelectedRowToTrackID(track_id_column_index, stream_id_column_index);
        if(target_track_id != INVALID_UINT64_INDEX)
        {
            std::pair<uint64_t, uint64_t> time_range = SelectedRowToTimeRange();
            if(time_range.first != INVALID_UINT64_INDEX &&
               time_range.second != INVALID_UINT64_INDEX)
            {
                spdlog::debug("Navigating to track ID: {} from row: {}",
                              target_track_id, m_selected_row);

                uint64_t uuid = TimelineSelection::INVALID_SELECTION_ID;
                if(m_important_column_idxs[kUUId] != INVALID_UINT64_INDEX)
                {
                    uuid = std::stoull(
                        table_data[m_selected_row][m_important_column_idxs[kUUId]]);
                }

                if(m_timeline_selection)
                {
                    m_timeline_selection->NavigateToEvent(
                        target_track_id, uuid,
                        static_cast<double>(time_range.first),
                        static_cast<double>(time_range.second - time_range.first));
                }
            }
        }
        else
        {
            spdlog::warn("No valid track or stream ID found for row: {}", m_selected_row);
        }
    }
}

void
InfiniteScrollTable::SelectedRowContextMenu()
{
    m_open_context_menu = true;
}

void
InfiniteScrollTable::FormatTimeColumns() const
{
    const std::vector<std::vector<std::string>>& table_data =
        m_table_model().GetTableData(m_table_type);
    std::vector<FormattedColumnInfo>& formatted_column_data =
        m_table_model_mutable().GetMutableFormattedTableData(m_table_type);
    auto   time_format = m_settings.GetUserSettings().unit_settings.time_format;
    double start_time  = m_data_provider.DataModel().GetTimeline().GetStartTime();
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
                m_table_model_mutable().GetTableParams(m_table_type);
            if(table_params &&
               m_data_provider.FetchTable(TableRequestParams(
                   m_request_table_type, table_params->m_track_ids,
                   table_params->m_op_types, table_params->m_start_ts,
                   table_params->m_end_ts, table_params->m_where.c_str(),
                   table_params->m_filter.c_str(), table_params->m_group.c_str(),
                   table_params->m_group_columns.c_str(),
                   table_params->m_string_table_filters, INVALID_UINT64_INDEX,
                   INVALID_UINT64_INDEX, table_params->m_sort_column_index,
                   table_params->m_sort_order, file_path)))
            {
                NotificationManager::GetInstance().ShowPersistent(
                    m_export_notification_id, "Exporting: " + file_path,
                    NotificationLevel::Info);
            }
        });
}

}  // namespace View
}  // namespace RocProfVis
