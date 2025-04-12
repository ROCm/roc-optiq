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

#include "rocprofvis_dm_pmc_track_slice.h"

namespace RocProfVis
{
namespace DataModel
{

//AddRecord method adds an object to a m_samples
rocprofvis_dm_result_t PmcTrackSlice::AddRecord(rocprofvis_db_record_data_t & data){
    try{
        m_samples.push_back(std::make_unique<PmcRecord>(data.pmc.timestamp,data.pmc.value));
    }
    catch(std::exception ex)
    {
        return kRocProfVisDmResultAllocFailure;
    }
    return kRocProfVisDmResultSuccess;
}

size_t PmcTrackSlice::GetMemoryFootprint(){
    return sizeof(std::vector<PmcRecord>) + ((sizeof(PmcRecord)+sizeof(std::unique_ptr<PmcRecord>)) * m_samples.size());
}

size_t PmcTrackSlice::GetNumberOfRecords(){
    return m_samples.size();
}

rocprofvis_dm_result_t PmcTrackSlice::ConvertTimestampToIndex(const rocprofvis_dm_timestamp_t timestamp, rocprofvis_dm_index_t & index){
    std::vector<std::unique_ptr<PmcRecord>>::iterator it = std::find_if( m_samples.begin(), m_samples.end(),
            [&timestamp](std::unique_ptr < PmcRecord> & x) { return x.get()->Timestamp() >= timestamp;});
    if (it != m_samples.end())
    {
        index = (rocprofvis_dm_index_t)(it - m_samples.begin());
        return kRocProfVisDmResultSuccess;
    }
    return kRocProfVisDmResultNotLoaded;
}

rocprofvis_dm_result_t PmcTrackSlice::GetRecordTimestampAt(const rocprofvis_dm_property_index_t index, rocprofvis_dm_timestamp_t & timestamp){
    ROCPROFVIS_ASSERT_MSG_RETURN(index < m_samples.size(), ERROR_INDEX_OUT_OF_RANGE, kRocProfVisDmResultNotLoaded);
    timestamp = m_samples[index].get()->Timestamp();
    return kRocProfVisDmResultSuccess;
}

rocprofvis_dm_result_t PmcTrackSlice::GetRecordValueAt(const rocprofvis_dm_property_index_t index, rocprofvis_dm_value_t & value){
    ROCPROFVIS_ASSERT_MSG_RETURN(index < m_samples.size(), ERROR_INDEX_OUT_OF_RANGE, kRocProfVisDmResultNotLoaded);
    value = m_samples[index].get()->Value();
    return kRocProfVisDmResultSuccess;
}

}  // namespace DataModel
}  // namespace RocProfVis