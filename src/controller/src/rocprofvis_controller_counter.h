// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

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
class Processor;
class Track;

class Counter : public Handle
{
public:
    Counter();
    virtual ~Counter();

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
                                  char const* value, uint32_t length) final;

private:
    Node* m_node;
    Process* m_process;
    Processor* m_processor;
    Track* m_track;
    std::string m_name;
    std::string m_symbol;
    std::string m_description;
    std::string m_long_description;
    std::string m_component;
    std::string m_units;
    std::string m_value_type;
    std::string m_block;
    std::string m_expression;
    std::string m_guid;
    std::string m_extdata;
    std::string m_target_arch;
    uint32_t    m_id;
    uint32_t    m_event_code;
    uint32_t    m_instance_id;
    bool        m_is_constant;
    bool        m_is_derived;
};

}
}
