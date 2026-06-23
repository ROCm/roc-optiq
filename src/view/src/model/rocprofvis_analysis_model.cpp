// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_analysis_model.h"
#include "../rocprofvis_utils.h"
#include "rocprofvis_topology_model.h"
#include <array>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <tuple>

namespace RocProfVis
{
namespace View
{

constexpr int SIGNIFICANT_DIGITS = 1;
const double  ROUND_FACTOR       = std::pow(10.0, SIGNIFICANT_DIGITS);
// Name, compact name, accent color index
constexpr std::array<std::tuple<const char*, const char*, size_t>,
                     AnalysisTrackStatistics::Queue::kQueueCount>
    DISPLAY_PROPS_QUEUE = { { { "Utilization", "Util", 2 } } };
constexpr std::array<std::tuple<const char*, const char*, size_t>,
                     AnalysisTrackStatistics::Counter::kCounterCount>
    DISPLAY_PROPS_COUNTER = { { { "Minimum", "Min", 0 },
                                { "Maximum", "Max", 5 },
                                { "Mean", "Avg", 3 },
                                { "Standard Deviation", "Std Dev", 8 } } };

AnalysisModel::AnalysisModel(const TopologyDataModel& topology)
: m_analysis_range_start_ns(0.0)
, m_analysis_range_end_ns(0.0)
, m_topology(topology)
{}

void
AnalysisModel::SetAnalysisRange(double start_ns, double end_ns)
{
    if(m_analysis_range_start_ns != start_ns || m_analysis_range_end_ns != end_ns)
    {
        m_analysis_range_start_ns = start_ns;
        m_analysis_range_end_ns   = end_ns;
        for(std::pair<const uint64_t, AnalysisTrackStatistics>& stats : m_track_stats)
        {
            stats.second.state = AnalysisTrackStatistics::kStale;
        }
    }
}

const AnalysisTrackStatistics*
AnalysisModel::RegisterTrack(const TrackInfo& track)
{
    AnalysisTrackStatistics* store = nullptr;
    if(m_track_stats.count(track.id) == 0)
    {
        switch(track.topology.type)
        {
            case TrackInfo::TrackType::Queue:
            {
                store = &m_track_stats[track.id];
                store->stats.resize(AnalysisTrackStatistics::Queue::kQueueCount);
                store->stats[AnalysisTrackStatistics::Queue::kQueueUtilization].name =
                    std::get<0>(DISPLAY_PROPS_QUEUE
                                    [AnalysisTrackStatistics::Queue::kQueueUtilization]);
                store->stats[AnalysisTrackStatistics::Queue::kQueueUtilization]
                    .compact_name =
                    std::get<1>(DISPLAY_PROPS_QUEUE
                                    [AnalysisTrackStatistics::Queue::kQueueUtilization]);
                store->stats[AnalysisTrackStatistics::Queue::kQueueUtilization]
                    .accent_color =
                    std::get<2>(DISPLAY_PROPS_QUEUE
                                    [AnalysisTrackStatistics::Queue::kQueueUtilization]);
                break;
            }
            case TrackInfo::TrackType::Counter:
            {
                store = &m_track_stats[track.id];
                store->stats.resize(AnalysisTrackStatistics::Counter::kCounterCount);
                store->stats[AnalysisTrackStatistics::Counter::kCounterMin]
                    .name = std::get<0>(
                    DISPLAY_PROPS_COUNTER[AnalysisTrackStatistics::Counter::kCounterMin]);
                store->stats[AnalysisTrackStatistics::Counter::kCounterMin]
                    .compact_name = std::get<1>(
                    DISPLAY_PROPS_COUNTER[AnalysisTrackStatistics::Counter::kCounterMin]);
                store->stats[AnalysisTrackStatistics::Counter::kCounterMin]
                    .accent_color = std::get<2>(
                    DISPLAY_PROPS_COUNTER[AnalysisTrackStatistics::Counter::kCounterMin]);
                store->stats[AnalysisTrackStatistics::Counter::kCounterMax]
                    .name = std::get<0>(
                    DISPLAY_PROPS_COUNTER[AnalysisTrackStatistics::Counter::kCounterMax]);
                store->stats[AnalysisTrackStatistics::Counter::kCounterMax]
                    .compact_name = std::get<1>(
                    DISPLAY_PROPS_COUNTER[AnalysisTrackStatistics::Counter::kCounterMax]);
                store->stats[AnalysisTrackStatistics::Counter::kCounterMax]
                    .accent_color = std::get<2>(
                    DISPLAY_PROPS_COUNTER[AnalysisTrackStatistics::Counter::kCounterMax]);
                store->stats[AnalysisTrackStatistics::Counter::kCounterMean].name =
                    std::get<0>(DISPLAY_PROPS_COUNTER
                                    [AnalysisTrackStatistics::Counter::kCounterMean]);
                store->stats[AnalysisTrackStatistics::Counter::kCounterMean]
                    .compact_name =
                    std::get<1>(DISPLAY_PROPS_COUNTER
                                    [AnalysisTrackStatistics::Counter::kCounterMean]);
                store->stats[AnalysisTrackStatistics::Counter::kCounterMean]
                    .accent_color =
                    std::get<2>(DISPLAY_PROPS_COUNTER
                                    [AnalysisTrackStatistics::Counter::kCounterMean]);
                store->stats[AnalysisTrackStatistics::Counter::kCounterStandardDeviation]
                    .name = std::get<0>(
                    DISPLAY_PROPS_COUNTER
                        [AnalysisTrackStatistics::Counter::kCounterStandardDeviation]);
                store->stats[AnalysisTrackStatistics::Counter::kCounterStandardDeviation]
                    .compact_name = std::get<1>(
                    DISPLAY_PROPS_COUNTER
                        [AnalysisTrackStatistics::Counter::kCounterStandardDeviation]);
                store->stats[AnalysisTrackStatistics::Counter::kCounterStandardDeviation]
                    .accent_color = std::get<2>(
                    DISPLAY_PROPS_COUNTER
                        [AnalysisTrackStatistics::Counter::kCounterStandardDeviation]);
                break;
            }
        }
        if(store)
        {
            store->track = &track;
            store->state = AnalysisTrackStatistics::kReady;
        }
    }
    else
    {
        store = &m_track_stats.at(track.id);
    }
    return store;
}

void
AnalysisModel::SetQueueUtilization(uint64_t track_id, const double& util_pct)
{
    AnalysisTrackStatistics& store = m_track_stats.at(track_id);
    if(store.track->topology.type == TrackInfo::TrackType::Queue &&
       store.state == AnalysisTrackStatistics::State::kRequested)
    {
        store.stats[AnalysisTrackStatistics::Queue::kQueueUtilization].value =
            Round(util_pct);
        ToString(store.track,
                 store.stats[AnalysisTrackStatistics::Queue::kQueueUtilization], "%");
        store.state = AnalysisTrackStatistics::State::kReady;
    }
}

void
AnalysisModel::SetCounterStatistics(uint64_t track_id,
                                    const rocprofvis_analysis_counter_statistics_t& stats)
{
    AnalysisTrackStatistics& store = m_track_stats.at(track_id);
    if(store.track->topology.type == TrackInfo::TrackType::Counter &&
       store.state == AnalysisTrackStatistics::State::kRequested)
    {
        store.stats[AnalysisTrackStatistics::Counter::kCounterMin].value =
            Round(stats.min_value);
        store.stats[AnalysisTrackStatistics::Counter::kCounterMax].value =
            Round(stats.max_value);
        store.stats[AnalysisTrackStatistics::Counter::kCounterMean].value =
            Round(stats.mean_value);
        store.stats[AnalysisTrackStatistics::Counter::kCounterStandardDeviation].value =
            Round(stats.std_dev);
        const CounterInfo* counter =
            m_topology.GetCounter(store.track->topology.id.value);
        if(counter)
        {
            ToString(store.track,
                     store.stats[AnalysisTrackStatistics::Counter::kCounterMin],
                     counter->units);
            ToString(store.track,
                     store.stats[AnalysisTrackStatistics::Counter::kCounterMax],
                     counter->units);
            ToString(store.track,
                     store.stats[AnalysisTrackStatistics::Counter::kCounterMean],
                     counter->units);
            ToString(
                store.track,
                store.stats[AnalysisTrackStatistics::Counter::kCounterStandardDeviation],
                counter->units);
        }
        store.state = AnalysisTrackStatistics::State::kReady;
    }
}

const std::unordered_map<uint64_t, AnalysisTrackStatistics>&
AnalysisModel::GetTrackStatistics() const
{
    return m_track_stats;
}

const TablesModel&
AnalysisModel::GetTables() const
{
    return m_tables;
}

TablesModel&
AnalysisModel::GetTables()
{
    return m_tables;
}

void
AnalysisModel::Clear()
{
    m_tables.ClearAllTables();
    m_track_stats.clear();
}

void
AnalysisModel::ToString(const TrackInfo* track, AnalysisTrackStatistics::Stat& stat,
                        const std::string& units)
{
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(SIGNIFICANT_DIGITS) << stat.value;
    switch(track->topology.type)
    {
        case TrackInfo::Queue:
        {
            stat.compact  = oss.str() + (units.empty() ? "" : " " + units);
            stat.extended = std::string(stat.compact_name) + ": " + stat.compact;
            stat.full     = std::string(stat.name) + ": " + oss.str() +
                            (units.empty() ? "" : " " + units);
            break;
        }
        case TrackInfo::Counter:
        {
            stat.compact =
                compact_number_format(stat.value) + (units.empty() ? "" : " " + units);
            stat.extended = std::string(stat.compact_name) + ": " + stat.compact;
            stat.full     = std::string(stat.name) + ": " + oss.str() +
                            (units.empty() ? "" : " " + units);
            break;
        }
    }
}

double
AnalysisModel::Round(double d) const
{
    return std::round(d * ROUND_FACTOR) / ROUND_FACTOR;
}

}  // namespace View
}  // namespace RocProfVis
