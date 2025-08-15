// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_controller_job_system.h"
#include "rocprofvis_controller_future.h"
#include "rocprofvis_core_assert.h"
#include <cfloat>

namespace RocProfVis
{
namespace Controller
{

Job::Job(JobFunction function, Future* future)
: m_function(function)
, m_result(kRocProfVisResultPending)
, m_future(future)
{

}

Job::~Job()
{

}

void Job::Execute()
{
    m_result = m_function(m_future);
    m_condition_variable.notify_all();
}

void Job::Cancel()
{
    m_result = kRocProfVisResultCancelled;
    m_condition_variable.notify_all();
}

rocprofvis_result_t Job::GetResult() const
{
    return m_result;
}

rocprofvis_result_t Job::Wait(float timeout)
{
    rocprofvis_result_t result = kRocProfVisResultUnknownError;
    if(m_result == kRocProfVisResultPending)
    {
        if(timeout == FLT_MAX)
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_condition_variable.wait(lock);
            result = kRocProfVisResultSuccess;
        }
        else
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            std::chrono::microseconds    time =
                std::chrono::microseconds(uint64_t(timeout * 1000000.0));
            std::cv_status status = m_condition_variable.wait_for(lock, time);
            if(status == std::cv_status::timeout)
            {
                result = kRocProfVisResultTimeout;
            }
            else
            {
                result = kRocProfVisResultSuccess;
            }
        }
    }
    else
    {
        result = kRocProfVisResultSuccess;
    }
    return result;
}

JobSystem JobSystem::s_self;

JobSystem::JobSystem()
: m_terminate(false)
{
    size_t thread_count = std::thread::hardware_concurrency();
    for (size_t i = 0; i < thread_count; ++i)
    {
        m_workers.emplace_back([this]()
        {
            while (true)
            {
                Job* job = nullptr;
                {
                    std::unique_lock<std::mutex> lock(m_queue_mutex);
                    m_condition_variable.wait(lock, [this]()
                    { 
                        return m_terminate || !m_jobs.empty(); 
                    });
                    
                    if (m_terminate && m_jobs.empty())
                    {
                        break;
                    }
                    else
                    {
                        job = m_jobs.front();
                        m_jobs.erase(m_jobs.begin());
                    }
                }
                if (job)
                {
                    job->Execute();
                }
            }
        });
    }
}

JobSystem::~JobSystem()
{
    {
        std::unique_lock<std::mutex> lock(m_queue_mutex);
        while(!m_jobs.empty())
        {
            m_jobs.front()->Cancel();
            m_jobs.erase(m_jobs.begin());
        }
        m_terminate = true;
    }
    m_condition_variable.notify_all();
    for (std::thread &worker : m_workers)
    {
        worker.join();
    }
}

JobSystem& JobSystem::Get()
{
    return s_self;
}

Job* JobSystem::IssueJob(JobFunction function, Future* future)
{
    Job* job = nullptr;
    try
    {
        job = new Job(function, future);
        if(EnqueueJob(job) != kRocProfVisResultSuccess)
        {
            spdlog::error("Failed to enqueue job");
            delete job;
            job = nullptr;
        }
    }
    catch (std::exception)
    {
        spdlog::error("Failed to allocate job");
    }
    ROCPROFVIS_ASSERT(job);
    return job;
}

rocprofvis_result_t JobSystem::EnqueueJob(Job* job)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if (job)
    {
        {
            std::unique_lock<std::mutex> lock(m_queue_mutex);
            m_jobs.push_back(job);
        }
        m_condition_variable.notify_one();
        result = kRocProfVisResultSuccess;
    }
    return result;
}

rocprofvis_result_t JobSystem::CancelJob(Job* job)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if (job)
    {
        std::unique_lock<std::mutex> lock(m_queue_mutex);
        auto it = std::find(m_jobs.begin(), m_jobs.end(), job);
        if (it != m_jobs.end())
        {
            (*it)->Cancel();
            m_jobs.erase(it);
            result = kRocProfVisResultSuccess;
        }
        else
        {
            result = kRocProfVisResultNotSupported;
        }
    }
    return result;
}

}
}
