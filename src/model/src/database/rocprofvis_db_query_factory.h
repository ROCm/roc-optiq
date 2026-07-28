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
    std::string GetRocprofMemoryAllocActivityQuery();
    std::string GetRocprofMemoryAllocActivityLoadQuery();

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

    std::string GetRocprofMemoryActivitySubQuery();
    std::string GetRocprofMemoryActivityTrackQuery();
    std::string GetRocprofMemoryActivityTableQuery();
    std::string GetRocprofMemoryActivityLevelQuery();
    std::string GetRocprofMemoryActivitySliceQuery();

    std::string GetRocprofKernelDispatchStreamFlowQuery();
    std::string GetRocprofMemoryAllocStreamFlowQuery();
    std::string GetRocprofMemoryCopyStreamFlowQuery();

private:
    ProfileDatabase* m_db;
};

}  // namespace DataModel
}  // namespace RocProfVis