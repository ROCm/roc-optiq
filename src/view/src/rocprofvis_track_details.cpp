// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_track_details.h"
#include "icons/rocprovfis_icon_defines.h"
#include "model/rocprofvis_model_types.h"
#include "rocprofvis_data_provider.h"
#include "rocprofvis_events.h"
#include "rocprofvis_settings_manager.h"
#include "rocprofvis_timeline_selection.h"
#include "rocprofvis_utils.h"
#include "widgets/rocprofvis_gui_helpers.h"
#include "widgets/rocprofvis_widget.h"

#include <string>
#include <vector>

namespace RocProfVis
{
namespace View
{

// TrackInfo::TrackType
constexpr const char* TRACK_PREFIX[] = { "Unknown", "Queue",  "Stream",
                                         "Thread",  "Thread", "Counter" };

// Shared cell context-menu popup id. Only one cell menu is open at a time, and
// BeginTable scopes ids per table, so a single constant is unambiguous.
constexpr const char* CELL_CONTEXT_MENU_ID = "##track_details_cell_menu";

TrackDetails::TrackDetails(DataProvider&                      dp,
                           std::shared_ptr<TimelineSelection> timeline_selection)
: m_data_provider(dp)
, m_timeline_selection(timeline_selection)
, m_settings(SettingsManager::GetInstance())
, m_selection_dirty(false)
, m_data_valid(false)
, m_topology_changed_event_token(EventManager::InvalidSubscriptionToken)
, m_track_metadata_changed_event_token(EventManager::InvalidSubscriptionToken)
, m_time_format_changed_token(EventManager::InvalidSubscriptionToken)
{
    auto topology_changed_event_handler = [this](std::shared_ptr<RocEvent> event) {
        if(event)
        {
            if(m_data_provider.GetTraceFilePath() == event->GetSourceId())
            {
                m_selection_dirty = true;
            }
        }
    };

    m_topology_changed_event_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kTopologyChanged), topology_changed_event_handler);

    // Track names carry metadata (compare labels, pid suffixes), so a metadata
    // change means the resolved details have to be rebuilt, not just redrawn.
    auto metadata_changed_event_handler = [this](std::shared_ptr<RocEvent> event) {
        if(event)
        {
            if(m_data_provider.GetTraceFilePath() == event->GetSourceId())
            {
                m_data_valid      = false;
                m_selection_dirty = true;
            }
        }
    };

    m_track_metadata_changed_event_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kTrackMetadataChanged),
        metadata_changed_event_handler);

    m_time_format_changed_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kTimeFormatChanged),
        [this](std::shared_ptr<RocEvent> e) {
            (void) e;
            for(DetailItem& item : m_track_details)
            {
                FormatTimeCells(item);
            }
        });
}

TrackDetails::~TrackDetails() {
    EventManager::GetInstance()->Unsubscribe(
        static_cast<int>(RocEvents::kTopologyChanged),
        m_topology_changed_event_token);

    EventManager::GetInstance()->Unsubscribe(
        static_cast<int>(RocEvents::kTrackMetadataChanged),
        m_track_metadata_changed_event_token);

    EventManager::GetInstance()->Unsubscribe(
        static_cast<int>(RocEvents::kTimeFormatChanged), m_time_format_changed_token);
}

