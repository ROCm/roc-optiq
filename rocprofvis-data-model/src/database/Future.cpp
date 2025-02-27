// MIT License
//
// Copyright (c) 2023 Advanced Micro Devices, Inc. All rights reserved.
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

#include "Future.h"

Future::Future() {
    m_future=m_promise.get_future();
    m_stop=false;
}

rocprofvis_dm_result_t Future::WaitForCompletion(uint32_t timeout_ms) {
    rocprofvis_dm_result_t result = kRocProfVisDmResultTimeout;
    auto status = m_future.wait_for(std::chrono::milliseconds(timeout_ms));
    if (status == std::future_status::ready) 
        result = kRocProfVisDmResultSuccess;
    m_stop = true;
    m_worker.join();
    return m_future.get();
}

rocprofvis_dm_result_t Future::SetPromise(rocprofvis_dm_result_t status) {
    m_promise.set_value(status);
    return status;
}

bool Future::Stopped() {
    return m_stop;
}
