// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once
#include "rocprofvis_view_structs.h"
#include "widgets/rocprofvis_widget.h"
#include <vector>

namespace RocProfVis
{
namespace View
{

class Settings;
class TrackTopology;
class TimelineSelection;

class SideBar : public RocWidget
{
public:
    SideBar(std::shared_ptr<TrackTopology>     topology,
            std::shared_ptr<TimelineSelection> timeline_selection,
            std::vector<rocprofvis_graph_t>*   graphs);
    ~SideBar();
    virtual void Render() override;
    virtual void Update() override;

private:
    void RenderTrackItem(const int& index);

    Settings&                          m_settings;
    std::shared_ptr<TrackTopology>     m_track_topology;
    std::shared_ptr<TimelineSelection> m_timeline_selection;
    std::vector<rocprofvis_graph_t>*   m_graphs;
};

}  // namespace View
}  // namespace RocProfVis