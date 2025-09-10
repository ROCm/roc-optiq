// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_sidebar.h"
#include "rocprofvis_data_provider.h"
#include "rocprofvis_flame_track_item.h"
#include "rocprofvis_settings_manager.h"
#include "rocprofvis_font_manager.h"
#include "rocprofvis_timeline_selection.h"
#include "rocprofvis_track_topology.h"
#include "icons/rocprovfis_icon_defines.h"

namespace RocProfVis
{
namespace View
{

constexpr ImGuiTreeNodeFlags CATEGORY_HEADER_FLAGS =
    ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanLabelWidth;
constexpr ImVec2 DEFAULT_WINDOW_PADDING = ImVec2(4.0f, 4.0f);

SideBar::SideBar(std::shared_ptr<TrackTopology>     topology,
                 std::shared_ptr<TimelineSelection> timeline_selection,
                 std::vector<rocprofvis_graph_t>*   graphs,
                 DataProvider &dp)
: m_settings(SettingsManager::GetInstance())
, m_track_topology(topology)
, m_timeline_selection(timeline_selection)
, m_graphs(graphs)
, m_data_provider(dp)
{}

SideBar::~SideBar() {}

void
SideBar::Render()
{
    if(!m_track_topology->Dirty())
    {
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, DEFAULT_WINDOW_PADDING);
        ImGui::PushStyleColor(ImGuiCol_Header, m_settings.GetColor(Colors::kTransparent));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered,
                              m_settings.GetColor(Colors::kTransparent));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive,
                              m_settings.GetColor(Colors::kTransparent));

