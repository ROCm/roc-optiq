// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once
#include "rocprofvis_controller_analysis.h"
#include "rocprofvis_model_types.h"
#include "rocprofvis_tables_model.h"
#include <cstdint>
#include <unordered_map>

namespace RocProfVis
{
namespace View
{

class TopologyDataModel;

class AnalysisModel
{
public:
    AnalysisModel(const TopologyDataModel& topology);
    ~AnalysisModel() = default;

    void SetAnalysisRange(double start_ns, double end_ns);

    const AnalysisTrackStatistics* RegisterTrack(const TrackInfo& track);

    void SetQueueUtilization(uint64_t track_id, const double& util_pct);
    void SetCounterStatistics(uint64_t                                        track_id,
                              const rocprofvis_analysis_counter_statistics_t& stats);

    const TablesModel& GetTables() const;
    TablesModel&       GetTables();

    void Clear();

private:
    void   ToString(const TrackInfo* track, AnalysisTrackStatistics::Stat& stat,
                    const std::string& units);
    double Round(double d) const;

    double m_analysis_range_start_ns;
    double m_analysis_range_end_ns;

    TablesModel                                           m_tables;
    std::unordered_map<uint64_t, AnalysisTrackStatistics> m_track_stats;
    const TopologyDataModel&                              m_topology;
};

}  // namespace View
}  // namespace RocProfVis
