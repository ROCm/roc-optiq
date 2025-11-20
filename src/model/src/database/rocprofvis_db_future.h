// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

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

        void                                SetAsyncQuery(std::string query) { m_async_query = query; }

        const char*                         GetAsyncQueryPtr(){return m_async_query.c_str(); }

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
        std::mutex            m_mutex;
        std::string           m_async_query;
};

}  // namespace DataModel
}  // namespace RocProfVis