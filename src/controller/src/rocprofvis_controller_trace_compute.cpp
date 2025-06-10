// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_controller_trace_compute.h"
#include "rocprofvis_controller_compute_metrics.h"
#include "rocprofvis_controller_table_compute.h"
#include "rocprofvis_core_assert.h"
#include <filesystem>

namespace RocProfVis
{
namespace Controller
{

ComputeTrace::ComputeTrace()
{

}

ComputeTrace::~ComputeTrace()
{
    for (auto& it : m_tables)
    {
        ComputeTable* table = it.second;
        if (table)
        {
            delete table;
        }
    }
}

rocprofvis_result_t ComputeTrace::Load(char const* const directory)
{
    rocprofvis_result_t result = kRocProfVisResultUnknownError;
    if (std::filesystem::exists(directory) && std::filesystem::is_directory(directory))
    {        
        for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator{directory})
        {
            if (entry.path().extension() == ".csv")
            {
                const std::string& file = entry.path().filename().string();
                if (COMPUTE_TABLE_DEFINITIONS.count(file) > 0)
                {
                    const ComputeTableDefinition& definition = COMPUTE_TABLE_DEFINITIONS.at(file);
                    ComputeTable* table = new ComputeTable(m_tables.size(), definition.m_type, definition.m_title);
                    ROCPROFVIS_ASSERT(table);
                    result = table->Load(entry.path().string());
                    ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
                    m_tables[definition.m_type] = table;
                    spdlog::info("ComputeTrace::Load - {}/{}", m_tables.size(), COMPUTE_TABLE_DEFINITIONS.size());
                }
            }
        }
    }
    return result;
}

rocprofvis_controller_object_type_t ComputeTrace::GetType(void) 
{
    return kRPVControllerObjectTypeComputeTrace;
}

rocprofvis_result_t ComputeTrace::GetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t* value) 
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    return result;
}

rocprofvis_result_t ComputeTrace::GetDouble(rocprofvis_property_t property, uint64_t index, double* value) 
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    return result;
}

rocprofvis_result_t ComputeTrace::GetObject(rocprofvis_property_t property, uint64_t index, rocprofvis_handle_t** value) 
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if (value)
    {
        if (kRPVControllerComputeTableTypeKernelList <= property && property < kRPVControllerComputeTableTypeCount)
        {
            rocprofvis_controller_compute_table_types_t table_type = static_cast<rocprofvis_controller_compute_table_types_t>(property);
            if (m_tables.count(table_type) > 0)
            {
                *value = (rocprofvis_handle_t*)m_tables[static_cast<rocprofvis_controller_compute_table_types_t>(property)];
                result = kRocProfVisResultSuccess;
            }
            else
            {
                result = kRocProfVisResultNotLoaded;
            }          
        }
        else
        {
            result = kRocProfVisResultInvalidEnum;
        }
    }
    return result;
}


rocprofvis_result_t ComputeTrace::GetString(rocprofvis_property_t property, uint64_t index, char* value, uint32_t* length) 
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    return result;
}

rocprofvis_result_t ComputeTrace::SetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t value) 
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    return result;
}
rocprofvis_result_t ComputeTrace::SetDouble(rocprofvis_property_t property, uint64_t index, double value) 
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    return result;
}

rocprofvis_result_t ComputeTrace::SetObject(rocprofvis_property_t property, uint64_t index, rocprofvis_handle_t* value) 
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    return result;
}
rocprofvis_result_t ComputeTrace::SetString(rocprofvis_property_t property, uint64_t index, char const* value, uint32_t length) 
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    return result;
}

}
}
