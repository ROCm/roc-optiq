#include "ExtData.h"
#include <sstream>

rocprofvis_dm_size_t  RpvDmExtData::GetMemoryFootprint(){
    return sizeof(RpvDmExtData) + (sizeof(RpvDmExtDataRecord) * m_extdata_records.size());
}

rocprofvis_dm_result_t  RpvDmExtData::AddRecord( rocprofvis_db_ext_data_t & data){
    try{
        m_extdata_records.push_back(RpvDmExtDataRecord(data.category, data.name, data.data));
    }
    catch(std::exception ex)
    {
        return kRocProfVisDmResultAllocFailure;
    }
    return kRocProfVisDmResultSuccess;
}

rocprofvis_dm_result_t RpvDmExtData::GetRecordNameAt(const rocprofvis_dm_property_index_t index, rocprofvis_dm_charptr_t & name){
    ASSERT_MSG_RETURN(index < m_extdata_records.size(), ERROR_INDEX_OUT_OF_RANGE, kRocProfVisDmResultNotLoaded);
    name = m_extdata_records[index].Name();
    return kRocProfVisDmResultSuccess;
}

rocprofvis_dm_result_t RpvDmExtData::GetRecordDataAt(const rocprofvis_dm_property_index_t index, rocprofvis_dm_charptr_t &data){
    ASSERT_MSG_RETURN(index < m_extdata_records.size(), ERROR_INDEX_OUT_OF_RANGE, kRocProfVisDmResultNotLoaded);
    data = m_extdata_records[index].Data();
    return kRocProfVisDmResultSuccess;
}

rocprofvis_dm_result_t RpvDmExtData::GetRecordCategoryAt(const rocprofvis_dm_property_index_t index, rocprofvis_dm_charptr_t& category) {
    ASSERT_MSG_RETURN(index < m_extdata_records.size(), ERROR_INDEX_OUT_OF_RANGE, kRocProfVisDmResultNotLoaded);
    category = m_extdata_records[index].Category();
    return kRocProfVisDmResultSuccess;
}

rocprofvis_dm_result_t RpvDmExtData::GetPropertyAsUint64(rocprofvis_dm_property_t property, rocprofvis_dm_property_index_t index, uint64_t* value){
    ASSERT_MSG_RETURN(value, ERROR_REFERENCE_POINTER_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
    switch(property)
    {
        case kRPVDMNumberOfExtDataRecordsUInt64:
            *value = GetNumberOfRecords();
            return kRocProfVisDmResultSuccess;
        default:
            ASSERT_ALWAYS_MSG_RETURN(ERROR_INVALID_PROPERTY_GETTER, kRocProfVisDmResultInvalidProperty);
    }

}

rocprofvis_dm_result_t RpvDmExtData::MakeJsonBlob(rocprofvis_dm_charptr_t & blob)
{
    if (m_json_blob.length() == 0)
    {
        std::stringstream json_blob;
        json_blob << "{\n";
        std::string category = "";
        bool category_bracket_open = false;
        for (int i = 0; i < m_extdata_records.size(); i++)
        {
            if (category.compare(m_extdata_records[i].Category())!=0)
            {
                if (category_bracket_open) {
                    json_blob << "\t}\n";
                }
                category = m_extdata_records[i].Category();
                if (category.length() > 0)
                {
                    json_blob << "\"" << category << "\":\n\t{\n";
                    category_bracket_open = true;
                }
            }
            if (category.length() > 0) json_blob << "\t";

            json_blob << "\t\"" << m_extdata_records[i].Name() << "\":";
            std::string data = m_extdata_records[i].Data();
            bool quatation_needed = (data.length() == 0) || (data.length() > 0 && data[0] != '{');
            if (quatation_needed) json_blob << "\"";
            json_blob << data;
            if (quatation_needed) json_blob << "\",\n";
        }
        if (category_bracket_open) {
                json_blob << "\t}\n";
        }
        json_blob << "}";
        m_json_blob = json_blob.str().c_str();
    }
    blob = m_json_blob.c_str();
    return kRocProfVisDmResultSuccess;
}

 rocprofvis_dm_result_t RpvDmExtData::GetPropertyAsCharPtr(rocprofvis_dm_property_t property, rocprofvis_dm_property_index_t index, char** value){
    ASSERT_MSG_RETURN(value, ERROR_REFERENCE_POINTER_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
    switch(property)
    {
        case kRPVDMExtDataCategoryCharPtrIndexed:
            return GetRecordCategoryAt(index, *(rocprofvis_dm_charptr_t*)value);
        case kRPVDMExtDataNameCharPtrIndexed:
            return GetRecordNameAt(index, *(rocprofvis_dm_charptr_t*)value);
        case kRPVDMExtDataValueCharPtrIndexed:
            return GetRecordDataAt(index, *(rocprofvis_dm_charptr_t*)value);
        case kRPVDMExtDataJsonBlobCharPtr:
            return MakeJsonBlob(*(rocprofvis_dm_charptr_t*)value);
        default:
            ASSERT_ALWAYS_MSG_RETURN(ERROR_INVALID_PROPERTY_GETTER, kRocProfVisDmResultInvalidProperty);
    }
}

#ifdef TEST
const char*  RpvDmExtData::GetPropertySymbol(rocprofvis_dm_property_t property) {
    switch(property)
    {
        case kRPVDMNumberOfExtDataRecordsUInt64:
            return "kRPVDMNumberOfExtDataRecordsUInt64";        
        case kRPVDMExtDataCategoryCharPtrIndexed:
            return "kRPVDMExtDataCategoryCharPtrIndexed";
        case kRPVDMExtDataNameCharPtrIndexed:
            return "kRPVDMExtDataNameCharPtrIndexed";
        case kRPVDMExtDataValueCharPtrIndexed:
            return "kRPVDMExtDataCharPtrIndexed";
        case kRPVDMExtDataJsonBlobCharPtr:
            return "kRPVDMExtDataJsonBlobCharPtr";
        default:
            return "Unknown property";
    }   
}
#endif

bool RpvDmExtData::HasRecord(rocprofvis_db_ext_data_t & data)
{
    auto it = find_if(m_extdata_records.begin(), m_extdata_records.end(), [&](RpvDmExtDataRecord & ext){
        return ext.Compare(data.category, data.name);
    });
    return it != m_extdata_records.end();
}
