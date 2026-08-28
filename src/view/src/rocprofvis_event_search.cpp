// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_event_search.h"
#include "icons/rocprovfis_icon_defines.h"
#include "rocprofvis_common_defs.h"
#include "rocprofvis_settings_manager.h"
#include "rocprofvis_timeline_selection.h"
#include "widgets/rocprofvis_gui_helpers.h"
#include "widgets/rocprofvis_notification_manager.h"

namespace RocProfVis
{
namespace View
{

constexpr uint64_t    MAX_RESULTS_DISPLAYED           = 5;
constexpr const char* TRACK_ID_COLUMN_NAME            = "__trackId";
constexpr const char* STREAM_ID_COLUMN_NAME           = "__streamTrackId";
constexpr const char* ID_COLUMN_NAME                  = "__uuid";
constexpr const char* EVENT_ID_COLUMN_NAME            = "id";
constexpr const char* NAME_COLUMN_NAME                = "name";
constexpr const char* CATEGORY_COLUMN_NAME            = "category";
constexpr bool        DEFAULT_INCLUDE_SUBSTRINGS      = true;
constexpr bool        DEFAULT_INCLUDE_CATEGORY        = false;
constexpr bool        DEFAULT_PARTIAL_MATCHING        = false;
constexpr bool        DEFAULT_RESPECT_RANGE_SELECTION = false;

EventSearch::EventSearch(DataProvider&                      dp,
                         std::shared_ptr<TimelineSelection> timeline_selection)
: InfiniteScrollTable(
      dp, TableType::kEventSearchTable, kRPVControllerTableTypeSearchResults,
      DataProvider::EVENT_SEARCH_REQUEST_ID,
      [&dp]() -> const TablesModel& { return dp.DataModel().GetTables(); },
      [&dp]() -> TablesModel& { return dp.DataModel().GetTables(); }, timeline_selection,
      1, kRPVControllerSortOrderAscending, "Event Search Table", "")
, m_show_options(false)
, m_include_substrings(DEFAULT_INCLUDE_SUBSTRINGS)
, m_include_category(DEFAULT_INCLUDE_CATEGORY)
, m_partial_matching(DEFAULT_PARTIAL_MATCHING)
, m_respect_range_selection(DEFAULT_RESPECT_RANGE_SELECTION)
, m_should_open(false)
, m_should_close(false)
, m_is_open(false)
, m_focus_text_input(false)
, m_search_deferred(false)
, m_searched(false)
, m_options_changed(false)
, m_advanced_active(false)
, m_width(1000.0f)
, m_time_range_changed_token(EventManager::InvalidSubscriptionToken)
{
    m_time_range_changed_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kTimelineTimeRangeChanged),
        [this](std::shared_ptr<RocEvent> e) {
            if(e && e->GetSourceId() == m_data_provider.GetTraceFilePath() &&
               m_respect_range_selection)
            {
                m_search_deferred = m_searched;
            }
        });
}

EventSearch::~EventSearch()
{
    EventManager::GetInstance()->Unsubscribe(
        static_cast<int>(RocEvents::kTimelineTimeRangeChanged),
        m_time_range_changed_token);
}

void
EventSearch::Update()
{
    if(m_is_open && m_search_deferred && !m_data_provider.IsRequestPending(m_request_id))
    {
        Search();
        m_search_deferred = false;
    }
    if(m_options_changed)
    {
        m_advanced_active = m_include_substrings != DEFAULT_INCLUDE_SUBSTRINGS ||
                            m_include_category != DEFAULT_INCLUDE_CATEGORY ||
                            m_partial_matching != DEFAULT_PARTIAL_MATCHING ||
                            m_respect_range_selection != DEFAULT_RESPECT_RANGE_SELECTION;
        m_options_changed = false;
    }
    if(m_data_changed)
    {
        m_hidden_column_indices.clear();
        const std::vector<std::string>& column_names =
            m_table_model().GetTableHeader(m_table_type);
        for(size_t i = 0; i < column_names.size(); i++)
        {
            if(i != m_important_column_idxs[kDbEventId] &&
               i != m_important_column_idxs[kCategory] &&
               i != m_important_column_idxs[kName] &&
               i != m_time_column_indices[kTimeStartNs] &&
               i != m_time_column_indices[kDurationNs])
            {
                m_hidden_column_indices.push_back(static_cast<int>(i));
            }
        }
    }
    InfiniteScrollTable::Update();
}

