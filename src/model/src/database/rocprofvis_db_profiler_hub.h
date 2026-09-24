// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#ifdef ROCPROFVIS_PROFILER_HUB_ENABLED

#include "rocprofvis_db_rocprof.h"

#include "c/profiler_hub.h"

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

class ProfilerHubDatabase : public Database
{
public:
    explicit ProfilerHubDatabase(rocprofvis_db_filename_t path);
    ~ProfilerHubDatabase() override;

    rocprofvis_dm_result_t Open() override;
    rocprofvis_dm_result_t Close() override;

    rocprofvis_dm_result_t BindTrace(rocprofvis_dm_db_bind_struct* binding_info) override;

protected:
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

    void BuildPhCandidateMaps();

    bool TryMapTrack(rocprofvis_dm_track_params_t* track);

    void WidenTrackTimeBounds(rocprofvis_dm_track_params_t* track, uint32_t node);

    uint64_t AbsoluteTimeOffset(rocprofvis_dm_track_id_t legacy_track_id);

    static rocprofvis_dm_result_t InterceptedAddTrack(const rocprofvis_dm_trace_t   object,
                                                       rocprofvis_dm_track_params_t* params);

    std::unordered_map<uint32_t, std::vector<ph_track_t>> by_tid_;
    std::unordered_map<uint32_t, std::vector<ph_track_t>> by_agent_;
    std::unordered_map<uint64_t, std::vector<ph_track_t>> by_agent_queue_;
    std::unordered_map<uint32_t, std::vector<ph_track_t>> by_stream_;
    uint32_t                                              mapped_count_   = 0;
    uint32_t                                              unmapped_count_ = 0;

    rocprofvis_dm_result_t ReadTraceSliceViaPH(rocprofvis_dm_timestamp_t start, rocprofvis_dm_timestamp_t end,
                                                rocprofvis_dm_hashed_timestamp_tag_t tag,
                                                rocprofvis_dm_track_id_t legacy_track_id,
                                                uint32_t ph_track_id, Future* future);

    rocprofvis_dm_result_t ReadTracePMCSliceViaPH(rocprofvis_dm_timestamp_t start,
                                                   rocprofvis_dm_timestamp_t end,
                                                   rocprofvis_dm_hashed_timestamp_tag_t tag,
                                                   rocprofvis_dm_track_id_t legacy_track_id,
                                                   uint32_t ph_track_id, Future* future);

    std::mutex string_intern_mutex_;

    std::unique_ptr<Database> legacy_;
    std::unique_ptr<optiq::TraceContext> ph_ctx_;
    std::unordered_map<uint32_t, uint32_t> track_id_map_;
};

}
}

#endif
