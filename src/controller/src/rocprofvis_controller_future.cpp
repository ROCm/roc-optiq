// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_controller_future.h"
#include "rocprofvis_core_assert.h"

#include <algorithm>
#include <cfloat>

namespace RocProfVis
{
namespace Controller
{

Future::Future()
: Handle(__kRPVControllerFuturePropertiesFirst, __kRPVControllerFuturePropertiesLast)
, m_job(nullptr)
, m_cancelled(false)
, m_progress_percentage(0)
{}

Future::~Future()
{
    delete m_job;
}

rocprofvis_controller_object_type_t Future::GetType(void)
{
    return kRPVControllerObjectTypeFuture;
}

void Future::Set(Job* job)
{
    ROCPROFVIS_ASSERT(job);
    m_job = job;
}

bool Future::IsValid() const
{
    return m_job;
}

rocprofvis_result_t Future::Wait(float timeout)
{
    rocprofvis_result_t result = kRocProfVisResultUnknownError;
    if (m_job)
    {
        result = m_job->Wait(timeout);
    }
    return result;
}

rocprofvis_result_t Future::Cancel()
{
    rocprofvis_result_t result = kRocProfVisResultUnknownError;
    if(m_job)
    {
        result = JobSystem::Get().CancelJob(m_job);
        m_cancelled = true;
        {
            std::unique_lock lock(m_mutex);
            if(m_db_futures.size() > 0)
            {
                for(auto future : m_db_futures)
                {
                    rocprofvis_db_future_cancel(future);
                }
            }
        }
    }
    return result;
}

bool Future::IsCancelled() {
    return m_cancelled;
}

rocprofvis_result_t
Future::AddDependentFuture(rocprofvis_db_future_t db_future)
{
    std::unique_lock lock(m_mutex);
    m_db_futures.push_back(db_future);
    return kRocProfVisResultSuccess;
}

rocprofvis_result_t
Future::RemoveDependentFuture(rocprofvis_db_future_t db_future)
{
    std::unique_lock lock(m_mutex);
    auto it = std::find(m_db_futures.begin(), m_db_futures.end(), db_future);
    if (it != m_db_futures.end())
    {
        m_db_futures.erase(it);
        return kRocProfVisResultSuccess;
    }
    return kRocProfVisResultNotLoaded;
}

rocprofvis_result_t Future::GetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t* value)
{
    (void) index;
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if (value)
    {
        switch (property)
        {
            case kRPVControllerCommonMemoryUsageInclusive:
            case kRPVControllerCommonMemoryUsageExclusive:
            {
                *value = sizeof(Future);
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerFutureResult:
            {
                if(m_job)
                {
                    *value = m_job->GetResult();
                    result = kRocProfVisResultSuccess;
                }
                else
                {
                    result = kRocProfVisResultUnknownError;
                }
                break;
            }
            case kRPVControllerFutureProgressPercentage:
            {
                std::lock_guard lock(m_mutex);
                *value = m_progress_percentage;
                result = kRocProfVisResultSuccess;
                break;
            }
            default:
            {
                result = UnhandledProperty(property);
                break;
            }
        }
    }
    return result;
}

rocprofvis_result_t Future::GetString(rocprofvis_property_t property, uint64_t index, char* value, uint32_t* length)
{
    (void) index;
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerFutureProgressMessage:
        {
            std::lock_guard lock(m_mutex);
            result = GetStdStringImpl(value, length, m_progress_message);
            break;
        }
        default:
        {
            result = UnhandledProperty(property);
            break;
        }
    }
    return result;
}

void Future::ResetProgress()
{
    std::lock_guard lock(m_mutex);
    m_progress_map.clear();
    m_progress_percentage = 0;
}

void Future::ProgressCallback(rocprofvis_db_filename_t db_filename, rocprofvis_db_future_id_t db_future_id, rocprofvis_db_progress_percent_t progress_percent, rocprofvis_db_status_t status, rocprofvis_db_status_message_t message, void* user_data)
{
    (void) db_filename;
    (void) status;
    ROCPROFVIS_ASSERT(user_data);
    Future* future = (Future*)user_data;
    if(future)
    {
        std::lock_guard lock(future->m_mutex);
        future->m_progress_map[db_future_id] = progress_percent;
        future->m_progress_percentage = 0;
        for (const std::pair<const uint64_t, uint16_t>& progress : future->m_progress_map)
        {
            future->m_progress_percentage += progress.second;
        }
        future->m_progress_percentage /= future->m_progress_map.size();
        future->m_progress_message = message ? message : "";
    }
}

}
}
