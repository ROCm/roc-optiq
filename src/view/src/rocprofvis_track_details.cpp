// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_track_details.h"
#include "icons/rocprovfis_icon_defines.h"
#include "model/rocprofvis_model_types.h"
#include "rocprofvis_data_provider.h"
#include "rocprofvis_events.h"
#include "rocprofvis_settings_manager.h"
#include "rocprofvis_timeline_selection.h"
#include "rocprofvis_track_topology.h"
#include "widgets/rocprofvis_gui_helpers.h"
#include "widgets/rocprofvis_widget.h"

namespace RocProfVis
{
namespace View
{

// TrackInfo::TrackType
constexpr const char* TRACK_PREFIX[] = { "Unknown", "Queue",  "Stream",
                                         "Thread",  "Thread", "Counter" };

TrackDetails::TrackDetails(DataProvider& dp, std::shared_ptr<TrackTopology> topology,
                           std::shared_ptr<TimelineSelection> timeline_selection)
: m_track_topology(topology)
, m_data_provider(dp)
, m_timeline_selection(timeline_selection)
, m_settings(SettingsManager::GetInstance())
, m_selection_dirty(false)
, m_data_valid(false)
, m_topology_changed_event_token(EventManager::InvalidSubscriptionToken)
, m_track_metadata_changed_event_token(EventManager::InvalidSubscriptionToken)
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

    auto metadata_changed_event_handler = [this](std::shared_ptr<RocEvent> event) {
        if(event)
        {
            if(m_data_provider.GetTraceFilePath() == event->GetSourceId())
            {
                m_data_valid = false;
            }
        }
    };

    m_track_metadata_changed_event_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kTrackMetadataChanged),
        metadata_changed_event_handler);
}

TrackDetails::~TrackDetails() {
    EventManager::GetInstance()->Unsubscribe(
        static_cast<int>(RocEvents::kTopologyChanged),
        m_topology_changed_event_token);

    EventManager::GetInstance()->Unsubscribe(
        static_cast<int>(RocEvents::kTrackMetadataChanged),
        m_track_metadata_changed_event_token);
}

