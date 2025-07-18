// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_sidebar.h"
#include "rocprofvis_event_manager.h"
#include "rocprofvis_events.h"
#include "rocprofvis_track_item.h"
#include "rocprofvis_flame_track_item.h"
#include "widgets/rocprofvis_widget.h"

namespace RocProfVis
{
namespace View
{

constexpr ImGuiTreeNodeFlags CATEGORY_HEADER_FLAGS = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanLabelWidth;
constexpr ImGuiTreeNodeFlags TRACK_HEADER_FLAGS = ImGuiTreeNodeFlags_SpanLabelWidth;
constexpr float FRAME_PADDING_X = 4.0f;

SideBar::SideBar(DataProvider& dp, std::vector<rocprofvis_graph_t>* graphs)
: m_data_provider(dp)
, m_graphs(graphs)
, m_settings(Settings::GetInstance())
, m_topology_dirty(true)
, m_graphs_dirty(true)
, m_metadata_changed_event_token(-1)
{
    auto metadata_changed_event_handler = [this](std::shared_ptr<RocEvent> event) 
    {
        m_graphs_dirty = true;
    };
    m_metadata_changed_event_token = EventManager::GetInstance()->Subscribe(static_cast<int>(RocEvents::kTrackMetadataChanged), metadata_changed_event_handler);
}

SideBar::~SideBar()
{
    EventManager::GetInstance()->Unsubscribe(static_cast<int>(RocEvents::kTrackMetadataChanged), m_metadata_changed_event_token);
}

void 
SideBar::Update()
{
    UpdateTopology();
    UpdateGraphs();
}

void
SideBar::UpdateTopology()
{ 
    if(m_topology_dirty && m_data_provider.GetState() == ProviderState::kReady)
    {
        m_project.header = "Project 1";
        m_project.nodes.clear();
        std::vector<const node_info_t*> node_infos = m_data_provider.GetNodeInfoList();
        m_project.nodes.resize(node_infos.size());
        std::vector<std::vector<rocprofvis_graph_t*>> graph_categories(node_infos.size());
        m_project.node_header = "Nodes (" + std::to_string(node_infos.size()) + ")";
        for(int i = 0; i < node_infos.size(); i ++)
        {
            const node_info_t* node_info = node_infos[i];
            if(node_info)
            {            
                m_project.node_lut[node_info->id] = &m_project.nodes[i];
                m_project.nodes[i].info = node_info;
                m_project.nodes[i].info_table = InfoTable{{
                    {InfoTable::Cell{"OS", false}, InfoTable::Cell{node_infos[i]->os_name, false}}, 
                    {InfoTable::Cell{"OS Release", false}, InfoTable::Cell{node_infos[i]->os_release, false}}, 
                    {InfoTable::Cell{"OS Version", false}, InfoTable::Cell{node_infos[i]->os_version, false}}
                }};
                for(const uint64_t& device_id : node_info->device_ids)
                {
                    const device_info_t* device_info = m_data_provider.GetDeviceInfo(device_id);
                    if(device_info)
                    {
                        m_project.nodes[i].info_table.cells.emplace_back(std::vector{InfoTable::Cell{device_info->type + " " + std::to_string(device_info->type_index), false}, InfoTable::Cell{device_info->product_name, false}});
                    }
                }
                const std::vector<uint64_t>& process_ids = node_infos[i]->process_ids;
                m_project.nodes[i].processes.resize(process_ids.size());
                m_project.nodes[i].process_header = "Processes (" + std::to_string(process_ids.size()) + ")";
                for(int j = 0; j < process_ids.size(); j ++)
                {
                    m_project.nodes[i].process_lut[process_ids[j]] = &m_project.nodes[i].processes[j];
                    const process_info_t* process_info = m_data_provider.GetProcessInfo(process_ids[j]);
                    if(process_info)
                    {                   
                        m_project.nodes[i].processes[j].info = process_info;
                        m_project.nodes[i].processes[j].info_table = InfoTable{{
                            {InfoTable::Cell{"Start Time", false}, InfoTable::Cell{std::to_string(process_info->start_time)}}, 
                            {InfoTable::Cell{"End time", false}, InfoTable::Cell{std::to_string(process_info->end_time)}}, 
                            {InfoTable::Cell{"Command", false}, InfoTable::Cell{process_info->command, false}}, 
                            {InfoTable::Cell{"Environment", false}, InfoTable::Cell{process_info->environment, false}}
                        }};
                        m_project.nodes[i].processes[j].header = "[" + std::to_string(process_info->id) + "]";
                        const std::vector<uint64_t>& queue_ids = process_info->queue_ids;
                        m_project.nodes[i].processes[j].queues.resize(queue_ids.size());
                        m_project.nodes[i].processes[j].queue_header = "Queues (" + std::to_string(queue_ids.size()) + ")";
                        for(int k = 0; k < queue_ids.size(); k ++)
                        {
                            m_project.nodes[i].processes[j].queue_lut[queue_ids[k]] = &m_project.nodes[i].processes[j].queues[k];
                            const queue_info_t* queue_info = m_data_provider.GetQueueInfo(queue_ids[k]);
                            if(queue_info)
                            {
                                const device_info_t* device_info = m_data_provider.GetDeviceInfo(queue_info->device_id);
                                m_project.nodes[i].processes[j].queues[k].info = queue_info;
                                if(device_info)
                                {
                                    m_project.nodes[i].processes[j].queues[k].info_table = InfoTable{{
                                        {InfoTable::Cell{device_info->type + " " + std::to_string(device_info->type_index), false}, InfoTable::Cell{device_info->product_name, false}}
                                    }};
                                }
                            }
                        }
                        const std::vector<uint64_t>& thread_ids = process_info->thread_ids;
                        m_project.nodes[i].processes[j].threads.resize(thread_ids.size());
                        m_project.nodes[i].processes[j].thread_header = "Threads (" + std::to_string(thread_ids.size()) + ")";
                        for(int k = 0; k < thread_ids.size(); k ++)
                        {
                            m_project.nodes[i].processes[j].thread_lut[thread_ids[k]] = &m_project.nodes[i].processes[j].threads[k];
                            const thread_info_t* thread_info = m_data_provider.GetThreadInfo(thread_ids[k]);
                            if(thread_info)
                            {
                                m_project.nodes[i].processes[j].threads[k].info = thread_info;
                                m_project.nodes[i].processes[j].threads[k].info_table = InfoTable{{
                                    {InfoTable::Cell{"Start Time", false}, InfoTable::Cell{std::to_string(thread_info->start_time), false}}, 
                                    {InfoTable::Cell{"End time", false}, InfoTable::Cell{std::to_string(thread_info->end_time), false}},
                                }};
                            }
                        }
                        const std::vector<uint64_t>& counter_ids = process_info->counter_ids;
                        m_project.nodes[i].processes[j].counters.resize(counter_ids.size());
                        m_project.nodes[i].processes[j].counter_header = "Counters (" + std::to_string(counter_ids.size()) + ")";
                        for(int k = 0; k < counter_ids.size(); k ++)
                        {
                            m_project.nodes[i].processes[j].counter_lut[counter_ids[k]] = &m_project.nodes[i].processes[j].counters[k];
                            const counter_info_t* counter_info = m_data_provider.GetCounterInfo(counter_ids[k]);
                            if(counter_info)
                            {
                                const device_info_t* device_info = m_data_provider.GetDeviceInfo(counter_info->device_id);
                                m_project.nodes[i].processes[j].counters[k].info = counter_info;
                                m_project.nodes[i].processes[j].counters[k].info_table = InfoTable {{
                                    {InfoTable::Cell{"Description", false}, InfoTable::Cell{counter_info->description, false}},
                                    {InfoTable::Cell{"Units", false}, InfoTable::Cell{counter_info->units, false}},
                                    {InfoTable::Cell{"Value Type", false}, InfoTable::Cell{counter_info->value_type, false}}
                                }};
                                if(device_info)
                                {
                                    m_project.nodes[i].processes[j].counters[k].info_table.cells.push_back({
                                        InfoTable::Cell{device_info->type + " " + std::to_string(device_info->type_index), false}, InfoTable::Cell{device_info->product_name, false}
                                    });
                                }
                            }
                        }
                    }
                }
            }
        }
        m_topology_dirty = false;
    }
}

void 
SideBar::UpdateGraphs()
{
    if(m_graphs_dirty && m_data_provider.GetState() == ProviderState::kReady)
    {
        m_project.uncategorized_graph_indices.clear();
        for(const track_info_t* track : m_data_provider.GetTrackInfoList())
        {
            if(track)
            {
                const uint64_t& node_id = track->topology.node_id;
                const uint64_t& process_id = track->topology.process_id;
                const uint64_t& id = track->topology.id;
                const uint64_t& index = track->index;
                switch(track->topology.type)
                {
                    case track_info_t::Topology::Queue:
                    {
                        m_project.node_lut[node_id]->process_lut[process_id]->queue_lut[id]->graph_index = index;
                        break;
                    }
                    case track_info_t::Topology::Thread:
                    {
                        m_project.node_lut[node_id]->process_lut[process_id]->thread_lut[id]->graph_index = index;
                        break;
                    }
                    case track_info_t::Topology::Counter:
                    {
                        m_project.node_lut[node_id]->process_lut[process_id]->counter_lut[id]->graph_index = index;
                        break;
                    }
                    default: 
                    {
                        m_project.uncategorized_graph_indices.push_back(index);
                        break;
                    }
                }
            }            
        }
        m_graphs_dirty = false;
    }
}

void
SideBar::Render()
{
    if (!m_topology_dirty && !m_graphs_dirty)
    {
        //Disable horizontal scrolling
        ImGui::SetNextWindowContentSize(ImVec2(ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ScrollbarSize, 0));
        ImGui::BeginChild("sidebar_scrollable");

        ImGui::PushStyleColor(ImGuiCol_Header, m_settings.GetColor(static_cast<int>(Colors::kTransparent)));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, m_settings.GetColor(static_cast<int>(Colors::kTransparent)));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive,m_settings.GetColor(static_cast<int>(Colors::kTransparent)));

