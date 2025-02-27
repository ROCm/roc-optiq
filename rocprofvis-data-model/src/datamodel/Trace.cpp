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

#include "Trace.h"
#include "Track.h"
#include "TrackSlice.h"

RpvDmTrace::RpvDmTrace()
{
    m_strings_mem_footprint=0;
    m_db = nullptr;
    m_parameters.start_time=0;
    m_parameters.end_time=0;
    m_parameters.strings_offset=0;
    m_parameters.symbols_offset=0;
    m_parameters.metadata_loaded=false;
}

rocprofvis_dm_result_t RpvDmTrace::BindDatabase(rocprofvis_dm_database_t db)
{
    ASSERT_MSG_RETURN(db, ERROR_DATABASE_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
    rocprofvis_dm_db_bind_struct bind_data = {0}; 
    bind_data.trace_object = this;
    bind_data.trace_parameters = &m_parameters;
    bind_data.FuncAddTrack = AddTrack;
    bind_data.FuncAddRecord = AddRecord;
    bind_data.FuncAddSlice = AddSlice;
    bind_data.FuncAddString = AddString;
    m_db = db;
    return kRocProfVisDmResultSuccess;
}
    
rocprofvis_dm_result_t RpvDmTrace::DeleteSliceAtTimeRange(rocprofvis_dm_timestamp_t start, rocprofvis_dm_timestamp_t end){

    for (int i=0; i < m_tracks.size(); i++)
    {
        rocprofvis_dm_slice_t slice = nullptr;
        rocprofvis_dm_result_t result = m_tracks[i].get()->GetSliceAtTime(start, slice);
        if (result == kRocProfVisDmResultSuccess)
        {
            ASSERT_MSG_RETURN(slice, ERROR_SLICE_CANNOT_BE_NULL, kRocProfVisDmResultUnknownError);
            if (kRocProfVisDmResultSuccess != m_tracks[i].get()->DeleteSlice(slice)) return kRocProfVisDmResultUnknownError;
        }
        else
            return result;
    }
    return kRocProfVisDmResultSuccess;
}

rocprofvis_dm_result_t RpvDmTrace::DeleteAllSlices(){
    for (int i=0; i < m_tracks.size(); i++)
    {
        rocprofvis_dm_size_t num_slices = m_tracks[i].get()->NumberOfSlices();
        for (int j=0; j < num_slices; j++)
        {
            rocprofvis_dm_slice_t slice = nullptr;
            rocprofvis_dm_result_t result = m_tracks[i].get()->GetSliceAtIndex(j, slice);
            if (result == kRocProfVisDmResultSuccess)
            {
                ASSERT_MSG_RETURN(slice, ERROR_SLICE_CANNOT_BE_NULL, kRocProfVisDmResultUnknownError);
                if (kRocProfVisDmResultSuccess != m_tracks[i].get()->DeleteSlice(slice)) return kRocProfVisDmResultUnknownError;
            }
        }
    }
    return kRocProfVisDmResultSuccess;
}

rocprofvis_dm_result_t RpvDmTrace::GetTrackAtIndex(rocprofvis_dm_index_t index, rocprofvis_dm_track_t & value) {
    ASSERT_MSG_RETURN(index < m_tracks.size(), ERROR_INDEX_OUT_OF_RANGE, kRocProfVisDmResultInvalidParameter);
    value = m_tracks[index].get();
    return kRocProfVisDmResultSuccess;
}  

rocprofvis_dm_size_t RpvDmTrace::GetMemoryFootprint(){
    rocprofvis_dm_size_t size = sizeof(RpvDmTrace)+m_strings_mem_footprint;
    for (int i=0; i < m_tracks.size(); i++)
    {
        size+=m_tracks[i].get()->GetMemoryFootprint();
    }   
    return size;
}

rocprofvis_dm_result_t RpvDmTrace::AddTrack(const rocprofvis_dm_trace_t object, rocprofvis_dm_track_params_t * params){
    ASSERT_MSG_RETURN(object, ERROR_TRACE_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
    RpvDmTrace* trace = (RpvDmTrace*)object;
    try{
        trace->m_tracks.push_back(std::make_unique<RpvDmTrack>(trace, params));
    }
    catch(std::exception ex)
    {
        ASSERT_MSG_RETURN(object, ERROR_TRACE_CANNOT_BE_NULL, kRocProfVisDmResultAllocFailure);
    }
    return kRocProfVisDmResultSuccess;
}

rocprofvis_dm_result_t  RpvDmTrace::GetExtendedTrackInfo(rocprofvis_dm_id_t track_id, rocprofvis_dm_json_blob_t & string) {
    //todo:call  database to get extended info per track
    string = "{}";
    ASSERT_ALWAYS_MSG_RETURN(ERROR_UNSUPPORTED_PROPERTY, kRocProfVisDmResultNotSupported);
}

rocprofvis_dm_result_t  RpvDmTrace::GetExtendedEventInfo(rocprofvis_dm_event_id_t event_id, rocprofvis_dm_json_blob_t & string) {
    //todo:call  database to get extended info per track
    string = "{}";
    ASSERT_ALWAYS_MSG_RETURN(ERROR_UNSUPPORTED_PROPERTY, kRocProfVisDmResultNotSupported);
}

rocprofvis_dm_result_t RpvDmTrace::GetFlowTraceHandle(rocprofvis_dm_id_t event_id, rocprofvis_dm_flowtrace_t & flowtrace){
    flowtrace = nullptr;
    ASSERT_ALWAYS_MSG_RETURN(ERROR_UNSUPPORTED_PROPERTY, kRocProfVisDmResultNotSupported);
}

rocprofvis_dm_result_t RpvDmTrace::GetStackTraceHandle(rocprofvis_dm_id_t event_id, rocprofvis_dm_stacktrace_t & stacktrace){
    stacktrace = nullptr;
    ASSERT_ALWAYS_MSG_RETURN(ERROR_UNSUPPORTED_PROPERTY, kRocProfVisDmResultNotSupported);
}

rocprofvis_dm_slice_t RpvDmTrace::AddSlice(const rocprofvis_dm_trace_t object, const rocprofvis_dm_track_id_t track_id, const rocprofvis_dm_timestamp_t start, const rocprofvis_dm_timestamp_t end){
    ASSERT_MSG_RETURN(object, ERROR_TRACE_CANNOT_BE_NULL, nullptr);
    RpvDmTrace* trace = (RpvDmTrace*)object;
    rocprofvis_dm_track_t track = nullptr;
    rocprofvis_dm_result_t result = trace->GetTrackAtIndex(track_id, track);
    if (result == kRocProfVisDmResultSuccess)
    {
        ASSERT_MSG_RETURN(track, ERROR_TRACK_CANNOT_BE_NULL, nullptr);
        return ((RpvDmTrack*)track)->AddSlice(start, end);
    }
    else
        return nullptr;
}  

rocprofvis_dm_result_t RpvDmTrace::AddRecord(const rocprofvis_dm_slice_t object, rocprofvis_db_record_data_t & data){
    ASSERT_MSG_RETURN(object, ERROR_TRACE_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
    RpvDmTrackSlice* slice = (RpvDmTrackSlice*) object;
    return slice->AddRecord(data);
}      
rocprofvis_dm_index_t RpvDmTrace::AddString(const rocprofvis_dm_trace_t object,  const char* stringValue){
    ASSERT_MSG_RETURN(object, ERROR_TRACE_CANNOT_BE_NULL, INVALID_INDEX);
    RpvDmTrace* trace = (RpvDmTrace*)object;
    rocprofvis_dm_index_t current_index = (rocprofvis_dm_index_t)trace->m_strings.size();
    try{
        trace->m_strings.push_back(stringValue);
    }
    catch(std::exception ex)
    {
        ASSERT_MSG_RETURN(object, ERROR_TRACE_CANNOT_BE_NULL, INVALID_INDEX);
    }
    return current_index;
}

rocprofvis_dm_charptr_t RpvDmTrace::GetStringAt(rocprofvis_dm_index_t index){
    ASSERT_MSG_RETURN(m_parameters.metadata_loaded, ERROR_METADATA_IS_NOT_LOADED, nullptr);
    ASSERT_MSG_RETURN((index + m_parameters.strings_offset) < m_strings.size(), ERROR_INDEX_OUT_OF_RANGE, nullptr);
    return m_strings[index + m_parameters.strings_offset].c_str();
}

rocprofvis_dm_charptr_t RpvDmTrace::GetSymbolAt(rocprofvis_dm_index_t index){
    ASSERT_MSG_RETURN(m_parameters.metadata_loaded, ERROR_METADATA_IS_NOT_LOADED, nullptr);
    ASSERT_MSG_RETURN((index + m_parameters.symbols_offset) < m_strings.size(), ERROR_INDEX_OUT_OF_RANGE, nullptr);
    return m_strings[index + m_parameters.symbols_offset].c_str();
}


rocprofvis_dm_result_t  RpvDmTrace::GetPropertyAsUint64(rocprofvis_dm_trace_property_t property, rocprofvis_dm_property_index_t index, uint64_t* value){
    
    switch(property)
    {
        case kRPVDMStartTimeUInt64:
            *value = StartTime();
            return kRocProfVisDmResultSuccess;
        case kRPVDMEndTimeUInt64:
            *value = EndTime();
            return kRocProfVisDmResultSuccess;
        case kRPVDMNumberOfTracksUInt64:
            *value = NumberOfTracks();
            return kRocProfVisDmResultSuccess;
        case kRPVDMTraceMemoryFootprintUInt64:
            *value = GetMemoryFootprint();
            return kRocProfVisDmResultSuccess;
        default:
            ASSERT_ALWAYS_MSG_RETURN(ERROR_INVALID_PROPERTY_GETTER, kRocProfVisDmResultInvalidProperty);
    }

}

rocprofvis_dm_result_t    RpvDmTrace::GetPropertyAsInt64(rocprofvis_dm_trace_property_t property, rocprofvis_dm_property_index_t index, int64_t* value){
    ASSERT_ALWAYS_MSG_RETURN(ERROR_INVALID_PROPERTY_GETTER, kRocProfVisDmResultInvalidProperty);
}

 rocprofvis_dm_result_t   RpvDmTrace::GetPropertyAsCharPtr(rocprofvis_dm_trace_property_t property, rocprofvis_dm_property_index_t index, char** value){
    switch(property)
    {
        case kRPVDMEventInfoJsonCharPtrIndexed:
            return GetExtendedEventInfo(*(rocprofvis_dm_event_id_t*)&index, *(rocprofvis_dm_json_blob_t*)value);
        break;
        default:
            ASSERT_ALWAYS_MSG_RETURN(ERROR_INVALID_PROPERTY_GETTER, kRocProfVisDmResultInvalidProperty);
    }
}

rocprofvis_dm_result_t   RpvDmTrace::GetPropertyAsDouble(rocprofvis_dm_trace_property_t property, rocprofvis_dm_property_index_t index, double* value){

    ASSERT_ALWAYS_MSG_RETURN(ERROR_INVALID_PROPERTY_GETTER, kRocProfVisDmResultInvalidProperty);

}

rocprofvis_dm_result_t    RpvDmTrace::GetPropertyAsHandle(rocprofvis_dm_trace_property_t property, rocprofvis_dm_property_index_t index, rocprofvis_dm_handle_t* value){
    switch(property)
    {
        case kRPVDMTrackObjectHandleIndexed:
            return GetTrackAtIndex(index, *value);
        case kRPVDMDatabaseObjectHandle:
            *value = Database();
            return kRocProfVisDmResultSuccess;
        case kRPVDMFlowTraceByEventIDHandle:
            return GetFlowTraceHandle(index, *value);
        case kRPVDMStackTraceByEventIDHandle:
            return GetStackTraceHandle(index, *value);
        default:
            ASSERT_ALWAYS_MSG_RETURN(ERROR_INVALID_PROPERTY_GETTER, kRocProfVisDmResultInvalidProperty);
    }
}