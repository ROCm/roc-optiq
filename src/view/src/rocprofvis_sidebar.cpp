// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_sidebar.h"
#include "icons/rocprovfis_icon_defines.h"
#include "widgets/rocprofvis_gui_helpers.h"
#include "rocprofvis_data_provider.h"
#include "rocprofvis_flame_track_item.h"
#include "rocprofvis_font_manager.h"
#include "rocprofvis_settings_manager.h"
#include "rocprofvis_timeline_selection.h"

#include <algorithm>

namespace RocProfVis
{
namespace View
{

constexpr ImGuiTreeNodeFlags CATEGORY_HEADER_FLAGS =
    ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanLabelWidth;
constexpr ImVec2 DEFAULT_WINDOW_PADDING = ImVec2(4.0f, 4.0f);
constexpr float  TREE_LINE_W           = 1.5f;

class TreeConnector
{
public:
    explicit TreeConnector(SettingsManager& s)
    {
        float indent = ImGui::GetStyle().IndentSpacing;
        m_draw_list  = ImGui::GetWindowDrawList();
        m_color      = s.GetColor(Colors::kMetaDataSeparator);
        m_line_x     = ImGui::GetCursorScreenPos().x - indent * 0.5f;
        m_branch_len = indent * 0.45f;
        m_prev_y     = ImGui::GetCursorScreenPos().y;
    }

    void Branch()
    {
        float mid_y = ImGui::GetCursorScreenPos().y + ImGui::GetFrameHeight() * 0.5f;
        m_draw_list->AddLine(ImVec2(m_line_x, m_prev_y),
                             ImVec2(m_line_x, mid_y), m_color, TREE_LINE_W);
        m_draw_list->AddLine(ImVec2(m_line_x, mid_y),
                             ImVec2(m_line_x + m_branch_len, mid_y), m_color, TREE_LINE_W);
        m_prev_y = mid_y;
    }

private:
    ImDrawList* m_draw_list  = nullptr;
    ImU32       m_color      = 0;
    float       m_line_x     = 0;
    float       m_branch_len = 0;
    float       m_prev_y     = 0;
};

SideBar::SideBar(std::shared_ptr<TrackTopology>                   topology,
                 std::shared_ptr<TimelineSelection>               timeline_selection,
                 std::shared_ptr<std::vector<TrackGraph>> graphs,
                 DataProvider&                                    dp)
: m_settings(SettingsManager::GetInstance())
, m_track_topology(topology)
, m_timeline_selection(timeline_selection)
, m_graphs(graphs)
, m_data_provider(dp)
, m_root_eye_button_state(EyeButtonState::kMixed)
{}

SideBar::~SideBar() {}

void
SideBar::Render()
{
    if(!m_track_topology->Dirty())
    {
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(3, 2));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, 14.0f);
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered,
                              ImGui::ColorConvertU32ToFloat4(
                                  m_settings.GetColor(Colors::kBgFrame)));

        const TopologyModel& topology = m_track_topology->GetTopology();
        if(ImGui::TreeNodeEx("Project",
                             CATEGORY_HEADER_FLAGS | ImGuiTreeNodeFlags_Framed))
        {
            TreeConnector project_tc(m_settings);
            if(!topology.nodes.empty())
            {
                project_tc.Branch();
                m_root_eye_button_state =
                    DrawTopology(topology, m_root_eye_button_state, true);

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
                    project_tc.Branch();
                    header_open =
                        ImGui::CollapsingHeader("Uncategorized", CATEGORY_HEADER_FLAGS);
                    ImGui::Indent();
                }
                if(header_open)
                {
                    for(const uint64_t& index : topology.uncategorized_graph_indices)
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
        }
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(4);
    }
}

void
SideBar::Update()
{}

