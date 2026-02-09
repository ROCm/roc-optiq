// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_event_search.h"
#include "icons/rocprovfis_icon_defines.h"
#include "rocprofvis_event_manager.h"
#include "rocprofvis_settings_manager.h"
#include "rocprofvis_timeline_selection.h"
#include "rocprofvis_utils.h"
#include "spdlog/spdlog.h"
#include "widgets/rocprofvis_notification_manager.h"

namespace RocProfVis
{
namespace View
{

constexpr uint64_t    MAX_RESULTS_DISPLAYED = 5;
constexpr const char* TRACK_ID_COLUMN_NAME  = "__trackId";
constexpr const char* STREAM_ID_COLUMN_NAME = "__streamTrackId";
constexpr const char* ID_COLUMN_NAME        = "__uuid";
constexpr const char* EVENT_ID_COLUMN_NAME  = "id";
constexpr const char* NAME_COLUMN_NAME      = "name";

EventSearch::EventSearch(DataProvider& dp, std::shared_ptr<TimelineSelection> timeline_selection)
: InfiniteScrollTable(dp, TableType::kEventSearchTable)
, m_should_open(false)
, m_should_close(false)
, m_open_context_menu(false)
, m_focus_text_input(false)
, m_search_deferred(false)
, m_searched(false)
, m_width(1000.0f)
, m_text_input("\0")
, m_timeline_selection(timeline_selection)
{
    m_widget_name = GenUniqueName("Event Search Table");
}

void
EventSearch::Update()
{
    if(m_search_deferred && !m_data_provider.IsRequestPending(GetRequestID()))
    {
        Search();
    }
    if(m_data_changed)
    {
        m_hidden_column_indices.clear();
        const std::vector<std::string>& column_names =
            m_data_provider.DataModel().GetTables().GetTableHeader(m_table_type);
        for(size_t i = 0; i < column_names.size(); i++)
        {
            if(i != m_important_column_idxs[kDbEventId] && i != m_important_column_idxs[kName] &&
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

    if(Open())
    {
        const TablesModel& tm = m_data_provider.DataModel().GetTables();
        ImGui::SetNextWindowSize(ImVec2(m_width, Height()));
        ImGui::SetNextWindowPos(
            ImVec2(ImGui::GetItemRectMin().x,
                   ImGui::GetItemRectMax().y + ImGui::GetStyle().FramePadding.y));
        if(ImGui::BeginPopup("event_search", ImGuiWindowFlags_NoFocusOnAppearing))
        {
            if(m_data_provider.IsRequestPending(GetRequestID()) ||
               tm.GetTableTotalRowCount(m_table_type) > 0)
            {
                ImGui::SetNextWindowSize(ImGui::GetContentRegionAvail() -
                                         ImVec2(0, ImGui::GetFrameHeightWithSpacing()));
                InfiniteScrollTable::Render();
                RenderContextMenu();
            }
            ImGui::AlignTextToFramePadding();
#ifdef ROCPROFVIS_DEVELOPER_MODE
            auto table_params =
                tm.GetTableParams(m_table_type);
            if(table_params)
            {
                ImGui::Text("Showing %llu to %llu of %llu result(s)",
                            table_params->m_start_row,
                            table_params->m_start_row + table_params->m_req_row_count,
                            tm.GetTableTotalRowCount(m_table_type));
            }
#else
            ImGui::Text("Showing %llu result(s)",
                        tm.GetTableTotalRowCount(m_table_type));
#endif
            if(m_should_close)
            {
                ImGui::CloseCurrentPopup();
                m_should_close = false;
            }
            ImGui::EndPopup();
        }
    }
}

void
EventSearch::Show()
{
    if(!Open())
    {
        m_should_open = true;
    }
}

void
EventSearch::Search()
{
    size_t input_length = strlen(m_text_input);
    if(input_length > 0)
    {
        bool                     valid = false;
        std::vector<std::string> terms;
        if(m_text_input[0] == '"')
        {
            std::string term;
            bool        open_quote = true;
            for(int i = 1; i < input_length; i++)
            {
                if(m_text_input[i] == '"')
                {
                    if(open_quote)
                    {
                        if(!term.empty())
                        {
                            terms.emplace_back(term);
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
            valid = !terms.empty() && !open_quote;
        }
        else
        {
            terms.emplace_back(m_text_input);
            valid = true;
        }
        if(valid)
        {
            const TimelineModel& timeline = m_data_provider.DataModel().GetTimeline();
            m_data_provider.CancelRequest(GetRequestID());
            m_search_deferred = !m_data_provider.FetchTable(TableRequestParams(
                m_req_table_type, {},
                { kRocProfVisDmOperationLaunch, kRocProfVisDmOperationDispatch,
                  kRocProfVisDmOperationLaunchSample },
                timeline.GetStartTime(), timeline.GetEndTime(), "", "", "", "", { terms },
                0, m_fetch_chunk_size));
            m_searched        = true;
            m_should_open     = true;
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
    m_data_provider.CancelRequest(GetRequestID());
    m_data_provider.DataModel().GetTables().ClearTable(m_table_type);
    m_text_input[0] = '\0';
    m_searched      = false;
    m_should_close  = true;
    if(m_timeline_selection)
    {
        m_timeline_selection->ClearSearchHighlights();
    }
}

void
EventSearch::SetWidth(float width)
{
    m_width = std::max(0.0f, width);
}

char*
EventSearch::TextInput()
{
    return m_text_input;
}

size_t
EventSearch::TextInputLimit() const
{
    return IM_ARRAYSIZE(m_text_input);
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
        m_focus_text_input = Open() && !ImGui::IsItemFocused() &&
                             ImGui::IsMouseHoveringRect(ImGui::GetItemRectMin(),
                                                        ImGui::GetItemRectMax()) &&
                             ImGui::IsMouseClicked(ImGuiMouseButton_Left);
        return m_focus_text_input;
    }
}

bool
EventSearch::Open() const
{
    return ImGui::IsPopupOpen("event_search");
}

bool
EventSearch::Searched() const
{
    return m_searched;
}

float
EventSearch::Width() const
{
    return m_width;
}

void
EventSearch::FormatData() const
{
    TablesModel& tm = m_data_provider.DataModel().GetTables();

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
        m_data_provider.DataModel().GetTables().GetTableHeader(m_table_type);
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

        if(m_timeline_selection && m_selected_row >= 0)
        {
            const std::vector<std::vector<std::string>>& table_data =
                m_data_provider.DataModel().GetTables().GetTableData(m_table_type);
            if(m_selected_row < static_cast<int>(table_data.size()))
            {
                uint64_t uuid = INVALID_UINT64_INDEX;
                uint64_t track_id = SelectedRowToTrackID(
                    m_important_column_idxs[kTrackId],
                    m_important_column_idxs[kStreamId]);
                if(m_important_column_idxs[kUUId] != INVALID_UINT64_INDEX &&
                   m_important_column_idxs[kUUId] < table_data[m_selected_row].size())
                {
                    try
                    {
                        uuid = std::stoull(
                            table_data[m_selected_row][m_important_column_idxs[kUUId]]);
                    }
                    catch(const std::exception&)
                    {
                        spdlog::warn("Failed to parse UUID from search result row");
                    }
                }
                if(uuid != INVALID_UINT64_INDEX && track_id != INVALID_UINT64_INDEX)
                {
                    m_timeline_selection->UnselectAllEvents();
                    m_timeline_selection->ClearSearchHighlights();
                    m_timeline_selection->SearchHighlightEvent(track_id, uuid);
                }
            }
        }

        m_should_close = true;
    }
    else if(mouse_button == ImGuiMouseButton_Right)
    {
        m_open_context_menu = true;
    }
    InfiniteScrollTable::RowSelected(mouse_button);
}

void
EventSearch::RenderContextMenu()
{
    if(m_open_context_menu)
    {
        ImGui::OpenPopup("");
        m_open_context_menu = false;
    }

    const ImGuiStyle& style = m_settings.GetDefaultStyle();
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, style.WindowPadding);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, style.ItemSpacing);
    if(ImGui::BeginPopup(""))
    {
        uint64_t target_track_id = SelectedRowToTrackID(
            m_important_column_idxs[kTrackId], m_important_column_idxs[kStreamId]);
        if(ImGui::MenuItem("Copy Row Data", nullptr, false))
        {
            SelectedRowToClipboard();
        }
        else if(ImGui::MenuItem("Go To Event", nullptr, false,
                                target_track_id != INVALID_UINT64_INDEX))
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
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar(2);
}

float
EventSearch::Height() const
{
    float    height;
    uint64_t results =
        m_data_provider.DataModel().GetTables().GetTableTotalRowCount(m_table_type);
    if(results > 0)
    {
        height = (m_horizontal_scroll ? ImGui::GetStyle().ScrollbarSize : 0) +
                 (2 + std::min(results, MAX_RESULTS_DISPLAYED)) *
                     ImGui::GetFrameHeightWithSpacing();
    }
    else
    {
        height = m_data_provider.IsRequestPending(GetRequestID())
                     ? 2 * ImGui::GetFrameHeightWithSpacing()
                     : ImGui::GetFrameHeightWithSpacing();
    }
    return height;
}

}  // namespace View
}  // namespace RocProfVis
