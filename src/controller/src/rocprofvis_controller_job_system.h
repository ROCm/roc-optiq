// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_controller.h"

#include <atomic>
#include <condition_variable>
#include <exception>
#include <functional>
#include <mutex>
#include <new>
#include <thread>
#include <vector>

namespace RocProfVis
{
namespace Controller
{

class Future;

typedef std::function<rocprofvis_result_t(Future*)> JobFunction;

// Log the exception caught by CallCatchingExceptions. Defined out of line so this
// header doesn't need spdlog.
void LogOutOfMemoryException();
void LogStandardException(const char* what);
void LogUnknownException();

// Calls function and turns any exception it throws into a failed result, so the
// exception can't terminate a worker thread or skip the caller's cleanup.
// Taking the callable by reference, not as a std::function, means passing it
// can't allocate and so can't throw before the try.
template <typename Function>
rocprofvis_result_t CallCatchingExceptions(Function&& function)
{
    rocprofvis_result_t result = kRocProfVisResultUnknownError;
    try
    {
        result = function();
    }
    catch(const std::bad_alloc&)
    {
        LogOutOfMemoryException();
        result = kRocProfVisResultMemoryAllocError;
    }
    catch(const std::exception& e)
    {
        LogStandardException(e.what());
    }
    catch(...)
    {
        LogUnknownException();
    }
    return result;
}

class Job
{
public:
    Job(JobFunction function, Future* future);
    ~Job();

    void Execute();
    void Cancel();
    // Complete a job that is not run by JobSystem (e.g. the Python
    // interpreter thread). No-op if the job already left Pending.
    void Complete(rocprofvis_result_t result);
    rocprofvis_result_t GetResult() const;
    rocprofvis_result_t Wait(float timeout);

private:
    JobFunction m_function;
    mutable std::mutex m_mutex;
    std::condition_variable m_condition_variable;
    rocprofvis_result_t m_result;
    Future* m_future;
};

class JobSystem
{
public:
    JobSystem();
    ~JobSystem();

    static JobSystem& Get();

    Job* IssueJob(JobFunction function, Future* future);
    rocprofvis_result_t CancelJob(Job* job);

private:
    rocprofvis_result_t EnqueueJob(Job* job);

private:
    std::vector<std::thread> m_workers;
    std::vector<Job*> m_jobs;
    std::mutex m_queue_mutex;
    std::condition_variable m_condition_variable;
    std::atomic<bool> m_terminate;
    static JobSystem s_self;
};

}
}
