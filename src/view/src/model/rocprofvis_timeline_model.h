// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_model_types.h"
#include "rocprofvis_raw_track_data.h"

#include <map>
#include <tuple>
#include <unordered_map>
#include <vector>

namespace RocProfVis
{
namespace View
{
namespace Model
{

/**
 * @brief Manages timeline visualization data.
 * 
 * This model holds track metadata, event data, histograms, and mini-maps
 * used for rendering the trace timeline visualization.
 */
class TimelineModel
{
public:
    TimelineModel();
    ~TimelineModel();

    // Timeline bounds
    double GetMinTimestamp() const { return m_min_ts; }
    double GetMaxTimestamp() const { return m_max_ts; }
    void SetTimeRange(double min_ts, double max_ts);

    // Track count
    uint64_t GetTrackCount() const { return m_num_tracks; }
    void SetTrackCount(uint64_t count) { m_num_tracks = count; }

    // Track metadata access
    const TrackInfo* GetTrack(uint64_t track_id) const;
    std::vector<const TrackInfo*> GetTrackList() const;
    void AddTrack(uint64_t track_id, TrackInfo&& track);
    void ClearTracks();

    // Raw track data access
    const RawTrackData* GetRawTrackData(uint64_t track_id) const;
    void SetRawTrackData(uint64_t track_id, RawTrackData* data);
    void FreeRawTrackData(uint64_t track_id);
    void FreeAllRawTrackData();

    // Event data access
    const EventInfo* GetEvent(uint64_t event_id) const;
    void AddEvent(uint64_t event_id, EventInfo&& event);
    void RemoveEvent(uint64_t event_id);
    void ClearEvents();

    // Histogram access
    const std::vector<double>& GetHistogram() const { return m_histogram; }
    std::vector<double>& GetHistogram() { return m_histogram; }
    void SetHistogram(std::vector<double>&& histogram);
    void ResizeHistogram(size_t size);

    // Mini-map access
    const std::map<uint64_t, std::tuple<std::vector<double>, bool>>& GetMiniMap() const 
    { 
        return m_mini_map; 
    }
    std::map<uint64_t, std::tuple<std::vector<double>, bool>>& GetMiniMap() 
    { 
        return m_mini_map; 
    }
    void SetMiniMap(std::map<uint64_t, std::tuple<std::vector<double>, bool>>&& mini_map);

    // Clear all timeline data
    void Clear();

private:
    uint64_t m_num_tracks;
    double   m_min_ts;
    double   m_max_ts;

    std::unordered_map<uint64_t, TrackInfo>     m_track_metadata;
    std::unordered_map<uint64_t, RawTrackData*> m_raw_track_data;
    std::unordered_map<uint64_t, EventInfo>     m_event_data;

    std::vector<double>                                        m_histogram;
    std::map<uint64_t, std::tuple<std::vector<double>, bool>> m_mini_map;
};

}  // namespace Model
}  // namespace View
}  // namespace RocProfVis
