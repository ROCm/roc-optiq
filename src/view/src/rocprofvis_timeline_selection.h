// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once
#include <cstdint>
#include <limits>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace RocProfVis
{
namespace View
{

class DataProvider;
struct TrackGraph;

class TimelineSelection
{
public:
    TimelineSelection(DataProvider& dp);
    ~TimelineSelection();

    void SelectTrack(TrackGraph& graph);
    void UnselectTrack(TrackGraph& graph);
    void ToggleSelectTrack(TrackGraph& graph);
    void UnselectAllTracks(std::vector<TrackGraph>& graphs);
    bool GetSelectedTracks(std::vector<uint64_t>& track_ids) const;
    bool HasSelectedTracks() const;

    void SelectTimeRange(double start_ts, double end_ts);
    bool GetSelectedTimeRange(double& start_ts_out, double& end_ts_out) const;
    void ClearTimeRange();
    bool HasValidTimeRangeSelection() const;

    void SelectTrackEvent(uint64_t track_id, uint64_t event_id, double start_ts, double end_ts);
    void UnselectTrackEvent(uint64_t track_id, uint64_t event_id);
    bool GetSelectedEvents(std::vector<uint64_t>& event_ids) const;
    bool EventSelected(uint64_t event_id) const;
    void UnselectAllEvents();
    bool HasSelectedEvents() const;
    bool GetSelectedEventsTimeRange(double& start_ts_out, double& end_ts_out) const;

    static constexpr double INVALID_SELECTION_TIME =
        std::numeric_limits<double>::lowest();
    static constexpr uint64_t INVALID_SELECTION_ID = std::numeric_limits<uint64_t>::max();

private:
    /*
     * Notifies event manager if selections have changed.
     */
    void SendEventSelectionChanged(uint64_t event_id, uint64_t track_id, bool selected,
                                   bool all = false);
    void SendTrackSelectionChanged(uint64_t track_id, bool selected);

    DataProvider& m_data_provider;

    std::unordered_set<uint64_t> m_selected_track_ids;
    double                       m_selected_range_start;
    double                       m_selected_range_end;

    std::unordered_set<uint64_t>                        m_selected_event_ids;
    std::unordered_map<uint64_t, std::pair<double, double>> m_selected_event_times;
};

}  // namespace View
}  // namespace RocProfVis
