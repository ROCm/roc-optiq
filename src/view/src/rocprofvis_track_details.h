// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once
#include "widgets/rocprofvis_widget.h"
#include <vector>

namespace RocProfVis
{
namespace View
{

class DataProvider;
class TrackTopology;
class NodeModel;
class ProcessModel;
class QueueModel;
class ThreadModel;
class CounterModel;
class TrackSelectionChangedEvent;
struct InfoTable;

class TrackDetails : public RocWidget
{
public:
    TrackDetails(DataProvider& dp, std::shared_ptr<TrackTopology> topology);
    ~TrackDetails();
    virtual void Render() override;
    virtual void Update() override;

    void HandleTrackSelectionChanged(std::shared_ptr<TrackSelectionChangedEvent> event);

private:
    struct Details
    {
        const std::string& track_name;
        NodeModel&              node;
        ProcessModel&           process;
        QueueModel*             queue;
        ThreadModel*            thread;
        CounterModel*           counter;
    };

    void RenderTable(InfoTable& table);

    std::shared_ptr<TrackTopology> m_track_topology;
    DataProvider&                  m_data_provider;
    std::vector<uint64_t>          m_selected_track_ids;
    bool                           m_selection_dirty;
    std::vector<Details>           m_track_details;
};

}  // namespace View
}  // namespace RocProfVis