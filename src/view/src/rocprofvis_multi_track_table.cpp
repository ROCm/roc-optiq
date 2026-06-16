// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_common_defs.h"
#include "rocprofvis_multi_track_table.h"
#include "icons/rocprovfis_icon_defines.h"
#include "widgets/rocprofvis_gui_helpers.h"
#include "rocprofvis_settings_manager.h"
#include "rocprofvis_timeline_selection.h"
#include "spdlog/spdlog.h"

namespace RocProfVis
{
namespace View
{

constexpr const char* NO_DATA_TEXT =
    "No data available for the selected tracks or filters.";
constexpr const char* TRACK_ID_COLUMN_NAME  = "__trackId";
constexpr const char* STREAM_ID_COLUMN_NAME = "__streamTrackId";
constexpr const char* ID_COLUMN_NAME        = "__uuid";
constexpr const char* EVENT_ID_COLUMN_NAME  = "id";
constexpr const char* NAME_COLUMN_NAME      = "name";
constexpr const char* FOUND_ENTRIES_TEXT    = "Found %llu item(s) on %llu track(s)";

MultiTrackTable::MultiTrackTable(DataProvider& dp, TableType table_type,
                                 rocprofvis_controller_table_type_t request_table_type,
                                 uint64_t                           request_id,
                                 const std::function<const TablesModel&()> table_model,
                                 const std::function<TablesModel&()> table_model_mutable,
                                 bool                                display_filters,
                                 std::shared_ptr<TimelineSelection>  timeline_selection,
                                 uint64_t default_sort_column_index,
                                 rocprofvis_controller_sort_order_t default_sort_order,
                                 const std::string&                 friendly_name,
                                 const std::string&                 no_data_text)
: InfiniteScrollTable(dp, table_type, request_table_type, request_id, table_model,
                      table_model_mutable, timeline_selection, default_sort_column_index,
                      default_sort_order, friendly_name, NO_DATA_TEXT)
, m_retry_selection_fetch(false)
, m_display_filters(display_filters)
, m_group_by_selection_index(0)
{
    m_filter_store[0] = '\0';
}

MultiTrackTable::~MultiTrackTable() {}

void
MultiTrackTable::HandleTrackSelectionChanged(uint64_t track_id, bool selected)
{
    if(IncludeTrack(track_id) ||
       (track_id == TimelineSelection::INVALID_SELECTION_ID && !selected))
    {
        FetchSelectionData();
    }
}

void
MultiTrackTable::HandleTimeRangeSelectionChanged(double start_ns, double end_ns)
{
    (void) start_ns;
    (void) end_ns;
    FetchSelectionData();
}

void
MultiTrackTable::Render()
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::BeginChild("multitrack_table", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleVar();
    auto table_params = m_table_model().GetTableParams(m_table_type);
    if(m_display_filters || table_params)
    {
        const ImGuiStyle& style = ImGui::GetStyle();
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding,
                            m_settings.GetDefaultStyle().ChildRounding);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                            ImVec2(style.WindowPadding.x, style.WindowPadding.y * 0.75f));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, m_settings.GetColor(Colors::kBgPanel));
        ImGui::PushStyleColor(ImGuiCol_Border, m_settings.GetColor(Colors::kBorderColor));
        ImGui::BeginChild("##table_header", ImVec2(0.0f, 0.0f),
                          ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Borders |
                              ImGuiChildFlags_AlwaysUseWindowPadding);
        if(m_display_filters)
        {
            ImGui::PushStyleVar(
                ImGuiStyleVar_CellPadding,
                ImVec2(style.FramePadding.x, style.ItemSpacing.y * 0.35f));
            if(ImGui::BeginTable("##table_filter_controls", 3,
                                 ImGuiTableFlags_SizingStretchProp |
                                     ImGuiTableFlags_NoSavedSettings))
            {
                ImFont* icon_font = m_settings.GetFontManager().GetFont(FontType::kIcon);
                const ImGuiStyle& base_style = m_settings.GetDefaultStyle();
                const ImU32       input_bg   = m_settings.GetColor(Colors::kBgFrame);

                ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed,
                                        ImGui::GetFontSize() * 10.0f);
                ImGui::TableSetupColumn("control", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("submit", ImGuiTableColumnFlags_WidthFixed,
                                        ImGui::GetFontSize() * 7.5f);

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::AlignTextToFramePadding();
                ImGui::TextDisabled("Aggregate");

                ImGui::TableNextColumn();
                ImGui::SetNextItemWidth(-FLT_MIN);
                ImGui::PushFont(icon_font);
                const ImVec2 clear_icon_size = ImGui::CalcTextSize(ICON_X_CIRCLED);
                ImGui::PopFont();
                const float clear_icon_hit_width =
                    clear_icon_size.x + base_style.FramePadding.x * 2.0f;
                const float  combo_arrow_w = ImGui::GetFrameHeight();
                const ImVec2 combo_min     = ImGui::GetCursorScreenPos();
                const ImVec2 combo_max(combo_min.x + ImGui::CalcItemWidth(),
                                       combo_min.y + ImGui::GetFrameHeight());
                const ImVec2 clear_min(combo_max.x - combo_arrow_w - clear_icon_hit_width,
                                       combo_min.y);
                const ImVec2 clear_max(combo_max.x - combo_arrow_w, combo_max.y);
                const bool   has_group_by_selection = (m_group_by_selection_index != 0);
                const bool   clear_icon_hit_hovered =
                    has_group_by_selection &&
                    ImGui::IsMouseHoveringRect(clear_min, clear_max, false);

                PushComboStyles();
                ImGui::SetNextItemAllowOverlap();
                // Prevent the combo from also handling clicks in the overlaid clear-icon
                // hit area.
                if(clear_icon_hit_hovered)
                {
                    ImGui::BeginDisabled();
                }
                const bool group_by_changed =
                    ImGui::Combo("##group_by", &m_group_by_selection_index,
                                 m_group_by_choices_ptr.data(),
                                 static_cast<int>(m_group_by_choices_ptr.size()));
                if(clear_icon_hit_hovered)
                {
                    ImGui::EndDisabled();
                }
                PopComboStyles();

                if(has_group_by_selection)
                {
                    const ImVec2 clear_size(clear_max.x - clear_min.x,
                                            clear_max.y - clear_min.y);

                    ImGui::SetCursorScreenPos(clear_min);
                    ImGui::PushID("group_by_clear");
                    const bool clear_clicked =
                        ImGui::InvisibleButton("##clear_icon_hit", clear_size);
                    const bool clear_icon_hovered = ImGui::IsItemHovered();
                    ImGui::PopID();

                    ImDrawList* draw_list = ImGui::GetWindowDrawList();
                    if(clear_icon_hovered)
                    {
                        SetTooltipStyled("Clear");
                    }
                    const ImVec2 text_pos(
                        clear_min.x + (clear_icon_hit_width - clear_icon_size.x) * 0.5f,
                        clear_min.y +
                            ((clear_max.y - clear_min.y) - clear_icon_size.y) * 0.5f);
                    draw_list->AddText(icon_font, icon_font->LegacySize, text_pos,
                                       m_settings.GetColor(Colors::kTextMain),
                                       ICON_X_CIRCLED);

                    if(clear_clicked)
                    {
                        m_pending_filter_options.group_by = "";
                        m_group_by_selection_index        = 0;
                        ImGui::CloseCurrentPopup();
                    }
                }

                if(group_by_changed)
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

                ImGui::TableNextColumn();
                if(ImGui::Button("Submit", ImVec2(-FLT_MIN, 0.0f)))
                {
                    m_filter_requested  = true;
                    const bool grouping = (m_group_by_selection_index != 0);
                    if(!grouping && m_filter_store[0] != '\0')
                    {
                        // Reinstate the filter that was stashed when grouping was
                        // enabled.
                        snprintf(m_pending_filter_options.filter,
                                 IM_ARRAYSIZE(m_pending_filter_options.filter), "%s",
                                 m_filter_store);
                        m_filter_store[0] = '\0';
                    }
                    else if(grouping && m_pending_filter_options.filter[0] != '\0')
                    {
                        // Stash and clear the filter so it cannot fight the group-by
                        // query.
                        snprintf(m_filter_store, IM_ARRAYSIZE(m_filter_store), "%s",
                                 m_pending_filter_options.filter);
                        m_pending_filter_options.filter[0] = '\0';
                    }
                }

#ifdef ROCPROFVIS_DEVELOPER_MODE
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::AlignTextToFramePadding();
                ImGui::TextDisabled("Group columns");

                ImGui::TableNextColumn();
                ImGui::BeginDisabled(m_filter_options.group_by == "");
                const float group_cols_width = ImGui::GetContentRegionAvail().x;
                const std::pair<bool, bool> group_cols_input = InputTextWithClear(
                    "group_columns",
                    "name, COUNT(*) as num_invocations, AVG(duration) as avg_duration, "
                    "MIN(duration) as min_duration, MAX(duration) as max_duration",
                    m_pending_filter_options.group_columns,
                    IM_ARRAYSIZE(m_pending_filter_options.group_columns), icon_font,
                    input_bg, style, group_cols_width);
                if(group_cols_input.second)
                {
                    m_pending_filter_options.group_columns[0] = '\0';
                }
                ImGui::EndDisabled();
                ImGui::TableNextColumn();
#endif

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::AlignTextToFramePadding();
                ImGui::TextDisabled("Filter");

                ImGui::TableNextColumn();
                // Filter disabled when "group by" is selected
                ImGui::BeginDisabled(m_filter_options.group_by != "");
                const float filter_width = ImGui::GetContentRegionAvail().x;
                const std::pair<bool, bool> filter_input = InputTextWithClear(
                    "filters", "SQL WHERE comparisons", m_pending_filter_options.filter,
                    IM_ARRAYSIZE(m_pending_filter_options.filter), icon_font, input_bg,
                    style, filter_width);
                if(filter_input.second)
                {
                    m_pending_filter_options.filter[0] = '\0';
                }
                ImGui::EndDisabled();

                ImGui::EndTable();
            }
            ImGui::PopStyleVar();
        }
        if(table_params)
        {
            ImGui::TextDisabled(FOUND_ENTRIES_TEXT,
                                m_table_model().GetTableTotalRowCount(m_table_type),
                                table_params->m_track_ids.size());
#ifdef ROCPROFVIS_DEVELOPER_MODE
            ImGui::SameLine();
            ImGui::TextDisabled(
                " | Cached %llu to %llu entries", table_params->m_start_row,
                table_params->m_start_row + table_params->m_req_row_count);
#endif
        }
        ImGui::EndChild();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
    }

    InfiniteScrollTable::Render();

    ImGui::EndChild();
}

