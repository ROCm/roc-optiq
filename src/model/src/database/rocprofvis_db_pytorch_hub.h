// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#ifdef ROCPROFVIS_PROFILER_HUB_ENABLED

#include "rocprofvis_db.h"

#include <memory>
#include <mutex>
#include <unordered_map>

namespace optiq
{
class TraceContext;
}

namespace RocProfVis
{
namespace DataModel
{

class PytorchHubDatabase : public Database
{
public:
    explicit PytorchHubDatabase(rocprofvis_db_filename_t path);
    ~PytorchHubDatabase() override;

    rocprofvis_dm_result_t Open() override;
    rocprofvis_dm_result_t Close() override;

    // kAutodetect for any other file, and for a PyTorch trace when
    // OPTIQ_PYTORCH_ROUTE=perfetto, so that trace falls through to Optiq's own detection.
    static rocprofvis_db_type_t Detect(rocprofvis_db_filename_t filename);

private:
    rocprofvis_dm_result_t BuildComputeQuery(rocprofvis_db_compute_use_case_enum_t use_case,
                                             rocprofvis_db_num_of_params_t         num,
                                             rocprofvis_db_compute_params_t        params,
                                             rocprofvis_dm_string_t&               query) override;

    rocprofvis_dm_result_t BuildTableQuery(
        rocprofvis_dm_table_use_case_enum_t use_case, rocprofvis_dm_timestamp_t start,
        rocprofvis_dm_timestamp_t end, rocprofvis_db_num_of_tracks_t num,
        rocprofvis_db_track_selection_t tracks, rocprofvis_dm_charptr_t where,
        rocprofvis_dm_charptr_t filter, rocprofvis_dm_charptr_t group,
        rocprofvis_dm_charptr_t group_cols, rocprofvis_dm_charptr_t sort_column,
        rocprofvis_dm_sort_order_t sort_order, uint64_t max_count, uint64_t offset,
        bool count_only, rocprofvis_dm_string_t& query) override;

    rocprofvis_dm_result_t SaveTrimmedData(rocprofvis_dm_timestamp_t start,
                                           rocprofvis_dm_timestamp_t end,
                                           rocprofvis_dm_charptr_t new_db_path,
                                           Future* future) override;

    rocprofvis_dm_result_t ReadTraceMetadata(Future* future) override;

    rocprofvis_dm_result_t ReadTraceSlice(rocprofvis_dm_timestamp_t            start,
                                          rocprofvis_dm_timestamp_t            end,
                                          rocprofvis_dm_hashed_timestamp_tag_t tag,
                                          rocprofvis_db_num_of_tracks_t        num,
                                          rocprofvis_db_track_selection_t      tracks,
                                          Future* future) override;

    rocprofvis_dm_result_t ReadFlowTraceInfo(rocprofvis_dm_event_id_t event_id,
                                             Future*                  future) override;
    rocprofvis_dm_result_t ReadStackTraceInfo(rocprofvis_dm_event_id_t event_id,
                                              Future*                  future) override;
    rocprofvis_dm_result_t ReadExtEventInfo(rocprofvis_dm_event_id_t event_id,
                                            Future*                  future) override;

    rocprofvis_dm_result_t ExecuteQuery(rocprofvis_dm_charptr_t query,
                                        rocprofvis_dm_charptr_t description,
                                        Future*                 future) override;

    rocprofvis_dm_result_t ExecuteComputeQuery(rocprofvis_db_compute_use_case_enum_t use_case,
                                               rocprofvis_dm_charptr_t query,
                                               Future*                 future) override;

    std::mutex                              string_intern_mutex_;
    std::unique_ptr<optiq::TraceContext>    ph_ctx_;
    std::unordered_map<uint32_t, uint32_t>  track_id_map_;
};

}  // namespace DataModel
}  // namespace RocProfVis

#endif
