// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_timeline_model.h"
#include "spdlog/spdlog.h"

#include <algorithm>

namespace RocProfVis
{
namespace View
{

TimelineModel::TimelineModel()
: m_num_tracks(0)
, m_min_ts(0.0)
, m_max_ts(0.0)
, m_histogram_max_value_global(DBL_MIN)
, m_minimap_global_min(DBL_MAX)
, m_minimap_global_max(-DBL_MAX)
, m_normalize_global(true)
{}

TimelineModel::~TimelineModel() { FreeAllTrackData(); }

void
TimelineModel::SetTimeRange(double min_ts, double max_ts)
{
    m_min_ts = min_ts;
    m_max_ts = max_ts;
}

// Track metadata access
const TrackInfo*
TimelineModel::GetTrack(uint64_t track_id) const
{
    auto it = m_track_metadata.find(track_id);
    return (it != m_track_metadata.end()) ? &it->second : nullptr;
}

std::vector<const TrackInfo*>
TimelineModel::GetTrackList() const
{
    std::vector<const TrackInfo*> result(m_track_metadata.size(), nullptr);
    for(const auto& pair : m_track_metadata)
    {
        if(pair.second.index < result.size())
        {
            result[pair.second.index] = &pair.second;
        }
    }
    return result;
}

void
TimelineModel::AddTrackMetaData(uint64_t track_id, TrackInfo&& track)
{
    m_track_metadata[track_id] = std::move(track);
}

void
TimelineModel::ClearTrackMetaData()
{
    m_track_metadata.clear();
}

// Raw track data access
const RawTrackData*
TimelineModel::GetTrackData(uint64_t track_id) const
{
    auto it = m_raw_track_data.find(track_id);
    return (it != m_raw_track_data.end()) ? it->second : nullptr;
}

void
TimelineModel::SetTrackData(uint64_t track_id, RawTrackData* data)
{
    m_raw_track_data[track_id] = data;
}

bool
TimelineModel::FreeTrackData(uint64_t track_id, bool force /* = false */)
{
    auto it = m_raw_track_data.find(track_id);
    if(it != m_raw_track_data.end())
    {
        const RawTrackData* track = it->second;
        if(track)
        {
            if(!track->AllDataReady() && !force)
            {
                spdlog::debug(
                    "Cannot delete track data, not all data is ready for id: {}",
                    track_id);
                return false;
            }

            delete it->second;
            m_raw_track_data.erase(it);
            return true;
        }
    }

    spdlog::debug("Cannot delete track data, invalid id: {}", track_id);
    return false;
}

void
TimelineModel::FreeAllTrackData()
{
    for(auto& pair : m_raw_track_data)
    {
        delete pair.second;
    }
    m_raw_track_data.clear();
}

// Histogram access
void
TimelineModel::SetHistogram(std::vector<double>&& histogram)
{
    m_histogram = std::move(histogram);
}

void
TimelineModel::ResizeHistogram(size_t size)
{
    m_histogram.resize(size, 0.0);
}

// Mini-map access
void
TimelineModel::SetMiniMap(
    std::map<uint64_t, std::tuple<std::vector<double>, bool>>&& mini_map)
{
    m_mini_map = std::move(mini_map);
}

void
TimelineModel::UpdateHistogram(const std::vector<uint64_t>& interest_id, bool add)
{
    /*
    This function updates histogram and mini_map based on the interest_id list (which
    is a list of track IDs to add or remove from the histogram).
    */

    // Update visibility flags in mini_map
    for(const auto& id : interest_id)
    {
        auto it = m_mini_map.find(id);
        if(it != m_mini_map.end())
        {
            const std::vector<double>& mini_data   = std::get<0>(it->second);
            bool                       is_included = std::get<1>(it->second);
            if(add && !is_included)
            {
                it->second = std::make_tuple(mini_data, true);
            }
            else if(!add && is_included)
            {
                it->second = std::make_tuple(mini_data, false);
            }
        }
    }

    // Recompute histogram from all visible tracks
    if(!m_histogram.empty())
    {
        std::fill(m_histogram.begin(), m_histogram.end(), 0.0);
        for(const auto& kv : m_mini_map)
        {
            bool is_included = std::get<1>(kv.second);
            if(is_included)
            {
                // Retrieve track metadata to check its type
                const TrackInfo* track_info = GetTrack(kv.first);

                // If it's a counter/sample track, skip adding it to the global histogram
                if(track_info && track_info->track_type == kRPVControllerTrackTypeSamples)
                {
                    continue;
                }

                const std::vector<double>& mini_data = std::get<0>(kv.second);
                for(size_t i = 0; i < mini_data.size() && i < m_histogram.size(); ++i)
                {
                    m_histogram[i] += mini_data[i];
                }
            }
        }

        if(m_histogram_max_value_global == DBL_MIN)
        {
            /*This block only exists to get the global maximum value for
             * normalization*/
            std::vector<double> global_sum_histogram(m_histogram.size(), 0.0);

            m_minimap_global_min = DBL_MAX;
            m_minimap_global_max = -DBL_MAX;

            for(const auto& kv : m_mini_map)
            {
                // Retrieve track metadata to check its type
                const TrackInfo* track_info = GetTrack(kv.first);

                // If it's a counter/sample track, skip adding it to the global histogram
                if(track_info && track_info->track_type == kRPVControllerTrackTypeSamples)
                {
                    continue;
                }

                const std::vector<double>& mini_data = std::get<0>(kv.second);

                for(double val : mini_data)
                {
                    if(val != 0)  // Skip zeros to match local min/max calculation
                    {
                        if(val < m_minimap_global_min) m_minimap_global_min = val;
                        if(val > m_minimap_global_max) m_minimap_global_max = val;
                    }
                }

                for(size_t i = 0; i < mini_data.size() && i < global_sum_histogram.size(); ++i)
                {
                    global_sum_histogram[i] += mini_data[i];
                }
            }
            if(!global_sum_histogram.empty())
            {
                m_histogram_max_value_global =
                    *std::max_element(global_sum_histogram.begin(),
                                      global_sum_histogram.end());
            }
            if(m_minimap_global_min == DBL_MAX) m_minimap_global_min = 0.0;
            if(m_minimap_global_max == -DBL_MAX) m_minimap_global_max = 0.0;
        }

        // Normalize histogram to [0, 1]
        NormalizeHistogram();
    }
}

void
TimelineModel::NormalizeHistogram()
{
    // Normalize histogram to [0, 1]
    double max_value = *std::max_element(m_histogram.begin(), m_histogram.end());
    if(max_value > 0.0)
    {
        double norm_factor = m_normalize_global ? m_histogram_max_value_global : max_value;
        if(norm_factor > 0.0)
        {
            for(auto& val : m_histogram)
            {
                val /= norm_factor;
            }
        }
    }
}

// Clear all
void
TimelineModel::Clear()
{
    m_num_tracks = 0;
    m_min_ts     = 0.0;
    m_max_ts     = 0.0;
    ClearTrackMetaData();
    FreeAllTrackData();
    m_histogram.clear();
    m_mini_map.clear();
    m_histogram_max_value_global = DBL_MIN;
    m_minimap_global_min         = DBL_MAX;
    m_minimap_global_max         = -DBL_MAX;
}

bool
TimelineModel::DumpTrack(uint64_t track_id) const
{
    auto it = m_raw_track_data.find(track_id);
    if(it != m_raw_track_data.end())
    {
        if(it->second)
        {
            bool result = false;
            switch(it->second->GetType())
            {
                case kRPVControllerTrackTypeSamples:
                {
                    RawTrackSampleData* track =
                        dynamic_cast<RawTrackSampleData*>(it->second);
                    if(track)
                    {
                        const std::vector<TraceCounter>& buffer = track->GetData();
                        int64_t                          i      = 0;
                        for(const auto item : buffer)
                        {
                            spdlog::debug("{}, start_ts {}, value {}", i, item.m_start_ts,
                                          item.m_value);
                            ++i;
                        }
                        result = true;
                    }
                    else
                    {
                        spdlog::debug("Cannot dump track data, Type Error");
                    }
                    break;
                }
                case kRPVControllerTrackTypeEvents:
                {
                    RawTrackEventData* track =
                        dynamic_cast<RawTrackEventData*>(it->second);
                    if(track)
                    {
                        const std::vector<TraceEvent>& buffer = track->GetData();
                        int64_t                        i      = 0;
                        for(const auto item : buffer)
                        {
                            spdlog::debug(
                                "{}, name: {}, start_ts {}, duration {}, end_ts {}", i,
                                item.m_name, item.m_start_ts, item.m_duration,
                                item.m_duration + item.m_start_ts);
                            ++i;
                        }
                        result = true;
                    }
                    else
                    {
                        spdlog::debug("Cannot dump track data, Type Error");
                    }
                    break;
                }
                default:
                {
                    spdlog::debug("Cannot dump track data, unknown type");
                    break;
                }
            }
            return result;
        }
        else
        {
            spdlog::debug("No track data with id: {}", track_id);
        }
    }
    else
    {
        spdlog::debug("Cannot show track data, invalid id: {}", track_id);
    }

    return false;
}

void
TimelineModel::DumpMetaData() const
{
    for(const TrackInfo* track_info : GetTrackList())
    {
        spdlog::debug("Track index {}, id {}, name {}, min ts {}, max ts {}, type {}, "
                      "num entries {}, min value {}, max value {}",
                      track_info->index, track_info->id, track_info->name,
                      track_info->min_ts, track_info->max_ts,
                      track_info->track_type == kRPVControllerTrackTypeSamples ? "Samples"
                                                                               : "Events",
                      track_info->num_entries, track_info->min_value,
                      track_info->max_value);
    }
}

}  // namespace View
}  // namespace RocProfVis
