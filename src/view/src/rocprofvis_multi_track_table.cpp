// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_multi_track_table.h"
#include "icons/rocprovfis_icon_defines.h"
#include "rocprofvis_event_manager.h"
#include "rocprofvis_settings_manager.h"
#include "rocprofvis_timeline_selection.h"
#include "rocprofvis_utils.h"
#include "spdlog/spdlog.h"
#include "widgets/rocprofvis_notification_manager.h"
#include <sstream>

namespace RocProfVis
{
namespace View
{

constexpr uint64_t INVALID_UINT64_INDEX = std::numeric_limits<uint64_t>::max();

const std::string TRACK_ID_COLUMN_NAME  = "__trackId";
const std::string STREAM_ID_COLUMN_NAME = "__streamTrackId";
const std::string ID_COLUMN_NAME        = "id";
const std::string NAME_COLUMN_NAME      = "name";
const std::string START_TS_COLUMN_NAME  = "startTs";
const std::string END_TS_COLUMN_NAME    = "endTs";
const std::string DURATION_COLUMN_NAME  = "duration";

MultiTrackTable::MultiTrackTable(DataProvider&                      dp,
                                 std::shared_ptr<TimelineSelection> timeline_selection,
                                 TableType                          table_type)
: InfiniteScrollTable(dp, table_type)
, m_important_column_idxs(std::vector<size_t>(kNumImportantColumns, INVALID_UINT64_INDEX))
, m_timeline_selection(timeline_selection)
, m_defer_track_selection_changed(false)
{
    m_widget_name = (table_type == TableType::kEventTable)
                        ? GenUniqueName("Event Table")
                        : GenUniqueName("Sample Table");
}

void
MultiTrackTable::HandleTrackSelectionChanged()
{
    std::vector<uint64_t> tracks;
    double                start_ns;
    double                end_ns;
    m_timeline_selection->GetSelectedTracks(tracks);

    // If no valid time range is provided, use the full trace range
    if(m_timeline_selection->HasValidTimeRangeSelection())
    {
        m_timeline_selection->GetSelectedTimeRange(start_ns, end_ns);
    }
    else
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
            m_req_table_type, filtered_tracks, start_ns, end_ns, m_filter_options.filter,
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
MultiTrackTable::Update()
{
    // Handle track selection changed event
    if(m_defer_track_selection_changed)
    {
        if(!m_data_provider.IsRequestPending(m_table_type == TableType::kEventTable
                                                 ? DataProvider::EVENT_TABLE_REQUEST_ID
                                                 : DataProvider::SAMPLE_TABLE_REQUEST_ID))
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
            m_data_provider.GetTableHeader(m_table_type);

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
    }
    InfiniteScrollTable::Update();
}

uint64_t
MultiTrackTable::GetTrackIdHelper(
    const std::vector<std::vector<std::string>>& table_data) const
{
    uint64_t track_id  = INVALID_UINT64_INDEX;
    uint64_t stream_id = INVALID_UINT64_INDEX;

    // get track id or stream id
    if(m_important_column_idxs[kTrackId] != INVALID_UINT64_INDEX &&
       m_important_column_idxs[kTrackId] < table_data[m_selected_row].size())
    {
        track_id =
            std::stoull(table_data[m_selected_row][m_important_column_idxs[kTrackId]]);
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
MultiTrackTable::FetchData(const TableRequestParams& params) const
{
    m_data_provider.FetchMultiTrackTable(params);
}

void
MultiTrackTable::RenderContextMenu() const
{
    auto style = m_settings.GetDefaultStyle();

    // Render context menu for row actions
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, style.WindowPadding);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, style.ItemSpacing);
    if(ImGui::BeginPopup(ROWCONTEXTMENU_POPUP_NAME))
    {
        const std::vector<std::vector<std::string>>& table_data =
            m_data_provider.GetTableData(m_table_type);
        uint64_t target_track_id = GetTrackIdHelper(table_data);

        if(ImGui::MenuItem("Copy Row Data", nullptr, false))
        {
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
        else if(ImGui::MenuItem("Go to event", nullptr, false,
                                target_track_id != INVALID_UINT64_INDEX))
        {
            if(m_selected_row < 0 || m_selected_row >= (int) table_data.size())
            {
                spdlog::warn("Selected row index out of bounds: {}", m_selected_row);
            }
            else
            {
                // Handle navigation
                if(target_track_id != INVALID_UINT64_INDEX)
                {
                    spdlog::info("Navigating to track ID: {} from row: {}",
                                 target_track_id, m_selected_row);
                    EventManager::GetInstance()->AddEvent(
                        std::make_shared<ScrollToTrackEvent>(
                            static_cast<int>(RocEvents::kHandleUserGraphNavigationEvent),
                            target_track_id, m_data_provider.GetTraceFilePath()));
                    // get start time and duration
                    uint64_t start_time = 0;
                    uint64_t duration   = 0;
                    if(m_important_column_idxs[kTimeStartNs] != INVALID_UINT64_INDEX &&
                       m_important_column_idxs[kTimeStartNs] <
                           table_data[m_selected_row].size())
                    {
                        start_time = std::stoull(
                            table_data[m_selected_row]
                                      [m_important_column_idxs[kTimeStartNs]]);
                    }

                    if(m_important_column_idxs[kDurationNs] != INVALID_UINT64_INDEX &&
                       m_important_column_idxs[kDurationNs] <
                           table_data[m_selected_row].size())
                    {
                        duration =
                            std::stoull(table_data[m_selected_row]
                                                  [m_important_column_idxs[kDurationNs]]);
                    }

                    ViewRangeNS view_range = calculate_adaptive_view_range(
                        static_cast<double>(start_time), static_cast<double>(duration));
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
    // Pop the style vars for window padding and item spacing
    ImGui::PopStyleVar(2);
}

}  // namespace View
}  // namespace RocProfVis
