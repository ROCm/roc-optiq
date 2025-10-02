// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_sidebar.h"
#include "icons/rocprovfis_icon_defines.h"
#include "rocprofvis_data_provider.h"
#include "rocprofvis_flame_track_item.h"
#include "rocprofvis_font_manager.h"
#include "rocprofvis_settings_manager.h"
#include "rocprofvis_timeline_selection.h"


namespace RocProfVis
{
namespace View
{

constexpr ImGuiTreeNodeFlags CATEGORY_HEADER_FLAGS =
    ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanLabelWidth;
constexpr ImVec2 DEFAULT_WINDOW_PADDING = ImVec2(4.0f, 4.0f);

SideBar::SideBar(std::shared_ptr<TrackTopology>     topology,
                 std::shared_ptr<TimelineSelection> timeline_selection,
                 std::shared_ptr<std::vector<rocprofvis_graph_t>> graphs,
                 DataProvider&                                    dp)
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


        if(m_root_eye_button_state == EyeButtonState::kAllVisible)
        {
            topology.all_subitems_hidden = false;
        }

        if(topology.all_subitems_hidden)
        {
            m_root_eye_button_state = DrawEyeButton(EyeButtonState::kAllHidden);
        }
        else
        {
            m_root_eye_button_state = DrawEyeButton(m_root_eye_button_state);
        }
        ImGui::SameLine();

        if(ImGui::TreeNodeEx("Project",
                             CATEGORY_HEADER_FLAGS | ImGuiTreeNodeFlags_Framed))
        {
            if (!topology.nodes.empty())
            {
                m_root_eye_button_state =
                    DrawTopology(topology, m_root_eye_button_state, false);

                if(m_root_eye_button_state == EyeButtonState::kAllHidden)
                {
                    topology.all_subitems_hidden = true;
                }
                else
                {
                    topology.all_subitems_hidden = false;
                }          
            }
            if(m_root_eye_button_state == EyeButtonState::kAllHidden)
            {
                HideAllUncategorizedItems(topology.uncategorized_graph_indices);
            }
            else if(m_root_eye_button_state == EyeButtonState::kAllVisible)
            {
                UnhideAllUncategorizedItems(topology.uncategorized_graph_indices);
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
                        if(RenderTrackItem(index))
                        {
                            m_root_eye_button_state = EyeButtonState::kMixed;
                        }
                    }
                }
                if(use_header)
                {
                    ImGui::Unindent();
                }
            }
            ImGui::TreePop();
            ImGui::Unindent();
        }
        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar();
    }
}

void
SideBar::Update()
{}

