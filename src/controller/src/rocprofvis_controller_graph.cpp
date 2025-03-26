// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_controller_graph.h"
#include "rocprofvis_controller_track.h"
#include <cassert>

namespace RocProfVis
{
namespace Controller
{

Graph::Graph(rocprofvis_controller_graph_type_t type, uint64_t id)
: m_id(id)
, m_track(nullptr)
, m_type(type)
{
}

Graph::~Graph()
{
}

rocprofvis_result_t Graph::Fetch(uint32_t pixels, double start, double end, Array& array, uint64_t& index)
{
    rocprofvis_result_t result = kRocProfVisResultUnknownError;
    if(m_track)
    {
        uint32_t lod      = 0;
        double   duration = (end - start);
        while(duration > (pixels * 10.0))
        {
            duration /= 10.0;
            lod++;
        }
        result = m_track->Fetch(lod, start, end, array, index);
    }
    assert(result == kRocProfVisResultSuccess || result == kRocProfVisResultOutOfRange);
    return result;
}

rocprofvis_controller_object_type_t Graph::GetType(void)
{
    return kRPVControllerObjectTypeGraph;
}

rocprofvis_result_t Graph::GetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t* value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if (value)
    {
        switch (property)
        {
            case kRPVControllerGraphId:
            {
                *value = m_id;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerGraphType:
            {
                *value = m_type;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerGraphNumEntries:
            {
                *value = 0;
                result = m_track ? m_track->GetUInt64(kRPVControllerTrackNumberOfEntries, index, value) : kRocProfVisResultUnknownError;
                break;
            }
            case kRPVControllerGraphTrack:
            case kRPVControllerGraphStartTimestamp:
            case kRPVControllerGraphEndTimestamp:
            case kRPVControllerGraphEntryIndexed:
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
rocprofvis_result_t Graph::GetDouble(rocprofvis_property_t property, uint64_t index, double* value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            case kRPVControllerGraphStartTimestamp:
            {
                *value = 0;
                result = m_track ? m_track->GetDouble(kRPVControllerTrackMinTimestamp, index, value) : kRocProfVisResultUnknownError;
                break;
            }
            case kRPVControllerGraphEndTimestamp:
            {
                *value = 0;
                result = m_track ? m_track->GetDouble(kRPVControllerTrackMaxTimestamp, index, value) : kRocProfVisResultUnknownError;
                break;
            }
            case kRPVControllerGraphId:
            case kRPVControllerGraphType:
            case kRPVControllerGraphTrack:
            case kRPVControllerGraphNumEntries:
            case kRPVControllerGraphEntryIndexed:
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
rocprofvis_result_t Graph::GetObject(rocprofvis_property_t property, uint64_t index, rocprofvis_handle_t** value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            case kRPVControllerGraphTrack:
            {
                *value = (rocprofvis_handle_t*)m_track;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerGraphId:
            case kRPVControllerGraphType:
            case kRPVControllerGraphStartTimestamp:
            case kRPVControllerGraphEndTimestamp:
            case kRPVControllerGraphNumEntries:
            case kRPVControllerGraphEntryIndexed:
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
rocprofvis_result_t Graph::GetString(rocprofvis_property_t property, uint64_t index, char* value, uint32_t* length)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(length)
    {
        switch(property)
        {
            case kRPVControllerGraphId:
            case kRPVControllerGraphType:
            case kRPVControllerGraphTrack:
            case kRPVControllerGraphStartTimestamp:
            case kRPVControllerGraphEndTimestamp:
            case kRPVControllerGraphNumEntries:
            case kRPVControllerGraphEntryIndexed:
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

rocprofvis_result_t Graph::SetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerGraphId:
        {
            result = kRocProfVisResultReadOnlyError;
            break;
        }
        case kRPVControllerGraphType:
        {
            m_type = (rocprofvis_controller_graph_type_t)value;
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerGraphNumEntries:
        {
            result = m_track ? m_track->SetUInt64(kRPVControllerTrackNumberOfEntries, index, value) : kRocProfVisResultUnknownError;
            break;
        }
        case kRPVControllerGraphTrack:
        case kRPVControllerGraphStartTimestamp:
        case kRPVControllerGraphEndTimestamp:
        case kRPVControllerGraphEntryIndexed:
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
rocprofvis_result_t Graph::SetDouble(rocprofvis_property_t property, uint64_t index, double value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerGraphStartTimestamp:
        {
            result = m_track ? m_track->SetDouble(kRPVControllerTrackMinTimestamp, index, value) : kRocProfVisResultUnknownError;
            break;
        }
        case kRPVControllerGraphEndTimestamp:
        {
            result = m_track ? m_track->SetDouble(kRPVControllerTrackMaxTimestamp, index, value) : kRocProfVisResultUnknownError;
            break;
        }
        case kRPVControllerGraphId:
        case kRPVControllerGraphType:
        case kRPVControllerGraphTrack:
        case kRPVControllerGraphNumEntries:
        case kRPVControllerGraphEntryIndexed:
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
rocprofvis_result_t Graph::SetObject(rocprofvis_property_t property, uint64_t index, rocprofvis_handle_t* value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            case kRPVControllerGraphTrack:
            {
                m_track = (Track*)value;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerGraphId:
            case kRPVControllerGraphType:
            case kRPVControllerGraphStartTimestamp:
            case kRPVControllerGraphEndTimestamp:
            case kRPVControllerGraphNumEntries:
            case kRPVControllerGraphEntryIndexed:
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
rocprofvis_result_t Graph::SetString(rocprofvis_property_t property, uint64_t index, char const* value, uint32_t length)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            case kRPVControllerGraphId:
            case kRPVControllerGraphType:
            case kRPVControllerGraphTrack:
            case kRPVControllerGraphStartTimestamp:
            case kRPVControllerGraphEndTimestamp:
            case kRPVControllerGraphNumEntries:
            case kRPVControllerGraphEntryIndexed:
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
