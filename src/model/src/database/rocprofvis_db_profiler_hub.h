// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#ifdef ROCPROFVIS_PROFILER_HUB_ENABLED

#include "rocprofvis_db_rocprof.h"

#include "c_interface/profiler_hub.h"

#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace optiq
{
class TraceContext;
}

namespace RocProfVis
{
namespace DataModel
{

// Decorator over the legacy SQL-backed Database implementations: overrides
// worker methods the profiler-hub library covers, forwards everything else
// to `legacy_` unchanged. Callers only ever see a `Database*` - which
// worker methods are PH-backed vs SQL-backed is an internal detail.
class ProfilerHubDatabase : public Database
{
public:
    explicit ProfilerHubDatabase(rocprofvis_db_filename_t path);
    ~ProfilerHubDatabase() override;

    rocprofvis_dm_result_t Open() override;
    rocprofvis_dm_result_t Close() override;

    // legacy_ needs the same binding struct as this object - both must
    // reach the real Trace's FuncAddTrack/FuncAddRecord/etc, since worker
    // methods that forward to legacy_ execute legacy_'s own code, which
    // calls back through legacy_->BindObject(), not this->BindObject().
    rocprofvis_dm_result_t BindTrace(rocprofvis_dm_db_bind_struct* binding_info) override;

protected:
    // The static Func{FindCachedTableValue,GetInfoTableHandle} callbacks
    // bound into the shared bind_struct always operate on "this" object
    // (fixed at bind time, see BindTrace) - since ReadTraceMetadata runs on
    // legacy_ and populates legacy_'s own cached tables, this must forward
    // there too or every Node/Agent/PMC info-table lookup comes back empty.
    DatabaseCache* CachedTables(uint32_t node_id) override;

private:
    rocprofvis_dm_result_t BuildComputeQuery(rocprofvis_db_compute_use_case_enum_t use_case,
                                              rocprofvis_db_num_of_params_t         num,
                                              rocprofvis_db_compute_params_t        params,
                                              rocprofvis_dm_string_t& query) override;

    rocprofvis_dm_result_t BuildTableQuery(rocprofvis_dm_table_use_case_enum_t use_case,
                                            rocprofvis_dm_timestamp_t start, rocprofvis_dm_timestamp_t end,
                                            rocprofvis_db_num_of_tracks_t num,
                                            rocprofvis_db_track_selection_t tracks,
                                            rocprofvis_dm_charptr_t where, rocprofvis_dm_charptr_t filter,
                                            rocprofvis_dm_charptr_t group, rocprofvis_dm_charptr_t group_cols,
                                            rocprofvis_dm_charptr_t sort_column,
                                            rocprofvis_dm_sort_order_t sort_order, uint64_t max_count,
                                            uint64_t offset, bool count_only,
                                            rocprofvis_dm_string_t& query) override;

    rocprofvis_dm_result_t BuildEventSearchQuery(
        rocprofvis_dm_timestamp_t start, rocprofvis_dm_timestamp_t end, rocprofvis_db_num_of_tracks_t num,
        rocprofvis_db_track_selection_t ops, rocprofvis_dm_charptr_t where,
        rocprofvis_dm_num_string_table_filters_t num_string_table_filters,
        rocprofvis_dm_string_table_filters_t string_table_filters, bool include_substring,
        bool include_category, bool partial_matching, rocprofvis_dm_charptr_t sort_column,
        rocprofvis_dm_sort_order_t sort_order, uint64_t max_count, uint64_t offset, bool count_only,
        rocprofvis_dm_string_t& query) override;

    rocprofvis_dm_result_t SaveTrimmedData(rocprofvis_dm_timestamp_t start, rocprofvis_dm_timestamp_t end,
                                            rocprofvis_dm_charptr_t new_db_path, Future* future) override;

    rocprofvis_dm_result_t ReadTraceMetadata(Future* object) override;

    rocprofvis_dm_result_t ReadTraceSlice(rocprofvis_dm_timestamp_t start, rocprofvis_dm_timestamp_t end,
                                           rocprofvis_dm_hashed_timestamp_tag_t tag,
                                           rocprofvis_db_num_of_tracks_t   num,
                                           rocprofvis_db_track_selection_t tracks, Future* object) override;

    rocprofvis_dm_result_t ReadTracePMCSlice(rocprofvis_dm_timestamp_t start, rocprofvis_dm_timestamp_t end,
                                              rocprofvis_dm_hashed_timestamp_tag_t tag,
                                              rocprofvis_db_track_selection_t track, bool left_neighbor,
                                              bool right_neighbor, Future* object) override;