bool
SideBar::RenderTrackItem(const uint64_t& index)
{
    bool state_changed = false;

    if(!m_graphs || m_graphs->empty())
    {
        return state_changed;
    }
    TrackGraph& graph = (*m_graphs)[index];

    ImGui::PushID(static_cast<int>(graph.chart->GetID()));
    ImGui::PushStyleColor(ImGuiCol_Button, m_settings.GetColor(Colors::kTransparent));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2, 0));
    bool display = graph.display;
    ImGui::PushFont(m_settings.GetFontManager().GetIconFont(FontType::kDefault));
    if(ImGui::Button(display ? ICON_EYE : ICON_EYE_SLASH))
    {
        graph.display         = !graph.display;
        graph.display_changed = true;
        state_changed         = true;
        if(!graph.display)
        {
            m_data_provider.DataModel().GetTimeline().UpdateHistogram(
                { graph.chart->GetID() }, false);
        }
        else
        {
            m_data_provider.DataModel().GetTimeline().UpdateHistogram(
                { graph.chart->GetID() }, true);
        }
    }

    ImGui::PopFont();
    if(ImGui::IsItemHovered())
        SetTooltipStyled("Toggle Track Visibility");
    ImGui::SameLine();
    ImGui::PushFont(m_settings.GetFontManager().GetIconFont(FontType::kDefault));
    if(ImGui::Button(ICON_ARROWS_SHRINK))
    {
        EventManager::GetInstance()->AddEvent(std::make_shared<ScrollToTrackEvent>(
            static_cast<int>(RocEvents::kHandleUserGraphNavigationEvent),
            graph.chart->GetID(), m_data_provider.GetTraceFilePath()));
    }
    ImGui::PopFont();
    if(ImGui::IsItemHovered())
        SetTooltipStyled("Scroll To Track");
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
    ImGui::SameLine();
    bool highlight = graph.selected;
    if(!highlight)
    {
        ImGui::PushStyleColor(ImGuiCol_Button, m_settings.GetColor(Colors::kTransparent));
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
        ImGui::PopStyleColor();
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
    return state_changed;
}

template<typename Model>
bool
SideBar::IsAllSubItemsHidden(const std::vector<Model>& container)
{
    bool all_hidden = true;
    if(m_graphs && !m_graphs->empty())
    {
        for(const Model& elem : container)
        {
            TrackGraph& graph = (*m_graphs)[elem.graph_index];
            if(graph.display)
            {
                all_hidden = false;
                break;
            }
            if constexpr (std::is_same_v<Model, StreamModel>)
            {
                for(const auto& proc : elem.processors)
                {
                    if(!IsAllSubItemsHidden(proc.queues))
                    {
                        all_hidden = false;
                        break;
                    }
                }
                if(!all_hidden) break;
            }
        }
    }
    return all_hidden;
}

template<typename Model>
void
SideBar::HideAllSubItems(const std::vector<Model>& container)
{
    if(m_graphs && !m_graphs->empty())
    {
        std::vector<uint64_t> ids_to_remove;
        for(const Model& elem : container)
        {
            TrackGraph& graph = (*m_graphs)[elem.graph_index];
            if(graph.display == true)
            {
                graph.display         = false;
                graph.display_changed = true;
                ids_to_remove.push_back(graph.chart->GetID());
            }
        }
        if(!ids_to_remove.empty())
        {
            m_data_provider.DataModel().GetTimeline().UpdateHistogram(ids_to_remove, false);
        }
        if constexpr (std::is_same_v<Model, StreamModel>)
        {
            for(const Model& elem : container)
            {
                for(const auto& proc : elem.processors)
                {
                    HideAllSubItems(proc.queues);
                }
            }
        }
    }
}

void
SideBar::HideAllUncategorizedItems(const std::vector<uint64_t>& indices)
{
    if(m_graphs && !m_graphs->empty())
    {
        std::vector<uint64_t> ids_to_remove;
        for(const uint64_t& index : indices)
        {
            TrackGraph& graph = (*m_graphs)[index];
            if(graph.display == true)
            {
                graph.display         = false;
                graph.display_changed = true;
                ids_to_remove.push_back(graph.chart->GetID());
            }
        }
        if(!ids_to_remove.empty())
        {
            m_data_provider.DataModel().GetTimeline().UpdateHistogram(ids_to_remove, false);
        }
    }
}

