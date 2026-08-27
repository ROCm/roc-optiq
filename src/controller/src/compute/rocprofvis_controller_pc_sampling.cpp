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

std::recursive_mutex& PcSampling::GetLayerMutex(DataLayer layer)
{
    switch(layer)
    {
        case DataLayer::kIsa: return m_isa_data_mutex;
        case DataLayer::kSource: return m_source_data_mutex;
        case DataLayer::kStalls: return m_stalls_data_mutex;
    }

    return m_stalls_data_mutex;
}

std::recursive_mutex& PcSampling::GetPropertyMutex(rocprofvis_property_t property)
{
    const auto property_value = static_cast<uint32_t>(property);
    const auto in_range       = [property_value](auto first, auto last) {
        return property_value >= static_cast<uint32_t>(first) &&
               property_value <= static_cast<uint32_t>(last);
    };

    if(in_range(kRPVControllerPCSamplingNumCodeObjects,
                kRPVControllerPCSamplingInstructionLineInstruction))
        return m_isa_data_mutex;

    if(in_range(kRPVControllerPCSamplingNumSourceFiles,
                kRPVControllerPCSamplingSourceLineContent) ||
       in_range(kRPVControllerPCSamplingNumInstructionSourceLines,
                kRPVControllerPCSamplingInstructionSourceLineFrameIndex))
        return m_source_data_mutex;

    return m_stalls_data_mutex;
}

rocprofvis_controller_object_type_t PcSampling::GetType(void)
{
    return kRPVControllerObjectTypePCSampling;
}

