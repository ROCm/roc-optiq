// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include "rocprofvis_controller.h"

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace RocProfVis
{
namespace Controller
{

class Future;

typedef std::function<rocprofvis_result_t(Future*)> JobFunction;

class Job
{
public:
    Job(JobFunction function, Future* future);
    ~Job();

    void Execute();
    void Cancel();
    rocprofvis_result_t GetResult() const;
    rocprofvis_result_t Wait(float timeout);

private:
    JobFunction m_function;
    std::mutex m_mutex;
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
