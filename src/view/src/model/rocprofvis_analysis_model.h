// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once
#include "rocprofvis_model_types.h"
#include <cstdint>
#include <unordered_map>

namespace RocProfVis
{
namespace View
{

class AnalysisModel
{
public:
    AnalysisModel();
    ~AnalysisModel() = default;

    void SetAnalysisRange(double start_ns, double end_ns);

    const AnalysisQueueUtilization* GetPerTrackQueueUtilization(const TrackInfo& track);
    void SetPerTrackQueueUtilizationValue(uint64_t track_id, double util_pct);

    void ClearPerTrackQueueUtilization();
    void Clear();

private:
    double                                                 m_analysis_range_start_ns;
    double                                                 m_analysis_range_end_ns;
    std::unordered_map<uint64_t, AnalysisQueueUtilization> m_per_track_queue_utilization;
};

}  // namespace View
}  // namespace RocProfVis
