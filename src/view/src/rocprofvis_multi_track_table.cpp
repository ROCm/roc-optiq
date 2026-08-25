// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_multi_track_table.h"
#include "icons/rocprovfis_icon_defines.h"
#include "rocprofvis_common_defs.h"
#include "rocprofvis_compare_panes.h"
#include "rocprofvis_settings_manager.h"
#include "rocprofvis_timeline_selection.h"
#include "rocprofvis_utils.h"
#include "spdlog/spdlog.h"
#include "widgets/rocprofvis_gui_helpers.h"

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
constexpr const char* FOUND_ENTRIES_TEXT    = "Found %llu item(s) on %llu track(s)";

constexpr const char* SHARED_APPLY_LABEL  = "Apply to Both";
constexpr const char* GROUP_BY_NONE_LABEL = "-- None --";
constexpr const char* SHARED_GROUP_BY_TOOLTIP =
    "Aggregate both tables by a column, or leave as -- None --. Columns tagged "
    "(A) or (B) exist on only that source and are not applied to the other.";
constexpr float SHARED_LABEL_GAP    = 2.0f;
constexpr float SHARED_COMBO_GLYPHS = 12.0f;

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
                                 const std::string&                 no_data_text,
                                 std::optional<uint64_t>            source_file_id)
: InfiniteScrollTable(dp, table_type, request_table_type, request_id, table_model,
                      table_model_mutable, timeline_selection, default_sort_column_index,
                      default_sort_order, friendly_name,
                      no_data_text.empty() ? NO_DATA_TEXT : no_data_text)
, m_source_file_id(source_file_id)
, m_display_filters(display_filters)
, m_display_summary(true)
, m_group_by_selection_index(0)
{
    m_filter_store[0] = '\0';
}

MultiTrackTable::~MultiTrackTable() {}

void
MultiTrackTable::ApplySharedFiltersFrom(const MultiTrackTable& source)
{
    m_pending_filter_options = source.m_pending_filter_options;
    // Commit now: the peer may have no table_params (empty pane), so
    // ProcessSortOrFilterRequest cannot copy pending -> applied, and
    // FetchSelectionData reads m_filter_options when tracks appear later.
    m_filter_options           = m_pending_filter_options;
    m_group_by_selection_index = source.m_group_by_selection_index;
    snprintf(m_filter_store, IM_ARRAYSIZE(m_filter_store), "%s", source.m_filter_store);
    m_filter_requested = true;
}

void
MultiTrackTable::SetFilterSubmitCallback(const FilterSubmitCallback& callback)
{
    m_filter_submit_callback = callback;
}

void
MultiTrackTable::ApplySharedSortFrom(const MultiTrackTable& source)
{
    if(m_sort_column_index == source.m_sort_column_index &&
       m_sort_order == source.m_sort_order)
    {
        return;
    }
    SetPendingSort(source.m_sort_column_index, source.m_sort_order);
}

void
MultiTrackTable::SetSortSyncCallback(const SortSyncCallback& callback)
{
    m_sort_sync_callback = callback;
}

void
MultiTrackTable::OnSortChanged()
{
    if(m_sort_sync_callback)
    {
        m_sort_sync_callback(*this);
    }
}

void
MultiTrackTable::SetDisplaySummary(bool display)
{
    m_display_summary = display;
}

void
MultiTrackTable::SetHeaderRenderer(std::function<void()> renderer)
{
    m_header_renderer = std::move(renderer);
    // The card drawn by RenderCard is the only frame around the title and body.
    SetDrawBorder(!m_header_renderer);
}

uint64_t
MultiTrackTable::GetTotalRowCount() const
{
    return m_table_model().GetTableTotalRowCount(m_table_type);
}

size_t
MultiTrackTable::GetIncludedTrackCount() const
{
    return m_included_tracks.size();
}

