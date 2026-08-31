// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once
#include "rocprofvis_data_provider.h"
#include "rocprofvis_event_manager.h"
#include "rocprofvis_tree_node.h"
#include <memory>
#include <string>
#include <vector>

namespace RocProfVis
{
namespace View
{

/*
 * Projects the system topology onto the sidebar.
 *
 * The topology itself lives in the data model as a TopologyTree; this class
 * only turns it into the presentation tree the sidebar renders (list headers,
 * inline stream subtrees, per-row eye buttons) plus the topology-order track
 * list the timeline sorts by.
 */
class TrackTopology
{
public:
    TrackTopology(DataProvider& dp);
    ~TrackTopology();
    void Update();

    bool                         Dirty();
    const SidebarTree&           GetSidebarTree() const;
    const std::vector<uint64_t>& GetTrackIdsInTreeOrder() const;

private:
    void BuildSidebarTree();

    DataProvider&                   m_data_provider;
    bool                            m_dirty;
    EventManager::SubscriptionToken m_metadata_changed_event_token;

    SidebarTree           m_sidebar_tree;
    std::vector<uint64_t> m_track_order;
};

}  // namespace View
}  // namespace RocProfVis