        const TopologyModel& topology = m_track_topology->GetTopology();
        if(ImGui::CollapsingHeader("Project", CATEGORY_HEADER_FLAGS))
        {
            ImGui::Indent();
            if(!topology.nodes.empty() &&
               ImGui::CollapsingHeader(topology.node_header.c_str(),
                                       CATEGORY_HEADER_FLAGS))
            {
                ImGui::Indent();
                for(const NodeModel& node : topology.nodes)
                {
                    if(node.info)
                    {
                        ImGui::PushID(node.info->id);
                        if(ImGui::CollapsingHeader(node.info->host_name.c_str(),
                                                   CATEGORY_HEADER_FLAGS))
                        {
                            ImGui::Indent();
                            if(!node.processes.empty() &&
                               ImGui::CollapsingHeader(node.process_header.c_str(),
                                                       CATEGORY_HEADER_FLAGS))
                            {
                                ImGui::Indent();
                                for(const ProcessModel& process : node.processes)
                                {
                                    if(process.info)
                                    {
                                        ImGui::PushID(process.info->id);
                                        if(ImGui::CollapsingHeader(process.header.c_str(),
                                                                   CATEGORY_HEADER_FLAGS))
                                        {
                                            ImGui::Indent();
                                            if(!process.queues.empty() &&
                                               ImGui::CollapsingHeader(
                                                   process.queue_header.c_str(),
                                                   CATEGORY_HEADER_FLAGS))
                                            {
                                                ImGui::Indent();
                                                for(const QueueModel& queue :
                                                    process.queues)
                                                {
                                                    if(queue.info)
                                                    {
                                                        RenderTrackItem(
                                                            queue.graph_index);
                                                    }
                                                }
                                                ImGui::Unindent();
                                            }
                                            if(!process.streams.empty() &&
                                               ImGui::CollapsingHeader(
                                                   process.stream_header.c_str(),
                                                   CATEGORY_HEADER_FLAGS))
                                            {
                                                ImGui::Indent();
                                                for(const StreamModel& stream :
                                                    process.streams)
                                                {
                                                    if(stream.info)
                                                    {
                                                        RenderTrackItem(
                                                            stream.graph_index);
                                                    }
                                                }
                                                ImGui::Unindent();
                                            }
                                            if(!process.threads.empty() &&
                                               ImGui::CollapsingHeader(
                                                   process.thread_header.c_str(),
                                                   CATEGORY_HEADER_FLAGS))
                                            {
                                                ImGui::Indent();
                                                for(const ThreadModel& thread :
                                                    process.threads)
                                                {
                                                    if(thread.info)
                                                    {
                                                        RenderTrackItem(
                                                            thread.graph_index);
                                                    }
                                                }
                                                ImGui::Unindent();
                                            }
                                            if(!process.counters.empty() &&
                                               ImGui::CollapsingHeader(
                                                   process.counter_header.c_str(),
                                                   CATEGORY_HEADER_FLAGS))
                                            {
                                                ImGui::Indent();
                                                for(const CounterModel& counter :
                                                    process.counters)
                                                {
                                                    if(counter.info)
                                                    {
                                                        RenderTrackItem(
                                                            counter.graph_index);
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
            if(!topology.uncategorized_graph_indices.empty())
            {
                bool use_header  = !topology.nodes.empty();
                bool header_open = true;
                if(use_header)
                {
                    header_open =
                        ImGui::CollapsingHeader("Uncategorized", CATEGORY_HEADER_FLAGS);
                    ImGui::Indent();
                }
                if(header_open)
                {
                    for(const int& index : topology.uncategorized_graph_indices)
                    {
                        RenderTrackItem(index);
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
        ImGui::PopStyleVar();
    }
}

void
SideBar::Update()
{}

void
SideBar::RenderTrackItem(const int& index)
{
    rocprofvis_graph_t& graph = (*m_graphs)[index];

    ImGui::PushID(graph.chart->GetID());
    ImGui::PushStyleColor(ImGuiCol_Button, m_settings.GetColor(Colors::kTransparent));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                          m_settings.GetColor(Colors::kTransparent));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                          m_settings.GetColor(Colors::kTransparent));
    bool display = graph.display;
    ImGui::PushFont(m_settings.GetFontManager().GetIconFont(FontType::kDefault));
    if(ImGui::Button(display ? ICON_EYE : ICON_EYE_SLASH))
    {
        graph.display = !graph.display;
    }
    ImGui::PopFont();
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, DEFAULT_WINDOW_PADDING);
    if(ImGui::BeginItemTooltip())
    {
        ImGui::TextUnformatted("Toggle Track Visibility");
        ImGui::EndTooltip();
    }
    ImGui::SameLine();
    ImGui::PushFont(m_settings.GetFontManager().GetIconFont(FontType::kDefault));
    if(ImGui::Button(ICON_ARROWS_SHRINK))
    {
        EventManager::GetInstance()->AddEvent(std::make_shared<ScrollToTrackEvent>(
            static_cast<int>(RocEvents::kHandleUserGraphNavigationEvent),
            graph.chart->GetID(), m_data_provider.GetTraceFilePath()));
    }
    ImGui::PopFont();
    if(ImGui::BeginItemTooltip())
    {
        ImGui::TextUnformatted("Scroll To Track");
        ImGui::EndTooltip();
    }
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);
    ImGui::SameLine();
    bool highlight = graph.selected;
    if(!highlight)
    {
        ImGui::PushStyleColor(ImGuiCol_Button, m_settings.GetColor(Colors::kTransparent));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                              m_settings.GetColor(Colors::kTransparent));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                              m_settings.GetColor(Colors::kTransparent));
    }
    if(!display)
    {
        ImGui::PushStyleColor(ImGuiCol_Text,
                              ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    }
    if(ImGui::Button(graph.chart->GetName().c_str()))
    {
        m_timeline_selection->ToggleSelectTrack(graph);
    }
    if(!display)
    {
        ImGui::PopStyleColor();
    }
    if(!highlight)
    {
        ImGui::PopStyleColor(3);
    }
#ifdef ROCPROFVIS_DEVELOPER_MODE
    // Lets you know if component is in Frame. Dev purpose only.
    if(graph.chart->IsInViewVertical())
    {
        ImGui::TextColored(
            ImGui::ColorConvertU32ToFloat4(m_settings.GetColor(Colors::kTextSuccess)),
            "Component Is: ");

        ImGui::SameLine();
        ImGui::TextColored(
            ImGui::ColorConvertU32ToFloat4(m_settings.GetColor(Colors::kTextSuccess)),
            "In Frame ");
    }
    else
    {
        ImGui::TextColored(
            ImGui::ColorConvertU32ToFloat4(m_settings.GetColor(Colors::kTextSuccess)),
            "Component Is: ");

        ImGui::SameLine();
        ImGui::TextColored(
            ImGui::ColorConvertU32ToFloat4(m_settings.GetColor(Colors::kTextError)),
            "Not In Frame by: %f units.", graph.chart->GetDistanceToView());
    }
#endif
    ImGui::PopID();
}

}  // namespace View
}  // namespace RocProfVis