void
MultiTrackTable::Update()
{
    // Handle track selection changed event
    if(m_retry_selection_fetch)
    {
        if(!m_data_provider.IsRequestPending(m_request_id))
        {
            // Try to reprocess the deferred track selection event.
            spdlog::debug(
                "Reprocessing deferred track selection changed event for table: {}",
                m_widget_name);
            FetchSelectionData();
        }
    }

    if(m_data_changed)
    {
        const std::vector<std::string>& column_names =
            m_table_model().GetTableHeader(m_table_type);

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

    InfiniteScrollTable::Update();
}

bool
MultiTrackTable::IncludeTrack(uint64_t track_id) const
{
    bool             include = false;
    const TrackInfo* track_info =
        m_data_provider.DataModel().GetTimeline().GetTrack(track_id);
    if(track_info)
    {
        include = (track_info->track_type == kRPVControllerTrackTypeSamples &&
                   m_table_type == TableType::kSampleTable) ||
                  (track_info->track_type == kRPVControllerTrackTypeEvents &&
                   m_table_type == TableType::kEventTable);
    }
    return include;
}

void
MultiTrackTable::FormatData() const
{
    std::vector<FormattedColumnInfo>& formatted_column_data =
        m_table_model_mutable().GetMutableFormattedTableData(m_table_type);

    // clear previous formatting info
    formatted_column_data.clear();
    formatted_column_data.resize(m_table_model().GetTableHeader(m_table_type).size());
    InfiniteScrollTable::FormatTimeColumns();
}

void
MultiTrackTable::IndexColumns()
{
    const std::vector<std::string>& column_names =
        m_table_model().GetTableHeader(m_table_type);
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
        InfiniteScrollTable::SelectedRowContextMenu();
    }
    InfiniteScrollTable::RowSelected(mouse_button);
}

