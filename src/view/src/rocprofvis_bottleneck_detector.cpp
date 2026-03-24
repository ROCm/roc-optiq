// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_bottleneck_detector.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <numeric>
#include <sstream>
#include <unordered_map>

namespace RocProfVis
{
namespace View
{

// ===========================================================================
// Construction
// ===========================================================================

BottleneckDetector::BottleneckDetector(Snapshot snapshot)
: m_snap(std::move(snapshot))
{}

// ===========================================================================
// CaptureSnapshot — main-thread-only, copies everything the detector needs
// ===========================================================================

BottleneckDetector::Snapshot
BottleneckDetector::CaptureSnapshot(const TimelineModel& model)
{
    Snapshot snap;
    snap.timeline_start = model.GetStartTime();
    snap.timeline_end   = model.GetEndTime();
    snap.num_buckets    = 0;

    const auto& mini_map   = model.GetMiniMap();
    auto        track_list = model.GetTrackList();

    for(const auto* ti : track_list)
    {
        auto it = mini_map.find(ti->id);
        if(it == mini_map.end()) continue;

        TrackSnapshot ts;
        ts.id         = ti->id;
        ts.track_type = ti->track_type;
        ts.category   = ti->category;
        ts.name       = ti->main_name + " / " + ti->sub_name;
        ts.min_value  = ti->min_value;
        ts.max_value  = ti->max_value;
        ts.buckets    = std::get<0>(it->second);
        ts.visible    = std::get<1>(it->second);

        if(snap.num_buckets == 0 && !ts.buckets.empty())
            snap.num_buckets = ts.buckets.size();

        snap.tracks.push_back(std::move(ts));
    }

    return snap;
}

// ===========================================================================
// Helpers
// ===========================================================================

double
BottleneckDetector::BinToTime(size_t bin) const
{
    if(m_snap.num_buckets == 0) return m_snap.timeline_start;
    double frac = static_cast<double>(bin) / m_snap.num_buckets;
    return m_snap.timeline_start + frac * (m_snap.timeline_end - m_snap.timeline_start);
}

double
BottleneckDetector::BinRangeToTime(size_t start_bin, size_t end_bin) const
{
    double bin_width =
        (m_snap.timeline_end - m_snap.timeline_start) / m_snap.num_buckets;
    return static_cast<double>(end_bin - start_bin) * bin_width;
}

std::string
BottleneckDetector::FormatTime(double ns) const
{
    if(ns >= 1e9)
    {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%.3f s", ns / 1e9);
        return buf;
    }
    if(ns >= 1e6)
    {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%.3f ms", ns / 1e6);
        return buf;
    }
    if(ns >= 1e3)
    {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%.3f us", ns / 1e3);
        return buf;
    }
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.0f ns", ns);
    return buf;
}

// ===========================================================================
// Analyze — runs every sub-detector, merges, sorts, deduplicates
// ===========================================================================

std::vector<Insight>
BottleneckDetector::Analyze() const
{
    if(m_snap.tracks.empty() || m_snap.num_buckets == 0) return {};

    std::vector<Insight> all;

    auto append = [&](std::vector<Insight>&& v) {
        all.insert(all.end(), std::make_move_iterator(v.begin()),
                   std::make_move_iterator(v.end()));
    };

    append(DetectHotspots());
    append(DetectGpuStarvation());
    append(DetectCpuBlocked());
    append(DetectSyncBarriers());
    append(DetectLoadImbalance());
    append(DetectCounterAnomalies());
    append(DetectSerialisation());

    // Sort by severity (critical first), then by descending score within
    // the same severity band so the most impactful insights surface first.
    std::sort(all.begin(), all.end(), [](const Insight& a, const Insight& b) {
        if(a.severity != b.severity)
            return static_cast<int>(a.severity) < static_cast<int>(b.severity);
        return a.score > b.score;
    });

    return all;
}

// ===========================================================================
// Tier 1 — Hotspot detection (per-track statistical outliers)
//
// For each visible event track, computes mean and standard deviation of the
// non-zero bucket values.  Any contiguous run of bins that exceeds
// mean + 2*sigma is flagged.  The z-score of the peak bin within the run
// determines severity:
//   >= 4 sigma  → Critical
//   >= 3 sigma  → Warning
//   >= 2 sigma  → Info
// ===========================================================================

std::vector<Insight>
BottleneckDetector::DetectHotspots() const
{
    std::vector<Insight> results;
    constexpr double     kSigmaThreshold = 2.0;

    for(const auto& trk : m_snap.tracks)
    {
        if(!trk.visible) continue;
        if(trk.track_type != kRPVControllerTrackTypeEvents) continue;

        // Collect non-zero values for stats
        double sum    = 0.0;
        double sum_sq = 0.0;
        size_t count  = 0;
        for(double v : trk.buckets)
        {
            if(v > 0.0)
            {
                sum += v;
                sum_sq += v * v;
                ++count;
            }
        }
        if(count < 3) continue;

        double mean   = sum / count;
        double var    = (sum_sq / count) - (mean * mean);
        double stddev = (var > 0.0) ? std::sqrt(var) : 0.0;
        if(stddev < 1e-12) continue;

        double threshold = mean + kSigmaThreshold * stddev;

        // Walk the bins and find contiguous runs above the threshold.
        size_t run_start = 0;
        bool   in_run    = false;
        double peak_val  = 0.0;

        auto flush_run = [&](size_t run_end) {
            double peak_z = (peak_val - mean) / stddev;

            InsightSeverity sev = InsightSeverity::kInfo;
            if(peak_z >= 4.0)
                sev = InsightSeverity::kCritical;
            else if(peak_z >= 3.0)
                sev = InsightSeverity::kWarning;

            Insight ins;
            ins.category   = InsightCategory::kHotspot;
            ins.severity   = sev;
            ins.time_start = BinToTime(run_start);
            ins.time_end   = BinToTime(run_end);
            ins.track_ids  = {trk.id};
            ins.score      = peak_z;
            ins.title      = "Hotspot";

            std::ostringstream desc;
            desc << "Track \"" << trk.name << "\" has unusually high density ("
                 << std::fixed;
            desc.precision(1);
            desc << peak_z << " sigma) from " << FormatTime(ins.time_start)
                 << " to " << FormatTime(ins.time_end) << ".";
            ins.description = desc.str();

            ins.suggestion = "Investigate the kernels or API calls in this "
                             "region — they dominate runtime.";
            results.push_back(std::move(ins));
        };

        for(size_t i = 0; i < trk.buckets.size(); ++i)
        {
            if(trk.buckets[i] >= threshold)
            {
                if(!in_run)
                {
                    run_start = i;
                    peak_val  = trk.buckets[i];
                    in_run    = true;
                }
                peak_val = std::max(peak_val, trk.buckets[i]);
            }
            else if(in_run)
            {
                flush_run(i);
                in_run = false;
            }
        }
        if(in_run) flush_run(trk.buckets.size());
    }

    return results;
}

// ===========================================================================
// Tier 2a — GPU Starvation
//
// Slides a window across the time axis.  For each window position, averages
// event density across all visible *event* tracks whose category suggests
// they are GPU work (queue, stream, GPU keywords) and all tracks that suggest
// CPU/API activity.  If CPU is active (above its mean) and GPU is near-idle
// (below 10% of GPU mean), the window is flagged.
//
// Adjacent flagged windows are merged into a single insight.
//
// Severity:
//   GPU idle fraction >= 0.90 → Critical
//   >= 0.70                   → Warning
//   otherwise                 → Info
// ===========================================================================

namespace
{

bool
IsGpuTrack(const BottleneckDetector::TrackSnapshot& t)
{
    if(t.track_type == kRPVControllerTrackTypeSamples) return false;
    const auto& c = t.category;
    return c.find("GPU") != std::string::npos || c.find("Queue") != std::string::npos ||
           c.find("Stream") != std::string::npos || c.find("HIP") != std::string::npos ||
           c.find("HSA") != std::string::npos || c.find("Copy") != std::string::npos;
}

bool
IsCpuTrack(const BottleneckDetector::TrackSnapshot& t)
{
    if(t.track_type == kRPVControllerTrackTypeSamples) return false;
    const auto& c = t.category;
    return c.find("CPU") != std::string::npos || c.find("Thread") != std::string::npos ||
           c.find("API") != std::string::npos || c.find("Marker") != std::string::npos;
}

}  // anonymous namespace

std::vector<Insight>
BottleneckDetector::DetectGpuStarvation() const
{
    std::vector<Insight> results;

    // Partition tracks
    std::vector<size_t> gpu_idx, cpu_idx;
    for(size_t i = 0; i < m_snap.tracks.size(); ++i)
    {
        if(!m_snap.tracks[i].visible) continue;
        if(IsGpuTrack(m_snap.tracks[i]))
            gpu_idx.push_back(i);
        else if(IsCpuTrack(m_snap.tracks[i]))
            cpu_idx.push_back(i);
    }

    if(gpu_idx.empty() || cpu_idx.empty()) return results;

    size_t n = m_snap.num_buckets;

    // Build per-bin aggregate density for each group
    std::vector<double> gpu_density(n, 0.0), cpu_density(n, 0.0);
    for(size_t b = 0; b < n; ++b)
    {
        for(size_t i : gpu_idx) gpu_density[b] += m_snap.tracks[i].buckets[b];
        for(size_t i : cpu_idx) cpu_density[b] += m_snap.tracks[i].buckets[b];
    }

    // Compute means of each group (non-zero only for a meaningful baseline)
    auto non_zero_mean = [](const std::vector<double>& v) -> double {
        double s = 0;
        size_t c = 0;
        for(double d : v)
            if(d > 0)
            {
                s += d;
                ++c;
            }
        return c > 0 ? s / c : 0.0;
    };

    double gpu_mean = non_zero_mean(gpu_density);
    double cpu_mean = non_zero_mean(cpu_density);
    if(gpu_mean < 1e-12 || cpu_mean < 1e-12) return results;

    double gpu_idle_thresh  = gpu_mean * 0.10;
    double cpu_active_thresh = cpu_mean * 0.50;

    // Slide and flag bins
    std::vector<bool> flagged(n, false);
    for(size_t b = 0; b < n; ++b)
        flagged[b] = (gpu_density[b] <= gpu_idle_thresh &&
                      cpu_density[b] >= cpu_active_thresh);

    // Merge contiguous flagged regions
    size_t run_start = 0;
    bool   in_run    = false;
    for(size_t b = 0; b <= n; ++b)
    {
        bool f = (b < n) ? flagged[b] : false;
        if(f && !in_run)
        {
            run_start = b;
            in_run    = true;
        }
        else if(!f && in_run)
        {
            size_t run_len = b - run_start;
            if(run_len >= 2)  // ignore single-bin blips
            {
                double idle_frac =
                    static_cast<double>(run_len) / static_cast<double>(n);

                InsightSeverity sev = InsightSeverity::kInfo;
                if(idle_frac >= 0.15)
                    sev = InsightSeverity::kCritical;
                else if(idle_frac >= 0.05)
                    sev = InsightSeverity::kWarning;

                Insight ins;
                ins.category   = InsightCategory::kGpuStarvation;
                ins.severity   = sev;
                ins.time_start = BinToTime(run_start);
                ins.time_end   = BinToTime(b);
                ins.score      = idle_frac;
                ins.title      = "GPU Starvation";

                for(size_t i : gpu_idx)
                    ins.track_ids.push_back(m_snap.tracks[i].id);

                std::ostringstream desc;
                desc << "GPU idle while CPU active for "
                     << FormatTime(BinRangeToTime(run_start, b))
                     << " (" << std::fixed;
                desc.precision(1);
                desc << (idle_frac * 100.0) << "% of trace).";
                ins.description = desc.str();

                ins.suggestion =
                    "Batch more work before submission, overlap "
                    "transfers with compute, or reduce CPU-side prep.";
                results.push_back(std::move(ins));
            }
            in_run = false;
        }
    }
    return results;
}

// ===========================================================================
// Tier 2b — CPU Blocked
//
// Mirror of GPU Starvation: GPU is busy but CPU tracks are idle,
// suggesting the host is blocked on a synchronization call.
// ===========================================================================

std::vector<Insight>
BottleneckDetector::DetectCpuBlocked() const
{
    std::vector<Insight> results;

    std::vector<size_t> gpu_idx, cpu_idx;
    for(size_t i = 0; i < m_snap.tracks.size(); ++i)
    {
        if(!m_snap.tracks[i].visible) continue;
        if(IsGpuTrack(m_snap.tracks[i]))
            gpu_idx.push_back(i);
        else if(IsCpuTrack(m_snap.tracks[i]))
            cpu_idx.push_back(i);
    }

    if(gpu_idx.empty() || cpu_idx.empty()) return results;

    size_t              n = m_snap.num_buckets;
    std::vector<double> gpu_density(n, 0.0), cpu_density(n, 0.0);
    for(size_t b = 0; b < n; ++b)
    {
        for(size_t i : gpu_idx) gpu_density[b] += m_snap.tracks[i].buckets[b];
        for(size_t i : cpu_idx) cpu_density[b] += m_snap.tracks[i].buckets[b];
    }

    auto nz_mean = [](const std::vector<double>& v) -> double {
        double s = 0;
        size_t c = 0;
        for(double d : v)
            if(d > 0)
            {
                s += d;
                ++c;
            }
        return c > 0 ? s / c : 0.0;
    };

    double gpu_mean = nz_mean(gpu_density);
    double cpu_mean = nz_mean(cpu_density);
    if(gpu_mean < 1e-12 || cpu_mean < 1e-12) return results;

    double gpu_active_thresh = gpu_mean * 0.50;
    double cpu_idle_thresh   = cpu_mean * 0.10;

    std::vector<bool> flagged(n, false);
    for(size_t b = 0; b < n; ++b)
        flagged[b] = (cpu_density[b] <= cpu_idle_thresh &&
                      gpu_density[b] >= gpu_active_thresh);

    size_t run_start = 0;
    bool   in_run    = false;
    for(size_t b = 0; b <= n; ++b)
    {
        bool f = (b < n) ? flagged[b] : false;
        if(f && !in_run)
        {
            run_start = b;
            in_run    = true;
        }
        else if(!f && in_run)
        {
            size_t run_len = b - run_start;
            if(run_len >= 2)
            {
                double frac =
                    static_cast<double>(run_len) / static_cast<double>(n);

                InsightSeverity sev = InsightSeverity::kInfo;
                if(frac >= 0.15)
                    sev = InsightSeverity::kCritical;
                else if(frac >= 0.05)
                    sev = InsightSeverity::kWarning;

                Insight ins;
                ins.category   = InsightCategory::kCpuBlocked;
                ins.severity   = sev;
                ins.time_start = BinToTime(run_start);
                ins.time_end   = BinToTime(b);
                ins.score      = frac;
                ins.title      = "CPU Blocked on GPU";

                for(size_t i : cpu_idx)
                    ins.track_ids.push_back(m_snap.tracks[i].id);

                std::ostringstream desc;
                desc << "CPU idle while GPU active for "
                     << FormatTime(BinRangeToTime(run_start, b))
                     << " (" << std::fixed;
                desc.precision(1);
                desc << (frac * 100.0) << "% of trace).";
                ins.description = desc.str();

                ins.suggestion =
                    "Use asynchronous GPU synchronization (events/fences) "
                    "or overlap CPU work during GPU execution.";
                results.push_back(std::move(ins));
            }
            in_run = false;
        }
    }
    return results;
}

// ===========================================================================
// Tier 3 — Sync Barriers
//
// Detects time regions where *all* visible event tracks simultaneously drop
// below 10% of their respective means.  This signals a global stall — every
// processing unit is waiting.
//
// Severity:
//   Barrier duration >= 5% of trace  → Critical
//   >= 2%                             → Warning
//   otherwise                         → Info
// ===========================================================================

std::vector<Insight>
BottleneckDetector::DetectSyncBarriers() const
{
    std::vector<Insight> results;

    // Collect visible event tracks
    struct EventTrack
    {
        size_t              idx;
        double              mean;
        std::vector<double> bins;
    };

    std::vector<EventTrack> evt_tracks;
    for(size_t i = 0; i < m_snap.tracks.size(); ++i)
    {
        if(!m_snap.tracks[i].visible) continue;
        if(m_snap.tracks[i].track_type != kRPVControllerTrackTypeEvents) continue;

        double s = 0;
        size_t c = 0;
        for(double v : m_snap.tracks[i].buckets)
            if(v > 0)
            {
                s += v;
                ++c;
            }
        if(c == 0) continue;

        EventTrack et;
        et.idx  = i;
        et.mean = s / c;
        et.bins = m_snap.tracks[i].buckets;
        evt_tracks.push_back(std::move(et));
    }

    if(evt_tracks.size() < 2) return results;

    size_t n = m_snap.num_buckets;
    std::vector<bool> all_idle(n, false);

    for(size_t b = 0; b < n; ++b)
    {
        bool idle = true;
        for(const auto& et : evt_tracks)
        {
            if(et.bins[b] > et.mean * 0.10)
            {
                idle = false;
                break;
            }
        }
        all_idle[b] = idle;
    }

    // Merge contiguous idle regions
    size_t run_start = 0;
    bool   in_run    = false;
    for(size_t b = 0; b <= n; ++b)
    {
        bool f = (b < n) ? all_idle[b] : false;
        if(f && !in_run)
        {
            run_start = b;
            in_run    = true;
        }
        else if(!f && in_run)
        {
            size_t run_len = b - run_start;
            if(run_len >= 2)
            {
                double frac = static_cast<double>(run_len) / static_cast<double>(n);

                InsightSeverity sev = InsightSeverity::kInfo;
                if(frac >= 0.05)
                    sev = InsightSeverity::kCritical;
                else if(frac >= 0.02)
                    sev = InsightSeverity::kWarning;

                Insight ins;
                ins.category   = InsightCategory::kSyncBarrier;
                ins.severity   = sev;
                ins.time_start = BinToTime(run_start);
                ins.time_end   = BinToTime(b);
                ins.score      = frac;
                ins.title      = "Synchronization Barrier";

                std::ostringstream desc;
                desc << "All " << evt_tracks.size()
                     << " event tracks are idle for "
                     << FormatTime(BinRangeToTime(run_start, b))
                     << " (" << std::fixed;
                desc.precision(1);
                desc << (frac * 100.0) << "% of trace).";
                ins.description = desc.str();

                ins.suggestion =
                    "Investigate barriers or fences at this point. "
                    "Consider reducing synchronization granularity.";
                results.push_back(std::move(ins));
            }
            in_run = false;
        }
    }

    return results;
}

// ===========================================================================
// Tier 4 — Load Imbalance
//
// Groups tracks by category and computes the coefficient of variation (CV)
// of total activity across tracks within each group.  A high CV means some
// tracks are doing far more work than others of the same kind.
//
// Severity:
//   CV >= 1.5  → Critical
//   CV >= 0.8  → Warning
//   CV >= 0.5  → Info (returned)
//   otherwise  → not reported
// ===========================================================================

std::vector<Insight>
BottleneckDetector::DetectLoadImbalance() const
{
    std::vector<Insight> results;

    // Group visible event tracks by category
    std::unordered_map<std::string, std::vector<size_t>> groups;
    for(size_t i = 0; i < m_snap.tracks.size(); ++i)
    {
        if(!m_snap.tracks[i].visible) continue;
        if(m_snap.tracks[i].track_type != kRPVControllerTrackTypeEvents) continue;
        groups[m_snap.tracks[i].category].push_back(i);
    }

    for(const auto& [cat, indices] : groups)
    {
        if(indices.size() < 2) continue;

        // Total activity per track
        std::vector<double> totals;
        for(size_t idx : indices)
        {
            double total = 0;
            for(double v : m_snap.tracks[idx].buckets) total += v;
            totals.push_back(total);
        }

        double mean =
            std::accumulate(totals.begin(), totals.end(), 0.0) / totals.size();
        if(mean < 1e-12) continue;

        double var = 0;
        for(double t : totals) var += (t - mean) * (t - mean);
        var /= totals.size();
        double cv = std::sqrt(var) / mean;

        if(cv < 0.5) continue;

        InsightSeverity sev = InsightSeverity::kInfo;
        if(cv >= 1.5)
            sev = InsightSeverity::kCritical;
        else if(cv >= 0.8)
            sev = InsightSeverity::kWarning;

        double max_total = *std::max_element(totals.begin(), totals.end());
        double min_total = *std::min_element(totals.begin(), totals.end());

        Insight ins;
        ins.category   = InsightCategory::kLoadImbalance;
        ins.severity   = sev;
        ins.time_start = m_snap.timeline_start;
        ins.time_end   = m_snap.timeline_end;
        ins.score      = cv;
        ins.title      = "Load Imbalance";

        for(size_t idx : indices) ins.track_ids.push_back(m_snap.tracks[idx].id);

        std::ostringstream desc;
        desc << "Category \"" << cat << "\" has " << indices.size()
             << " tracks with coefficient of variation " << std::fixed;
        desc.precision(2);
        desc << cv << ". Busiest track has "
             << std::fixed;
        desc.precision(1);
        desc << (max_total / (min_total > 0 ? min_total : 1.0))
             << "x more work than the least busy.";
        ins.description = desc.str();

        ins.suggestion = "Balance workloads across queues/streams/threads, "
                         "or consolidate underutilized tracks.";
        results.push_back(std::move(ins));
    }

    return results;
}

// ===========================================================================
// Tier 5 — Counter Anomalies
//
// For each visible sample/counter track, finds bins where the counter value
// is a statistical outlier (>2 sigma).  Then checks whether any nearby event
// tracks show a *drop* in density at the same time — a classic sign that a
// resource bottleneck (memory bandwidth, cache pressure) is limiting throughput.
//
// The score is the counter z-score.
// ===========================================================================

std::vector<Insight>
BottleneckDetector::DetectCounterAnomalies() const
{
    std::vector<Insight> results;

    // Pre-aggregate event density across all visible event tracks
    size_t              n = m_snap.num_buckets;
    std::vector<double> evt_density(n, 0.0);
    size_t              evt_count = 0;
    for(const auto& trk : m_snap.tracks)
    {
        if(!trk.visible || trk.track_type != kRPVControllerTrackTypeEvents)
            continue;
        for(size_t b = 0; b < n; ++b)
            evt_density[b] += trk.buckets[b];
        ++evt_count;
    }
    if(evt_count == 0) return results;

    // Event density stats
    double evt_sum = 0, evt_sum2 = 0;
    size_t evt_nz = 0;
    for(double v : evt_density)
        if(v > 0)
        {
            evt_sum += v;
            evt_sum2 += v * v;
            ++evt_nz;
        }
    double evt_mean   = evt_nz > 0 ? evt_sum / evt_nz : 0;
    double evt_var    = evt_nz > 0 ? (evt_sum2 / evt_nz) - (evt_mean * evt_mean) : 0;
    double evt_stddev = evt_var > 0 ? std::sqrt(evt_var) : 0;

    for(const auto& trk : m_snap.tracks)
    {
        if(!trk.visible) continue;
        if(trk.track_type != kRPVControllerTrackTypeSamples) continue;

        // Counter stats
        double sum = 0, sum2 = 0;
        size_t cnt = 0;
        for(double v : trk.buckets)
            if(v > 0)
            {
                sum += v;
                sum2 += v * v;
                ++cnt;
            }
        if(cnt < 3) continue;

        double mean   = sum / cnt;
        double var    = (sum2 / cnt) - (mean * mean);
        double stddev = var > 0 ? std::sqrt(var) : 0;
        if(stddev < 1e-12) continue;

        double threshold = mean + 2.0 * stddev;

        // Find anomalous bins where counter is high AND event density is low
        for(size_t b = 0; b < n; ++b)
        {
            if(trk.buckets[b] < threshold) continue;

            double counter_z = (trk.buckets[b] - mean) / stddev;
            bool   evt_drop  = (evt_stddev > 0 &&
                              evt_density[b] < evt_mean - evt_stddev);

            if(!evt_drop) continue;

            InsightSeverity sev = InsightSeverity::kInfo;
            if(counter_z >= 4.0)
                sev = InsightSeverity::kCritical;
            else if(counter_z >= 3.0)
                sev = InsightSeverity::kWarning;

            Insight ins;
            ins.category   = InsightCategory::kCounterAnomaly;
            ins.severity   = sev;
            ins.time_start = BinToTime(b);
            ins.time_end   = BinToTime(b + 1);
            ins.track_ids  = {trk.id};
            ins.score      = counter_z;
            ins.title      = "Counter Anomaly";

            std::ostringstream desc;
            desc << "Counter \"" << trk.name << "\" spiked to "
                 << std::fixed;
            desc.precision(1);
            desc << counter_z
                 << " sigma while event throughput dropped at "
                 << FormatTime(ins.time_start) << ".";
            ins.description = desc.str();

            ins.suggestion =
                "This counter spike correlates with reduced throughput. "
                "Check for memory bandwidth saturation or cache thrashing.";
            results.push_back(std::move(ins));
        }
    }

    return results;
}

// ===========================================================================
// Tier 6 — Serialisation detection
//
// For every pair of visible event tracks in the same category, computes
// the temporal overlap of their active (non-zero) regions.  If both tracks
// have significant activity but low overlap, they are likely being
// serialized — one waits while the other runs.
//
// Only the top few worst pairs are reported to avoid noise.
//
// Score is (1 - overlap_ratio), where 0 = perfect overlap, 1 = zero overlap.
// ===========================================================================

std::vector<Insight>
BottleneckDetector::DetectSerialisation() const
{
    std::vector<Insight> results;

    std::unordered_map<std::string, std::vector<size_t>> groups;
    for(size_t i = 0; i < m_snap.tracks.size(); ++i)
    {
        if(!m_snap.tracks[i].visible) continue;
        if(m_snap.tracks[i].track_type != kRPVControllerTrackTypeEvents) continue;
        groups[m_snap.tracks[i].category].push_back(i);
    }

    size_t n = m_snap.num_buckets;

    for(const auto& [cat, indices] : groups)
    {
        if(indices.size() < 2) continue;

        // Build active masks (above 10% of each track's own mean)
        struct MaskInfo
        {
            size_t            idx;
            std::vector<bool> active;
            size_t            active_count;
        };

        std::vector<MaskInfo> masks;
        for(size_t i : indices)
        {
            double s = 0;
            size_t c = 0;
            for(double v : m_snap.tracks[i].buckets)
                if(v > 0)
                {
                    s += v;
                    ++c;
                }
            if(c == 0) continue;
            double t = (s / c) * 0.10;

            MaskInfo mi;
            mi.idx          = i;
            mi.active_count = 0;
            mi.active.resize(n);
            for(size_t b = 0; b < n; ++b)
            {
                mi.active[b] = m_snap.tracks[i].buckets[b] > t;
                if(mi.active[b]) ++mi.active_count;
            }
            if(mi.active_count >= n * 0.05)  // at least 5% active
                masks.push_back(std::move(mi));
        }

        // Compare all pairs within the group
        for(size_t a = 0; a < masks.size(); ++a)
        {
            for(size_t b = a + 1; b < masks.size(); ++b)
            {
                size_t both_active = 0, either_active = 0;
                for(size_t i = 0; i < n; ++i)
                {
                    bool aa = masks[a].active[i];
                    bool bb = masks[b].active[i];
                    if(aa && bb) ++both_active;
                    if(aa || bb) ++either_active;
                }
                if(either_active == 0) continue;

                double overlap = static_cast<double>(both_active) / either_active;
                double serial_score = 1.0 - overlap;

                if(serial_score < 0.6) continue;

                InsightSeverity sev = InsightSeverity::kInfo;
                if(serial_score >= 0.90)
                    sev = InsightSeverity::kCritical;
                else if(serial_score >= 0.75)
                    sev = InsightSeverity::kWarning;

                Insight ins;
                ins.category   = InsightCategory::kSerialisation;
                ins.severity   = sev;
                ins.time_start = m_snap.timeline_start;
                ins.time_end   = m_snap.timeline_end;
                ins.track_ids  = {m_snap.tracks[masks[a].idx].id,
                                  m_snap.tracks[masks[b].idx].id};
                ins.score      = serial_score;
                ins.title      = "Serialized Execution";

                std::ostringstream desc;
                desc << "Tracks \"" << m_snap.tracks[masks[a].idx].name
                     << "\" and \"" << m_snap.tracks[masks[b].idx].name
                     << "\" overlap only " << std::fixed;
                desc.precision(1);
                desc << (overlap * 100.0)
                     << "% of the time despite both being active.";
                ins.description = desc.str();

                ins.suggestion =
                    "These tracks appear serialized. Submit work to both "
                    "concurrently or use async dispatches for parallelism.";
                results.push_back(std::move(ins));
            }
        }
    }

    // Keep only top 5 worst pairs to avoid noise
    std::sort(results.begin(), results.end(),
              [](const Insight& a, const Insight& b) { return a.score > b.score; });
    if(results.size() > 5) results.resize(5);

    return results;
}

}  // namespace View
}  // namespace RocProfVis
