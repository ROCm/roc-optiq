// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace RocProfVis
{
namespace View
{

enum class NodeType : uint8_t
{
    kRoot,
    kNodeList,
    kNode,
    kProcessorList,
    kProcessor,
    kQueueList,
    kCounterList,
    kProcessList,
    kProcess,
    kStreamList,
    kStream,
    kInstrumentedThreadList,
    kSampledThreadList,
    kUncategorizedList,
    kLeaf
};

class TreeNode
{
public:
    TreeNode(NodeType t, const std::string& lbl, bool can_collapse = true)
    : type(t)
    , label(lbl)
    , collapsable(can_collapse)
    {}

    virtual ~TreeNode() = default;

    virtual bool IsLeaf() const { return false; }

    TreeNode* AddChild(std::unique_ptr<TreeNode> child)
    {
        TreeNode* raw = child.get();
        children.push_back(std::move(child));
        return raw;
    }

    NodeType                                type;
    std::string                             label;
    bool                                    collapsable;
    bool                                    show_eye_button         = true;
    bool                                    breaks_visibility_chain = false;
    bool                                    framed                  = false;
    bool                                    render_children_inline  = false;
    std::vector<std::unique_ptr<TreeNode>>  children;
};

class LeafNode : public TreeNode
{
public:
    LeafNode(const std::string& lbl, uint64_t graph_idx, uint64_t trk_id)
    : TreeNode(NodeType::kLeaf, lbl, false)
    , graph_index(graph_idx)
    , track_id(trk_id)
    {}

    bool IsLeaf() const override { return true; }

    uint64_t graph_index = 0;
    uint64_t track_id    = 0;
};

struct SidebarTree
{
    std::unique_ptr<TreeNode>                            root;
    std::unordered_map<uint64_t, std::vector<LeafNode*>> leaf_lookup;
};

}  // namespace View
}  // namespace RocProfVis
