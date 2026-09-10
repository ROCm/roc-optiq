// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_common_defs.h"
#include "rocprofvis_multi_track_table.h"
#include "icons/rocprovfis_icon_defines.h"
#include "widgets/rocprofvis_gui_helpers.h"
#include "rocprofvis_settings_manager.h"
#include "rocprofvis_timeline_selection.h"
#include "rocprofvis_utils.h"
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
constexpr const char* FOUND_ENTRIES_TEXT    = "Found %llu item(s) on %llu track(s)";

MultiTrackTable::MultiTrackTable(DataProvider& dp, TableType table_type,
                                 rocprofvis_controller_table_type_t request_table_type,
                                 uint64_t                           request_id,
                                 const std::function<const TablesModel&()> table_model,
                                 const std::function<TablesModel&()> table_model_mutable,
                                 std::shared_ptr<TimelineSelection>  timeline_selection,
                                 int      available_filter_modes,
                                 uint64_t default_sort_column_index,
                                 rocprofvis_controller_sort_order_t default_sort_order,
                                 const std::string&                 friendly_name,
                                 const std::string&                 no_data_text)
: InfiniteScrollTable(dp, table_type, request_table_type, request_id, table_model,
                      table_model_mutable, timeline_selection, default_sort_column_index,
                      default_sort_order, friendly_name, NO_DATA_TEXT)
, m_group_by_selection_index(0)
, m_available_filter_modes(available_filter_modes)
, m_filter_mode(kNone)
, m_filter_advanced({ "", "", "" })
{
    if(m_available_filter_modes & kBasic)
    {
        m_filter_mode = kBasic;
    }
    else if(m_available_filter_modes & kAdvanced)
    {
        m_filter_mode = kAdvanced;
    }
    DisplayFilterRow(m_filter_mode == kBasic);
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
    std::shared_ptr<TrackTableRequestParams> table_params =
        std::static_pointer_cast<TrackTableRequestParams>(
            m_table_model().GetTableParams(m_table_type));
    if(table_params)
    {
        const ImGuiStyle& style = ImGui::GetStyle();
        ImFont* icon_font       = m_settings.GetFontManager().GetFont(FontType::kIcon);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding,
                            m_settings.GetDefaultStyle().ChildRounding);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                            ImVec2(style.WindowPadding.x, style.WindowPadding.y * 0.75f));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, m_settings.GetColor(Colors::kBgPanel));
        ImGui::PushStyleColor(ImGuiCol_Border, m_settings.GetColor(Colors::kBorderColor));
        ImGui::BeginChild("##status", ImVec2(0.0f, 0.0f),
                          ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Borders |
                              ImGuiChildFlags_AlwaysUseWindowPadding);
        ImGui::Text(FOUND_ENTRIES_TEXT,
                    m_table_model().GetTableTotalRowCount(m_table_type),
                    table_params->m_track_ids.size());
