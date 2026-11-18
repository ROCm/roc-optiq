// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_db_future.h"
#include "rocprofvis_db.h"
#include "rocprofvis_c_interface.h"

#include <algorithm>

namespace RocProfVis
{
namespace DataModel
{

std::atomic<uint64_t> Future::s_counter{0};
#ifdef ROCPROFVIS_PROFILER_HUB_ENABLED
std::unordered_map<ph_future_t, Future*> Future::s_ph_future_lookup;
std::mutex Future::s_mutex;

namespace
{

rocprofvis_dm_result_t
profiler_hub_result(ph_result_t result)
{
    switch(result)
    {
        case PH_RESULT_SUCCESS: return kRocProfVisDmResultSuccess;
        case PH_RESULT_CANCELLED: return kRocProfVisDmResultDbAbort;
        case PH_RESULT_INVALID_ARGUMENT: [[fallthrough]];
        case PH_RESULT_INVALID_CONTEXT: return kRocProfVisDmResultInvalidParameter;
        case PH_RESULT_CONTEXT_ALLOCATION_FAILED: [[fallthrough]];
        case PH_RESULT_FUTURE_ALLOCATION_FAILED: return kRocProfVisDmResultAllocFailure;
        case PH_RESULT_UNSUPPORTED_FILE: return kRocProfVisDmResultNotSupported;
        case PH_RESULT_INTERNAL_ERROR: break;
    }
    return kRocProfVisDmResultUnknownError;
}

}
#endif

Future::Future(rocprofvis_db_progress_callback_t progress_callback, void* user_data):
                                m_id(s_counter++),
                                m_progress_callback(progress_callback),
                                m_interrupt_status(false),
                                m_progress(0.0),
                                m_processed_rows(0), 
                                m_db(nullptr), 
                                m_connection(nullptr), 
                                m_user_data(user_data)
{
    m_future=m_promise.get_future();
}

Future::~Future()
{
    SetInterrupted();
    if (m_worker.joinable())
    {
        m_worker.join();
    }

#ifdef ROCPROFVIS_PROFILER_HUB_ENABLED
    std::vector<ph_future_t> hub_futures;
    {
        std::unique_lock lock(m_mutex);
        hub_futures.swap(m_ph_futures);
    }
    for (ph_future_t hub_future : hub_futures)
    {
        ph_future_wait(hub_future);
        {
            std::unique_lock lock(s_mutex);
            s_ph_future_lookup.erase(hub_future);
        }
        ph_future_free(hub_future);
    }
#endif
}

Future* Future::AddSubFuture() {
    Future* sub_feature = (Future*)rocprofvis_db_future_alloc(nullptr);
    std::unique_lock lock(m_mutex);
    m_sub_futures.push_back(sub_feature);
    return sub_feature;
}

void   
Future::DeleteSubFuture(Future* sub_future) {

    std::unique_lock lock(m_mutex);
    auto it = std::find_if(m_sub_futures.begin(), m_sub_futures.end(), [&](Future* f) { return f == sub_future; });
    if (it != m_sub_futures.end())
    {
        m_sub_futures.erase(it);
        rocprofvis_db_future_free(sub_future);
    }
}

rocprofvis_dm_result_t   
Future::WaitAndDeleteSubFuture(Future* sub_future) {
    rocprofvis_dm_result_t result = kRocProfVisDmResultSuccess;
    if(kRocProfVisDmResultSuccess !=
        rocprofvis_db_future_wait(sub_future, UINT64_MAX))
    {
        result = kRocProfVisDmResultUnknownError;
    }
    DeleteSubFuture(sub_future);
    return result;
}

#ifdef ROCPROFVIS_PROFILER_HUB_ENABLED
void Future::ProfilerHubOnProgress(ph_future_t future, double value, ph_progress_description_t description)
{
    Future* parent = nullptr;
    {
        std::unique_lock lock(s_mutex);
        const auto it = s_ph_future_lookup.find(future);
        if (it == s_ph_future_lookup.end())
        {
            return;
        }
        parent = it->second;
    }

    const std::string message = description == nullptr ? std::string() : description;
    std::string db_name;
    rocprofvis_db_progress_callback_t callback = nullptr;
    void* user_data = nullptr;
    uint64_t id = 0;
    {
        std::unique_lock lock(parent->m_mutex);
        db_name = parent->m_progress_db_name;
        callback = parent->m_progress_callback;
        user_data = parent->m_user_data;
        id = parent->m_id;
    }

    const double percent = value * 100.0;
    parent->m_progress = percent;
    if (callback != nullptr)
    {
        callback(db_name.c_str(),
                 id,
                 static_cast<rocprofvis_db_progress_percent_t>(percent),
                 kRPVDbBusy,
                 message.c_str(),
                 user_data);
    }
}

ph_future_t Future::AddProfilerHubFuture(rocprofvis_dm_charptr_t db_name)
{
    ph_future_t future = nullptr;
    if (ph_future_create(ProfilerHubOnProgress, nullptr, &future) != PH_RESULT_SUCCESS ||
        future == nullptr)
    {
        return nullptr;
    }
    {
        std::unique_lock lock(m_mutex);
        if (db_name != nullptr)
        {
            m_progress_db_name = db_name;
        }
        m_ph_futures.push_back(future);
    }
    {
        std::unique_lock lock(s_mutex);
        s_ph_future_lookup[future] = this;
    }
    return future;
}

void Future::DeleteProfilerHubFuture(ph_future_t future)
{
    if (future == nullptr)
    {
        return;
    }

    bool owned = false;
    {
        std::unique_lock lock(m_mutex);
        const auto it = std::find(m_ph_futures.begin(), m_ph_futures.end(), future);
        if (it != m_ph_futures.end())
        {
            m_ph_futures.erase(it);
            owned = true;
        }
    }
    if (!owned)
    {
        return;
    }

    {
        std::unique_lock lock(s_mutex);
        s_ph_future_lookup.erase(future);
    }
    ph_future_free(future);
}

rocprofvis_dm_result_t Future::WaitAndDeleteProfilerHubFuture(ph_future_t future)
{
    rocprofvis_dm_result_t result = kRocProfVisDmResultInvalidParameter;
    if (future != nullptr)
    {
        const ph_result_t waited = ph_future_wait(future);
        if (waited == PH_RESULT_SUCCESS)
        {
            ph_result_t hub_result = PH_RESULT_INTERNAL_ERROR;
            if (ph_future_result(future, &hub_result) == PH_RESULT_SUCCESS)
            {
                result = profiler_hub_result(hub_result);
            }
            else
            {
                result = kRocProfVisDmResultUnknownError;
            }
        }
        else
        {
            result = profiler_hub_result(waited);
        }
        DeleteProfilerHubFuture(future);
    }
    return result;
}
#endif

void
Future::SetInterrupted()
{
    std::unique_lock lock(m_mutex);
#ifdef ROCPROFVIS_PROFILER_HUB_ENABLED
    for (auto ph_future : m_ph_futures)
    {
        ph_future_cancel(ph_future);
    }
#endif

    for (auto future : m_sub_futures)
    {
        future->SetInterrupted();
    }

    if (m_db != nullptr && m_connection != nullptr)
    {
        m_db->InterruptQuery(m_connection);
    }
    m_interrupt_status = true;
};

void
Future::LinkDatabase(Database* db, void* connection)
{
    std::unique_lock lock(m_mutex);
    m_db = db;
    m_connection = connection;
}

rocprofvis_dm_result_t Future::WaitForCompletion(rocprofvis_db_timeout_ms_t timeout_ms) {
    rocprofvis_dm_result_t result = kRocProfVisDmResultTimeout;
    std::future_status     status;
    if(timeout_ms == UINT64_MAX)
    {
        m_future.wait();
        status = std::future_status::ready;
    }
    else
    {
        status = m_future.wait_for(std::chrono::milliseconds(timeout_ms));
    }
    if (status != std::future_status::ready) {
        if(timeout_ms == 0) return result;
        m_interrupt_status = true;
        spdlog::warn("Timeout expired!");
    }
    if (m_worker.joinable()) m_worker.join();
    result = m_future.get();
    m_promise = std::promise<rocprofvis_dm_result_t>();
    m_future=m_promise.get_future();
    m_progress=0.0;
    m_interrupt_status=false;
    return result;
}

rocprofvis_dm_result_t Future::SetPromise(rocprofvis_dm_result_t status) {
    m_promise.set_value(status);
    return status;
}

void Future::ShowProgress(rocprofvis_dm_charptr_t db_name, double step, rocprofvis_dm_charptr_t action, rocprofvis_db_status_t status){
    m_progress = m_progress+step; 
    if (m_progress_callback) m_progress_callback(db_name, m_id, (rocprofvis_db_progress_percent_t)m_progress, status, action, m_user_data);
}

}  // namespace DataModel
}  // namespace RocProfVis