// Copyright (c) 2025 Advanced Micro Devices, Inc. All rights reserved.
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


RpvDmTrack::RpvDmTrack( RpvDmTrace* ctx,
                rocprofvis_dm_track_params_t* params) :
                m_ctx(ctx),
                m_track_params(params) {
                    rocprofvis_dm_event_id_t id = { 0 };
                    m_ext_data = RpvDmExtData(ctx,id);
                    params->extdata = &m_ext_data;
                };

rocprofvis_dm_slice_t  RpvDmTrack::AddSlice(rocprofvis_dm_timestamp_t start, rocprofvis_dm_timestamp_t end)
{
    ASSERT_MSG_RETURN(m_track_params, ERROR_TRACK_PARAMETERS_NOT_ASSIGNED, nullptr);
    if (nullptr == m_track_params) return nullptr;
    if (m_track_params->track_category == kRocProfVisDmPmcTrack)
    {
        try{
            m_slices.push_back(std::make_unique<RpvDmPmcTrackSlice>(this, start, end));
        }
        catch(std::exception ex)
        {
            ASSERT_ALWAYS_MSG_RETURN("Error! Falure allocating PMC track time slice!", nullptr);
        }
        return m_slices.back().get();
    } else
    {
        try{
            m_slices.push_back(std::make_unique<RpvDmEventTrackSlice>(this, start, end));
        }
        catch(std::exception ex)
        {
            ASSERT_ALWAYS_MSG_RETURN("Error! Falure allocating event track time slice!", nullptr);
        }
        return m_slices.back().get();
    }
}

rocprofvis_dm_result_t RpvDmTrack::DeleteSlice(rocprofvis_dm_slice_t slice)
{
    for (int i = 0; i < m_slices.size(); i++)
    {
        if (slice == m_slices[i].get())
        {
            m_slices.erase(m_slices.begin()+i);
            return kRocProfVisDmResultSuccess;
        }
    }
    return kRocProfVisDmResultNotLoaded;
}

rocprofvis_dm_result_t RpvDmTrack::GetSliceAtIndex(rocprofvis_dm_property_index_t index,  rocprofvis_dm_slice_t & slice)
{
    ASSERT_MSG_RETURN(index < m_slices.size(), ERROR_INDEX_OUT_OF_RANGE, kRocProfVisDmResultInvalidParameter);
    slice = m_slices[index].get();
    return kRocProfVisDmResultSuccess;
}

