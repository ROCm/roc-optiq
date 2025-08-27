// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include "rocprofvis_controller.h"
#include "rocprofvis_controller_data.h"
#include "rocprofvis_controller_handle.h"
#include "rocprofvis_controller_job_system.h"
#include "rocprofvis_c_interface.h"

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
    rocprofvis_result_t GetDouble(rocprofvis_property_t property, uint64_t index, double* value) final;
    rocprofvis_result_t GetObject(rocprofvis_property_t property, uint64_t index, rocprofvis_handle_t** value) final;
    rocprofvis_result_t GetString(rocprofvis_property_t property, uint64_t index, char* value, uint32_t* length) final;

    rocprofvis_result_t SetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t value) final;
    rocprofvis_result_t SetDouble(rocprofvis_property_t property, uint64_t index, double value) final;
    rocprofvis_result_t SetObject(rocprofvis_property_t property, uint64_t index, rocprofvis_handle_t* value) final;
    rocprofvis_result_t SetString(rocprofvis_property_t property, uint64_t index, char const* value, uint32_t length) final;

private:
    Job* m_job;
    Data m_object;
    std::vector<rocprofvis_db_future_t> m_db_futures;
    std::atomic<bool>                   m_cancelled;
    std::mutex                          m_mutex;

};

}
}
