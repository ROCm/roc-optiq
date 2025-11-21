// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_controller_flow_control.h"
#include "rocprofvis_controller_reference.h"

namespace RocProfVis
{
namespace Controller
{

typedef Reference<rocprofvis_controller_flow_control_t, FlowControl, kRPVControllerObjectTypeFlowControl> FlowControlRef;

FlowControl::FlowControl(uint64_t id, uint64_t start_timestamp,uint64_t end_timestamp,uint32_t track_id,
                         uint32_t level, uint32_t direction, const char* category,
                         const char* symbol)
: Handle(__kRPVControllerFlowControlPropertiesFirst, __kRPVControllerFlowControlPropertiesLast)
, m_id(id)
, m_start_timestamp(start_timestamp)
, m_end_timestamp(end_timestamp)    
, m_track_id(track_id)
, m_level(level)
, m_direction(direction)
, m_category(category)
, m_symbol(symbol)
{}

FlowControl::~FlowControl() {}

rocprofvis_controller_object_type_t FlowControl::GetType(void) 
{
    return kRPVControllerObjectTypeFlowControl;
}

rocprofvis_result_t FlowControl::GetUInt64(rocprofvis_property_t property, uint64_t index,
                                uint64_t* value) 
{
    (void) index;
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            case kRPVControllerCommonMemoryUsageInclusive:
            case kRPVControllerCommonMemoryUsageExclusive:
            {
                *value = sizeof(FlowControl);
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerFlowControltId:
            {
                *value = m_id;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerFlowControlTimestamp:
            {
                *value = m_start_timestamp;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerFlowControlEndTimestamp:
            {
                *value = m_end_timestamp;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerFlowControlTrackId:
            {
                *value = m_track_id;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerFlowControlDirection:
            {
                *value = m_direction;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerFlowControlLevel:
            {
                *value = m_level;
                result = kRocProfVisResultSuccess;
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

rocprofvis_result_t FlowControl::GetString(rocprofvis_property_t property, uint64_t index,
                                char* value, uint32_t* length) 
{
    (void) index;
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerFlowControlName:
        {
            if(*length == 0)
            {
                result = m_symbol.GetString(value, length);
                if(result != kRocProfVisResultSuccess || *length == 0)
                {
                    result = m_category.GetString(value, length);
                }
            }
            else
            {
                uint32_t len = 0;
                result       = m_symbol.GetString(value, &len);
                if(result != kRocProfVisResultSuccess || len == 0)
                {
                    result = m_category.GetString(value, length);
                }
                else
                {
                    result = m_symbol.GetString(value, length);
                }
            }
            break;
        }
        case kRPVControllerFlowControltId:
        case kRPVControllerFlowControlTimestamp:
        case kRPVControllerFlowControlTrackId:
        case kRPVControllerFlowControlDirection:
        case kRPVControllerFlowControlLevel:
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

}
}
