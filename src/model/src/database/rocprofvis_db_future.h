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

#include "rocprofvis_common_types.h"
#include <future>
#include <thread>

namespace RocProfVis
{
namespace DataModel
{

class Database;

// Asynchronous execution handler class
class Future
{
    public:
        // Future object constructor
        // @param progress_callback - optional progress callback pointer 
        Future(rocprofvis_db_progress_callback_t progress_callback, void* user_data = nullptr);
        // Future object destructor. Takes care of joining worker thread, if joinable
        ~Future();
        // @return callback method pointer
        rocprofvis_db_progress_callback_t   ProgressCallback() { return m_progress_callback;}
        // @return current progress in percents
        double                              Progress() { return m_progress; }
        // waits for thread completion for specified number of milliseconds. If timeouts, sends command to end the thread as soon as possible.
        // @param - timeout time in milliseconds
        // @return status of operation
        rocprofvis_dm_result_t              WaitForCompletion(rocprofvis_db_timeout_ms_t timeout);
        // fills promise object with operation status
        // @param status - status of operation
        // @return status of operation
        rocprofvis_dm_result_t              SetPromise(rocprofvis_dm_result_t status);
        // moves worker thread into the class object memory.  
        // @param thread - std::thread object allocated in caller code
        void                                SetWorker(std::thread && thread) {m_worker = std::move(thread);};
        // reports if thread is still working
        // @return True is thread is still running
        bool                                IsWorking() { return m_worker.joinable();}
        // reports if thread was interrupted by timeout logic
        // @return True if thread has been timeouted
        bool                                Interrupted() {return m_interrupt_status;};
        // sets interrupted flag
        void                                SetInterrupted();
        // calls progress callback, if provided
        // @param db_name - path to database file
        // @param step - progress percentage of single database operation
        // @param action - operation description
        // @param status - operation status
        void                                ShowProgress(
                                                        rocprofvis_dm_charptr_t db_name, 
                                                        double step, 
                                                        rocprofvis_dm_charptr_t action, 
                                                        rocprofvis_db_status_t status);
        // increases processed rows counter
        // the row counter is used for testing (data integrity validation) and detecting first row
        void 								CountThisRow() { m_processed_rows++; }
        // returns processed rows counter
        uint32_t 							GetProcessedRowsCount() { return m_processed_rows; }

        void                                LinkDatabase(Database* db, void* connection);

    private:
        // stdlib promise object
        std::promise<rocprofvis_dm_result_t> m_promise;
        // stdlib future object
        std::future<rocprofvis_dm_result_t> m_future;
        // interrupt status, gets set by timeout logic
        std::atomic<bool> m_interrupt_status;
        // progress callback method pointer 
        rocprofvis_db_progress_callback_t m_progress_callback;
        // progress value in percent
        std::atomic<double> m_progress;
        // worker thread
        std::thread m_worker;
        // number of rows processed by query
        std::atomic<uint32_t> m_processed_rows;
        Database*             m_db;
        void*                 m_connection;
        void*                 m_user_data;
};

}  // namespace DataModel
}  // namespace RocProfVis