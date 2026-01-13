// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_controller.h"
#include "rocprofvis_controller_handle.h"
#include <string>
#include <vector>

namespace RocProfVis
{
namespace Controller
{

class Thread;
class Queue;
class Stream;
class Counter;

class Process : public Handle
{
public:
    Process();
    virtual ~Process();

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
    std::vector<Thread*> m_threads;
    std::vector<Queue*>  m_queues;
    std::vector<Stream*> m_streams;
    std::vector<Counter*> m_counters;
    std::string          m_command;
    std::string          m_environment;
    std::string          m_ext_data;
    double               m_init_time;
    double               m_finish_time;
    double               m_start_time;
    double               m_end_time;
    uint32_t             m_id;
    uint64_t             m_node_id;
    uint32_t             m_parent_id;
};

}
}