rocprofvis_dm_result_t RpvDmTrack::GetSliceAtTime(rocprofvis_dm_timestamp_t start,  rocprofvis_dm_slice_t & slice)
{
    for (int i = 0; i < m_slices.size(); i++)
    {
        if (start == m_slices[i]->StartTime())
        {
            slice = m_slices[i].get();
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
        size+=m_slices[i].get()->GetMemoryFootprint();
    }
    return size;
}


rocprofvis_dm_charptr_t  RpvDmTrack::CategoryString(){
    switch (m_track_params->track_category){
        case kRocProfVisDmPmcTrack: return "Counters Track";
        case kRocProfVisDmRegionTrack: return "Launch Track";
        case kRocProfVisDmKernelTrack: return "Dispatch Track";
        case kRocProfVisDmSQTTTrack: return "SQTT Track";
        case kRocProfVisDmNICTrack: return "NIC Track";
    }
    return "Invalid track";
}

rocprofvis_dm_result_t  RpvDmTrack::GetPropertyAsUint64(rocprofvis_dm_property_t property, rocprofvis_dm_property_index_t index, uint64_t* value){
    ASSERT_MSG_RETURN(value, ERROR_REFERENCE_POINTER_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
    ASSERT_MSG_RETURN(m_track_params, ERROR_TRACK_PARAMETERS_NOT_ASSIGNED, kRocProfVisDmResultNotLoaded);
    ASSERT_MSG_RETURN(m_track_params->extdata, ERROR_TRACK_PARAMETERS_NOT_ASSIGNED, kRocProfVisDmResultNotLoaded);
    switch(property)
    {
        case kRPVDMTrackCategoryEnumUInt64:           
            *value = Category();
            return kRocProfVisDmResultSuccess;
        case kRPVDMTrackIdUInt64:
            *value = TrackId();
            return kRocProfVisDmResultSuccess;
        case kRPVDMTrackNodeIdUInt64:
            *value = NodeId();
            return kRocProfVisDmResultSuccess;
        case kRPVDMNumberOfSlicesUInt64:
            *value = NumberOfSlices();
            return kRocProfVisDmResultSuccess;
        case kRPVDMNumberOfTrackExtDataRecordsUInt64:
            *value = m_ext_data.GetNumberOfRecords();
            return kRocProfVisDmResultSuccess;
        case kRPVDMTrackMemoryFootprintUInt64:
            *value = GetMemoryFootprint();
            return kRocProfVisDmResultSuccess;
        default:
            ASSERT_ALWAYS_MSG_RETURN(ERROR_INVALID_PROPERTY_GETTER, kRocProfVisDmResultInvalidProperty);
    }
}

 rocprofvis_dm_result_t   RpvDmTrack::GetPropertyAsCharPtr(rocprofvis_dm_property_t property, rocprofvis_dm_property_index_t index, char** value){
    ASSERT_MSG_RETURN(value, ERROR_REFERENCE_POINTER_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
    ASSERT_MSG_RETURN(m_track_params, ERROR_TRACK_PARAMETERS_NOT_ASSIGNED, kRocProfVisDmResultNotLoaded);
    ASSERT_MSG_RETURN(m_track_params->extdata, ERROR_TRACK_PARAMETERS_NOT_ASSIGNED, kRocProfVisDmResultNotLoaded);
    switch(property)
    {
        case kRPVDMTrackExtDataCategoryCharPtrIndexed:
            return m_ext_data.GetPropertyAsCharPtr(kRPVDMExtDataCategoryCharPtrIndexed, index, value);
	    case kRPVDMTrackExtDataNameCharPtrIndexed:
            return m_ext_data.GetPropertyAsCharPtr(kRPVDMExtDataNameCharPtrIndexed, index, value);
        case kRPVDMTrackExtDataValueCharPtrIndexed:
            return m_ext_data.GetPropertyAsCharPtr(kRPVDMExtDataValueCharPtrIndexed, index, value);
        case kRPVDMTrackInfoJsonCharPtr:
            return GetExtendedInfoAsJsonBlob(*(rocprofvis_dm_json_blob_t*)value);
        case kRPVDMTrackMainProcessNameCharPtr:
            *value = (char*)Process();
            return kRocProfVisDmResultSuccess;
        case kRPVDMTrackSubProcessNameCharPtr:
            *value = (char*)SubProcess();
            return kRocProfVisDmResultSuccess;
        case kRPVDMTrackCategoryEnumCharPtr:
            *value = (char*)CategoryString();
            return kRocProfVisDmResultSuccess;
        default:
            ASSERT_ALWAYS_MSG_RETURN(ERROR_INVALID_PROPERTY_GETTER, kRocProfVisDmResultInvalidProperty);
    }
}


rocprofvis_dm_result_t  RpvDmTrack::GetPropertyAsHandle(rocprofvis_dm_property_t property, rocprofvis_dm_property_index_t index, rocprofvis_dm_handle_t* value){
    ASSERT_MSG_RETURN(value, ERROR_REFERENCE_POINTER_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
    ASSERT_MSG_RETURN(m_track_params, ERROR_TRACK_PARAMETERS_NOT_ASSIGNED, kRocProfVisDmResultNotLoaded);
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

rocprofvis_dm_result_t RpvDmTrack::GetExtendedInfoAsJsonBlob(rocprofvis_dm_json_blob_t & json) {
    ASSERT_MSG_RETURN(m_track_params, ERROR_TRACK_PARAMETERS_NOT_ASSIGNED, kRocProfVisDmResultNotLoaded);
    ASSERT_MSG_RETURN(m_track_params->extdata, ERROR_TRACK_PARAMETERS_NOT_ASSIGNED, kRocProfVisDmResultNotLoaded);
    return m_ext_data.GetPropertyAsCharPtr(kRPVDMExtDataJsonBlobCharPtr, 0, (char**) & json);
}


#ifdef TEST
const char*  RpvDmTrack::GetPropertySymbol(rocprofvis_dm_property_t property) {
    switch(property)
    {
        case kRPVDMTrackCategoryEnumUInt64:
            return "kRPVDMTrackCategoryEnumUInt64";        
        case kRPVDMTrackIdUInt64:
            return "kRPVDMTrackIdUInt64";
        case kRPVDMTrackNodeIdUInt64:
            return "kRPVDMTrackNodeIdUInt64";
        case kRPVDMNumberOfSlicesUInt64:
            return "kRPVDMNumberOfSlicesUInt64";
        case kRPVDMNumberOfTrackExtDataRecordsUInt64:
            return "kRPVDMNumberOfTrackExtDataRecordsUInt64";
        case kRPVDMTrackMemoryFootprintUInt64:
            return "kRPVDMTrackMemoryFootprintUInt64";
        case kRPVDMTrackExtDataCategoryCharPtrIndexed:
            return "kRPVDMTrackExtDataCategoryCharPtrIndexed";
        case kRPVDMTrackExtDataNameCharPtrIndexed:
            return "kRPVDMTrackExtDataNameCharPtrIndexed";
        case kRPVDMTrackExtDataValueCharPtrIndexed:
            return "kRPVDMTrackExtDataValueCharPtrIndexed";
        case kRPVDMTrackInfoJsonCharPtr:
            return "kRPVDMTrackInfoJsonCharPtr";
        case kRPVDMTrackMainProcessNameCharPtr:
            return "kRPVDMTrackMainProcessNameCharPtr";
        case kRPVDMTrackSubProcessNameCharPtr:
            return "kRPVDMTrackSubProcessNameCharPtr";
        case kRPVDMTrackCategoryEnumCharPtr:
            return "kRPVDMTrackCategoryEnumCharPtr";
        case kRPVDMSliceHandleIndexed:
            return "kRPVDMSliceHandleIndexed";
        case kRPVDMSliceHandleTimed:
            return "kRPVDMSliceHandleTimed";
        default:
            return "Unknown property";
    }   
}
#endif