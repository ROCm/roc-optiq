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

#include "EventTrackSlice.h"
#include "Track.h"
#include "Trace.h"


rocprofvis_dm_result_t RpvDmEventTrackSlice::AddRecord(rocprofvis_db_record_data_t & data){
    try {
        m_samples.push_back(RpvDmEventRecord(data.event.op, data.event.id, data.event.timestamp, data.event.duration, data.event.category, data.event.symbol)); 
    }
    catch(std::exception ex)
    {
        return kRocProfVisDmResultAllocFailure;
    }
    return kRocProfVisDmResultSuccess;
}

rocprofvis_dm_size_t RpvDmEventTrackSlice::GetMemoryFootprint(){
    return sizeof(RpvDmEventTrackSlice) + (sizeof(RpvDmEventRecord) * m_samples.size());
}

rocprofvis_dm_size_t RpvDmEventTrackSlice::GetNumberOfRecords(){
    return m_samples.size();
}

rocprofvis_dm_result_t RpvDmEventTrackSlice::ConvertTimestampToIndex(const rocprofvis_dm_timestamp_t timestamp, rocprofvis_dm_index_t & index){
    std::vector<RpvDmEventRecord>::iterator it = std::find_if( m_samples.begin(), m_samples.end(),
            [&timestamp](RpvDmEventRecord & x) { return x.Timestamp() >= timestamp;});
    if (it != m_samples.end())
    {
        index = (uint32_t)(it - m_samples.begin());
        return kRocProfVisDmResultSuccess;
    }
    return kRocProfVisDmResultNotLoaded;
}

rocprofvis_dm_result_t RpvDmEventTrackSlice::GetRecordTimestampAt(const rocprofvis_dm_index_t index, rocprofvis_dm_timestamp_t & timestamp){
    ASSERT_MSG_RETURN(index < m_samples.size(), ERROR_INDEX_OUT_OF_RANGE, kRocProfVisDmResultNotLoaded);
    timestamp = m_samples[index].Timestamp();
    return kRocProfVisDmResultSuccess;
}

rocprofvis_dm_result_t RpvDmEventTrackSlice::GetRecordIdAt(const rocprofvis_dm_index_t index, rocprofvis_dm_id_t & id){
    ASSERT_MSG_RETURN(index < m_samples.size(), ERROR_INDEX_OUT_OF_RANGE, kRocProfVisDmResultNotLoaded);
    id = m_samples[index].EventId();
    return kRocProfVisDmResultSuccess;
}

rocprofvis_dm_result_t RpvDmEventTrackSlice::GetRecordOperationAt(const rocprofvis_dm_index_t index, rocprofvis_dm_op_t & op){
    ASSERT_MSG_RETURN(index < m_samples.size(), ERROR_INDEX_OUT_OF_RANGE, kRocProfVisDmResultNotLoaded);
    op = m_samples[index].Operation();
    return kRocProfVisDmResultSuccess;
}

rocprofvis_dm_result_t  RpvDmEventTrackSlice::GetRecordDurationAt(const rocprofvis_dm_index_t index, rocprofvis_dm_duration_t & duration){
    ASSERT_MSG_RETURN(index < m_samples.size(), ERROR_INDEX_OUT_OF_RANGE, kRocProfVisDmResultNotLoaded);
    duration = m_samples[index].Duration();
    return kRocProfVisDmResultSuccess;
}

rocprofvis_dm_result_t  RpvDmEventTrackSlice::GetRecordCategoryIndexAt(const rocprofvis_dm_index_t index, rocprofvis_dm_index_t & category_index){
    ASSERT_MSG_RETURN(index < m_samples.size(), ERROR_INDEX_OUT_OF_RANGE, kRocProfVisDmResultNotLoaded);
    category_index = m_samples[index].CategoryIndex();
    return kRocProfVisDmResultSuccess; 
}

rocprofvis_dm_result_t  RpvDmEventTrackSlice::GetRecordSymbolIndexAt(const rocprofvis_dm_index_t index, rocprofvis_dm_index_t & symbol_index){
    ASSERT_MSG_RETURN(index < m_samples.size(), ERROR_INDEX_OUT_OF_RANGE, kRocProfVisDmResultNotLoaded);
    symbol_index = m_samples[index].SymbolIndex();
    return kRocProfVisDmResultSuccess;
}

rocprofvis_dm_result_t  RpvDmEventTrackSlice::GetRecordCategoryStringAt(const rocprofvis_dm_index_t index, rocprofvis_dm_charptr_t & category_charptr){
    ASSERT_MSG_RETURN(index < m_samples.size(), ERROR_INDEX_OUT_OF_RANGE, kRocProfVisDmResultNotLoaded);
    category_charptr = Ctx()->Ctx()->GetStringAt(m_samples[index].CategoryIndex());
    return kRocProfVisDmResultSuccess;
}

rocprofvis_dm_result_t  RpvDmEventTrackSlice::GetRecordSymbolStringAt(const rocprofvis_dm_index_t index, rocprofvis_dm_charptr_t & symbol_charptr){
    ASSERT_MSG_RETURN(index < m_samples.size(), ERROR_INDEX_OUT_OF_RANGE, kRocProfVisDmResultNotLoaded);
    symbol_charptr = Ctx()->Ctx()->GetSymbolAt(m_samples[index].SymbolIndex());
    return kRocProfVisDmResultSuccess;
}