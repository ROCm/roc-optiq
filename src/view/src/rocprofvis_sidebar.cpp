// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_sidebar.h"
#include "icons/rocprovfis_icon_defines.h"
#include "model/rocprofvis_topology_model.h"
#include "widgets/rocprofvis_gui_helpers.h"
#include "rocprofvis_data_provider.h"
#include "rocprofvis_events.h"
#include "rocprofvis_render_scheduler.h"
#include "rocprofvis_track_item.h"
#include "rocprofvis_settings_manager.h"
#include "rocprofvis_timeline_selection.h"
#include "spdlog/spdlog.h"

#include <cmath>
#include <unordered_set>

namespace RocProfVis
{
namespace View
{

constexpr ImGuiTreeNodeFlags HEADER_FLAGS = ImGuiTreeNodeFlags_Framed |
                                            ImGuiTreeNodeFlags_DefaultOpen |
                                            ImGuiTreeNodeFlags_SpanLabelWidth;
constexpr float TREE_LINE_W = 1.5f;
constexpr float MENU_PAD_X  = 8.0f;
constexpr float MENU_PAD_Y  = 6.0f;
// ImGui offsets a framed tree node's label by FontSize + FramePadding.x * this
// factor (see TreeNodeBehavior); used to place the inline device lead arrow.
constexpr float FRAMED_LABEL_PAD_MULT = 3.0f;
// Lead-arrow slot width used if the font reports a zero-width space.
constexpr int DEFAULT_LEAD_ARROW_PAD = 2;

// Matches TimelineSelection::HIGHLIGHT_TIMEOUT_S so reveal and "go to event"
// pulse for the same duration.
constexpr double REVEAL_PULSE_DURATION_S = 10.0;
// Force-open ancestors for a few frames; the scroll extent is only known once
// the newly expanded rows have been laid out.
constexpr int   REVEAL_SCROLL_FRAMES       = 3;
constexpr float REVEAL_HIGHLIGHT_THICKNESS = 1.5f;
constexpr float REVEAL_HIGHLIGHT_ROUNDING  = 2.0f;

// Recolors a framed tree node's collapse arrow, matching ImGui::RenderArrow's
// geometry so it overlaps the default arrow exactly.
static void
DrawTreeArrow(ImDrawList* draw_list, float cx, float cy, float font_size, bool open,
              ImU32 col)
{
    const float r = font_size * 0.40f;
    if(open)  // pointing down
    {
        draw_list->AddTriangleFilled(ImVec2(cx, cy + 0.75f * r),
                                     ImVec2(cx - 0.866f * r, cy - 0.75f * r),
                                     ImVec2(cx + 0.866f * r, cy - 0.75f * r), col);
    }
    else  // pointing right
    {
        draw_list->AddTriangleFilled(ImVec2(cx + 0.75f * r, cy),
                                     ImVec2(cx - 0.75f * r, cy + 0.866f * r),
                                     ImVec2(cx - 0.75f * r, cy - 0.866f * r), col);
    }
}

namespace
{

TreeNode*
AddBranchNode(TreeNode* parent, NodeType type, const std::string& label,
              bool collapsable = true, bool show_eye_button = true)
{
    auto node             = std::make_unique<TreeNode>(type, label, collapsable);
    node->show_eye_button = show_eye_button;
    return parent->AddChild(std::move(node));
}

/*
 * Adds a row for one track. The topology name wins when it has one; tracks
 * that only exist on the timeline (the uncategorized list) fall back to the
 * name the timeline gave them.
 */
LeafNode*
AddLeafNode(TreeNode* parent, uint64_t track_id, const std::string& topology_label,
            const TimelineModel& timeline, bool render_children_inline = false)
{
    std::string label = topology_label;
    if(label.empty())
    {
        const TrackInfo* track = timeline.GetTrack(track_id);
        if(track)
        {
            label = track->main_name;
        }
    }

    auto leaf                    = std::make_unique<LeafNode>(label, track_id);
    leaf->render_children_inline = render_children_inline;
    LeafNode* raw                = leaf.get();
    parent->AddChild(std::move(leaf));
    return raw;
}

void
BuildLeafList(TreeNode* parent, NodeType type, const std::string& label,
              const std::vector<TopologyNode*>& items, const TimelineModel& timeline,
              bool show_list_header)
{
    if(items.empty())
    {
        return;
    }

    TreeNode* target = parent;
    if(show_list_header)
    {
        target = AddBranchNode(parent, type, label);
    }
    for(const TopologyNode* item : items)
    {
        AddLeafNode(target, item->GetTrackId(), item->GetName(), timeline);
    }
}

/*
 * Builds a processor subtree. Queues are passed in rather than read from the
 * processor because a processor shown under a stream lists only the queues that
 * stream dispatched to.
 *
 * In that inline position show_controls is false: the row becomes a plain
 * "GPU0" lead-in with no eye button and no intermediate list headers, and
 * breaks_visibility_chain isolates it from ancestor bulk-visibility toggles.
 */
void
BuildProcessorBranch(TreeNode* parent, const ProcessorInfo& processor,
                     const std::vector<TopologyNode*>& queues,
                     const std::vector<TopologyNode*>& counters,
                     const TimelineModel& timeline, bool show_controls,
                     bool breaks_chain = false)
{
    const std::string label =
        show_controls ? processor.GetHeader()
                      : TopologyTree::GetProcessorTypeName(processor.type) +
                            std::to_string(processor.type_index);

    TreeNode* node =
        AddBranchNode(parent, NodeType::kProcessor, label, true, show_controls);
    node->breaks_visibility_chain = breaks_chain;
    node->show_lead_arrow         = !show_controls;

    BuildLeafList(node, NodeType::kQueueList,
                  "Queues (" + std::to_string(queues.size()) + ")", queues, timeline,
                  show_controls);
    BuildLeafList(node, NodeType::kCounterList,
                  "Counters (" + std::to_string(counters.size()) + ")", counters,
                  timeline, show_controls);
}

void
BuildProcessBranch(TreeNode* parent, const ProcessInfo& process,
                   const TimelineModel& timeline)
{
    TreeNode* process_node =
        AddBranchNode(parent, NodeType::kProcess, process.GetHeader());

    const std::vector<TopologyNode*>& streams =
        process.GetChildren(TopologyNodeType::kStream);
    if(!streams.empty())
    {
        TreeNode* stream_list =
            AddBranchNode(process_node, NodeType::kStreamList,
                          "Streams (" + std::to_string(streams.size()) + ")");
        for(const TopologyNode* stream : streams)
        {
            const std::vector<TopologyNode*>& stream_processors =
                stream->GetLinkedChildren(TopologyNodeType::kProcessor);
            const std::vector<TopologyNode*>& stream_queues =
                stream->GetLinkedChildren(TopologyNodeType::kQueue);

            LeafNode* stream_leaf =
                AddLeafNode(stream_list, stream->GetTrackId(), stream->GetName(),
                            timeline, !stream_processors.empty());

            for(const TopologyNode* node : stream_processors)
            {
                const ProcessorInfo& processor =
                    static_cast<const ProcessorInfo&>(*node);
                std::vector<TopologyNode*> queues;
                for(TopologyNode* queue : stream_queues)
                {
                    if(queue->GetParent() == &processor)
                    {
                        queues.push_back(queue);
                    }
                }
                BuildProcessorBranch(stream_leaf, processor, queues, {}, timeline,
                                     false, true);
            }
        }
    }

    std::vector<TopologyNode*> instrumented;
    std::vector<TopologyNode*> sampled;
    for(TopologyNode* node : process.GetChildren(TopologyNodeType::kThread))
    {
        const ThreadInfo& thread = static_cast<const ThreadInfo&>(*node);
        if(thread.kind == ThreadInfo::Kind::kInstrumented)
        {
            instrumented.push_back(node);
        }
        else
        {
            sampled.push_back(node);
        }
    }

    BuildLeafList(process_node, NodeType::kInstrumentedThreadList,
                  "Threads (" + std::to_string(instrumented.size()) + ")", instrumented,
                  timeline, true);
    BuildLeafList(process_node, NodeType::kSampledThreadList,
                  "Sampled Threads (" + std::to_string(sampled.size()) + ")", sampled,
                  timeline, true);
}

/*
 * Collects the track ids that already have a row. A set, because a track can
 * legitimately appear more than once - a queue sits under its processor and
 * again under every stream that dispatched to it. The walk recurses through
 * leaves as well: a stream row carries its inline processor subtree.
 */
void
CollectLeafTrackIds(const TreeNode* node, std::unordered_set<uint64_t>& out)
{
    if(node == nullptr)
    {
        return;
    }
    if(node->IsLeaf())
    {
        out.insert(static_cast<const LeafNode*>(node)->track_id);
    }
    for(const std::unique_ptr<TreeNode>& child : node->children)
    {
        CollectLeafTrackIds(child.get(), out);
    }
}

}  // namespace

class TreeConnector
{
public:
    explicit TreeConnector(SettingsManager& s, ImU32 color = 0)
    {
        float indent = ImGui::GetStyle().IndentSpacing;
        m_draw_list  = ImGui::GetWindowDrawList();
        m_color      = color ? color : s.GetColor(Colors::kMetaDataSeparator);
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

SideBar::SideBar(std::shared_ptr<TimelineSelection>       timeline_selection,
                 std::shared_ptr<std::vector<TrackItem*>> tracks,
                 DataProvider&                           dp)
: m_settings(SettingsManager::GetInstance())
, m_timeline_selection(timeline_selection)
, m_tracks(tracks)
, m_data_provider(dp)
, m_active_node_color(0)
, m_track_visibility_token(EventManager::InvalidSubscriptionToken)
, m_metadata_changed_token(EventManager::InvalidSubscriptionToken)
{
    m_track_visibility_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kTrackVisibilityChanged),
        [this](std::shared_ptr<RocEvent> e) {
            if(e && e->GetSourceId() == m_data_provider.GetTraceFilePath())
            {
                if(m_sidebar_tree.root)
                {
                    InvalidateEyeStateCache(*m_sidebar_tree.root);
                }
            }
        });
    m_metadata_changed_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kTrackMetadataChanged),
        [this](std::shared_ptr<RocEvent> e) {
            if(e && e->GetSourceId() == m_data_provider.GetTraceFilePath())
            {
                m_rebuild_pending = true;
            }
        });
    m_reveal_track_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kRevealTrackInTopology),
        [this](std::shared_ptr<RocEvent> e) { HandleRevealTrack(e); });
}

