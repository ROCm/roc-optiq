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

#ifndef RPV_DATAMODEL_TRACK_SLICE_H
#define RPV_DATAMODEL_TRACK_SLICE_H


#include "../common/CommonTypes.h"
#include "RpvObject.h"

/* Base class for PMC and Event time slices
** Defines set of virtual methods to access an array of POD storage objects 
*/

class RpvDmTrack;

class RpvDmTrackSlice : public RpvObject {
    public:
        RpvDmTrackSlice(    RpvDmTrack* ctx, 
                            rocprofvis_dm_timestamp_t start, 
                            rocprofvis_dm_timestamp_t end) : 
                            m_ctx(ctx), 
                            m_start_timestamp(start), 
                            m_end_timestamp(end) {}; 
        virtual ~RpvDmTrackSlice() {}
        virtual rocprofvis_dm_result_t  AddRecord( rocprofvis_db_record_data_t & data) = 0;
        virtual rocprofvis_dm_size_t    GetMemoryFootprint() = 0;
        virtual rocprofvis_dm_size_t    GetNumberOfRecords() = 0;
        virtual rocprofvis_dm_result_t  ConvertTimestampToIndex(const rocprofvis_dm_timestamp_t timestamp, 
                                                                    rocprofvis_dm_index_t & index)=0;

        // getters
        RpvDmTrack*                     Ctx() {return m_ctx;};
        rocprofvis_dm_timestamp_t       StartTime() {return m_start_timestamp;}
        rocprofvis_dm_timestamp_t       EndTime() {return m_end_timestamp;}

        // accessors
        virtual rocprofvis_dm_result_t  GetRecordTimestampAt(const rocprofvis_dm_property_index_t index, rocprofvis_dm_timestamp_t & timestamp) = 0;
        virtual rocprofvis_dm_result_t  GetRecordIdAt(const rocprofvis_dm_property_index_t index, rocprofvis_dm_id_t & id);
        virtual rocprofvis_dm_result_t  GetRecordOperationAt(const rocprofvis_dm_property_index_t index, rocprofvis_dm_op_t & op);
        virtual rocprofvis_dm_result_t  GetRecordOperationStringAt(const rocprofvis_dm_property_index_t index, rocprofvis_dm_charptr_t & op);
        virtual rocprofvis_dm_result_t  GetRecordValueAt(const rocprofvis_dm_property_index_t index, rocprofvis_dm_value_t & value);
        virtual rocprofvis_dm_result_t  GetRecordDurationAt(const rocprofvis_dm_property_index_t index, rocprofvis_dm_duration_t & duration);
        virtual rocprofvis_dm_result_t  GetRecordCategoryIndexAt(const rocprofvis_dm_property_index_t index, rocprofvis_dm_index_t & category_index);
        virtual rocprofvis_dm_result_t  GetRecordSymbolIndexAt(const rocprofvis_dm_property_index_t index, rocprofvis_dm_index_t & symbol_index);
        virtual rocprofvis_dm_result_t  GetRecordCategoryStringAt(const rocprofvis_dm_property_index_t index, rocprofvis_dm_charptr_t & category_charptr);
        virtual rocprofvis_dm_result_t  GetRecordSymbolStringAt(const rocprofvis_dm_property_index_t index, rocprofvis_dm_charptr_t & symbol_charptr);

        rocprofvis_dm_result_t          GetPropertyAsUint64(rocprofvis_dm_property_t property, 
                                                            rocprofvis_dm_property_index_t index, 
                                                            uint64_t* value) override;
        rocprofvis_dm_result_t          GetPropertyAsInt64(rocprofvis_dm_property_t property, 
                                                            rocprofvis_dm_property_index_t index, 
                                                            int64_t* value) override;
        rocprofvis_dm_result_t          GetPropertyAsCharPtr(rocprofvis_dm_property_t property, 
                                                            rocprofvis_dm_property_index_t index, 
                                                            char** value) override;
        rocprofvis_dm_result_t          GetPropertyAsDouble(rocprofvis_dm_property_t property, 
                                                            rocprofvis_dm_property_index_t index, 
                                                            double* value) override;
#ifdef TEST
        const char*                     GetPropertySymbol(rocprofvis_dm_property_t property) override;
#endif

    private:

        rocprofvis_dm_timestamp_t                    m_start_timestamp;
        rocprofvis_dm_timestamp_t                    m_end_timestamp;
        RpvDmTrack*                                  m_ctx;
};

#endif //RPV_DATAMODEL_TRACK_SLICE_H