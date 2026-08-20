// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_controller_pc_sampling.h"

namespace RocProfVis
{
namespace Controller
{

PcSampling::PcSampling()
: Handle(__kRPVControllerPCSamplingPropertiesFirst, __kRPVControllerPCSamplingPropertiesLast)
{}

PcSampling::~PcSampling() {}

std::recursive_mutex& PcSampling::GetDataMutex()
{
    return m_data_mutex;
}

rocprofvis_controller_object_type_t PcSampling::GetType(void)
{
    return kRPVControllerObjectTypePCSampling;
}

rocprofvis_result_t PcSampling::GetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t* value)
{
    std::lock_guard<std::recursive_mutex> lock(m_data_mutex);
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            case kRPVControllerPCSamplingNumSourceFiles:
            {
                (void)index;
                *value = m_source_files.size();
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerPCSamplingSourceFileId:
            {
                if(index < m_source_files.size())
                {
                    *value = m_source_files[index].id;
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVControllerPCSamplingNumSourceLines:
            {
                (void)index;
                *value = m_source_lines.size();
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerPCSamplingSourceLineId:
            {
                if(index < m_source_lines.size())
                {
                    *value = m_source_lines[index].id;
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVControllerPCSamplingSourceLineSourceFileId:
            {
                if(index < m_source_lines.size())
                {
                    *value = m_source_lines[index].source_file_id;
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVControllerPCSamplingSourceLineNumber:
            {
                if(index < m_source_lines.size())
                {
                    *value = m_source_lines[index].line_number;
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVControllerPCSamplingNumCodeObjects:
            {
                (void)index;
                *value = m_code_object_store.size();
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerPCSamplingCodeObjectUuid:
            {
                if(index < m_code_object_store.size())
                {
                    *value = m_code_object_store[index].code_object_uuid;
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVControllerPCSamplingCodeObjectWorkloadId:
            {
                if(index < m_code_object_store.size())
                {
                    *value = m_code_object_store[index].workload_id;
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVControllerPCSamplingCodeObjectPid:
            {
                if(index < m_code_object_store.size())
                {
                    *value = m_code_object_store[index].pid;
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVControllerPCSamplingCodeObjectId:
            {
                if(index < m_code_object_store.size())
                {
                    *value = m_code_object_store[index].code_object_id;
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVControllerPCSamplingCodeObjectLoadBase:
            {
                if(index < m_code_object_store.size())
                {
                    *value = m_code_object_store[index].load_base;
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVControllerPCSamplingNumIsaLines:
            {
                (void)index;
                *value = m_isa_lines.size();
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerPCSamplingIsaLineId:
            {
                if(index < m_isa_lines.size())
                {
                    *value = m_isa_lines[index].instruction_uuid;
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVControllerPCSamplingIsaLineCodeObjectId:
            {
                if(index < m_isa_lines.size())
                {
                    *value = m_isa_lines[index].code_object_uuid;
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVControllerPCSamplingIsaLineKernelUuid:
            {
                if(index < m_isa_lines.size())
                {
                    *value = m_isa_lines[index].kernel_uuid;
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVControllerPCSamplingIsaLineCodeObjectOffset:
            {
                if(index < m_isa_lines.size())
                {
                    *value = m_isa_lines[index].code_object_offset;
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVControllerPCSamplingIsaLineInstructionTypeId:
            {
                if(index < m_isa_lines.size())
                {
                    // Kept for compatibility with the legacy controller API.
                    *value = 0;
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVControllerPCSamplingNumIsaToIsaDeps:
            {
                (void)index;
                *value = m_isa_to_isa_deps.size();
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerPCSamplingIsaToIsaDependentIsaLineId:
            {
                if(index < m_isa_to_isa_deps.size())
                {
                    *value = m_isa_to_isa_deps[index].dependent_isa_line_id;
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVControllerPCSamplingIsaToIsaDependencyIsaLineId:
            {
                if(index < m_isa_to_isa_deps.size())
                {
                    *value = m_isa_to_isa_deps[index].dependency_isa_line_id;
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVControllerPCSamplingNumIsaToSourceDeps:
            {
                (void)index;
                *value = m_isa_to_source_deps.size();
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerPCSamplingIsaToSourceIsaLineId:
            {
                if(index < m_isa_to_source_deps.size())
                {
                    *value = m_isa_to_source_deps[index].isa_line_id;
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVControllerPCSamplingIsaToSourceSourceLineId:
            {
                if(index < m_isa_to_source_deps.size())
                {
                    *value = m_isa_to_source_deps[index].source_line_id;
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVControllerPCSamplingIsaToSourceDepth:
            {
                if(index < m_isa_to_source_deps.size())
                {
                    *value = m_isa_to_source_deps[index].depth;
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVControllerPCSamplingNumSamplingStates:
            case kRPVControllerPCSamplingNumPcSampleStates:
            {
                (void)index;
                *value = m_pc_sample_states.size();
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerPCSamplingStateId:
            case kRPVControllerPCSamplingPcSampleStateUuid:
            {
                if(index < m_pc_sample_states.size())
                {
                    *value = m_pc_sample_states[index].pc_sample_state_uuid;
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVControllerPCSamplingStateIsaLineId:
            case kRPVControllerPCSamplingPcSampleStateInstructionUuid:
            {
                if(index < m_pc_sample_states.size())
                {
                    *value = m_pc_sample_states[index].instruction_uuid;
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVControllerPCSamplingStateDispatchId:
            {
                if(index < m_pc_sample_states.size())
                {
                    // Retained for compatibility with the legacy controller API.
                    *value = 0;
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVControllerPCSamplingStateIssuedCount:
            case kRPVControllerPCSamplingPcSampleStateIssueCount:
            {
                if(index < m_pc_sample_states.size())
                {
                    *value = m_pc_sample_states[index].issue_count;
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVControllerPCSamplingStateStalledCount:
            case kRPVControllerPCSamplingPcSampleStateStallCount:
            {
                if(index < m_pc_sample_states.size())
                {
                    *value = m_pc_sample_states[index].stall_count;
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVControllerPCSamplingStateTotalCount:
            case kRPVControllerPCSamplingPcSampleStateTotalCount:
            {
                if(index < m_pc_sample_states.size())
                {
                    *value = m_pc_sample_states[index].total_count;
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVControllerPCSamplingNumStallReasonCounts:
            case kRPVControllerPCSamplingNumPcSampleStallReasons:
            {
                (void)index;
                *value = m_pc_sample_stall_reasons.size();
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerPCSamplingStallReasonSamplingStateId:
            case kRPVControllerPCSamplingPcSampleStallReasonStateUuid:
            {
                if(index < m_pc_sample_stall_reasons.size())
                {
                    *value = m_pc_sample_stall_reasons[index].pc_sample_state_uuid;
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVControllerPCSamplingStallReasonId:
            case kRPVControllerPCSamplingPcSampleStallReasonTypeUuid:
            {
                if(index < m_pc_sample_stall_reasons.size())
                {
                    *value = m_pc_sample_stall_reasons[index].pc_sample_stall_reason_type_uuid;
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVControllerPCSamplingStallReasonCount:
            case kRPVControllerPCSamplingPcSampleStallReasonCount:
            {
                if(index < m_pc_sample_stall_reasons.size())
                {
                    *value = m_pc_sample_stall_reasons[index].count;
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVControllerPCSamplingPcSampleStallReasonUuid:
            {
                if(index < m_pc_sample_stall_reasons.size())
                {
                    *value = m_pc_sample_stall_reasons[index].pc_sample_stall_reason_uuid;
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVControllerPCSamplingNumPcSampleStallReasonTypes:
            {
                (void)index;
                *value = m_pc_sample_stall_reason_types.size();
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerPCSamplingPcSampleStallReasonTypeRecordUuid:
            {
                if(index < m_pc_sample_stall_reason_types.size())
                {
                    *value = m_pc_sample_stall_reason_types[index]
                                 .pc_sample_stall_reason_type_uuid;
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            default:
            {
                result = UnhandledProperty(property);
                break;
            }
        }
    }
    return result;
}

rocprofvis_result_t PcSampling::SetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t value)
{
    std::lock_guard<std::recursive_mutex> lock(m_data_mutex);
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerPCSamplingNumSourceFiles:
        {
            (void)index;
            m_source_files.resize(value);
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerPCSamplingSourceFileId:
        {
            if(index < m_source_files.size())
            {
                m_source_files[index].id = static_cast<uint32_t>(value);
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerPCSamplingNumSourceLines:
        {
            (void)index;
            m_source_lines.resize(value);
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerPCSamplingSourceLineId:
        {
            if(index < m_source_lines.size())
            {
                m_source_lines[index].id = static_cast<uint32_t>(value);
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerPCSamplingSourceLineSourceFileId:
        {
            if(index < m_source_lines.size())
            {
                m_source_lines[index].source_file_id = static_cast<uint32_t>(value);
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerPCSamplingSourceLineNumber:
        {
            if(index < m_source_lines.size())
            {
                m_source_lines[index].line_number = static_cast<uint32_t>(value);
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerPCSamplingNumCodeObjects:
        {
            (void)index;
            m_code_object_store.resize(value);
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerPCSamplingCodeObjectUuid:
        {
            if(index < m_code_object_store.size())
            {
                m_code_object_store[index].code_object_uuid = value;
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerPCSamplingCodeObjectWorkloadId:
        {
            if(index < m_code_object_store.size())
            {
                m_code_object_store[index].workload_id = value;
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerPCSamplingCodeObjectPid:
        {
            if(index < m_code_object_store.size())
            {
                m_code_object_store[index].pid = value;
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerPCSamplingCodeObjectId:
        {
            if(index < m_code_object_store.size())
            {
                m_code_object_store[index].code_object_id = value;
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerPCSamplingCodeObjectLoadBase:
        {
            if(index < m_code_object_store.size())
            {
                m_code_object_store[index].load_base = value;
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerPCSamplingNumIsaLines:
        {
            (void)index;
            m_isa_lines.resize(value);
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerPCSamplingIsaLineId:
        {
            if(index < m_isa_lines.size())
            {
                m_isa_lines[index].instruction_uuid = value;
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerPCSamplingIsaLineCodeObjectId:
        {
            if(index < m_isa_lines.size())
            {
                m_isa_lines[index].code_object_uuid = value;
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerPCSamplingIsaLineKernelUuid:
        {
            if(index < m_isa_lines.size())
            {
                m_isa_lines[index].kernel_uuid = value;
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerPCSamplingIsaLineCodeObjectOffset:
        {
            if(index < m_isa_lines.size())
            {
                m_isa_lines[index].code_object_offset = value;
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerPCSamplingIsaLineInstructionTypeId:
        {
            if(index < m_isa_lines.size())
            {
                // Kept for compatibility with legacy query results.
                (void)value;
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerPCSamplingNumIsaToIsaDeps:
        {
            (void)index;
            m_isa_to_isa_deps.resize(value);
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerPCSamplingIsaToIsaDependentIsaLineId:
        {
            if(index < m_isa_to_isa_deps.size())
            {
                m_isa_to_isa_deps[index].dependent_isa_line_id = static_cast<uint32_t>(value);
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerPCSamplingIsaToIsaDependencyIsaLineId:
        {
            if(index < m_isa_to_isa_deps.size())
            {
                m_isa_to_isa_deps[index].dependency_isa_line_id = static_cast<uint32_t>(value);
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerPCSamplingNumIsaToSourceDeps:
        {
            (void)index;
            m_isa_to_source_deps.resize(value);
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerPCSamplingIsaToSourceIsaLineId:
        {
            if(index < m_isa_to_source_deps.size())
            {
                m_isa_to_source_deps[index].isa_line_id = static_cast<uint32_t>(value);
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerPCSamplingIsaToSourceSourceLineId:
        {
            if(index < m_isa_to_source_deps.size())
            {
                m_isa_to_source_deps[index].source_line_id = static_cast<uint32_t>(value);
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerPCSamplingIsaToSourceDepth:
        {
            if(index < m_isa_to_source_deps.size())
            {
                m_isa_to_source_deps[index].depth = static_cast<uint32_t>(value);
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerPCSamplingNumSamplingStates:
        case kRPVControllerPCSamplingNumPcSampleStates:
        {
            (void)index;
            m_pc_sample_states.resize(value);
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerPCSamplingStateId:
        case kRPVControllerPCSamplingPcSampleStateUuid:
        {
            if(index < m_pc_sample_states.size())
            {
                m_pc_sample_states[index].pc_sample_state_uuid = value;
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerPCSamplingStateIsaLineId:
        case kRPVControllerPCSamplingPcSampleStateInstructionUuid:
        {
            if(index < m_pc_sample_states.size())
            {
                m_pc_sample_states[index].instruction_uuid = value;
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerPCSamplingStateDispatchId:
        {
            if(index < m_pc_sample_states.size())
            {
                // Retained for compatibility with legacy query results.
                (void)value;
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerPCSamplingStateIssuedCount:
        case kRPVControllerPCSamplingPcSampleStateIssueCount:
        {
            if(index < m_pc_sample_states.size())
            {
                m_pc_sample_states[index].issue_count = value;
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerPCSamplingStateStalledCount:
        case kRPVControllerPCSamplingPcSampleStateStallCount:
        {
            if(index < m_pc_sample_states.size())
            {
                m_pc_sample_states[index].stall_count = value;
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerPCSamplingStateTotalCount:
        case kRPVControllerPCSamplingPcSampleStateTotalCount:
        {
            if(index < m_pc_sample_states.size())
            {
                m_pc_sample_states[index].total_count = value;
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerPCSamplingNumStallReasonCounts:
        case kRPVControllerPCSamplingNumPcSampleStallReasons:
        {
            (void)index;
            m_pc_sample_stall_reasons.resize(value);
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerPCSamplingStallReasonSamplingStateId:
        case kRPVControllerPCSamplingPcSampleStallReasonStateUuid:
        {
            if(index < m_pc_sample_stall_reasons.size())
            {
                m_pc_sample_stall_reasons[index].pc_sample_state_uuid = value;
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerPCSamplingStallReasonId:
        case kRPVControllerPCSamplingPcSampleStallReasonTypeUuid:
        {
            if(index < m_pc_sample_stall_reasons.size())
            {
                m_pc_sample_stall_reasons[index].pc_sample_stall_reason_type_uuid = value;
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerPCSamplingStallReasonCount:
        case kRPVControllerPCSamplingPcSampleStallReasonCount:
        {
            if(index < m_pc_sample_stall_reasons.size())
            {
                m_pc_sample_stall_reasons[index].count = value;
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerPCSamplingPcSampleStallReasonUuid:
        {
            if(index < m_pc_sample_stall_reasons.size())
            {
                m_pc_sample_stall_reasons[index].pc_sample_stall_reason_uuid = value;
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerPCSamplingNumPcSampleStallReasonTypes:
        {
            (void)index;
            m_pc_sample_stall_reason_types.resize(value);
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerPCSamplingPcSampleStallReasonTypeRecordUuid:
        {
            if(index < m_pc_sample_stall_reason_types.size())
            {
                m_pc_sample_stall_reason_types[index]
                    .pc_sample_stall_reason_type_uuid = value;
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        default:
        {
            result = UnhandledProperty(property);
            break;
        }
    }
    return result;
}

rocprofvis_result_t PcSampling::GetDouble(rocprofvis_property_t property, uint64_t index, double* value)
{
    std::lock_guard<std::recursive_mutex> lock(m_data_mutex);
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            case kRPVControllerPCSamplingStateActiveThreadsPercent:
            case kRPVControllerPCSamplingStateWaveOccupancyPercent:
            {
                if(index < m_pc_sample_states.size())
                {
                    // Retained for compatibility with the legacy controller API.
                    *value = 0.0;
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            default:
            {
                result = UnhandledProperty(property);
                break;
            }
        }
    }
    return result;
}

rocprofvis_result_t PcSampling::SetDouble(rocprofvis_property_t property, uint64_t index, double value)
{
    std::lock_guard<std::recursive_mutex> lock(m_data_mutex);
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerPCSamplingStateActiveThreadsPercent:
        case kRPVControllerPCSamplingStateWaveOccupancyPercent:
        {
            if(index < m_pc_sample_states.size())
            {
                // Retained for compatibility with legacy query results.
                (void)value;
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        default:
        {
            result = UnhandledProperty(property);
            break;
        }
    }
    return result;
}

rocprofvis_result_t PcSampling::GetString(rocprofvis_property_t property, uint64_t index, char* value, uint32_t* length)
{
    std::lock_guard<std::recursive_mutex> lock(m_data_mutex);
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(length)
    {
        switch(property)
        {
            case kRPVControllerPCSamplingFilePath:
            {
                if(index < m_source_files.size())
                {
                    result = GetStdStringImpl(value, length, m_source_files[index].file_path);
                }
                break;
            }
            case kRPVControllerPCSamplingSourceFileChecksum:
            {
                if(index < m_source_files.size())
                {
                    result = GetStdStringImpl(value, length, m_source_files[index].content_checksum);
                }
                break;
            }
            case kRPVControllerPCSamplingSourceLineContent:
            {
                if(index < m_source_lines.size())
                {
                    result = GetStdStringImpl(value, length, m_source_lines[index].content);
                }
                break;
            }
            case kRPVControllerPCSamplingCodeObjectUri:
            case kRPVControllerPCSamplingCodeObjectChecksum:
            {
                if(index < m_code_object_store.size())
                {
                    // Retained for compatibility with the legacy controller API.
                    result = GetStdStringImpl(value, length, std::string{});
                }
                break;
            }
            case kRPVControllerPCSamplingIsaLineInstruction:
            {
                if(index < m_isa_lines.size())
                {
                    result = GetStdStringImpl(value, length, m_isa_lines[index].instruction);
                }
                break;
            }
            case kRPVControllerPCSamplingIsaLineComment:
            {
                if(index < m_isa_lines.size())
                {
                    result = GetStdStringImpl(value, length, m_isa_lines[index].comment);
                }
                break;
            }
            case kRPVControllerPCSamplingPcSampleStallReasonTypeText:
            {
                if(index < m_pc_sample_stall_reason_types.size())
                {
                    result = GetStdStringImpl(
                        value, length, m_pc_sample_stall_reason_types[index].text);
                }
                break;
            }
            default:
            {
                result = UnhandledProperty(property);
                break;
            }
        }
    }
    return result;
}

rocprofvis_result_t PcSampling::SetString(rocprofvis_property_t property, uint64_t index, char const* value)
{
    std::lock_guard<std::recursive_mutex> lock(m_data_mutex);
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerPCSamplingFilePath:
        {
            if(index < m_source_files.size())
            {
                m_source_files[index].file_path = value;
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerPCSamplingSourceFileChecksum:
        {
            if(index < m_source_files.size())
            {
                m_source_files[index].content_checksum = value;
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerPCSamplingSourceLineContent:
        {
            if(index < m_source_lines.size())
            {
                m_source_lines[index].content = value;
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerPCSamplingCodeObjectUri:
        case kRPVControllerPCSamplingCodeObjectChecksum:
        {
            if(index < m_code_object_store.size())
            {
                // Retained for compatibility with legacy query results.
                (void)value;
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerPCSamplingIsaLineInstruction:
        {
            if(index < m_isa_lines.size())
            {
                m_isa_lines[index].instruction = value;
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerPCSamplingIsaLineComment:
        {
            if(index < m_isa_lines.size())
            {
                m_isa_lines[index].comment = value;
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerPCSamplingPcSampleStallReasonTypeText:
        {
            if(index < m_pc_sample_stall_reason_types.size())
            {
                m_pc_sample_stall_reason_types[index].text = value;
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        default:
        {
            result = UnhandledProperty(property);
            break;
        }
    }
    return result;
}

bool PcSampling::QueryToPropertyEnum(rocprofvis_db_compute_column_enum_t in, rocprofvis_property_t& property, rocprofvis_controller_primitive_type_t& type) const
{
    bool valid = true;
    switch(in)
    {
        case kRPVComputeColumnPcSamplingSourceFileId:
        {
            property = kRPVControllerPCSamplingSourceFileId;
            type = kRPVControllerPrimitiveTypeUInt64;
            break;
        }
        case kRPVComputeColumnPcSamplingSourceFilePath:
        {
            property = kRPVControllerPCSamplingFilePath;
            type = kRPVControllerPrimitiveTypeString;
            break;
        }
        case kRPVComputeColumnPcSamplingSourceFileChecksum:
        {
            property = kRPVControllerPCSamplingSourceFileChecksum;
            type = kRPVControllerPrimitiveTypeString;
            break;
        }
        case kRPVComputeColumnPcSamplingSourceLineId:
        {
            property = kRPVControllerPCSamplingSourceLineId;
            type = kRPVControllerPrimitiveTypeUInt64;
            break;
        }
        case kRPVComputeColumnPcSamplingSourceLineFileId:
        {
            property = kRPVControllerPCSamplingSourceLineSourceFileId;
            type = kRPVControllerPrimitiveTypeUInt64;
            break;
        }
        case kRPVComputeColumnPcSamplingSourceLineNumber:
        {
            property = kRPVControllerPCSamplingSourceLineNumber;
            type = kRPVControllerPrimitiveTypeUInt64;
            break;
        }
        case kRPVComputeColumnPcSamplingSourceLineContent:
        {
            property = kRPVControllerPCSamplingSourceLineContent;
            type = kRPVControllerPrimitiveTypeString;
            break;
        }
        case kRPVComputeColumnPcSamplingCodeObjectId:
        {
            property = kRPVControllerPCSamplingCodeObjectId;
            type = kRPVControllerPrimitiveTypeUInt64;
            break;
        }
        case kRPVComputeColumnPcSamplingCodeObjectUri:
        {
            property = kRPVControllerPCSamplingCodeObjectUri;
            type = kRPVControllerPrimitiveTypeString;
            break;
        }
        case kRPVComputeColumnPcSamplingCodeObjectChecksum:
        {
            property = kRPVControllerPCSamplingCodeObjectChecksum;
            type = kRPVControllerPrimitiveTypeString;
            break;
        }
        case kRPVComputeColumnPcSamplingIsaLineId:
        {
            property = kRPVControllerPCSamplingIsaLineId;
            type = kRPVControllerPrimitiveTypeUInt64;
            break;
        }
        case kRPVComputeColumnPcSamplingIsaLineCodeObjectId:
        {
            property = kRPVControllerPCSamplingIsaLineCodeObjectId;
            type = kRPVControllerPrimitiveTypeUInt64;
            break;
        }
        case kRPVComputeColumnPcSamplingIsaLineCodeObjectOffset:
        {
            property = kRPVControllerPCSamplingIsaLineCodeObjectOffset;
            type = kRPVControllerPrimitiveTypeUInt64;
            break;
        }
        case kRPVComputeColumnPcSamplingCodeObjectUuid:
        {
            property = kRPVControllerPCSamplingCodeObjectUuid;
            type = kRPVControllerPrimitiveTypeUInt64;
            break;
        }
        case kRPVComputeColumnPcSamplingCodeObjectWorkloadId:
        {
            property = kRPVControllerPCSamplingCodeObjectWorkloadId;
            type = kRPVControllerPrimitiveTypeUInt64;
            break;
        }
        case kRPVComputeColumnPcSamplingCodeObjectPid:
        {
            property = kRPVControllerPCSamplingCodeObjectPid;
            type = kRPVControllerPrimitiveTypeUInt64;
            break;
        }
        case kRPVComputeColumnPcSamplingCodeObjectLoadBase:
        {
            property = kRPVControllerPCSamplingCodeObjectLoadBase;
            type = kRPVControllerPrimitiveTypeUInt64;
            break;
        }
        case kRPVComputeColumnPcSamplingIsaLineKernelUuid:
        {
            property = kRPVControllerPCSamplingIsaLineKernelUuid;
            type = kRPVControllerPrimitiveTypeUInt64;
            break;
        }
        case kRPVComputeColumnPcSamplingIsaLineInstructionTypeId:
        {
            property = kRPVControllerPCSamplingIsaLineInstructionTypeId;
            type = kRPVControllerPrimitiveTypeUInt64;
            break;
        }
        case kRPVComputeColumnPcSamplingIsaLineInstruction:
        {
            property = kRPVControllerPCSamplingIsaLineInstruction;
            type = kRPVControllerPrimitiveTypeString;
            break;
        }
        case kRPVComputeColumnPcSamplingIsaLineComment:
        {
            property = kRPVControllerPCSamplingIsaLineComment;
            type = kRPVControllerPrimitiveTypeString;
            break;
        }
        case kRPVComputeColumnPcSamplingIsaToIsaDependentIsaLineId:
        {
            property = kRPVControllerPCSamplingIsaToIsaDependentIsaLineId;
            type = kRPVControllerPrimitiveTypeUInt64;
            break;
        }
        case kRPVComputeColumnPcSamplingIsaToIsaDependencyIsaLineId:
        {
            property = kRPVControllerPCSamplingIsaToIsaDependencyIsaLineId;
            type = kRPVControllerPrimitiveTypeUInt64;
            break;
        }
        case kRPVComputeColumnPcSamplingIsaToSourceIsaLineId:
        {
            property = kRPVControllerPCSamplingIsaToSourceIsaLineId;
            type = kRPVControllerPrimitiveTypeUInt64;
            break;
        }
        case kRPVComputeColumnPcSamplingIsaToSourceSourceLineId:
        {
            property = kRPVControllerPCSamplingIsaToSourceSourceLineId;
            type = kRPVControllerPrimitiveTypeUInt64;
            break;
        }
        case kRPVComputeColumnPcSamplingIsaToSourceDepth:
        {
            property = kRPVControllerPCSamplingIsaToSourceDepth;
            type = kRPVControllerPrimitiveTypeUInt64;
            break;
        }
        case kRPVComputeColumnPcSamplingStateId:
        {
            property = kRPVControllerPCSamplingStateId;
            type = kRPVControllerPrimitiveTypeUInt64;
            break;
        }
        case kRPVComputeColumnPcSamplingStateIsaLineId:
        {
            property = kRPVControllerPCSamplingStateIsaLineId;
            type = kRPVControllerPrimitiveTypeUInt64;
            break;
        }
        case kRPVComputeColumnPcSamplingStateDispatchId:
        {
            property = kRPVControllerPCSamplingStateDispatchId;
            type = kRPVControllerPrimitiveTypeUInt64;
            break;
        }
        case kRPVComputeColumnPcSamplingStateActiveThreadsPercent:
        {
            property = kRPVControllerPCSamplingStateActiveThreadsPercent;
            type = kRPVControllerPrimitiveTypeDouble;
            break;
        }
        case kRPVComputeColumnPcSamplingStateWaveOccupancyPercent:
        {
            property = kRPVControllerPCSamplingStateWaveOccupancyPercent;
            type = kRPVControllerPrimitiveTypeDouble;
            break;
        }
        case kRPVComputeColumnPcSamplingStateIssuedCount:
        {
            property = kRPVControllerPCSamplingStateIssuedCount;
            type = kRPVControllerPrimitiveTypeUInt64;
            break;
        }
        case kRPVComputeColumnPcSamplingStateStalledCount:
        {
            property = kRPVControllerPCSamplingStateStalledCount;
            type = kRPVControllerPrimitiveTypeUInt64;
            break;
        }
        case kRPVComputeColumnPcSamplingStateTotalCount:
        {
            property = kRPVControllerPCSamplingStateTotalCount;
            type = kRPVControllerPrimitiveTypeUInt64;
            break;
        }
        case kRPVComputeColumnPcSamplingStallReasonSamplingStateId:
        {
            property = kRPVControllerPCSamplingStallReasonSamplingStateId;
            type = kRPVControllerPrimitiveTypeUInt64;
            break;
        }
        case kRPVComputeColumnPcSamplingStallReasonId:
        {
            property = kRPVControllerPCSamplingStallReasonId;
            type = kRPVControllerPrimitiveTypeUInt64;
            break;
        }
        case kRPVComputeColumnPcSamplingStallReasonCount:
        {
            property = kRPVControllerPCSamplingStallReasonCount;
            type = kRPVControllerPrimitiveTypeUInt64;
            break;
        }
        case kRPVComputeColumnPcSampleStateUuid:
        {
            property = kRPVControllerPCSamplingPcSampleStateUuid;
            type = kRPVControllerPrimitiveTypeUInt64;
            break;
        }
        case kRPVComputeColumnPcSampleStateInstructionUuid:
        {
            property = kRPVControllerPCSamplingPcSampleStateInstructionUuid;
            type = kRPVControllerPrimitiveTypeUInt64;
            break;
        }
        case kRPVComputeColumnPcSampleStateTotalCount:
        {
            property = kRPVControllerPCSamplingPcSampleStateTotalCount;
            type = kRPVControllerPrimitiveTypeUInt64;
            break;
        }
        case kRPVComputeColumnPcSampleStateIssueCount:
        {
            property = kRPVControllerPCSamplingPcSampleStateIssueCount;
            type = kRPVControllerPrimitiveTypeUInt64;
            break;
        }
        case kRPVComputeColumnPcSampleStateStallCount:
        {
            property = kRPVControllerPCSamplingPcSampleStateStallCount;
            type = kRPVControllerPrimitiveTypeUInt64;
            break;
        }
        case kRPVComputeColumnPcSampleStallReasonUuid:
        {
            property = kRPVControllerPCSamplingPcSampleStallReasonUuid;
            type = kRPVControllerPrimitiveTypeUInt64;
            break;
        }
        case kRPVComputeColumnPcSampleStallReasonStateUuid:
        {
            property = kRPVControllerPCSamplingPcSampleStallReasonStateUuid;
            type = kRPVControllerPrimitiveTypeUInt64;
            break;
        }
        case kRPVComputeColumnPcSampleStallReasonTypeUuid:
        {
            property = kRPVControllerPCSamplingPcSampleStallReasonTypeUuid;
            type = kRPVControllerPrimitiveTypeUInt64;
            break;
        }
        case kRPVComputeColumnPcSampleStallReasonCount:
        {
            property = kRPVControllerPCSamplingPcSampleStallReasonCount;
            type = kRPVControllerPrimitiveTypeUInt64;
            break;
        }
        case kRPVComputeColumnPcSampleStallReasonTypeRecordUuid:
        {
            property = kRPVControllerPCSamplingPcSampleStallReasonTypeRecordUuid;
            type = kRPVControllerPrimitiveTypeUInt64;
            break;
        }
        case kRPVComputeColumnPcSampleStallReasonTypeText:
        {
            property = kRPVControllerPCSamplingPcSampleStallReasonTypeText;
            type = kRPVControllerPrimitiveTypeString;
            break;
        }
        default:
        {
            valid = false;
            break;
        }
    }
    return valid;
}

}
}