void
SideBar::UnhideAllUncategorizedItems(const std::vector<uint64_t>& indices)
{
    if(m_graphs && !m_graphs->empty())
    {
        std::vector<uint64_t> ids_to_add;
        for(const uint64_t& index : indices)
        {
            TrackGraph& graph = (*m_graphs)[index];
            if(graph.display == false)
            {
                graph.display         = true;
                graph.display_changed = true;
                ids_to_add.push_back(graph.chart->GetID());
            }
        }
        if(!ids_to_add.empty())
        {
            m_data_provider.DataModel().GetTimeline().UpdateHistogram(ids_to_add, true);
        }
    }
}

template<typename Model>
void
SideBar::UnhideAllSubItems(const std::vector<Model>& container)
{
    if(m_graphs && !m_graphs->empty())
    {
        std::vector<uint64_t> ids_to_add;
        for(const Model& elem : container)
        {
            TrackGraph& graph = (*m_graphs)[elem.graph_index];
            if(graph.display == false)
            {
                graph.display         = true;
                graph.display_changed = true;
                ids_to_add.push_back(graph.chart->GetID());
            }
        }
        if(!ids_to_add.empty())
        {
            m_data_provider.DataModel().GetTimeline().UpdateHistogram(ids_to_add, true);
        }
        if constexpr (std::is_same_v<Model, StreamModel>)
        {
            for(const Model& elem : container)
            {
                for(const auto& proc : elem.processors)
                {
                    UnhideAllSubItems(proc.queues);
                }
            }
        }
    }
}


SideBar::EyeButtonState
SideBar::DrawTopology(const TopologyModel& topology,
                      EyeButtonState parent_eye_button_state, bool show_eye_button)
{
    if(parent_eye_button_state == EyeButtonState::kAllVisible)
    {
        topology.all_subitems_hidden = false;
    }
    EyeButtonState new_button_state = parent_eye_button_state;
    if(show_eye_button)
    {
        EyeButtonState display_state = parent_eye_button_state;
        if(display_state == EyeButtonState::kMixed && topology.all_subitems_hidden)
            display_state = EyeButtonState::kAllHidden;
            
        ImGui::PushID("TopologyEye");
        new_button_state = DrawEyeButton(display_state);
        ImGui::PopID();
        ImGui::SameLine();
    }

    if(ImGui::TreeNodeEx(topology.node_header.c_str(), CATEGORY_HEADER_FLAGS))
    {
        new_button_state = DrawNodes(topology.nodes, new_button_state, true);
        if(new_button_state == EyeButtonState::kAllHidden)
        {
            topology.all_subitems_hidden = true;
        }
        else
        {
            topology.all_subitems_hidden = false;
        }
        ImGui::TreePop();
    }
    else if(new_button_state == EyeButtonState::kAllHidden || new_button_state == EyeButtonState::kAllVisible)
    {
        bool hide = (new_button_state == EyeButtonState::kAllHidden);
        for(const auto& node : topology.nodes)
        {
            for(const auto& proc : node.processors)
            {
                if(hide) { HideAllSubItems(proc.queues); HideAllSubItems(proc.counters); }
                else     { UnhideAllSubItems(proc.queues); UnhideAllSubItems(proc.counters); }
                proc.all_subitems_hidden = hide;
            }
            for(const auto& process : node.processes)
            {
                if(hide)
                {
                    HideAllSubItems(process.streams);
                    HideAllSubItems(process.instrumented_threads);
                    HideAllSubItems(process.sampled_threads);
                    for(const auto& stream : process.streams)
                        for(const auto& sp : stream.processors)
                            HideAllSubItems(sp.queues);
                }
                else
                {
                    UnhideAllSubItems(process.streams);
                    UnhideAllSubItems(process.instrumented_threads);
                    UnhideAllSubItems(process.sampled_threads);
                    for(const auto& stream : process.streams)
                        for(const auto& sp : stream.processors)
                            UnhideAllSubItems(sp.queues);
                }
                process.all_subitems_hidden = hide;
            }
            node.all_subitems_hidden = hide;
        }
        topology.all_subitems_hidden = hide;
    }
    return new_button_state;
}

