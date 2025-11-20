// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_controller.h"
#include "rocprofvis_controller_handle.h"
#include <string>

namespace RocProfVis
{
namespace Controller
{

class Node;
class Process;
class Track;

class Thread : public Handle
{
public:
    Thread();
    virtual ~Thread();

    rocprofvis_controller_object_type_t GetType(void) override;

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
    rocprofvis_result_t SetString(rocprofvis_property_t property, uint64_t index,
                                  char const* value) final;

private:
    std::string                         m_name;
    std::string                         m_ext_data;
    Node*                               m_node;
    Process*                            m_process;
    Track*                              m_track;
    double                              m_start_time;
    double                              m_end_time;
    uint32_t                            m_id;
    uint32_t                            m_tid;
    uint32_t                            m_parent_id;
    rocprofvis_controller_thread_type_t m_thread_type;
};

}  // namespace Controller
}  // namespace RocProfVis
