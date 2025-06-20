// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_controller_job_system.h"

namespace RocProfVis
{
namespace Controller
{

Job::Job(JobFunction function)
: m_function(function)
, m_result(kRocProfVisResultUnknownError)
{

}

Job::~Job()
{

}

void Job::Execute()
{
    m_result = m_function();
}

void Job::Cancel()
{
    m_result = kRocProfVisResultCancelled;
}

rocprofvis_result_t Job::GetResult() const
{
    return m_result;
}

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

rocprofvis_result_t JobSystem::EnqueueJob(Job* job)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if (job)
    {
        std::unique_lock<std::mutex> lock(m_queue_mutex);
        m_jobs.push_back(job);
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
