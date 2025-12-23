// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_track_details.h"
#include "rocprofvis_data_provider.h"
#include "rocprofvis_events.h"
#include "rocprofvis_timeline_selection.h"
#include "rocprofvis_track_topology.h"
#include "widgets/rocprofvis_widget.h"

namespace RocProfVis
{
namespace View
{

TrackDetails::TrackDetails(DataProvider& dp, std::shared_ptr<TrackTopology> topology,
                           std::shared_ptr<TimelineSelection> timeline_selection)
: m_data_provider(dp)
, m_track_topology(topology)
, m_timeline_selection(timeline_selection)
    , m_selection_dirty(false)
    , m_detail_item_id(0)
    , m_topology_changed_event_token(EventManager::InvalidSubscriptionToken)
    , m_track_metadata_changed_event_token(EventManager::InvalidSubscriptionToken)
    , m_data_valid(false)
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
        ImGui::BeginChild("track_details", ImVec2(0, 0), ImGuiChildFlags_Borders);
        if(m_track_details.empty())
        {
            ImGui::TextUnformatted("No data available for the selected tracks.");
        }
        else
        {
            for(DetailItem& detail : m_track_details)
            {
                ImGui::PushID(detail.id);
                if(ImGui::CollapsingHeader(detail.track_name->c_str(),
                                           ImGuiTreeNodeFlags_DefaultOpen))
                {
                    ImGui::TextUnformatted("Node: ");
                    ImGui::SameLine();
                    ImGui::TextUnformatted(detail.node->info->host_name.c_str());
                    RenderTable(detail.node->info_table);
                    ImGui::TextUnformatted("Process: ");
                    ImGui::SameLine();
                    ImGui::TextUnformatted(detail.process->header.c_str());
                    RenderTable(detail.process->info_table);
                    if(detail.queue)
                    {
                        ImGui::TextUnformatted("Queue: ");
                        ImGui::SameLine();
                        ImGui::TextUnformatted(detail.queue->info->name.c_str());
                        RenderTable(detail.queue->info_table);
                    }
                    else if(detail.instrumented_thread)
                    {
                        ImGui::TextUnformatted("Thread: ");
                        ImGui::SameLine();
                        ImGui::TextUnformatted(
                            detail.instrumented_thread->info->name.c_str());
                        RenderTable(detail.instrumented_thread->info_table);
                    }
                    else if(detail.sampled_thread)
                    {
                        ImGui::TextUnformatted("Thread: ");
                        ImGui::SameLine();
                        ImGui::TextUnformatted(detail.sampled_thread->info->name.c_str());
                        RenderTable(detail.sampled_thread->info_table);
                    }
                    else if(detail.counter)
                    {
                        ImGui::TextUnformatted("Counter: ");
                        ImGui::SameLine();
                        ImGui::TextUnformatted(detail.counter->info->name.c_str());
                        RenderTable(detail.counter->info_table);
                    }
                    else if(detail.stream)
                    {
                        ImGui::TextUnformatted("Stream: ");
                        ImGui::SameLine();
                        ImGui::TextUnformatted(detail.stream->info->name.c_str());
                        RenderTable(detail.stream->info_table);
                    }
                }
                ImGui::PopID();
            }
        }
        ImGui::EndChild();
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
            const TrackInfo* metadata = m_data_provider.GetTrackInfo(item.track_id);
            if(metadata && metadata->topology.type != TrackInfo::Topology::Unknown)
            {
                item.track_name = &metadata->name;

                const uint64_t& node_id    = metadata->topology.node_id;
                const uint64_t& process_id = metadata->topology.process_id;
                const uint64_t& type_id    = metadata->topology.id;
                if(topology.node_lut.count(node_id) > 0)
                {
                    NodeModel& node = *topology.node_lut.at(node_id);
                    item.node       = &node;
                    if(node.process_lut.count(process_id) > 0)
                    {
                        ProcessModel& process = *node.process_lut.at(process_id);
                        item.process          = &process;
                        switch(metadata->topology.type)
                        {
                            case TrackInfo::Topology::Queue:
                            {
                                if(process.queue_lut.count(type_id) > 0)
                                {
                                    item.queue = process.queue_lut.at(type_id);
                                }
                                break;
                            }
                            case TrackInfo::Topology::InstrumentedThread:
                            {
                                if(process.instrumented_thread_lut.count(type_id) > 0)
                                {
                                    item.instrumented_thread =
                                        process.instrumented_thread_lut.at(type_id);
                                }
                                break;
                            }
                            case TrackInfo::Topology::SampledThread:
                            {
                                if(process.sampled_thread_lut.count(type_id) > 0)
                                {
                                    item.sampled_thread =
                                        process.sampled_thread_lut.at(type_id);
                                }
                                break;
                            }
                            case TrackInfo::Topology::Counter:
                            {
                                if(process.counter_lut.count(type_id) > 0)
                                {
                                    item.counter = process.counter_lut.at(type_id);
                                }
                                break;
                            }
                            case TrackInfo::Topology::Stream:
                            {
                                if(process.stream_lut.count(type_id) > 0)
                                {
                                    item.stream = process.stream_lut.at(type_id);
                                }
                                break;
                            }
                        }
                    }
                }
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
TrackDetails::RenderTable(InfoTable& table)
{
    if(!table.cells.empty() && !table.cells[0].empty())
    {
        const int& rows = static_cast<int>(table.cells.size());
        const int& cols = static_cast<int>(table.cells[0].size());

        float table_x_min = ImGui::GetCursorScreenPos().x;
        float table_width = ImGui::GetContentRegionAvail().x;
        float table_x_max = table_x_min + table_width;
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
            ImGui::EndTable();
        }
    }
}

void
TrackDetails::HandleTrackSelectionChanged(const uint64_t track_id, const bool selected)
{
    if(selected)
    {
        m_track_details.emplace_front(DetailItem{ m_detail_item_id++, track_id, nullptr,
                                                  nullptr, nullptr, nullptr, nullptr,
                                                  nullptr, nullptr });
    }
    else if(track_id == TimelineSelection::INVALID_SELECTION_ID)
    {
        m_track_details.clear();
    }
    else
    {
        m_track_details.remove(DetailItem{ 0, track_id, nullptr, nullptr, nullptr,
                                           nullptr, nullptr, nullptr, nullptr });
    }
    m_selection_dirty = true;
}

}  // namespace View
}  // namespace RocProfVis