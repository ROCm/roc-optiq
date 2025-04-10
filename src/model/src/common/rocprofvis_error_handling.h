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

#include "rocprofvis_core_assert.h"
#include <string>
#include <mutex>
#include <chrono>

namespace RocProfVis
{
namespace DataModel
{

// static methods set/get last error message
const char * GetLastStatusMessage();
void SetStatusMessage(const char*);
void AddStatusMessage(const char*);

// printf text colors
#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[92m"
#define ANSI_COLOR_YELLOW  "\x1b[93m"
#define ANSI_COLOR_BLUE    "\x1b[94m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"
#define ANSI_COLOR_GREY    "\x1b[90m"

// compile with -DTEST for profiling interface methods performace
#ifdef TEST

class TimeRecorder {
public:
    TimeRecorder(const char* function);
    TimeRecorder(const char* function, void* handle, uint32_t property, uint64_t index);
    ~TimeRecorder();
private:
    std::chrono::time_point<std::chrono::steady_clock> m_start_time;
    std::string m_function;
};
#define PROFILE RocProfVis::DataModel::TimeRecorder time_recorder(__FUNCTION__)
#define PROFILE_PROP_ACCESS RocProfVis::DataModel::TimeRecorder time_recorder(__FUNCTION__, handle, property, index)
#else
#define PROFILE
#define PROFILE_PROP_ACCESS
#endif

// Error strings
extern const char* ERROR_INDEX_OUT_OF_RANGE;
extern const char* ERROR_TRACE_CANNOT_BE_NULL;
extern const char* ERROR_TRACK_CANNOT_BE_NULL;
extern const char* ERROR_SLICE_CANNOT_BE_NULL;
extern const char* ERROR_DATABASE_CANNOT_BE_NULL;
extern const char* ERROR_TRACK_PARAMETERS_NOT_ASSIGNED;
extern const char* ERROR_VIRTUAL_METHOD_CALL;
extern const char* ERROR_FUTURE_CANNOT_BE_NULL;
extern const char* ERROR_FUTURE_CANNOT_BE_USED;
extern const char* ERROR_METADATA_IS_NOT_LOADED;
extern const char* ERROR_TRACE_PROPERTIES_CANNOT_BE_NULL;
extern const char* ERROR_DATABASE_QUERY_PARAMETERS_MISMATCH;
extern const char* ERROR_MEMORY_ALLOCATION_FAILURE;
extern const char* ERROR_VIRTUAL_METHOD_PROPERTY;
extern const char* ERROR_INVALID_PROPERTY_GETTER;
extern const char* ERROR_UNSUPPORTED_PROPERTY;
extern const char* ERROR_FLOW_TRACE_CANNOT_BE_NULL;
extern const char* ERROR_STACK_TRACE_CANNOT_BE_NULL;
extern const char* ERROR_TABLE_CANNOT_BE_NULL;
extern const char* ERROR_TABLE_ROW_CANNOT_BE_NULL;
extern const char* ERROR_EXT_DATA_CANNOT_BE_NULL;
extern const char* ERROR_SQL_QUERY_PARAMETERS_CANNOT_BE_NULL;
extern const char* ERROR_REFERENCE_POINTER_CANNOT_BE_NULL;

#define LOG(msg) RocProfVis::DataModel::SetStatusMessage(msg)
#define ADD_LOG(msg) RocProfVis::DataModel::AddStatusMessage(msg)

}  // namespace DataModel
}  // namespace RocProfVis