        if(ImGui::CollapsingHeader(m_project.header.c_str(), CATEGORY_HEADER_FLAGS))
        {
            ImGui::Indent();
            if(!m_project.nodes.empty() && ImGui::CollapsingHeader(m_project.node_header.c_str(), CATEGORY_HEADER_FLAGS))
            {
                ImGui::Indent();
                for(Node& node : m_project.nodes)
                {
                    if(node.info)
                    {                    
                        ImGui::PushID(node.info->id);
                        if(ImGui::CollapsingHeader(node.info->host_name.c_str(), CATEGORY_HEADER_FLAGS))
                        { 
                            ImGui::Indent();
                            RenderTable(node.info_table);
                            if(!node.processes.empty() && ImGui::CollapsingHeader(node.process_header.c_str(), CATEGORY_HEADER_FLAGS))
                            {                            
                                ImGui::Indent();
                                for(Process& process : node.processes)
                                {                        
                                    if(process.info)
                                    {                                    
                                        ImGui::PushID(process.info->id);
                                        if(ImGui::CollapsingHeader(process.header.c_str(), CATEGORY_HEADER_FLAGS))
                                        {           
                                            ImGui::Indent();
                                            RenderTable(process.info_table);
                                            if(!process.queues.empty() && ImGui::CollapsingHeader(process.queue_header.c_str(), CATEGORY_HEADER_FLAGS))
                                            {
                                                ImGui::Indent();
                                                for(Queue& queue : process.queues)
                                                {
                                                    if(queue.info)
                                                    {                                                    
                                                        ImGui::PushID(queue.info->name.c_str());
                                                        if(ImGui::CollapsingHeader(queue.info->name.c_str(), TRACK_HEADER_FLAGS))
                                                        {
                                                            ImGui::Indent();
                                                            ImGui::BeginGroup();
                                                            RenderTable(queue.info_table);
                                                            RenderTrackItem(queue.graph_index);
                                                            ImGui::EndGroup();
                                                            ImGui::GetWindowDrawList()->AddRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), ImGui::GetColorU32(ImGui::GetStyleColorVec4(ImGuiCol_TableBorderLight)));
                                                            ImGui::Unindent();
                                                        }
                                                        ImGui::PopID();
                                                    }
                                                }
                                                ImGui::Unindent();
                                            }                                    
                                            if(!process.threads.empty() && ImGui::CollapsingHeader(process.thread_header.c_str(), CATEGORY_HEADER_FLAGS))
                                            {                            
                                                ImGui::Indent();
                                                for(Thread& thread : process.threads)
                                                {
                                                    if(thread.info)
                                                    {                                                    
                                                        ImGui::PushID(thread.info->name.c_str());
                                                        if(ImGui::CollapsingHeader(thread.info->name.c_str(), TRACK_HEADER_FLAGS))
                                                        {
                                                            ImGui::Indent();
                                                            ImGui::BeginGroup();
                                                            RenderTable(thread.info_table);
                                                            RenderTrackItem(thread.graph_index);
                                                            ImGui::EndGroup();
                                                            ImGui::GetWindowDrawList()->AddRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), ImGui::GetColorU32(ImGui::GetStyleColorVec4(ImGuiCol_TableBorderLight)));
                                                            ImGui::Unindent();
                                                        }
                                                        ImGui::PopID();
                                                    }
                                                }
                                                ImGui::Unindent();
                                            }
                                            if(!process.counters.empty() && ImGui::CollapsingHeader(process.counter_header.c_str(), CATEGORY_HEADER_FLAGS))
                                            {                            
                                                ImGui::Indent();
                                                for(Counter& counter : process.counters)
                                                {
                                                    if(counter.info)
                                                    {
                                                        ImGui::PushID(counter.info->name.c_str());
                                                        if(ImGui::CollapsingHeader(counter.info->name.c_str(), TRACK_HEADER_FLAGS))
                                                        {
                                                            ImGui::Indent();
                                                            ImGui::BeginGroup();
                                                            RenderTable(counter.info_table);
                                                            RenderTrackItem(counter.graph_index);
                                                            ImGui::EndGroup();
                                                            ImGui::GetWindowDrawList()->AddRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), ImGui::GetColorU32(ImGui::GetStyleColorVec4(ImGuiCol_TableBorderLight)));
                                                            ImGui::Unindent();
                                                        }
                                                        ImGui::PopID();
                                                    }
                                                }
                                                ImGui::Unindent();
                                            }
                                            ImGui::Unindent();
                                        }
                                        ImGui::PopID();
                                    }
                                }
                                ImGui::Unindent();
                            }
                            ImGui::Unindent();
                        }
                        ImGui::PopID();
                    }
                }
                ImGui::Unindent();
            }           
            if(!m_project.uncategorized_graph_indices.empty())
            {                
                bool use_header = !m_project.nodes.empty();
                bool header_open = true; 
                if(use_header)
                {                    
                    header_open = ImGui::CollapsingHeader("Uncategorized", CATEGORY_HEADER_FLAGS);
                    ImGui::Indent();
                }
                if(header_open)
                {
                    for(const int& index : m_project.uncategorized_graph_indices)
                    {
                        ImGui::PushID((*m_graphs)[index].chart->GetID());
                        std::string track_name = (*m_graphs)[index].chart->GetName();
                        if(ImGui::CollapsingHeader(track_name.c_str()))
                        {
                            ImGui::Indent();
                            ImGui::BeginGroup();
                            RenderTrackItem(index);
                            ImGui::EndGroup();
                            ImGui::GetWindowDrawList()->AddRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), ImGui::GetColorU32(ImGui::GetStyleColorVec4(ImGuiCol_TableBorderLight)));
                            ImGui::Unindent();
                        }                  
                        ImGui::PopID();
                    }
                }
                if(use_header)
                {
                    ImGui::Unindent();
                }                
            }
            ImGui::Unindent();
        }
        ImGui::PopStyleColor(3);
        ImGui::EndChild();
    }
}