bool
SideBar::RenderTrackItem(const int& index)
{
    bool stateChanged = false;

    if(!m_graphs || m_graphs->empty())
    {
        return stateChanged;
    }
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
        graph.display         = !graph.display;
        graph.display_changed = true;
        stateChanged = true;
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
    if (!display)
    {
        ImGui::PushStyleColor(ImGuiCol_Text,
            ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    }
    if (ImGui::Button(graph.chart->GetName().c_str()))
    {
        m_timeline_selection->ToggleSelectTrack(graph);
    }
    if (!display)
    {
        ImGui::PopStyleColor();
    }
    if (!highlight)
    {
        ImGui::PopStyleColor(3);
    }
#ifdef ROCPROFVIS_DEVELOPER_MODE
    // Lets you know if component is in Frame. Dev purpose only.
    if (graph.chart->IsInViewVertical())
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
    return stateChanged;
}

bool
SideBar::IsAllSubItemsHidden(const std::vector<IterableModel>& container)
{
    bool all_hidden = true;
    if (m_graphs && !m_graphs->empty())
    {
        for (const IterableModel& elem : container)
        {
            rocprofvis_graph_t& graph = (*m_graphs)[elem.graph_index];
            if (graph.display)
            {
                all_hidden = false;
                break;
            }
        }
    }
    return all_hidden;
}

void
SideBar::HideAllSubItems(const std::vector<IterableModel>& container)
{
    if (m_graphs && !m_graphs->empty())
    {
        for (const IterableModel& elem : container)
        {
            rocprofvis_graph_t& graph = (*m_graphs)[elem.graph_index];
            if (graph.display == true)
            {
                graph.display         = false;
                graph.display_changed = true;
            }
        }
    }
}

void
SideBar::HideAllUncategorizedItems(const std::vector<uint64_t>& indices)
{
    if (m_graphs && !m_graphs->empty())
    {
        for (const uint64_t& index : indices)
        {
            rocprofvis_graph_t& graph = (*m_graphs)[index];
            if(graph.display == true)
            {
                graph.display         = false;
                graph.display_changed = true;
            }
        }
    }
}

void
SideBar::UnhideAllUncategorizedItems(const std::vector<uint64_t>& indices)
{
    if(m_graphs && !m_graphs->empty())
    {
        for(const uint64_t& index : indices)
        {
            rocprofvis_graph_t& graph = (*m_graphs)[index];
            if(graph.display == false)
            {
                graph.display         = true;
                graph.display_changed = true;
            }            
        }
    }
}

void
SideBar::UnhideAllSubItems(const std::vector<IterableModel>& container)
{
    if (m_graphs && !m_graphs->empty())
    {
        for (const IterableModel& elem : container)
        {
            rocprofvis_graph_t& graph = (*m_graphs)[elem.graph_index];
            if(graph.display == false)
            {
                graph.display         = true;
                graph.display_changed = true;
            }            
        }
    }
}

SideBar::EyeButtonState
SideBar::DrawTopology(const TopologyModel& topology,
    EyeButtonState parentEyeButtonState, bool show_eye_button)
{
    if(parentEyeButtonState == EyeButtonState::kAllVisible)
    {
        topology.all_subitems_hidden = false;
    }
    ImGui::Indent();
    EyeButtonState newButtonState = parentEyeButtonState;
    if(show_eye_button)
    {
        if(topology.all_subitems_hidden)
        {
            newButtonState = DrawEyeButton(EyeButtonState::kAllHidden);
        }
        else
        {
            newButtonState = DrawEyeButton(parentEyeButtonState);
        }
        ImGui::SameLine();
    }

    if(ImGui::TreeNodeEx(topology.node_header.c_str(),
                         CATEGORY_HEADER_FLAGS | ImGuiTreeNodeFlags_Framed))
    {
        newButtonState = DrawNodes(topology.nodes, newButtonState, false);
        if(newButtonState == EyeButtonState::kAllHidden)
        {
            topology.all_subitems_hidden = true;
        }
        else
        {
            topology.all_subitems_hidden = false;
        }
        ImGui::TreePop();
    }
    return newButtonState;
}

SideBar::EyeButtonState
SideBar::DrawNodes(const std::vector<NodeModel>& nodes,
    EyeButtonState parentEyeButtonState,
    bool show_eye_button)
{
    if (parentEyeButtonState == EyeButtonState::kAllVisible)
    {
        for (const auto& node : nodes)
        {
            node.all_subitems_hidden = false;
        }
    }
    EyeButtonState newButtonState = parentEyeButtonState;
    for(const NodeModel& node : nodes)
    {
        if(node.info)
        {
            if(show_eye_button)
            {
                if(node.all_subitems_hidden)
                {
                    newButtonState = DrawEyeButton(EyeButtonState::kAllHidden);
                }
                else
                {
                    newButtonState = DrawEyeButton(parentEyeButtonState);
                }
                ImGui::SameLine();
            }

            ImGui::PushID(node.info->id);
            if(ImGui::TreeNodeEx(node.info->host_name.c_str(),
                                 CATEGORY_HEADER_FLAGS | ImGuiTreeNodeFlags_Framed))
            {
                newButtonState = DrawNode(node, newButtonState, false);
                if(newButtonState == EyeButtonState::kAllHidden)
                {
                    node.all_subitems_hidden = true;
                }
                else
                {
                    node.all_subitems_hidden = false;
                }
                ImGui::TreePop();
            }
            ImGui::PopID();
        }
    }
    return newButtonState;
}

SideBar::EyeButtonState
SideBar::DrawNode(const NodeModel& node,
    EyeButtonState parentEyeButtonState, bool show_eye_button)
{
    if(parentEyeButtonState == EyeButtonState::kAllVisible)
    {
        node.all_subitems_hidden = false;
    }
    EyeButtonState newButtonState = parentEyeButtonState;

    if(show_eye_button)
    {
        if(node.all_subitems_hidden)
        {
            newButtonState = DrawEyeButton(EyeButtonState::kAllHidden);
        }
        else
        {
            newButtonState = DrawEyeButton(parentEyeButtonState);
        }
        ImGui::SameLine();
    }

    bool open = ImGui::TreeNodeEx(node.process_header.c_str(),
                                  CATEGORY_HEADER_FLAGS | ImGuiTreeNodeFlags_Framed);

    if(!node.processes.empty() && open)
    {
        newButtonState = DrawProcesses(node.processes, newButtonState, false);
        if(newButtonState == EyeButtonState::kAllHidden)
        {
            node.all_subitems_hidden = true;
        }
        else
        {
            node.all_subitems_hidden = false;
        }

        ImGui::TreePop();
    }
    return newButtonState;
}

SideBar::EyeButtonState
SideBar::DrawProcesses(const std::vector<ProcessModel>& processes,
                       EyeButtonState                   parentEyeButtonState,
                       bool                             show_eye_button)
{
    if(parentEyeButtonState == EyeButtonState::kAllVisible)
    {
        for(const auto& process : processes)
        {
            process.all_subitems_hidden = false;
        }
    }

    EyeButtonState allProcessesState = EyeButtonState::kAllHidden;
    for(const ProcessModel& process : processes)     
    {
        {
            EyeButtonState queueButtonState   = parentEyeButtonState;
            EyeButtonState streamButtonState  = parentEyeButtonState;
            EyeButtonState threadButtonState  = parentEyeButtonState;
            EyeButtonState counterButtonState = parentEyeButtonState;
            ImGui::PushID(process.info->id);

            EyeButtonState currentEyeButtonState = parentEyeButtonState;
            if (show_eye_button)
            {
                if(process.all_subitems_hidden)
                {
                    currentEyeButtonState = DrawEyeButton(EyeButtonState::kAllHidden);
                }
                else
                {
                    currentEyeButtonState = DrawEyeButton(parentEyeButtonState);
                }
                ImGui::SameLine();
            }
            
            if(ImGui::TreeNodeEx(process.header.c_str(),
                                 CATEGORY_HEADER_FLAGS | ImGuiTreeNodeFlags_Framed))
            {
                ImGui::Indent();
                if(!process.queues.empty())
                {
                    queueButtonState = DrawCollapsable(
                        process.queues, process.queue_header, currentEyeButtonState);
                }
                if(!process.streams.empty())
                {
                    streamButtonState = DrawCollapsable(
                        process.streams, process.stream_header, currentEyeButtonState);
                }
                if(!process.threads.empty())
                {
                    threadButtonState = DrawCollapsable(
                        process.threads, process.thread_header, currentEyeButtonState);
                }
                if(!process.counters.empty())
                {
                    counterButtonState = DrawCollapsable(
                        process.counters, process.counter_header, currentEyeButtonState);
                }
                ImGui::TreePop();
                ImGui::Unindent();
            }

            if(queueButtonState == EyeButtonState::kAllHidden &&
               streamButtonState == EyeButtonState::kAllHidden &&
               threadButtonState == EyeButtonState::kAllHidden &&
               counterButtonState == EyeButtonState::kAllHidden)
            {
                process.all_subitems_hidden = true;
                allProcessesState = EyeButtonState::kAllHidden;
            }
            else
            {
                process.all_subitems_hidden = false;
                allProcessesState = EyeButtonState::kMixed;
            }
            ImGui::PopID();
        }
    }
    return allProcessesState;
}

SideBar::EyeButtonState
SideBar::DrawCollapsable(const std::vector<IterableModel>& container,
                         const std::string&                collapsable_header,
                         EyeButtonState                    parentEyeButtonState)
{
    ImGui::PushID(collapsable_header.c_str());
    if(parentEyeButtonState == EyeButtonState::kAllVisible)
    if(parentEyeButtonState == EyeButtonState::kAllVisible)
    {
        UnhideAllSubItems(container);
    }
    EyeButtonState newButtonState = parentEyeButtonState;
    if(IsAllSubItemsHidden(container))
    {
        newButtonState = DrawEyeButton(EyeButtonState::kAllHidden);        
    }
    else
    {
        newButtonState = DrawEyeButton(parentEyeButtonState);
    }
    
    if(newButtonState == EyeButtonState::kAllHidden)
    {
        HideAllSubItems(container);
    }
    else if(newButtonState == EyeButtonState::kAllVisible)
    {
        UnhideAllSubItems(container);
    }

    ImGui::SameLine();
    bool open = ImGui::TreeNodeEx(collapsable_header.c_str(),
                                  CATEGORY_HEADER_FLAGS | ImGuiTreeNodeFlags_Framed);
    if(open)
    {
        ImGui::Indent();
        for(const IterableModel& item : container)
        {
            if(item.info)
            {
                if(RenderTrackItem(item.graph_index))
                    newButtonState = EyeButtonState::kMixed;
            }
        }
        ImGui::TreePop();
        ImGui::Unindent();
    }
    ImGui::PopID();
    return newButtonState;
}

SideBar::EyeButtonState
SideBar::DrawEyeButton(EyeButtonState eyeButtonState)
{
    ImGui::PushStyleColor(ImGuiCol_Button, m_settings.GetColor(Colors::kTransparent));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                          m_settings.GetColor(Colors::kTransparent));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                          m_settings.GetColor(Colors::kTransparent));
    ImGui::PushFont(m_settings.GetFontManager().GetIconFont(FontType::kDefault));

    ImVec2 eye_size = ImGui::CalcTextSize(ICON_EYE);
    float  button_w = eye_size.x + ImGui::GetStyle().FramePadding.x * 2;
    float  button_h = eye_size.y + ImGui::GetStyle().FramePadding.y * 2;

    EyeButtonState newButtonState = eyeButtonState;
    if(ImGui::Button(eyeButtonState == EyeButtonState::kAllHidden ? ICON_EYE_SLASH
                                                                  : ICON_EYE,
                     ImVec2(button_w, button_h)))
    {
        if(eyeButtonState == EyeButtonState::kAllHidden)
        {
            newButtonState = EyeButtonState::kAllVisible;
        }
        else if(eyeButtonState == EyeButtonState::kAllVisible ||
                eyeButtonState == EyeButtonState::kMixed)
        {
            newButtonState = EyeButtonState::kAllHidden;
        }
    }
    ImGui::PopFont();
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, DEFAULT_WINDOW_PADDING);
    if(ImGui::BeginItemTooltip())
    {
        ImGui::TextUnformatted("Toggle All Track Visibility");
        ImGui::EndTooltip();
    }
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);

    return newButtonState;
}

bool
SideBar::IsEyeButtonVisible()
{
    ImVec2 sidebar_min  = ImGui::GetWindowPos();
    ImVec2 sidebar_size = ImGui::GetWindowSize();
    ImVec2 sidebar_max =
        ImVec2(sidebar_min.x + sidebar_size.x, sidebar_min.y + sidebar_size.y);

    ImVec2 line_start = ImGui::GetCursorScreenPos();

    float  line_height = ImGui::GetFontSize() + ImGui::GetStyle().FramePadding.y * 2.0f;
    ImVec2 line_end = ImVec2(line_start.x + sidebar_size.x, line_start.y + line_height);

    ImVec2 mouse_pos = ImGui::GetMousePos();

    bool mouse_in_sidebar = mouse_pos.x >= sidebar_min.x && mouse_pos.x <= sidebar_max.x;
    bool mouse_on_line    = mouse_pos.y >= line_start.y && mouse_pos.y <= line_end.y;

    return mouse_in_sidebar && mouse_on_line;
}
}  // namespace View
}  // namespace RocProfVis
