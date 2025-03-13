#include "TableRow.h"

rocprofvis_dm_size_t  RpvDmTableRow::GetMemoryFootprint(){
    size_t size = sizeof(RpvDmTableRow);
    for (int i=0; i < m_values.size(); i++) size+=m_values[i].length();
    return size;
}

rocprofvis_dm_result_t  RpvDmTableRow::AddCellValue(rocprofvis_dm_charptr_t value){
    try{
        m_values.push_back(value);
    }
    catch(std::exception ex)
    {
        return kRocProfVisDmResultAllocFailure;
    }
    return kRocProfVisDmResultSuccess;
}


rocprofvis_dm_result_t RpvDmTableRow::GetValueAt(const rocprofvis_dm_property_index_t index, rocprofvis_dm_charptr_t & value){
    ASSERT_MSG_RETURN(index < m_values.size(), ERROR_INDEX_OUT_OF_RANGE, kRocProfVisDmResultNotLoaded);
    value = m_values[index].c_str();
    return kRocProfVisDmResultSuccess;
}

rocprofvis_dm_result_t RpvDmTableRow::GetPropertyAsUint64(rocprofvis_dm_property_t property, rocprofvis_dm_property_index_t index, uint64_t* value){
    ASSERT_MSG_RETURN(value, ERROR_REFERENCE_POINTER_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
    switch(property)
    {
        case kRPVDMNumberOfTableRowCellsUInt64:
            *value = GetNumberOfCells();
            return kRocProfVisDmResultSuccess;
        default:
            ASSERT_ALWAYS_MSG_RETURN(ERROR_INVALID_PROPERTY_GETTER, kRocProfVisDmResultInvalidProperty);
    }
}

 rocprofvis_dm_result_t RpvDmTableRow::GetPropertyAsCharPtr(rocprofvis_dm_property_t property, rocprofvis_dm_property_index_t index, char** value){
    ASSERT_MSG_RETURN(value, ERROR_REFERENCE_POINTER_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
    switch(property)
    {
        case kRPVDMExtTableRowCellValueCharPtrIndexed:
            return GetValueAt(index, *(rocprofvis_dm_charptr_t*)value);
        default:
            ASSERT_ALWAYS_MSG_RETURN(ERROR_INVALID_PROPERTY_GETTER, kRocProfVisDmResultInvalidProperty);
    }
}
