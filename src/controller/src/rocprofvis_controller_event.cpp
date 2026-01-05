// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_controller_event.h"
#include "rocprofvis_controller_track.h"
#include "rocprofvis_controller_reference.h"
#include "rocprofvis_controller_array.h"
#include "rocprofvis_controller_ext_data.h"
#include "rocprofvis_controller_flow_control.h"
#include "rocprofvis_controller_call_stack.h"
#include "rocprofvis_controller_string_table.h"
#include "json.h"
#include <cstring>

namespace RocProfVis
{
namespace Controller
{

typedef Reference<rocprofvis_controller_track_t, Track, kRPVControllerObjectTypeTrack> TrackRef;

Event::Event(uint64_t id, double start_ts, double end_ts)
: Handle(__kRPVControllerEventPropertiesFirst, __kRPVControllerEventPropertiesLast)
, m_children(nullptr)
, m_id(id)
, m_start_timestamp(start_ts)
, m_end_timestamp(end_ts)
, m_name(UINT64_MAX)
, m_category(UINT64_MAX)
, m_combined_top_name(UINT64_MAX)
, m_level(0)
, m_retain_counter(0)
{}

Event& Event::operator=(Event&& other)
{
    m_children        = other.m_children;
    m_id              = other.m_id;
    m_start_timestamp = other.m_start_timestamp;
    m_end_timestamp   = other.m_end_timestamp;
    m_name            = other.m_name;
    m_category        = other.m_category;
    m_level           = other.m_level;    
    return *this;
}

Event::~Event()
{ 
    if(m_children)
    {
        delete m_children;
    }
}

bool
Event::IsDeletable()
{
    return --m_retain_counter <= 0;
}

void
Event::IncreaseRetainCounter()
{
    m_retain_counter++;
}

rocprofvis_controller_object_type_t Event::GetType(void) 
{
    return kRPVControllerObjectTypeEvent;
}

rocprofvis_result_t
Event::FetchDataModelFlowTraceProperty(uint64_t event_id, Array& array,
                                        rocprofvis_dm_trace_t dm_trace_handle)
{
    rocprofvis_result_t      result      = kRocProfVisResultUnknownError;
    rocprofvis_dm_event_id_t dm_event_id = *(rocprofvis_dm_event_id_t*) &event_id;
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
                    if(kRocProfVisDmResultSuccess == rocprofvis_db_future_wait(object, UINT64_MAX))
                    {
                        if(kRocProfVisDmResultSuccess ==
                               rocprofvis_dm_get_property_as_handle(
                                   dm_trace_handle, kRPVDMFlowTraceHandleByEventID, event_id,
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
                                result = array.SetUInt64(kRPVControllerArrayNumEntries, 0, records_count);
                                ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
                                for(int index = 0; index < records_count; index++)
                                {
                                    uint64_t id   = 0;
                                    uint64_t start_timestamp = 0;
                                    uint64_t end_timestamp = 0;
                                    uint64_t track_id  = 0;
                                    uint64_t level     = 0;
                                    char* category  = nullptr;
                                    char* symbol    = nullptr;                                    
                                    if(kRocProfVisDmResultSuccess ==
                                           rocprofvis_dm_get_property_as_uint64(
                                               dm_flowtrace,
                                               kRPVDMEndpointIDUInt64Indexed, index,
                                               &id) &&
                                       kRocProfVisDmResultSuccess ==
                                           rocprofvis_dm_get_property_as_uint64(
                                               dm_flowtrace,
                                               kRPVDMEndpointTimestampUInt64Indexed,
                                               index, &start_timestamp) &&
                                       kRocProfVisDmResultSuccess ==
                                           rocprofvis_dm_get_property_as_uint64(
                                               dm_flowtrace,
                                               kRPVDMEndpointEndTimestampUInt64Indexed,
                                               index, &end_timestamp) &&
                                       kRocProfVisDmResultSuccess ==
                                           rocprofvis_dm_get_property_as_uint64(
                                               dm_flowtrace,
                                               kRPVDMEndpointTrackIDUInt64Indexed, index,
                                               &track_id) &&
                                       kRocProfVisDmResultSuccess ==
                                           rocprofvis_dm_get_property_as_charptr(
                                               dm_flowtrace,
                                               kRPVDMEndpointCategoryCharPtrIndexed,
                                               index, &category) &&
                                       kRocProfVisDmResultSuccess ==
                                           rocprofvis_dm_get_property_as_charptr(
                                               dm_flowtrace,
                                               kRPVDMEndpointSymbolCharPtrIndexed,
                                               index, &symbol) &&
                                       kRocProfVisDmResultSuccess ==
                                           rocprofvis_dm_get_property_as_uint64(
                                               dm_flowtrace,
                                               kRPVDMEndpointLevelUInt64Indexed, index,
                                               &level)
                                        )
                                    {
                                        FlowControl* flow_control = new FlowControl(
                                            id, start_timestamp, end_timestamp, track_id,
                                            level,
                                            dm_event_id.bitfield.event_op ==
                                                        kRocProfVisDmOperationLaunch ||
                                            dm_event_id.bitfield.event_op ==
                                                        kRocProfVisDmOperationLaunchSample
                                                ? 0
                                                : 1,
                                                category, symbol);
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
                    dm_trace_handle, kRPVDMEventFlowTrace, dm_event_id);
            }
            rocprofvis_db_future_free(object);
        }
    }
    return result;
}

rocprofvis_result_t
Event::FetchDataModelStackTraceProperty(uint64_t event_id, Array& array,
                                          rocprofvis_dm_trace_t dm_trace_handle)
{
    rocprofvis_result_t      result      = kRocProfVisResultUnknownError;
    rocprofvis_dm_event_id_t dm_event_id = *(rocprofvis_dm_event_id_t*) &event_id;
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
                    if(kRocProfVisDmResultSuccess == rocprofvis_db_future_wait(object, UINT64_MAX))
                    {
                        if(kRocProfVisDmResultSuccess ==
                               rocprofvis_dm_get_property_as_handle(
                                   dm_trace_handle, kRPVDMStackTraceHandleByEventID, event_id,
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
                                        std::string file;
                                        std::string pc;
                                        std::string name;
                                        std::string line_name;
                                        std::string line_address;

                                        std::pair<jt::Json::Status, jt::Json> parsed = jt::Json::parse(symbol);
                                        if(parsed.first == jt::Json::Status::success)
                                        {
                                            file = FromJson("file", parsed.second);
                                            pc = FromJson("pc", parsed.second);
                                            name = FromJson("name", parsed.second);
                                        }
                                        parsed = jt::Json::parse(codeline);
                                        if(parsed.first == jt::Json::Status::success)
                                        {
                                            line_name = FromJson("name", parsed.second);
                                            line_address = FromJson("line_address", parsed.second);
                                        }
                                        
                                        if(!file.empty() || !pc.empty() || !name.empty() || !line_name.empty() || !line_address.empty())
                                        {
                                            CallStack* call_stack = new CallStack(file.c_str(), pc.c_str(), name.c_str(), line_name.c_str(), line_address.c_str());
                                            result = array.SetUInt64(kRPVControllerArrayNumEntries, 0, entry_counter + 1);
                                            if(result == kRocProfVisResultSuccess)
                                            {
                                                result = array.SetObject(kRPVControllerArrayEntryIndexed, entry_counter++, (rocprofvis_handle_t*) call_stack);
                                            }
                                            else
                                            {
                                                delete call_stack;
                                            }
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
Event::FetchDataModelExtendedDataProperty(uint64_t event_id, Array& array, rocprofvis_dm_trace_t dm_trace_handle)
{
    rocprofvis_result_t      result      = kRocProfVisResultUnknownError;
    rocprofvis_dm_event_id_t dm_event_id = *(rocprofvis_dm_event_id_t*) &event_id;
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
                    if(kRocProfVisDmResultSuccess == rocprofvis_db_future_wait(object, UINT64_MAX))
                    {
                        if(kRocProfVisDmResultSuccess ==
                               rocprofvis_dm_get_property_as_handle(
                                   dm_trace_handle, kRPVDMExtInfoHandleByEventID, event_id,
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
                                result = array.SetUInt64(kRPVControllerArrayNumEntries, 0, records_count);
                                for(int index = 0; index < records_count; index++)
                                {
                                    char* category = nullptr;
                                    char* name     = nullptr;
                                    char* value    = nullptr;
                                    uint64_t type     = 0;   
                                    uint64_t cat_enum = 0;
                                    if(kRocProfVisDmResultSuccess ==
                                           rocprofvis_dm_get_property_as_charptr(
                                               dm_extdata,
                                               kRPVDMExtDataCategoryCharPtrIndexed, index,
                                               &category) &&
                                       kRocProfVisDmResultSuccess ==
                                           rocprofvis_dm_get_property_as_charptr(
                                               dm_extdata,
                                               kRPVDMExtDataNameCharPtrIndexed, index,
                                               &name) &&
                                       kRocProfVisDmResultSuccess ==
                                           rocprofvis_dm_get_property_as_charptr(
                                               dm_extdata,
                                               kRPVDMExtDataValueCharPtrIndexed, index,
                                               &value) &&
                                       kRocProfVisDmResultSuccess ==
                                           rocprofvis_dm_get_property_as_uint64(
                                               dm_extdata,
                                               kRPVDMExtDataTypeUint64Indexed, index,
                                               &type) && 
                                       kRocProfVisDmResultSuccess ==
                                           rocprofvis_dm_get_property_as_uint64(
                                               dm_extdata,
                                               kRPVDMExtDataEnumUint64Indexed, index,
                                               &cat_enum))
                                    {
                                        ExtData* ext_data =
                                            new ExtData(category, name, value,
                                                        (rocprofvis_db_data_type_t) type, cat_enum);

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
    (void) array;
    rocprofvis_result_t result = kRocProfVisResultUnknownError;
    if(dm_trace_handle)
    {
        switch(property)
        {
            default:
            {
                result = kRocProfVisResultInvalidEnum;
                break;
            }
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
            {
                uint64_t size = 0;
                if (m_children)
                {
                    result = m_children->GetUInt64(property, index, &size);
                }
                else
                {
                    result = kRocProfVisResultSuccess;
                }
                size += sizeof(Event);
                *value = size;
                break;
            }
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
                result = m_children ? m_children->GetUInt64(kRPVControllerArrayNumEntries, 0, value) : kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerEventChildIndexed:
            {
                uint64_t num_children = 0;
                if (m_children 
                    && (m_children->GetUInt64(kRPVControllerArrayNumEntries, 0, &num_children) == kRocProfVisResultSuccess))
                {
                    if(index < num_children)
                    {
                        result = m_children->GetUInt64(kRPVControllerArrayEntryIndexed, index, value);
                    }
                    else
                    {
                        result = kRocProfVisResultOutOfRange;
                    }
                }
                break;
            }
            case kRPVControllerEventLevel:
            {
                *value = m_level;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerEventNameStrIndex:
            {
                *value = m_name;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerEventCategoryStrIndex:
            {
                *value = m_category;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerEventTopCombinedNameStrIndex:
            {
                *value = m_combined_top_name;
                result = kRocProfVisResultSuccess;
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

rocprofvis_result_t Event::GetDouble(rocprofvis_property_t property, uint64_t index, double* value) 
{
    (void) index;
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
            case kRPVControllerEventDuration:
            {
                *value = m_end_timestamp - m_start_timestamp;
                result = kRocProfVisResultSuccess;
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

rocprofvis_result_t Event::GetString(rocprofvis_property_t property, uint64_t index, char* value, uint32_t* length) 
{
    (void) index;
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerEventName:
        {
            char const* name     = StringTable::Get().GetString(m_name);
            result = GetStringImpl(value, length, name, static_cast<uint32_t>(strlen(name)));
            break;
        }
        case kRPVControllerEventCategory:
        {
            char const* category = StringTable::Get().GetString(m_category);
            result = GetStringImpl(value, length, category, static_cast<uint32_t>(strlen(category)));
            break;
        }
        case kRPVControllerEventTopCombinedName:
        {
            char const* combined_name = StringTable::Get().GetString(m_combined_top_name);
            result = GetStringImpl(value, length, combined_name, static_cast<uint32_t>(strlen(combined_name)));
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
            try
            {
                if(!m_children)
                {
                    m_children = new Array;
                }
                result = m_children->SetUInt64(kRPVControllerArrayNumEntries, 0, value);
            }
            catch(std::exception const&)
            {
                result = kRocProfVisResultMemoryAllocError;
            }
            break;
        }
        case kRPVControllerEventChildIndexed:
        {
            if(m_children)
            {
                result = m_children->SetUInt64(kRPVControllerArrayEntryIndexed, index, value);
            }
            break;
        }
        case kRPVControllerEventLevel:
        {
            // Currently the level should be an 8bit unsigned integer
            // Anything beyond 255 will probably not display well.
            // TODO: review this
            ROCPROFVIS_ASSERT(value < 256);
            m_level = std::min( static_cast<uint8_t>(value), static_cast<uint8_t>(255));
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerEventTopCombinedNameStrIndex:
        {
            // Set string index for top combined name
            m_combined_top_name = static_cast<size_t>(value);
            result = kRocProfVisResultSuccess;
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

rocprofvis_result_t Event::SetDouble(rocprofvis_property_t property, uint64_t index, double value) 
{
    (void) index;
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
        default:
        {
            result = UnhandledProperty(property);
            break;
        }
    }
    return result;
}

rocprofvis_result_t Event::SetString(rocprofvis_property_t property, uint64_t index, char const* value) 
{
    (void) index;
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if (value)
    {
        switch(property)
        {
            case kRPVControllerEventName:
            {
                m_name = StringTable::Get().AddString(value, *value != 0);
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerEventCategory:
            {
                m_category = StringTable::Get().AddString(value, *value != 0);
                result = kRocProfVisResultSuccess;
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

std::string 
Event::FromJson(const char* key, jt::Json& json)
{
    std::string value;
    if(json[key].isString())
    {
        value = json[key].toString();
        if(value.length() > 1 && value.front() == '\"' && value.back() == '\"')
        {
            value = value.substr(1, value.length() - 2);
        }
    }
    return value;
}

}
}
