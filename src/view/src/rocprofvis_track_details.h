// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once
#include "rocprofvis_event_manager.h"
#include "widgets/rocprofvis_widget.h"
#include <list>

namespace RocProfVis
{
namespace View
{

class DataProvider;
class TrackTopology;
struct NodeModel;
struct ProcessModel;
struct ProcessorModel;
struct IterableModel;
class TimelineSelection;
struct InfoTable;

class TrackDetails : public RocWidget
{
public:
    TrackDetails(DataProvider& dp, std::shared_ptr<TrackTopology> topology,
                 std::shared_ptr<TimelineSelection> timeline_selection);
    ~TrackDetails();
    virtual void Render() override;
    virtual void Update() override;

    void HandleTrackSelectionChanged(const uint64_t track_id, const bool selected);

private:
    struct DetailItem
    {
        const int          id;
        const uint64_t     track_id;
        const std::string* track_name;
        NodeModel*         node;
        ProcessModel*      process;
        ProcessorModel*    processor;
        IterableModel*     queue;
        IterableModel*     instrumented_thread;
        IterableModel*     sampled_thread;
        IterableModel*     counter;
        IterableModel*     stream;

        bool operator==(const DetailItem& other) const
        {
            return track_id == other.track_id;
        }
    };

    void RenderTable(InfoTable& table);

    std::shared_ptr<TrackTopology>     m_track_topology;
    DataProvider&                      m_data_provider;
    std::shared_ptr<TimelineSelection> m_timeline_selection;
    bool                               m_selection_dirty;
    std::list<DetailItem>              m_track_details;
    int                                m_detail_item_id;
    bool                               m_data_valid;

    EventManager::SubscriptionToken m_topology_changed_event_token;
    EventManager::SubscriptionToken m_track_metadata_changed_event_token;
};

}  // namespace View
}  // namespace RocProfVis