// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once
#include "rocprofvis_track_topology.h"
#include "widgets/rocprofvis_widget.h"

#include <optional>
#include <vector>

namespace RocProfVis
{
namespace View
{

class SettingsManager;
class TrackTopology;
class TimelineSelection;
class DataProvider;


class ISidebarNode
{
};


class NewSideBar : public RocWidget
{
public:
    NewSideBar(std::shared_ptr<TrackTopology>                   topology,
            std::shared_ptr<TimelineSelection>               timeline_selection,
            std::shared_ptr<std::vector<TrackGraph>> graphs, DataProvider& dp);
    ~NewSideBar();
    virtual void Render() override;
    virtual void Update() override;

private:
    SettingsManager&                         m_settings;
    std::shared_ptr<TrackTopology>           m_track_topology;
    std::shared_ptr<TimelineSelection>       m_timeline_selection;
    std::shared_ptr<std::vector<TrackGraph>> m_graphs;
    DataProvider&                            m_data_provider;
};

}  // namespace View
}  // namespace RocProfVis