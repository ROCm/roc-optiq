// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "model/rocprofvis_timeline_model.h"
#include "rocprofvis_controller_enums.h"
#include <cstdint>
#include <map>
#include <string>
#include <tuple>
#include <vector>

namespace RocProfVis
{
namespace View
{

// ---------------------------------------------------------------------------
// Insight severity — ordered from most to least urgent so callers can sort
// directly on the enum value.
// ---------------------------------------------------------------------------
enum class InsightSeverity
{
    kCritical = 0,
    kWarning  = 1,
    kInfo     = 2
};

// ---------------------------------------------------------------------------
// Category that describes *what kind* of bottleneck was detected.
//
// kHotspot          — A time region whose event density is a statistical
//                     outlier (>2 sigma above the per-track mean).
//
// kGpuStarvation    — GPU tracks are idle while CPU/API tracks are active,
//                     meaning the GPU is waiting for work.
//
// kCpuBlocked       — CPU tracks are idle while GPU tracks are active,
//                     suggesting the host is stalled on a GPU fence/sync.
//
// kSyncBarrier      — ALL tracks go near-idle at the same time, indicating a
//                     global synchronization point (barrier, fence, etc.).
//
// kLoadImbalance    — Activity is unevenly spread across tracks of the same
//                     type: some are heavily utilized while others are starved.
//
// kCounterAnomaly   — A counter/sample track spikes while event-track
//                     throughput drops in the same time window.
//
// kSerialisation    — Tracks that could run in parallel show an alternating
//                     on/off pattern, indicating serialized dispatches.
// ---------------------------------------------------------------------------
enum class InsightCategory
{
    kHotspot,
    kGpuStarvation,
    kCpuBlocked,
    kSyncBarrier,
    kLoadImbalance,
    kCounterAnomaly,
    kSerialisation
};

// ---------------------------------------------------------------------------
// A single detected insight.  Fully self-contained — the view layer needs no
// back-reference to the model to display or navigate to this result.
// ---------------------------------------------------------------------------
struct Insight
{
    InsightCategory category;
    InsightSeverity severity;

    // Where in the timeline the issue lives (nanosecond timestamps).
    double time_start;
    double time_end;

    // Which tracks are involved (may be empty for global insights).
    std::vector<uint64_t> track_ids;

    // Quantified impact.  Interpretation depends on category:
    //   Hotspot        — z-score of the peak bin.
    //   GpuStarvation  — fraction of the window the GPU was idle (0-1).
    //   CpuBlocked     — fraction of the window the CPU was idle (0-1).
    //   SyncBarrier    — duration of the barrier relative to total time.
    //   LoadImbalance  — coefficient of variation across tracks.
    //   CounterAnomaly — counter z-score.
    //   Serialisation  — 1 minus the overlap ratio of the two tracks.
    double score;

    // Human-readable title, e.g. "GPU Starvation".
    std::string title;

    // Human-readable description with numbers, e.g.
    //   "GPU idle for 78% of the 1.2 ms window while CPU was active."
    std::string description;

    // Actionable suggestion, e.g.
    //   "Consider batching smaller dispatches or reducing CPU-side prep."
    std::string suggestion;
};

// ---------------------------------------------------------------------------
// BottleneckDetector
//
// Pure analysis engine with no UI dependencies.  Operates entirely on a
// *snapshot* of the timeline model data so it can run on a background thread
// without touching any ImGui state.
//
// Usage:
//   1. Construct with a snapshot of the minimap data, track metadata, and
//      timeline bounds (all captured on the main thread).
//   2. Call Analyze() from any thread — it is fully thread-safe because it
//      works only on its own copy of the data.
//   3. The returned vector is sorted by (severity ASC, score DESC).
//
// The caller (TraceView) is responsible for launching this via std::async
// and polling the resulting std::future each frame.
// ---------------------------------------------------------------------------
class BottleneckDetector
{
public:
    // -----------------------------------------------------------------------
    // Snapshot of one track's minimap row plus its metadata.
    // Captured on the main thread before launching the async analysis.
    // -----------------------------------------------------------------------
    struct TrackSnapshot
    {
        uint64_t                           id;
        rocprofvis_controller_track_type_t track_type;
        std::string                        category;
        std::string                        name;
        double                             min_value;
        double                             max_value;
        std::vector<double>                buckets;
        bool                               visible;
    };

    // -----------------------------------------------------------------------
    // All data the detector needs, captured at the moment the user clicks
    // "Analyze".  This struct is move-constructed into the async lambda.
    // -----------------------------------------------------------------------
    struct Snapshot
    {
        std::vector<TrackSnapshot> tracks;
        double                     timeline_start;
        double                     timeline_end;
        size_t                     num_buckets;
    };

    explicit BottleneckDetector(Snapshot snapshot);
    ~BottleneckDetector() = default;

    BottleneckDetector(const BottleneckDetector&)            = delete;
    BottleneckDetector& operator=(const BottleneckDetector&) = delete;
    BottleneckDetector(BottleneckDetector&&)                 = default;
    BottleneckDetector& operator=(BottleneckDetector&&)      = default;

    // Run the full analysis pipeline. Safe to call from any thread.
    std::vector<Insight> Analyze() const;

    // Build a Snapshot from live model data. Must be called on the main thread.
    static Snapshot CaptureSnapshot(const TimelineModel& model);

private:
    // Individual analysis passes.  Each returns its own findings;
    // Analyze() merges and sorts the combined set.
    std::vector<Insight> DetectHotspots() const;
    std::vector<Insight> DetectGpuStarvation() const;
    std::vector<Insight> DetectCpuBlocked() const;
    std::vector<Insight> DetectSyncBarriers() const;
    std::vector<Insight> DetectLoadImbalance() const;
    std::vector<Insight> DetectCounterAnomalies() const;
    std::vector<Insight> DetectSerialisation() const;

    // Helpers
    double BinToTime(size_t bin) const;
    double BinRangeToTime(size_t start_bin, size_t end_bin) const;
    std::string FormatTime(double ns) const;

    Snapshot m_snap;
};

}  // namespace View
}  // namespace RocProfVis
