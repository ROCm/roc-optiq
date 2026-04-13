// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once
#include "rocprofvis_track_topology.h"
#include "widgets/rocprofvis_widget.h"

#include <vector>

namespace RocProfVis
{
namespace View
{

class SettingsManager;
class TrackTopology;
class TimelineSelection;
class DataProvider;

class SideBar : public RocWidget
{
public:
    SideBar(std::shared_ptr<TrackTopology>                   topology,
            std::shared_ptr<TimelineSelection>               timeline_selection,
            std::shared_ptr<std::vector<TrackGraph>> graphs, DataProvider& dp);
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

    bool RenderTrackItem(const uint64_t& index, bool show_eye_button = true);
    EyeButtonState MergeEyeButtonState(EyeButtonState lhs,
                                       EyeButtonState rhs) const;
    EyeButtonState GetLeafState(const LeafNode& leaf) const;
    EyeButtonState GetTreeState(const TreeNode& node) const;
    ImGuiTreeNodeFlags GetTreeNodeFlags(const TreeNode& node) const;
    void ApplyVisibility(const TreeNode& node, bool visible);
    void RenderLeafNode(const LeafNode& leaf);
    void RenderBranchNode(const TreeNode& node,
                          const TreeNode* state_node  = nullptr,
                          const TreeNode* target_node = nullptr);
    void RenderTreeNode(const TreeNode& node);
    void RenderTreeChildren(const TreeNode& node);
    EyeButtonState DrawEyeButton(EyeButtonState eye_button_state);

    SettingsManager&                                 m_settings;
    std::shared_ptr<TrackTopology>                   m_track_topology;
    std::shared_ptr<TimelineSelection>               m_timeline_selection;
    std::shared_ptr<std::vector<TrackGraph>> m_graphs;
    DataProvider&                                    m_data_provider;
};

}  // namespace View
}  // namespace RocProfVis