SideBar::EyeButtonState
SideBar::DrawNodes(const std::vector<NodeModel>& nodes,
                   EyeButtonState parent_eye_button_state, bool show_eye_button)
{
    if(parent_eye_button_state == EyeButtonState::kAllVisible)
    {
        for(const auto& node : nodes)
            node.all_subitems_hidden = false;
    }
    else if(parent_eye_button_state == EyeButtonState::kAllHidden)
    {
        for(const auto& node : nodes)
            node.all_subitems_hidden = true;
    }
    EyeButtonState new_button_state = parent_eye_button_state;
    TreeConnector tc(m_settings);
    for(const NodeModel& node : nodes)
    {
        if(node.info)
        {
            tc.Branch();
            ImGui::PushID(static_cast<int>(node.info->id));
            
            if(show_eye_button)
            {
                EyeButtonState display_state = parent_eye_button_state;
                if(display_state == EyeButtonState::kMixed && node.all_subitems_hidden)
                    display_state = EyeButtonState::kAllHidden;
                    
                ImGui::PushID("NodesEye");
                new_button_state = DrawEyeButton(display_state);
                ImGui::PopID();
                ImGui::SameLine();
            }

            if(ImGui::TreeNodeEx(node.info->host_name.c_str(), CATEGORY_HEADER_FLAGS))
            {
                new_button_state = DrawNode(node, new_button_state, true);
                if(new_button_state == EyeButtonState::kAllHidden)
                {
                    node.all_subitems_hidden = true;
                }
                else
                {
                    node.all_subitems_hidden = false;
                }
                ImGui::TreePop();
            }
            else if(new_button_state == EyeButtonState::kAllHidden || new_button_state == EyeButtonState::kAllVisible)
            {
                bool hide = (new_button_state == EyeButtonState::kAllHidden);
                for(const auto& proc : node.processors)
                {
                    if(hide) { HideAllSubItems(proc.queues); HideAllSubItems(proc.counters); }
                    else     { UnhideAllSubItems(proc.queues); UnhideAllSubItems(proc.counters); }
                    proc.all_subitems_hidden = hide;
                }
                for(const auto& process : node.processes)
                {
                    if(hide)
                    {
                        HideAllSubItems(process.streams);
                        HideAllSubItems(process.instrumented_threads);
                        HideAllSubItems(process.sampled_threads);
                        for(const auto& stream : process.streams)
                            for(const auto& sp : stream.processors)
                                HideAllSubItems(sp.queues);
                    }
                    else
                    {
                        UnhideAllSubItems(process.streams);
                        UnhideAllSubItems(process.instrumented_threads);
                        UnhideAllSubItems(process.sampled_threads);
                        for(const auto& stream : process.streams)
                            for(const auto& sp : stream.processors)
                                UnhideAllSubItems(sp.queues);
                    }
                    process.all_subitems_hidden = hide;
                }
                node.all_subitems_hidden = hide;
            }
            ImGui::PopID();
        }
    }
    return new_button_state;
}