SideBar::~SideBar()
{
    EventManager::GetInstance()->Unsubscribe(
        static_cast<int>(RocEvents::kTrackVisibilityChanged), m_track_visibility_token);
    EventManager::GetInstance()->Unsubscribe(
        static_cast<int>(RocEvents::kTrackMetadataChanged), m_metadata_changed_token);
    EventManager::GetInstance()->Unsubscribe(
        static_cast<int>(RocEvents::kRevealTrackInTopology), m_reveal_track_token);
}

void
SideBar::HandleRevealTrack(const std::shared_ptr<RocEvent>& event)
{
    auto reveal = std::dynamic_pointer_cast<ScrollToTrackEvent>(event);
    if(!reveal || reveal->GetSourceId() != m_data_provider.GetTraceFilePath())
    {
        return;
    }

    m_reveal_track_id      = reveal->GetTrackID();
    m_reveal_active        = true;
    m_reveal_scroll_frames = REVEAL_SCROLL_FRAMES;
    m_reveal_start         = std::chrono::steady_clock::now();
}

// A track can appear more than once in the tree. Collects the ancestors of
// every matching leaf (so all occurrences glow) and picks one jump target in
// m_reveal_leaf, preferring the Processors subtree.
bool
SideBar::BuildRevealPath(const TreeNode& node, bool in_processors)
{
    if(node.IsLeaf())
    {
        const LeafNode& leaf = static_cast<const LeafNode&>(node);
        if(leaf.track_id != m_reveal_track_id)
        {
            return false;
        }
        if(m_reveal_leaf == nullptr ||
           (in_processors && !m_reveal_leaf_in_processors))
        {
            m_reveal_leaf               = &leaf;
            m_reveal_leaf_in_processors = in_processors;
        }
        return true;
    }

    const bool child_in_processors =
        in_processors || node.type == NodeType::kProcessorList;
    bool contains_match = false;
    for(const auto& child : node.children)
    {
        if(child && BuildRevealPath(*child, child_in_processors))
        {
            contains_match = true;
        }
    }
    if(contains_match)
    {
        m_reveal_path.insert(&node);
    }
    return contains_match;
}

