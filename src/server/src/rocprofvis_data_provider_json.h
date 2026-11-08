// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "json.h"
#include "model/rocprofvis_tables_model.h"

#include <cstdint>
#include <string>

namespace RocProfVis
{
namespace View
{

class ComputeDataModel;
class ComputeKernelSelectionTable;
class DataProvider;
class EventModel;
class TimelineModel;
class TopologyTree;
class TraceDataModel;
struct AnalysisTrackStatistics;
struct KernelInfo;
struct PcSamplingData;
struct WorkloadInfo;

/*
 * Translates what a DataProvider holds into JSON, for clients other than the
 * ImGui UI. Nothing here issues a request or changes the provider: the caller
 * decides when data is current, for instance from the provider's ready
 * callbacks, and sends what these functions return.
 *
 * Call these on the thread that runs DataProvider::Update(). The models are not
 * locked, so reading them from a server's worker thread races the next update.
 *
 * Every function follows the same conventions:
 * - 64-bit identifiers (tracks, events, topology nodes, processes, threads,
 *   PC-sampling uuids, client ids) are decimal strings. JavaScript reads JSON
 *   numbers as doubles, which round anything past 2^53, and event uuids and
 *   multi-node topology ids are past it. 32-bit ids (workloads, kernels,
 *   metric categories) stay numbers.
 * - Other uint64 values stay numbers through JavaScript's safe-integer limit
 *   and become decimal strings above it.
 * - Times are nanoseconds as the provider stores them, in keys ending in _ns.
 * - Enums are lower_snake_case names rather than their numeric values.
 * - A single item the provider has not loaded (a track's data, an event, a
 *   table slot, the summary) is null.
 */
namespace DataProviderJson
{

enum class TraceKind
{
    kSystem,
    kCompute
};

const char*
TraceKindName(TraceKind kind);

// Status plus everything small enough to send whole. The active trace kind has
// its data under "system" or "compute"; the inactive section is null.
// Track data, tables, events, the minimap, metric values and PC sampling have
// their own calls because each can run to megabytes.
jt::Json
Snapshot(DataProvider& provider, TraceKind kind);

// Trace kind, load state, progress message, trace path and requests in flight.
jt::Json
Status(DataProvider& provider, TraceKind kind);

// Trace bounds and every track's metadata, in timeline order.
jt::Json
Timeline(const TraceDataModel& model);
// One track's metadata, or null for an unknown id.
jt::Json
Track(const TraceDataModel& model, uint64_t track_id);
// The events or counter samples loaded for a track, or null if none are.
jt::Json
TrackData(const TimelineModel& timeline, uint64_t track_id);
// Every event track's density summed per bin and scaled to [0, 1]: the
// timeline's histogram with all tracks shown.
jt::Json
Histogram(const TimelineModel& timeline);
// Per-track density bins, keyed by track id.
jt::Json
Minimap(const TimelineModel& timeline);

jt::Json
Topology(const TopologyTree& topology);

// The rows a table slot holds and the request that filled them, or null for an
// empty slot. Reads whichever of the trace, summary or analysis tables owns it.
jt::Json
Table(const TraceDataModel& model, TableType type);
// The name clients address a slot by, e.g. "event_table".
const char*
TableTypeName(TableType type);
// False when name is not a slot's name.
bool
TableTypeFromName(const std::string& name, TableType& type_out);

// A fetched event's details, or null until the provider has fetched them.
jt::Json
Event(const EventModel& events, uint64_t event_uuid);

// The summary tree, or null until a summary of the current topology has loaded.
jt::Json
Summary(const TraceDataModel& model);

jt::Json
TrackStatistics(const AnalysisTrackStatistics& statistics);

// Profiler metadata and every workload with its kernels, metric catalog,
// memory-chart layout and roofline.
jt::Json
Compute(const ComputeDataModel& model);
jt::Json
Workload(const WorkloadInfo& workload);
jt::Json
Kernel(const KernelInfo& kernel);
// The metric values client_id fetched for a kernel, or null if it has none.
jt::Json
KernelMetrics(const ComputeDataModel& model, uint64_t client_id, uint32_t kernel_id);
// The workload-wide metric values client_id fetched, in catalog order.
jt::Json
WorkloadMetrics(const ComputeDataModel& model, uint64_t client_id,
                const WorkloadInfo& workload);
// The kernel selection table, or null before it has been fetched.
jt::Json
KernelSelectionTable(const ComputeKernelSelectionTable& selection_table);
jt::Json
PcSampling(const PcSamplingData& data);

}  // namespace DataProviderJson
}  // namespace View
}  // namespace RocProfVis
