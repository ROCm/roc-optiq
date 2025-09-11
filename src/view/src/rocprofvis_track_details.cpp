// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_track_details.h"
#include "rocprofvis_data_provider.h"
#include "rocprofvis_events.h"
#include "rocprofvis_track_topology.h"
#include "widgets/rocprofvis_widget.h"

namespace RocProfVis
{
namespace View
{

TrackDetails::TrackDetails(DataProvider& dp, std::shared_ptr<TrackTopology> topology)
: m_data_provider(dp)
, m_track_topology(topology)
, m_selection_dirty(false)
{}

TrackDetails::~TrackDetails() {}

void
TrackDetails::Render()
{
    if(!m_selection_dirty && !m_track_topology->Dirty())
    {
        ImGui::BeginChild("track_details", ImVec2(0, 0), ImGuiChildFlags_Borders);
        if(m_track_details.empty())
        {
            ImGui::TextUnformatted("No data available for the selected tracks.");
        }
        else
        {
            for(Details& detail : m_track_details)
            {
                if(ImGui::CollapsingHeader(detail.track_name.c_str(),
                                           ImGuiTreeNodeFlags_DefaultOpen))
                {
                    ImGui::TextUnformatted("Node: ");
                    ImGui::SameLine();
                    ImGui::TextUnformatted(detail.node.info->host_name.c_str());
                    RenderTable(detail.node.info_table);
                    ImGui::TextUnformatted("Process: ");
                    ImGui::SameLine();
                    ImGui::TextUnformatted(detail.process.header.c_str());
                    RenderTable(detail.process.info_table);
                    if(detail.queue)
                    {
                        ImGui::TextUnformatted("Queue: ");
                        ImGui::SameLine();
                        ImGui::TextUnformatted(detail.queue->info->name.c_str());
                        RenderTable(detail.queue->info_table);
                    }
                    else if(detail.thread)
                    {
                        ImGui::TextUnformatted("Thread: ");
                        ImGui::SameLine();
                        ImGui::TextUnformatted(detail.thread->info->name.c_str());
                        RenderTable(detail.thread->info_table);
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
        m_track_details.clear();
        const TopologyModel& topology = m_track_topology->GetTopology();
        for(const uint64_t& track_id : m_selected_track_ids)
        {
            const track_info_t* metadata = m_data_provider.GetTrackInfo(track_id);
            if(metadata && metadata->topology.type != track_info_t::Topology::Unknown)
            {
                const uint64_t& node_id    = metadata->topology.node_id;
                const uint64_t& process_id = metadata->topology.process_id;
                const uint64_t& type_id    = metadata->topology.id;
                if(topology.node_lut.count(node_id) > 0)
                {
                    NodeModel& node = *topology.node_lut.at(node_id);
                    if(node.process_lut.count(process_id) > 0)
                    {
                        ProcessModel& process = *node.process_lut.at(process_id);
                        switch(metadata->topology.type)
                        {
                            case track_info_t::Topology::Queue:
                            {
                                if(process.queue_lut.count(type_id) > 0)
                                {
                                    m_track_details.push_back(
                                        std::move(Details{ metadata->name, node, process,
                                                           process.queue_lut.at(type_id),
                                                           nullptr, nullptr, nullptr }));
                                }
                                break;
                            }
                            case track_info_t::Topology::Thread:
                            {
                                if(process.thread_lut.count(type_id) > 0)
                                {
                                    m_track_details.push_back(std::move(
                                        Details{ metadata->name, node, process, nullptr,
                                                 process.thread_lut.at(type_id), nullptr,
                                                 nullptr }));
                                }
                                break;
                            }
                            case track_info_t::Topology::Counter:
                            {
                                if(process.counter_lut.count(type_id) > 0)
                                {
                                    m_track_details.push_back(std::move(Details{
                                        metadata->name, node, process, nullptr, nullptr,
                                        process.counter_lut.at(type_id), nullptr }));
                                }
                                break;
                            }
                            case track_info_t::Topology::Stream:
                            {
                                if(process.stream_lut.count(type_id) > 0)
                                {
                                    m_track_details.push_back(std::move(Details{
                                        metadata->name, node, process, nullptr, nullptr,
                                        nullptr, process.stream_lut.at(type_id) }));
                                }
                                break;
                            }
                        }
                    }
                }
            }
        }
        m_selection_dirty = false;
    }
}

void
TrackDetails::RenderTable(InfoTable& table)
{
    if(!table.cells.empty() && !table.cells[0].empty())
    {
        const int& rows = table.cells.size();
        const int& cols = table.cells[0].size();

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
TrackDetails::HandleTrackSelectionChanged(
    std::shared_ptr<TrackSelectionChangedEvent> event)
{
    if(event && event->GetSourceId() == m_data_provider.GetTraceFilePath())
    {
        m_selected_track_ids = event->GetSelectedTracks();
        m_selection_dirty    = true;
    }
}

}  // namespace View
}  // namespace RocProfVis