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

#include "rocprofvis_dm_table_row.h"
#include "rocprofvis_dm_table.h"

namespace RocProfVis
{
namespace DataModel
{

std::shared_mutex* TableRow::Mutex()
{
    return m_ctx->Mutex();
}

rocprofvis_dm_size_t  TableRow::GetMemoryFootprint(){
    size_t size = sizeof(TableRow);
    for (int i=0; i < m_values.size(); i++) size+=m_values[i].length();
    return size;
}

rocprofvis_dm_result_t  TableRow::AddCellValue(rocprofvis_dm_charptr_t value){
    try{
        m_values.push_back(value);
    }
    catch(std::exception ex)
    {
        return kRocProfVisDmResultAllocFailure;
    }
    return kRocProfVisDmResultSuccess;
}


rocprofvis_dm_result_t TableRow::GetValueAt(const rocprofvis_dm_property_index_t index, rocprofvis_dm_charptr_t & value){
    ROCPROFVIS_ASSERT_MSG_RETURN(index < m_values.size(), ERROR_INDEX_OUT_OF_RANGE, kRocProfVisDmResultNotLoaded);
    value = m_values[index].c_str();
    return kRocProfVisDmResultSuccess;
}

rocprofvis_dm_result_t TableRow::GetPropertyAsUint64(rocprofvis_dm_property_t property, rocprofvis_dm_property_index_t index, uint64_t* value){
    ROCPROFVIS_ASSERT_MSG_RETURN(value, ERROR_REFERENCE_POINTER_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
    switch(property)
    {
        case kRPVDMNumberOfTableRowCellsUInt64:
            *value = GetNumberOfCells();
            return kRocProfVisDmResultSuccess;
        default:
            ROCPROFVIS_ASSERT_ALWAYS_MSG_RETURN(ERROR_INVALID_PROPERTY_GETTER, kRocProfVisDmResultInvalidProperty);
    }
}

 rocprofvis_dm_result_t TableRow::GetPropertyAsCharPtr(rocprofvis_dm_property_t property, rocprofvis_dm_property_index_t index, char** value){
    ROCPROFVIS_ASSERT_MSG_RETURN(value, ERROR_REFERENCE_POINTER_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
    switch(property)
    {
        case kRPVDMExtTableRowCellValueCharPtrIndexed:
            return GetValueAt(index, *(rocprofvis_dm_charptr_t*)value);
        default:
            ROCPROFVIS_ASSERT_ALWAYS_MSG_RETURN(ERROR_INVALID_PROPERTY_GETTER, kRocProfVisDmResultInvalidProperty);
    }
}


#ifdef TEST
const char* TableRow::GetPropertySymbol(rocprofvis_dm_property_t property) {
    switch(property)
    {
        case kRPVDMNumberOfTableRowCellsUInt64:
            return "kRPVDMNumberOfTableRowCellsUInt64";        
        case kRPVDMExtTableRowCellValueCharPtrIndexed:
            return "kRPVDMExtTableRowCellValueCharPtrIndexed";
        default:
            return "Unknown property";
    }   
}
#endif

}  // namespace DataModel
}  // namespace RocProfVis