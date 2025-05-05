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

#include "rocprofvis_dm_event_track_slice.h"
#include "rocprofvis_dm_track.h"
#include "rocprofvis_dm_trace.h"

namespace RocProfVis
{
namespace DataModel
{

rocprofvis_dm_result_t EventTrackSlice::AddRecord(rocprofvis_db_record_data_t & data){
    try {
        m_samples.push_back(std::make_unique<EventRecord>(data.event.id, data.event.timestamp, data.event.duration, data.event.category, data.event.symbol));
    }
    catch(std::exception ex)
    {
        return kRocProfVisDmResultAllocFailure;
    }
    return kRocProfVisDmResultSuccess;
}

rocprofvis_dm_size_t EventTrackSlice::GetMemoryFootprint(){
    return sizeof(EventTrackSlice) + ((sizeof(EventRecord)+sizeof(std::unique_ptr<EventRecord>)) * m_samples.size());
}

rocprofvis_dm_size_t EventTrackSlice::GetNumberOfRecords(){
    return m_samples.size();
}

rocprofvis_dm_result_t EventTrackSlice::ConvertTimestampToIndex(const rocprofvis_dm_timestamp_t timestamp, rocprofvis_dm_index_t & index){
    std::vector<std::unique_ptr<EventRecord>>::iterator it = std::find_if( m_samples.begin(), m_samples.end(),
            [&timestamp](std::unique_ptr<EventRecord> & x) { return x.get()->Timestamp() >= timestamp;});
    if (it != m_samples.end())
    {
        index = (uint32_t)(it - m_samples.begin());
        return kRocProfVisDmResultSuccess;
    }
    return kRocProfVisDmResultNotLoaded;
}

rocprofvis_dm_result_t EventTrackSlice::GetRecordTimestampAt(const rocprofvis_dm_property_index_t index, rocprofvis_dm_timestamp_t & timestamp){
    ROCPROFVIS_ASSERT_MSG_RETURN(index < m_samples.size(), ERROR_INDEX_OUT_OF_RANGE, kRocProfVisDmResultNotLoaded);
    timestamp = m_samples[index].get()->Timestamp();
    return kRocProfVisDmResultSuccess;
}

rocprofvis_dm_result_t EventTrackSlice::GetRecordIdAt(const rocprofvis_dm_property_index_t index, rocprofvis_dm_id_t & id){
    ROCPROFVIS_ASSERT_MSG_RETURN(index < m_samples.size(), ERROR_INDEX_OUT_OF_RANGE, kRocProfVisDmResultNotLoaded);
    id = m_samples[index].get()->EventId();
    return kRocProfVisDmResultSuccess;
}

rocprofvis_dm_result_t EventTrackSlice::GetRecordOperationAt(const rocprofvis_dm_property_index_t index, rocprofvis_dm_op_t & op){
    ROCPROFVIS_ASSERT_MSG_RETURN(index < m_samples.size(), ERROR_INDEX_OUT_OF_RANGE, kRocProfVisDmResultNotLoaded);
    op = m_samples[index].get()->Operation();
    return kRocProfVisDmResultSuccess;
}


rocprofvis_dm_result_t EventTrackSlice::GetRecordOperationStringAt(const rocprofvis_dm_property_index_t index, rocprofvis_dm_charptr_t & op){
    ROCPROFVIS_ASSERT_MSG_RETURN(index < m_samples.size(), ERROR_INDEX_OUT_OF_RANGE, kRocProfVisDmResultNotLoaded);
    op = "Invalid";
    switch (m_samples[index].get()->Operation()){
        case kRocProfVisDmOperationLaunch: 
            op = "Launch";
            break;
        case kRocProfVisDmOperationDispatch: 
            op = "Dispatch";
            break;
        case kRocProfVisDmOperationMemoryAllocate: 
            op = "MemAlloc";
            break;
        case kRocProfVisDmOperationMemoryCopy: 
            op = "MemCopy";
            break;
    }
    return kRocProfVisDmResultSuccess;
}

rocprofvis_dm_result_t  EventTrackSlice::GetRecordDurationAt(const rocprofvis_dm_property_index_t index, rocprofvis_dm_duration_t & duration){
    ROCPROFVIS_ASSERT_MSG_RETURN(index < m_samples.size(), ERROR_INDEX_OUT_OF_RANGE, kRocProfVisDmResultNotLoaded);
    duration = m_samples[index].get()->Duration();
    return kRocProfVisDmResultSuccess;
}

rocprofvis_dm_result_t  EventTrackSlice::GetRecordCategoryIndexAt(const rocprofvis_dm_property_index_t index, rocprofvis_dm_index_t & category_index){
    ROCPROFVIS_ASSERT_MSG_RETURN(index < m_samples.size(), ERROR_INDEX_OUT_OF_RANGE, kRocProfVisDmResultNotLoaded);
    category_index = m_samples[index].get()->CategoryIndex();
    return kRocProfVisDmResultSuccess; 
}

rocprofvis_dm_result_t  EventTrackSlice::GetRecordSymbolIndexAt(const rocprofvis_dm_property_index_t index, rocprofvis_dm_index_t & symbol_index){
    ROCPROFVIS_ASSERT_MSG_RETURN(index < m_samples.size(), ERROR_INDEX_OUT_OF_RANGE, kRocProfVisDmResultNotLoaded);
    symbol_index = m_samples[index].get()->SymbolIndex();
    return kRocProfVisDmResultSuccess;
}

rocprofvis_dm_result_t  EventTrackSlice::GetRecordCategoryStringAt(const rocprofvis_dm_property_index_t index, rocprofvis_dm_charptr_t & category_charptr){
    ROCPROFVIS_ASSERT_MSG_RETURN(index < m_samples.size(), ERROR_INDEX_OUT_OF_RANGE, kRocProfVisDmResultNotLoaded);
    ROCPROFVIS_ASSERT_MSG_RETURN(Ctx(), ERROR_TRACK_CANNOT_BE_NULL, kRocProfVisDmResultNotLoaded);
    ROCPROFVIS_ASSERT_MSG_RETURN(Ctx()->Ctx(), ERROR_TRACE_CANNOT_BE_NULL, kRocProfVisDmResultNotLoaded);
    category_charptr = Ctx()->Ctx()->GetStringAt(m_samples[index].get()->CategoryIndex());
    return kRocProfVisDmResultSuccess;
}

rocprofvis_dm_result_t  EventTrackSlice::GetRecordSymbolStringAt(const rocprofvis_dm_property_index_t index, rocprofvis_dm_charptr_t & symbol_charptr){
    ROCPROFVIS_ASSERT_MSG_RETURN(index < m_samples.size(), ERROR_INDEX_OUT_OF_RANGE, kRocProfVisDmResultNotLoaded);
    ROCPROFVIS_ASSERT_MSG_RETURN(Ctx(), ERROR_TRACK_CANNOT_BE_NULL, kRocProfVisDmResultNotLoaded);
    ROCPROFVIS_ASSERT_MSG_RETURN(Ctx()->Ctx(), ERROR_TRACE_CANNOT_BE_NULL, kRocProfVisDmResultNotLoaded);
    symbol_charptr = Ctx()->Ctx()->GetStringAt(m_samples[index].get()->SymbolIndex());
    return kRocProfVisDmResultSuccess;
}

rocprofvis_dm_result_t EventTrackSlice::GetRecordGraphLevelAt(const rocprofvis_dm_property_index_t index, rocprofvis_dm_event_level_t& level) {
    ROCPROFVIS_ASSERT_MSG_RETURN(index < m_samples.size(), ERROR_INDEX_OUT_OF_RANGE, kRocProfVisDmResultNotLoaded);
    ROCPROFVIS_ASSERT_MSG_RETURN(Ctx(), ERROR_TRACK_CANNOT_BE_NULL, kRocProfVisDmResultNotLoaded);
    ROCPROFVIS_ASSERT_MSG_RETURN(Ctx()->Ctx(), ERROR_TRACE_CANNOT_BE_NULL, kRocProfVisDmResultNotLoaded);
    level = Ctx()->Ctx()->GetEventLevelAt(m_samples[index].get()->EventIdFull());
    return kRocProfVisDmResultSuccess;
}

}  // namespace DataModel
}  // namespace RocProfVis