void
EventSearch::Render()
{
    if(m_should_open)
    {
        ImGui::OpenPopup("event_search", ImGuiPopupFlags_NoReopen);
        m_should_open  = false;
        m_should_close = false;
    }

    m_is_open = ImGui::IsPopupOpen("event_search");
    if(m_is_open)
    {
        const TablesModel& tm    = m_table_model();
        const ImGuiStyle&  style = m_settings.GetDefaultStyle();
        ImGui::SetNextWindowPos(
            ImVec2(ImGui::GetItemRectMin().x,
                   ImGui::GetItemRectMax().y + ImGui::GetStyle().WindowPadding.y));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, style.ChildRounding);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                            ImVec2(style.WindowPadding.x, style.WindowPadding.y * 0.75f));
        ImGui::SetNextWindowSize(ImVec2(m_width, 0.0f));
        if(ImGui::BeginPopup("event_search", ImGuiWindowFlags_NoFocusOnAppearing))
        {
            const uint64_t& row_count = tm.GetTableTotalRowCount(m_table_type);
            if(m_data_provider.IsRequestPending(m_request_id) || row_count > 0)
            {
                ImGui::SetNextWindowSize(
                    ImVec2(0.0f, (m_horizontal_scroll ? style.ScrollbarSize : 0.0f) +
                                     (1.0f + std::min(row_count, MAX_RESULTS_DISPLAYED)) *
                                         TableRowHeight()));
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
                InfiniteScrollTable::Render();
                ImGui::PopStyleVar();
            }
            auto table_params = tm.GetTableParams(m_table_type);
            if(table_params)
            {
                ImGui::AlignTextToFramePadding();
#ifdef ROCPROFVIS_DEVELOPER_MODE
                ImGui::Text("Showing %llu to %llu of %llu result(s)",
                            table_params->m_start_row,
                            table_params->m_start_row + table_params->m_req_row_count,
                            tm.GetTableTotalRowCount(m_table_type));
#else
                ImGui::Text("Showing %llu result(s)",
                            tm.GetTableTotalRowCount(m_table_type));
#endif
            }
            if(m_show_options)
            {
                ImGui::SeparatorText("Advanced");
                ImGui::PushFont(m_settings.GetFontManager().GetFont(FontType::kIcon));
                ImGui::SameLine(ImGui::GetContentRegionMax().x -
                                style.SeparatorTextPadding.x -
                                ImGui::CalcTextSize(ICON_ARROWS_CYCLE).x -
                                2.0f * style.FramePadding.x);
                ImGui::PopFont();
                if(IconButton(ICON_ARROWS_CYCLE,
                              m_settings.GetFontManager().GetFont(FontType::kIcon),
                              ImVec2(0, 0), "Reset to Defaults", false,
                              style.FramePadding, m_settings.GetColor(Colors::kBgPanel),
                              m_settings.GetColor(Colors::kBgPanel),
                              m_settings.GetColor(Colors::kBgPanel)) &&
                   m_advanced_active)
                {
                    ResetOptions();
                    m_options_changed = true;
                    m_search_deferred = m_searched;
                }
                ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding,
                                    m_settings.GetDefaultStyle().ChildRounding);
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                                    m_settings.GetDefaultStyle().ItemSpacing);
                ImGui::PushStyleColor(ImGuiCol_ChildBg,
                                      m_settings.GetColor(Colors::kBgPanel));
                ImGui::PushStyleColor(ImGuiCol_Border,
                                      m_settings.GetColor(Colors::kBorderColor));
                ImGui::BeginChild("advanced_search", ImVec2(0, 0),
                                  ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);
                ImGui::BeginGroup();
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted("Match Criteria:");
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted("Multiple Terms:");
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted("Search Range:");
                ImGui::EndGroup();
                ImGui::SameLine();
                ImGui::BeginGroup();
                ImGui::GetWindowDrawList()->AddRectFilled(
                    ImGui::GetCursorScreenPos(),
                    ImGui::GetCursorScreenPos() +
                        ImVec2(ImGui::CalcTextSize("Contains").x +
                                   ImGui::CalcTextSize("Equals").x +
                                   4.0f * m_settings.GetDefaultStyle().FramePadding.x,
                               ImGui::GetFrameHeight()),
                    m_settings.GetColor(Colors::kButton),
                    m_settings.GetDefaultStyle().FrameRounding);
                if(ColoredButton(
                       "Contains",
                       m_settings.GetColor(m_include_substrings ? Colors::kAccent
                                                                : Colors::kButton),
                       m_settings.GetColor(m_include_substrings ? Colors::kAccent
                                                                : Colors::kButtonHovered),
                       m_settings.GetColor(m_include_substrings ? Colors::kAccent
                                                                : Colors::kButtonActive),
                       m_settings.GetColor(m_include_substrings ? Colors::kTextOnAccent
                                                                : Colors::kTextMain),
                       "Include events whose names contain search term(s).") &&
                   !m_include_substrings)
                {
                    m_include_substrings = true;
                    m_options_changed    = true;
                    m_search_deferred    = m_searched;
                }
                ImGui::SameLine(0.0f, 0.0f);
                if(ColoredButton(
                       "Equals",
                       m_settings.GetColor(!m_include_substrings ? Colors::kAccent
                                                                 : Colors::kButton),
                       m_settings.GetColor(!m_include_substrings
                                               ? Colors::kAccent
                                               : Colors::kButtonHovered),
                       m_settings.GetColor(!m_include_substrings ? Colors::kAccent
                                                                 : Colors::kButtonActive),
                       m_settings.GetColor(!m_include_substrings ? Colors::kTextOnAccent
                                                                 : Colors::kTextMain),
                       "Include events whose names equal search term(s).") &&
                   m_include_substrings)
                {
                    m_include_substrings = false;
                    m_options_changed    = true;
                    m_search_deferred    = m_searched;
                }
                ImGui::GetWindowDrawList()->AddRectFilled(
                    ImGui::GetCursorScreenPos(),
                    ImGui::GetCursorScreenPos() +
                        ImVec2(ImGui::CalcTextSize("AND").x +
                                   ImGui::CalcTextSize("OR").x +
                                   4.0f * m_settings.GetDefaultStyle().FramePadding.x,
                               ImGui::GetFrameHeight()),
                    m_settings.GetColor(Colors::kButton),
                    m_settings.GetDefaultStyle().FrameRounding);
                if(ColoredButton(
                       "AND",
                       m_settings.GetColor(!m_partial_matching ? Colors::kAccent
                                                               : Colors::kButton),
                       m_settings.GetColor(!m_partial_matching ? Colors::kAccent
                                                               : Colors::kButtonHovered),
                       m_settings.GetColor(!m_partial_matching ? Colors::kAccent
                                                               : Colors::kButtonActive),
                       m_settings.GetColor(!m_partial_matching ? Colors::kTextOnAccent
                                                               : Colors::kTextMain),
                       "Include events whose names match all search terms.") &&
                   m_partial_matching)
                {
                    m_partial_matching = false;
                    m_options_changed  = true;
                    m_search_deferred  = m_searched;
                }
                ImGui::SameLine(0.0f, 0.0f);
                if(ColoredButton(
                       "OR",
                       m_settings.GetColor(m_partial_matching ? Colors::kAccent
                                                              : Colors::kButton),
                       m_settings.GetColor(m_partial_matching ? Colors::kAccent
                                                              : Colors::kButtonHovered),
                       m_settings.GetColor(m_partial_matching ? Colors::kAccent
                                                              : Colors::kButtonActive),
                       m_settings.GetColor(m_partial_matching ? Colors::kTextOnAccent
                                                              : Colors::kTextMain),
                       "Include events whose names match any search terms.") &&
                   !m_partial_matching)
                {
                    m_partial_matching = true;
                    m_options_changed  = true;
                    m_search_deferred  = m_searched;
                }
                ImGui::GetWindowDrawList()->AddRectFilled(
                    ImGui::GetCursorScreenPos(),
                    ImGui::GetCursorScreenPos() +
                        ImVec2(ImGui::CalcTextSize("Whole Trace").x +
                                   ImGui::CalcTextSize("Selected Time Range").x +
                                   4.0f * m_settings.GetDefaultStyle().FramePadding.x,
                               ImGui::GetFrameHeight()),
                    m_settings.GetColor(Colors::kButton),
                    m_settings.GetDefaultStyle().FrameRounding);
                if(ColoredButton("Whole Trace",
                                 m_settings.GetColor(!m_respect_range_selection
                                                         ? Colors::kAccent
                                                         : Colors::kButton),
                                 m_settings.GetColor(!m_respect_range_selection
                                                         ? Colors::kAccent
                                                         : Colors::kButtonHovered),
                                 m_settings.GetColor(!m_respect_range_selection
                                                         ? Colors::kAccent
                                                         : Colors::kButtonActive),
                                 m_settings.GetColor(!m_respect_range_selection
                                                         ? Colors::kTextOnAccent
                                                         : Colors::kTextMain),
                                 "Consider all events.") &&
                   m_respect_range_selection)
                {
                    m_respect_range_selection = false;
                    m_options_changed         = true;
                    m_search_deferred =
                        m_searched && m_timeline_selection &&
                        m_timeline_selection->HasValidTimeRangeSelection();
                }
                ImGui::SameLine(0.0f, 0.0f);
                if(ColoredButton("Selected Time Range",
                                 m_settings.GetColor(m_respect_range_selection
                                                         ? Colors::kAccent
                                                         : Colors::kButton),
                                 m_settings.GetColor(m_respect_range_selection
                                                         ? Colors::kAccent
                                                         : Colors::kButtonHovered),
                                 m_settings.GetColor(m_respect_range_selection
                                                         ? Colors::kAccent
                                                         : Colors::kButtonActive),
                                 m_settings.GetColor(m_respect_range_selection
                                                         ? Colors::kTextOnAccent
                                                         : Colors::kTextMain),
                                 "Consider events inside the selected time "
                                 "range (when available).") &&
                   !m_respect_range_selection)
                {
                    m_respect_range_selection = true;
                    m_options_changed         = true;
                    m_search_deferred =
                        m_searched && m_timeline_selection &&
                        m_timeline_selection->HasValidTimeRangeSelection();
                }
                ImGui::EndGroup();
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));
                if(ImGui::Checkbox("Search Event Categories", &m_include_category))
                {
                    m_options_changed = true;
                    m_search_deferred = m_searched;
                }
                if(BeginItemTooltipStyled())
                {
                    ImGui::TextUnformatted("Consider event categories (when "
                                           "available) in addition to event names.");
                    EndTooltipStyled();
                }
                ImGui::PopStyleVar();
                ImGui::EndChild();
                ImGui::PopStyleColor(2);
                ImGui::PopStyleVar(2);
            }
            if(m_should_close)
            {
                ImGui::CloseCurrentPopup();
                m_should_close = false;
            }
            ImGui::EndPopup();
        }
        ImGui::PopStyleVar(2);
    }
}

