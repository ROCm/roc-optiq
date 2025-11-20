// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_core_profile.h"
#include "rocprofvis_core.h"

namespace RocProfVis
{
namespace Core
{

#ifdef TEST

TimeRecorder::TimeRecorder(const char* function) : m_function(function) {
    m_start_time = std::chrono::steady_clock::now();
}

TimeRecorder::TimeRecorder(const char* function, const char* property, uint64_t index):
        TimeRecorder(function) {
            if (property!=nullptr)
            {
                m_function+="(";
                m_function+=property;
                m_function+=",";
                m_function+=std::to_string(index);
                m_function+=")";
            }
        }

TimeRecorder::~TimeRecorder() {
    auto t = std::chrono::steady_clock::now();
    std::chrono::duration<double> diff = t - m_start_time;
    spdlog::info("{0:13.9f} seconds | {1}", diff.count(), m_function.c_str());   
}

#endif

}  // namespace Core
}  // namespace RocProfVis