void
TrackDetails::Render()
{
    if(m_data_valid && !m_selection_dirty)
    {
        const ImGuiStyle& style = m_settings.GetDefaultStyle();
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, style.ChildRounding);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, style.WindowPadding);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, m_settings.GetColor(Colors::kBgPanel));
        ImGui::PushStyleColor(ImGuiCol_Border, m_settings.GetColor(Colors::kBorderColor));
        ImGui::BeginChild("track_details", ImVec2(0, 0),
                          ImGuiChildFlags_Borders |
                              ImGuiChildFlags_AlwaysUseWindowPadding);
        if(m_track_details.empty())
        {
            CenterNextTextItem("No data available for the selected tracks.");
            ImGui::SetCursorPosY((ImGui::GetWindowHeight() - ImGui::GetTextLineHeight()) *
                                 0.5f);
            ImGui::TextDisabled("No data available for the selected tracks.");
        }
        else
        {
            ImFont* icons = m_settings.GetFontManager().GetFont(FontType::kIcon);
            ImGui::PushFont(icons);
            ImVec2 icon_size = ImGui::CalcTextSize(ICON_CHEVRON_DOWN);
            ImGui::PopFont();
            int id = 0;
            for(DetailItem& detail : m_track_details)
            {
                ImGui::PushID(id++);
                if(ImGui::CollapsingHeader(detail.track_name.c_str(),
                                           ImGuiTreeNodeFlags_DefaultOpen))
                {
                    if(detail.parents.node || detail.parents.process || detail.track)
                    {
                        ImGui::BeginChild("topology", ImVec2(0.0f, 0.0f),
                                          ImGuiChildFlags_Borders |
                                              ImGuiChildFlags_AutoResizeY);
                        ImGui::BeginGroup();
                        IconButton(
                            detail.parents.expand ? ICON_CHEVRON_DOWN
                                                  : ICON_CHEVRON_RIGHT,
                            icons,
                            ImVec2(icon_size.x + style.FramePadding.x * 2.0f,
                                   icon_size.y + style.FramePadding.y * 2.0f),
                            nullptr, false, style.FramePadding,
                            SettingsManager::GetInstance().GetColor(Colors::kTransparent),
                            SettingsManager::GetInstance().GetColor(
                                Colors::kButtonHovered),
                            SettingsManager::GetInstance().GetColor(
                                Colors::kTransparent));
                        if(detail.parents.node)
                        {
                            ImGui::SameLine();
                            ImGui::TextUnformatted(
                                detail.parents.node->host_name.c_str());
                        }
                        if(detail.parents.process)
                        {
                            ImGui::PushFont(icons);
                            ImGui::SameLine(0.0f, style.ItemSpacing.x);
                            ImGui::TextUnformatted(ICON_ARROW_FORWARD);
                            ImGui::PopFont();
                            ImGui::SameLine(0.0f, style.ItemSpacing.x);
                            ImGui::TextUnformatted(
                                detail.parents.process->GetHeader().c_str());
                        }
                        if(detail.track)
                        {
                            ImGui::PushFont(icons);
                            ImGui::SameLine(0.0f, style.ItemSpacing.x);
                            ImGui::TextUnformatted(ICON_ARROW_FORWARD);
                            ImGui::PopFont();
                            ImGui::SameLine(0.0f, style.ItemSpacing.x);
                            ImGui::TextUnformatted(detail.track->GetName().c_str());
                        }
                        ImGui::EndGroup();
                        if(ImGui::IsItemClicked())
                        {
                            detail.parents.expand = !detail.parents.expand;
                        }
                        if(detail.parents.expand)
                        {
                            if(detail.parents.node)
                            {
                                ImGui::Text("Node: %s",
                                            detail.parents.node->host_name.c_str());
                                RenderTable(detail.node_table, "##td_node_table");
                            }
                            if(detail.parents.process)
                            {
                                ImGui::Text("Process: %s",
                                            detail.parents.process->GetHeader().c_str());
                                RenderTable(detail.process_table, "##td_process_table");
                            }
                        }
                        ImGui::EndChild();
                    }
                    if(detail.track)
                    {
                        ImGui::BeginChild("track", ImVec2(0.0f, 0.0f),
                                          ImGuiChildFlags_Borders |
                                              ImGuiChildFlags_AutoResizeY);
                        ImGui::Text("%s: %s", TRACK_PREFIX[detail.track_type],
                                    detail.track->GetName().c_str());
                        RenderTable(detail.track_table, "##td_track_table", detail.stats);
                        ImGui::EndChild();
                    }
                }
                ImGui::PopID();
            }
        }
        ImGui::EndChild();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
    }
}

