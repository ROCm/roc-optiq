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

#include "rocprofvis_dm_track_slice.h"

namespace RocProfVis
{
namespace DataModel
{

rocprofvis_dm_result_t TrackSlice::GetRecordTimestampAt(const rocprofvis_dm_property_index_t index, rocprofvis_dm_timestamp_t & timestamp){
    ASSERT_ALWAYS_MSG_RETURN(ERROR_VIRTUAL_METHOD_CALL, kRocProfVisDmResultNotLoaded);
}
rocprofvis_dm_result_t TrackSlice::GetRecordIdAt(const rocprofvis_dm_property_index_t index, rocprofvis_dm_id_t & id){
    ASSERT_ALWAYS_MSG_RETURN(ERROR_VIRTUAL_METHOD_CALL, kRocProfVisDmResultNotLoaded);
}
rocprofvis_dm_result_t TrackSlice::GetRecordOperationAt(const rocprofvis_dm_property_index_t index, rocprofvis_dm_op_t & op){
    ASSERT_ALWAYS_MSG_RETURN(ERROR_VIRTUAL_METHOD_CALL, kRocProfVisDmResultNotLoaded);
}
rocprofvis_dm_result_t TrackSlice::GetRecordOperationStringAt(const rocprofvis_dm_property_index_t index, rocprofvis_dm_charptr_t & op){
    ASSERT_ALWAYS_MSG_RETURN(ERROR_VIRTUAL_METHOD_CALL, kRocProfVisDmResultNotLoaded);
}
rocprofvis_dm_result_t TrackSlice::GetRecordValueAt(const rocprofvis_dm_property_index_t index, rocprofvis_dm_value_t & value){
    ASSERT_ALWAYS_MSG_RETURN(ERROR_VIRTUAL_METHOD_CALL, kRocProfVisDmResultNotLoaded);
}
rocprofvis_dm_result_t TrackSlice::GetRecordDurationAt(const rocprofvis_dm_property_index_t index, rocprofvis_dm_duration_t & duration){
    ASSERT_ALWAYS_MSG_RETURN(ERROR_VIRTUAL_METHOD_CALL, kRocProfVisDmResultNotLoaded);
}
rocprofvis_dm_result_t TrackSlice::GetRecordCategoryIndexAt(const rocprofvis_dm_property_index_t index, rocprofvis_dm_index_t & category_index){
    ASSERT_ALWAYS_MSG_RETURN(ERROR_VIRTUAL_METHOD_CALL, kRocProfVisDmResultNotLoaded);
}
rocprofvis_dm_result_t TrackSlice::GetRecordSymbolIndexAt(const rocprofvis_dm_property_index_t index, rocprofvis_dm_index_t & symbol_index){
    ASSERT_ALWAYS_MSG_RETURN(ERROR_VIRTUAL_METHOD_CALL, kRocProfVisDmResultNotLoaded);
}
rocprofvis_dm_result_t TrackSlice::GetRecordCategoryStringAt(const rocprofvis_dm_property_index_t index, rocprofvis_dm_charptr_t & category_charptr){
    ASSERT_ALWAYS_MSG_RETURN(ERROR_VIRTUAL_METHOD_CALL, kRocProfVisDmResultNotLoaded);
}
rocprofvis_dm_result_t TrackSlice::GetRecordSymbolStringAt(const rocprofvis_dm_property_index_t, rocprofvis_dm_charptr_t & symbol_charptr){
    ASSERT_ALWAYS_MSG_RETURN(ERROR_VIRTUAL_METHOD_CALL, kRocProfVisDmResultNotLoaded);
}


rocprofvis_dm_result_t    TrackSlice::GetPropertyAsUint64(rocprofvis_dm_property_t property, rocprofvis_dm_property_index_t index, uint64_t* value){
    ASSERT_MSG_RETURN(value, ERROR_REFERENCE_POINTER_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
    switch(property)
    {
        case kRPVDMRecordIndexByTimestampUInt64:
            *value = 0;
            return ConvertTimestampToIndex(index, *(rocprofvis_dm_index_t*)value);
        case kRPVDMNumberOfRecordsUInt64:
            *value = GetNumberOfRecords();
            return kRocProfVisDmResultSuccess;
        case kRPVDMTimestampUInt64Indexed:
            return GetRecordTimestampAt(index, *value);
        case kRPVDMEventIdUInt64Indexed:
            return GetRecordIdAt(index, *value);
        case kRPVDMEventIdOperationEnumIndexed:
            *value = 0;
            return GetRecordOperationAt(index, *(rocprofvis_dm_op_t*)value);
        case kRPVDMSliceMemoryFootprintUInt64:
            *value = GetMemoryFootprint();
            return kRocProfVisDmResultSuccess;
        default:
            ASSERT_ALWAYS_MSG_RETURN(ERROR_INVALID_PROPERTY_GETTER, kRocProfVisDmResultInvalidProperty);
    }

}

#ifdef TEST
const char*  TrackSlice::GetPropertySymbol(rocprofvis_dm_property_t property) {
    switch(property)
    {
        case kRPVDMRecordIndexByTimestampUInt64:
            return "kRPVDMRecordIndexByTimestampUInt64";        
        case kRPVDMNumberOfRecordsUInt64:
            return "kRPVDMNumberOfRecordsUInt64";
        case kRPVDMTimestampUInt64Indexed:
            return "kRPVDMTimestampUInt64Indexed";
        case kRPVDMEventIdUInt64Indexed:
            return "kRPVDMEventIdUInt64Indexed";
        case kRPVDMEventIdOperationEnumIndexed:
            return "kRPVDMEventIdOperationEnumIndexed";
        case kRPVDMSliceMemoryFootprintUInt64:
            return "kRPVDMSliceMemoryFootprintUInt64";
        case kRPVDMEventDurationInt64Indexed:
            return "kRPVDMEventDurationInt64Indexed";
        case kRPVDMEventTypeStringCharPtrIndexed:
            return "kRPVDMEventTypeStringCharPtrIndexed";
        case kRPVDMEventSymbolStringCharPtrIndexed:
            return "kRPVDMEventSymbolStringCharPtrIndexed";
        case kRPVDMEventIdOperationCharPtrIndexed:
            return "kRPVDMEventIdOperationCharPtrIndexed";
        case kRPVDMPmcValueDoubleIndexed:
            return "kRPVDMPmcValueDoubleIndexed";
        default:
            return "Unknown property";
    }   
}
#endif
rocprofvis_dm_result_t    TrackSlice::GetPropertyAsInt64(rocprofvis_dm_property_t property, rocprofvis_dm_property_index_t index, int64_t* value){
    ASSERT_MSG_RETURN(value, ERROR_REFERENCE_POINTER_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
    switch(property)
    {
        case kRPVDMEventDurationInt64Indexed:
            return GetRecordDurationAt(index, *value);
        default:
            ASSERT_ALWAYS_MSG_RETURN(ERROR_INVALID_PROPERTY_GETTER, kRocProfVisDmResultInvalidProperty);
    }
}
 rocprofvis_dm_result_t   TrackSlice::GetPropertyAsCharPtr(rocprofvis_dm_property_t property, rocprofvis_dm_property_index_t index, char** value){
    ASSERT_MSG_RETURN(value, ERROR_REFERENCE_POINTER_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
    switch(property)
    {
        case kRPVDMEventTypeStringCharPtrIndexed:
            return GetRecordCategoryStringAt(index, *(rocprofvis_dm_charptr_t*)value);
        case kRPVDMEventSymbolStringCharPtrIndexed:
            return GetRecordSymbolStringAt(index, *(rocprofvis_dm_charptr_t*)value);
        case kRPVDMEventIdOperationCharPtrIndexed:
            return GetRecordOperationStringAt(index, *(rocprofvis_dm_charptr_t*)value);
        default:
            ASSERT_ALWAYS_MSG_RETURN(ERROR_INVALID_PROPERTY_GETTER, kRocProfVisDmResultInvalidProperty);
    }

}
rocprofvis_dm_result_t   TrackSlice::GetPropertyAsDouble(rocprofvis_dm_property_t property, rocprofvis_dm_property_index_t index, double* value){
    ASSERT_MSG_RETURN(value, ERROR_REFERENCE_POINTER_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
    switch(property)
    {
        case kRPVDMPmcValueDoubleIndexed:
            return GetRecordValueAt(index, *value);
        default:
            ASSERT_ALWAYS_MSG_RETURN(ERROR_INVALID_PROPERTY_GETTER, kRocProfVisDmResultInvalidProperty);
    }

}

}  // namespace DataModel
}  // namespace RocProfVis