SideBar::EyeButtonState
SideBar::DrawNode(const NodeModel& node, EyeButtonState parent_eye_button_state,
                  bool show_eye_button)
{
    if(parent_eye_button_state == EyeButtonState::kAllVisible)
        node.all_subitems_hidden = false;
    else if(parent_eye_button_state == EyeButtonState::kAllHidden)
        node.all_subitems_hidden = true;

    TreeConnector tc(m_settings);

    EyeButtonState processor_state = parent_eye_button_state;
    EyeButtonState process_state   = parent_eye_button_state;

    tc.Branch();
    
    if(show_eye_button && !node.processors.empty())
    {
        bool all_proc_hidden = true;
        for(const auto& proc : node.processors) {
            if(!proc.all_subitems_hidden) { all_proc_hidden = false; break; }
        }
        
        EyeButtonState display_state = parent_eye_button_state;
        if(display_state == EyeButtonState::kMixed && all_proc_hidden)
            display_state = EyeButtonState::kAllHidden;
            
        ImGui::PushID("ProcessorsEye");
        processor_state = DrawEyeButton(display_state);
        ImGui::PopID();
        ImGui::SameLine();
    }

    bool open = ImGui::TreeNodeEx(node.processor_header.c_str(), CATEGORY_HEADER_FLAGS);

    if(!node.processors.empty() && open)
    {
        processor_state = DrawProcessors(node.processors, processor_state, true);
        ImGui::TreePop();
    }
    else if(!node.processors.empty() &&
            (processor_state == EyeButtonState::kAllHidden ||
             processor_state == EyeButtonState::kAllVisible))
    {
        bool hide = (processor_state == EyeButtonState::kAllHidden);
        for(const auto& proc : node.processors)
        {
            if(hide) { HideAllSubItems(proc.queues); HideAllSubItems(proc.counters); }
            else     { UnhideAllSubItems(proc.queues); UnhideAllSubItems(proc.counters); }
            proc.all_subitems_hidden = hide;
        }
    }

    tc.Branch();
    
    if(show_eye_button && !node.processes.empty())
    {
        bool all_proc_hidden = true;
        for(const auto& proc : node.processes) {
            if(!proc.all_subitems_hidden) { all_proc_hidden = false; break; }
        }
        
        EyeButtonState display_state = parent_eye_button_state;
        if(display_state == EyeButtonState::kMixed && all_proc_hidden)
            display_state = EyeButtonState::kAllHidden;
            
        ImGui::PushID("ProcessesEye");
        process_state = DrawEyeButton(display_state);
        ImGui::PopID();
        ImGui::SameLine();
    }

    open = ImGui::TreeNodeEx(node.process_header.c_str(), CATEGORY_HEADER_FLAGS);

    if(!node.processes.empty() && open)
    {
        process_state = DrawProcesses(node.processes, process_state, true);
        ImGui::TreePop();
    }
    else if(!node.processes.empty() &&
            (process_state == EyeButtonState::kAllHidden ||
             process_state == EyeButtonState::kAllVisible))
    {
        bool hide = (process_state == EyeButtonState::kAllHidden);
        for(const auto& process : node.processes)
        {
            if(hide)
            {
                HideAllSubItems(process.streams);
                HideAllSubItems(process.instrumented_threads);
                HideAllSubItems(process.sampled_threads);
                for(const auto& stream : process.streams)
                    for(const auto& sp : stream.processors)
                        HideAllSubItems(sp.queues);
            }
            else
            {
                UnhideAllSubItems(process.streams);
                UnhideAllSubItems(process.instrumented_threads);
                UnhideAllSubItems(process.sampled_threads);
                for(const auto& stream : process.streams)
                    for(const auto& sp : stream.processors)
                        UnhideAllSubItems(sp.queues);
            }
            process.all_subitems_hidden = hide;
        }
    }

    if((node.processors.empty() || processor_state == EyeButtonState::kAllHidden) &&
       (node.processes.empty() || process_state == EyeButtonState::kAllHidden))
    {
        node.all_subitems_hidden = true;
        return EyeButtonState::kAllHidden;
    }

    node.all_subitems_hidden = false;
    if((node.processors.empty() || processor_state == EyeButtonState::kAllVisible) &&
       (node.processes.empty() || process_state == EyeButtonState::kAllVisible))
    {
        return EyeButtonState::kAllVisible;
    }
    return EyeButtonState::kMixed;
}

