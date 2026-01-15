// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_multi_track_table.h"
#include "icons/rocprovfis_icon_defines.h"
#include "rocprofvis_settings_manager.h"
#include "rocprofvis_timeline_selection.h"
#include "rocprofvis_utils.h"
#include "spdlog/spdlog.h"
#include "widgets/rocprofvis_notification_manager.h"

namespace RocProfVis
{
namespace View
{

constexpr const char* ROWCONTEXTMENU_POPUP_NAME = "RowContextMenu";
constexpr const char* NO_DATA_TEXT =
    "No data available for the selected tracks or filters.";
constexpr const char* TRACK_ID_COLUMN_NAME  = "__trackId";
constexpr const char* STREAM_ID_COLUMN_NAME = "__streamTrackId";
constexpr const char* ID_COLUMN_NAME        = "__uuid";
constexpr const char* EVENT_ID_COLUMN_NAME  = "id";
constexpr const char* NAME_COLUMN_NAME      = "name";

MultiTrackTable::MultiTrackTable(DataProvider&                      dp,
                                 std::shared_ptr<TimelineSelection> timeline_selection,
                                 TableType                          table_type)
: InfiniteScrollTable(dp, table_type, NO_DATA_TEXT)
, m_timeline_selection(timeline_selection)
, m_defer_track_selection_changed(false)
, m_open_context_menu(false)
, m_group_by_selection_index(0)
{
    m_widget_name = (table_type == TableType::kEventTable)
                        ? GenUniqueName("Event Table")
                        : GenUniqueName("Sample Table");
}

MultiTrackTable::~MultiTrackTable() {}

void
MultiTrackTable::HandleTrackSelectionChanged()
{
    std::vector<uint64_t> tracks;
    double                start_ns;
    double                end_ns;
    m_timeline_selection->GetSelectedTracks(tracks);

    const TimelineModel& tlm = m_data_provider.DataModel().GetTimeline();
    // If no valid time range is provided, use the full trace range
    if(m_timeline_selection->HasValidTimeRangeSelection())
    {
        m_timeline_selection->GetSelectedTimeRange(start_ns, end_ns);
    }
    else
    {
        start_ns = tlm.GetStartTime();
        end_ns   = tlm.GetEndTime();
    }

    // loop trough tracks and filter out ones that don't match the table type
    std::vector<uint64_t> filtered_tracks;
    for(uint64_t track_id : tracks)
    {
        const TrackInfo* track_info = tlm.GetTrack(track_id);
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

    uint64_t request_id = GetRequestID();
    // Cancel pending requests.
    if(m_data_provider.IsRequestPending(request_id))
    {
        m_data_provider.CancelRequest(request_id);
    }
    // if no tracks match the table type, clear the table
    if(filtered_tracks.empty())
    {
        m_data_provider.DataModel().GetTables().ClearTable(m_table_type);
        fetch_result = true;
    }
    else
    {
        // Fetch table data for the selected tracks
        TableRequestParams table_params(
            m_req_table_type, filtered_tracks, {}, start_ns, end_ns,
            m_filter_options.where,
			m_filter_options.filter,
            m_filter_options.group_by.c_str(),
            m_filter_options.group_columns, {}, 0, m_fetch_chunk_size);

        fetch_result = m_data_provider.FetchTable(table_params);
    }

    if(!fetch_result)
    {
        spdlog::error("Failed to queue table request for tracks: {}",
                      filtered_tracks.size());
        // save this selection event to reprocess it later (it's ok to replace the
        // previous one as the new one reflects the current selection)
        m_defer_track_selection_changed = true;
    }
    else
    {
        // clear any pending track selection event
        m_defer_track_selection_changed = false;
    }
    // Update the selected tracks for this table type
    m_selected_tracks = std::move(filtered_tracks);
}

void
MultiTrackTable::Render()
{
    auto table_params = m_data_provider.DataModel().GetTables().GetTableParams(m_table_type);
    if(table_params)
    {
        ImGui::Text("Cached %llu to %llu of %llu events for %llu tracks",
                    table_params->m_start_row,
                    table_params->m_start_row + table_params->m_req_row_count,
                    m_data_provider.DataModel().GetTables().GetTableTotalRowCount(m_table_type),
                    table_params->m_track_ids.size());
    }

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
        if(ImGui::Combo("##group_by", &m_group_by_selection_index,
                        m_group_by_choices_ptr.data(),
                        static_cast<int>(m_group_by_choices_ptr.size())))
        {
            if(m_group_by_selection_index == 0)
            {
                m_pending_filter_options.group_by = "";
            }
            else
            {
                m_pending_filter_options.group_by =
                    m_group_by_choices_ptr[m_group_by_selection_index];
            }
        }

        if(m_pending_filter_options.group_by != "")
        {
            ImGui::SameLine();
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() -
                                 ImGui::GetFrameHeightWithSpacing() -
                                 ImGui::GetFontSize());
            if(XButton("clear_group"))
            {
                m_pending_filter_options.group_by = "";
                m_group_by_selection_index = 0;
            }
        }

        ImGui::SetNextItemAllowOverlap();
        ImGui::BeginDisabled(m_filter_options.group_by == "");
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
        m_filter_requested = true;
    }
    InfiniteScrollTable::Render();
    RenderContextMenu();
}

void
MultiTrackTable::Update()
{
    // Handle track selection changed event
    if(m_defer_track_selection_changed)
    {
        if(!m_data_provider.IsRequestPending(GetRequestID()))
        {
            // try to repocess the deferred track selection event
            spdlog::debug(
                "Reprocessing deferred track selection changed event for table type: {}",
                m_table_type == TableType::kEventTable ? "Event Table" : "Sample Table");
            HandleTrackSelectionChanged();
        }
    }

    if(m_data_changed)
    {
        const std::vector<std::string>& column_names =
            m_data_provider.DataModel().GetTables().GetTableHeader(m_table_type);

        if(m_table_type == TableType::kEventTable)
        {
            if(m_filter_options.group_by == "")
            {
                m_group_by_selection_index = 0;
                m_group_by_choices.clear();

                // Create a list of choices for the combo box for selecting the group
                // column. Populate the combo box with column names but filter out empty
                // and internal columns (those starting with '_')
                m_group_by_choices.reserve(column_names.size() + 1);
                m_group_by_choices.push_back("-- None --");

                // Filter out columns
                for(size_t i = 0; i < column_names.size(); i++)
                {
                    const auto& col = column_names[i];
                    if(col.empty() || col[0] == '_')
                    {
                        continue;  // Skip empty or internal columns
                    }
                    // skip event id column too
                    if(i == m_important_column_idxs[ImportantColumns::kDbEventId])
                    {
                        continue;
                    }
                    m_group_by_choices.push_back(col);
                }
            }
            else
            {
                std::string selected_option = m_filter_options.group_by;
                m_group_by_choices.resize(2);
                m_group_by_choices[1]             = selected_option;
                m_pending_filter_options.group_by = selected_option;
                m_group_by_selection_index        = 1;
            }
            m_group_by_choices_ptr.resize(m_group_by_choices.size());
            for(size_t i = 0; i < m_group_by_choices.size(); i++)
            {
                m_group_by_choices_ptr[i] = m_group_by_choices[i].c_str();
            }
        }
    }

    InfiniteScrollTable::Update();
}

void
MultiTrackTable::FormatData() const
{
    std::vector<FormattedColumnInfo>& formatted_column_data =
        m_data_provider.DataModel().GetTables().GetMutableFormattedTableData(m_table_type);

    // clear previous formatting info
    formatted_column_data.clear();
    formatted_column_data.resize(m_data_provider.DataModel().GetTables().GetTableHeader(m_table_type).size());
    InfiniteScrollTable::FormatTimeColumns();
}

void
MultiTrackTable::IndexColumns()
{
    const std::vector<std::string>& column_names =
        m_data_provider.DataModel().GetTables().GetTableHeader(m_table_type);
    // remember column index positions
    m_important_column_idxs =
        std::vector<size_t>(kNumImportantColumns, INVALID_UINT64_INDEX);
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
                m_important_column_idxs[kUUId] = i;
            }
            else if(col == EVENT_ID_COLUMN_NAME)
            {
                m_important_column_idxs[kDbEventId] = i;
            }
            else if(col == NAME_COLUMN_NAME)
            {
                m_important_column_idxs[kName] = i;
            }
        }
    }
    InfiniteScrollTable::IndexColumns();
}

