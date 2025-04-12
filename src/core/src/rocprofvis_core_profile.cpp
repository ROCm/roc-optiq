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