#ifdef ROCPROFVIS_DEVELOPER_MODE
        ImGui::SameLine();
        ImGui::Text(" | Cached %llu to %llu entries", table_params->m_start_row,
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
                            m_settings.GetUserSettings().unit_settings.time_format, true)
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
        if(m_available_filter_modes & (kBasic | kAdvanced))
        {
            ImGui::BeginDisabled(m_table_model().GetTableHeader(m_table_type).empty());
            ImGui::PushFont(icon_font);
            ImGui::SameLine(ImGui::GetContentRegionMax().x -
                                ImGui::CalcTextSize(ICON_ARROWS_CYCLE).x -
                                style.ItemSpacing.x - ImGui::CalcTextSize(ICON_FUNNEL).x,
                            0.0f);
            ImGui::PopFont();
            ImGui::BeginDisabled(m_filter_options.group_by.empty() &&
                                 m_filter_options.group_columns.empty() &&
                                 m_filter_options.filter.empty());
            if(IconButton(ICON_ARROWS_CYCLE, icon_font, ImVec2(0, 0), "Reset Filter",
                          true, ImVec2(0, 0), m_settings.GetColor(Colors::kBgPanel),
                          m_settings.GetColor(Colors::kBgPanel),
                          m_settings.GetColor(Colors::kBgPanel)))
            {
                ApplyFilter(true);
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            IconButton(ICON_FUNNEL, icon_font, ImVec2(0, 0), "Filter Mode", true,
                       ImVec2(0, 0), m_settings.GetColor(Colors::kBgPanel),
                       m_settings.GetColor(Colors::kBgPanel),
                       m_settings.GetColor(Colors::kBgPanel));
            if(ImGui::IsPopupOpen("filter_controls"))
            {
                ImGui::SetNextWindowPos(ImGui::GetItemRectMax() + style.WindowPadding,
                                        ImGuiCond_Always, ImVec2(1.0f, 0.0f));
            }
            if(ImGui::BeginPopupContextItem("filter_controls",
                                            ImGuiPopupFlags_MouseButtonLeft))
            {
                if(m_available_filter_modes & kBasic &&
                   ImGui::MenuItem("Basic", nullptr, m_filter_mode == kBasic))
                {
                    m_filter_mode = kBasic;
                    ApplyFilter(false);
                }
                if(m_available_filter_modes & kAdvanced &&
                   ImGui::MenuItem("Advanced", nullptr, m_filter_mode == kAdvanced))
                {
                    m_filter_mode = kAdvanced;
                    ApplyFilter(false);
                }
                if(ImGui::MenuItem("None", nullptr, m_filter_mode == kNone))
                {
                    m_filter_mode = kNone;
                    ApplyFilter(false);
                }
                ImGui::EndPopup();
            }
            ImGui::EndDisabled();
        }
        ImGui::EndChild();
        if(m_table_model().GetTableHeader(m_table_type).size() &&
           m_filter_mode == kAdvanced)
        {
            ImGui::BeginChild("##advanced_filters", ImVec2(0.0f, 0.0f),
                              ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Borders |
                                  ImGuiChildFlags_AlwaysUseWindowPadding);
            ImGui::BeginGroup();
            ImGui::AlignTextToFramePadding();
            ImGui::TextDisabled("Aggregate");
#ifdef ROCPROFVIS_DEVELOPER_MODE
            ImGui::AlignTextToFramePadding();
            ImGui::TextDisabled("Group Columns");
#endif
            ImGui::AlignTextToFramePadding();
            ImGui::TextDisabled("Filter");
            ImGui::SameLine();
            ImGui::EndGroup();
            ImGui::SameLine();
            ImGui::BeginGroup();
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x -
                                    ImGui::CalcTextSize("Submit").x -
                                    2.0f * style.FramePadding.x - style.ItemSpacing.x);
            ImGui::BeginGroup();
            ImGui::SetNextItemAllowOverlap();
            if(ImGui::Combo("##group_by", &m_group_by_selection_index,
                            m_group_by_choices_ptr.data(),
                            static_cast<int>(m_group_by_choices_ptr.size())))
            {
                if(m_group_by_selection_index == 0)
                {
                    m_filter_advanced.group_by.clear();
                }
                else
                {
                    m_filter_advanced.group_by =
                        m_group_by_choices[m_group_by_selection_index];
                }
            }
            if(m_group_by_selection_index != 0)
            {
                ImGui::SameLine();
                ImGui::SetCursorScreenPos(ImVec2(
                    ImGui::GetItemRectMax().x - 2 * style.FramePadding.x -
                        ImGui::CalcTextSize(ICON_X_CIRCLED).x - ImGui::GetFrameHeight(),
                    ImGui::GetCursorScreenPos().y));
                if(XButton("clear_group", "Clear", &m_settings))
                {
                    m_filter_advanced.group_by.clear();
                    m_group_by_selection_index = 0;
                }
            }
            ImGui::EndGroup();
#ifdef ROCPROFVIS_DEVELOPER_MODE
            if(InputTextWithClear(
                   "group_columns",
                   "name, COUNT(*) as num_invocations, AVG(duration) as avg_duration, "
                   "MIN(duration) as min_duration, MAX(duration) as max_duration",
                   m_filter_advanced.group_columns, icon_font,
                   m_settings.GetColor(Colors::kBgFrame), style,
                   ImGui::GetItemRectSize().x)
                   .second)
            {
                m_filter_advanced.group_columns.clear();
            }
#endif
            ImGui::PushFont(icon_font);
            float import_width =
                ImGui::CalcTextSize(ICON_ARROW_IN_BOX).x + 2.0f * style.FramePadding.x;
            ImGui::PopFont();
            ImGui::BeginDisabled(!m_filter_options.group_by.empty());
            if(InputTextWithClear(
                   "filters", "SQL WHERE comparisons", m_filter_advanced.filter,
                   icon_font, m_settings.GetColor(Colors::kBgFrame), style,
                   ImGui::GetItemRectSize().x - style.ItemSpacing.x - import_width)
                   .second)
            {
                m_filter_advanced.filter.clear();
            }
            ImGui::SameLine();
            ImGui::BeginDisabled(ActiveFilterRowClause().empty());
            if(IconButton(ICON_ARROW_IN_BOX, icon_font, ImVec2(0, 0), "Import from Basic",
                          false, style.FramePadding, m_settings.GetColor(Colors::kButton),
                          m_settings.GetColor(Colors::kButtonHovered),
                          m_settings.GetColor(Colors::kButtonActive)))
            {
                m_filter_advanced.filter = ActiveFilterRowClause();
            }
            ImGui::EndDisabled();
            ImGui::EndDisabled();
            ImGui::EndGroup();
            ImGui::SameLine();
            if(ImGui::Button("Submit", ImVec2(0.0f, ImGui::GetItemRectSize().y)))
            {
                ApplyFilter(false);
            }
            ImGui::EndChild();
        }
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
            m_group_by_choices[1]      = selected_option;
            m_group_by_selection_index = 1;
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
MultiTrackTable::UpdateFetchParams(std::shared_ptr<TableRequestParams>& params) const
{
    if(!params)
    {
        params = std::make_shared<TrackTableRequestParams>();
    }
    InfiniteScrollTable::UpdateFetchParams(params);
    if(params)
    {
        std::shared_ptr<TrackTableRequestParams> track_params =
            std::static_pointer_cast<TrackTableRequestParams>(params);
        double start_ns;
        double end_ns;
        // If no valid time range is provided, use the full trace range
        if(!m_timeline_selection->GetSelectedTimeRange(start_ns, end_ns))
        {
            const TimelineModel& tlm = m_data_provider.DataModel().GetTimeline();
            start_ns                 = tlm.GetStartTime();
            end_ns                   = tlm.GetEndTime();
        }
        track_params->m_track_ids = m_included_tracks;
        params->m_start_ts        = start_ns;
        params->m_end_ts          = end_ns;
    }
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
    m_timeline_selection->GetSelectedTracks(tracks);
    m_included_tracks.clear();
    for(uint64_t& track_id : tracks)
    {
        if(IncludeTrack(track_id))
        {
            m_included_tracks.push_back(track_id);
        }
    }

    // if no tracks match the table type, clear the table
    if(m_included_tracks.empty())
    {
        m_table_model_mutable().ClearTable(m_table_type);
    }
    else
    {
        // Fetch table data for the selected tracks
        RequestFetch();
    }
}

