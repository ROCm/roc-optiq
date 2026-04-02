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
    enum class GraphBanchType
    {
        kNoBranches,
        kProcessorTreeBranch
    };
    bool RenderTrackItem(const uint64_t& index);

    template<typename Model>
    bool IsAllSubItemsHidden(const std::vector<Model>& container);
    template<typename Model>
    void HideAllSubItems(const std::vector<Model>& container);
    void HideAllUncategorizedItems(const std::vector<uint64_t>& indices);
    void UnhideAllUncategorizedItems(const std::vector<uint64_t>& indices);
    template<typename Model>
    void UnhideAllSubItems(const std::vector<Model>& container);

    EyeButtonState DrawTopology(const TopologyModel& topology,
                                EyeButtonState       parent_eye_button_state,
                                bool                 show_eye_button = true);

    EyeButtonState DrawNodes(const std::vector<NodeModel>& nodes,
                             EyeButtonState                parent_eye_button_state,
                             bool                          show_eye_button = true);

    EyeButtonState DrawNode(const NodeModel& node, EyeButtonState parent_eye_button_state,
                            bool show_eye_button = true);

    EyeButtonState DrawProcesses(const std::vector<ProcessModel>& processes,
                                 EyeButtonState                   parent_eye_button_state,
                                 bool                             show_eye_button = true);

    EyeButtonState DrawProcessors(const std::vector<ProcessorModel>& processors,
                                 EyeButtonState                   parent_eye_button_state,
                                 bool                             show_eye_button = true,
                                 uint64_t                         parent_id = 0);

    template<typename Model>
    EyeButtonState DrawCollapsable(const std::vector<Model>& container,
                                   const std::string&                collapsable_header,
                                   EyeButtonState parent_eye_button_state);

    EyeButtonState DrawEyeButton(EyeButtonState eye_button_state);
    bool           IsEyeButtonVisible();

    SettingsManager&                                 m_settings;
    std::shared_ptr<TrackTopology>                   m_track_topology;
    std::shared_ptr<TimelineSelection>               m_timeline_selection;
    std::shared_ptr<std::vector<TrackGraph>> m_graphs;
    DataProvider&                                    m_data_provider;
    EyeButtonState                                   m_root_eye_button_state;
};

}  // namespace View
}  // namespace RocProfVis