/*
 * Width of the lead-arrow slot, in spaces of the current font. Depends only on
 * the font and style, so one measurement serves every row in the frame.
 */
int
SideBar::MeasureLeadArrowPad() const
{
    ImGui::PushFont(m_settings.GetFontManager().GetFont(FontType::kIcon), 0.0f);
    const float arrow_w = ImGui::CalcTextSize(ICON_ARROW_FORWARD).x;
    ImGui::PopFont();

    const float gap     = ImGui::GetStyle().ItemInnerSpacing.x;
    const float space_w = ImGui::CalcTextSize(" ").x;
    // Rounded up, so the glyph always clears the start of the label.
    return (space_w > 0.0f) ? static_cast<int>((arrow_w + gap) / space_w) + 1
                            : DEFAULT_LEAD_ARROW_PAD;
}

void
SideBar::DrawRevealPulse(const ImVec2& row_min, const ImVec2& row_max) const
{
    double elapsed = std::chrono::duration<double>(
                         std::chrono::steady_clock::now() - m_reveal_start)
                         .count();
    float pulse     = 0.5f + 0.5f * std::sin(static_cast<float>(elapsed) * 6.0f);
    float thickness = REVEAL_HIGHLIGHT_THICKNESS + pulse * 1.5f;

    ImU32 color = m_settings.GetColor(Colors::kEventSearchHighlight);
    ImU32 alpha = (color >> 24) & 0xFF;
    ImU32 new_a = static_cast<ImU32>(alpha * (0.5f + 0.5f * pulse));
    color       = (color & 0x00FFFFFF) | (new_a << 24);

    ImGui::GetWindowDrawList()->AddRect(row_min, row_max, color,
                                        REVEAL_HIGHLIGHT_ROUNDING, 0, thickness);
}

