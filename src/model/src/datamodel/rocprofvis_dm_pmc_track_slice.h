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

#pragma once

#include "rocprofvis_dm_track_slice.h"
#include "rocprofvis_dm_pmc_record.h"
#include <vector>
#include <memory>

namespace RocProfVis
{
namespace DataModel
{

class Track;

/* Class for PMC time slices, based on pvDmTrackSlice
** Overrides set of methods for accessing array of PMC records 
*/
class PmcTrackSlice : public TrackSlice {
    public:
        // PmcTrackSlice class constructor
        // @param ctx - pointer to Track context
        // @param start - start timestamp of time slice
        // @param end - end timestamp of time slice
        PmcTrackSlice(Track* ctx, rocprofvis_dm_timestamp_t start,
                      rocprofvis_dm_timestamp_t end); 
        // PmcTrackSlice class destructor, not required unless declared as virtual
        ~PmcTrackSlice(){}
        // Method to add PMC data record
        // @param data - reference to a structure with the record data
        // @return status of operation
        rocprofvis_dm_result_t  AddRecord( rocprofvis_db_record_data_t & data) override;
        // Method to get amount of memory used by PmcTrackSlice class object
        // @return used memory size
        rocprofvis_dm_size_t    GetMemoryFootprint() override;
        // Method to get number of PMC records
        // @return number of records
        rocprofvis_dm_size_t    GetNumberOfRecords() override;
        // Method to convert a timestamp to an index of a record 
        // @param timestamp - timestamp to convert
        // @param index - reference to index
        // @return status of operation        
        rocprofvis_dm_result_t  ConvertTimestampToIndex(const rocprofvis_dm_timestamp_t timestamp, rocprofvis_dm_index_t& index) override;

        // Method to get PMC timestamp value by provided index of record
        // @param index - index of the record
        // @param timestamp - reference to timestamp value
        // @return status of operation
        rocprofvis_dm_result_t  GetRecordTimestampAt(const rocprofvis_dm_property_index_t index, rocprofvis_dm_timestamp_t & timestamp) override;
        // Method to get PMC value by provided index of record
        // @param index - index of the record
        // @param value - reference to PMC value
        // @return status of operation
        rocprofvis_dm_result_t  GetRecordValueAt(const rocprofvis_dm_property_index_t index, rocprofvis_dm_value_t & value) override;

    private:
        // vector array of PMC records
        std::vector<std::unique_ptr<PmcRecord>>      m_samples;
};

}  // namespace DataModel
}  // namespace RocProfVis