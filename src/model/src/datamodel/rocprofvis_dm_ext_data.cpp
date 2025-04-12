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

#include "rocprofvis_dm_ext_data.h"
#include "rocprofvis_dm_trace.h"
#include <cstdint>
#include <sstream>

namespace RocProfVis
{
namespace DataModel
{

rocprofvis_dm_size_t  ExtData::GetMemoryFootprint(){
    return sizeof(ExtData) + (sizeof(ExtDataRecord) * m_extdata_records.size());
}

rocprofvis_dm_result_t  ExtData::AddRecord( rocprofvis_db_ext_data_t & data){
    try{
        m_extdata_records.push_back(ExtDataRecord(data.category, data.name, data.data));
    }
    catch(std::exception ex)
    {
        return kRocProfVisDmResultAllocFailure;
    }
    return kRocProfVisDmResultSuccess;
}

rocprofvis_dm_result_t ExtData::GetRecordNameAt(const rocprofvis_dm_property_index_t index, rocprofvis_dm_charptr_t & name){
    ROCPROFVIS_ASSERT_MSG_RETURN(index < m_extdata_records.size(), ERROR_INDEX_OUT_OF_RANGE, kRocProfVisDmResultNotLoaded);
    name = m_extdata_records[index].Name();
    return kRocProfVisDmResultSuccess;
}

rocprofvis_dm_result_t ExtData::GetRecordDataAt(const rocprofvis_dm_property_index_t index, rocprofvis_dm_charptr_t &data){
    ROCPROFVIS_ASSERT_MSG_RETURN(index < m_extdata_records.size(), ERROR_INDEX_OUT_OF_RANGE, kRocProfVisDmResultNotLoaded);
    if (m_id.value != 0) {
        data = m_extdata_records[index].Data();
        return kRocProfVisDmResultSuccess;
    }
    else
    {
        rocprofvis_dm_id_t id = 0;
        const char* str = m_extdata_records[index].Data();
        for (int i=0; i < 4 && *str; i++,str++) ((char*)&id)[i]=*str;
        return m_ctx->BindingInfo()->FuncFindCachedTableValue(m_ctx->Database(), m_extdata_records[index].Category(), id, m_extdata_records[index].Name(), &data);
    }
}

rocprofvis_dm_result_t ExtData::GetRecordCategoryAt(const rocprofvis_dm_property_index_t index, rocprofvis_dm_charptr_t& category) {
    ROCPROFVIS_ASSERT_MSG_RETURN(index < m_extdata_records.size(), ERROR_INDEX_OUT_OF_RANGE, kRocProfVisDmResultNotLoaded);
    category = m_extdata_records[index].Category();
    return kRocProfVisDmResultSuccess;
}

rocprofvis_dm_result_t ExtData::GetPropertyAsUint64(rocprofvis_dm_property_t property, rocprofvis_dm_property_index_t index, uint64_t* value){
    ROCPROFVIS_ASSERT_MSG_RETURN(value, ERROR_REFERENCE_POINTER_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
    switch(property)
    {
        case kRPVDMNumberOfExtDataRecordsUInt64:
            *value = GetNumberOfRecords();
            return kRocProfVisDmResultSuccess;
        default:
            ROCPROFVIS_ASSERT_ALWAYS_MSG_RETURN(ERROR_INVALID_PROPERTY_GETTER, kRocProfVisDmResultInvalidProperty);
    }

}

rocprofvis_dm_result_t ExtData::MakeJsonBlob(rocprofvis_dm_charptr_t & blob)
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
            rocprofvis_dm_charptr_t cdata;
            std::string data="";
            if (kRocProfVisDmResultSuccess == GetRecordDataAt(i,cdata)) data = cdata;
            bool numeric = !data.empty();
            if (numeric)
            {
                try {
                    std::stoll(data.c_str());
                }
                catch (std::exception& e) {
                    numeric = false;
                }
            }
            bool quatation_needed =  !(numeric || data.empty() || data.at(0) =='{');
            if (quatation_needed) json_blob << "\"";
            json_blob << data;
            if (quatation_needed) json_blob << "\",\n"; else json_blob << "\n";
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

 rocprofvis_dm_result_t ExtData::GetPropertyAsCharPtr(rocprofvis_dm_property_t property, rocprofvis_dm_property_index_t index, char** value){
    ROCPROFVIS_ASSERT_MSG_RETURN(value, ERROR_REFERENCE_POINTER_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
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
            ROCPROFVIS_ASSERT_ALWAYS_MSG_RETURN(ERROR_INVALID_PROPERTY_GETTER, kRocProfVisDmResultInvalidProperty);
    }
}

#ifdef TEST
const char*  ExtData::GetPropertySymbol(rocprofvis_dm_property_t property) {
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

bool ExtData::HasRecord(rocprofvis_db_ext_data_t & data)
{
    auto it = find_if(m_extdata_records.begin(), m_extdata_records.end(), [&](ExtDataRecord & ext){
        return ext.Equal(data.category, data.name);
    });
    return it != m_extdata_records.end();
}

}  // namespace DataModel
}  // namespace RocProfVis