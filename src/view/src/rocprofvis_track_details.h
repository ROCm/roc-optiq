// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once
#include "model/rocprofvis_model_types.h"
#include "model/rocprofvis_topology_model.h"
#include "rocprofvis_event_manager.h"
#include "widgets/rocprofvis_gui_helpers.h"
#include "widgets/rocprofvis_widget.h"
#include <list>
#include <string>
#include <vector>

namespace RocProfVis
{
namespace View
{

class DataProvider;
class SettingsManager;
class TimelineSelection;

/*
 * Two-column property table for one topology node. Built per selected track,
 * so it covers the handful of tracks the pane actually shows rather than the
 * whole trace.
 */
struct DetailsTable
{
    struct Cell
    {
        std::string data;
        bool        expand = false;
        // Timestamps are kept raw in data and rendered from formatted, which is
        // rebuilt when the time-unit setting changes.
        bool        is_time = false;
        std::string formatted{};
    };

    std::vector<std::vector<Cell>> cells;
};

class TrackDetails : public RocWidget
{
public:
    TrackDetails(DataProvider& dp, std::shared_ptr<TimelineSelection> timeline_selection);
    ~TrackDetails();
    virtual void Render() override;
    virtual void Update() override;

    void HandleTrackSelectionChanged(const uint64_t track_id, const bool selected);

    friend struct TrackDetailsTestPeer;

private:
    struct DetailItem
    {
        struct Parents
        {
            const NodeInfo*    node    = nullptr;
            const ProcessInfo* process = nullptr;
            bool               expand  = false;
        };

        const uint64_t                 track_id;
        TrackInfo::TrackType           track_type = TrackInfo::TrackType::Unknown;
        std::string                    track_name;
        Parents                        parents;
        const TopologyNode*            track = nullptr;
        DetailsTable                   node_table;
        DetailsTable                   process_table;
        DetailsTable                   track_table;
        const AnalysisTrackStatistics* stats = nullptr;

        bool operator==(const DetailItem& other) const
        {
            return track_id == other.track_id;
        }
    };

    void Resolve(DetailItem& item, const TrackInfo& metadata);
    void BuildTables(DetailItem& item);
    // Rebuilds the display string of the item's time cells from the current
    // time-unit setting.
    void FormatTimeCells(DetailItem& item);

    void RenderTable(DetailsTable& table, const char* table_id,
                     const AnalysisTrackStatistics* = nullptr);

    DataProvider&                      m_data_provider;
    std::shared_ptr<TimelineSelection> m_timeline_selection;
    SettingsManager&                   m_settings;
    bool                               m_selection_dirty;
    std::list<DetailItem>              m_track_details;
    bool                               m_data_valid;
    CellMenuTarget                     m_cell_menu;
    // Revision of the topology tree the resolved items point into.
    uint64_t                           m_topology_revision;

    EventManager::SubscriptionToken m_track_metadata_changed_event_token;
    EventManager::SubscriptionToken m_time_format_changed_token;
};

}  // namespace View
}  // namespace RocProfVis