SideBar::EyeButtonState
SideBar::DrawProcesses(const std::vector<ProcessModel>& processes,
                       EyeButtonState parent_eye_button_state, bool show_eye_button)
{
    if(parent_eye_button_state == EyeButtonState::kAllVisible)
    {
        for(const auto& process : processes)
            process.all_subitems_hidden = false;
    }
    else if(parent_eye_button_state == EyeButtonState::kAllHidden)
    {
        for(const auto& process : processes)
            process.all_subitems_hidden = true;
    }

    EyeButtonState all_process_state = EyeButtonState::kAllHidden;
    TreeConnector tc(m_settings);
    for(const ProcessModel& process : processes)
    {
        if(process.info)
        {
            tc.Branch();
            EyeButtonState queue_button_state               = parent_eye_button_state;
            EyeButtonState stream_button_state              = parent_eye_button_state;
            EyeButtonState instrumented_thread_button_state = parent_eye_button_state;
            EyeButtonState sampled_thread_button_state      = parent_eye_button_state;
            EyeButtonState counter_button_state             = parent_eye_button_state;
            ImGui::PushID(static_cast<int>(process.info->id));

            EyeButtonState current_eye_button_state = parent_eye_button_state;
            if(show_eye_button)
            {
                EyeButtonState display_state = parent_eye_button_state;
                if(display_state == EyeButtonState::kMixed && process.all_subitems_hidden)
                    display_state = EyeButtonState::kAllHidden;
                    
                ImGui::PushID("ProcessEye");
                current_eye_button_state = DrawEyeButton(display_state);
                ImGui::PopID();
                ImGui::SameLine();
            }

            if(ImGui::TreeNodeEx(process.header.c_str(), CATEGORY_HEADER_FLAGS))
            {
                TreeConnector ps_tc(m_settings);
                if(!process.streams.empty())
                {
                    ps_tc.Branch();
                    stream_button_state = DrawCollapsable(
                        process.streams, process.stream_header, current_eye_button_state);
                }
                if(!process.instrumented_threads.empty())
                {
                    ps_tc.Branch();
                    instrumented_thread_button_state = DrawCollapsable(
                        process.instrumented_threads, process.instrumented_thread_header,
                        current_eye_button_state);
                }
                if(!process.sampled_threads.empty())
                {
                    ps_tc.Branch();
                    sampled_thread_button_state = DrawCollapsable(
                        process.sampled_threads, process.sampled_thread_header,
                        current_eye_button_state);
                }
                ImGui::TreePop();
            }
            else if(current_eye_button_state == EyeButtonState::kAllHidden)
            {
                HideAllSubItems(process.streams);
                HideAllSubItems(process.instrumented_threads);
                HideAllSubItems(process.sampled_threads);
                for(const auto& stream : process.streams)
                    for(const auto& sp : stream.processors)
                        HideAllSubItems(sp.queues);
                stream_button_state              = EyeButtonState::kAllHidden;
                instrumented_thread_button_state = EyeButtonState::kAllHidden;
                sampled_thread_button_state      = EyeButtonState::kAllHidden;
            }
            else if(current_eye_button_state == EyeButtonState::kAllVisible)
            {
                UnhideAllSubItems(process.streams);
                UnhideAllSubItems(process.instrumented_threads);
                UnhideAllSubItems(process.sampled_threads);
                for(const auto& stream : process.streams)
                    for(const auto& sp : stream.processors)
                        UnhideAllSubItems(sp.queues);
                stream_button_state              = EyeButtonState::kAllVisible;
                instrumented_thread_button_state = EyeButtonState::kAllVisible;
                sampled_thread_button_state      = EyeButtonState::kAllVisible;
            }

            if(queue_button_state == EyeButtonState::kAllHidden &&
               stream_button_state == EyeButtonState::kAllHidden &&
               instrumented_thread_button_state == EyeButtonState::kAllHidden &&
               sampled_thread_button_state == EyeButtonState::kAllHidden &&
               counter_button_state == EyeButtonState::kAllHidden)
            {
                process.all_subitems_hidden = true;
                all_process_state           = EyeButtonState::kAllHidden;
            }
            else
            {
                process.all_subitems_hidden = false;
                all_process_state           = EyeButtonState::kMixed;
            }
            ImGui::PopID();
        }
    }
    return all_process_state;
}