void
MultiTrackTable::RowSelected(const ImGuiMouseButton mouse_button)
{
    if(mouse_button == ImGuiMouseButton_Right)
    {
        m_open_context_menu = true;
    }
    InfiniteScrollTable::RowSelected(mouse_button);
}

bool
MultiTrackTable::XButton(const char* id) const
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

void
MultiTrackTable::RenderContextMenu()
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
        if(ImGui::MenuItem("Copy Row Data", nullptr, false))
        {
            SelectedRowToClipboard();
        }
        else if(ImGui::MenuItem(m_table_type == TableType::kSampleTable ? "Go To Sample"
                                                                        : "Go To Event",
                                nullptr, false, target_track_id != INVALID_UINT64_INDEX))
        {
            SelectedRowNavigateEvent(m_important_column_idxs[kTrackId],
                                     m_important_column_idxs[kStreamId]);
        }
        else if(ImGui::MenuItem("Export To File", nullptr, false,
                                !m_data_provider.IsRequestPending(
                                    DataProvider::TABLE_EXPORT_REQUEST_ID)))
        {
            ExportToFile();
        }
        else if(ImGui::MenuItem("Copy Cell Data", nullptr, false))
        {
            const std::vector<std::vector<std::string>>& table_data =
                m_data_provider.DataModel().GetTables().GetTableData(m_table_type);
            if(m_selected_row < 0 || m_selected_row >= (int) table_data.size())
            {
                spdlog::warn("Selected row index out of bounds: {}", m_selected_row);
            }
            else if(m_selected_column < 0 ||
                    m_selected_column >= (int) table_data[m_selected_row].size())
            {
                spdlog::warn("Selected column index out of bounds: {}",
                             m_selected_column);
            }
            else
            {
                std::string cell_text = table_data[m_selected_row][m_selected_column];
                ImGui::SetClipboardText(cell_text.c_str());
                NotificationManager::GetInstance().Show("Cell data was copied",
                                                        NotificationLevel::Info);
            }

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

}  // namespace View
}  // namespace RocProfVis
