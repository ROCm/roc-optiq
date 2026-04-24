// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_controller.h"
#include "rocprofvis_controller_data.h"
#include "rocprofvis_controller_handle.h"
#include "rocprofvis_controller_job_system.h"
#include "rocprofvis_c_interface.h"
#include <string>
#include <unordered_map>

namespace RocProfVis
{
namespace Controller
{

class Future : public Handle
{
public:
    Future();
    virtual ~Future();

    rocprofvis_controller_object_type_t GetType(void) final;

    void Set(Job* job);

    bool IsValid() const;

    rocprofvis_result_t Wait(float timeout);
    rocprofvis_result_t Cancel();
    bool                IsCancelled();
    rocprofvis_result_t AddDependentFuture(rocprofvis_db_future_t db_future);
    rocprofvis_result_t RemoveDependentFuture(rocprofvis_db_future_t db_future);

    // Handlers for getters.
    rocprofvis_result_t GetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t* value) final;
    rocprofvis_result_t GetString(rocprofvis_property_t property, uint64_t index, char* value, uint32_t* length) final;

    void ResetProgress();
    static void ProgressCallback(rocprofvis_db_filename_t db_filename, rocprofvis_db_future_id_t db_future_id, rocprofvis_db_progress_percent_t progress_percent, rocprofvis_db_status_t status, rocprofvis_db_status_message_t message, void* user_data);

private:
    Job* m_job;
    std::vector<rocprofvis_db_future_t> m_db_futures;
    std::atomic<bool> m_cancelled;
    std::mutex m_mutex;
    uint16_t m_progress_percentage;
    std::string m_progress_message;
    std::unordered_map<uint64_t, uint16_t> m_progress_map;
};

}
}