SideBar::EyeButtonState
SideBar::DrawProcessors(const std::vector<ProcessorModel>& processors,
    EyeButtonState parent_eye_button_state, bool show_eye_button, uint64_t parent_id)
{
    if(parent_eye_button_state == EyeButtonState::kAllVisible)
    {
        for(const auto& process : processors)
            process.all_subitems_hidden = false;
    }
    else if(parent_eye_button_state == EyeButtonState::kAllHidden)
    {
        for(const auto& process : processors)
            process.all_subitems_hidden = true;
    }

    EyeButtonState all_process_state = EyeButtonState::kAllHidden;
    TreeConnector tc(m_settings);
    for(const ProcessorModel& processor : processors)
    {
        if(processor.info)
        {
            tc.Branch();
            ImGui::PushID(static_cast<int>(processor.info->id.value));

            EyeButtonState current_eye_button_state = parent_eye_button_state;
            if(show_eye_button)
            {
                EyeButtonState display_state = parent_eye_button_state;
                if(display_state == EyeButtonState::kMixed && processor.all_subitems_hidden)
                    display_state = EyeButtonState::kAllHidden;
                    
                ImGui::PushID("ProcessorEye");
                current_eye_button_state = DrawEyeButton(display_state);
                ImGui::PopID();
                ImGui::SameLine();
            }

            EyeButtonState queue_button_state               = current_eye_button_state;
            EyeButtonState counter_button_state             = current_eye_button_state;
            EyeButtonState stream_button_state              = EyeButtonState::kAllHidden;
            EyeButtonState instrumented_thread_button_state = EyeButtonState::kAllHidden;
            EyeButtonState sampled_thread_button_state      = EyeButtonState::kAllHidden;

            if(ImGui::TreeNodeEx(processor.header.c_str(), CATEGORY_HEADER_FLAGS))
            {
                TreeConnector proc_tc(m_settings);
                if(!processor.queues.empty())
                {
                    proc_tc.Branch();
                    queue_button_state = DrawCollapsable(
                        processor.queues, processor.queue_header, current_eye_button_state);
                }
                if(!processor.counters.empty())
                {
                    proc_tc.Branch();
                    counter_button_state =
                        DrawCollapsable(processor.counters, processor.counter_header,
                            current_eye_button_state);
                }
                ImGui::TreePop();
            }
            else if(current_eye_button_state == EyeButtonState::kAllHidden)
            {
                HideAllSubItems(processor.queues);
                HideAllSubItems(processor.counters);
                queue_button_state   = EyeButtonState::kAllHidden;
                counter_button_state = EyeButtonState::kAllHidden;
            }
            else if(current_eye_button_state == EyeButtonState::kAllVisible)
            {
                UnhideAllSubItems(processor.queues);
                UnhideAllSubItems(processor.counters);
                queue_button_state   = EyeButtonState::kAllVisible;
                counter_button_state = EyeButtonState::kAllVisible;
            }

            if(queue_button_state == EyeButtonState::kAllHidden &&
                stream_button_state == EyeButtonState::kAllHidden &&
                instrumented_thread_button_state == EyeButtonState::kAllHidden &&
                sampled_thread_button_state == EyeButtonState::kAllHidden &&
                counter_button_state == EyeButtonState::kAllHidden)
            {
                processor.all_subitems_hidden = true;
            }
            else
            {
                processor.all_subitems_hidden = false;
                all_process_state           = EyeButtonState::kMixed;
            }
            ImGui::PopID();
        }
    }
    return all_process_state;
}

