// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_controller.h"
#include <functional>
#include <string_view>
#include <unordered_map>
#include <vector>

#ifdef __cplusplus
extern "C"
{
#endif

/*
* Calculates the queue utilization for the specified track within the given time range.
* @param controller The system trace controller instance.
* @param track The handle track to analyze.
* @param start_time The start time in ns of the analysis range.
* @param end_time The end time in ns of the analysis range.
* @param result The future object to store the result.
* @param output The output value to write the queue utilization.
* @returns kRocProfVisResultSuccess or an error code.
*/
rocprofvis_result_t rocprofvis_controller_analysis_fetch_queue_utilization(rocprofvis_controller_t* controller, rocprofvis_controller_track_t* track, double start_time, double end_time, rocprofvis_controller_future_t* result, double* output);

#ifdef __cplusplus
}
#endif

namespace RocProfVis
{
namespace Controller
{

class Trace;
class SystemTrace;
class Future;
class Track;

struct QueryDataStore
{
    std::unordered_map<std::string_view, uint64_t> columns;
    std::vector<std::vector<const char*>> rows;
};
typedef std::function<rocprofvis_result_t(const QueryDataStore&)> QueryCallback;

class Analysis
{
public:
    static Analysis& GetInstance();

    rocprofvis_result_t AsyncFetchQueueUtilization(SystemTrace* trace, Track* track, double start, double end, double* output, Future* future) const;

private:
    Analysis();
    ~Analysis();

    rocprofvis_result_t ExecuteQuery(Trace* trace, const char* query, const char* description, Future& future, QueryCallback callback) const;
};

}
}