void
TrackDetails::Update()
{
    if(m_selection_dirty && m_data_provider.GetState() == ProviderState::kReady)
    {
        std::list<DetailItem> uncategorized_tracks;
        for(DetailItem& item : m_track_details)
        {
            const TrackInfo* metadata =
                m_data_provider.DataModel().GetTimeline().GetTrack(item.track_id);
            if(metadata && metadata->topology.type != TrackInfo::TrackType::Unknown)
            {
                Resolve(item, *metadata);
                BuildTables(item);
                item.stats =
                    m_data_provider.DataModel().GetAnalysis().RegisterTrack(*metadata);
            }
            else
            {
                uncategorized_tracks.push_back(item);
            }
        }
        for(DetailItem& item : uncategorized_tracks)
        {
            m_track_details.remove(item);
        }
        m_selection_dirty = false;
        m_data_valid = true;
    }
}

/*
 * Locates the topology nodes behind one selected track.
 *
 * The track's own node comes from the track id it was bound to at load. Node
 * and process come from the track metadata rather than by walking up from the
 * track, because a queue's ancestors are processor -> node: it has no process
 * ancestor, but the pane still shows the process that dispatched to it.
 */
void
TrackDetails::Resolve(DetailItem& item, const TrackInfo& metadata)
{
    const TopologyTree& tree = m_data_provider.DataModel().GetTopology();

    item.track_name       = m_data_provider.DataModel().BuildTrackName(item.track_id);
    item.track_type       = metadata.topology.type;
    item.track            = tree.FindByTrackId(item.track_id);
    item.parents.node     = tree.GetNode(metadata.topology.node_id);
    item.parents.process  = tree.GetProcess(metadata.topology.process_id);
}

void
TrackDetails::BuildTables(DetailItem& item)
{
    item.node_table.cells.clear();
    item.process_table.cells.clear();
    item.track_table.cells.clear();

    if(const NodeInfo* node = item.parents.node)
    {
        item.node_table.cells = {
            { DetailsTable::Cell{ "OS" }, DetailsTable::Cell{ node->os_name } },
            { DetailsTable::Cell{ "OS Release" },
              DetailsTable::Cell{ node->os_release } },
            { DetailsTable::Cell{ "OS Version" },
              DetailsTable::Cell{ node->os_version } }
        };
    }

    if(const ProcessInfo* process = item.parents.process)
    {
        item.process_table.cells = {
            { DetailsTable::Cell{ "Start Time" },
              DetailsTable::Cell{ std::to_string(process->start_time), false, true } },
            { DetailsTable::Cell{ "End Time" },
              DetailsTable::Cell{ std::to_string(process->end_time), false, true } },
            { DetailsTable::Cell{ "Command" }, DetailsTable::Cell{ process->command } },
            { DetailsTable::Cell{ "Environment" },
              DetailsTable::Cell{ process->environment } }
        };
    }

    if(item.track)
    {
        switch(item.track->GetNodeType())
        {
            case TopologyNodeType::kQueue:
            {
                // A queue's own properties are its processor's.
                const ProcessorInfo* processor = static_cast<const ProcessorInfo*>(
                    item.track->GetParent(TopologyNodeType::kProcessor));
                if(processor)
                {
                    const std::string processor_label =
                        std::string(
                            TopologyTree::GetProcessorTypeName(processor->type)) +
                        " " + std::to_string(processor->type_index);
                    item.track_table.cells = {
                        { DetailsTable::Cell{ processor_label },
                          DetailsTable::Cell{ processor->product_name } }
                    };
                }
                break;
            }
            case TopologyNodeType::kCounter:
            {
                const CounterInfo& counter =
                    static_cast<const CounterInfo&>(*item.track);
                item.track_table.cells = {
                    { DetailsTable::Cell{ "Description" },
                      DetailsTable::Cell{ counter.description } },
                    { DetailsTable::Cell{ "Value Type" },
                      DetailsTable::Cell{ counter.value_type } }
                };
                break;
            }
            case TopologyNodeType::kThread:
            {
                const ThreadInfo& thread = static_cast<const ThreadInfo&>(*item.track);
                item.track_table.cells   = {
                    { DetailsTable::Cell{ "Start Time" },
                      DetailsTable::Cell{ std::to_string(thread.start_time), false,
                                            true } },
                    { DetailsTable::Cell{ "End Time" },
                      DetailsTable::Cell{ std::to_string(thread.end_time), false, true } }
                };
                break;
            }
            default: break;  // Streams have no properties of their own.
        }
    }

    FormatTimeCells(item);
}

