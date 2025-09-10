// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once
#include "rocprofvis_view_structs.h"
#include "rocprofvis_data_provider.h"
#include "widgets/rocprofvis_widget.h"
#include <vector>

namespace RocProfVis
{
namespace View
{

class SettingsManager;
class TrackTopology;
class TimelineSelection;

class SideBar : public RocWidget
{
public:
    SideBar(std::shared_ptr<TrackTopology>     topology,
            std::shared_ptr<TimelineSelection> timeline_selection,
            std::vector<rocprofvis_graph_t>*   graphs,
            DataProvider &dp);
    ~SideBar();
    virtual void Render() override;
    virtual void Update() override;

private:
    void RenderTrackItem(const int& index);

    SettingsManager&                   m_settings;
    std::shared_ptr<TrackTopology>     m_track_topology;
    std::shared_ptr<TimelineSelection> m_timeline_selection;
    std::vector<rocprofvis_graph_t>*   m_graphs;
    DataProvider &                     m_data_provider;
};

}  // namespace View
}  // namespace RocProfVis