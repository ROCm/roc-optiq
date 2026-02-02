// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_controller_handle.h"
#include "rocprofvis_c_interface_types.h"
#include <string>
#include <vector>

namespace RocProfVis
{
namespace Controller
{

class Kernel;
class Roofline;

class Workload : public Handle
{
public:
    Workload();

    virtual ~Workload();

    rocprofvis_controller_object_type_t GetType(void) final;

    rocprofvis_result_t GetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t* value) final;
    rocprofvis_result_t GetString(rocprofvis_property_t property, uint64_t index, char* value, uint32_t* length) final;
    rocprofvis_result_t GetObject(rocprofvis_property_t property, uint64_t index, rocprofvis_handle_t** value) final;

    rocprofvis_result_t SetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t value) final;
    rocprofvis_result_t SetObject(rocprofvis_property_t property, uint64_t index, rocprofvis_handle_t* value) final;
    rocprofvis_result_t SetString(rocprofvis_property_t property, uint64_t index, char const* value) final;

    bool QueryToPropertyEnum(rocprofvis_db_compute_column_enum_t in, rocprofvis_property_t& property, rocprofvis_controller_primitive_type_t& type) const;

private:
    struct JsonData {
        // Vector to preserve order
        std::vector<std::string> keys;
        std::vector<std::string> values;
    };
    struct MetricDefinition {
        uint32_t category_id;
        uint32_t table_id;
        size_t category_name_idx;
        size_t table_name_idx;
        size_t name_idx;
        size_t description_idx;
        size_t unit_idx;
    };

    uint32_t m_id;
    std::string m_name;
    std::string m_sub_name;
    JsonData m_system_info;
    JsonData m_profiling_config;
    std::vector<MetricDefinition> m_available_metrics;
    std::vector<Kernel*> m_kernels;
    Roofline* m_roofline;
};

}
}
