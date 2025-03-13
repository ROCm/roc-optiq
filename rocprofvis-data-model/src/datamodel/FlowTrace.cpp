#include "FlowTrace.h"

rocprofvis_dm_size_t  RpvDmFlowTrace::GetMemoryFootprint(){
    return sizeof(RpvDmFlowTrace) + (sizeof(RpvDmFlowRecord) * m_flows.size());
}

rocprofvis_dm_result_t  RpvDmFlowTrace::AddRecord( rocprofvis_db_flow_data_t & data){
    try{
        m_flows.push_back(RpvDmFlowRecord(data.time, data.id, data.track_id));
    }
    catch(std::exception ex)
    {
        return kRocProfVisDmResultAllocFailure;
    }
    return kRocProfVisDmResultSuccess;
}

rocprofvis_dm_result_t RpvDmFlowTrace::GetRecordTimestampAt(const rocprofvis_dm_property_index_t index, rocprofvis_dm_timestamp_t & timestamp){
    ASSERT_MSG_RETURN(index < m_flows.size(), ERROR_INDEX_OUT_OF_RANGE, kRocProfVisDmResultNotLoaded);
    timestamp = m_flows[index].Timestamp();
    return kRocProfVisDmResultSuccess;
}
rocprofvis_dm_result_t RpvDmFlowTrace::GetRecordIdAt(const rocprofvis_dm_property_index_t index, rocprofvis_dm_event_id_t & event_id){
    ASSERT_MSG_RETURN(index < m_flows.size(), ERROR_INDEX_OUT_OF_RANGE, kRocProfVisDmResultNotLoaded);
    event_id = m_flows[index].EventId();
    return kRocProfVisDmResultSuccess;
}
rocprofvis_dm_result_t RpvDmFlowTrace::GetRecordTrackIdAt(const rocprofvis_dm_property_index_t index, rocprofvis_dm_track_id_t & track_id){
    ASSERT_MSG_RETURN(index < m_flows.size(), ERROR_INDEX_OUT_OF_RANGE, kRocProfVisDmResultNotLoaded);
    track_id = m_flows[index].TrackId();
    return kRocProfVisDmResultSuccess;
}


rocprofvis_dm_result_t RpvDmFlowTrace::GetPropertyAsUint64(rocprofvis_dm_property_t property, rocprofvis_dm_property_index_t index, uint64_t* value){
    ASSERT_MSG_RETURN(value, ERROR_REFERENCE_POINTER_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
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
            ASSERT_ALWAYS_MSG_RETURN(ERROR_INVALID_PROPERTY_GETTER, kRocProfVisDmResultInvalidProperty);
    }

}