template<typename Model>
SideBar::EyeButtonState
SideBar::DrawCollapsable(const std::vector<Model>& container,
                         const std::string&                collapsable_header,
                         EyeButtonState                    parent_eye_button_state)
{
    bool open = true;
    EyeButtonState new_button_state = parent_eye_button_state;
    if (!collapsable_header.empty())
    {
        ImGui::PushID(collapsable_header.c_str());
        
        EyeButtonState display_state = parent_eye_button_state;
        if (display_state == EyeButtonState::kMixed && IsAllSubItemsHidden(container))
            display_state = EyeButtonState::kAllHidden;
            
        ImGui::PushID("CollapsableEye");
        new_button_state = DrawEyeButton(display_state);
        ImGui::PopID();

        if (new_button_state == EyeButtonState::kAllHidden)
        {
            HideAllSubItems(container);
        }
        else if (new_button_state == EyeButtonState::kAllVisible)
        {
            UnhideAllSubItems(container);
        }

        ImGui::SameLine();
        open = ImGui::TreeNodeEx(collapsable_header.c_str(),
            CATEGORY_HEADER_FLAGS | ImGuiTreeNodeFlags_Framed);
    }
    EyeButtonState group_state = new_button_state;

    if(open)
    {
        TreeConnector tc(m_settings);
        for(const Model& item : container)
        {
            if(item.info)
            {
                tc.Branch();
                bool track_changed = RenderTrackItem(item.graph_index);
                if(track_changed)
                {
                    new_button_state = EyeButtonState::kMixed;
                }

                if constexpr (std::is_same_v<Model, StreamModel>)
                {
                    ImGui::Indent();
                    EyeButtonState pass_state = group_state;
                    if (track_changed)
                    {
                        if (m_graphs && item.graph_index < m_graphs->size())
                        {
                            TrackGraph& graph = (*m_graphs)[item.graph_index];
                            pass_state = graph.display ? EyeButtonState::kAllVisible : EyeButtonState::kAllHidden;
                        }
                    }
                    DrawProcessors(item.processors, pass_state, false, item.info->id);
                    ImGui::Unindent();

                }

            }
        }
        if (collapsable_header.empty())
        {
            return new_button_state;
        }
        ImGui::TreePop();
    }
    ImGui::PopID();
    return new_button_state;
}

SideBar::EyeButtonState
SideBar::DrawEyeButton(EyeButtonState eye_button_state)
{
    ImGui::PushStyleColor(ImGuiCol_Button, m_settings.GetColor(Colors::kTransparent));
    ImGui::PushFont(m_settings.GetFontManager().GetIconFont(FontType::kDefault));

    ImVec2 eye_size = ImGui::CalcTextSize(ICON_EYE);
    float  button_w = eye_size.x + ImGui::GetStyle().FramePadding.x * 2;
    float  button_h = eye_size.y + ImGui::GetStyle().FramePadding.y * 2;

    EyeButtonState new_button_state = eye_button_state;
    if(ImGui::Button(eye_button_state == EyeButtonState::kAllHidden ? ICON_EYE_SLASH
                                                                    : ICON_EYE,
                     ImVec2(button_w, button_h)))
    {
        if(eye_button_state == EyeButtonState::kAllHidden)
        {
            new_button_state = EyeButtonState::kAllVisible;
        }
        else if(eye_button_state == EyeButtonState::kAllVisible ||
                eye_button_state == EyeButtonState::kMixed)
        {
            new_button_state = EyeButtonState::kAllHidden;
        }
    }
    ImGui::PopFont();
    if(ImGui::IsItemHovered())
        SetTooltipStyled("Toggle All Track Visibility");
    ImGui::PopStyleColor();

    return new_button_state;
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
