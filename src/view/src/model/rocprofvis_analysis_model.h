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

    // Monotonic counter bumped whenever the analysis range changes. Requests are stamped with
    // the generation at issue time; a result whose generation no longer matches is stale (its
    // range was superseded) and must be discarded, otherwise a late result could pin a pill
    // to an old range's values.
    uint64_t GetGeneration() const { return m_generation; }

    const AnalysisTrackStatistics* RegisterTrack(const TrackInfo& track);

    void SetQueueUtilization(uint64_t track_id, const double& util_pct);
    void SetCounterStatistics(uint64_t                                        track_id,
                              const rocprofvis_analysis_counter_statistics_t& stats);

    // Resolve a track's statistics to zero and mark them ready, for the case where the analysis
    // range does not intersect the track's data window (so there is nothing to fetch). Without
    // this the stat would loop kPending and its pill would stay greyed indefinitely.
    void SetTrackStatisticsEmpty(uint64_t track_id);

    const TablesModel& GetTables() const;
    TablesModel&       GetTables();

    void Clear();

private:
    void ToString(const TrackInfo* track, AnalysisTrackStatistics::Stat& stat,
                  const std::string& units);

    double   m_analysis_range_start_ns;
    double   m_analysis_range_end_ns;
    uint64_t m_generation = 0;

    TablesModel                                           m_tables;
    std::unordered_map<uint64_t, AnalysisTrackStatistics> m_track_stats;
    const TopologyDataModel&                              m_topology;
};

}  // namespace View
}  // namespace RocProfVis
