// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once
#include <cstdint>
#include <limits>
#include <unordered_set>

namespace RocProfVis
{
namespace View
{

struct rocprofvis_graph_t;

class TimelineSelection
{
public:
    TimelineSelection();
    ~TimelineSelection();

    /*
     * Notifies event manager if selections have changed.
     */
    void Update();

    void SelectTrack(rocprofvis_graph_t& graph);
    void UnselectTrack(rocprofvis_graph_t& graph);
    void ToggleSelectTrack(rocprofvis_graph_t& graph);

    void SelectTimeRange(double start_ts, double end_ts);
    bool GetSelectedTimeRange(double& start_ts_out, double& end_ts_out) const;
    void ClearTimeRange();
    bool HasValidTimeRangeSelection() const;
    static constexpr double INVALID_SELECTION_TIME =
        std::numeric_limits<double>::lowest();

    void SelectTrackEvent(uint64_t track_id, uint64_t event_id);
    void UnselectTrackEvent(uint64_t track_id, uint64_t event_id);
    bool GetSelectedEvents(std::vector<uint64_t>& event_ids);
    bool EventSelected(uint64_t event_id);

private:
    std::unordered_set<uint64_t> m_selected_track_ids;
    double                       m_selected_range_start;
    double                       m_selected_range_end;

    std::unordered_set<uint64_t> m_selected_event_ids;
    uint64_t                     m_last_event_id;
    uint64_t                     m_last_event_track_id;
    bool                         m_last_event_selected;

    bool m_tracks_changed;
    bool m_range_changed;
    bool m_events_changed;
};

}  // namespace View
}  // namespace RocProfVis