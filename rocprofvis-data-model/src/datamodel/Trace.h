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

#ifndef RPV_DATAMODEL_TRACE_H
#define RPV_DATAMODEL_TRACE_H

#include "FlowTrace.h"
#include "StackTrace.h"
#include "TrackSlice.h"
#include "ExtData.h"
#include "Track.h"
#include "Table.h"
#include <vector> 
#include <memory> 

class RpvDatabase;


class RpvDmTrace : public RpvObject{
    public:
        RpvDmTrace();
        ~RpvDmTrace(){}
        rocprofvis_dm_timestamp_t                       StartTime() {return m_parameters.start_time;}
        rocprofvis_dm_timestamp_t                       EndTime() {return m_parameters.end_time;}
        rocprofvis_dm_size_t                            NumberOfTracks() {return m_tracks.size();}
        rocprofvis_dm_size_t                            NumberOfTables() {return m_tables.size();}
        rocprofvis_dm_database_t                        Database() { return m_db; }

        rocprofvis_dm_result_t                          BindDatabase(rocprofvis_dm_database_t db, rocprofvis_dm_db_bind_struct & bind_data);
        rocprofvis_dm_result_t                          DeleteSliceAtTimeRange(rocprofvis_dm_timestamp_t start, rocprofvis_dm_timestamp_t end);
        rocprofvis_dm_result_t                          DeleteAllSlices();
        rocprofvis_dm_result_t                          DeleteEventPropertyFor(rocprofvis_dm_event_property_type_t type, rocprofvis_dm_event_id_t event_id);
        rocprofvis_dm_result_t                          DeleteAllEventPropertiesFor(rocprofvis_dm_event_property_type_t type);
        rocprofvis_dm_result_t                          DeleteTableAt(rocprofvis_dm_index_t index);
        rocprofvis_dm_result_t                          DeleteAllTables();


        rocprofvis_dm_charptr_t                         GetStringAt(rocprofvis_dm_index_t index);
        rocprofvis_dm_charptr_t                         GetSymbolAt(rocprofvis_dm_index_t index);

        rocprofvis_dm_result_t                          GetPropertyAsUint64(rocprofvis_dm_property_t property, rocprofvis_dm_property_index_t index, uint64_t* value) override;
        rocprofvis_dm_result_t                          GetPropertyAsHandle(rocprofvis_dm_property_t property, rocprofvis_dm_property_index_t index, rocprofvis_dm_handle_t* value) override;
#ifdef TEST
        const char*                                     GetPropertySymbol(rocprofvis_dm_property_t property) override;
#endif

    private:

        rocprofvis_dm_result_t                          GetTrackAtIndex(rocprofvis_dm_property_index_t index, rocprofvis_dm_track_t & track);
        rocprofvis_dm_size_t                            GetMemoryFootprint();
        rocprofvis_dm_result_t                          GetFlowTraceHandle(rocprofvis_dm_event_id_t event_id, rocprofvis_dm_flowtrace_t & flowtrace);
        rocprofvis_dm_result_t                          GetStackTraceHandle(rocprofvis_dm_event_id_t event_id, rocprofvis_dm_stacktrace_t & stacktrace);
        rocprofvis_dm_result_t                          GetExtInfoHandle(rocprofvis_dm_event_id_t event_id, rocprofvis_dm_extdata_t & extinfo);
        rocprofvis_dm_result_t                          GetTableHandle(rocprofvis_dm_property_index_t event_id, rocprofvis_dm_table_t & table);
        
        static rocprofvis_dm_result_t                   AddTrack(const rocprofvis_dm_trace_t object, rocprofvis_dm_track_params_t * params);
        static rocprofvis_dm_slice_t                    AddSlice(const rocprofvis_dm_trace_t object, const rocprofvis_dm_track_id_t track_id, const rocprofvis_dm_timestamp_t start, const rocprofvis_dm_timestamp_t end);  
        static rocprofvis_dm_result_t                   AddRecord(const rocprofvis_dm_slice_t object, rocprofvis_db_record_data_t & data);      
        static rocprofvis_dm_index_t                    AddString(const rocprofvis_dm_trace_t object,  const char* stringValue);
        static rocprofvis_dm_flowtrace_t                AddFlowTrace(const rocprofvis_dm_trace_t object, const rocprofvis_dm_event_id_t event_id);
        static rocprofvis_dm_result_t                   AddFlow(const rocprofvis_dm_flowtrace_t object, rocprofvis_db_flow_data_t & data);
        static rocprofvis_dm_stacktrace_t               AddStackTrace(const rocprofvis_dm_trace_t object, const rocprofvis_dm_event_id_t event_id);
        static rocprofvis_dm_result_t                   AddStackFrame(const rocprofvis_dm_stacktrace_t object, rocprofvis_db_stack_data_t & data);
        static rocprofvis_dm_flowtrace_t                AddExtData(const rocprofvis_dm_trace_t object, const rocprofvis_dm_event_id_t event_id);
        static rocprofvis_dm_result_t                   AddExtDataRecord(const rocprofvis_dm_extdata_t object, rocprofvis_db_ext_data_t & data);
        static rocprofvis_dm_table_t                    AddTable(const rocprofvis_dm_trace_t object, rocprofvis_dm_charptr_t query, rocprofvis_dm_charptr_t description);
        static rocprofvis_dm_table_row_t                AddTableRow(const rocprofvis_dm_table_t object);
        static rocprofvis_dm_result_t                   AddTableColumn(const rocprofvis_dm_table_t object, rocprofvis_dm_charptr_t column_name);
        static rocprofvis_dm_result_t                   AddTableRowCell(const rocprofvis_dm_table_row_t object, rocprofvis_dm_charptr_t cell_value);

        rocprofvis_dm_trace_params_t                    m_parameters;
        rocprofvis_dm_database_t                        m_db;
        std::vector<std::unique_ptr<RpvDmTrack>>        m_tracks;
        std::vector<std::unique_ptr<RpvDmFlowTrace>>    m_flow_traces;
        std::vector<std::unique_ptr<RpvDmStackTrace>>   m_stack_traces;
        std::vector<std::unique_ptr<RpvDmExtData>>      m_ext_data;
        std::vector<std::unique_ptr<RpvDmTable>>        m_tables;
        std::vector<std::string>                        m_strings;

};

#endif //RPV_DATAMODEL_TRACE_H

