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

#include "PmcTrackSlice.h"

//AddRecord method adds an object to a m_samples
rocprofvis_dm_result_t RpvDmPmcTrackSlice::AddRecord(rocprofvis_db_record_data_t & data){
    try{
        m_samples.push_back(RpvDmPmcRecord(data.pmc.timestamp,data.pmc.value));
    }
    catch(std::exception ex)
    {
        return kRocProfVisDmResultAllocFailure;
    }
    return kRocProfVisDmResultSuccess;
}

size_t RpvDmPmcTrackSlice::GetMemoryFootprint(){
    return sizeof(std::vector<RpvDmPmcRecord>) + (sizeof(RpvDmPmcRecord) * m_samples.size());
}

size_t RpvDmPmcTrackSlice::GetNumberOfRecords(){
    return m_samples.size();
}

rocprofvis_dm_result_t RpvDmPmcTrackSlice::ConvertTimestampToIndex(const rocprofvis_dm_timestamp_t timestamp, rocprofvis_dm_index_t & index){
    std::vector<RpvDmPmcRecord>::iterator it = std::find_if( m_samples.begin(), m_samples.end(),
            [&timestamp](RpvDmPmcRecord & x) { return x.Timestamp() >= timestamp;});
    if (it != m_samples.end())
    {
        index = (rocprofvis_dm_index_t)(it - m_samples.begin());
        return kRocProfVisDmResultSuccess;
    }
    return kRocProfVisDmResultNotLoaded;
}

rocprofvis_dm_result_t RpvDmPmcTrackSlice::GetRecordTimestampAt(const rocprofvis_dm_index_t index, rocprofvis_dm_timestamp_t & timestamp){
    ASSERT_MSG_RETURN(index < m_samples.size(), ERROR_INDEX_OUT_OF_RANGE, kRocProfVisDmResultNotLoaded);
    timestamp = m_samples[index].Timestamp();
    return kRocProfVisDmResultSuccess;
}

rocprofvis_dm_result_t RpvDmPmcTrackSlice::GetRecordValueAt(const rocprofvis_dm_index_t index, rocprofvis_dm_value_t & value){
    ASSERT_MSG_RETURN(index < m_samples.size(), ERROR_INDEX_OUT_OF_RANGE, kRocProfVisDmResultNotLoaded);
    value = m_samples[index].Value();
    return kRocProfVisDmResultSuccess;
}