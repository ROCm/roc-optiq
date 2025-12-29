// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_timeline_model.h"

namespace RocProfVis
{
namespace View
{
namespace Model
{

TimelineModel::TimelineModel()
    : m_num_tracks(0)
    , m_min_ts(0.0)
    , m_max_ts(0.0)
{
}

TimelineModel::~TimelineModel()
{
    FreeAllRawTrackData();
}

void TimelineModel::SetTimeRange(double min_ts, double max_ts)
{
    m_min_ts = min_ts;
    m_max_ts = max_ts;
}

// Track metadata access
const TrackInfo* TimelineModel::GetTrack(uint64_t track_id) const
{
    auto it = m_track_metadata.find(track_id);
    return (it != m_track_metadata.end()) ? &it->second : nullptr;
}

std::vector<const TrackInfo*> TimelineModel::GetTrackList() const
{
    std::vector<const TrackInfo*> result(m_track_metadata.size(), nullptr);
    for (const auto& pair : m_track_metadata)
    {
        if (pair.second.index < result.size())
        {
            result[pair.second.index] = &pair.second;
        }
    }
    return result;
}

void TimelineModel::AddTrack(uint64_t track_id, TrackInfo&& track)
{
    m_track_metadata[track_id] = std::move(track);
}

void TimelineModel::ClearTracks()
{
    m_track_metadata.clear();
}

// Raw track data access
const RawTrackData* TimelineModel::GetRawTrackData(uint64_t track_id) const
{
    auto it = m_raw_track_data.find(track_id);
    return (it != m_raw_track_data.end()) ? it->second : nullptr;
}

void TimelineModel::SetRawTrackData(uint64_t track_id, RawTrackData* data)
{
    m_raw_track_data[track_id] = data;
}

void TimelineModel::FreeRawTrackData(uint64_t track_id)
{
    auto it = m_raw_track_data.find(track_id);
    if (it != m_raw_track_data.end())
    {
        delete it->second;
        m_raw_track_data.erase(it);
    }
}

void TimelineModel::FreeAllRawTrackData()
{
    for (auto& pair : m_raw_track_data)
    {
        delete pair.second;
    }
    m_raw_track_data.clear();
}

// Event data access
const EventInfo* TimelineModel::GetEvent(uint64_t event_id) const
{
    auto it = m_event_data.find(event_id);
    return (it != m_event_data.end()) ? &it->second : nullptr;
}

void TimelineModel::AddEvent(uint64_t event_id, EventInfo&& event)
{
    m_event_data[event_id] = std::move(event);
}

void TimelineModel::RemoveEvent(uint64_t event_id)
{
    m_event_data.erase(event_id);
}

void TimelineModel::ClearEvents()
{
    m_event_data.clear();
}

// Histogram access
void TimelineModel::SetHistogram(std::vector<double>&& histogram)
{
    m_histogram = std::move(histogram);
}

void TimelineModel::ResizeHistogram(size_t size)
{
    m_histogram.resize(size, 0.0);
}

// Mini-map access
void TimelineModel::SetMiniMap(std::map<uint64_t, std::tuple<std::vector<double>, bool>>&& mini_map)
{
    m_mini_map = std::move(mini_map);
}

// Clear all
void TimelineModel::Clear()
{
    m_num_tracks = 0;
    m_min_ts = 0.0;
    m_max_ts = 0.0;
    ClearTracks();
    FreeAllRawTrackData();
    ClearEvents();
    m_histogram.clear();
    m_mini_map.clear();
}

}  // namespace Model
}  // namespace View
}  // namespace RocProfVis