void
SideBar::Render()
{
    if(m_sidebar_tree.root)
    {
        const SidebarTree& sidebar_tree = m_sidebar_tree;
        if(m_reveal_active)
        {
            double elapsed = std::chrono::duration<double>(
                                 std::chrono::steady_clock::now() - m_reveal_start)
                                 .count();
            if(elapsed >= REVEAL_PULSE_DURATION_S)
            {
                m_reveal_active        = false;
                m_reveal_scroll_frames = 0;
                m_reveal_path.clear();
            }
            else
            {
                RenderScheduler::GetInstance().RequestRender();

                if(m_reveal_scroll_frames > 0)
                {
                    // Rebuilt each frame: the tree may have been rebuilt since
                    // the last one, invalidating cached node pointers.
                    m_reveal_path.clear();
                    m_reveal_leaf               = nullptr;
                    m_reveal_leaf_in_processors = false;
                    if(!sidebar_tree.root ||
                       !BuildRevealPath(*sidebar_tree.root, false))
                    {
                        m_reveal_active        = false;
                        m_reveal_scroll_frames = 0;
                        m_reveal_path.clear();
                    }
                }
            }
        }

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(5, 3));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(5, 2));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding,
                            m_settings.GetDefaultStyle().FrameRounding);
        ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, 14.0f);

        // Derived from the font and style, so it is the same for every row.
        // Measured here, after the style is pushed, rather than per branch node.
        m_lead_arrow_pad = MeasureLeadArrowPad();

        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered,
                              ImGui::ColorConvertU32ToFloat4(
                                  m_settings.GetColor(Colors::kBgFrame)));
        if(m_reveal_scroll_frames > 0)
        {
            ImGui::SetNextItemOpen(true);
        }
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

        if(m_reveal_scroll_frames > 0)
        {
            --m_reveal_scroll_frames;
        }
    }
}

void
SideBar::Update()
{
    if(m_data_provider.GetState() != ProviderState::kReady)
    {
        return;
    }

    const uint64_t revision =
        m_data_provider.DataModel().GetTopology().GetRevision();
    if(revision == m_built_revision && !m_rebuild_pending)
    {
        return;
    }

    BuildTree();
    m_built_revision  = revision;
    m_rebuild_pending = false;
}

