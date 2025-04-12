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

#include "rocprofvis_dm_flow_trace.h"

namespace RocProfVis
{
namespace DataModel
{

rocprofvis_dm_size_t  FlowTrace::GetMemoryFootprint(){
    return sizeof(FlowTrace) + (sizeof(FlowRecord) * m_flows.size());
}

rocprofvis_dm_result_t  FlowTrace::AddRecord( rocprofvis_db_flow_data_t & data){
    try{
        m_flows.push_back(FlowRecord(data.time, data.id, data.track_id));
    }
    catch(std::exception ex)
    {
        return kRocProfVisDmResultAllocFailure;
    }
    return kRocProfVisDmResultSuccess;
}

rocprofvis_dm_result_t FlowTrace::GetRecordTimestampAt(const rocprofvis_dm_property_index_t index, rocprofvis_dm_timestamp_t & timestamp){
    ROCPROFVIS_ASSERT_MSG_RETURN(index < m_flows.size(), ERROR_INDEX_OUT_OF_RANGE, kRocProfVisDmResultNotLoaded);
    timestamp = m_flows[index].Timestamp();
    return kRocProfVisDmResultSuccess;
}
rocprofvis_dm_result_t FlowTrace::GetRecordIdAt(const rocprofvis_dm_property_index_t index, rocprofvis_dm_event_id_t & event_id){
    ROCPROFVIS_ASSERT_MSG_RETURN(index < m_flows.size(), ERROR_INDEX_OUT_OF_RANGE, kRocProfVisDmResultNotLoaded);
    event_id = m_flows[index].EventId();
    return kRocProfVisDmResultSuccess;
}
rocprofvis_dm_result_t FlowTrace::GetRecordTrackIdAt(const rocprofvis_dm_property_index_t index, rocprofvis_dm_track_id_t & track_id){
    ROCPROFVIS_ASSERT_MSG_RETURN(index < m_flows.size(), ERROR_INDEX_OUT_OF_RANGE, kRocProfVisDmResultNotLoaded);
    track_id = m_flows[index].TrackId();
    return kRocProfVisDmResultSuccess;
}


rocprofvis_dm_result_t FlowTrace::GetPropertyAsUint64(rocprofvis_dm_property_t property, rocprofvis_dm_property_index_t index, uint64_t* value){
    ROCPROFVIS_ASSERT_MSG_RETURN(value, ERROR_REFERENCE_POINTER_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
    switch(property)
    {
        case kRPVDMNumberOfEndpointsUInt64:
            *value = GetNumberOfRecords();
            return kRocProfVisDmResultSuccess;
        case kRPVDMEndpointTimestampUInt64Indexed:
            return GetRecordTimestampAt(index, *value);
        case kRPVDMEndpointIDUInt64Indexed:
            return GetRecordIdAt(index, *(rocprofvis_dm_event_id_t*)value);
        case kRPVDMEndpointTrackIDUInt64Indexed:
            return GetRecordTrackIdAt(index, *(rocprofvis_dm_track_id_t*)value);
        default:
            ROCPROFVIS_ASSERT_ALWAYS_MSG_RETURN(ERROR_INVALID_PROPERTY_GETTER, kRocProfVisDmResultInvalidProperty);
    }

}


#ifdef TEST
const char*  FlowTrace::GetPropertySymbol(rocprofvis_dm_property_t property) {
    switch(property)
    {
        case kRPVDMNumberOfEndpointsUInt64:
            return "kRPVDMNumberOfEndpointsUInt64";        
        case kRPVDMEndpointTimestampUInt64Indexed:
            return "kRPVDMEndpointTimestampUInt64Indexed";
        case kRPVDMEndpointIDUInt64Indexed:
            return "kRPVDMEndpointIDUInt64Indexed";
        case kRPVDMEndpointTrackIDUInt64Indexed:
            return "kRPVDMEndpointTrackIDUInt64Indexed";
        default:
            return "Unknown property";
    }   
}
#endif

}  // namespace DataModel
}  // namespace RocProfVis