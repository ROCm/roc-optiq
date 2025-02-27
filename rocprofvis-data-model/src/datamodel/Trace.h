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

#ifndef RPV_DATAMODEL_TRACE_H
#define RPV_DATAMODEL_TRACE_H

#include "../common/CommonTypes.h"
#include "RpvObject.h"
#include <vector> 
#include <memory> 

class RpvDatabase;
class RpvDmTrack;

class RpvDmTrace : public RpvObject{
    public:
        RpvDmTrace();
        rocprofvis_dm_timestamp_t                       StartTime() {return m_parameters.start_time;}
        rocprofvis_dm_timestamp_t                       EndTime() {return m_parameters.end_time;}
        rocprofvis_dm_size_t                            NumberOfTracks() {return m_tracks.size();}
        rocprofvis_dm_database_t                        Database() { return m_db; }

        rocprofvis_dm_result_t                          BindDatabase(rocprofvis_dm_database_t db);
        rocprofvis_dm_result_t                          DeleteSliceAtTimeRange(rocprofvis_dm_timestamp_t start, rocprofvis_dm_timestamp_t end);
        rocprofvis_dm_result_t                          DeleteAllSlices();
        rocprofvis_dm_result_t                          GetTrackAtIndex(rocprofvis_dm_index_t index, rocprofvis_dm_track_t & track);
        rocprofvis_dm_size_t                            GetMemoryFootprint();
        rocprofvis_dm_charptr_t                         GetStringAt(rocprofvis_dm_index_t index);
        rocprofvis_dm_charptr_t                         GetSymbolAt(rocprofvis_dm_index_t index);
        rocprofvis_dm_result_t                          GetExtendedTrackInfo(rocprofvis_dm_id_t track_id, rocprofvis_dm_json_blob_t & json);
        rocprofvis_dm_result_t                          GetExtendedEventInfo(rocprofvis_dm_event_id_t event_id, rocprofvis_dm_json_blob_t & json);
        rocprofvis_dm_result_t                          GetFlowTraceHandle(rocprofvis_dm_id_t event_id, rocprofvis_dm_flowtrace_t & flowtrace);
        rocprofvis_dm_result_t                          GetStackTraceHandle(rocprofvis_dm_id_t event_id, rocprofvis_dm_stacktrace_t & stacktrace);

        rocprofvis_dm_result_t                          GetPropertyAsUint64(rocprofvis_dm_trace_property_t property, rocprofvis_dm_property_index_t index, uint64_t* value) override;
        rocprofvis_dm_result_t                          GetPropertyAsInt64(rocprofvis_dm_trace_property_t property, rocprofvis_dm_property_index_t index, int64_t* value) override;
        rocprofvis_dm_result_t                          GetPropertyAsCharPtr(rocprofvis_dm_trace_property_t property, rocprofvis_dm_property_index_t index, char** value) override;
        rocprofvis_dm_result_t                          GetPropertyAsDouble(rocprofvis_dm_trace_property_t property, rocprofvis_dm_property_index_t index, double* value) override;
        rocprofvis_dm_result_t                          GetPropertyAsHandle(rocprofvis_dm_trace_property_t property, rocprofvis_dm_property_index_t index, rocprofvis_dm_handle_t* value) override;

    private:
        
        static rocprofvis_dm_result_t                   AddTrack(const rocprofvis_dm_trace_t object, rocprofvis_dm_track_params_t * params);
        static rocprofvis_dm_slice_t                    AddSlice(const rocprofvis_dm_trace_t object, const rocprofvis_dm_track_id_t track_id, const rocprofvis_dm_timestamp_t start, const rocprofvis_dm_timestamp_t end);  
        static rocprofvis_dm_result_t                   AddRecord(const rocprofvis_dm_slice_t object, rocprofvis_db_record_data_t & data);      
        static rocprofvis_dm_index_t                    AddString(const rocprofvis_dm_trace_t object,  const char* stringValue);

        std::vector<std::unique_ptr<RpvDmTrack>>        m_tracks;
        rocprofvis_dm_trace_params_t                    m_parameters;
        rocprofvis_dm_database_t                        m_db;

        std::vector<std::string>                        m_strings;
        rocprofvis_dm_size_t                            m_strings_mem_footprint;
};

#endif //RPV_DATAMODEL_TRACE_H