void
SideBar::BuildTree()
{
    m_sidebar_tree = {};
    // The reveal state holds node pointers into the tree being replaced.
    m_reveal_path.clear();
    m_reveal_leaf               = nullptr;
    m_reveal_leaf_in_processors = false;

    auto root             = std::make_unique<TreeNode>(NodeType::kRoot, "Project", true);
    root->show_eye_button = false;

    TreeNode*            root_node = root.get();
    const TopologyTree&  topology  = m_data_provider.DataModel().GetTopology();
    const TimelineModel& timeline  = m_data_provider.DataModel().GetTimeline();

    const std::vector<TopologyNode*>& nodes = topology.GetNodes();
    if(!nodes.empty())
    {
        TreeNode* node_list =
            AddBranchNode(root_node, NodeType::kNodeList,
                          "Nodes (" + std::to_string(nodes.size()) + ")");
        const bool multi_node = nodes.size() > 1;

        for(const TopologyNode* node : nodes)
        {
            const NodeInfo&   node_info  = static_cast<const NodeInfo&>(*node);
            const size_t      node_index = node_info.display_index;
            const std::string node_label =
                (multi_node && node_index > 0)
                    ? "[" + std::to_string(node_index) + "] " + node_info.host_name
                    : node_info.host_name;

            TreeNode* node_branch =
                AddBranchNode(node_list, NodeType::kNode, node_label);
            if(multi_node && node_index > 0)
            {
                const size_t wheel_size = m_settings.GetColorWheel().size();
                node_branch->show_color_swatch = true;
                node_branch->color_index = wheel_size ? (node_index - 1) % wheel_size : 0;
            }

            const std::vector<TopologyNode*>& processors =
                node_info.GetChildren(TopologyNodeType::kProcessor);
            if(!processors.empty())
            {
                TreeNode* processor_list = AddBranchNode(
                    node_branch, NodeType::kProcessorList,
                    "Processors (" + std::to_string(processors.size()) + ")");
                for(const TopologyNode* processor : processors)
                {
                    BuildProcessorBranch(
                        processor_list, static_cast<const ProcessorInfo&>(*processor),
                        processor->GetChildren(TopologyNodeType::kQueue),
                        processor->GetChildren(TopologyNodeType::kCounter), timeline,
                        true);
                }
            }

            const std::vector<TopologyNode*>& processes =
                node_info.GetChildren(TopologyNodeType::kProcess);
            if(!processes.empty())
            {
                TreeNode* process_list =
                    AddBranchNode(node_branch, NodeType::kProcessList,
                                  "Processes (" + std::to_string(processes.size()) + ")");
                for(const TopologyNode* process : processes)
                {
                    BuildProcessBranch(process_list,
                                       static_cast<const ProcessInfo&>(*process),
                                       timeline);
                }
            }
        }
    }

    /*
     * Every track needs at least one row. Bucket on "the walk emitted no row for
     * it" rather than on track type: a track can be typed (Queue, Stream, ...)
     * and still have none, because its queue or processor was unreachable during
     * the load walk, so keying off the type would drop it entirely.
     */
    std::unordered_set<uint64_t> placed;
    CollectLeafTrackIds(root_node, placed);

    std::vector<const TrackInfo*>       uncategorized;
    const std::vector<const TrackInfo*> track_list = timeline.GetTrackList();
    for(const TrackInfo* track : track_list)
    {
        if(track == nullptr || placed.count(track->id) > 0)
        {
            continue;
        }
        if(track->topology.type != TrackInfo::TrackType::Unknown)
        {
            spdlog::debug("Sidebar: track {} is typed {} but the topology tree has "
                          "no node bound to it; listing it as uncategorized",
                          track->id, static_cast<int>(track->topology.type));
        }
        uncategorized.push_back(track);
    }
    if(!uncategorized.empty())
    {
        TreeNode* uncategorized_list =
            AddBranchNode(root_node, NodeType::kUncategorizedList, "Uncategorized",
                          !nodes.empty(), false);
        for(const TrackInfo* track : uncategorized)
        {
            AddLeafNode(uncategorized_list, track->id, "", timeline);
        }
    }

    m_sidebar_tree.root = std::move(root);
}

TrackItem*
SideBar::TrackFromMetadata(const TrackInfo* info) const
{
    if(!info || !m_tracks || info->index >= m_tracks->size())
    {
        return nullptr;
    }
    TrackItem* track = (*m_tracks)[info->index];
    return (track && track->GetID() == info->id) ? track : nullptr;
}

