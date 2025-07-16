// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_controller_counter.h"
#include "rocprofvis_controller_node.h"
#include "rocprofvis_controller_process.h"
#include "rocprofvis_controller_processor.h"
#include "rocprofvis_controller_track.h"
#include "rocprofvis_controller_reference.h"
#include "rocprofvis_core_assert.h"

namespace RocProfVis
{
namespace Controller
{

typedef Reference<rocprofvis_controller_node_t, Node, kRPVControllerObjectTypeNode>
    NodeRef;
typedef Reference<rocprofvis_controller_process_t, Process, kRPVControllerObjectTypeProcess>
    ProcessRef;
typedef Reference<rocprofvis_controller_processor_t, Processor, kRPVControllerObjectTypeProcessor>
    ProcessorRef;
typedef Reference<rocprofvis_controller_track_t, Track, kRPVControllerObjectTypeTrack>
    TrackRef;

Counter::Counter()
: m_node(nullptr)
, m_process(nullptr)
, m_processor(nullptr)
, m_track(nullptr)
, m_id(0)
, m_event_code(0)
, m_instance_id(0)
, m_is_constant(false)
, m_is_derived(false)
{

}

Counter::~Counter() {}

rocprofvis_controller_object_type_t
Counter::GetType(void)
{
    return kRPVControllerObjectTypeCounter;
}

rocprofvis_result_t
Counter::GetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t* value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            case kRPVControllerCounterId:
            {
                *value = m_id;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerCounterEventCode:
            {
                *value = m_event_code;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerCounterInstanceId:
            {
                *value = m_instance_id;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerCounterIsConstant:
            {
                *value = m_is_constant;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerCounterIsDerived:
            {
                *value = m_is_derived;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerCounterTrack:
            case kRPVControllerCounterNode:
            case kRPVControllerCounterProcess:
            case kRPVControllerCounterProcessor:
            case kRPVControllerCounterName:
            case kRPVControllerCounterSymbol:
            case kRPVControllerCounterDescription:
            case kRPVControllerCounterExtendedDesc:
            case kRPVControllerCounterComponent:
            case kRPVControllerCounterUnits:
            case kRPVControllerCounterValueType:
            case kRPVControllerCounterBlock:
            case kRPVControllerCounterExpression:
            case kRPVControllerCounterGuid:
            case kRPVControllerCounterExtData:
            case kRPVControllerCounterTargetArch:
            {
                result = kRocProfVisResultInvalidType;
                break;
            }
            default:
            {
                result = kRocProfVisResultInvalidEnum;
                break;
            }
        }
    }
    return result;
}

rocprofvis_result_t
Counter::GetDouble(rocprofvis_property_t property, uint64_t index, double* value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerCounterTrack:
        case kRPVControllerCounterId:
        case kRPVControllerCounterNode:
        case kRPVControllerCounterProcess:
        case kRPVControllerCounterProcessor:
        case kRPVControllerCounterName:
        case kRPVControllerCounterSymbol:
        case kRPVControllerCounterDescription:
        case kRPVControllerCounterExtendedDesc:
        case kRPVControllerCounterComponent:
        case kRPVControllerCounterUnits:
        case kRPVControllerCounterValueType:
        case kRPVControllerCounterBlock:
        case kRPVControllerCounterExpression:
        case kRPVControllerCounterGuid:
        case kRPVControllerCounterExtData:
        case kRPVControllerCounterTargetArch:
        case kRPVControllerCounterEventCode:
        case kRPVControllerCounterInstanceId:
        case kRPVControllerCounterIsConstant:
        case kRPVControllerCounterIsDerived:
        {
            result = kRocProfVisResultInvalidType;
            break;
        }
        default:
        {
            result = kRocProfVisResultInvalidEnum;
            break;
        }
    }
    return result;
}

rocprofvis_result_t
Counter::GetObject(rocprofvis_property_t property, uint64_t index,
                     rocprofvis_handle_t** value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            case kRPVControllerCounterNode:
            {
                *value = (rocprofvis_handle_t*) m_node;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerCounterProcess:
            {
                *value = (rocprofvis_handle_t*) m_process;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerCounterProcessor:
            {
                *value = (rocprofvis_handle_t*) m_processor;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerCounterTrack:
            {
                *value = (rocprofvis_handle_t*) m_track;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerCounterId:
            case kRPVControllerCounterName:
            case kRPVControllerCounterSymbol:
            case kRPVControllerCounterDescription:
            case kRPVControllerCounterExtendedDesc:
            case kRPVControllerCounterComponent:
            case kRPVControllerCounterUnits:
            case kRPVControllerCounterValueType:
            case kRPVControllerCounterBlock:
            case kRPVControllerCounterExpression:
            case kRPVControllerCounterGuid:
            case kRPVControllerCounterExtData:
            case kRPVControllerCounterTargetArch:
            case kRPVControllerCounterEventCode:
            case kRPVControllerCounterInstanceId:
            case kRPVControllerCounterIsConstant:
            case kRPVControllerCounterIsDerived:
            {
                result = kRocProfVisResultInvalidType;
                break;
            }
            default:
            {
                result = kRocProfVisResultInvalidEnum;
                break;
            }
        }
    }
    return result;
}

rocprofvis_result_t
Counter::GetString(rocprofvis_property_t property, uint64_t index, char* value,
                     uint32_t* length)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerCounterName:
        {
            result = GetStdStringImpl(value, length, m_name);
            break;
        }
        case kRPVControllerCounterSymbol:
        {
            result = GetStdStringImpl(value, length, m_symbol);
            break;
        }
        case kRPVControllerCounterDescription:
        {
            result = GetStdStringImpl(value, length, m_description);
            break;
        }
        case kRPVControllerCounterExtendedDesc:
        {
            result = GetStdStringImpl(value, length, m_long_description);
            break;
        }
        case kRPVControllerCounterComponent:
        {
            result = GetStdStringImpl(value, length, m_component);
            break;
        }
        case kRPVControllerCounterUnits:
        {
            result = GetStdStringImpl(value, length, m_units);
            break;
        }
        case kRPVControllerCounterValueType:
        {
            result = GetStdStringImpl(value, length, m_value_type);
            break;
        }
        case kRPVControllerCounterBlock:
        {
            result = GetStdStringImpl(value, length, m_block);
            break;
        }
        case kRPVControllerCounterExpression:
        {
            result = GetStdStringImpl(value, length, m_block);
            break;
        }
        case kRPVControllerCounterGuid:
        {
            result = GetStdStringImpl(value, length, m_guid);
            break;
        }
        case kRPVControllerCounterExtData:
        {
            result = GetStdStringImpl(value, length, m_extdata);
            break;
        }
        case kRPVControllerCounterTargetArch:
        {
            result = GetStdStringImpl(value, length, m_target_arch);
            break;
        }
        case kRPVControllerCounterTrack:
        case kRPVControllerCounterId:
        case kRPVControllerCounterNode:
        case kRPVControllerCounterProcess:
        case kRPVControllerCounterProcessor:
        case kRPVControllerCounterEventCode:
        case kRPVControllerCounterInstanceId:
        case kRPVControllerCounterIsConstant:
        case kRPVControllerCounterIsDerived:
        {
            result = kRocProfVisResultInvalidType;
            break;
        }
        default:
        {
            result = kRocProfVisResultInvalidEnum;
            break;
        }
    }
    return result;
}

rocprofvis_result_t
Counter::SetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerCounterId:
        {
            m_id   = value;
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerCounterEventCode:
        {
            m_event_code   = value;
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerCounterInstanceId:
        {
            m_instance_id   = value;
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerCounterIsConstant:
        {
            m_is_constant   = value ? true : false;
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerCounterIsDerived:
        {
            m_is_derived   = value ? true : false;
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerCounterTrack:
        case kRPVControllerCounterNode:
        case kRPVControllerCounterProcess:
        case kRPVControllerCounterProcessor:
        case kRPVControllerCounterName:
        case kRPVControllerCounterSymbol:
        case kRPVControllerCounterDescription:
        case kRPVControllerCounterExtendedDesc:
        case kRPVControllerCounterComponent:
        case kRPVControllerCounterUnits:
        case kRPVControllerCounterValueType:
        case kRPVControllerCounterBlock:
        case kRPVControllerCounterExpression:
        case kRPVControllerCounterGuid:
        case kRPVControllerCounterExtData:
        case kRPVControllerCounterTargetArch:
        {
            result = kRocProfVisResultInvalidType;
            break;
        }
        default:
        {
            result = kRocProfVisResultInvalidEnum;
            break;
        }
    }
    return result;
}

rocprofvis_result_t
Counter::SetDouble(rocprofvis_property_t property, uint64_t index, double value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerCounterTrack:
        case kRPVControllerCounterId:
        case kRPVControllerCounterNode:
        case kRPVControllerCounterProcess:
        case kRPVControllerCounterProcessor:
        case kRPVControllerCounterName:
        case kRPVControllerCounterSymbol:
        case kRPVControllerCounterDescription:
        case kRPVControllerCounterExtendedDesc:
        case kRPVControllerCounterComponent:
        case kRPVControllerCounterUnits:
        case kRPVControllerCounterValueType:
        case kRPVControllerCounterBlock:
        case kRPVControllerCounterExpression:
        case kRPVControllerCounterGuid:
        case kRPVControllerCounterExtData:
        case kRPVControllerCounterTargetArch:
        case kRPVControllerCounterEventCode:
        case kRPVControllerCounterInstanceId:
        case kRPVControllerCounterIsConstant:
        case kRPVControllerCounterIsDerived:
        {
            result = kRocProfVisResultInvalidType;
            break;
        }
        default:
        {
            result = kRocProfVisResultInvalidEnum;
            break;
        }
    }
    return result;
}

rocprofvis_result_t
Counter::SetObject(rocprofvis_property_t property, uint64_t index,
                     rocprofvis_handle_t* value)

{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            case kRPVControllerCounterNode:
            {
                NodeRef ref(value);
                if(ref.IsValid())
                {
                    m_node = ref.Get();
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVControllerCounterProcess:
            {
                ProcessRef ref(value);
                if(ref.IsValid())
                {
                    m_process = ref.Get();
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVControllerCounterProcessor:
            {
                ProcessorRef ref(value);
                if(ref.IsValid())
                {
                    m_processor = ref.Get();
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVControllerCounterTrack:
            {
                TrackRef ref(value);
                if(ref.IsValid())
                {
                    m_track = ref.Get();
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVControllerCounterId:
            case kRPVControllerCounterName:
            case kRPVControllerCounterSymbol:
            case kRPVControllerCounterDescription:
            case kRPVControllerCounterExtendedDesc:
            case kRPVControllerCounterComponent:
            case kRPVControllerCounterUnits:
            case kRPVControllerCounterValueType:
            case kRPVControllerCounterBlock:
            case kRPVControllerCounterExpression:
            case kRPVControllerCounterGuid:
            case kRPVControllerCounterExtData:
            case kRPVControllerCounterTargetArch:
            case kRPVControllerCounterEventCode:
            case kRPVControllerCounterInstanceId:
            case kRPVControllerCounterIsConstant:
            case kRPVControllerCounterIsDerived:
            {
                result = kRocProfVisResultInvalidType;
                break;
            }
            default:
            {
                result = kRocProfVisResultInvalidEnum;
                break;
            }
        }
    }
    return result;
}

rocprofvis_result_t
Counter::SetString(rocprofvis_property_t property, uint64_t index, char const* value,
                     uint32_t length)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value && length)
    {
        switch(property)
        {
            case kRPVControllerCounterName:
            {
                m_name = value;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerCounterSymbol:
            {
                m_symbol = value;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerCounterDescription:
            {
                m_description = value;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerCounterExtendedDesc:
            {
                m_long_description = value;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerCounterComponent:
            {
                m_component = value;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerCounterUnits:
            {
                m_units = value;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerCounterValueType:
            {
                m_value_type = value;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerCounterBlock:
            {
                m_block = value;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerCounterExpression:
            {
                m_expression = value;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerCounterGuid:
            {
                m_guid = value;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerCounterExtData:
            {
                m_extdata = value;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerCounterTargetArch:
            {
                m_target_arch = value;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerCounterTrack:
            case kRPVControllerCounterId:
            case kRPVControllerCounterNode:
            case kRPVControllerCounterProcess:
            case kRPVControllerCounterProcessor:
            case kRPVControllerCounterEventCode:
            case kRPVControllerCounterInstanceId:
            case kRPVControllerCounterIsConstant:
            case kRPVControllerCounterIsDerived:
            {
                result = kRocProfVisResultInvalidType;
                break;
            }
            default:
            {
                result = kRocProfVisResultInvalidEnum;
                break;
            }
        }
    }
    return result;
}

}
}
