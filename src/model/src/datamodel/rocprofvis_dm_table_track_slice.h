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
#include "rocprofvis_dm_event_track_slice.h"
#include "rocprofvis_dm_pmc_track_slice.h"
#include "rocprofvis_core_assert.h"
#include <vector>
#include <map>
#include <memory>

namespace RocProfVis
{
namespace DataModel
{

/* Class for table slices, based on pvDmTrackSlice
** Overrides set of methods for accessing array of records 
*/
class TableSlice : public TrackSlice {
    // Method to add event record to the time slice
    // @param data - reference to a structure with record data
    // @return status of operation
    rocprofvis_dm_result_t AddRecord(rocprofvis_db_record_data_t& data) override;

    public:
        // TableSlice class constructor
        // @param start - start time of the slice
        // @param end - end time of the slice
        TableSlice(             TrackSlice* records,
                                rocprofvis_dm_timestamp_t start, 
                                rocprofvis_dm_timestamp_t end,
                                std::map<uint32_t, Track*>& track_map)
        : TrackSlice(nullptr, start, end)
        , m_track_map(track_map)
        , m_records(records)
        {
            ROCPROFVIS_ASSERT(m_records);
        } 
        // TableSlice class destructor, not required unless declared as virtual
        ~TableSlice()
        {
            delete m_records;
        }
        // Method to add event record to the time slice
        // @param track - track for the record
        // @param data - reference to a structure with record data
        // @return status of operation
        rocprofvis_dm_result_t AddRecord(uint32_t track, rocprofvis_db_record_data_t& data);
        // Method to get amount of memory used by the class object
        // @return used memory size
        rocprofvis_dm_size_t    GetMemoryFootprint() override;
        // Method to get time slice number of event records
        // @return number of records
        rocprofvis_dm_size_t    GetNumberOfRecords() override;
        // Method to convert a timestamp to index in event record array 
        // @param timestamp - timestamp to convert
        // @param index - reference to index
        // @return status of operation
        rocprofvis_dm_result_t  ConvertTimestampToIndex(const rocprofvis_dm_timestamp_t timestamp, rocprofvis_dm_index_t & index) override;
        // Method to get event timestamp value by provided index of record
        // @param index - index of the record
        // @param timestamp - reference to timestamp value
        // @return status of operation
        rocprofvis_dm_result_t  GetRecordTimestampAt(const rocprofvis_dm_property_index_t index, rocprofvis_dm_timestamp_t & timestamp) override;
        // Method to get event ID value by provided index of record
        // @param index - index of the record
        // @param id - reference to ID value
        // @return status of operation
        rocprofvis_dm_result_t  GetRecordIdAt(const rocprofvis_dm_property_index_t index, rocprofvis_dm_id_t & id) override;
        // Method to get event operation value by provided index of record
        // @param index - index of the record
        // @param op - reference to operation value
        // @return status of operation
        rocprofvis_dm_result_t  GetRecordOperationAt(const rocprofvis_dm_property_index_t index, rocprofvis_dm_op_t & op) override;
        // Method to get event operation string by provided index of record
        // @param index - index of the record
        // @param op - reference to operation string
        // @return status of operation        
        rocprofvis_dm_result_t  GetRecordOperationStringAt(const rocprofvis_dm_property_index_t index, rocprofvis_dm_charptr_t & op) override;
        // Method to get event duration value by provided index of record
        // @param index - index of the record
        // @param duration - reference to duration value
        // @return status of operation
        rocprofvis_dm_result_t  GetRecordDurationAt(const rocprofvis_dm_property_index_t index, rocprofvis_dm_duration_t & duration) override;
        // Method to get event category string index by provided index of record
        // @param index - index of the record
        // @param category_index - reference to category index
        // @return status of operation       
        rocprofvis_dm_result_t  GetRecordCategoryIndexAt(const rocprofvis_dm_property_index_t index, rocprofvis_dm_index_t & category_index) override;
        // Method to get event symbol string index by provided index of record
        // @param index - index of the record
        // @param symbol_index - reference to symbol index
        // @return status of operation 
        rocprofvis_dm_result_t  GetRecordSymbolIndexAt(const rocprofvis_dm_property_index_t index, rocprofvis_dm_index_t & symbol_index) override;
        // Method to get event category string by provided index of record
        // @param index - index of the record
        // @param category_charptr - reference to category string
        // @return status of operation         
        rocprofvis_dm_result_t  GetRecordCategoryStringAt(const rocprofvis_dm_property_index_t index, rocprofvis_dm_charptr_t & category_charptr) override;
        // Method to get event symbol string by provided index of record
        // @param index - index of the record
        // @param symbol_charptr - reference to symbol string
        // @return status of operation  
        rocprofvis_dm_result_t  GetRecordSymbolStringAt(const rocprofvis_dm_property_index_t index, rocprofvis_dm_charptr_t & symbol_charptr) override;
        // Method to get event level value by provided index of record
        // @param index - index of the record
        // @param level - graph level for the event
        // @return status of operation
        rocprofvis_dm_result_t GetRecordGraphLevelAt(const rocprofvis_dm_property_index_t index, rocprofvis_dm_event_level_t & level) override;
        // Method to read TrackSlice object property as uint64
        // @param property - property enumeration rocprofvis_dm_slice_property_t
        // @param index - index of any indexed property
        // @param value - pointer reference to uint64_t return value
        // @return status of operation
        rocprofvis_dm_result_t GetPropertyAsUint64(rocprofvis_dm_property_t property,
                                                   rocprofvis_dm_property_index_t index,
                                                   uint64_t* value) override;
        // Method to read TrackSlice object property as char*
        // @param property - property enumeration rocprofvis_dm_slice_property_t
        // @param index - index of any indexed property
        // @param value - pointer reference to char* return value
        // @return status of operation
        rocprofvis_dm_result_t GetPropertyAsCharPtr(rocprofvis_dm_property_t property,
                                                    rocprofvis_dm_property_index_t index,
                                                    char** value) override;
    private:
        std::map<uint32_t, Track*> m_track_map;
        // vector array of tracks that match samples for underlying record slice
        std::vector<Track*> m_tracks;
        // Slice containing actual records
        TrackSlice* m_records;
};

}  // namespace DataModel
}  // namespace RocProfVis