TrackItem*
SideBar::FindTrack(const uint64_t& track_id) const
{
    return TrackFromMetadata(
        m_data_provider.DataModel().GetTimeline().GetTrack(track_id));
}

void
SideBar::RenderTrackItem(const uint64_t& track_id, bool show_eye_button)
{
    const TrackInfo* track_info =
        m_data_provider.DataModel().GetTimeline().GetTrack(track_id);
    TrackItem*       item = TrackFromMetadata(track_info);
    if(!item)
    {
        return;
    }

    TrackItem& track = *item;

    // Compact mode drops the row buttons entirely; the context menu below still
    // toggles visibility and goes to the track.
    const bool draw_buttons = show_eye_button && !m_settings.CompactSidebar();

    ImGui::PushID(static_cast<int>(track.GetID()));
    ImGui::PushStyleColor(ImGuiCol_Button, m_settings.GetColor(Colors::kTransparent));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, m_settings.GetColor(Colors::kHighlightChart));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, m_settings.GetColor(Colors::kHighlightChart));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2, 0));

    bool display = track.IsDisplayed();
    if(draw_buttons)
    {
        ImGui::PushFont(m_settings.GetFontManager().GetFont(FontType::kIcon), 0.0f);
        if(ImGui::Button(display ? ICON_EYE : ICON_EYE_SLASH))
        {
            track.SetDisplay(!track.IsDisplayed());
            EventManager::GetInstance()->AddEvent(std::make_shared<RocEvent>(
                static_cast<int>(RocEvents::kTrackVisibilityChanged),
                m_data_provider.GetTraceFilePath()));
        }
        ImGui::PopFont();
        if(ImGui::IsItemHovered())
            SetTooltipStyled("Toggle Track Visibility");

        ImGui::SameLine();
        ImGui::PushFont(m_settings.GetFontManager().GetFont(FontType::kIcon), 0.0f);
        if(ImGui::Button(ICON_ARROWS_SHRINK))
        {
            ScrollToTrack(track);
        }
        ImGui::PopFont();
        if(ImGui::IsItemHovered())
            SetTooltipStyled("Scroll To Track");
    }

    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);
    if(draw_buttons)
    {
        ImGui::SameLine();
    }

    if(track_info && !track_info->compare_source.id.empty())
    {
        RenderCompareSourceBadge(track_info, m_settings);
        ImGui::SameLine();
    }

    ImGui::PushStyleColor(
        ImGuiCol_Button,
        m_settings.GetColor(track.IsSelected() ? Colors::kSelection : Colors::kTransparent));
    if(!display)
    {
        ImGui::PushStyleColor(ImGuiCol_Text,
                              ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    }
    if(ImGui::Button(track.GetName().c_str()))
    {
        m_timeline_selection->ToggleSelectTrack(track);
    }
    if(!display)
    {
        ImGui::PopStyleColor();
    }
    ImGui::PopStyleColor();

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(MENU_PAD_X, MENU_PAD_Y));
    if(ImGui::BeginPopupContextItem("##track_ctx"))
    {
        if(ImGui::MenuItem("Go to Track"))
        {
            ScrollToTrack(track);
        }

        ImGui::Separator();

        if(ImGui::MenuItem(display ? "Hide Track" : "Show Track"))
        {
            SetTrackVisibility(track, !display);
            EventManager::GetInstance()->AddEvent(std::make_shared<RocEvent>(
                static_cast<int>(RocEvents::kTrackVisibilityChanged),
                m_data_provider.GetTraceFilePath()));
        }
        if(ImGui::MenuItem("Show All Tracks", nullptr, false,
                           HasTrackVisibility(false)))
        {
            ApplyAllTrackVisibility(true);
        }
        if(ImGui::MenuItem("Hide All But This Track"))
        {
            HideAllButTrack(track_id);
        }

        ImGui::Separator();

        const bool has_selected_tracks =
            m_timeline_selection && m_timeline_selection->HasSelectedTracks();
        if(ImGui::MenuItem("Show Selected Tracks", nullptr, false,
                           has_selected_tracks))
        {
            ApplySelectedTrackVisibility(true);
        }
        if(ImGui::MenuItem("Hide Selected Tracks", nullptr, false,
                           has_selected_tracks))
        {
            ApplySelectedTrackVisibility(false);
        }
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar();

    ImGui::PopID();
}