    rocprofvis_dm_result_t ReadFlowTraceInfo(rocprofvis_dm_event_id_t event_id, Future* object) override;
    rocprofvis_dm_result_t ReadStackTraceInfo(rocprofvis_dm_event_id_t event_id, Future* object) override;
    rocprofvis_dm_result_t ReadExtEventInfo(rocprofvis_dm_event_id_t event_id, Future* object) override;

    rocprofvis_dm_result_t ExecuteQuery(rocprofvis_dm_charptr_t query, rocprofvis_dm_charptr_t description,
                                         Future* object) override;

    rocprofvis_dm_result_t ExecuteComputeQuery(rocprofvis_db_compute_use_case_enum_t use_case,
                                                rocprofvis_dm_charptr_t query, Future* future) override;

    rocprofvis_dm_result_t ExportTableCSV(rocprofvis_dm_charptr_t query, rocprofvis_dm_charptr_t file_path,
                                           Future* future) override;

    // Builds by_tid_/by_agent_/by_agent_queue_/by_stream_ from
    // ph_ctx_->GetTrackList(), grouped by the numeric key relevant to each
    // PH category. Called once per ReadTraceMetadata, before legacy_ starts
    // discovering tracks, so matching can happen inline as each legacy
    // track is created (see InterceptedAddTrack) instead of in a separate
    // pass after the fact (too late - see TryMapTrack's doc comment).
    void BuildPhCandidateMaps();

    // Per-track match attempt against the maps BuildPhCandidateMaps() built.
    // Correlates legacy_'s track ids (insertion-order indices into its own
    // m_track_properties) with profiler-hub's independently-numbered
    // ph_track_t.id, by matching identifying fields (pid/tid/agent/queue/
    // stream, per category), resolved through the same CachedTables lookups
    // ProcessTrack itself uses (ids.id[] are DB row keys, not raw external
    // numbers - see ParseTrailingNumber's doc comment). On a match, widens
    // the track's timestamp bounds (see WidenTrackTimeBounds) and records
    // track_id_map_[legacy_id] = ph_id.
    //
    // Must run BEFORE the very first BindObject()->FuncAddTrack() call for
    // this track - Track's constructor (rocprofvis_controller_track.cpp)
    // snapshots min_ts/max_ts from track_indentifiers at that moment, and
    // there is no way to update them afterwards (Trace::AddTrack always
    // push_back()s a new Track, it doesn't update in place). That's why
    // this runs from inside InterceptedAddTrack, not as a separate pass
    // after legacy_->ReadTraceMetadata() returns like the first version of
    // this class did.
    bool TryMapTrack(rocprofvis_dm_track_params_t* track);

    // legacy's own min_ts/max_ts for a track come from ITS OWN SQL scan of
    // that track's rows - not necessarily the same extent as the PH track
    // it got matched to (different grouping, possibly more/fewer events).
    // Track::SetObject (rocprofvis_controller_track.cpp) drops any event
    // whose timestamp falls outside [min_ts, max_ts] as "out of range" -
    // widen it to TraceProperties()->db_inst_start_time/end_time[node] (the
    // whole trace's current known absolute span) so PH-sourced events for a
    // mapped track are not spuriously rejected. NOT an extreme sentinel
    // (e.g. [0, UINT64_MAX]): legacy's own ProfileDatabase::BuildHistogram,
    // which runs later in the SAME ReadTraceMetadata call, does real
    // arithmetic on track->min_ts/max_ts (bucket sizing) - an absurdly wide
    // span crashed it with SIGFPE (integer division producing a zero
    // divisor downstream). db_inst_start/end_time are progressively
    // refined via std::min/std::max as legacy discovers more tracks, so
    // they may not be the FINAL global bounds yet at this point in the
    // scan - but they're always a real, sane value from actual track data,
    // never a value that breaks legacy's own arithmetic.
    void WidenTrackTimeBounds(rocprofvis_dm_track_params_t* track, uint32_t node);

    // Legacy normalizes timestamps to 0 at trace start; profiler-hub's are
    // absolute. Returns the offset to add to a legacy [start,end] window
    // before passing it to ph_get_track_events/ph_get_track_samples. Unlike
    // WidenTrackTimeBounds, this runs later (during slice reads, after
    // ReadTraceMetadata has fully completed), so db_inst_start_time is
    // final by the time it's called - no ordering hazard here.
    uint64_t AbsoluteTimeOffset(rocprofvis_dm_track_id_t legacy_track_id);

