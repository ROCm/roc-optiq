// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once
#include "rocprofvis_new_sidebar.h"
#include "rocprofvis_track_topology.h"
#include "widgets/rocprofvis_widget.h"
#include "rocprofvis_settings_manager.h"


#include <optional>
#include <vector>

namespace RocProfVis
{
namespace View
{

NewSideBar::NewSideBar(std::shared_ptr<TrackTopology>     topology,
                 std::shared_ptr<TimelineSelection>       timeline_selection,
                 std::shared_ptr<std::vector<TrackGraph>> graphs,
                 DataProvider&                            dp)
: m_settings(SettingsManager::GetInstance())
, m_track_topology(topology)
, m_timeline_selection(timeline_selection)
, m_graphs(graphs)
, m_data_provider(dp)
{}

NewSideBar::~NewSideBar() {}

void
NewSideBar::Render()
{}

void
NewSideBar::Update()
{}


}  // namespace View
}  // namespace RocProfVis