void
SideBar::RenderTable(InfoTable& table)
{
    if(!table.cells.empty() && !table.cells[0].empty())
    {   
        const int& rows = table.cells.size();
        const int& cols = table.cells[0].size();
        float table_x_min = ImGui::GetCursorPosX();
        float table_width = ImGui::GetContentRegionAvail().x - FRAME_PADDING_X;
        float table_x_max = table_x_min + table_width;
        if(ImGui::BeginTable("", cols, ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoKeepColumnsVisible,  ImVec2(table_width, 0)))
        {    
            for(int r = 0; r < rows; r ++)
            {
                ImGui::PushID(r);
                ImGui::TableNextRow();
                for(int c = 0; c < cols; c ++)
                {
                    ImGui::PushID(c);
                    ImGui::TableSetColumnIndex(c);
                    const char* data = table.cells[r][c].data.c_str();
                    bool& expand = table.cells[r][c].expand;
                    ImVec2& cursor_pos = ImGui::GetCursorPos();
                    bool elide = cursor_pos.x + ImGui::CalcTextSize(data).x > table_x_max;
                    if(elide)
                    {
                        ImGuiStyle& style = ImGui::GetStyle();
                        if(expand)
                        {
                            ImGui::PushTextWrapPos(table_x_max  - (ImGui::CalcTextSize("-").x + 2 * (style.FramePadding.x + style.CellPadding.x)));
                        }
                        else
                        {
                            ImVec2& screen_pos = ImGui::GetCursorScreenPos();
                            ImGui::PushClipRect(screen_pos, ImVec2(table_x_max - (ImGui::CalcTextSize("-").x + 2 * (style.FramePadding.x + style.CellPadding.x)), screen_pos.y + ImGui::GetTextLineHeightWithSpacing()), true);
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
                        ImGui::SetCursorPos(ImVec2(table_x_max - (ImGui::CalcTextSize("-").x + 2 * style.FramePadding.x + style.CellPadding.x), cursor_pos.y));
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
SideBar::RenderTrackItem(const int& index)
{
    rocprofvis_graph_t& graph = (*m_graphs)[index]; 
    ImGui::PushID(index);
    ImGui::Dummy(ImVec2(ImGui::GetContentRegionAvail().x - FRAME_PADDING_X, 0));
    ImGui::PushTextWrapPos(ImGui::GetItemRectMax().x);
    std::string label = graph.chart->GetName() + " ID:" + std::to_string(graph.chart->GetID());
    ImGui::Text(label.c_str());
    if(ImGui::Button("Go To Track"))
    {
        auto evt = std::make_shared<ScrollToTrackByNameEvent>(
            static_cast<int>(RocEvents::kHandleUserGraphNavigationEvent),
            graph.chart->GetName());
        EventManager::GetInstance()->AddEvent(evt);
    }
    ImGui::Checkbox(
            "Enable/Disable Track",
            &graph.display);
    if(graph.graph_type == rocprofvis_graph_t::TYPE_FLAMECHART)
    {
        if(ImGui::Checkbox(
                "Enable/Disable Color",
                &graph.colorful_flamechart))

        {
            static_cast<FlameTrackItem*>(graph.chart)
                ->SetRandomColorFlag(graph.colorful_flamechart);
        }
    }
    if(graph.graph_type == rocprofvis_graph_t::TYPE_LINECHART)
    {
        ImGui::Checkbox(
                "Color By Value",
                &graph.color_by_value);

        ImGui::Checkbox(
                "Convert to Boxplot",
                &graph.make_boxplot);

        if(graph.color_by_value)
        {
            ImGui::Text("Color By Value");
            ImGui::Spacing();
            ImGui::PushItemWidth(40.0f);
            // Use this menu to expand user options in the future
            ImGui::InputFloat(
                "Region of Interest Min",
                &graph.color_by_value_digits.interest_1_min);

            ImGui::InputFloat(
                "Region of Interest Max",
                &graph.color_by_value_digits.interest_1_max);

            ImGui::PopItemWidth();
        }
    }
#ifdef ROCPROFVIS_DEVELOPER_MODE
    // Lets you know if component is in Frame. Dev purpose only.
    if(graph.chart->IsInViewVertical())
    {
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(m_settings.GetColor(
                                static_cast<int>(Colors::kTextSuccess))),
                            "Component Is: ");

        ImGui::SameLine();
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(m_settings.GetColor(
                                static_cast<int>(Colors::kTextSuccess))),
                            "In Frame ");
    }
    else
    {
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(m_settings.GetColor(
                                static_cast<int>(Colors::kTextSuccess))),
                            "Component Is: ");

        ImGui::SameLine();
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(m_settings.GetColor(
                                           static_cast<int>(Colors::kTextError))),
                                        "Not In Frame by: %f units.", graph.chart->GetDistanceToView());
    }
#endif
    ImGui::PopTextWrapPos();
    ImGui::PopID();
}

}  // namespace View
}  // namespace RocProfVis