void
SideBar::ScrollToTrack(TrackItem& track)
{
    EventManager::GetInstance()->AddEvent(std::make_shared<ScrollToTrackEvent>(
        static_cast<int>(RocEvents::kHandleUserGraphNavigationEvent),
        track.GetID(), m_data_provider.GetTraceFilePath()));
}

void
SideBar::SetTrackVisibility(TrackItem& track, bool visible)
{
    if(track.IsDisplayed() == visible)
    {
        return;
    }

    track.SetDisplay(visible);
}

void
SideBar::HideAllButTrack(const uint64_t& track_id)
{
    if(!m_tracks)
    {
        return;
    }

    for(TrackItem* track : *m_tracks)
    {
        if(!track)
        {
            continue;
        }
        SetTrackVisibility(*track, track->GetID() == track_id);
    }
    EventManager::GetInstance()->AddEvent(
        std::make_shared<RocEvent>(static_cast<int>(RocEvents::kTrackVisibilityChanged),
                                   m_data_provider.GetTraceFilePath()));
}

void
SideBar::ApplyAllTrackVisibility(bool visible)
{
    if(!m_tracks)
    {
        return;
    }

    for(auto* track : *m_tracks)
    {
        if(!track)
        {
            continue;
        }
        SetTrackVisibility(*track, visible);
    }
    EventManager::GetInstance()->AddEvent(
        std::make_shared<RocEvent>(static_cast<int>(RocEvents::kTrackVisibilityChanged),
                                   m_data_provider.GetTraceFilePath()));
}

void
SideBar::ApplySelectedTrackVisibility(bool visible)
{
    if(!m_tracks)
    {
        return;
    }

    for(auto* track : *m_tracks)
    {
        if(!track || !track->IsSelected())
        {
            continue;
        }
        SetTrackVisibility(*track, visible);
    }
    EventManager::GetInstance()->AddEvent(
        std::make_shared<RocEvent>(static_cast<int>(RocEvents::kTrackVisibilityChanged),
                                   m_data_provider.GetTraceFilePath()));
}

