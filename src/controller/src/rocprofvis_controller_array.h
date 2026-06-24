// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_controller.h"
#include "rocprofvis_controller_handle.h"
#include "rocprofvis_controller_data.h"
#include <vector>

namespace RocProfVis
{
namespace Controller
{

class Trace;

class Array : public Handle
{
public:
    Array();
    virtual ~Array();

    std::vector<Data>& GetVector(void);

    void SetContext(Handle* ctx);
    Handle* GetContext(void) override;

    rocprofvis_controller_object_type_t GetType(void) final;

    // Handlers for getters.
    rocprofvis_result_t GetUInt64(rocprofvis_property_t property, uint64_t index,
                                  uint64_t* value) final;
    rocprofvis_result_t GetDouble(rocprofvis_property_t property, uint64_t index,
                                  double* value) final;
    rocprofvis_result_t GetObject(rocprofvis_property_t property, uint64_t index,
                                  rocprofvis_handle_t** value) final;
    rocprofvis_result_t GetString(rocprofvis_property_t property, uint64_t index,
                                  char* value, uint32_t* length) final;

    rocprofvis_result_t SetUInt64(rocprofvis_property_t property, uint64_t index,
                                  uint64_t value) final;
    rocprofvis_result_t SetDouble(rocprofvis_property_t property, uint64_t index,
                                  double value) final;
    rocprofvis_result_t SetObject(rocprofvis_property_t property, uint64_t index,
                                  rocprofvis_handle_t* value) final;

    // Like SetObject(), but the Array takes ownership of the entry: the object
    // is deleted when this Array is destroyed (via rocprofvis_controller_array_free).
    // Use for heap children whose lifetime is bound to this Array, such as the
    // per-row Arrays produced by table fetches.
    rocprofvis_result_t SetOwnedObject(rocprofvis_property_t property, uint64_t index,
                                       rocprofvis_handle_t* value);
    rocprofvis_result_t SetString(rocprofvis_property_t property, uint64_t index,
                                  char const* value) final;

private:
    std::vector<Data> m_array;
    Trace*            m_ctx;
};

}
}