void
MultiTrackTable::RenderSharedFilterControls(const std::vector<std::string>& column_names,
                                            const std::vector<std::string>& column_labels)
{
    // Same shape as the per table form: a label column, the group by combo, the
    // apply button on the right, then a filter input filling the row.
    const ImGuiStyle& style     = ImGui::GetStyle();
    ImFont*           icon_font = m_settings.GetFontManager().GetFont(FontType::kIcon);
    const bool        grouping  = !m_pending_filter_options.group_by.empty();

    const float label_col =
        ImGui::CalcTextSize("Group by").x + style.ItemSpacing.x * SHARED_LABEL_GAP;
    const float apply_width =
        ImGui::CalcTextSize(SHARED_APPLY_LABEL).x + style.FramePadding.x * 2.0f;

    std::vector<std::string> names  = column_names;
    std::vector<std::string> labels = column_labels;
    if(labels.size() < names.size())
    {
        labels.resize(names.size());
        for(size_t i = 0; i < names.size(); i++)
        {
            if(labels[i].empty())
            {
                labels[i] = names[i];
            }
        }
    }
    else if(labels.size() > names.size())
    {
        labels.resize(names.size());
    }
    if(!m_pending_filter_options.group_by.empty())
    {
        bool found = false;
        for(size_t i = 0; i < names.size(); i++)
        {
            if(names[i] == m_pending_filter_options.group_by)
            {
                found = true;
                break;
            }
        }
        if(!found)
        {
            names.push_back(m_pending_filter_options.group_by);
            labels.push_back(m_pending_filter_options.group_by);
        }
    }

    std::vector<std::string> combo_items;
    combo_items.reserve(labels.size() + 1);
    combo_items.push_back(GROUP_BY_NONE_LABEL);
    for(size_t i = 0; i < labels.size(); i++)
    {
        combo_items.push_back(labels[i]);
    }
    std::vector<const char*> combo_items_ptr(combo_items.size());
    for(size_t i = 0; i < combo_items.size(); i++)
    {
        combo_items_ptr[i] = combo_items[i].c_str();
    }

    m_group_by_selection_index = 0;
    for(size_t i = 0; i < names.size(); i++)
    {
        if(names[i] == m_pending_filter_options.group_by)
        {
            m_group_by_selection_index = static_cast<int>(i + 1);
            break;
        }
    }

    ImGui::Spacing();

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Group by");
    ImGui::SameLine(label_col);
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * SHARED_COMBO_GLYPHS);
    PushComboStyles();
    if(ImGui::Combo("##shared_group_by", &m_group_by_selection_index,
                    combo_items_ptr.data(), static_cast<int>(combo_items_ptr.size())))
    {
        if(m_group_by_selection_index <= 0 ||
           static_cast<size_t>(m_group_by_selection_index) > names.size())
        {
            m_pending_filter_options.group_by = "";
        }
        else
        {
            m_pending_filter_options.group_by =
                names[static_cast<size_t>(m_group_by_selection_index) - 1];
        }
    }
    PopComboStyles();
    if(ImGui::IsItemHovered())
    {
        SetTooltipStyled(SHARED_GROUP_BY_TOOLTIP);
    }

    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetContentRegionMax().x - apply_width);
    if(ImGui::Button(SHARED_APPLY_LABEL, ImVec2(apply_width, 0.0f)))
    {
        SubmitFilters();
    }
    if(ImGui::IsItemHovered())
    {
        SetTooltipStyled("Apply the same aggregation and filter to sources A and B.");
    }

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Filter");
    ImGui::SameLine(label_col);
    const float filter_width = ImGui::GetContentRegionMax().x - ImGui::GetCursorPosX();
    ImGui::BeginDisabled(grouping);
    const std::pair<bool, bool> filter_input = InputTextWithClear(
        "shared_filters", grouping ? "Disabled while grouping" : "Filter both sources",
        m_pending_filter_options.filter, IM_ARRAYSIZE(m_pending_filter_options.filter),
        icon_font, m_settings.GetColor(Colors::kBgFrame), style, filter_width);
    if(filter_input.second)
    {
        m_pending_filter_options.filter[0] = '\0';
    }
    ImGui::EndDisabled();

    ImGui::Spacing();
}

