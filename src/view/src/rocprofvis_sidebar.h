// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once
#include "rocprofvis_event_manager.h"
#include "rocprofvis_tree_node.h"
#include "widgets/rocprofvis_widget.h"

#include <chrono>
#include <unordered_set>
#include <vector>

namespace RocProfVis
{
namespace View
{

class SettingsManager;
class TimelineSelection;
class DataProvider;
class TrackItem;
struct TrackInfo;

/*
 * The topology pane.
 *
 * Owns the presentation tree it renders: the topology itself lives in the data
 * model as a TopologyTree, and this projects it into rows, adding what is
 * purely presentational (group headers, eye-state cache, node colors, the
 * inline processor subtree under a stream).
 */
class SideBar : public RocWidget
{
public:
    SideBar(std::shared_ptr<TimelineSelection>       timeline_selection,
            std::shared_ptr<std::vector<TrackItem*>> tracks,
            DataProvider&                           dp);
    ~SideBar();
    virtual void Render() override;
    virtual void Update() override;

    friend struct SideBarTestPeer;

private:
    enum class EyeButtonState
    {
        kAllVisible,
        kAllHidden,
        kMixed
    };

    // Rebuilds the presentation tree from the topology tree.
    void               BuildTree();

    // Track ids are stable, their position in m_tracks is not, so a track's
    // metadata provides its current index. Callers that already hold the
    // metadata use TrackFromMetadata to avoid looking it up twice per row.
    TrackItem*         TrackFromMetadata(const TrackInfo* info) const;
    TrackItem*         FindTrack(const uint64_t& track_id) const;
    void               RenderTrackItem(const uint64_t& track_id,
                                       bool show_eye_button = true);
    void               ScrollToTrack(TrackItem& track);
    void               SetTrackVisibility(TrackItem& track, bool visible);
    void               HideAllButTrack(const uint64_t& track_id);
    void               ApplyAllTrackVisibility(bool visible);
    void               ApplySelectedTrackVisibility(bool visible);
    bool               HasTrackVisibility(bool visible) const;
    EyeButtonState     MergeEyeButtonState(EyeButtonState lhs,
                                           EyeButtonState rhs) const;
    EyeButtonState     GetLeafState(const LeafNode& leaf) const;
    EyeButtonState     GetTreeState(const TreeNode& node) const;
    EyeButtonState     GetSubtreeEyeState(const TreeNode& node,
                                          bool cross_boundaries) const;
    void               ApplyVisibility(const TreeNode& node, bool visible);
    void               RenderLeafNode(const LeafNode& leaf);
    void               RenderBranchNode(const TreeNode& node,
                                        const TreeNode* state_node  = nullptr,
                                        const TreeNode* target_node = nullptr);
    void               RenderTreeNode(const TreeNode& node);
    void               RenderTreeChildren(const TreeNode& node);
    EyeButtonState     DrawEyeButton(EyeButtonState eye_button_state);
    void               InvalidateEyeStateCache(const TreeNode& node);
    int                MeasureLeadArrowPad() const;

    // "Reveal in topology": locate a track's leaf, expand only the ancestors
    // needed to see it, scroll it into view, and pulse-highlight the row.
    void               HandleRevealTrack(const std::shared_ptr<RocEvent>& event);
    bool               BuildRevealPath(const TreeNode& node, bool in_processors);
    void               DrawRevealPulse(const ImVec2& row_min, const ImVec2& row_max) const;

    SettingsManager&                         m_settings;
    std::shared_ptr<TimelineSelection>       m_timeline_selection;
    std::shared_ptr<std::vector<TrackItem*>> m_tracks;
    DataProvider&                            m_data_provider;
    ImU32                                    m_active_node_color;
    // Remeasured once per Render(), shared by every lead-arrow row that frame.
    int                                      m_lead_arrow_pad = 0;
    EventManager::SubscriptionToken          m_track_visibility_token;
    EventManager::SubscriptionToken          m_metadata_changed_token;

    SidebarTree m_sidebar_tree;
    // Revision of the topology tree m_sidebar_tree was built from. Row labels
    // also come from track metadata, so a metadata change rebuilds too even
    // when the topology itself has not moved.
    uint64_t    m_built_revision = 0;
    bool        m_rebuild_pending = true;

    EventManager::SubscriptionToken       m_reveal_track_token;
    uint64_t                              m_reveal_track_id = 0;
    bool                                  m_reveal_active   = false;
    int                                   m_reveal_scroll_frames = 0;
    std::chrono::steady_clock::time_point m_reveal_start;
    std::unordered_set<const TreeNode*>   m_reveal_path;
    const LeafNode*                       m_reveal_leaf = nullptr;
    bool                                  m_reveal_leaf_in_processors = false;
};

}  // namespace View
}  // namespace RocProfVis
