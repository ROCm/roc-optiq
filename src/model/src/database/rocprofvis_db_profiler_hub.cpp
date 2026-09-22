// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#ifdef ROCPROFVIS_PROFILER_HUB_ENABLED

#    include "rocprofvis_db_profiler_hub.h"
#    include "trace_context.h"

#    include "spdlog/spdlog.h"

#    include <algorithm>
#    include <cctype>
#    include <cstdint>

namespace RocProfVis
{
namespace DataModel
{

namespace
{

// Splits a mixed-style identifier into lowercase word tokens, breaking on
// '_'/' ' and on lowercase->uppercase transitions (CamelCase). E.g.
// "device_busy_gfx" -> {device, busy, gfx}; "GFX Busy" -> {gfx, busy};
// "MemUsg" -> {mem, usg}. Lets legacy's abbreviated/reordered counter names
// (e.g. "GFX Busy", "MemUsg") be compared against PH's canonical
// snake_case names (e.g. "device_busy_gfx", "device_memory_usage")
// regardless of word order.
std::vector<std::string>
Tokenize(const std::string& s)
{
    std::vector<std::string> tokens;
    std::string              current;
    for(size_t i = 0; i < s.size(); ++i)
    {
        char c = s[i];
        if(c == '_' || c == ' ' || c == '-')
        {
            if(!current.empty()) tokens.push_back(current);
            current.clear();
            continue;
        }
        if(!current.empty() && std::isupper(static_cast<unsigned char>(c)) &&
           std::islower(static_cast<unsigned char>(current.back())))
        {
            tokens.push_back(current);
            current.clear();
        }
        current.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    if(!current.empty()) tokens.push_back(current);
    return tokens;
}

// True if every character of `needle` appears in `haystack` in order
// (not necessarily contiguous) - lets an abbreviated token like "usg"
// match its unabbreviated form "usage".
bool
IsSubsequence(const std::string& needle, const std::string& haystack)
{
    size_t pos = 0;
    for(char c : needle)
    {
        pos = haystack.find(c, pos);
        if(pos == std::string::npos) return false;
        ++pos;
    }
    return true;
}

// Order-independent, abbreviation-tolerant name match: every word token in
// `name_hint` (legacy's display name, e.g. "GFX Busy" or "MemUsg") must be
// a subsequence of some word token in `candidate_name` (PH's canonical
// name, e.g. "device_busy_gfx"). Used to disambiguate PH tracks that share
// the same numeric key (e.g. several PMC counters on the same pid/agent).
bool
NameHintMatches(const std::string& candidate_name, const std::string& name_hint)
{
    std::vector<std::string> hint_tokens      = Tokenize(name_hint);
    std::vector<std::string> candidate_tokens = Tokenize(candidate_name);
    if(hint_tokens.empty()) return false;
    for(const std::string& hint_token : hint_tokens)
    {
        bool matched = false;
        for(const std::string& candidate_token : candidate_tokens)
        {
            if(IsSubsequence(hint_token, candidate_token))
            {
                matched = true;
                break;
            }
        }
        if(!matched) return false;
    }
    return true;
}

// Extracts the trailing run of digits from a string (e.g. "Thread 3930708"
// -> 3930708, "Queue 1" -> 1). Legacy cached-table cells embed the real,
// externally-meaningful id as a formatted display string; this recovers it.
uint32_t
ParseTrailingNumber(const std::string& s)
{
    size_t end   = s.size();
    size_t begin = end;
    while(begin > 0 && std::isdigit(static_cast<unsigned char>(s[begin - 1])))
        --begin;
    if(begin == end) return 0;
    return static_cast<uint32_t>(std::stoul(s.substr(begin, end - begin)));
}

}  // namespace

ProfilerHubDatabase::ProfilerHubDatabase(rocprofvis_db_filename_t path)
: Database(path)
, legacy_(std::make_unique<RocprofDatabase>(path))
{}

ProfilerHubDatabase::~ProfilerHubDatabase() = default;

rocprofvis_dm_result_t
ProfilerHubDatabase::Open()
{
    rocprofvis_dm_result_t result = legacy_->Open();
    try
    {
        ph_ctx_ = std::make_unique<optiq::TraceContext>(Path());
    } catch(const optiq::TraceOpenError&)
    {
        // profiler-hub doesn't recognize this trace (yet) - fall back to
        // legacy_ for everything, silently. legacy_'s Open() result above
        // is what actually determines whether this trace can be read at all.
        ph_ctx_.reset();
    }
    return result;
}

rocprofvis_dm_result_t
ProfilerHubDatabase::Close()
{
    ph_ctx_.reset();
    return legacy_->Close();
}

rocprofvis_dm_result_t
ProfilerHubDatabase::BindTrace(rocprofvis_dm_db_bind_struct* binding_info)
{
    rocprofvis_dm_result_t result = Database::BindTrace(binding_info);
    if(result != kRocProfVisDmResultSuccess) return result;
    return legacy_->BindTrace(binding_info);
}

DatabaseCache*
ProfilerHubDatabase::CachedTables(uint32_t node_id)
{
    return legacy_->CachedTables(node_id);
}

rocprofvis_dm_result_t
ProfilerHubDatabase::BuildComputeQuery(rocprofvis_db_compute_use_case_enum_t use_case,
                                       rocprofvis_db_num_of_params_t         num,
                                       rocprofvis_db_compute_params_t        params,
                                       rocprofvis_dm_string_t&               query)
{
    return legacy_->BuildComputeQuery(use_case, num, params, query);
}

rocprofvis_dm_result_t
ProfilerHubDatabase::BuildTableQuery(
    rocprofvis_dm_table_use_case_enum_t use_case, rocprofvis_dm_timestamp_t start,
    rocprofvis_dm_timestamp_t end, rocprofvis_db_num_of_tracks_t num,
    rocprofvis_db_track_selection_t tracks, rocprofvis_dm_charptr_t where,
    rocprofvis_dm_charptr_t filter, rocprofvis_dm_charptr_t group,
    rocprofvis_dm_charptr_t group_cols, rocprofvis_dm_charptr_t sort_column,
    rocprofvis_dm_sort_order_t sort_order, uint64_t max_count, uint64_t offset,
    bool count_only, rocprofvis_dm_string_t& query)
{
    return legacy_->BuildTableQuery(use_case, start, end, num, tracks, where, filter,
                                    group, group_cols, sort_column, sort_order, max_count,
                                    offset, count_only, query);
}

rocprofvis_dm_result_t
ProfilerHubDatabase::BuildEventSearchQuery(
    rocprofvis_dm_timestamp_t start, rocprofvis_dm_timestamp_t end,
    rocprofvis_db_num_of_tracks_t num, rocprofvis_db_track_selection_t ops,
    rocprofvis_dm_charptr_t                  where,
    rocprofvis_dm_num_string_table_filters_t num_string_table_filters,
    rocprofvis_dm_string_table_filters_t string_table_filters, bool include_substring,
    bool include_category, bool partial_matching, rocprofvis_dm_charptr_t sort_column,
    rocprofvis_dm_sort_order_t sort_order, uint64_t max_count, uint64_t offset,
    bool count_only, rocprofvis_dm_string_t& query)
{
    return legacy_->BuildEventSearchQuery(
        start, end, num, ops, where, num_string_table_filters, string_table_filters,
        include_substring, include_category, partial_matching, sort_column, sort_order,
        max_count, offset, count_only, query);
}

rocprofvis_dm_result_t
ProfilerHubDatabase::SaveTrimmedData(rocprofvis_dm_timestamp_t start,
                                     rocprofvis_dm_timestamp_t end,
                                     rocprofvis_dm_charptr_t new_db_path, Future* future)
{
    return legacy_->SaveTrimmedData(start, end, new_db_path, future);
}

namespace
{
// Carries the active ProfilerHubDatabase instance and the real FuncAddTrack
// across InterceptedAddTrack's plain-function-pointer boundary. Valid only
// between ReadTraceMetadata installing the interceptor and removing it.
//
// Deliberately NOT thread_local: legacy_'s own metadata scan runs its
// per-category track discovery on a pool of internal worker threads (see
// m_add_track_mutex in RocprofDatabase), each of which calls
// BindObject()->FuncAddTrack directly - a thread_local here would be
// nullptr on every one of those threads (only the thread that entered this
// ReadTraceMetadata override would have it set), and InterceptedAddTrack
// would then call through a null "original" pointer -> segfault (observed).
// A plain static is safe here because it's set once before legacy_'s
// worker threads are spawned and only cleared after they've all been
// joined (ReadTraceMetadata is synchronous/blocking) - std::thread's
// constructor and join() both establish the happens-before relationship
// this relies on.
ProfilerHubDatabase*           g_intercepting_instance = nullptr;
rocprofvis_dm_add_track_func_t g_original_add_track    = nullptr;
}  // namespace

rocprofvis_dm_result_t
ProfilerHubDatabase::ReadTraceMetadata(Future* object)
{
    if(ph_ctx_) BuildPhCandidateMaps();

    // Intercept FuncAddTrack so each track can be matched/widened at the
    // moment legacy_ creates it - see TryMapTrack's doc comment for why
    // this can't be done as a separate pass after ReadTraceMetadata
    // returns (Track's controller-side bounds are snapshotted immediately
    // and can't be updated afterwards).
    rocprofvis_dm_add_track_func_t previous_add_track = nullptr;
    if(ph_ctx_)
    {
        mapped_count_   = 0;
        unmapped_count_ = 0;
        track_id_map_.clear();
        previous_add_track         = BindObject()->FuncAddTrack;
        g_original_add_track       = previous_add_track;
        g_intercepting_instance    = this;
        BindObject()->FuncAddTrack = &ProfilerHubDatabase::InterceptedAddTrack;
    }

    rocprofvis_dm_result_t result = legacy_->ReadTraceMetadata(object);

    if(ph_ctx_)
    {
        BindObject()->FuncAddTrack = previous_add_track;
        g_intercepting_instance    = nullptr;
        g_original_add_track       = nullptr;

        if(unmapped_count_ > 0)
            spdlog::info("[profiler-hub] track-id mapping: {} matched, {} unmapped",
                         mapped_count_, unmapped_count_);
        else
            spdlog::info("[profiler-hub] track-id mapping: all {} legacy tracks matched",
                         mapped_count_);
    }

    return result;
}

rocprofvis_dm_result_t
ProfilerHubDatabase::InterceptedAddTrack(const rocprofvis_dm_trace_t   object,
                                         rocprofvis_dm_track_params_t* params)
{
    ProfilerHubDatabase*           instance = g_intercepting_instance;
    rocprofvis_dm_add_track_func_t original = g_original_add_track;
    if(instance != nullptr) instance->TryMapTrack(params);
    return original(object, params);
}

void
ProfilerHubDatabase::WidenTrackTimeBounds(rocprofvis_dm_track_params_t* track,
                                          uint32_t                      node)
{
    // DISABLED: mutating a mapped track's min_ts/max_ts here - even to a
    // "sane" value like TraceProperties()->db_inst_start/end_time[node],
    // not just an extreme sentinel - reliably crashed legacy's OWN later
    // ReadTraceMetadata passes with SIGFPE (observed inside
    // ProfileDatabase::BuildHistogram / QueryManager::
    // CalculateParallelProcessSplitCount's "(record_count * 10) /
    // total_event_count" divide, though the exact causal path from a
    // widened min_ts/max_ts to a zero divisor there was not fully traced -
    // isolation testing (commenting out just these two call sites)
    // reliably fixed it, confirmed via a 100s controller-test stress run
    // with zero crashes vs. crashing within the first few seconds
    // otherwise). track->min_ts/max_ts feed real arithmetic in legacy's
    // own metadata-building passes (histogram bucketing, parallel-split
    // sizing, etc.), not just the controller's out-of-bounds display
    // check we were trying to satisfy - overwriting them is not safe.
    //
    // Net effect of leaving this disabled: PH-mapped tracks whose real
    // profiler-hub event range extends beyond what legacy's own (narrower)
    // per-track SQL scan found will have those extra events dropped by
    // Track::SetObject's "out of range" check (rocprofvis_controller_track.cpp)
    // - a logged warning, not a crash. Tracks are no longer empty (the
    // bigger bug, fixed by AbsoluteTimeOffset in ReadTraceSliceViaPH/
    // ReadTracePMCSliceViaPH), just potentially clipped at the edges.
    (void) track;
    (void) node;
}

void
ProfilerHubDatabase::BuildPhCandidateMaps()
{
    by_tid_.clear();
    by_agent_.clear();
    by_agent_queue_.clear();
    by_stream_.clear();

    auto Key2 = [](uint32_t a, uint32_t b) -> uint64_t {
        return (static_cast<uint64_t>(a) << 32) | b;
    };

    for(const ph_track_t& track : ph_ctx_->GetTrackList())
    {
        switch(track.category)
        {
            case PH_TRACK_CATEGORY_THREAD: by_tid_[track.tid].push_back(track); break;
            case PH_TRACK_CATEGORY_PMC_AGENT:
                by_agent_[track.agent_id].push_back(track);
                break;
            case PH_TRACK_CATEGORY_KERNEL_DISPATCH_AGENT_QUEUE:
            case PH_TRACK_CATEGORY_MEMORY_ALLOCATE_AGENT_QUEUE:
            case PH_TRACK_CATEGORY_MEMORY_COPY_AGENT_QUEUE:
                by_agent_queue_[Key2(track.agent_id, track.queue_id)].push_back(track);
                break;
            case PH_TRACK_CATEGORY_STREAM:
                by_stream_[track.stream_id].push_back(track);
                break;
        }
    }
}

bool
ProfilerHubDatabase::TryMapTrack(rocprofvis_dm_track_params_t* track)
{
    if(!ph_ctx_ || legacy_->NumDbInstances() > 1)
    {
        // Multi-node merge not supported yet - legacy node ids are 64-bit
        // guid hashes, unrelated to profiler-hub's plain nid, so matching
        // would be unreliable across more than one db instance.
        ++unmapped_count_;
        return false;
    }

    rocprofvis_dm_track_identifiers_t& ids       = track->track_indentifiers;
    uint32_t                           legacy_id = ids.track_id;

    // ids.id[TRACK_ID_*] are DB row keys (foreign keys into the
    // Thread/Agent/Queue/Stream cached tables), not the raw external
    // tid/agent/queue/stream numbers PH reports - resolve through the
    // same cached-table lookups ProcessTrack itself uses to build display
    // names, and recover the real number from that text.
    DbInstance* db_instance = static_cast<DbInstance*>(ids.db_instance);
    uint32_t    node        = db_instance ? db_instance->GuidIndex() : 0;

    const std::vector<ph_track_t>* candidates = nullptr;
    std::string                    name_hint;

    auto Key2 = [](uint32_t a, uint32_t b) -> uint64_t {
        return (static_cast<uint64_t>(a) << 32) | b;
    };

    switch(ids.category)
    {
        case kRocProfVisDmRegionTrack:
        case kRocProfVisDmRegionMainTrack:
        case kRocProfVisDmRegionSampleTrack:
        {
            uint32_t real_tid =
                ParseTrailingNumber(legacy_->CachedTables(node)->GetTableCell(
                    "Thread", ids.id[TRACK_ID_TID], "name"));
            if(auto it = by_tid_.find(real_tid); it != by_tid_.end())
                candidates = &it->second;
            break;
        }
        case kRocProfVisDmPmcTrack:
        {
            // Unlike TID, the Agent row-key coincides with PH's logical
            // agent_id in every trace observed so far (both small,
            // sequential, profiler-assigned indices) - use it directly, no
            // CachedTables resolution.
            uint32_t real_agent = static_cast<uint32_t>(ids.id[TRACK_ID_AGENT]);
            if(auto it = by_agent_.find(real_agent); it != by_agent_.end())
                candidates = &it->second;
            name_hint =
                ids.name[TRACK_ID_QUEUE];  // PMC symbol, resolved by ProcessTrack already
            break;
        }
        case kRocProfVisDmKernelDispatchTrack:
        case kRocProfVisDmMemoryAllocationTrack:
        case kRocProfVisDmMemoryCopyTrack:
        {
            uint32_t real_agent = static_cast<uint32_t>(ids.id[TRACK_ID_AGENT]);
            uint32_t real_queue = static_cast<uint32_t>(ids.id[TRACK_ID_QUEUE]);
            if(auto it = by_agent_queue_.find(Key2(real_agent, real_queue));
               it != by_agent_queue_.end())
                candidates = &it->second;
            break;
        }
        case kRocProfVisDmStreamTrack:
        {
            uint32_t real_stream =
                ParseTrailingNumber(legacy_->CachedTables(node)->GetTableCell(
                    "Stream", ids.id[TRACK_ID_STREAM], "name"));
            if(auto it = by_stream_.find(real_stream); it != by_stream_.end())
                candidates = &it->second;
            break;
        }
        default: break;
    }

    if(!candidates || candidates->empty())
    {
        ++unmapped_count_;
        return false;
    }
    if(candidates->size() == 1 || name_hint.empty())
    {
        track_id_map_[legacy_id] = (*candidates)[0].id;
        WidenTrackTimeBounds(track, node);
        ++mapped_count_;
        return true;
    }
    // Multiple PH tracks share this key (e.g. several PMC counters on the
    // same pid/agent) - disambiguate by counter name.
    for(const ph_track_t& candidate : *candidates)
    {
        if(NameHintMatches(candidate.track_name, name_hint))
        {
            track_id_map_[legacy_id] = candidate.id;
            WidenTrackTimeBounds(track, node);
            ++mapped_count_;
            return true;
        }
    }
    ++unmapped_count_;
    return false;
}

rocprofvis_dm_result_t
ProfilerHubDatabase::ReadTraceSlice(rocprofvis_dm_timestamp_t            start,
                                    rocprofvis_dm_timestamp_t            end,
                                    rocprofvis_dm_hashed_timestamp_tag_t tag,
                                    rocprofvis_db_num_of_tracks_t        num,
                                    rocprofvis_db_track_selection_t      tracks,
                                    Future*                              object)
{
    // QueryManager::ReadTraceSlice never actually batches (asserts num==1) -
    // only handle the single-track case via PH, forward everything else.
    // Mirrors QueryManager::ReadTraceSlice's own behavior: callers may ask
    // for a PMC track through this entry point too, and it internally
    // redirects to the PMC path - do the same, or PMC tracks silently come
    // back empty (ph_get_track_events on a PMC/sample track_id legitimately
    // returns nothing; it's the wrong PH call for that category).
    if(num == 1 && ph_ctx_)
    {
        if(auto it = track_id_map_.find(*tracks); it != track_id_map_.end())
        {
            rocprofvis_dm_track_params_t* props = legacy_->TrackPropertiesAt(*tracks);
            if(props->track_indentifiers.category == kRocProfVisDmPmcTrack)
            {
                spdlog::info("[profiler-hub] ReadTraceSlice: reading via PH");
                return ReadTracePMCSliceViaPH(start, end, tag, *tracks, it->second,
                                              object);
            }
            spdlog::info("[profiler-hub] ReadTraceSlice: reading via PH");
            return ReadTraceSliceViaPH(start, end, tag, *tracks, it->second, object);
        }
    }
    spdlog::info("[profiler-hub] ReadTraceSlice: reading via legacy");
    return legacy_->ReadTraceSlice(start, end, tag, num, tracks, object);
}

uint64_t
ProfilerHubDatabase::AbsoluteTimeOffset(rocprofvis_dm_track_id_t legacy_track_id)
{
    // Legacy normalizes timestamps to 0 at the trace's start (see
    // rocprofvis_dm_track.cpp: min_ts/max_ts getters subtract
    // db_inst_start_time), but the raw event timestamps in the trace file -
    // and everything ph_get_track_events/ph_get_track_samples returns - are
    // absolute. Any [start,end] window this class receives must be shifted
    // by this offset before being handed to profiler-hub, or every query
    // misses (the two clocks don't overlap at all).
    rocprofvis_dm_track_params_t* props = legacy_->TrackPropertiesAt(legacy_track_id);
    auto* db_instance = static_cast<DbInstance*>(props->track_indentifiers.db_instance);
    if(db_instance == nullptr) return 0;
    return TraceProperties()->db_inst_start_time[db_instance->GuidIndex()];
}

rocprofvis_dm_result_t
ProfilerHubDatabase::ReadTraceSliceViaPH(rocprofvis_dm_timestamp_t            start,
                                         rocprofvis_dm_timestamp_t            end,
                                         rocprofvis_dm_hashed_timestamp_tag_t tag,
                                         rocprofvis_dm_track_id_t legacy_track_id,
                                         uint32_t ph_track_id, Future* future)
{
    uint64_t                offset = AbsoluteTimeOffset(legacy_track_id);
    std::vector<ph_event_t> events =
        ph_ctx_->GetTrackEvents(ph_track_id, start + offset, end + offset);

    spdlog::info("[profiler-hub] ReadTraceSliceViaPH legacy_track={} ph_track={} "
                 "start={} end={} -> "
                 "{} events",
                 legacy_track_id, ph_track_id, start, end, events.size());

    // Nesting depth via a simple interval stack - approximate (only sees
    // this window's events, not ancestors that started earlier and are
    // still open), acceptable for a first pass since legacy's own level
    // values are advisory display depth, not used for correctness checks.
    std::sort(events.begin(), events.end(),
              [](const ph_event_t& a, const ph_event_t& b) { return a.start < b.start; });
    std::vector<uint64_t> open_ends;

    rocprofvis_dm_slice_t slice = BindObject()->FuncAddSlice(
        BindObject()->trace_object, legacy_track_id, start, end, tag);

    uint32_t sequence = 0;
    for(const ph_event_t& event : events)
    {
        while(!open_ends.empty() && open_ends.back() <= event.start)
            open_ends.pop_back();

        rocprofvis_db_record_data_t record{};
        record.event.id.bitfield.event_id =
            (static_cast<uint64_t>(ph_track_id) << 32) | sequence++;
        record.event.id.bitfield.event_node = 0;
        record.event.id.bitfield.event_op   = kRocProfVisDmOperationNoOp;
        record.event.timestamp              = event.start - offset;
        record.event.duration =
            static_cast<rocprofvis_dm_duration_t>(event.end - event.start);
        {
            std::lock_guard<std::mutex> lock(string_intern_mutex_);
            record.event.category =
                BindObject()->FuncAddString(BindObject()->trace_object, event.name);
        }
        record.event.symbol = record.event.category;
        record.event.level  = static_cast<rocprofvis_dm_event_level_t>(open_ends.size());

        BindObject()->FuncAddRecord(slice, record);
        open_ends.push_back(event.end);
    }

    BindObject()->FuncCompleteSlice(slice);
    ShowProgress(100 - future->Progress(),
                 "Time slice successfully loaded! (profiler-hub)", kRPVDbSuccess, future);
    return future->SetPromise(kRocProfVisDmResultSuccess);
}

rocprofvis_dm_result_t
ProfilerHubDatabase::ReadTracePMCSlice(rocprofvis_dm_timestamp_t            start,
                                       rocprofvis_dm_timestamp_t            end,
                                       rocprofvis_dm_hashed_timestamp_tag_t tag,
                                       rocprofvis_db_track_selection_t      track,
                                       bool left_neighbor, bool right_neighbor,
                                       Future* object)
{
    if(ph_ctx_)
    {
        if(auto it = track_id_map_.find(*track); it != track_id_map_.end())
        {
            spdlog::info("[profiler-hub] ReadTracePMCSlice: reading via PH");
            return ReadTracePMCSliceViaPH(start, end, tag, *track, it->second, object);
        }
    }
    spdlog::info("[profiler-hub] ReadTracePMCSlice: reading via legacy");
    return legacy_->ReadTracePMCSlice(start, end, tag, track, left_neighbor,
                                      right_neighbor, object);
}

rocprofvis_dm_result_t
ProfilerHubDatabase::ReadTracePMCSliceViaPH(rocprofvis_dm_timestamp_t            start,
                                            rocprofvis_dm_timestamp_t            end,
                                            rocprofvis_dm_hashed_timestamp_tag_t tag,
                                            rocprofvis_dm_track_id_t legacy_track_id,
                                            uint32_t ph_track_id, Future* future)
{
    // Left/right-neighbor extension (continuity padding just outside
    // [start,end], used by legacy for chart rendering) is not implemented
    // yet - PH samples are fetched strictly within the requested window.
    uint64_t                 offset = AbsoluteTimeOffset(legacy_track_id);
    std::vector<ph_sample_t> samples =
        ph_ctx_->GetTrackSamples(ph_track_id, start + offset, end + offset);

    spdlog::info("[profiler-hub] ReadTracePMCSliceViaPH ph_track={} start={} end={} -> "
                 "{} samples",
                 ph_track_id, start, end, samples.size());

    rocprofvis_dm_slice_t slice = BindObject()->FuncAddSlice(
        BindObject()->trace_object, legacy_track_id, start, end, tag);

    for(const ph_sample_t& sample : samples)
    {
        rocprofvis_db_record_data_t record{};
        record.pmc.timestamp = sample.timestamp - offset;
        record.pmc.value     = sample.value;
        BindObject()->FuncAddRecord(slice, record);
    }

    BindObject()->FuncCompleteSlice(slice);
    ShowProgress(100 - future->Progress(),
                 "Time slice successfully loaded! (profiler-hub)", kRPVDbSuccess, future);
    return future->SetPromise(kRocProfVisDmResultSuccess);
}

rocprofvis_dm_result_t
ProfilerHubDatabase::ReadFlowTraceInfo(rocprofvis_dm_event_id_t event_id, Future* object)
{
    return legacy_->ReadFlowTraceInfo(event_id, object);
}

rocprofvis_dm_result_t
ProfilerHubDatabase::ReadStackTraceInfo(rocprofvis_dm_event_id_t event_id, Future* object)
{
    return legacy_->ReadStackTraceInfo(event_id, object);
}

rocprofvis_dm_result_t
ProfilerHubDatabase::ReadExtEventInfo(rocprofvis_dm_event_id_t event_id, Future* object)
{
    return legacy_->ReadExtEventInfo(event_id, object);
}

rocprofvis_dm_result_t
ProfilerHubDatabase::ExecuteQuery(rocprofvis_dm_charptr_t query,
                                  rocprofvis_dm_charptr_t description, Future* object)
{
    return legacy_->ExecuteQuery(query, description, object);
}

rocprofvis_dm_result_t
ProfilerHubDatabase::ExecuteComputeQuery(rocprofvis_db_compute_use_case_enum_t use_case,
                                         rocprofvis_dm_charptr_t query, Future* future)
{
    return legacy_->ExecuteComputeQuery(use_case, query, future);
}

rocprofvis_dm_result_t
ProfilerHubDatabase::ExportTableCSV(rocprofvis_dm_charptr_t query,
                                    rocprofvis_dm_charptr_t file_path, Future* future)
{
    return legacy_->ExportTableCSV(query, file_path, future);
}

}  // namespace DataModel
}  // namespace RocProfVis

#endif  // ROCPROFVIS_PROFILER_HUB_ENABLED
