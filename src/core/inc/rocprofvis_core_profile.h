// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT


#pragma once

#include "rocprofvis_core_assert.h"
#include <chrono>

namespace RocProfVis
{
namespace Core
{

// compile with -DTEST for profiling interface methods performace
#ifdef TEST

class TimeRecorder {
public:
    TimeRecorder(const char* function);
    TimeRecorder(const char* function, const char* property, uint64_t index);
    ~TimeRecorder();
private:
    std::chrono::time_point<std::chrono::steady_clock> m_start_time;
    std::string m_function;
};
#define PROFILE RocProfVis::Core::TimeRecorder time_recorder(__FUNCTION__)
#define PROFILE_PROP_ACCESS(property, index) RocProfVis::Core::TimeRecorder time_recorder(__FUNCTION__, property, index)
#else
#define PROFILE
#define PROFILE_PROP_ACCESS(property, index)
#endif

}  // namespace Core
}  // namespace RocProfVis