rocprofvis_result_t PcSampling::GetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t* value)
{
    std::lock_guard<std::recursive_mutex> lock(GetPropertyMutex(property));
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
            case kRPVControllerPCSamplingSourceFileUuid:
            {
                if(index < m_source_files.size())
                {
                    *value = m_source_files[index].source_file_uuid;
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
            case kRPVControllerPCSamplingSourceLineUuid:
            {
                if(index < m_source_lines.size())
                {
                    *value = m_source_lines[index].source_line_uuid;
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVControllerPCSamplingSourceLineSourceFileUuid:
            {
                if(index < m_source_lines.size())
                {
                    *value = m_source_lines[index].source_file_uuid;
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
            case kRPVControllerPCSamplingSourceFileWorkloadId:
            {
                if(index < m_source_files.size())
                {
                    *value = m_source_files[index].workload_id;
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
            case kRPVControllerPCSamplingNumKernelSymbols:
            {
                (void)index;
                *value = m_kernel_symbols.size();
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerPCSamplingKernelSymbolUuid:
            {
                if(index < m_kernel_symbols.size())
                {
                    *value = m_kernel_symbols[index].kernel_symbol_uuid;
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVControllerPCSamplingKernelSymbolCodeObjectUuid:
            {
                if(index < m_kernel_symbols.size())
                {
                    *value = m_kernel_symbols[index].code_object_uuid;
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVControllerPCSamplingKernelSymbolKernelUuid:
            {
                if(index < m_kernel_symbols.size())
                {
                    *value = m_kernel_symbols[index].kernel_uuid;
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVControllerPCSamplingKernelSymbolCodeObjectOffset:
            {
                if(index < m_kernel_symbols.size())
                {
                    *value = m_kernel_symbols[index].code_object_offset;
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVControllerPCSamplingNumInstructionLines:
            {
                (void)index;
                *value = m_instruction_lines.size();
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerPCSamplingInstructionLineUuid:
            {
                if(index < m_instruction_lines.size())
                {
                    *value = m_instruction_lines[index].instruction_uuid;
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVControllerPCSamplingInstructionLineKernelSymbolUuid:
            {
                if(index < m_instruction_lines.size())
                {
                    *value = m_instruction_lines[index].kernel_symbol_uuid;
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVControllerPCSamplingInstructionLineCodeObjectOffset:
            {
                if(index < m_instruction_lines.size())
                {
                    *value = m_instruction_lines[index].code_object_offset;
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVControllerPCSamplingInstructionLineInstructionTypeUuid:
            {
                if(index < m_instruction_lines.size())
                {
                    *value = m_instruction_lines[index].instruction_type_uuid;
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVControllerPCSamplingNumInstructionSourceLines:
            {
                (void)index;
                *value = m_instruction_source_lines.size();
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerPCSamplingInstructionSourceLineInstructionUuid:
            {
                if(index < m_instruction_source_lines.size())
                {
                    *value = m_instruction_source_lines[index].instruction_uuid;
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVControllerPCSamplingInstructionSourceLineSourceLineUuid:
            {
                if(index < m_instruction_source_lines.size())
                {
                    *value = m_instruction_source_lines[index].source_line_uuid;
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVControllerPCSamplingInstructionSourceLineFrameIndex:
            {
                if(index < m_instruction_source_lines.size())
                {
                    *value = m_instruction_source_lines[index].frame_index;
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVControllerPCSamplingInstructionSourceLineUuid:
            {
                if(index < m_instruction_source_lines.size())
                {
                    *value = m_instruction_source_lines[index].instruction_source_line_uuid;
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVControllerPCSamplingNumPcSampleStates:
            {
                (void)index;
                *value = m_pc_sample_states.size();
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerPCSamplingPcSampleStateUuid:
            {
                if(index < m_pc_sample_states.size())
                {
                    *value = m_pc_sample_states[index].pc_sample_state_uuid;
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVControllerPCSamplingPcSampleStateInstructionUuid:
            {
                if(index < m_pc_sample_states.size())
                {
                    *value = m_pc_sample_states[index].instruction_uuid;
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVControllerPCSamplingPcSampleStateDispatchUuid:
            {
                if(index < m_pc_sample_states.size())
                {
                    *value = m_pc_sample_states[index].dispatch_uuid;
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVControllerPCSamplingPcSampleStateIssueCount:
            {
                if(index < m_pc_sample_states.size())
                {
                    *value = m_pc_sample_states[index].issue_count;
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVControllerPCSamplingPcSampleStateStallCount:
            {
                if(index < m_pc_sample_states.size())
                {
                    *value = m_pc_sample_states[index].stall_count;
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVControllerPCSamplingPcSampleStateTotalCount:
            {
                if(index < m_pc_sample_states.size())
                {
                    *value = m_pc_sample_states[index].total_count;
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVControllerPCSamplingNumPcSampleStallReasons:
            {
                (void)index;
                *value = m_pc_sample_stall_reasons.size();
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerPCSamplingPcSampleStallReasonStateUuid:
            {
                if(index < m_pc_sample_stall_reasons.size())
                {
                    *value = m_pc_sample_stall_reasons[index].pc_sample_state_uuid;
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVControllerPCSamplingPcSampleStallReasonLookupUuid:
            {
                if(index < m_pc_sample_stall_reasons.size())
                {
                    *value = m_pc_sample_stall_reasons[index].pc_sample_stall_reason_lookup_uuid;
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
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
            case kRPVControllerPCSamplingNumPcSampleStallReasonLookups:
            {
                (void)index;
                *value = m_pc_sample_stall_reason_lookups.size();
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerPCSamplingPcSampleStallReasonLookupRecordUuid:
            {
                if(index < m_pc_sample_stall_reason_lookups.size())
                {
                    *value = m_pc_sample_stall_reason_lookups[index]
                                 .pc_sample_stall_reason_lookup_uuid;
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVControllerPCSamplingNumInstructionTypeLookups:
            {
                (void)index;
                *value = m_instruction_type_lookups.size();
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerPCSamplingInstructionTypeLookupUuid:
            {
                if(index < m_instruction_type_lookups.size())
                {
                    *value = m_instruction_type_lookups[index].instruction_type_lookup_uuid;
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVControllerPCSamplingNumInstructionSamples:
            {
                (void)index;
                *value = m_instruction_samples.size();
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerPCSamplingInstructionSampleUuid:
            case kRPVControllerPCSamplingInstructionSampleStateUuid:
            case kRPVControllerPCSamplingInstructionSampleLookupUuid:
            case kRPVControllerPCSamplingInstructionSampleCount:
            {
                if(index < m_instruction_samples.size())
                {
                    const InstructionSample& sample = m_instruction_samples[index];
                    if(property == kRPVControllerPCSamplingInstructionSampleUuid)
                        *value = sample.instruction_sample_uuid;
                    else if(property == kRPVControllerPCSamplingInstructionSampleStateUuid)
                        *value = sample.pc_sample_state_uuid;
                    else if(property == kRPVControllerPCSamplingInstructionSampleLookupUuid)
                        *value = sample.instruction_sample_lookup_uuid;
                    else
                        *value = sample.count;
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVControllerPCSamplingNumInstructionSampleLookups:
            {
                (void)index;
                *value = m_instruction_sample_lookups.size();
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerPCSamplingInstructionSampleLookupRecordUuid:
            {
                if(index < m_instruction_sample_lookups.size())
                {
                    *value = m_instruction_sample_lookups[index]
                                 .instruction_sample_lookup_uuid;
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
    std::lock_guard<std::recursive_mutex> lock(GetPropertyMutex(property));
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
        case kRPVControllerPCSamplingSourceFileUuid:
        {
            if(index < m_source_files.size())
            {
                m_source_files[index].source_file_uuid = value;
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
        case kRPVControllerPCSamplingSourceLineUuid:
        {
            if(index < m_source_lines.size())
            {
                m_source_lines[index].source_line_uuid = value;
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerPCSamplingSourceLineSourceFileUuid:
        {
            if(index < m_source_lines.size())
            {
                m_source_lines[index].source_file_uuid = value;
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerPCSamplingSourceLineNumber:
        {
            if(index < m_source_lines.size())
            {
                m_source_lines[index].line_number = value;
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerPCSamplingSourceFileWorkloadId:
        {
            if(index < m_source_files.size())
            {
                m_source_files[index].workload_id = value;
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
        case kRPVControllerPCSamplingNumKernelSymbols:
        {
            (void)index;
            m_kernel_symbols.resize(value);
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerPCSamplingKernelSymbolUuid:
        {
            if(index < m_kernel_symbols.size())
            {
                m_kernel_symbols[index].kernel_symbol_uuid = value;
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerPCSamplingKernelSymbolCodeObjectUuid:
        {
            if(index < m_kernel_symbols.size())
            {
                m_kernel_symbols[index].code_object_uuid = value;
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerPCSamplingKernelSymbolKernelUuid:
        {
            if(index < m_kernel_symbols.size())
            {
                m_kernel_symbols[index].kernel_uuid = value;
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerPCSamplingKernelSymbolCodeObjectOffset:
        {
            if(index < m_kernel_symbols.size())
            {
                m_kernel_symbols[index].code_object_offset = value;
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerPCSamplingNumInstructionLines:
        {
            (void)index;
            m_instruction_lines.resize(value);
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerPCSamplingInstructionLineUuid:
        {
            if(index < m_instruction_lines.size())
            {
                m_instruction_lines[index].instruction_uuid = value;
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerPCSamplingInstructionLineKernelSymbolUuid:
        {
            if(index < m_instruction_lines.size())
            {
                m_instruction_lines[index].kernel_symbol_uuid = value;
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerPCSamplingInstructionLineCodeObjectOffset:
        {
            if(index < m_instruction_lines.size())
            {
                m_instruction_lines[index].code_object_offset = value;
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerPCSamplingInstructionLineInstructionTypeUuid:
        {
            if(index < m_instruction_lines.size())
            {
                m_instruction_lines[index].instruction_type_uuid = value;
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerPCSamplingNumInstructionSourceLines:
        {
            (void)index;
            m_instruction_source_lines.resize(value);
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerPCSamplingInstructionSourceLineInstructionUuid:
        {
            if(index < m_instruction_source_lines.size())
            {
                m_instruction_source_lines[index].instruction_uuid = value;
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerPCSamplingInstructionSourceLineSourceLineUuid:
        {
            if(index < m_instruction_source_lines.size())
            {
                m_instruction_source_lines[index].source_line_uuid = value;
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerPCSamplingInstructionSourceLineFrameIndex:
        {
            if(index < m_instruction_source_lines.size())
            {
                m_instruction_source_lines[index].frame_index = value;
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerPCSamplingInstructionSourceLineUuid:
        {
            if(index < m_instruction_source_lines.size())
            {
                m_instruction_source_lines[index].instruction_source_line_uuid = value;
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerPCSamplingNumPcSampleStates:
        {
            (void)index;
            m_pc_sample_states.resize(value);
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerPCSamplingPcSampleStateUuid:
        {
            if(index < m_pc_sample_states.size())
            {
                m_pc_sample_states[index].pc_sample_state_uuid = value;
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerPCSamplingPcSampleStateInstructionUuid:
        {
            if(index < m_pc_sample_states.size())
            {
                m_pc_sample_states[index].instruction_uuid = value;
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerPCSamplingPcSampleStateDispatchUuid:
        {
            if(index < m_pc_sample_states.size())
            {
                m_pc_sample_states[index].dispatch_uuid = value;
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerPCSamplingPcSampleStateIssueCount:
        {
            if(index < m_pc_sample_states.size())
            {
                m_pc_sample_states[index].issue_count = value;
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerPCSamplingPcSampleStateStallCount:
        {
            if(index < m_pc_sample_states.size())
            {
                m_pc_sample_states[index].stall_count = value;
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerPCSamplingPcSampleStateTotalCount:
        {
            if(index < m_pc_sample_states.size())
            {
                m_pc_sample_states[index].total_count = value;
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerPCSamplingNumPcSampleStallReasons:
        {
            (void)index;
            m_pc_sample_stall_reasons.resize(value);
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerPCSamplingPcSampleStallReasonStateUuid:
        {
            if(index < m_pc_sample_stall_reasons.size())
            {
                m_pc_sample_stall_reasons[index].pc_sample_state_uuid = value;
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerPCSamplingPcSampleStallReasonLookupUuid:
        {
            if(index < m_pc_sample_stall_reasons.size())
            {
                m_pc_sample_stall_reasons[index].pc_sample_stall_reason_lookup_uuid = value;
                result = kRocProfVisResultSuccess;
            }
            break;
        }
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
        case kRPVControllerPCSamplingNumPcSampleStallReasonLookups:
        {
            (void)index;
            m_pc_sample_stall_reason_lookups.resize(value);
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerPCSamplingPcSampleStallReasonLookupRecordUuid:
        {
            if(index < m_pc_sample_stall_reason_lookups.size())
            {
                m_pc_sample_stall_reason_lookups[index]
                    .pc_sample_stall_reason_lookup_uuid = value;
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerPCSamplingNumInstructionTypeLookups:
        {
            (void)index;
            m_instruction_type_lookups.resize(value);
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerPCSamplingInstructionTypeLookupUuid:
        {
            if(index < m_instruction_type_lookups.size())
            {
                m_instruction_type_lookups[index].instruction_type_lookup_uuid = value;
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerPCSamplingNumInstructionSamples:
        {
            (void)index;
            m_instruction_samples.resize(value);
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerPCSamplingInstructionSampleUuid:
        case kRPVControllerPCSamplingInstructionSampleStateUuid:
        case kRPVControllerPCSamplingInstructionSampleLookupUuid:
        case kRPVControllerPCSamplingInstructionSampleCount:
        {
            if(index < m_instruction_samples.size())
            {
                InstructionSample& sample = m_instruction_samples[index];
                if(property == kRPVControllerPCSamplingInstructionSampleUuid)
                    sample.instruction_sample_uuid = value;
                else if(property == kRPVControllerPCSamplingInstructionSampleStateUuid)
                    sample.pc_sample_state_uuid = value;
                else if(property == kRPVControllerPCSamplingInstructionSampleLookupUuid)
                    sample.instruction_sample_lookup_uuid = value;
                else
                    sample.count = value;
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerPCSamplingNumInstructionSampleLookups:
        {
            (void)index;
            m_instruction_sample_lookups.resize(value);
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerPCSamplingInstructionSampleLookupRecordUuid:
        {
            if(index < m_instruction_sample_lookups.size())
            {
                m_instruction_sample_lookups[index].instruction_sample_lookup_uuid = value;
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
    std::lock_guard<std::recursive_mutex> lock(GetPropertyMutex(property));
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            case kRPVControllerPCSamplingPcSampleStateActiveThreadPercent:
            {
                if(index < m_pc_sample_states.size())
                {
                    *value = m_pc_sample_states[index].active_thread_percent;
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVControllerPCSamplingPcSampleStateWaveOccupancyPercent:
            {
                if(index < m_pc_sample_states.size())
                {
                    *value = m_pc_sample_states[index].wave_occupancy_percent;
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
    std::lock_guard<std::recursive_mutex> lock(GetPropertyMutex(property));
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerPCSamplingPcSampleStateActiveThreadPercent:
        {
            if(index < m_pc_sample_states.size())
            {
                m_pc_sample_states[index].active_thread_percent = value;
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerPCSamplingPcSampleStateWaveOccupancyPercent:
        {
            if(index < m_pc_sample_states.size())
            {
                m_pc_sample_states[index].wave_occupancy_percent = value;
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
    std::lock_guard<std::recursive_mutex> lock(GetPropertyMutex(property));
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(length)
    {
        switch(property)
        {
            case kRPVControllerPCSamplingSourceFilePath:
            {
                if(index < m_source_files.size())
                {
                    result = GetStdStringImpl(value, length, m_source_files[index].file_path);
                }
                break;
            }
            case kRPVControllerPCSamplingSourceFileMd5Checksum:
            {
                if(index < m_source_files.size())
                {
                    result = GetStdStringImpl(value, length, m_source_files[index].md5_checksum);
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
            case kRPVControllerPCSamplingInstructionLineInstruction:
            {
                if(index < m_instruction_lines.size())
                {
                    result = GetStdStringImpl(value, length, m_instruction_lines[index].instruction);
                }
                break;
            }
            case kRPVControllerPCSamplingPcSampleStallReasonLookupText:
            {
                if(index < m_pc_sample_stall_reason_lookups.size())
                {
                    result = GetStdStringImpl(
                        value, length, m_pc_sample_stall_reason_lookups[index].text);
                }
                break;
            }
            case kRPVControllerPCSamplingInstructionTypeLookupText:
            {
                if(index < m_instruction_type_lookups.size())
                {
                    result = GetStdStringImpl(value, length,
                                              m_instruction_type_lookups[index].text);
                }
                break;
            }
            case kRPVControllerPCSamplingInstructionSampleLookupText:
            {
                if(index < m_instruction_sample_lookups.size())
                {
                    result = GetStdStringImpl(value, length,
                                              m_instruction_sample_lookups[index].text);
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
    std::lock_guard<std::recursive_mutex> lock(GetPropertyMutex(property));
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerPCSamplingSourceFilePath:
        {
            if(index < m_source_files.size())
            {
                m_source_files[index].file_path = value;
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerPCSamplingSourceFileMd5Checksum:
        {
            if(index < m_source_files.size())
            {
                m_source_files[index].md5_checksum = value;
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
        case kRPVControllerPCSamplingInstructionLineInstruction:
        {
            if(index < m_instruction_lines.size())
            {
                m_instruction_lines[index].instruction = value;
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerPCSamplingPcSampleStallReasonLookupText:
        {
            if(index < m_pc_sample_stall_reason_lookups.size())
            {
                m_pc_sample_stall_reason_lookups[index].text = value;
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerPCSamplingInstructionTypeLookupText:
        {
            if(index < m_instruction_type_lookups.size())
            {
                m_instruction_type_lookups[index].text = value;
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerPCSamplingInstructionSampleLookupText:
        {
            if(index < m_instruction_sample_lookups.size())
            {
                m_instruction_sample_lookups[index].text = value;
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
        case kRPVComputeColumnPcSamplingSourceFileUuid:
        {
            property = kRPVControllerPCSamplingSourceFileUuid;
            type = kRPVControllerPrimitiveTypeUInt64;
            break;
        }
        case kRPVComputeColumnPcSamplingSourceFilePath:
        {
            property = kRPVControllerPCSamplingSourceFilePath;
            type = kRPVControllerPrimitiveTypeString;
            break;
        }
        case kRPVComputeColumnPcSamplingSourceFileMd5Checksum:
        {
            property = kRPVControllerPCSamplingSourceFileMd5Checksum;
            type = kRPVControllerPrimitiveTypeString;
            break;
        }
        case kRPVComputeColumnPcSamplingSourceFileWorkloadId:
        {
            property = kRPVControllerPCSamplingSourceFileWorkloadId;
            type = kRPVControllerPrimitiveTypeUInt64;
            break;
        }
        case kRPVComputeColumnPcSamplingSourceLineUuid:
        {
            property = kRPVControllerPCSamplingSourceLineUuid;
            type = kRPVControllerPrimitiveTypeUInt64;
            break;
        }
        case kRPVComputeColumnPcSamplingSourceLineSourceFileUuid:
        {
            property = kRPVControllerPCSamplingSourceLineSourceFileUuid;
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
        case kRPVComputeColumnPcSamplingInstructionLineUuid:
        {
            property = kRPVControllerPCSamplingInstructionLineUuid;
            type = kRPVControllerPrimitiveTypeUInt64;
            break;
        }
        case kRPVComputeColumnPcSamplingKernelSymbolUuid:
        {
            property = kRPVControllerPCSamplingKernelSymbolUuid;
            type = kRPVControllerPrimitiveTypeUInt64;
            break;
        }
        case kRPVComputeColumnPcSamplingKernelSymbolCodeObjectUuid:
        {
            property = kRPVControllerPCSamplingKernelSymbolCodeObjectUuid;
            type = kRPVControllerPrimitiveTypeUInt64;
            break;
        }
        case kRPVComputeColumnPcSamplingKernelSymbolKernelUuid:
        {
            property = kRPVControllerPCSamplingKernelSymbolKernelUuid;
            type = kRPVControllerPrimitiveTypeUInt64;
            break;
        }
        case kRPVComputeColumnPcSamplingKernelSymbolCodeObjectOffset:
        {
            property = kRPVControllerPCSamplingKernelSymbolCodeObjectOffset;
            type = kRPVControllerPrimitiveTypeUInt64;
            break;
        }
        case kRPVComputeColumnPcSamplingInstructionLineKernelSymbolUuid:
        {
            property = kRPVControllerPCSamplingInstructionLineKernelSymbolUuid;
            type = kRPVControllerPrimitiveTypeUInt64;
            break;
        }
        case kRPVComputeColumnPcSamplingInstructionLineInstructionTypeUuid:
        {
            property = kRPVControllerPCSamplingInstructionLineInstructionTypeUuid;
            type = kRPVControllerPrimitiveTypeUInt64;
            break;
        }
        case kRPVComputeColumnPcSamplingInstructionLineCodeObjectOffset:
        {
            property = kRPVControllerPCSamplingInstructionLineCodeObjectOffset;
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
        case kRPVComputeColumnPcSamplingInstructionLineInstruction:
        {
            property = kRPVControllerPCSamplingInstructionLineInstruction;
            type = kRPVControllerPrimitiveTypeString;
            break;
        }
        case kRPVComputeColumnPcSamplingInstructionSourceLineInstructionUuid:
        {
            property = kRPVControllerPCSamplingInstructionSourceLineInstructionUuid;
            type = kRPVControllerPrimitiveTypeUInt64;
            break;
        }
        case kRPVComputeColumnPcSamplingInstructionSourceLineUuid:
        {
            property = kRPVControllerPCSamplingInstructionSourceLineUuid;
            type = kRPVControllerPrimitiveTypeUInt64;
            break;
        }
        case kRPVComputeColumnPcSamplingInstructionSourceLineSourceLineUuid:
        {
            property = kRPVControllerPCSamplingInstructionSourceLineSourceLineUuid;
            type = kRPVControllerPrimitiveTypeUInt64;
            break;
        }
        case kRPVComputeColumnPcSamplingInstructionSourceLineFrameIndex:
        {
            property = kRPVControllerPCSamplingInstructionSourceLineFrameIndex;
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
        case kRPVComputeColumnPcSampleStateActiveThreadPercent:
        {
            property = kRPVControllerPCSamplingPcSampleStateActiveThreadPercent;
            type = kRPVControllerPrimitiveTypeDouble;
            break;
        }
        case kRPVComputeColumnPcSampleStateWaveOccupancyPercent:
        {
            property = kRPVControllerPCSamplingPcSampleStateWaveOccupancyPercent;
            type = kRPVControllerPrimitiveTypeDouble;
            break;
        }
        case kRPVComputeColumnPcSampleStateDispatchUuid:
        {
            property = kRPVControllerPCSamplingPcSampleStateDispatchUuid;
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
        case kRPVComputeColumnPcSampleStallReasonCount:
        {
            property = kRPVControllerPCSamplingPcSampleStallReasonCount;
            type = kRPVControllerPrimitiveTypeUInt64;
            break;
        }
        case kRPVComputeColumnPcSampleStallReasonLookupUuid:
        {
            property = kRPVControllerPCSamplingPcSampleStallReasonLookupUuid;
            type = kRPVControllerPrimitiveTypeUInt64;
            break;
        }
        case kRPVComputeColumnPcSampleStallReasonLookupRecordUuid:
        {
            property = kRPVControllerPCSamplingPcSampleStallReasonLookupRecordUuid;
            type = kRPVControllerPrimitiveTypeUInt64;
            break;
        }
        case kRPVComputeColumnPcSampleStallReasonLookupText:
        {
            property = kRPVControllerPCSamplingPcSampleStallReasonLookupText;
            type = kRPVControllerPrimitiveTypeString;
            break;
        }
        case kRPVComputeColumnPcSamplingInstructionTypeLookupUuid:
        {
            property = kRPVControllerPCSamplingInstructionTypeLookupUuid;
            type = kRPVControllerPrimitiveTypeUInt64;
            break;
        }
        case kRPVComputeColumnPcSamplingInstructionTypeLookupText:
        {
            property = kRPVControllerPCSamplingInstructionTypeLookupText;
            type = kRPVControllerPrimitiveTypeString;
            break;
        }
        case kRPVComputeColumnPcSamplingInstructionSampleUuid:
        {
            property = kRPVControllerPCSamplingInstructionSampleUuid;
            type = kRPVControllerPrimitiveTypeUInt64;
            break;
        }
        case kRPVComputeColumnPcSamplingInstructionSampleStateUuid:
        {
            property = kRPVControllerPCSamplingInstructionSampleStateUuid;
            type = kRPVControllerPrimitiveTypeUInt64;
            break;
        }
        case kRPVComputeColumnPcSamplingInstructionSampleLookupUuid:
        {
            property = kRPVControllerPCSamplingInstructionSampleLookupUuid;
            type = kRPVControllerPrimitiveTypeUInt64;
            break;
        }
        case kRPVComputeColumnPcSamplingInstructionSampleCount:
        {
            property = kRPVControllerPCSamplingInstructionSampleCount;
            type = kRPVControllerPrimitiveTypeUInt64;
            break;
        }
        case kRPVComputeColumnPcSamplingInstructionSampleLookupRecordUuid:
        {
            property = kRPVControllerPCSamplingInstructionSampleLookupRecordUuid;
            type = kRPVControllerPrimitiveTypeUInt64;
            break;
        }
        case kRPVComputeColumnPcSamplingInstructionSampleLookupText:
        {
            property = kRPVControllerPCSamplingInstructionSampleLookupText;
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
