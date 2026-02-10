// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_c_interface.h"
#include "rocprofvis_controller_data.h"
#include "rocprofvis_controller_handle.h"

namespace RocProfVis
{
namespace Controller
{

class CallStack : public Handle
{
public:
    CallStack(const char* file, const char* pc, const char* name, const char* line_name, const char* line_address);

    virtual ~CallStack();

    rocprofvis_controller_object_type_t GetType(void) override;

    // Handlers for getters.
    rocprofvis_result_t GetUInt64(rocprofvis_property_t property, uint64_t index,
                                  uint64_t* value) override;
    rocprofvis_result_t GetString(rocprofvis_property_t property, uint64_t index,
                                  char* value, uint32_t* length) override;

private:
    Data m_file;
    Data m_pc;
    Data m_name;
    Data m_line_name;
    Data m_line_address;
};

}
}
