// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once
#include "model/rocprofvis_model_types.h"
#include "rocprofvis_event_manager.h"
#include "widgets/rocprofvis_gui_helpers.h"
#include "widgets/rocprofvis_widget.h"
#include <list>
#include <memory>
#include <optional>
#include <string>

namespace RocProfVis
{
namespace View
{

class DataProvider;
class SettingsManager;
class TrackTopology;
class TimelineSelection;
class HSplitContainer;
struct NodeModel;
struct ProcessModel;
struct ProcessorModel;
struct IterableModel;
struct StreamModel;
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

    friend struct TrackDetailsTestPeer;

private:
    struct DetailItem
    {
        struct Parents
        {
            NodeModel*    node;
            ProcessModel* process;
            bool          expand;
        };

        const uint64_t                 track_id;
        TrackInfo::TrackType           track_type;
        std::string                    track_name;
        Parents                        parents;
        IterableModel*                 track;
        StreamModel*                   stream_track;
        const AnalysisTrackStatistics* stats;

        bool operator==(const DetailItem& other) const
        {
            return track_id == other.track_id;
        }
    };

    void RenderTable(InfoTable& table, const char* table_id,
                     const AnalysisTrackStatistics* = nullptr);

    /* Renders the selected tracks' cards and returns how many were drawn. In
     * compare mode source_index limits a column to one source's tracks.
     */
    size_t RenderDetailList(std::optional<uint64_t> source_index);
    void   RenderSourceColumn(size_t source_index);

    std::shared_ptr<TrackTopology>     m_track_topology;
    DataProvider&                      m_data_provider;
    std::shared_ptr<TimelineSelection> m_timeline_selection;
    SettingsManager&                   m_settings;
    bool                               m_selection_dirty;
    std::list<DetailItem>              m_track_details;
    bool                               m_data_valid;
    CellMenuTarget                     m_cell_menu;

    bool                             m_compare_mode;
    std::shared_ptr<HSplitContainer> m_detail_split;

    EventManager::SubscriptionToken m_topology_changed_event_token;
    EventManager::SubscriptionToken m_track_metadata_changed_event_token;
};

}  // namespace View
}  // namespace RocProfVis