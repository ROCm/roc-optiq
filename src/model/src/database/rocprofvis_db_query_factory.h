// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_db_sqlite.h"
#include <vector>

namespace RocProfVis
{
namespace DataModel
{

class QueryFactory : public DatabaseVersion
{
public:
    QueryFactory(SqliteDatabase* db);

    std::string GetRocprofRegionTrackQuery(bool is_sample_track);
    std::string GetRocprofRegionLevelQuery(bool is_sample_track);
    std::string GetRocprofRegionSliceQuery(bool is_sample_track);
    std::string GetRocprofRegionTableQuery(bool is_sample_track);


    std::string GetRocprofKernelDispatchTrackQuery();
    std::string GetRocprofKernelDispatchTrackQueryForStream();
    std::string GetRocprofKernelDispatchLevelQuery();
    std::string GetRocprofKernelDispatchSliceQuery();
    std::string GetRocprofKernelDispatchSliceQueryForStream();
    std::string GetRocprofKernelDispatchTableQuery();

    std::string GetRocprofMemoryAllocTrackQuery();
    std::string GetRocprofMemoryAllocTrackQueryForStream();
    std::string GetRocprofMemoryAllocLevelQuery();
    std::string GetRocprofMemoryAllocSliceQuery();
    std::string GetRocprofMemoryAllocSliceQueryForStream();
    std::string GetRocprofMemoryAllocTableQuery();
    std::string GetRocprofMemoryAllocActivityQuery();
    std::string GetRocprofMemoryAllocActivityLoadQuery();

    std::string GetRocprofMemoryCopyTrackQuery();
    std::string GetRocprofMemoryCopyTrackQueryForStream();
    std::string GetRocprofMemoryCopyLevelQuery();
    std::string GetRocprofMemoryCopySliceQuery();
    std::string GetRocprofMemoryCopySliceQueryForStream();
    std::string GetRocprofMemoryCopyTableQuery();

    // KFD (Kernel Fusion Driver) events. These live in rocpd_region and are
    // grouped onto per-GPU tracks; the owning GPU agent and the human-readable
    // sub-label are resolved from rocpd_arg joined to rocpd_info_agent.
    std::string GetRocprofKfdTrackQuery();
    std::string GetRocprofKfdLevelQuery();
    std::string GetRocprofKfdSliceQuery();
    std::string GetRocprofKfdTableQuery();

    std::string GetRocprofPerformanceCountersTrackQuery();
    std::string GetRocprofPerformanceCountersLevelQuery();
    std::string GetRocprofPerformanceCountersSliceQuery();
    std::string GetRocprofPerformanceCountersTableQuery();

    std::string GetRocprofSMIPerformanceCountersTrackQuery();
    std::string GetRocprofSMIPerformanceCountersLevelQuery();
    std::string GetRocprofSMIPerformanceCountersSliceQuery();
    std::string GetRocprofSMIPerformanceCountersTableQuery();

    std::string GetRocprofMemoryActivitySubQuery();
    std::string GetRocprofMemoryActivityTrackQuery();
    std::string GetRocprofMemoryActivityTableQuery();
    std::string GetRocprofMemoryActivityLevelQuery();
    std::string GetRocprofMemoryActivitySliceQuery();

    std::string GetRocprofDataFlowQueryForRegionEvent(uint64_t event_id);
    std::string GetRocprofDataFlowQueryForKernelDispatchEvent(uint64_t event_id);
    std::string GetRocprofDataFlowQueryForMemoryAllocEvent(uint64_t event_id);
    std::string GetRocprofDataFlowQueryForMemoryCopyEvent(uint64_t event_id);

    std::string GetRocprofEssentialInfoQueryForRegionEvent(uint64_t event_id, bool is_sample_track);
    std::string GetRocprofEssentialInfoQueryForKernelDispatchEvent(uint64_t event_id);
    std::string GetRocprofEssentialInfoQueryForMemoryAllocEvent(uint64_t event_id);
    std::string GetRocprofEssentialInfoQueryForMemoryCopyEvent(uint64_t event_id);

    std::string GetRocprofArgumentsInfoQueryForRegionEvent(uint64_t event_id);
    std::string GetRocprofArgumentsInfoQueryForKernelDispatchEvent(uint64_t event_id);
    std::string GetRocprofArgumentsInfoQueryForMemoryAllocEvent(uint64_t event_id);
    std::string GetRocprofArgumentsInfoQueryForMemoryCopyEvent(uint64_t event_id);

    std::string GetRocprofKernelDispatchStreamFlowQuery();
    std::string GetRocprofMemoryAllocStreamFlowQuery();
    std::string GetRocprofMemoryCopyStreamFlowQuery();

    std::string GetPerfettoEventSliceQuery();
    std::string GetPerfettoCounterSliceQuery();
    std::string GetPerfettoRegionTableQuery();
    std::string GetPerfettoPerformanceCountersTableQuery();

private:
    // Shared building blocks for the KFD queries so the track / level / slice
    // queries all compute the owning GPU agent and the sub-label identically
    // (BuildSliceQueryMap matches events to tracks by these exact expressions).
    std::vector<std::string> GetRocprofKfdFromClause();
    static const char* GetRocprofKfdOwningAgentExpr();
    static const char* GetRocprofKfdLabelExpr();

    SqliteDatabase* m_db;
};


}  // namespace DataModel
}  // namespace RocProfVis