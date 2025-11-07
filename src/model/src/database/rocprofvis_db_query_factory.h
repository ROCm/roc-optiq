// Copyright (c) 2025 Advanced Micro Devices, Inc. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once

#include "rocprofvis_db.h"
#include <vector>

namespace RocProfVis
{
namespace DataModel
{

class SqliteDatabase;

class QueryFactory
{
public:
    QueryFactory(SqliteDatabase* db);

    void SetVersion(const char* version);

    bool IsVersionEqual(const char*);
    bool IsVersionGreaterOrEqual(const char*);

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

private:

    std::vector<uint32_t>  ConvertVersionStringToInt(const char* version);

    uint32_t GetMajorVersion()
    {
        return m_db_version.size() > 0 ? m_db_version[0] : 0;
    }
    uint32_t GetMinorVersion()
    {
        return m_db_version.size() > 1 ? m_db_version[1] : 0;
    }
    uint32_t GetPatchVersion()
    {
        return m_db_version.size() > 2 ? m_db_version[2] : 0;
    }

    SqliteDatabase* m_db;
    std::vector<uint32_t> m_db_version;
};


}  // namespace DataModel
}  // namespace RocProfVis