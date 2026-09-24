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

}

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
ProfilerHubDatabase*           g_intercepting_instance = nullptr;
rocprofvis_dm_add_track_func_t g_original_add_track    = nullptr;
}

rocprofvis_dm_result_t
ProfilerHubDatabase::ReadTraceMetadata(Future* object)
{
    if(ph_ctx_) BuildPhCandidateMaps();

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
        ++unmapped_count_;
        return false;
    }

    rocprofvis_dm_track_identifiers_t& ids       = track->track_indentifiers;
    uint32_t                           legacy_id = ids.track_id;

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
            uint32_t real_agent = static_cast<uint32_t>(ids.id[TRACK_ID_AGENT]);
            if(auto it = by_agent_.find(real_agent); it != by_agent_.end())
                candidates = &it->second;
            name_hint =
                ids.name[TRACK_ID_QUEUE];
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

}
}

#endif
