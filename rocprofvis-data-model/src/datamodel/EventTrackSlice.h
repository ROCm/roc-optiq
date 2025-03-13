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

#ifndef RPV_DATAMODEL_EVENT_TRACK_SLICE_H
#define RPV_DATAMODEL_EVENT_TRACK_SLICE_H

#include "TrackSlice.h"
#include "EventRecord.h"
#include <vector>

/* Class for Event time slices, based on pvDmTrackSlice
** Overrides set of methods for accessing array of Event records 
*/
class RpvDmEventTrackSlice : public RpvDmTrackSlice {
    public:
        RpvDmEventTrackSlice(   RpvDmTrack* ctx, 
                                rocprofvis_dm_timestamp_t start, 
                                rocprofvis_dm_timestamp_t end) : 
                                RpvDmTrackSlice(ctx, start, end) {}; 

        rocprofvis_dm_result_t  AddRecord( rocprofvis_db_record_data_t & data) override;
        rocprofvis_dm_size_t    GetMemoryFootprint() override;
        rocprofvis_dm_size_t    GetNumberOfRecords() override;
        rocprofvis_dm_result_t  ConvertTimestampToIndex(const rocprofvis_dm_timestamp_t timestamp, rocprofvis_dm_index_t & index) override;

        rocprofvis_dm_result_t  GetRecordTimestampAt(const rocprofvis_dm_property_index_t index, rocprofvis_dm_timestamp_t & timestamp) override;
        rocprofvis_dm_result_t  GetRecordIdAt(const rocprofvis_dm_property_index_t index, rocprofvis_dm_id_t & op) override;
        rocprofvis_dm_result_t  GetRecordOperationAt(const rocprofvis_dm_property_index_t index, rocprofvis_dm_op_t & id) override;
        rocprofvis_dm_result_t  GetRecordDurationAt(const rocprofvis_dm_property_index_t index, rocprofvis_dm_duration_t & duration) override;
        rocprofvis_dm_result_t  GetRecordCategoryIndexAt(const rocprofvis_dm_property_index_t index, rocprofvis_dm_index_t & category_index) override;
        rocprofvis_dm_result_t  GetRecordSymbolIndexAt(const rocprofvis_dm_property_index_t index, rocprofvis_dm_index_t & symbol_index) override;
        rocprofvis_dm_result_t  GetRecordCategoryStringAt(const rocprofvis_dm_property_index_t index, rocprofvis_dm_charptr_t & category_charptr) override;
        rocprofvis_dm_result_t  GetRecordSymbolStringAt(const rocprofvis_dm_property_index_t index, rocprofvis_dm_charptr_t & symbol_charptr) override;

    private:

        std::vector<RpvDmEventRecord>    m_samples;
};

#endif //RPV_DATAMODEL_EVENT_TRACK_SLICE_H