void
TrackDetails::Render()
{
    if(m_data_valid && !m_selection_dirty && !m_track_topology->Dirty())
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
                    if(detail.parents.node || detail.parents.process || detail.track ||
                       detail.stream_track)
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
                                detail.parents.node->info->host_name.c_str());
                        }
                        if(detail.parents.process)
                        {
                            ImGui::PushFont(icons);
                            ImGui::SameLine(0.0f, style.ItemSpacing.x);
                            ImGui::TextUnformatted(ICON_ARROW_FORWARD);
                            ImGui::PopFont();
                            ImGui::SameLine(0.0f, style.ItemSpacing.x);
                            ImGui::TextUnformatted(
                                detail.parents.process->header.c_str());
                        }
                        if(detail.track || detail.stream_track)
                        {
                            ImGui::PushFont(icons);
                            ImGui::SameLine(0.0f, style.ItemSpacing.x);
                            ImGui::TextUnformatted(ICON_ARROW_FORWARD);
                            ImGui::PopFont();
                            ImGui::SameLine(0.0f, style.ItemSpacing.x);
                            if(detail.track)
                            {
                                ImGui::TextUnformatted(detail.track->info->name.c_str());
                            }
                            else if(detail.stream_track)
                            {
                                ImGui::TextUnformatted(
                                    detail.stream_track->info->name.c_str());
                            }
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
                                            detail.parents.node->info->host_name.c_str());
                                RenderTable(detail.parents.node->info_table);
                            }
                            if(detail.parents.process)
                            {
                                ImGui::Text("Process: %s",
                                            detail.parents.process->header.c_str());
                                RenderTable(detail.parents.process->info_table);
                            }
                        }
                        ImGui::EndChild();
                    }
                    if(detail.track || detail.stream_track)
                    {
                        ImGui::BeginChild("track", ImVec2(0.0f, 0.0f),
                                          ImGuiChildFlags_Borders |
                                              ImGuiChildFlags_AutoResizeY);
                        if(detail.track)
                        {
                            ImGui::Text("%s: %s", TRACK_PREFIX[detail.track_type],
                                        detail.track->info->name.c_str());
                            RenderTable(detail.track->info_table, detail.stats);
                        }
                        else if(detail.stream_track)
                        {
                            ImGui::Text("%s: %s", TRACK_PREFIX[detail.track_type],
                                        detail.stream_track->info->name.c_str());
                            RenderTable(detail.stream_track->info_table, detail.stats);
                        }
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
    if(m_selection_dirty && !m_track_topology->Dirty())
    {
        const TopologyModel&  topology = m_track_topology->GetTopology();
        std::list<DetailItem> uncategorized_tracks;
        for(DetailItem& item : m_track_details)
        {
            const TrackInfo* metadata =
                m_data_provider.DataModel().GetTimeline().GetTrack(item.track_id);
            if(metadata && metadata->topology.type != TrackInfo::TrackType::Unknown)
            {
                item.track_name =
                    m_data_provider.DataModel().BuildTrackName(item.track_id);
                item.track_type              = metadata->topology.type;
                const uint64_t& node_id      = metadata->topology.node_id;
                const uint64_t& process_id   = metadata->topology.process_id;
                const uint64_t& processor_id = metadata->topology.device_id;
                const uint64_t& type_id      = metadata->topology.id.value;
                if(topology.node_lut.count(node_id) > 0)
                {
                    NodeModel& node   = *topology.node_lut.at(node_id);
                    item.parents.node = &node;
                    if(node.process_lut.count(process_id) > 0)
                    {
                        ProcessModel& process = *node.process_lut.at(process_id);
                        item.parents.process  = &process;
                        switch(metadata->topology.type)
                        {
                            case TrackInfo::TrackType::InstrumentedThread:
                            {
                                if(process.instrumented_thread_lut.count(type_id) > 0)
                                {
                                    item.track =
                                        process.instrumented_thread_lut.at(type_id);
                                }
                                break;
                            }
                            case TrackInfo::TrackType::SampledThread:
                            {
                                if(process.sampled_thread_lut.count(type_id) > 0)
                                {
                                    item.track = process.sampled_thread_lut.at(type_id);
                                }
                                break;
                            }
                            case TrackInfo::TrackType::Stream:
                            {
                                if(process.stream_lut.count(type_id) > 0)
                                {
                                    item.stream_track = process.stream_lut.at(type_id);
                                }
                                break;
                            }
                        }
                    }
                    if(node.processor_lut.count(processor_id) > 0)
                    {
                        ProcessorModel& processor = *node.processor_lut.at(processor_id);
                        switch(metadata->topology.type)
                        {
                            case TrackInfo::TrackType::Queue:
                            {
                                if(processor.queue_lut.count(type_id) > 0)
                                {
                                    item.track = processor.queue_lut.at(type_id);
                                }
                                break;
                            }
                            case TrackInfo::TrackType::Counter:
                            {
                                if(processor.counter_lut.count(type_id) > 0)
                                {
                                    item.track = processor.counter_lut.at(type_id);
                                }
                                break;
                            }
                        }
                    }
                }
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

void
TrackDetails::RenderTable(InfoTable& table, const AnalysisTrackStatistics* stats)
{
    if((!table.cells.empty() && table.cells[0].size() == 2) || stats)
    {
        SettingsManager& settings = SettingsManager::GetInstance();
        const int        rows     = static_cast<int>(table.cells.size());
        const int        cols =
            table.cells.empty() ? 2 : static_cast<int>(table.cells[0].size());

        float table_x_min = ImGui::GetCursorScreenPos().x;
        float table_width = ImGui::GetContentRegionAvail().x;
        float table_x_max = table_x_min + table_width;

        ImGui::PushStyleColor(ImGuiCol_TableRowBg,
                              settings.GetColor(Colors::kFillerColor));
        ImGui::PushStyleColor(ImGuiCol_TableRowBgAlt,
                              settings.GetColor(Colors::kFillerColor));
        if(ImGui::BeginTable("", cols,
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
                for(int c = 0; c < cols; c++)
                {
                    ImGui::PushID(c);
                    ImGui::TableSetColumnIndex(c);
                    const char* data             = table.cells[r][c].data.c_str();
                    if(table.cells[r][c].needs_format) {
                        data = table.cells[r][c].formatted.c_str();
                    }
                    
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
                    ImGui::PopID();
                }
                ImGui::PopID();
            }
            if(stats)
            {
                ImGui::TableNextRow();
                switch(stats->track->topology.type)
                {
                    case TrackInfo::TrackType::Queue:
                    {
                        for(size_t i = 0; i < AnalysisTrackStatistics::Queue::kQueueCount;
                            i++)
                        {
                            ImGui::TableNextColumn();
                            ImGui::TextUnformatted(stats->stats[i].name);
                            ImGui::TableNextColumn();
                            ImGui::BeginDisabled(stats->state !=
                                                 AnalysisTrackStatistics::kReady);
                            ImGui::Text("%.1f", stats->stats[i].value);
                            ImGui::EndDisabled();
                        }
                        break;
                    }
                    case TrackInfo::TrackType::Counter:
                    {
                        for(size_t i = 0;
                            i < AnalysisTrackStatistics::Counter::kCounterCount; i++)
                        {
                            ImGui::TableNextColumn();
                            ImGui::TextUnformatted(stats->stats[i].name);
                            ImGui::TableNextColumn();
                            ImGui::BeginDisabled(stats->state !=
                                                 AnalysisTrackStatistics::kReady);
                            ImGui::Text("%.1f", stats->stats[i].value);
                            ImGui::EndDisabled();
                        }
                        break;
                    }
                }
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