void
EventSearch::Show()
{
    if(!m_is_open)
    {
        m_should_open = true;
    }
}

void
EventSearch::Search()
{
    size_t input_length = m_text_input.size();
    if(input_length > 0)
    {
        bool valid = false;
        m_terms.clear();
        if(m_text_input[0] == '"')
        {
            std::string term;
            bool        open_quote = true;
            for(size_t i = 1; i < input_length; i++)
            {
                if(m_text_input[i] == '"')
                {
                    if(open_quote)
                    {
                        if(!term.empty())
                        {
                            m_terms.emplace_back(term);
                            term = "";
                        }
                        open_quote = false;
                    }
                    else
                    {
                        open_quote = true;
                    }
                }
                else
                {
                    term += m_text_input[i];
                }
            }
            valid = !m_terms.empty() && !open_quote;
        }
        else
        {
            m_terms.emplace_back(m_text_input);
            valid = true;
        }
        if(valid)
        {
            RequestFetch();
            m_searched    = true;
            m_should_open = true;
        }
        else
        {
            NotificationManager::GetInstance().Show("Invalid search term.",
                                                    NotificationLevel::Error);
        }
    }
}

void
EventSearch::Clear()
{
    m_data_provider.CancelRequest(m_request_id);
    m_table_model_mutable().ClearTable(m_table_type);
    m_terms.clear();
    m_text_input.clear();
    m_searched = false;
    if(!m_show_options)
    {
        m_should_close = true;
    }
}

