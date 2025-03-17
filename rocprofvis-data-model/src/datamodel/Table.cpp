#include "Table.h"

rocprofvis_dm_size_t RpvDmTable::GetMemoryFootprint(){
    size_t size = sizeof(RpvDmTable);
    for (int i=0; i < m_rows.size(); i++) size+=m_rows[i].GetMemoryFootprint();
    for (int i=0; i < m_columns.size(); i++) size+=m_columns[i].length();
    return size;
}

rocprofvis_dm_result_t RpvDmTable::AddColumn(rocprofvis_dm_charptr_t column_name){
    try{
        m_columns.push_back(column_name);
    }
    catch(std::exception ex)
    {
        return kRocProfVisDmResultAllocFailure;
    }
    return kRocProfVisDmResultSuccess;
}

rocprofvis_dm_table_row_t RpvDmTable::AddRow(){
    try{
        m_rows.push_back(RpvDmTableRow(this));
    }
    catch(std::exception ex)
    {
        ASSERT_ALWAYS_MSG_RETURN("Error! Failure allocating table row object", nullptr);
    }
    return &m_rows.back();
}

rocprofvis_dm_result_t RpvDmTable::GetColumnNameAt(const rocprofvis_dm_property_index_t index, rocprofvis_dm_charptr_t & name){
    ASSERT_MSG_RETURN(index < m_columns.size(), ERROR_INDEX_OUT_OF_RANGE, kRocProfVisDmResultNotLoaded);
    name = m_columns[index].c_str();
    return kRocProfVisDmResultSuccess;
}


rocprofvis_dm_result_t   RpvDmTable::GetRowHandleAt(const rocprofvis_dm_property_index_t index, rocprofvis_dm_handle_t & row){
    ASSERT_MSG_RETURN(index < m_rows.size(), ERROR_INDEX_OUT_OF_RANGE, kRocProfVisDmResultNotLoaded);
    row = &m_rows[index];
    return kRocProfVisDmResultSuccess;    
}


rocprofvis_dm_result_t RpvDmTable::GetPropertyAsUint64(rocprofvis_dm_property_t property, rocprofvis_dm_property_index_t index, uint64_t* value){
    ASSERT_MSG_RETURN(value, ERROR_REFERENCE_POINTER_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
    switch(property)
    {
        case kRPVDMNumberOfTableColumsUInt64:
            *value = GetNumberOfColumns();
            return kRocProfVisDmResultSuccess;
        case kRPVDMNumberOfTableRowsUInt64:
            *value = GetNumberOfRows();
            return kRocProfVisDmResultSuccess;
        default:
            ASSERT_ALWAYS_MSG_RETURN(ERROR_INVALID_PROPERTY_GETTER, kRocProfVisDmResultInvalidProperty);
    }
}

 rocprofvis_dm_result_t RpvDmTable::GetPropertyAsCharPtr(rocprofvis_dm_property_t property, rocprofvis_dm_property_index_t index, char** value){
    ASSERT_MSG_RETURN(value, ERROR_REFERENCE_POINTER_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
    switch(property)
    {
        case kRPVDMExtTableColumnNameCharPtrIndexed:
            return GetColumnNameAt(index, *(rocprofvis_dm_charptr_t*)value);
        case kRPVDMExtTableDescriptionCharPtr:
            *value = (char*)Description();
            return kRocProfVisDmResultSuccess;
        case kRPVDMExtTableQueryCharPtr:
            *value = (char*)Query();
            return kRocProfVisDmResultSuccess;
        default:
            ASSERT_ALWAYS_MSG_RETURN(ERROR_INVALID_PROPERTY_GETTER, kRocProfVisDmResultInvalidProperty);
    }
}

rocprofvis_dm_result_t  RpvDmTable::GetPropertyAsHandle(rocprofvis_dm_property_t property, rocprofvis_dm_property_index_t index, rocprofvis_dm_handle_t* value){
    ASSERT_MSG_RETURN(value, ERROR_REFERENCE_POINTER_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
    switch(property)
    {
        case kRPVDMExtTableRowHandleIndexed:
            return GetRowHandleAt(index, *value);
        default:
            ASSERT_ALWAYS_MSG_RETURN(ERROR_INVALID_PROPERTY_GETTER, kRocProfVisDmResultInvalidProperty);
    }
}

#ifdef TEST
const char*  RpvDmTable::GetPropertySymbol(rocprofvis_dm_property_t property) {
    switch(property)
    {
        case kRPVDMNumberOfTableColumsUInt64:
            return "kRPVDMNumberOfTableColumsUInt64";        
        case kRPVDMNumberOfTableRowsUInt64:
            return "kRPVDMNumberOfTableRowsUInt64";
        case kRPVDMExtTableColumnNameCharPtrIndexed:
            return "kRPVDMExtTableColumnNameCharPtrIndexed";
        case kRPVDMExtTableDescriptionCharPtr:
            return "kRPVDMExtTableDescriptionCharPtr";
        case kRPVDMExtTableQueryCharPtr:
            return "kRPVDMExtTableQueryCharPtr";
        case kRPVDMExtTableRowHandleIndexed:
            return "kRPVDMExtTableRowHandleIndexed";
        default:
            return "Unknown property";
    }   
}
#endif