bool
SideBar::HasTrackVisibility(bool visible) const
{
    if(!m_tracks)
    {
        return false;
    }

    for(const auto* track : *m_tracks)
    {
        if(track && track->IsDisplayed() == visible)
        {
            return true;
        }
    }

    return false;
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
    const TrackItem* track = FindTrack(leaf.track_id);
    if(!track)
    {
        return EyeButtonState::kAllHidden;
    }

    return track->IsDisplayed() ? EyeButtonState::kAllVisible
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
    if(!m_tracks || m_tracks->empty())
    {
        return;
    }

    std::unordered_set<uint64_t> visited_tracks;
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
            if(visited_tracks.insert(leaf.track_id).second)
            {
                TrackItem* track = FindTrack(leaf.track_id);
                if(track && track->IsDisplayed() != visible)
                {
                    track->SetDisplay(visible);
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
    EventManager::GetInstance()->AddEvent(
        std::make_shared<RocEvent>(static_cast<int>(RocEvents::kTrackVisibilityChanged),
                                   m_data_provider.GetTraceFilePath()));
}

void
SideBar::RenderLeafNode(const LeafNode& leaf)
{
    ImGui::PushID(static_cast<const void*>(&leaf));

    // Every occurrence of the track glows; only the prioritized one (chosen in
    // BuildRevealPath) is scrolled into view.
    const bool   is_reveal_match =
        m_reveal_active && leaf.track_id == m_reveal_track_id;
    const ImVec2 row_min          = ImGui::GetCursorScreenPos();

    RenderTrackItem(leaf.track_id, leaf.show_eye_button);

    if(is_reveal_match)
    {
        if(m_reveal_scroll_frames > 0 && &leaf == m_reveal_leaf)
        {
            ImGui::SetScrollHereY(0.5f);
        }
        const float content_max_x =
            ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;
        const ImVec2 row_max = ImVec2(content_max_x, ImGui::GetItemRectMax().y);
        DrawRevealPulse(row_min, row_max);
    }

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
    if(node.show_eye_button && !m_settings.CompactSidebar())
    {
        EyeButtonState current_state = GetTreeState(state_source);
        EyeButtonState new_state     = DrawEyeButton(current_state);
        if(new_state != current_state && new_state != EyeButtonState::kMixed)
        {
            ApplyVisibility(apply_target, new_state == EyeButtonState::kAllVisible);
        }
        ImGui::SameLine();
    }

    const bool color_arrow = node.show_color_swatch && m_settings.ShowNodeColors() &&
                             !m_settings.GetColorWheel().empty();

    bool open = true;
    if(node.collapsable)
    {
        // While revealing a track, force every ancestor on the path open so the
        // target leaf is laid out and can be scrolled to.
        if(m_reveal_scroll_frames > 0 && m_reveal_path.count(&node) > 0)
        {
            ImGui::SetNextItemOpen(true);
        }
        const ImVec2 node_pos = ImGui::GetCursorScreenPos();

        /*
         * Lead arrow: blank out a slot after the chevron with %*s, then draw the
         * glyph into it. The icon font is a separate ImFont rather than a range
         * merged into the text font (merging rendered corrupted glyphs on
         * Linux), so the arrow cannot just be part of the label string.
         */
        const int pad = node.show_lead_arrow ? m_lead_arrow_pad : 0;
        open = ImGui::TreeNodeEx(node.label.c_str(), HEADER_FLAGS, "%*s%s", pad, "",
                                 node.label.c_str());

        if(node.show_lead_arrow)
        {
            ImFont*     icon_font = m_settings.GetFontManager().GetFont(FontType::kIcon);
            const float arrow_size = ImGui::GetFontSize();
            const float arrow_x    = node_pos.x + arrow_size +
                                  ImGui::GetStyle().FramePadding.x * FRAMED_LABEL_PAD_MULT;
            const float cy =
                (ImGui::GetItemRectMin().y + ImGui::GetItemRectMax().y) * 0.5f;
            ImGui::GetWindowDrawList()->AddText(
                icon_font, arrow_size, ImVec2(arrow_x, cy - arrow_size * 0.5f),
                ImGui::GetColorU32(ImGuiCol_Text), ICON_ARROW_FORWARD);
        }

        if(color_arrow)
        {
            const std::vector<ImU32>& wheel     = m_settings.GetColorWheel();
            const float               font_size = ImGui::GetFontSize();
            DrawTreeArrow(
                ImGui::GetWindowDrawList(),
                node_pos.x + ImGui::GetStyle().FramePadding.x + font_size * 0.5f,
                (ImGui::GetItemRectMin().y + ImGui::GetItemRectMax().y) * 0.5f, font_size,
                open, wheel[node.color_index % wheel.size()]);
        }

        if(node.show_eye_button)
        {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                                ImVec2(MENU_PAD_X, MENU_PAD_Y));
            if(ImGui::BeginPopupContextItem("##branch_ctx"))
            {
                EyeButtonState current = GetTreeState(state_source);
                if(ImGui::MenuItem("Show All Tracks Below", nullptr, false,
                                   current != EyeButtonState::kAllVisible))
                {
                    ApplyVisibility(apply_target, true);
                }
                if(ImGui::MenuItem("Hide All Tracks Below", nullptr, false,
                                   current != EyeButtonState::kAllHidden))
                {
                    ApplyVisibility(apply_target, false);
                }
                ImGui::EndPopup();
            }
            ImGui::PopStyleVar();
        }
    }

    if(open)
    {
        // While inside a node's subtree, tint all descendant connector lines
        // with the node color (restored when the subtree finishes).
        const ImU32 prev_node_color = m_active_node_color;
        if(node.show_color_swatch && m_settings.ShowNodeColors())
        {
            const std::vector<ImU32>& wheel = m_settings.GetColorWheel();
            if(!wheel.empty())
            {
                m_active_node_color = wheel[node.color_index % wheel.size()];
            }
        }
        RenderTreeChildren(node);
        m_active_node_color = prev_node_color;
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

    // m_active_node_color carries the enclosing node's color down the whole
    // subtree so deeper connector lines are tinted too, not just direct children.
    TreeConnector tc(m_settings, m_active_node_color);
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
