// Copyright (c) 2025 Advanced Micro Devices, Inc. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "rocprofvis_db_future.h"
#include "rocprofvis_db.h"

namespace RocProfVis
{
namespace DataModel
{

Future::Future(rocprofvis_db_progress_callback_t progress_callback):
                                m_progress_callback(progress_callback),
                                m_interrupt_status(false),
                                m_progress(0.0),
                                m_processed_rows(0), 
                                m_db(nullptr), 
                                m_connection(nullptr)
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

void
Future::SetInterrupted()
{
    if (m_db != nullptr && m_connection != nullptr)
    {
        m_db->InterruptQuery(m_connection);
    }
    m_interrupt_status = true;
};

void
Future::LinkDatabase(Database* db, void* connection)
{
    m_db = db;
    m_connection = connection;
}

rocprofvis_dm_result_t Future::WaitForCompletion(rocprofvis_db_timeout_ms_t timeout_ms) {
    rocprofvis_dm_result_t result = kRocProfVisDmResultTimeout;
    std::future_status     status;
    m_processed_rows = 0;
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
    if (m_progress_callback) m_progress_callback(db_name, (int)m_progress, status, action);
}

}  // namespace DataModel
}  // namespace RocProfVis