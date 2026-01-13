// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_controller.h"
#include "rocprofvis_controller_data.h"
#include "rocprofvis_controller_handle.h"

namespace RocProfVis
{
namespace Controller
{

class Sample : public Handle
{
public:
    Sample(rocprofvis_controller_primitive_type_t type, uint64_t id, double timestamp);

    Sample& operator=(Sample&& other);

    virtual ~Sample();

    rocprofvis_controller_object_type_t GetType(void) override;

    // Handlers for getters.
    rocprofvis_result_t GetUInt64(rocprofvis_property_t property, uint64_t index,
                                  uint64_t* value) override;
    rocprofvis_result_t GetDouble(rocprofvis_property_t property, uint64_t index,
                                  double* value) override;
    rocprofvis_result_t GetObject(rocprofvis_property_t property, uint64_t index,
                                  rocprofvis_handle_t** value) override;
    rocprofvis_result_t GetString(rocprofvis_property_t property, uint64_t index,
                                  char* value, uint32_t* length) override;

    rocprofvis_result_t SetUInt64(rocprofvis_property_t property, uint64_t index,
                                  uint64_t value) override;
    rocprofvis_result_t SetDouble(rocprofvis_property_t property, uint64_t index,
                                  double value) override;
    rocprofvis_result_t SetObject(rocprofvis_property_t property, uint64_t index,
                                  rocprofvis_handle_t* value) override;
    rocprofvis_result_t SetString(rocprofvis_property_t property, uint64_t index,
                                  char const* value) override;

protected:
    double       m_data;
    double       m_timestamp;
    double       m_end_timestamp;
};

}
}
