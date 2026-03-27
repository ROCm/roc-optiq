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
constexpr float  TREE_LINE_W           = 2.0f;

static bool
IsRowHovered()
{
    ImVec2 pos   = ImGui::GetCursorScreenPos();
    ImVec2 mouse = ImGui::GetMousePos();
    return ImGui::IsWindowHovered(ImGuiHoveredFlags_None) &&
           mouse.y >= pos.y && mouse.y < pos.y + ImGui::GetFrameHeight();
}

struct TreeConnector
{
    ImDrawList* dl    = nullptr;
    ImU32       col   = 0;
    float       lx    = 0, blen = 0, cr = 0, prev_y = 0;
    int         n     = 0;

    TreeConnector(SettingsManager& s)
    {
        float indent = ImGui::GetStyle().IndentSpacing;
        dl   = ImGui::GetWindowDrawList();
        col  = s.GetColor(Colors::kMetaDataSeparator);
        lx   = ImGui::GetCursorScreenPos().x - indent * 0.5f;
        blen = indent * 0.45f;
        cr   = std::min(8.0f, indent * 0.4f);
    }

    void Branch()
    {
        float my = ImGui::GetCursorScreenPos().y + ImGui::GetFrameHeight() * 0.5f;
        if(n > 0)
            dl->AddLine(ImVec2(lx, prev_y), ImVec2(lx, my - cr), col, TREE_LINE_W);
        dl->AddBezierQuadratic(ImVec2(lx, my - cr), ImVec2(lx, my),
                               ImVec2(lx + cr, my), col, TREE_LINE_W, 16);
        dl->AddLine(ImVec2(lx + cr, my), ImVec2(lx + blen, my), col, TREE_LINE_W);
        prev_y = my;
        n++;
    }
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
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6, 5));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 6));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, 20.0f);
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered,
                              ImGui::ColorConvertU32ToFloat4(
                                  m_settings.GetColor(Colors::kBgFrame)));

        const TopologyModel& topology = m_track_topology->GetTopology();
        if(ImGui::TreeNodeEx("Project",
                             CATEGORY_HEADER_FLAGS | ImGuiTreeNodeFlags_Framed))
        {
            if(!topology.nodes.empty())
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
    bool row_hovered = IsRowHovered();
    ImGui::PushStyleColor(ImGuiCol_Button, m_settings.GetColor(Colors::kTransparent));
    if(!row_hovered)
    {
        ImVec4 dim = ImGui::ColorConvertU32ToFloat4(m_settings.GetColor(Colors::kTextDim));
        dim.w *= kIdleDimAlpha;
        ImGui::PushStyleColor(ImGuiCol_Text, dim);
    }
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
    if(!row_hovered) ImGui::PopStyleColor();
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

bool
SideBar::IsAllSubItemsHidden(const std::vector<IterableModel>& container)
{
    bool all_hidden = true;
    if(m_graphs && !m_graphs->empty())
    {
        for(const IterableModel& elem : container)
        {
            TrackGraph& graph = (*m_graphs)[elem.graph_index];
            if(graph.display)
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
    if(m_graphs && !m_graphs->empty())
    {
        std::vector<uint64_t> ids_to_remove;
        for(const IterableModel& elem : container)
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

void
SideBar::UnhideAllSubItems(const std::vector<IterableModel>& container)
{
    if(m_graphs && !m_graphs->empty())
    {
        std::vector<uint64_t> ids_to_add;
        for(const IterableModel& elem : container)
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
        bool h = IsRowHovered();
        if(topology.all_subitems_hidden)
        {
            new_button_state = DrawEyeButton(EyeButtonState::kAllHidden, !h);
        }
        else
        {
            new_button_state = DrawEyeButton(parent_eye_button_state, !h);
        }
        ImGui::SameLine();
    }

    if(ImGui::TreeNodeEx(topology.node_header.c_str(), CATEGORY_HEADER_FLAGS))
    {
        new_button_state = DrawNodes(topology.nodes, new_button_state, false);
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
    return new_button_state;
}

SideBar::EyeButtonState
SideBar::DrawNodes(const std::vector<NodeModel>& nodes,
                   EyeButtonState parent_eye_button_state, bool show_eye_button)
{
    if(parent_eye_button_state == EyeButtonState::kAllVisible)
    {
        for(const auto& node : nodes)
        {
            node.all_subitems_hidden = false;
        }
    }
    EyeButtonState new_button_state = parent_eye_button_state;
    TreeConnector tc(m_settings);
    for(const NodeModel& node : nodes)
    {
        if(node.info)
        {
            tc.Branch();
            if(show_eye_button)
            {
                bool h = IsRowHovered();
                if(node.all_subitems_hidden)
                {
                    new_button_state = DrawEyeButton(EyeButtonState::kAllHidden, !h);
                }
                else
                {
                    new_button_state = DrawEyeButton(parent_eye_button_state, !h);
                }
                ImGui::SameLine();
            }

            ImGui::PushID(static_cast<int>(node.info->id));
            if(ImGui::TreeNodeEx(node.info->host_name.c_str(), CATEGORY_HEADER_FLAGS))
            {
                new_button_state = DrawNode(node, new_button_state, false);
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
    {
        node.all_subitems_hidden = false;
    }
    EyeButtonState new_button_state = parent_eye_button_state;

    if(show_eye_button)
    {
        bool h = IsRowHovered();
        if(node.all_subitems_hidden)
        {
            new_button_state = DrawEyeButton(EyeButtonState::kAllHidden, !h);
        }
        else
        {
            new_button_state = DrawEyeButton(parent_eye_button_state, !h);
        }
        ImGui::SameLine();
    }

    TreeConnector tc(m_settings);
    tc.Branch();
    bool open = ImGui::TreeNodeEx(node.processor_header.c_str(), CATEGORY_HEADER_FLAGS);

    if(!node.processors.empty() && open)
    {
        new_button_state = DrawProcessors(node.processors, new_button_state, true);
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

    tc.Branch();
    open = ImGui::TreeNodeEx(node.process_header.c_str(), CATEGORY_HEADER_FLAGS);

    if(!node.processes.empty() && open)
    {
        new_button_state = DrawProcesses(node.processes, new_button_state, false);
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
    return new_button_state;
}

SideBar::EyeButtonState
SideBar::DrawProcesses(const std::vector<ProcessModel>& processes,
                       EyeButtonState parent_eye_button_state, bool show_eye_button)
{
    if(parent_eye_button_state == EyeButtonState::kAllVisible)
    {
        for(const auto& process : processes)
        {
            process.all_subitems_hidden = false;
        }
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
                bool h = IsRowHovered();
                if(process.all_subitems_hidden)
                {
                    current_eye_button_state = DrawEyeButton(EyeButtonState::kAllHidden, !h);
                }
                else
                {
                    current_eye_button_state = DrawEyeButton(parent_eye_button_state, !h);
                }
                ImGui::SameLine();
            }

            if(ImGui::TreeNodeEx(process.header.c_str(), CATEGORY_HEADER_FLAGS))
            {
                if(!process.streams.empty())
                {
                    stream_button_state = DrawCollapsable(
                        process.streams, process.stream_header, current_eye_button_state);
                }
                if(!process.instrumented_threads.empty())
                {
                    instrumented_thread_button_state = DrawCollapsable(
                        process.instrumented_threads, process.instrumented_thread_header,
                        current_eye_button_state);
                }
                if(!process.sampled_threads.empty())
                {
                    sampled_thread_button_state = DrawCollapsable(
                        process.sampled_threads, process.sampled_thread_header,
                        current_eye_button_state);
                }
                ImGui::TreePop();
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
    EyeButtonState parent_eye_button_state, bool show_eye_button)
{
    if(parent_eye_button_state == EyeButtonState::kAllVisible)
    {
        for(const auto& process : processors)
        {
            process.all_subitems_hidden = false;
        }
    }

    EyeButtonState all_process_state = EyeButtonState::kAllHidden;
    TreeConnector tc(m_settings);
    for(const ProcessorModel& processor : processors)
    {
        if(processor.info)
        {
            tc.Branch();
            ImGui::PushID(static_cast<int>(processor.info->id.fields.id));

            EyeButtonState current_eye_button_state = parent_eye_button_state;
            if(show_eye_button)
            {
                bool h = IsRowHovered();
                if(processor.all_subitems_hidden)
                {
                    current_eye_button_state = DrawEyeButton(EyeButtonState::kAllHidden, !h);
                }
                else
                {
                    current_eye_button_state = DrawEyeButton(EyeButtonState::kMixed, !h);
                }
                ImGui::SameLine();
            }

            EyeButtonState queue_button_state               = current_eye_button_state;
            EyeButtonState counter_button_state             = current_eye_button_state;
            EyeButtonState stream_button_state              = EyeButtonState::kAllHidden;
            EyeButtonState instrumented_thread_button_state = EyeButtonState::kAllHidden;
            EyeButtonState sampled_thread_button_state      = EyeButtonState::kAllHidden;

            if(ImGui::TreeNodeEx(processor.header.c_str(), CATEGORY_HEADER_FLAGS))
            {
                if(!processor.queues.empty())
                {
                    queue_button_state = DrawCollapsable(
                        processor.queues, processor.queue_header, current_eye_button_state);
                }
                if(!processor.counters.empty())
                {
                    counter_button_state =
                        DrawCollapsable(processor.counters, processor.counter_header,
                            current_eye_button_state);
                }
                ImGui::TreePop();
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

SideBar::EyeButtonState
SideBar::DrawCollapsable(const std::vector<IterableModel>& container,
                         const std::string&                collapsable_header,
                         EyeButtonState                    parent_eye_button_state)
{
    ImGui::PushID(collapsable_header.c_str());
    if(parent_eye_button_state == EyeButtonState::kAllVisible)
    {
        UnhideAllSubItems(container);
    }
    EyeButtonState new_button_state = parent_eye_button_state;
    bool           h               = IsRowHovered();
    if(IsAllSubItemsHidden(container))
    {
        new_button_state = DrawEyeButton(EyeButtonState::kAllHidden, !h);
    }
    else
    {
        new_button_state = DrawEyeButton(parent_eye_button_state, !h);
    }

    if(new_button_state == EyeButtonState::kAllHidden)
    {
        HideAllSubItems(container);
    }
    else if(new_button_state == EyeButtonState::kAllVisible)
    {
        UnhideAllSubItems(container);
    }

    ImGui::SameLine();
    bool open = ImGui::TreeNodeEx(collapsable_header.c_str(), CATEGORY_HEADER_FLAGS);
    if(open)
    {
        TreeConnector tc(m_settings);
        for(const IterableModel& item : container)
        {
            if(item.info)
            {
                tc.Branch();
                if(RenderTrackItem(item.graph_index))
                {
                    new_button_state = EyeButtonState::kMixed;
                }
            }
        }
        ImGui::TreePop();
    }
    ImGui::PopID();
    return new_button_state;
}

SideBar::EyeButtonState
SideBar::DrawEyeButton(EyeButtonState eye_button_state, bool dim)
{
    ImGui::PushStyleColor(ImGuiCol_Button, m_settings.GetColor(Colors::kTransparent));
    if(dim)
    {
        ImVec4 d = ImGui::ColorConvertU32ToFloat4(m_settings.GetColor(Colors::kTextDim));
        d.w *= kIdleDimAlpha;
        ImGui::PushStyleColor(ImGuiCol_Text, d);
    }
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
    if(dim) ImGui::PopStyleColor();
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
