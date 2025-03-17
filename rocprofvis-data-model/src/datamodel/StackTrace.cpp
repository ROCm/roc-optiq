#include "StackTrace.h"

rocprofvis_dm_size_t  RpvDmStackTrace::GetMemoryFootprint(){
    return sizeof(RpvDmStackTrace) + (sizeof(RpvDmStackFrameRecord) * m_stack_frames.size());
}

rocprofvis_dm_result_t  RpvDmStackTrace::AddRecord( rocprofvis_db_stack_data_t & data){
    try{
        m_stack_frames.push_back(RpvDmStackFrameRecord(data.symbol, data.args, data.line, data.depth));
    }
    catch(std::exception ex)
    {
        return kRocProfVisDmResultAllocFailure;
    }
    return kRocProfVisDmResultSuccess;
}

rocprofvis_dm_result_t RpvDmStackTrace::GetRecordSymbolAt(const rocprofvis_dm_property_index_t index, rocprofvis_dm_charptr_t & symbol){
    ASSERT_MSG_RETURN(index < m_stack_frames.size(), ERROR_INDEX_OUT_OF_RANGE, kRocProfVisDmResultNotLoaded);
    symbol = m_stack_frames[index].Symbol();
    return kRocProfVisDmResultSuccess;
}

rocprofvis_dm_result_t RpvDmStackTrace::GetRecordArgsAt(const rocprofvis_dm_property_index_t index, rocprofvis_dm_charptr_t &args){
    ASSERT_MSG_RETURN(index < m_stack_frames.size(), ERROR_INDEX_OUT_OF_RANGE, kRocProfVisDmResultNotLoaded);
    args = m_stack_frames[index].Args();
    return kRocProfVisDmResultSuccess;
}

rocprofvis_dm_result_t RpvDmStackTrace::GetRecordCodeLineAt(const rocprofvis_dm_property_index_t index, rocprofvis_dm_charptr_t & code_line){
    ASSERT_MSG_RETURN(index < m_stack_frames.size(), ERROR_INDEX_OUT_OF_RANGE, kRocProfVisDmResultNotLoaded);
    code_line = m_stack_frames[index].CodeLine();
    return kRocProfVisDmResultSuccess;
}

rocprofvis_dm_result_t RpvDmStackTrace::GetRecordDepthAt(const rocprofvis_dm_property_index_t index, uint32_t & depth){
    ASSERT_MSG_RETURN(index < m_stack_frames.size(), ERROR_INDEX_OUT_OF_RANGE, kRocProfVisDmResultNotLoaded);
    depth = m_stack_frames[index].Depth();
    return kRocProfVisDmResultSuccess;
}

rocprofvis_dm_result_t RpvDmStackTrace::GetPropertyAsUint64(rocprofvis_dm_property_t property, rocprofvis_dm_property_index_t index, uint64_t* value){
    ASSERT_MSG_RETURN(value, ERROR_REFERENCE_POINTER_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
    switch(property)
    {
        case kRPVDMNumberOfFramesUInt64:
            *value = GetNumberOfRecords();
            return kRocProfVisDmResultSuccess;
        case kRPVDMFrameDepthUInt64Indexed:
            *value = 0;
            return GetRecordDepthAt(index, *(uint32_t*)value);
        default:
            ASSERT_ALWAYS_MSG_RETURN(ERROR_INVALID_PROPERTY_GETTER, kRocProfVisDmResultInvalidProperty);
    }

}

 rocprofvis_dm_result_t   RpvDmStackTrace::GetPropertyAsCharPtr(rocprofvis_dm_property_t property, rocprofvis_dm_property_index_t index, char** value){
    ASSERT_MSG_RETURN(value, ERROR_REFERENCE_POINTER_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
    switch(property)
    {
        case kRPVDMFrameSymbolCharPtrIndexed:
            return GetRecordSymbolAt(index, *(rocprofvis_dm_charptr_t*)value);
        case kRPVDMFrameArgsCharPtrIndexed:
            return GetRecordArgsAt(index, *(rocprofvis_dm_charptr_t*)value);
        case kRPVDMFrameCodeLineCharPtrIndexed:
            return GetRecordCodeLineAt(index, *(rocprofvis_dm_charptr_t*)value);
        default:
            ASSERT_ALWAYS_MSG_RETURN(ERROR_INVALID_PROPERTY_GETTER, kRocProfVisDmResultInvalidProperty);
    }
}

#ifdef TEST
const char*  RpvDmStackTrace::GetPropertySymbol(rocprofvis_dm_property_t property) {
    switch(property)
    {
        case kRPVDMNumberOfFramesUInt64:
            return "kRPVDMNumberOfFramesUInt64";        
        case kRPVDMFrameDepthUInt64Indexed:
            return "kRPVDMFrameDepthUInt64Indexed";
        case kRPVDMFrameSymbolCharPtrIndexed:
            return "kRPVDMFrameSymbolCharPtrIndexed";
        case kRPVDMFrameArgsCharPtrIndexed:
            return "kRPVDMFrameArgsCharPtrIndexed";
        case kRPVDMFrameCodeLineCharPtrIndexed:
            return "kRPVDMFrameCodeLineCharPtrIndexed";
        default:
            return "Unknown property";
    }   
}
#endif
