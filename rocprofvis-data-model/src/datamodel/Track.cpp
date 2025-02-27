// MIT License
//
// Copyright (c) 2023 Advanced Micro Devices, Inc. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "Track.h"
#include "Trace.h"
#include "PmcTrackSlice.h"
#include "EventTrackSlice.h"


rocprofvis_dm_slice_t  RpvDmTrack::AddSlice(rocprofvis_dm_timestamp_t start, rocprofvis_dm_timestamp_t end)
{
    ASSERT_MSG_RETURN(m_track_params, ERROR_TRACK_PARAMETERS_NOT_ASSIGNED, nullptr);
    if (nullptr == m_track_params) return nullptr;
    if (m_track_params->track_category == kRocProfVisDmPmcTrack)
    {
        RpvDmPmcTrackSlice * slice =  new RpvDmPmcTrackSlice(this, start, end);
        if (slice != nullptr) {
            m_slices.push_back(slice);
        }
        return slice;
    } else
    {
        RpvDmEventTrackSlice * slice =  new RpvDmEventTrackSlice(this, start, end);
        if (slice != nullptr) {
            m_slices.push_back(slice);
        }
        return slice;
    }
}

rocprofvis_dm_result_t RpvDmTrack::DeleteSlice(rocprofvis_dm_slice_t slice)
{
    for (int i = 0; i < m_slices.size(); i++)
    {
        if (slice == m_slices[i])
        {
            delete m_slices[i];
            m_slices.erase(m_slices.begin()+i);
            return kRocProfVisDmResultSuccess;
        }
    }
    return kRocProfVisDmResultNotLoaded;
}

rocprofvis_dm_result_t RpvDmTrack::GetSliceAtIndex(rocprofvis_dm_index_t index,  rocprofvis_dm_slice_t & slice)
{
    ASSERT_MSG_RETURN(index < m_slices.size(), ERROR_INDEX_OUT_OF_RANGE, kRocProfVisDmResultInvalidParameter);
    slice = m_slices[index];
    return kRocProfVisDmResultSuccess;
}

rocprofvis_dm_result_t RpvDmTrack::GetSliceAtTime(rocprofvis_dm_timestamp_t start,  rocprofvis_dm_slice_t & slice)
{
    for (int i = 0; i < m_slices.size(); i++)
    {
        if (start == m_slices[i]->StartTime())
        {
            slice = m_slices[i];
            return kRocProfVisDmResultSuccess;
        }
    }
    return kRocProfVisDmResultNotLoaded;
}

rocprofvis_dm_size_t   RpvDmTrack::GetMemoryFootprint()
{
    rocprofvis_dm_size_t size = 0;
    for (int i = 0; i < m_slices.size(); i++)
    {
        size+=m_slices[i]->GetMemoryFootprint();
    }
    return size;
}

rocprofvis_dm_result_t RpvDmTrack::GetExtendedInfo(rocprofvis_dm_json_blob_t & json) {
    ASSERT_MSG_RETURN(m_ctx, ERROR_TRACE_CANNOT_BE_NULL, kRocProfVisDmResultUnknownError);
    ASSERT_MSG_RETURN(m_track_params, ERROR_TRACK_CANNOT_BE_NULL, kRocProfVisDmResultUnknownError);
    return m_ctx->GetExtendedTrackInfo(m_track_params->track_id, json);
}


rocprofvis_dm_result_t  RpvDmTrack::GetPropertyAsUint64(rocprofvis_dm_track_property_t property, rocprofvis_dm_property_index_t index, uint64_t* value){
    
    switch(property)
    {
        case kRPVDMTrackCategoryEnumUInt64:
            ASSERT_MSG_RETURN(m_track_params, ERROR_TRACK_PARAMETERS_NOT_ASSIGNED, kRocProfVisDmResultNotLoaded);
            *value = Category();
            return kRocProfVisDmResultSuccess;
        case kRPVDMTrackIdUInt64:
            ASSERT_MSG_RETURN(m_track_params, ERROR_TRACK_PARAMETERS_NOT_ASSIGNED, kRocProfVisDmResultNotLoaded);
            *value = TrackId();
            return kRocProfVisDmResultSuccess;
        case kRPVDMTrackNodeIdUInt64:
            ASSERT_MSG_RETURN(m_track_params, ERROR_TRACK_PARAMETERS_NOT_ASSIGNED, kRocProfVisDmResultNotLoaded);
            *value = NodeId();
            return kRocProfVisDmResultSuccess;
        case kRPVDMTNumberOfSlicesUInt64:
            *value = NumberOfSlices();
            return kRocProfVisDmResultSuccess;
        case kRPVDMTrackMemoryFootprintUInt64:
            *value = GetMemoryFootprint();
            return kRocProfVisDmResultSuccess;
        default:
            ASSERT_ALWAYS_MSG_RETURN(ERROR_INVALID_PROPERTY_GETTER, kRocProfVisDmResultInvalidProperty);
    }

}

rocprofvis_dm_result_t    RpvDmTrack::GetPropertyAsInt64(rocprofvis_dm_track_property_t property, rocprofvis_dm_property_index_t index, int64_t* value){
    ASSERT_ALWAYS_MSG_RETURN(ERROR_INVALID_PROPERTY_GETTER, kRocProfVisDmResultInvalidProperty);
}
 rocprofvis_dm_result_t   RpvDmTrack::GetPropertyAsCharPtr(rocprofvis_dm_track_property_t property, rocprofvis_dm_property_index_t index, char** value){
    switch(property)
    {
        case kRPVDMTrackInfoJsonCharPtr:
            return GetExtendedInfo(*(rocprofvis_dm_json_blob_t*)value);
        case kRPVDMTrackMainProcessNameCharPtr:
            ASSERT_MSG_RETURN(m_track_params, ERROR_TRACK_PARAMETERS_NOT_ASSIGNED, kRocProfVisDmResultNotLoaded);
            *value = (char*)Process();
            return kRocProfVisDmResultSuccess;
        kRPVDMTrackSubProcessNameCharPtr:
            ASSERT_MSG_RETURN(m_track_params, ERROR_TRACK_PARAMETERS_NOT_ASSIGNED, kRocProfVisDmResultNotLoaded);
            *value = (char*)SubProcess();
            return kRocProfVisDmResultSuccess;
        default:
            ASSERT_ALWAYS_MSG_RETURN(ERROR_INVALID_PROPERTY_GETTER, kRocProfVisDmResultInvalidProperty);
    }

}

rocprofvis_dm_result_t   RpvDmTrack::GetPropertyAsDouble(rocprofvis_dm_track_property_t property, rocprofvis_dm_property_index_t index, double* value){

    ASSERT_ALWAYS_MSG_RETURN(ERROR_INVALID_PROPERTY_GETTER, kRocProfVisDmResultInvalidProperty);

}

rocprofvis_dm_result_t  RpvDmTrack::GetPropertyAsHandle(rocprofvis_dm_track_property_t property, rocprofvis_dm_property_index_t index, rocprofvis_dm_handle_t* value){
    switch(property)
    {
        case kRPVDMSliceHandleIndexed:
            return GetSliceAtIndex(index, *value);
        case kRPVDMSliceHandleTimed:
            return GetSliceAtTime(index, *value);
        default:
            ASSERT_ALWAYS_MSG_RETURN(ERROR_INVALID_PROPERTY_GETTER, kRocProfVisDmResultInvalidProperty);
    }
}