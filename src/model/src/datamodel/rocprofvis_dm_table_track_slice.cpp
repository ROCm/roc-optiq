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

#include "rocprofvis_dm_table_track_slice.h"
#include "rocprofvis_dm_track.h"
#include "rocprofvis_dm_trace.h"

namespace RocProfVis
{
namespace DataModel
{
rocprofvis_dm_result_t TableSlice::AddRecord(uint32_t track, rocprofvis_db_record_data_t& data)
{
    ROCPROFVIS_ASSERT(m_track_map.find(track) != m_track_map.end());
    m_tracks.push_back(m_track_map[track]);
    return AddRecord(data);
}

rocprofvis_dm_result_t TableSlice::AddRecord(rocprofvis_db_record_data_t & data){
    return m_records->AddRecord(data);
}

rocprofvis_dm_size_t TableSlice::GetMemoryFootprint(){
    return sizeof(TableSlice) + m_records->GetMemoryFootprint();
}

rocprofvis_dm_size_t TableSlice::GetNumberOfRecords(){
    return m_records->GetNumberOfRecords();
}

rocprofvis_dm_result_t TableSlice::ConvertTimestampToIndex(const rocprofvis_dm_timestamp_t timestamp, rocprofvis_dm_index_t & index){
    return m_records->ConvertTimestampToIndex(timestamp, index);
}

rocprofvis_dm_result_t TableSlice::GetRecordTimestampAt(const rocprofvis_dm_property_index_t index, rocprofvis_dm_timestamp_t & timestamp){
    return m_records->GetRecordTimestampAt(index, timestamp);
}

rocprofvis_dm_result_t TableSlice::GetRecordIdAt(const rocprofvis_dm_property_index_t index, rocprofvis_dm_id_t & id){
    return m_records->GetRecordIdAt(index, id);
}

rocprofvis_dm_result_t TableSlice::GetRecordOperationAt(const rocprofvis_dm_property_index_t index, rocprofvis_dm_op_t & op){
    return m_records->GetRecordOperationAt(index, op);
}


rocprofvis_dm_result_t TableSlice::GetRecordOperationStringAt(const rocprofvis_dm_property_index_t index, rocprofvis_dm_charptr_t & op){
    return m_records->GetRecordOperationStringAt(index, op);
}

rocprofvis_dm_result_t  TableSlice::GetRecordDurationAt(const rocprofvis_dm_property_index_t index, rocprofvis_dm_duration_t & duration){
    return m_records->GetRecordDurationAt(index, duration);
}

rocprofvis_dm_result_t  TableSlice::GetRecordCategoryIndexAt(const rocprofvis_dm_property_index_t index, rocprofvis_dm_index_t & category_index){
    return m_records->GetRecordCategoryIndexAt(index, category_index);
}

rocprofvis_dm_result_t  TableSlice::GetRecordSymbolIndexAt(const rocprofvis_dm_property_index_t index, rocprofvis_dm_index_t & symbol_index){
    return m_records->GetRecordSymbolIndexAt(index, symbol_index);
}

rocprofvis_dm_result_t  TableSlice::GetRecordCategoryStringAt(const rocprofvis_dm_property_index_t index, rocprofvis_dm_charptr_t & category_charptr){
    ROCPROFVIS_ASSERT_MSG_RETURN(index < m_tracks.size(), ERROR_INDEX_OUT_OF_RANGE, kRocProfVisDmResultNotLoaded);
    ROCPROFVIS_ASSERT(m_tracks[index]);
    ROCPROFVIS_ASSERT_MSG_RETURN(m_tracks[index], ERROR_TRACK_CANNOT_BE_NULL,
                                 kRocProfVisDmResultNotLoaded);
    ROCPROFVIS_ASSERT_MSG_RETURN(m_tracks[index]->Ctx(), ERROR_TRACE_CANNOT_BE_NULL,
                                 kRocProfVisDmResultNotLoaded);
    rocprofvis_dm_index_t category_index;
    rocprofvis_dm_result_t result =
        m_records->GetRecordCategoryIndexAt(index, category_index);
    if(result == kRocProfVisDmResultSuccess)
    {
        category_charptr = m_tracks[index]->Ctx()->GetStringAt(category_index);
    }
    return kRocProfVisDmResultSuccess;
}

rocprofvis_dm_result_t  TableSlice::GetRecordSymbolStringAt(const rocprofvis_dm_property_index_t index, rocprofvis_dm_charptr_t & symbol_charptr){
    ROCPROFVIS_ASSERT_MSG_RETURN(index < m_tracks.size(), ERROR_INDEX_OUT_OF_RANGE, kRocProfVisDmResultNotLoaded);
    ROCPROFVIS_ASSERT(m_tracks[index]);
    ROCPROFVIS_ASSERT_MSG_RETURN(m_tracks[index], ERROR_TRACK_CANNOT_BE_NULL,
                                 kRocProfVisDmResultNotLoaded);
    ROCPROFVIS_ASSERT_MSG_RETURN(m_tracks[index]->Ctx(), ERROR_TRACE_CANNOT_BE_NULL,
                                 kRocProfVisDmResultNotLoaded);
    rocprofvis_dm_index_t symbol_index;
    rocprofvis_dm_result_t result = m_records->GetRecordSymbolIndexAt(index, symbol_index);
    if(result == kRocProfVisDmResultSuccess)
    {
        symbol_charptr = m_tracks[index]->Ctx()->GetStringAt(symbol_index);
    }
    return result;
}

rocprofvis_dm_result_t TableSlice::GetRecordGraphLevelAt(const rocprofvis_dm_property_index_t index, rocprofvis_dm_event_level_t& level) {
    ROCPROFVIS_ASSERT_MSG_RETURN(index < m_tracks.size(), ERROR_INDEX_OUT_OF_RANGE, kRocProfVisDmResultNotLoaded);
    ROCPROFVIS_ASSERT(m_tracks[index]);
    ROCPROFVIS_ASSERT_MSG_RETURN(m_tracks[index], ERROR_TRACK_CANNOT_BE_NULL,
                                 kRocProfVisDmResultNotLoaded);
    ROCPROFVIS_ASSERT_MSG_RETURN(m_tracks[index]->Ctx(), ERROR_TRACE_CANNOT_BE_NULL,
                                 kRocProfVisDmResultNotLoaded);
    rocprofvis_dm_event_id_t event_id;
    rocprofvis_dm_result_t result = ((EventTrackSlice*)m_records)->GetRecordEventIdFullAt(index, event_id);
    if(result == kRocProfVisDmResultSuccess)
    {
        level = m_tracks[index]->Ctx()->GetEventLevelAt(event_id);
    }
    return result;
}

rocprofvis_dm_result_t TableSlice::GetPropertyAsUint64(rocprofvis_dm_property_t property, 
                                                    rocprofvis_dm_property_index_t index, 
                                                    uint64_t* value)
{
    rocprofvis_dm_result_t result = kRocProfVisDmResultInvalidParameter;
    switch (property)
    {
        case kRPVDMTableTrackIdIndexed:
        {
            if(index < m_tracks.size())
            {
                *value = m_tracks[index]->TrackId();
            }
            break;
        }
        case kRPVDMTableTrackCategoryNameIndexed:
        case kRPVDMTableTrackMainProcessNameIndexed:
        case kRPVDMTableTrackSubProcessNameIndexed:
        {
            result = kRocProfVisDmResultInvalidParameter;
            break;
        }
        default:
        {
            result = m_records->GetPropertyAsUint64(property, index, value);
            break;
        }
    }
    return result;
}

rocprofvis_dm_result_t TableSlice::GetPropertyAsCharPtr(rocprofvis_dm_property_t property, 
                                                    rocprofvis_dm_property_index_t index, 
                                                    char** value)
{
    rocprofvis_dm_result_t result = kRocProfVisDmResultInvalidParameter;
    switch(property)
    {
        case kRPVDMTableTrackIdIndexed:
        {
            result = kRocProfVisDmResultInvalidParameter;
            break;
        }
        case kRPVDMEventTypeStringCharPtrIndexed:
        {
            result = GetRecordCategoryStringAt(index, *(rocprofvis_dm_charptr_t*) value);
            break;
        }
        case kRPVDMEventSymbolStringCharPtrIndexed:
        {
            result = GetRecordSymbolStringAt(index, *(rocprofvis_dm_charptr_t*) value);
            break;
        }
        case kRPVDMEventIdOperationCharPtrIndexed:
        {
            result = GetRecordOperationStringAt(index, *(rocprofvis_dm_charptr_t*) value);
            break;
        }
        case kRPVDMTableTrackCategoryNameIndexed:
        {
            if(index < m_tracks.size())
            {
                result = m_tracks[index]->GetPropertyAsCharPtr(kRPVDMTrackCategoryEnumCharPtr, 0, value);
            }
            break;
        }
        case kRPVDMTableTrackMainProcessNameIndexed:
        {
            if(index < m_tracks.size())
            {
                result = m_tracks[index]->GetPropertyAsCharPtr(kRPVDMTrackMainProcessNameCharPtr, 0, value);
            }
            break;
        }
        case kRPVDMTableTrackSubProcessNameIndexed:
        {
            if(index < m_tracks.size())
            {
                result = m_tracks[index]->GetPropertyAsCharPtr(kRPVDMTrackSubProcessNameCharPtr, 0, value);
            }
            break;
        }
        default:
        {
            result = m_records->GetPropertyAsCharPtr(property, index, value);
            break;
        }
    }
    return result;
}

}  // namespace DataModel
}  // namespace RocProfVis