void
EventSearch::ToggleOptions()
{
    if(m_is_open)
    {
        m_show_options = !m_show_options;
        if(!m_show_options && !m_searched)
        {
            m_should_close = true;
        }
    }
    else
    {
        m_show_options = true;
        Show();
    }
}

void
EventSearch::SetWidth(float width)
{
    m_width = std::max(0.0f, width);
}

std::string&
EventSearch::TextInput()
{
    return m_text_input;
}

bool
EventSearch::FocusTextInput()
{
    if(m_focus_text_input)
    {
        m_focus_text_input = false;
        return true;
    }
    else
    {
        m_focus_text_input = m_is_open && !ImGui::IsItemFocused() &&
                             ImGui::IsMouseHoveringRect(ImGui::GetItemRectMin(),
                                                        ImGui::GetItemRectMax()) &&
                             ImGui::IsMouseClicked(ImGuiMouseButton_Left);
        return m_focus_text_input;
    }
}

bool
EventSearch::Searched() const
{
    return m_searched;
}

bool
EventSearch::Advanced() const
{
    return m_advanced_active;
}

float
EventSearch::Width() const
{
    return m_width;
}

void
EventSearch::ResetOptions()
{
    m_include_substrings      = DEFAULT_INCLUDE_SUBSTRINGS;
    m_include_category        = DEFAULT_INCLUDE_CATEGORY;
    m_partial_matching        = DEFAULT_PARTIAL_MATCHING;
    m_respect_range_selection = DEFAULT_RESPECT_RANGE_SELECTION;
}

