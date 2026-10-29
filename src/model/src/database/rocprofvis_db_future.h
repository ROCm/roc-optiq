// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_common_types.h"
#include <array>
#include <atomic>
#include <future>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <variant>
#include <vector>
#ifdef ROCPROFVIS_PROFILER_HUB_ENABLED
#include <profiler-hub/c/profiler_hub.h>
#include <profiler-hub/c/profiler_hub_types.h>
#endif

namespace RocProfVis
{
namespace DataModel
{

using RuntimeValue = std::variant<uint64_t, double, std::string>;

typedef enum  rocprofvis_db_future_runtime_storage_t
{
    kRPVFutureStorageSampleValue,
    kRPVFutureStorageEventId,
    kRPVFutureStorageTrackId,

    kRPVFutureRuntimeStorageSize
}rocprofvis_db_future_runtime_storage_t;

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
        // @return unique identifier for this future
        uint64_t                            Id() const { return m_id; }
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

        std::vector<Future*>&               GetSubFutures() { return m_sub_futures; }
        template <typename T> 
        void                                SetRuntimeStorageValue(rocprofvis_db_future_runtime_storage_t key, T&& value) 
        {
            static_assert(std::is_same_v<std::decay_t<T>, uint64_t> || 
                std::is_same_v<std::decay_t<T>, double> ||
                std::is_same_v<std::decay_t<T>, std::string>,
                "Unsupported type!");

            m_runtime_storage[static_cast<size_t>(key)] = std::forward<T>(value);
        }

        template <typename T> 
        T                                   GetRuntimeStorageValue(rocprofvis_db_future_runtime_storage_t key, T fallback) const 
        {
            if (auto p = std::get_if<T>(&m_runtime_storage[static_cast<size_t>(key)]))
            {
                return *p;
            } 
            return fallback;
        }

        void                               ResetRowCount() { m_processed_rows = 0; }

        Future*                             AddSubFuture();
        void                                DeleteSubFuture(Future* sub_future);
        rocprofvis_dm_result_t              WaitAndDeleteSubFuture(Future* sub_future);

#ifdef ROCPROFVIS_PROFILER_HUB_ENABLED
        // Creates a profiler-hub future whose progress is reported through this future.
        // @param db_name - trace path forwarded with progress reports. May be null.
        // @return the hub future, or null if it could not be created
        ph_future_t                         AddProfilerHubFuture(rocprofvis_dm_charptr_t db_name = nullptr);
        // Unregisters and frees a hub future created by AddProfilerHubFuture().
        void                                DeleteProfilerHubFuture(ph_future_t future);
        // Waits for the hub operation, frees the hub future, and returns its outcome.
        rocprofvis_dm_result_t              WaitAndDeleteProfilerHubFuture(ph_future_t future);
        // Profiler-hub progress callback. Forwards a percent and a copied description.
        static void                         ProfilerHubOnProgress(ph_future_t future, double value, ph_progress_description_t description);
#endif

    private:
        static std::atomic<uint64_t> s_counter;
        // unique identifier for this future
        uint64_t m_id;
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
        std::vector<Future*>  m_sub_futures;
        std::array<RuntimeValue, static_cast<size_t>(kRPVFutureRuntimeStorageSize)> m_runtime_storage;

#ifdef ROCPROFVIS_PROFILER_HUB_ENABLED
        std::vector<ph_future_t> m_ph_futures;
        std::string           m_progress_db_name;
        static std::unordered_map<ph_future_t, Future*> s_ph_future_lookup;
        static std::mutex     s_mutex;
#endif
};

}  // namespace DataModel
}  // namespace RocProfVis