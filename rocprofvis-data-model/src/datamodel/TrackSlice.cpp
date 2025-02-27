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

#include "TrackSlice.h"

rocprofvis_dm_result_t RpvDmTrackSlice::GetRecordTimestampAt(const rocprofvis_dm_index_t index, rocprofvis_dm_timestamp_t & timestamp){
    ASSERT_ALWAYS_MSG_RETURN(ERROR_VIRTUAL_METHOD_CALL, kRocProfVisDmResultNotLoaded);
}
rocprofvis_dm_result_t RpvDmTrackSlice::GetRecordIdAt(const rocprofvis_dm_index_t index, rocprofvis_dm_id_t & id){
    ASSERT_ALWAYS_MSG_RETURN(ERROR_VIRTUAL_METHOD_CALL, kRocProfVisDmResultNotLoaded);
}
rocprofvis_dm_result_t RpvDmTrackSlice::GetRecordOperationAt(const rocprofvis_dm_index_t index, rocprofvis_dm_op_t & op){
    ASSERT_ALWAYS_MSG_RETURN(ERROR_VIRTUAL_METHOD_CALL, kRocProfVisDmResultNotLoaded);
}
rocprofvis_dm_result_t RpvDmTrackSlice::GetRecordValueAt(const rocprofvis_dm_index_t index, rocprofvis_dm_value_t & value){
    ASSERT_ALWAYS_MSG_RETURN(ERROR_VIRTUAL_METHOD_CALL, kRocProfVisDmResultNotLoaded);
}
rocprofvis_dm_result_t RpvDmTrackSlice::GetRecordDurationAt(const rocprofvis_dm_index_t index, rocprofvis_dm_duration_t & duration){
    ASSERT_ALWAYS_MSG_RETURN(ERROR_VIRTUAL_METHOD_CALL, kRocProfVisDmResultNotLoaded);
}
rocprofvis_dm_result_t RpvDmTrackSlice::GetRecordCategoryIndexAt(const rocprofvis_dm_index_t index, rocprofvis_dm_index_t & category_index){
    ASSERT_ALWAYS_MSG_RETURN(ERROR_VIRTUAL_METHOD_CALL, kRocProfVisDmResultNotLoaded);
}
rocprofvis_dm_result_t RpvDmTrackSlice::GetRecordSymbolIndexAt(const rocprofvis_dm_index_t index, rocprofvis_dm_index_t & symbol_index){
    ASSERT_ALWAYS_MSG_RETURN(ERROR_VIRTUAL_METHOD_CALL, kRocProfVisDmResultNotLoaded);
}
rocprofvis_dm_result_t RpvDmTrackSlice::GetRecordCategoryStringAt(const rocprofvis_dm_index_t index, rocprofvis_dm_charptr_t & category_charptr){
    ASSERT_ALWAYS_MSG_RETURN(ERROR_VIRTUAL_METHOD_CALL, kRocProfVisDmResultNotLoaded);
}
rocprofvis_dm_result_t RpvDmTrackSlice::GetRecordSymbolStringAt(const rocprofvis_dm_index_t, rocprofvis_dm_charptr_t & symbol_charptr){
    ASSERT_ALWAYS_MSG_RETURN(ERROR_VIRTUAL_METHOD_CALL, kRocProfVisDmResultNotLoaded);
}


rocprofvis_dm_result_t    RpvDmTrackSlice::GetPropertyAsUint64(rocprofvis_dm_slice_property_t property, rocprofvis_dm_property_index_t index, uint64_t* value){
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
rocprofvis_dm_result_t    RpvDmTrackSlice::GetPropertyAsInt64(rocprofvis_dm_slice_property_t property, rocprofvis_dm_property_index_t index, int64_t* value){
    switch(property)
    {
        case kRPVDMEventDurationInt64Indexed:
            return GetRecordDurationAt(index, *value);
        default:
            ASSERT_ALWAYS_MSG_RETURN(ERROR_INVALID_PROPERTY_GETTER, kRocProfVisDmResultInvalidProperty);
    }
}
 rocprofvis_dm_result_t   RpvDmTrackSlice::GetPropertyAsCharPtr(rocprofvis_dm_slice_property_t property, rocprofvis_dm_property_index_t index, char** value){
    switch(property)
    {
        case kRPVDMEventTypeStringCharPtrIndexed:
            return GetRecordCategoryStringAt(index, *(rocprofvis_dm_charptr_t*)value);
        break;
        case kRPVDMEventSymbolStringCharPtrIndexed:
            return GetRecordSymbolStringAt(index, *(rocprofvis_dm_charptr_t*)value);
        break;
        default:
            ASSERT_ALWAYS_MSG_RETURN(ERROR_INVALID_PROPERTY_GETTER, kRocProfVisDmResultInvalidProperty);
    }

}
rocprofvis_dm_result_t   RpvDmTrackSlice::GetPropertyAsDouble(rocprofvis_dm_slice_property_t property, rocprofvis_dm_property_index_t index, double* value){
    switch(property)
    {
        case kRPVDMPmcValueDoubleIndexed:
            return GetRecordValueAt(index, *value);
        break;
        default:
            ASSERT_ALWAYS_MSG_RETURN(ERROR_INVALID_PROPERTY_GETTER, kRocProfVisDmResultInvalidProperty);
    }

}
rocprofvis_dm_result_t    RpvDmTrackSlice::GetPropertyAsHandle(rocprofvis_dm_slice_property_t property, rocprofvis_dm_property_index_t index, rocprofvis_dm_handle_t* value){
    ASSERT_ALWAYS_MSG_RETURN(ERROR_INVALID_PROPERTY_GETTER, kRocProfVisDmResultInvalidProperty);
}