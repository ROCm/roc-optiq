// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_db_future.h"
#include "rocprofvis_db.h"
#include "rocprofvis_c_interface.h"

namespace RocProfVis
{
namespace DataModel
{

Future::Future(rocprofvis_db_progress_callback_t progress_callback, void* user_data):
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

Future::~Future(){
    if (m_worker.joinable())
    {
        m_interrupt_status = true;
        m_worker.join();
    }
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


void
Future::SetInterrupted()
{
    std::unique_lock lock(m_mutex);
    if (m_sub_futures.size() > 0)
    {
        for (auto future : m_sub_futures)
        {
            future->SetInterrupted();
        }
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
        spdlog::debug("Timeout expired!");
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
    if (m_progress_callback) m_progress_callback(db_name, (rocprofvis_db_progress_percent_t)m_progress, status, action, m_user_data);
}

}  // namespace DataModel
}  // namespace RocProfVis