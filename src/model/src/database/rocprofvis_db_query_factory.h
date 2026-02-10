// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_db.h"
#include <vector>

namespace RocProfVis
{
namespace DataModel
{

class ProfileDatabase;

class QueryFactory : public DatabaseVersion
{
public:
    QueryFactory(ProfileDatabase* db);

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

    std::string GetRocprofMemoryCopyTrackQuery();
    std::string GetRocprofMemoryCopyTrackQueryForStream();
    std::string GetRocprofMemoryCopyLevelQuery();
    std::string GetRocprofMemoryCopySliceQuery();
    std::string GetRocprofMemoryCopySliceQueryForStream();
    std::string GetRocprofMemoryCopyTableQuery();

    std::string GetRocprofPerformanceCountersTrackQuery();
    std::string GetRocprofPerformanceCountersLevelQuery();
    std::string GetRocprofPerformanceCountersSliceQuery();
    std::string GetRocprofPerformanceCountersTableQuery();

    std::string GetRocprofSMIPerformanceCountersTrackQuery();
    std::string GetRocprofSMIPerformanceCountersLevelQuery();
    std::string GetRocprofSMIPerformanceCountersSliceQuery();
    std::string GetRocprofSMIPerformanceCountersTableQuery();

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

private:

    ProfileDatabase* m_db;
};


}  // namespace DataModel
}  // namespace RocProfVis