void
TrackDetails::FormatTimeCells(DetailItem& item)
{
    const TimeFormat time_format =
        m_settings.GetUserSettings().unit_settings.time_format;

    for(DetailsTable* table :
        { &item.node_table, &item.process_table, &item.track_table })
    {
        for(std::vector<DetailsTable::Cell>& row : table->cells)
        {
            for(DetailsTable::Cell& cell : row)
            {
                if(cell.is_time)
                {
                    cell.formatted = nanosecond_to_formatted_str(
                        std::stod(cell.data), time_format, true);
                }
            }
        }
    }
}

/*
 * Renders a two-column info table for the selected track details.
 *
 * Each row carries a full-row right-click hit-box plus per-cell capture so the
 * shared copy context menu (Copy Row / Copy Cell) can be shown. When stats are
 * provided they are appended as extra rows; their row indices are offset by the
 * info-table row count so they can share the single m_cell_menu target. The
 * table_id gives each table a stable, unique ImGui id (required so its context
 * menu popup does not collide with the other tables drawn in the same pane).
 */
void
TrackDetails::RenderTable(DetailsTable& table, const char* table_id,
                          const AnalysisTrackStatistics* stats)
{
    if((!table.cells.empty() && table.cells[0].size() == 2) || stats)
    {
        SettingsManager& settings = SettingsManager::GetInstance();
        const int        rows     = static_cast<int>(table.cells.size());
        const int        cols =
            table.cells.empty() ? 2 : static_cast<int>(table.cells[0].size());

        int stat_count = 0;
        if(stats)
        {
            switch(stats->track->topology.type)
            {
                case TrackInfo::TrackType::Queue:
                    stat_count = static_cast<int>(
                        AnalysisTrackStatistics::Queue::kQueueCount);
                    break;
                case TrackInfo::TrackType::Counter:
                    stat_count = static_cast<int>(
                        AnalysisTrackStatistics::Counter::kCounterCount);
                    break;
            }
        }

        float table_x_min = ImGui::GetCursorScreenPos().x;
        float table_width = ImGui::GetContentRegionAvail().x;
        float table_x_max = table_x_min + table_width;

        ImGui::PushStyleColor(ImGuiCol_TableRowBg,
                              settings.GetColor(Colors::kFillerColor));
        ImGui::PushStyleColor(ImGuiCol_TableRowBgAlt,
                              settings.GetColor(Colors::kFillerColor));
        bool open_menu = false;
        if(ImGui::BeginTable(table_id, cols,
                             ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter |
                                 ImGuiTableFlags_BordersV |
                                 ImGuiTableFlags_SizingFixedFit |
                                 ImGuiTableFlags_NoKeepColumnsVisible,
                             ImVec2(table_width, 0)))
        {
            for(int r = 0; r < rows; r++)
            {
                ImGui::PushID(r);
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                RenderRowHitbox("##td_row_sel", r, cols, m_cell_menu, open_menu);
                for(int c = 0; c < cols; c++)
                {
                    ImGui::PushID(c);
                    PositionCell(c);
                    const char* data = table.cells[r][c].is_time
                                           ? table.cells[r][c].formatted.c_str()
                                           : table.cells[r][c].data.c_str();

                    bool&       expand           = table.cells[r][c].expand;
                    ImVec2      cursor_pos_local = ImGui::GetCursorPos();
                    ImVec2      cursor_pos_abs   = ImGui::GetCursorScreenPos();
                    bool        elide =
                        cursor_pos_abs.x + ImGui::CalcTextSize(data).x > table_x_max;
                    if(elide)
                    {
                        ImGuiStyle& style = ImGui::GetStyle();
                        if(expand)
                        {
                            ImGui::PushTextWrapPos(
                                table_width -
                                (ImGui::CalcTextSize("-").x +
                                 2 * (style.FramePadding.x + style.CellPadding.x)));
                        }
                        else
                        {
                            ImGui::PushClipRect(
                                cursor_pos_abs,
                                ImVec2(table_x_max - (ImGui::CalcTextSize("-").x +
                                                      2 * (style.FramePadding.x +
                                                           style.CellPadding.x)),
                                       cursor_pos_abs.y +
                                           ImGui::GetTextLineHeightWithSpacing()),
                                true);
                        }
                    }
                    ImGui::TextUnformatted(data);
                    if(elide)
                    {
                        if(expand)
                        {
                            ImGui::PopTextWrapPos();
                        }
                        else
                        {
                            ImGui::PopClipRect();
                        }
                        ImGuiStyle& style = ImGui::GetStyle();

                        ImGui::SetCursorPos(
                            ImVec2(table_width -
                                       (ImGui::CalcTextSize("-").x +
                                        2 * style.FramePadding.x + style.CellPadding.x),
                                   cursor_pos_local.y));
                        if(ImGui::SmallButton(expand ? "-" : "+"))
                        {
                            expand = !expand;
                        }
                    }
                    CaptureCellRightClick(c, r, m_cell_menu, open_menu);
                    ImGui::PopID();
                }
                ImGui::PopID();
            }
            if(stats)
            {
                const bool stats_ready =
                    stats->state == AnalysisTrackStatistics::kReady;
                for(int i = 0; i < stat_count; i++)
                {
                    const int stat_row = rows + i;
                    ImGui::PushID(stat_row);
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    RenderRowHitbox("##td_stat_sel", stat_row, cols, m_cell_menu,
                                    open_menu);
                    PositionCell(0);
                    ImGui::TextUnformatted(stats->stats[i].name);
                    CaptureCellRightClick(0, stat_row, m_cell_menu, open_menu);
                    PositionCell(1);
                    ImGui::BeginDisabled(!stats_ready);
                    const std::string stat_value =
                        stats_ready ? stats->stats[i].FullValue() : "--";
                    ImGui::TextUnformatted(stat_value.c_str());
                    ImGui::EndDisabled();
                    CaptureCellRightClick(1, stat_row, m_cell_menu, open_menu);
                    ImGui::PopID();
                }
            }

            if(open_menu)
            {
                ImGui::OpenPopup(CELL_CONTEXT_MENU_ID);
            }
            if(BeginCellContextMenu(CELL_CONTEXT_MENU_ID))
            {
                if(m_cell_menu.row >= 0 && m_cell_menu.row < rows)
                {
                    std::vector<std::string> row_cells;
                    row_cells.reserve(cols);
                    for(int c = 0; c < cols; c++)
                    {
                        const DetailsTable::Cell& cell =
                            table.cells[m_cell_menu.row][c];
                        row_cells.push_back(cell.is_time ? cell.formatted : cell.data);
                    }
                    AddCopyRowCellMenuItems(row_cells.data(), cols, m_cell_menu.column);
                }
                else if(stats && m_cell_menu.row >= rows &&
                        m_cell_menu.row < rows + stat_count)
                {
                    const AnalysisTrackStatistics::Stat& stat =
                        stats->stats[m_cell_menu.row - rows];
                    std::string stat_cells[2] = { std::string(stat.name),
                                                  stat.FullValue() };
                    AddCopyRowCellMenuItems(stat_cells, 2, m_cell_menu.column);
                }
                EndCellContextMenu();
            }
            ImGui::EndTable();
        }
        ImGui::PopStyleColor(2);
    }
}

void
TrackDetails::HandleTrackSelectionChanged(const uint64_t track_id, const bool selected)
{
    if(selected)
    {
        m_track_details.emplace_front(DetailItem{ track_id });
    }
    else if(track_id == TimelineSelection::INVALID_SELECTION_ID)
    {
        m_track_details.clear();
    }
    else
    {
        m_track_details.remove(DetailItem{ track_id });
    }
    m_selection_dirty = true;
}

}  // namespace View
}  // namespace RocProfVis