void
MultiTrackTable::ApplyFilter(bool reset)
{
    DisplayFilterRow(m_filter_mode == kBasic);
    switch(m_filter_mode)
    {
        case kBasic:
        {
            if(reset)
            {
                ResetFilterRow();
            }
            else
            {
                RequestFilter();
            }
            break;
        }
        case kAdvanced:
        {
            if(reset)
            {
                m_filter_advanced.group_by.clear();
                m_filter_advanced.group_columns.clear();
                m_filter_advanced.filter.clear();
                m_group_by_selection_index = 0;
                m_filter_store.clear();
                m_filter_options = m_filter_advanced;
            }
            else
            {
                const bool grouping = (m_group_by_selection_index != 0);
                if(!grouping && !m_filter_store.empty())
                {
                    // Reinstate the filter that was stashed when grouping was
                    // enabled.
                    m_filter_advanced.filter = m_filter_store;
                    m_filter_store.clear();
                }
                else if(grouping && !m_filter_advanced.filter.empty())
                {
                    // Stash and clear the filter so it cannot fight the group-by
                    // query.
                    m_filter_store = m_filter_advanced.filter;
                    m_filter_advanced.filter.clear();
                }
                m_filter_options = m_filter_advanced;
            }
            RequestFilter();
            break;
        }
        case kNone:
        {
            if(!reset)
            {
                m_filter_options.group_by.clear();
                m_filter_options.group_columns.clear();
                m_filter_options.filter.clear();
                RequestFilter();
            }
            break;
        }
        default:
        {
            break;
        }
    }
}

}  // namespace View
}  // namespace RocProfVis