void
MultiTrackTable::SubmitFilters()
{
    m_filter_requested  = true;
    const bool grouping = (m_group_by_selection_index != 0);
    if(!grouping && m_filter_store[0] != '\0')
    {
        // Reinstate the filter that was stashed when grouping was enabled.
        snprintf(m_pending_filter_options.filter,
                 IM_ARRAYSIZE(m_pending_filter_options.filter), "%s", m_filter_store);
        m_filter_store[0] = '\0';
    }
    else if(grouping && m_pending_filter_options.filter[0] != '\0')
    {
        // Stash and clear the filter so it cannot fight the group-by query.
        snprintf(m_filter_store, IM_ARRAYSIZE(m_filter_store), "%s",
                 m_pending_filter_options.filter);
        m_pending_filter_options.filter[0] = '\0';
    }
    if(m_filter_submit_callback)
    {
        m_filter_submit_callback(*this);
    }
}

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
MultiTrackTable::RenderCard(const ImVec2& size)
{
    ROCPROFVIS_ASSERT(m_header_renderer);
    BeginCompareCard("##source_card", m_settings, size,
                     ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    if(m_header_renderer)
    {
        m_header_renderer();
    }
    InfiniteScrollTable::Render();
    EndCompareCard();
}

void
MultiTrackTable::Render()
{
    // A header renderer means the table belongs to a compare card filling its pane.
    if(m_header_renderer)
    {
        RenderCard(ImVec2(0.0f, 0.0f));
        return;
    }
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::BeginChild("multitrack_table", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleVar();
    std::shared_ptr<TrackTableRequestParams> table_params =
        std::static_pointer_cast<TrackTableRequestParams>(
            m_table_model().GetTableParams(m_table_type));
    if(m_display_filters || (m_display_summary && table_params))
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
                    SubmitFilters();
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
        if(table_params && m_display_summary)
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
            // Make it explicit whether these results are scoped to a time-range
            // selection (accent) or the full trace (dim).
            if(m_timeline_selection)
            {
                ImGui::SameLine();
                ImGui::TextDisabled("|");
                ImGui::SameLine();
                if(m_timeline_selection->HasValidTimeRangeSelection())
                {
                    double sel_start = 0.0;
                    double sel_end   = 0.0;
                    m_timeline_selection->GetSelectedTimeRange(sel_start, sel_end);
                    const ImVec4 accent = ThemeColor(m_settings, Colors::kAccent);
                    ImGui::PushFont(m_settings.GetFontManager().GetFont(FontType::kIcon),
                                    0.0f);
                    ImGui::TextColored(accent, "%s", ICON_CROP);
                    ImGui::PopFont();
                    ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
                    ImGui::TextColored(accent, "Limited to time-range selection");
                    if(ImGui::IsItemHovered())
                    {
                        SetTooltipStyled(
                            "These results reflect the current time-range selection "
                            "(span: %s).",
                            nanosecond_to_formatted_str(
                                sel_end - sel_start,
                                m_settings.GetUserSettings().unit_settings.time_format,
                                true)
                                .c_str());
                    }
                }
                else
                {
                    ImGui::TextDisabled("Full trace");
                    if(ImGui::IsItemHovered())
                    {
                        SetTooltipStyled(
                            "No time-range selection - these results cover the full "
                            "trace.");
                    }
                }
            }
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
    if(m_data_changed)
    {
        if(!m_last_fetch_grouped)
        {
            RebuildEligibleGroupByColumns();
        }
        if(m_filter_options.group_by == "")
        {
            m_group_by_selection_index = 0;
            m_group_by_choices.clear();
            m_group_by_choices.push_back(GROUP_BY_NONE_LABEL);
            for(size_t i = 0; i < m_eligible_group_by_columns.size(); i++)
            {
                m_group_by_choices.push_back(m_eligible_group_by_columns[i]);
            }
        }
        else
        {
            std::string selected_option = m_filter_options.group_by;
            m_group_by_choices.resize(2);
            m_group_by_choices[0]             = GROUP_BY_NONE_LABEL;
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
                   m_request_table_type == kRPVControllerTableTypeSamples) ||
                  (track_info->track_type == kRPVControllerTrackTypeEvents &&
                   m_request_table_type == kRPVControllerTableTypeEvents);
        // In compare mode a table only aggregates the tracks of its own source.
        include = include &&
                  (!m_source_file_id || track_info->file_id == m_source_file_id.value());
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

    // Cancel pending requests.
    if(m_data_provider.IsRequestPending(m_request_id))
    {
        m_data_provider.CancelRequest(m_request_id);
    }
    // if no tracks match the table type, clear the table
    if(included_tracks.empty())
    {
        m_table_model_mutable().ClearTable(m_table_type);
        // There is nothing left to ask for, so drop a request still waiting its turn.
        ClearQueuedTableRequest();
    }
    else
    {
        FilterOptions request_filter = m_filter_options;
        AdjustFilterForRequest(request_filter);
        m_last_fetch_grouped = !request_filter.group_by.empty();
        // Fetch table data for the selected tracks. The request waits its turn when
        // the other compare source is holding the controller table.
        QueueTableRequest(std::make_shared<TrackTableRequestParams>(
            m_request_table_type, included_tracks, start_ns, end_ns,
            request_filter.where, request_filter.filter, request_filter.group_by.c_str(),
            request_filter.group_columns, 0, m_fetch_chunk_size, m_sort_column_index,
            m_sort_order, "", m_table_type, m_request_id));
    }
    // Update the included tracks for this table type
    m_included_tracks = std::move(included_tracks);
}

const std::vector<std::string>&
MultiTrackTable::EligibleGroupByColumns() const
{
    return m_eligible_group_by_columns;
}

void
MultiTrackTable::RebuildEligibleGroupByColumns()
{
    // Same skip rules as the per-table combo: empty names, internal '_'
    // columns, and the event id column.
    const std::vector<std::string>& column_names =
        m_table_model().GetTableHeader(m_table_type);
    m_eligible_group_by_columns.clear();
    m_eligible_group_by_columns.reserve(column_names.size());
    for(size_t i = 0; i < column_names.size(); i++)
    {
        const std::string& col = column_names[i];
        if(col.empty() || col[0] == '_')
        {
            continue;
        }
        if(i == m_important_column_idxs[ImportantColumns::kDbEventId])
        {
            continue;
        }
        m_eligible_group_by_columns.push_back(col);
    }
}

bool
MultiTrackTable::HasEligibleGroupByColumn(const std::string& name) const
{
    if(name.empty())
    {
        return false;
    }
    for(size_t i = 0; i < m_eligible_group_by_columns.size(); i++)
    {
        if(m_eligible_group_by_columns[i] == name)
        {
            return true;
        }
    }
    return false;
}

void
MultiTrackTable::AdjustFilterForRequest(FilterOptions& filter) const
{
    if(filter.group_by.empty())
    {
        return;
    }
    if(!HasEligibleGroupByColumn(filter.group_by))
    {
        filter.group_by.clear();
        filter.group_columns[0] = '\0';
    }
}

bool
MultiTrackTable::XButton(const char* id) const
{
    bool clicked = RocProfVis::View::XButton(id, "Clear", &m_settings);
    return clicked;
}

}  // namespace View
}  // namespace RocProfVis
