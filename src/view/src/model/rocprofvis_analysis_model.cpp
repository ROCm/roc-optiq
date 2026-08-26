// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_analysis_model.h"
#include "../rocprofvis_utils.h"
#include "rocprofvis_topology_model.h"
#include <array>
#include <tuple>

namespace RocProfVis
{
namespace View
{

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
        m_generation++;
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
            // Start kStale so the track is fetched when first visible, without needing a
            // SetAnalysisRange() invalidation (added-file tracks register after the range is
            // committed, so a kReady default would leave their pills blank).
            store->state = AnalysisTrackStatistics::kStale;
        }
    }
    else
    {
        // Refresh the cached track pointer: a graph-view rebuild recreates every TrackInfo, but
        // the stat store persists by id - without this the pointer would dangle (use-after-free).
        store        = &m_track_stats.at(track.id);
        store->track = &track;
    }
    return store;
}

void
AnalysisModel::SetQueueUtilization(uint64_t track_id, const double& util_pct)
{
    auto it = m_track_stats.find(track_id);
    if(it == m_track_stats.end())
    {
        return;  // track no longer registered (e.g. a late result after a rebuild)
    }
    AnalysisTrackStatistics& store = it->second;
    // Apply regardless of state: SetAnalysisRange can re-arm the stat kStale while a request
    // is in flight, and gating on kRequested here left pills permanently blank. Stale results
    // (from a superseded range) are filtered by the generation check before this point.
    if(store.track->topology.type == TrackInfo::TrackType::Queue)
    {
        store.stats[AnalysisTrackStatistics::Queue::kQueueUtilization].value =
            util_pct;
        ToString(store.track,
                 store.stats[AnalysisTrackStatistics::Queue::kQueueUtilization], "%");
        store.state = AnalysisTrackStatistics::State::kReady;
    }
}

void
AnalysisModel::SetCounterStatistics(uint64_t track_id,
                                    const rocprofvis_analysis_counter_statistics_t& stats)
{
    auto it = m_track_stats.find(track_id);
    if(it == m_track_stats.end())
    {
        return;  // track no longer registered (e.g. a late result after a rebuild)
    }
    AnalysisTrackStatistics& store = it->second;
    // Apply any successful result regardless of state (see SetQueueUtilization).
    if(store.track->topology.type == TrackInfo::TrackType::Counter)
    {
        store.stats[AnalysisTrackStatistics::Counter::kCounterMin].value =
            stats.min_value;
        store.stats[AnalysisTrackStatistics::Counter::kCounterMax].value =
            stats.max_value;
        store.stats[AnalysisTrackStatistics::Counter::kCounterMean].value =
            stats.mean_value;
        store.stats[AnalysisTrackStatistics::Counter::kCounterStandardDeviation].value =
            stats.std_dev;
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

void
AnalysisModel::SetTrackStatisticsEmpty(uint64_t track_id)
{
    auto it = m_track_stats.find(track_id);
    if(it == m_track_stats.end())
    {
        return;  // track no longer registered (e.g. a late result after a rebuild)
    }
    AnalysisTrackStatistics& store = it->second;
    // The analysis range does not overlap this track's data, so every statistic is definitively
    // zero for this range. Fill the values (so the pill shows a real number instead of staying
    // blank) and mark the stat ready so it stops being re-requested.
    if(store.track->topology.type == TrackInfo::TrackType::Queue)
    {
        AnalysisTrackStatistics::Stat& stat =
            store.stats[AnalysisTrackStatistics::Queue::kQueueUtilization];
        stat.value = 0.0;
        ToString(store.track, stat, "%");
    }
    else if(store.track->topology.type == TrackInfo::TrackType::Counter)
    {
        const CounterInfo* counter = m_topology.GetCounter(store.track->topology.id.value);
        const std::string  units   = counter ? counter->units : std::string();
        for(AnalysisTrackStatistics::Stat& stat : store.stats)
        {
            stat.value = 0.0;
            ToString(store.track, stat, units);
        }
    }
    store.state = AnalysisTrackStatistics::State::kReady;
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
    stat.suffix = units;
    stat.full   = full_number_format(stat.value);
    switch(track->topology.type)
    {
        case TrackInfo::Queue:
        {
            // Utilization percentages are already small; the compact form is
            // the same value as the full form.
            stat.compact = stat.full;
            break;
        }
        case TrackInfo::Counter:
        {
            stat.compact = compact_number_format(stat.value);
            break;
        }
    }
}

}  // namespace View
}  // namespace RocProfVis
