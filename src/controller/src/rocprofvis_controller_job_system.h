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

typedef std::function<rocprofvis_result_t()> JobFunction;

class Job
{
public:
    Job(JobFunction function);
    ~Job();

    void Execute();
    void Cancel();
    rocprofvis_result_t GetResult() const;

private:
    JobFunction m_function;
    rocprofvis_result_t m_result;
};

class JobSystem
{
public:
    JobSystem();
    ~JobSystem();

    rocprofvis_result_t EnqueueJob(Job* job);
    rocprofvis_result_t CancelJob(Job* job);

private:
    std::vector<std::thread> m_workers;
    std::vector<Job*> m_jobs;
    std::mutex m_queue_mutex;
    std::condition_variable m_condition_variable;
    std::atomic<bool> m_terminate;
};

}
}
