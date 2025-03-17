// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_controller_trace.h"
#include "rocprofvis_controller_timeline.h"

#include <cassert>

namespace RocProfVis
{
namespace Controller
{

Trace::Trace()
: m_id(0)
, m_timeline(nullptr)
{
}

Trace::~Trace()
{
    delete m_timeline;
}

rocprofvis_result_t Trace::Load(char const* const filename, RocProfVis::Controller::Future& future)
{
    assert (filename && strlen(filename));
        
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;

    m_timeline = new Timeline();
    if (m_timeline)
    {
        result = m_timeline->Load(filename, future);
    }
    else
    {
        result = kRocProfVisResultMemoryAllocError;
    }

    return result;
}

rocprofvis_result_t Trace::AsyncFetch(Track& track, Future& future, Array& array,
                                double start, double end)
{
    rocprofvis_result_t error = kRocProfVisResultUnknownError;
    if(m_timeline)
    {
        error = m_timeline->AsyncFetch(track, future, array, start, end);
    }
    return error;
}

rocprofvis_controller_object_type_t Trace::GetType(void) 
{
    return kRPVControllerObjectTypeController;
}

rocprofvis_result_t Trace::GetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t* value) 
{
    assert(0);
    rocprofvis_result_t result = kRocProfVisResultUnknownError;
    if (value)
    {
        switch (property)
        {
            case kRPVControllerId:
            {
                *value = m_id;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerNumAnalysisView:
            {
                *value = 0;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerTimeline:
            case kRPVControllerEventTable:
            case kRPVControllerAnalysisViewIndexed:
            default:
            {
                result = kRocProfVisResultNotSupported;
                break;
            }
        }
    }
    return result;
}
rocprofvis_result_t Trace::GetDouble(rocprofvis_property_t property, uint64_t index, double* value) 
{
    assert(0);
    return kRocProfVisResultNotSupported;
}
rocprofvis_result_t Trace::GetObject(rocprofvis_property_t property, uint64_t index, rocprofvis_handle_t** value) 
{
    rocprofvis_result_t result = kRocProfVisResultUnknownError;
    if (value)
    {
        switch (property)
        {
            case kRPVControllerTimeline:
            {
                *value = (rocprofvis_handle_t*)m_timeline;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerEventTable:
            {
                assert(0);
                *value = nullptr;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerAnalysisViewIndexed:
            {
                assert(0);
                *value = nullptr;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerId:
            case kRPVControllerNumAnalysisView:
            default:
            {
                result = kRocProfVisResultNotSupported;
                break;
            }
        }
    }
    return result;
}
rocprofvis_result_t Trace::GetString(rocprofvis_property_t property, uint64_t index, char* value, uint32_t* length) 
{
    assert(0);
    return kRocProfVisResultNotSupported;
}

rocprofvis_result_t Trace::SetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t value) 
{
    assert(0);
    return kRocProfVisResultReadOnlyError;
}
rocprofvis_result_t Trace::SetDouble(rocprofvis_property_t property, uint64_t index, double value) 
{
    assert(0);
    return kRocProfVisResultReadOnlyError;
}
rocprofvis_result_t Trace::SetObject(rocprofvis_property_t property, uint64_t index, rocprofvis_handle_t* value) 
{
    assert(0);
    return kRocProfVisResultReadOnlyError;
}
rocprofvis_result_t Trace::SetString(rocprofvis_property_t property, uint64_t index, char const* value, uint32_t length) 
{
    assert(0);
    return kRocProfVisResultReadOnlyError;
}

}
}