void
MultiTrackTable::FetchSelectionData()
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

    std::vector<uint64_t> included_tracks;
    for(uint64_t& track_id : tracks)
    {
        if(IncludeTrack(track_id))
        {
            included_tracks.push_back(track_id);
        }
    }

    bool fetch_result = false;

    // Cancel pending requests.
    if(m_data_provider.IsRequestPending(m_request_id))
    {
        m_data_provider.CancelRequest(m_request_id);
    }
    // if no tracks match the table type, clear the table
    if(included_tracks.empty())
    {
        m_table_model_mutable().ClearTable(m_table_type);
        fetch_result = true;
    }
    else
    {
        // Fetch table data for the selected tracks
        TableRequestParams table_params(
            m_request_table_type, included_tracks, {}, start_ns, end_ns,
            m_filter_options.where, m_filter_options.filter,
            m_filter_options.group_by.c_str(), m_filter_options.group_columns, {}, 0,
            m_fetch_chunk_size, m_sort_column_index, m_sort_order);

        fetch_result = m_data_provider.FetchTable(table_params);
    }

    if(!fetch_result)
    {
        spdlog::error("Failed to queue table request for tracks: {}",
                      included_tracks.size());
        // save this selection event to reprocess it later (it's ok to replace the
        // previous one as the new one reflects the current selection)
        m_retry_selection_fetch = true;
    }
    else
    {
        // clear any pending selection fetch
        m_retry_selection_fetch = false;
    }
    // Update the included tracks for this table type
    m_included_tracks = std::move(included_tracks);
}

bool
MultiTrackTable::XButton(const char* id) const
{
    bool clicked = RocProfVis::View::XButton(id, "Clear", &m_settings);
    return clicked;
}

}  // namespace View
}  // namespace RocProfVis
