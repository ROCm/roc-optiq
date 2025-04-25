// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_controller_event.h"
#include "rocprofvis_controller_track.h"
#include "rocprofvis_controller_reference.h"
#include "rocprofvis_controller_array.h"
#include "rocprofvis_controller_ext_data.h"
#include "rocprofvis_controller_flow_control.h"
#include "rocprofvis_controller_call_stack.h"
#include "rocprofvis_controller_string_table.h"
#include <cstring>

namespace RocProfVis
{
namespace Controller
{

typedef Reference<rocprofvis_controller_track_t, Track, kRPVControllerObjectTypeTrack> TrackRef;

Event::Event(uint64_t id, double start_ts, double end_ts)
: m_id(id)
, m_start_timestamp(start_ts)
, m_end_timestamp(end_ts)
, m_name(UINT64_MAX)
, m_category(UINT64_MAX)
{
}

Event::~Event()
{ 
}

rocprofvis_controller_object_type_t Event::GetType(void) 
{
    return kRPVControllerObjectTypeEvent;
}


rocprofvis_result_t
Event::FetchDataModelFlowTraceProperty(Array&                array,
                                        rocprofvis_dm_trace_t dm_trace_handle)
{
    rocprofvis_result_t      result      = kRocProfVisResultUnknownError;
    rocprofvis_dm_event_id_t dm_event_id = *(rocprofvis_dm_event_id_t*) &m_id;
    if(dm_trace_handle)
    {
        rocprofvis_dm_stacktrace_t dm_flowtrace = nullptr;
        rocprofvis_dm_database_t   db            = rocprofvis_dm_get_property_as_handle(
            dm_trace_handle, kRPVDMDatabaseHandle, 0);
        if(db != nullptr)
        {
            rocprofvis_db_future_t object = rocprofvis_db_future_alloc(nullptr);
            if(object != nullptr)
            {
                if(kRocProfVisDmResultSuccess ==
                   rocprofvis_db_read_event_property_async(db, kRPVDMEventFlowTrace,
                                                           dm_event_id, object))
                {
                    if(kRocProfVisDmResultSuccess == rocprofvis_db_future_wait(object, 2))
                    {
                        if(kRocProfVisDmResultSuccess ==
                               rocprofvis_dm_get_property_as_handle(
                                   dm_trace_handle, kRPVDMStackTraceHandleByEventID, m_id,
                                   &dm_flowtrace) &&
                           dm_flowtrace != nullptr)
                        {
                            uint64_t records_count = 0;
                            if(kRocProfVisDmResultSuccess ==
                               rocprofvis_dm_get_property_as_uint64(
                                   dm_flowtrace, kRPVDMNumberOfEndpointsUInt64, 0,
                                   (uint64_t*) &records_count))
                            {
                                uint64_t entry_counter = 0;
                                for(int index = 0; index < records_count; index++)
                                {
                                    uint64_t id   = 0;
                                    uint64_t timestamp = 0;
                                    uint64_t track_id  = 0;
                                    char* codeline = nullptr;
                                    if(kRocProfVisDmResultSuccess ==
                                           rocprofvis_dm_get_property_as_uint64(
                                               dm_flowtrace,
                                               kRPVDMEndpointIDUInt64Indexed, index,
                                               &id) &&
                                       kRocProfVisDmResultSuccess ==
                                           rocprofvis_dm_get_property_as_uint64(
                                               dm_flowtrace,
                                               kRPVDMEndpointTimestampUInt64Indexed,
                                               index,
                                               &timestamp) &&
                                       kRocProfVisDmResultSuccess ==
                                           rocprofvis_dm_get_property_as_uint64(
                                               dm_flowtrace,
                                               kRPVDMEndpointTrackIDUInt64Indexed, index,
                                               &track_id))
                                    {
                                        FlowControl* flow_control = new FlowControl(
                                            id, timestamp, track_id,
                                            dm_event_id.bitfield.event_op ==
                                                kRocProfVisDmOperationLaunch ? 0 : 1);
                                        result = array.SetUInt64(
                                            kRPVControllerArrayNumEntries, 0, 1);
                                        if(result == kRocProfVisResultSuccess)
                                        {
                                            result = array.SetObject(
                                                kRPVControllerArrayEntryIndexed,
                                                entry_counter++,
                                                (rocprofvis_handle_t*) flow_control);
                                        }
                                    }
                                }
                            }
                            result = kRocProfVisResultSuccess;
                        }
                        else
                            result = kRocProfVisResultUnknownError;
                    }
                    else
                    {
                        result = kRocProfVisResultTimeout;
                    }
                }
                rocprofvis_dm_delete_event_property_for(
                    dm_trace_handle, kRPVDMEventStackTrace, dm_event_id);
            }
            rocprofvis_db_future_free(object);
        }
    }
    return result;
}

rocprofvis_result_t
Event::FetchDataModelStackTraceProperty(Array& array,
                                          rocprofvis_dm_trace_t dm_trace_handle)
{
    rocprofvis_result_t      result      = kRocProfVisResultUnknownError;
    rocprofvis_dm_event_id_t dm_event_id = *(rocprofvis_dm_event_id_t*) &m_id;
    if(dm_trace_handle)
    {
        rocprofvis_dm_stacktrace_t dm_stacktrace = nullptr;
        rocprofvis_dm_database_t   db         = rocprofvis_dm_get_property_as_handle(
            dm_trace_handle, kRPVDMDatabaseHandle, 0);
        if(db != nullptr)
        {
            rocprofvis_db_future_t object = rocprofvis_db_future_alloc(nullptr);
            if(object != nullptr)
            {
                if(kRocProfVisDmResultSuccess ==
                   rocprofvis_db_read_event_property_async(db, kRPVDMEventStackTrace,
                                                           dm_event_id, object))
                {
                    if(kRocProfVisDmResultSuccess == rocprofvis_db_future_wait(object, 2))
                    {
                        if(kRocProfVisDmResultSuccess ==
                               rocprofvis_dm_get_property_as_handle(
                                   dm_trace_handle, kRPVDMStackTraceHandleByEventID, m_id,
                                   &dm_stacktrace) &&
                           dm_stacktrace != nullptr)
                        {
                            uint64_t records_count = 0;
                            if(kRocProfVisDmResultSuccess ==
                               rocprofvis_dm_get_property_as_uint64(
                                   dm_stacktrace, kRPVDMNumberOfFramesUInt64, 0,
                                   (uint64_t*) &records_count))
                            {
                                uint64_t entry_counter = 0;
                                for(int index = 0; index < records_count; index++)
                                {
                                    char* symbol = nullptr;
                                    char* args     = nullptr;
                                    char* codeline    = nullptr;
                                    if(kRocProfVisDmResultSuccess ==
                                           rocprofvis_dm_get_property_as_charptr(
                                               dm_stacktrace,
                                               kRPVDMFrameSymbolCharPtrIndexed, index,
                                               &symbol) &&
                                       kRocProfVisDmResultSuccess ==
                                           rocprofvis_dm_get_property_as_charptr(
                                               dm_stacktrace,
                                               kRPVDMFrameArgsCharPtrIndexed, index,
                                               &args) &&
                                       kRocProfVisDmResultSuccess ==
                                           rocprofvis_dm_get_property_as_charptr(
                                               dm_stacktrace,
                                               kRPVDMFrameCodeLineCharPtrIndexed, index,
                                               &codeline))
                                    {
                                        CallStack* call_stack =
                                            new CallStack(symbol, args, codeline);
                                        result = array.SetUInt64(
                                            kRPVControllerArrayNumEntries, 0, 1);
                                        if(result == kRocProfVisResultSuccess)
                                        {
                                            result = array.SetObject(
                                                kRPVControllerArrayEntryIndexed,
                                                entry_counter++,
                                                (rocprofvis_handle_t*) call_stack);
                                        }
                                    }
                                }
                            }
                            result = kRocProfVisResultSuccess;
                        }
                        else
                            result = kRocProfVisResultUnknownError;
                    }
                    else
                    {
                        result = kRocProfVisResultTimeout;
                    }
                }
                rocprofvis_dm_delete_event_property_for(dm_trace_handle, kRPVDMEventStackTrace, dm_event_id);
            }
            rocprofvis_db_future_free(object);
        }
    }
    return result;
}

rocprofvis_result_t
Event::FetchDataModelExtendedDataProperty(Array& array, rocprofvis_dm_trace_t dm_trace_handle)
{
    rocprofvis_result_t      result      = kRocProfVisResultUnknownError;
    rocprofvis_dm_event_id_t dm_event_id = *(rocprofvis_dm_event_id_t*) &m_id;
    if(dm_trace_handle)
    {
        rocprofvis_dm_stacktrace_t dm_extdata = nullptr;
        rocprofvis_dm_database_t   db         = rocprofvis_dm_get_property_as_handle(
            dm_trace_handle, kRPVDMDatabaseHandle, 0);
        if(db != nullptr)
        {
            rocprofvis_db_future_t object = rocprofvis_db_future_alloc(nullptr);
            if(object != nullptr)
            {
                if(kRocProfVisDmResultSuccess ==
                   rocprofvis_db_read_event_property_async(db, kRPVDMEventExtData,
                                                           dm_event_id, object))
                {
                    if(kRocProfVisDmResultSuccess == rocprofvis_db_future_wait(object, 2))
                    {
                        if(kRocProfVisDmResultSuccess ==
                               rocprofvis_dm_get_property_as_handle(
                                   dm_trace_handle, kRPVDMExtInfoHandleByEventID, m_id,
                                   &dm_extdata) &&
                          dm_extdata != nullptr)
                        {
                            uint64_t records_count = 0;
                            if(kRocProfVisDmResultSuccess ==
                               rocprofvis_dm_get_property_as_uint64(
                                   dm_extdata, kRPVDMNumberOfExtDataRecordsUInt64, 0,
                                   (uint64_t*) &records_count))
                            {
                                uint64_t entry_counter = 0;
                                for(int index = 0; index < records_count; index++)
                                {
                                    char* category = nullptr;
                                    char* name     = nullptr;
                                    char* value    = nullptr;
                                    if(kRocProfVisDmResultSuccess ==
                                           rocprofvis_dm_get_property_as_charptr(
                                               dm_extdata,
                                               kRPVDMExtDataCategoryCharPtrIndexed, index,
                                               &category) &&
                                       kRocProfVisDmResultSuccess ==
                                           rocprofvis_dm_get_property_as_charptr(
                                               dm_extdata,
                                               kRPVDMExtDataCategoryCharPtrIndexed, index,
                                               &name) &&
                                       kRocProfVisDmResultSuccess ==
                                           rocprofvis_dm_get_property_as_charptr(
                                               dm_extdata,
                                               kRPVDMExtDataCategoryCharPtrIndexed, index,
                                               &value))
                                    {
                                        ExtData* ext_data =
                                            new ExtData(category, name, value);
                                        result = array.SetUInt64(
                                            kRPVControllerArrayNumEntries, 0, 1);
                                        if(result == kRocProfVisResultSuccess)
                                        {
                                            result = array.SetObject(
                                                kRPVControllerArrayEntryIndexed,
                                                entry_counter++,
                                                (rocprofvis_handle_t*) ext_data);
                                        }
                                    }
                                }
                            }
                            result = kRocProfVisResultSuccess;
                        }
                        else
                            result = kRocProfVisResultUnknownError;
                    }
                    else
                    {
                        result = kRocProfVisResultTimeout;
                    }
                }
                rocprofvis_dm_delete_event_property_for(dm_trace_handle,
                                                        kRPVDMEventExtData, dm_event_id);
            }
            rocprofvis_db_future_free(object);
        }
    }
    return result;
}

rocprofvis_result_t
Event::Fetch(rocprofvis_property_t property, Array& array,
             rocprofvis_dm_trace_t dm_trace_handle)
{
    rocprofvis_result_t      result      = kRocProfVisResultUnknownError;
    if(dm_trace_handle)
    {
        switch(property)
        {
            case kRPVControllerEventDataExtData:
                result = FetchDataModelExtendedDataProperty(array, dm_trace_handle);
                break;
            case kRPVControllerEventDataCallStack:
                result = FetchDataModelStackTraceProperty(array, dm_trace_handle);
                break;
            case kRPVControllerEventDataFlowControl:
                result = FetchDataModelFlowTraceProperty(array, dm_trace_handle);
                break;
        }
    }
    return result;
}

rocprofvis_result_t Event::GetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t* value) 
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if (value)
    {
        switch(property)
        {
            case kRPVControllerCommonMemoryUsageInclusive:
            case kRPVControllerCommonMemoryUsageExclusive:
            {
                *value = sizeof(Event);
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerEventId:
            {
                *value = m_id;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerEventNumChildren:
            {
                *value = 0;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerEventStartTimestamp:
            case kRPVControllerEventEndTimestamp:
            case kRPVControllerEventName:
            case kRPVControllerEventChildIndexed:
            case kRPVControllerEventCallstackEntryIndexed:
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
rocprofvis_result_t Event::GetDouble(rocprofvis_property_t property, uint64_t index, double* value) 
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if (value)
    {
        switch(property)
        {
            case kRPVControllerEventStartTimestamp:
            {
                *value = m_start_timestamp;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerEventEndTimestamp:
            {
                *value = m_end_timestamp;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerEventId:
            case kRPVControllerEventNumChildren:
            case kRPVControllerEventName:
            case kRPVControllerEventChildIndexed:
            case kRPVControllerEventCallstackEntryIndexed:
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
rocprofvis_result_t Event::GetObject(rocprofvis_property_t property, uint64_t index, rocprofvis_handle_t** value) 
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if (value)
    {
        switch(property)
        {
            case kRPVControllerEventStartTimestamp:
            case kRPVControllerEventEndTimestamp:
            case kRPVControllerEventId:
            case kRPVControllerEventNumChildren:
            case kRPVControllerEventName:
            case kRPVControllerEventChildIndexed:
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
rocprofvis_result_t Event::GetString(rocprofvis_property_t property, uint64_t index, char* value, uint32_t* length) 
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerEventName:
        {
            if (length && (!value || *length == 0))
            {
                char const* name = StringTable::Get().GetString(m_name);
                char const* category = StringTable::Get().GetString(m_category);
                ROCPROFVIS_ASSERT(name && category);
                *length = strlen(name) + strlen(category)+1;
                result = kRocProfVisResultSuccess;
            }
            else if (length && value && *length > 0)
            {
                char const* name = StringTable::Get().GetString(m_name);
                char const* category = StringTable::Get().GetString(m_category);
                ROCPROFVIS_ASSERT(name && category);
                std::string full_name = category;
                full_name += " ";
                full_name += name; 
                strncpy(value, full_name.c_str(), *length);
       
                result = kRocProfVisResultSuccess;
            }
            else
            {
                result = kRocProfVisResultInvalidArgument;
            }
            break;
        }
        case kRPVControllerEventCategory:
        {
            if (length && (!value || *length == 0))
            {
                char const* category = StringTable::Get().GetString(m_category);
                ROCPROFVIS_ASSERT(category);
                *length = strlen(category);
                result = kRocProfVisResultSuccess;
            }
            else if (length && value && *length > 0)
            {
                char const* category = StringTable::Get().GetString(m_category);
                ROCPROFVIS_ASSERT(category);
                strncpy(value, category, *length);
                result = kRocProfVisResultSuccess;
            }
            else
            {
                result = kRocProfVisResultInvalidArgument;
            }
            break;
        }
        case kRPVControllerEventStartTimestamp:
        case kRPVControllerEventEndTimestamp:
        case kRPVControllerEventId:
        case kRPVControllerEventNumChildren:
        case kRPVControllerEventChildIndexed:
        case kRPVControllerEventCallstackEntryIndexed:
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

rocprofvis_result_t Event::SetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t value) 
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerEventId:
        {
            result = kRocProfVisResultReadOnlyError;
            break;
        }
        case kRPVControllerEventNumChildren:
        {
            result = kRocProfVisResultReadOnlyError;
            break;
        }
        case kRPVControllerEventStartTimestamp:
        case kRPVControllerEventEndTimestamp:
        case kRPVControllerEventName:
        case kRPVControllerEventChildIndexed:
        case kRPVControllerEventCallstackEntryIndexed:
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
rocprofvis_result_t Event::SetDouble(rocprofvis_property_t property, uint64_t index, double value) 
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerEventStartTimestamp:
        {
            m_start_timestamp = value;
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerEventEndTimestamp:
        {
            m_end_timestamp = value;
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerEventId:
        case kRPVControllerEventNumChildren:
        case kRPVControllerEventName:
        case kRPVControllerEventChildIndexed:
        case kRPVControllerEventCallstackEntryIndexed:
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
rocprofvis_result_t Event::SetObject(rocprofvis_property_t property, uint64_t index, rocprofvis_handle_t* value) 
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if (value)
    {
        switch(property)
        {
            case kRPVControllerEventCallstackEntryIndexed:
            {
                result = kRocProfVisResultInvalidType;
                break;
            }
            case kRPVControllerEventStartTimestamp:
            case kRPVControllerEventEndTimestamp:
            case kRPVControllerEventId:
            case kRPVControllerEventNumChildren:
            case kRPVControllerEventName:
            case kRPVControllerEventChildIndexed:
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
rocprofvis_result_t Event::SetString(rocprofvis_property_t property, uint64_t index, char const* value, uint32_t length) 
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if (value)
    {
        switch(property)
        {
            case kRPVControllerEventName:
            {
                std::string name = value;
                m_name = StringTable::Get().AddString(name);
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerEventCategory:
            {
                std::string category = value;
                m_category = StringTable::Get().AddString(category);
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerEventStartTimestamp:
            case kRPVControllerEventEndTimestamp:
            case kRPVControllerEventId:
            case kRPVControllerEventNumChildren:
            case kRPVControllerEventChildIndexed:
            case kRPVControllerEventCallstackEntryIndexed:
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
