// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_analysis_model.h"

namespace RocProfVis
{
namespace View
{

AnalysisModel::AnalysisModel()
: m_analysis_range_start_ns(0.0)
, m_analysis_range_end_ns(0.0)
{}
void
AnalysisModel::SetAnalysisRange(double start_ns, double end_ns)
{
    if(m_analysis_range_start_ns != start_ns || m_analysis_range_end_ns != end_ns)
    {
        m_analysis_range_start_ns = start_ns;
        m_analysis_range_end_ns   = end_ns;
        for(std::pair<const uint64_t, AnalysisQueueUtilization>& queue :
            m_per_track_queue_utilization)
        {
            queue.second.state = AnalysisQueueUtilization::kStale;
        }
    }
}

const AnalysisQueueUtilization*
AnalysisModel::GetPerTrackQueueUtilization(const TrackInfo& track)
{
    if(track.topology.type == TrackInfo::TrackType::Queue)
    {
        if(m_per_track_queue_utilization.count(track.id) == 0)
        {
            AnalysisQueueUtilization* queue = &m_per_track_queue_utilization[track.id];
            m_per_track_queue_utilization[track.id].track = &track;
            queue->util_pct                               = 0.0;
            queue->state = AnalysisQueueUtilization::kStale;
        }
        return &m_per_track_queue_utilization.at(track.id);
    }
    else
    {
        return nullptr;
    }
}

void
AnalysisModel::SetPerTrackQueueUtilizationValue(uint64_t track_id, double util_pct)
{
    AnalysisQueueUtilization& data = m_per_track_queue_utilization.at(track_id);
    if(data.state == AnalysisQueueUtilization::State::kRequested)
    {
        data.util_pct = util_pct;
        data.state    = AnalysisQueueUtilization::State::kReady;
    }
}

void
AnalysisModel::ClearPerTrackQueueUtilization()
{
    m_per_track_queue_utilization.clear();
}

void
AnalysisModel::Clear()
{
    ClearPerTrackQueueUtilization();
}

}  // namespace View
}  // namespace RocProfVis
