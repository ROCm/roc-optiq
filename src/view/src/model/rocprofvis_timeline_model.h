// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_event_model.h"
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

/**
 * @brief Manages timeline visualization data.
 *
 * This model holds track metadata, track data, histograms, and mini-maps
 * used for rendering the trace timeline visualization.
 */
class TimelineModel
{
public:
    TimelineModel();
    ~TimelineModel();

    // Timeline bounds
    double GetStartTime() const { return m_min_ts; }
    double GetEndTime() const { return m_max_ts; }
    void   SetTimeRange(double min_ts, double max_ts);

    // Track count
    uint64_t GetTrackCount() const { return m_num_tracks; }
    void     SetTrackCount(uint64_t count) { m_num_tracks = count; }

    // Track metadata access
    const TrackInfo*              GetTrack(uint64_t track_id) const;
    std::vector<const TrackInfo*> GetTrackList() const;
    void                          AddTrackMetaData(uint64_t track_id, TrackInfo&& track);
    void                          ClearTrackMetaData();

    // Raw track data access
    /*
     * Get access to the raw track data.
     * The returned pointer should only be considered valid until
     * the next call to:
     * - DataProvider::Update()
     * - DataProvider::FetchTrace()
     * - DataProvider::CloseController()
     * - any of the memory freeing functions (FreeTrackData, FreeAllTrackData)
     * @param id: The id of the track.
     * @return: Pointer to the raw track data
     * or null if data for this track is not loaded.
     */
    const RawTrackData* GetTrackData(uint64_t track_id) const;
    void                SetTrackData(uint64_t track_id, RawTrackData* data);
    bool                FreeTrackData(uint64_t track_id, bool force = false);
    void                FreeAllTrackData();

    // Histogram access
    const std::vector<double>& GetHistogram() const { return m_histogram; }
    std::vector<double>&       GetHistogram() { return m_histogram; }
    void                       SetHistogram(std::vector<double>&& histogram);
    void                       ResizeHistogram(size_t size);

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

    // Histogram updates
    void UpdateHistogram(const std::vector<uint64_t>& interest_id, bool add);
    void NormalizeHistogram();

    // Metadata modification
    std::unordered_map<uint64_t, TrackInfo>& GetMutableTrackMetadata()
    {
        return m_track_metadata;
    }

    // Clear all timeline data
    void Clear();

    // Ouptput functions for debugging
    bool DumpTrack(uint64_t track_id) const;
    void DumpMetaData() const;

private:
    uint64_t m_num_tracks;
    double   m_min_ts;
    double   m_max_ts;

    std::unordered_map<uint64_t, TrackInfo>     m_track_metadata;
    std::unordered_map<uint64_t, RawTrackData*> m_raw_track_data;

    std::vector<double>                                       m_histogram;
    std::map<uint64_t, std::tuple<std::vector<double>, bool>> m_mini_map;
};

}  // namespace View
}  // namespace RocProfVis
