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
#ifndef RPV_DATABASE_FUTURE_H
#define RPV_DATABASE_FUTURE_H

#include "../common/CommonTypes.h"
#include <future>
#include <thread>

class Future
{
    public:
        Future(rocprofvis_db_progress_callback_t progress_callback = nullptr);
        ~Future();

        rocprofvis_db_progress_callback_t   ProgressCallback() { return m_progress_callback;}
        double                              Progress() { return m_progress; }

        rocprofvis_dm_result_t WaitForCompletion(rocprofvis_db_timeout_ms_t timeout);

        rocprofvis_dm_result_t SetPromise(rocprofvis_dm_result_t status);
        void SetWorker(std::thread && thread) {m_worker = std::move(thread);};
        bool IsWorking() { return m_worker.joinable();}
        bool Interrupted() {return m_interrupt_status;};

        void ShowProgress(rocprofvis_dm_charptr_t db_name, double step, rocprofvis_dm_charptr_t action, rocprofvis_db_status_t status);

    private:
        std::promise<rocprofvis_dm_result_t> m_promise;
        std::future<rocprofvis_dm_result_t> m_future;
        std::atomic<bool> m_interrupt_status;
        rocprofvis_db_progress_callback_t m_progress_callback;
        double m_progress;
        std::thread m_worker;
};

#endif //RPV_DATABASE_FUTURE_H