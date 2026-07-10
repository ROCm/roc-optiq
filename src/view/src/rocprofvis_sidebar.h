// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once
#include "rocprofvis_event_manager.h"
#include "rocprofvis_track_topology.h"
#include "widgets/rocprofvis_widget.h"

#include <chrono>
#include <unordered_set>
#include <vector>

namespace RocProfVis
{
namespace View
{

class SettingsManager;
class TrackTopology;
class TimelineSelection;
class DataProvider;
class TrackItem;

class SideBar : public RocWidget
{
public:
    SideBar(std::shared_ptr<TrackTopology>         topology,
            std::shared_ptr<TimelineSelection>     timeline_selection,
            std::shared_ptr<std::vector<TrackItem*>> tracks,
            DataProvider&                          dp);
    ~SideBar();
    virtual void Render() override;
    virtual void Update() override;

private:
    enum class EyeButtonState
    {
        kAllVisible,
        kAllHidden,
        kMixed
    };

    void               RenderTrackItem(const uint64_t& index,
                                       bool show_eye_button = true);
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

    // "Reveal in topology": locate a track's leaf, expand only the ancestors
    // needed to see it, scroll it into view, and pulse-highlight the row.
    void               HandleRevealTrack(const std::shared_ptr<RocEvent>& event);
    bool               BuildRevealPath(const TreeNode& node);
    void               DrawRevealPulse(const ImVec2& row_min, const ImVec2& row_max) const;

    SettingsManager&                         m_settings;
    std::shared_ptr<TrackTopology>           m_track_topology;
    std::shared_ptr<TimelineSelection>       m_timeline_selection;
    std::shared_ptr<std::vector<TrackItem*>> m_tracks;
    DataProvider&                            m_data_provider;
    bool                                     m_eye_state_dirty = false;
    ImU32                                    m_active_node_color = 0;

    EventManager::SubscriptionToken       m_reveal_track_token;
    uint64_t                              m_reveal_track_id = 0;
    bool                                  m_reveal_active   = false;
    int                                   m_reveal_scroll_frames = 0;
    std::chrono::steady_clock::time_point m_reveal_start;
    std::unordered_set<const TreeNode*>   m_reveal_path;
};

}  // namespace View
}  // namespace RocProfVis
