// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_sidebar.h"
#include "icons/rocprovfis_icon_defines.h"
#include "widgets/rocprofvis_gui_helpers.h"
#include "rocprofvis_data_provider.h"
#include "rocprofvis_track_item.h"
#include "rocprofvis_settings_manager.h"
#include "rocprofvis_timeline_selection.h"

#include <unordered_set>

namespace RocProfVis
{
namespace View
{

constexpr ImGuiTreeNodeFlags HEADER_FLAGS = ImGuiTreeNodeFlags_Framed |
                                            ImGuiTreeNodeFlags_DefaultOpen |
                                            ImGuiTreeNodeFlags_SpanLabelWidth;
constexpr float TREE_LINE_W = 1.5f;

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

SideBar::SideBar(std::shared_ptr<TrackTopology>         topology,
                 std::shared_ptr<TimelineSelection>     timeline_selection,
                 std::shared_ptr<std::vector<TrackGraph>> graphs,
                 DataProvider&                          dp)
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
        const SidebarTree& sidebar_tree = m_track_topology->GetSidebarTree();
        if(m_eye_state_dirty && sidebar_tree.root)
        {
            InvalidateEyeStateCache(*sidebar_tree.root);
            m_eye_state_dirty = false;
        }

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(3, 2));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, 14.0f);
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered,
                              ImGui::ColorConvertU32ToFloat4(
                                  m_settings.GetColor(Colors::kBgFrame)));
        if(ImGui::TreeNodeEx("Project", HEADER_FLAGS))
        {
            TreeConnector project_tc(m_settings);
            if(sidebar_tree.root)
            {
                for(const auto& child : sidebar_tree.root->children)
                {
                    if(!child)
                    {
                        continue;
                    }

                    if(child->type == NodeType::kUncategorizedList &&
                       !child->collapsable)
                    {
                        RenderTreeChildren(*child);
                        continue;
                    }

                    project_tc.Branch();
                    if(child->type == NodeType::kNodeList)
                    {
                        RenderBranchNode(*child, sidebar_tree.root.get(),
                                         sidebar_tree.root.get());
                    }
                    else
                    {
                        RenderTreeNode(*child);
                    }
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

void
SideBar::RenderTrackItem(const uint64_t& index, bool show_eye_button)
{
    if(!m_graphs || index >= m_graphs->size())
    {
        return;
    }

    TrackGraph& graph = (*m_graphs)[index];

    ImGui::PushID(static_cast<int>(graph.chart->GetID()));
    ImGui::PushStyleColor(ImGuiCol_Button, m_settings.GetColor(Colors::kTransparent));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2, 0));

    bool display = graph.display;
    if(show_eye_button)
    {
        ImGui::PushFont(m_settings.GetFontManager().GetFont(FontType::kIcon), 0.0f);
        if(ImGui::Button(display ? ICON_EYE : ICON_EYE_SLASH))
        {
            graph.display         = !graph.display;
            graph.display_changed = true;
            m_eye_state_dirty     = true;
            m_data_provider.DataModel().GetTimeline().UpdateHistogram(
                { graph.chart->GetID() }, graph.display);
        }
        ImGui::PopFont();
        if(ImGui::IsItemHovered())
            SetTooltipStyled("Toggle Track Visibility");

        ImGui::SameLine();
        ImGui::PushFont(m_settings.GetFontManager().GetFont(FontType::kIcon), 0.0f);
        if(ImGui::Button(ICON_ARROWS_SHRINK))
        {
            EventManager::GetInstance()->AddEvent(std::make_shared<ScrollToTrackEvent>(
                static_cast<int>(RocEvents::kHandleUserGraphNavigationEvent),
                graph.chart->GetID(), m_data_provider.GetTraceFilePath()));
        }
        ImGui::PopFont();
        if(ImGui::IsItemHovered())
            SetTooltipStyled("Scroll To Track");
    }

    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
    if(show_eye_button)
    {
        ImGui::SameLine();
    }

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

    ImGui::PopID();
}

SideBar::EyeButtonState
SideBar::MergeEyeButtonState(EyeButtonState lhs, EyeButtonState rhs) const
{
    if(lhs == rhs)
    {
        return lhs;
    }
    return EyeButtonState::kMixed;
}

SideBar::EyeButtonState
SideBar::GetLeafState(const LeafNode& leaf) const
{
    if(!m_graphs || leaf.graph_index >= m_graphs->size())
    {
        return EyeButtonState::kAllHidden;
    }

    return (*m_graphs)[leaf.graph_index].display
               ? EyeButtonState::kAllVisible
               : EyeButtonState::kAllHidden;
}

SideBar::EyeButtonState
SideBar::GetTreeState(const TreeNode& node) const
{
    return GetSubtreeEyeState(node, true);
}

SideBar::EyeButtonState
SideBar::GetSubtreeEyeState(const TreeNode& node, bool cross_boundaries) const
{
    if(node.IsLeaf())
    {
        return GetLeafState(static_cast<const LeafNode&>(node));
    }

    if(node.cached_eye_state != 0)
    {
        return static_cast<EyeButtonState>(node.cached_eye_state - 1);
    }

    bool           has_state = false;
    EyeButtonState state     = EyeButtonState::kAllHidden;

    for(const auto& child : node.children)
    {
        if(!child ||
           (!cross_boundaries && child->breaks_visibility_chain))
        {
            continue;
        }

        EyeButtonState child_state = GetSubtreeEyeState(*child, false);

        if(!child->IsLeaf() && child->children.empty())
        {
            continue;
        }

        state     = has_state ? MergeEyeButtonState(state, child_state) : child_state;
        has_state = true;
        if(state == EyeButtonState::kMixed)
        {
            break;
        }
    }

    EyeButtonState result = has_state ? state : EyeButtonState::kAllHidden;
    node.cached_eye_state = static_cast<uint8_t>(result) + 1;
    return result;
}

void
SideBar::ApplyVisibility(const TreeNode& node, bool visible)
{
    if(!m_graphs || m_graphs->empty())
    {
        return;
    }

    std::unordered_set<uint64_t> visited_graphs;
    std::vector<uint64_t>        chart_ids;
    std::vector<const TreeNode*> stack = { &node };

    while(!stack.empty())
    {
        const TreeNode* current = stack.back();
        stack.pop_back();
        if(!current)
        {
            continue;
        }

        current->cached_eye_state = 0;

        if(current != &node && current->breaks_visibility_chain)
        {
            continue;
        }

        if(current->IsLeaf())
        {
            const LeafNode& leaf = static_cast<const LeafNode&>(*current);
            if(leaf.graph_index < m_graphs->size() &&
               visited_graphs.insert(leaf.graph_index).second)
            {
                TrackGraph& graph = (*m_graphs)[leaf.graph_index];
                if(graph.display != visible)
                {
                    graph.display         = visible;
                    graph.display_changed = true;
                    chart_ids.push_back(graph.chart->GetID());
                }
            }
        }

        for(const auto& child : current->children)
        {
            if(child)
            {
                stack.push_back(child.get());
            }
        }
    }

    if(!chart_ids.empty())
    {
        m_data_provider.DataModel().GetTimeline().UpdateHistogram(chart_ids, visible);
    }
}

void
SideBar::RenderLeafNode(const LeafNode& leaf)
{
    ImGui::PushID(static_cast<const void*>(&leaf));
    RenderTrackItem(leaf.graph_index, leaf.show_eye_button);

    if(leaf.render_children_inline && !leaf.children.empty())
    {
        ImGui::Indent();
        RenderTreeChildren(leaf);
        ImGui::Unindent();
    }

    ImGui::PopID();
}

void
SideBar::RenderBranchNode(const TreeNode& node, const TreeNode* state_node,
                          const TreeNode* target_node)
{
    const TreeNode& state_source = state_node ? *state_node : node;
    const TreeNode& apply_target = target_node ? *target_node : node;

    ImGui::PushID(static_cast<const void*>(&node));
    if(node.show_eye_button)
    {
        EyeButtonState current_state = GetTreeState(state_source);
        EyeButtonState new_state     = DrawEyeButton(current_state);
        if(new_state != current_state && new_state != EyeButtonState::kMixed)
        {
            ApplyVisibility(apply_target, new_state == EyeButtonState::kAllVisible);
        }
        ImGui::SameLine();
    }

    bool open = true;
    if(node.collapsable)
    {
        open = ImGui::TreeNodeEx(node.label.c_str(), HEADER_FLAGS);
    }

    if(open)
    {
        RenderTreeChildren(node);
        if(node.collapsable)
        {
            ImGui::TreePop();
        }
    }
    ImGui::PopID();
}

void
SideBar::RenderTreeNode(const TreeNode& node)
{
    if(node.IsLeaf())
    {
        RenderLeafNode(static_cast<const LeafNode&>(node));
        return;
    }

    RenderBranchNode(node);
}

void
SideBar::RenderTreeChildren(const TreeNode& node)
{
    if(node.children.empty())
    {
        return;
    }

    TreeConnector tc(m_settings);
    for(const auto& child : node.children)
    {
        if(!child)
        {
            continue;
        }

        tc.Branch();
        RenderTreeNode(*child);
    }
}

void
SideBar::InvalidateEyeStateCache(const TreeNode& node)
{
    node.cached_eye_state = 0;
    for(const auto& child : node.children)
    {
        if(child)
        {
            InvalidateEyeStateCache(*child);
        }
    }
}

SideBar::EyeButtonState
SideBar::DrawEyeButton(EyeButtonState eye_button_state)
{
    ImGui::PushStyleColor(ImGuiCol_Button, m_settings.GetColor(Colors::kTransparent));
    ImGui::PushFont(m_settings.GetFontManager().GetFont(FontType::kIcon), 0.0f);

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

}  // namespace View
}  // namespace RocProfVis