    // Swapped into BindObject()->FuncAddTrack for the duration of
    // legacy_->ReadTraceMetadata() (see ReadTraceMetadata). Runs
    // TryMapTrack() on the about-to-be-added track (widening it in place if
    // matched), then forwards to the real FuncAddTrack so Trace::AddTrack
    // still runs exactly once per track, with the (possibly widened)
    // params. Plain function pointer (matches rocprofvis_dm_add_track_func_t's
    // C-callback signature, which has no user-data slot) - the active
    // instance and the original callback are carried via thread_local
    // statics (see the .cpp), valid only for the duration of one
    // ReadTraceMetadata call on the thread that made it.
    static rocprofvis_dm_result_t InterceptedAddTrack(const rocprofvis_dm_trace_t   object,
                                                       rocprofvis_dm_track_params_t* params);

    std::unordered_map<uint32_t, std::vector<ph_track_t>> by_tid_;
    std::unordered_map<uint32_t, std::vector<ph_track_t>> by_agent_;
    std::unordered_map<uint64_t, std::vector<ph_track_t>> by_agent_queue_;
    std::unordered_map<uint32_t, std::vector<ph_track_t>> by_stream_;
    uint32_t                                              mapped_count_   = 0;
    uint32_t                                              unmapped_count_ = 0;

    // Fetches events for a mapped track directly from profiler-hub and
    // pushes them through the same FuncAddSlice/FuncAddRecord/
    // FuncCompleteSlice contract QueryManager::ReadTraceSlice uses, so the
    // rest of the model/view layer can't tell the difference. Event ids are
    // synthesized (track_id-and-sequence based) - they will not resolve via
    // ReadFlowTraceInfo/ReadStackTraceInfo/ReadExtEventInfo, which is fine:
    // those already degrade gracefully (logged "not loaded") for any event
    // id absent from the legacy DB. Levels (nesting depth) are recomputed
    // in-memory per call (simple interval-stack), since ph_event_t carries
    // no precomputed level.
    rocprofvis_dm_result_t ReadTraceSliceViaPH(rocprofvis_dm_timestamp_t start, rocprofvis_dm_timestamp_t end,
                                                rocprofvis_dm_hashed_timestamp_tag_t tag,
                                                rocprofvis_dm_track_id_t legacy_track_id,
                                                uint32_t ph_track_id, Future* future);

    // Same idea as ReadTraceSliceViaPH, but for PMC/counter tracks:
    // ph_sample_t{timestamp, value} maps directly onto pmc_record_t, no
    // synthesized id/level/string-interning needed.
    rocprofvis_dm_result_t ReadTracePMCSliceViaPH(rocprofvis_dm_timestamp_t start,
                                                   rocprofvis_dm_timestamp_t end,
                                                   rocprofvis_dm_hashed_timestamp_tag_t tag,
                                                   rocprofvis_dm_track_id_t legacy_track_id,
                                                   uint32_t ph_track_id, Future* future);

    // Trace::AddString (BindObject()->FuncAddString) grows a plain
    // std::vector<std::string> with no internal locking. Legacy rarely
    // triggers real growth from ReadTraceSlice's worker threads because
    // event names are pre-interned once, single-threaded, during
    // ReadTraceMetadata's "Loading strings" pass - but PH-sourced event
    // names go through FuncAddString on every ReadTraceSliceViaPH call,
    // and FetchFromDataModel (rocprofvis_controller_track.cpp) splits a
    // single track's fetch across up to 2 concurrent worker threads. Two
    // concurrent vector reallocations corrupt the heap (observed: SIGABRT,
    // "double free or corruption", crash inside Trace::AddString). Guard
    // every FuncAddString call from this class with this mutex.
    std::mutex string_intern_mutex_;

    // Legacy SQL-backed implementation. Owns everything this class does not
    // yet source from profiler-hub.
    std::unique_ptr<Database> legacy_;
    // Null if the trace file could not be opened via profiler-hub (e.g. not
    // a schema it recognizes yet) - in that case every worker method simply
    // forwards to `legacy_`.
    std::unique_ptr<optiq::TraceContext> ph_ctx_;
    // legacy_ track id -> ph_track_t.id, populated by BuildTrackIdMapping().
    std::unordered_map<uint32_t, uint32_t> track_id_map_;
};

}  // namespace DataModel
}  // namespace RocProfVis

#endif  // ROCPROFVIS_PROFILER_HUB_ENABLED