void
EventSearch::UpdateFetchParams(std::shared_ptr<TableRequestParams>& params) const
{
    if(!params)
    {
        params = std::make_shared<EventSearchRequestParams>();
    }
    InfiniteScrollTable::UpdateFetchParams(params);
    if(params)
    {
        std::shared_ptr<EventSearchRequestParams> search_params =
            std::static_pointer_cast<EventSearchRequestParams>(params);
        const TimelineModel& timeline = m_data_provider.DataModel().GetTimeline();
        double               start_ts;
        double               end_ts;
        if(!(m_respect_range_selection && m_timeline_selection &&
             m_timeline_selection->GetSelectedTimeRange(start_ts, end_ts)))
        {
            start_ts = timeline.GetStartTime();
            end_ts   = timeline.GetEndTime();
        }
        search_params->m_op_types             = { kRocProfVisDmOperationLaunch,
                                                  kRocProfVisDmOperationDispatch,
                                                  kRocProfVisDmOperationMemoryCopy,
                                                  kRocProfVisDmOperationMemoryAllocate,
                                                  kRocProfVisDmOperationLaunchSample };
        search_params->m_string_table_filters = m_terms;
        search_params->m_include_substrings   = m_include_substrings;
        search_params->m_include_category     = m_include_category;
        search_params->m_partial_matching     = m_partial_matching;
        params->m_start_ts                    = start_ts;
        params->m_end_ts                      = end_ts;
    }
}

void
EventSearch::FormatData() const
{
    TablesModel& tm = m_table_model_mutable();

    std::vector<FormattedColumnInfo>& formatted_column_data =
        tm.GetMutableFormattedTableData(m_table_type);
    formatted_column_data.clear();
    formatted_column_data.resize(tm.GetTableHeader(m_table_type).size());
    InfiniteScrollTable::FormatTimeColumns();
}

void
EventSearch::IndexColumns()
{
    const std::vector<std::string>& column_names =
        m_table_model().GetTableHeader(m_table_type);
    m_important_column_idxs =
        std::vector<size_t>(kNumImportantColumns, INVALID_UINT64_INDEX);
    m_hidden_column_indices.clear();
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
            else if(m_include_category && col == CATEGORY_COLUMN_NAME)
            {
                m_important_column_idxs[kCategory] = i;
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
EventSearch::RowSelected(const ImGuiMouseButton mouse_button)
{
    if(mouse_button == ImGuiMouseButton_Left)
    {
        SelectedRowNavigateEvent(m_important_column_idxs[kTrackId],
                                 m_important_column_idxs[kStreamId]);
        m_should_close = true;
    }
    else if(mouse_button == ImGuiMouseButton_Right)
    {
        SelectedRowContextMenu();
    }
    InfiniteScrollTable::RowSelected(mouse_button);
}

}  // namespace View
}  // namespace RocProfVis
