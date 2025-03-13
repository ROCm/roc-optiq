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

#include "Trace.h"
#include "Track.h"
#include "TrackSlice.h"
#include "FlowTrace.h"
#include "StackTrace.h"
#include "ExtData.h"
#include "Table.h"
#include "TableRow.h"

RpvDmTrace::RpvDmTrace()
{
    m_db = nullptr;
    m_parameters.start_time=0;
    m_parameters.end_time=0;
    m_parameters.strings_offset=0;
    m_parameters.symbols_offset=0;
    m_parameters.metadata_loaded=false;
}

rocprofvis_dm_result_t RpvDmTrace::BindDatabase(rocprofvis_dm_database_t db, rocprofvis_dm_db_bind_struct & bind_data)
{
    ASSERT_MSG_RETURN(db, ERROR_DATABASE_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
     
    bind_data.trace_object = this;
    bind_data.trace_parameters = &m_parameters;
    bind_data.FuncAddTrack = AddTrack;
    bind_data.FuncAddRecord = AddRecord;
    bind_data.FuncAddSlice = AddSlice;
    bind_data.FuncAddString = AddString;
    bind_data.FuncAddFlowTrace = AddFlowTrace;
    bind_data.FuncAddFlow = AddFlow;
    bind_data.FuncAddStackTrace = AddStackTrace;
    bind_data.FuncAddStackFrame = AddStackFrame;
    bind_data.FuncAddExtData = AddExtData;
    bind_data.FuncAddExtDataRecord = AddExtDataRecord;
    bind_data.FuncAddTable = AddTable;
    bind_data.FuncAddTableRow = AddTableRow;
    bind_data.FuncAddTableColumn = AddTableColumn;
    bind_data.FuncAddTableRowCell = AddTableRowCell;
    m_db = db;
    return kRocProfVisDmResultSuccess;
}
    
rocprofvis_dm_result_t RpvDmTrace::DeleteSliceAtTimeRange(rocprofvis_dm_timestamp_t start, rocprofvis_dm_timestamp_t end){

    for (int i=0; i < m_tracks.size(); i++)
    {
        rocprofvis_dm_slice_t slice = nullptr;
        rocprofvis_dm_result_t result = m_tracks[i].get()->GetSliceAtTime(start, slice);
        if (result == kRocProfVisDmResultSuccess)
        {
            ASSERT_MSG_RETURN(slice, ERROR_SLICE_CANNOT_BE_NULL, kRocProfVisDmResultUnknownError);
            if (kRocProfVisDmResultSuccess != m_tracks[i].get()->DeleteSlice(slice)) return kRocProfVisDmResultUnknownError;
        }
        else
            return result;
    }
    return kRocProfVisDmResultSuccess;
}

rocprofvis_dm_result_t RpvDmTrace::DeleteAllSlices(){
    for (int i=0; i < m_tracks.size(); i++)
    {
        size_t num_slices = m_tracks[i].get()->NumberOfSlices();
        for (int j=0; j < num_slices; j++)
        {
            rocprofvis_dm_slice_t slice = nullptr;
            rocprofvis_dm_result_t result = m_tracks[i].get()->GetSliceAtIndex(j, slice);
            if (result == kRocProfVisDmResultSuccess)
            {
                ASSERT_MSG_RETURN(slice, ERROR_SLICE_CANNOT_BE_NULL, kRocProfVisDmResultUnknownError);
                if (kRocProfVisDmResultSuccess != m_tracks[i].get()->DeleteSlice(slice)) return kRocProfVisDmResultUnknownError;
            }
        }
    }
    return kRocProfVisDmResultSuccess;
}

rocprofvis_dm_result_t  RpvDmTrace::DeleteEventPropertyFor(     rocprofvis_dm_event_property_type_t type,
                                                                rocprofvis_dm_event_id_t event_id) {
    switch (type)
    {
        case kRPVDMEventFlowTrace:
        {
            size_t num = m_flow_traces.size();
            for (size_t i=0; i < num; i++)
            {
                if (m_flow_traces[i].get()->EventId().value == event_id.value) {
                    m_flow_traces.erase(m_flow_traces.begin()+i);
                    return kRocProfVisDmResultSuccess;
                }
            }
            return kRocProfVisDmResultNotLoaded;
        }
        break;
        case kRPVDMEventStackTrace:
        {
            size_t num = m_stack_traces.size();
            for (size_t i=0; i < num; i++)
            {
                if (m_stack_traces[i].get()->EventId().value == event_id.value) {
                    m_stack_traces.erase(m_stack_traces.begin()+i);
                    return kRocProfVisDmResultSuccess;
                }
            }
            return kRocProfVisDmResultNotLoaded;
        }
        break;
        case kRPVDMEventExtData:
        {
            size_t num = m_ext_data.size();
            for (size_t i=0; i < num; i++)
            {
                if (m_ext_data[i].get()->EventId().value == event_id.value) {
                    m_ext_data.erase(m_ext_data.begin()+i);
                    return kRocProfVisDmResultSuccess;
                }
            }
            return kRocProfVisDmResultNotLoaded;
        }  
        break;   
    }
    ASSERT_ALWAYS_MSG_RETURN(ERROR_UNSUPPORTED_PROPERTY, kRocProfVisDmResultNotSupported); 
}

rocprofvis_dm_result_t  RpvDmTrace::DeleteAllEventPropertiesFor(rocprofvis_dm_event_property_type_t type){
    switch (type)
    {
        case kRPVDMEventFlowTrace:
            m_flow_traces.clear();
            return kRocProfVisDmResultSuccess;
        case kRPVDMEventStackTrace:
            m_stack_traces.clear();
            return kRocProfVisDmResultSuccess;
        case kRPVDMEventExtData:
            m_ext_data.clear();
            return kRocProfVisDmResultSuccess;
    }
    ASSERT_ALWAYS_MSG_RETURN(ERROR_UNSUPPORTED_PROPERTY, kRocProfVisDmResultNotSupported); 
}

rocprofvis_dm_result_t RpvDmTrace::DeleteTableAt(rocprofvis_dm_index_t index){
    ASSERT_MSG_RETURN(index < m_tables.size(), ERROR_INDEX_OUT_OF_RANGE, kRocProfVisDmResultInvalidParameter);
    m_tables.erase(m_tables.begin()+index);
    return kRocProfVisDmResultSuccess;
}

rocprofvis_dm_result_t RpvDmTrace::DeleteAllTables(){
    m_tables.clear();
    return kRocProfVisDmResultSuccess;
}

rocprofvis_dm_result_t RpvDmTrace::GetTrackAtIndex(rocprofvis_dm_property_index_t index, rocprofvis_dm_track_t & value) {
    ASSERT_MSG_RETURN(index < m_tracks.size(), ERROR_INDEX_OUT_OF_RANGE, kRocProfVisDmResultInvalidParameter);
    value = m_tracks[index].get();
    return kRocProfVisDmResultSuccess;
}  

rocprofvis_dm_size_t RpvDmTrace::GetMemoryFootprint(){
    rocprofvis_dm_size_t size = sizeof(RpvDmTrace);
    for (int i=0; i < m_tracks.size(); i++)
    {
        size+=m_tracks[i].get()->GetMemoryFootprint();
    }   
    for (int i=0; i < m_flow_traces.size(); i++)
    {
        size+=m_flow_traces[i].get()->GetMemoryFootprint();
    }
    for (int i=0; i < m_stack_traces.size(); i++)
    {
        size+=m_stack_traces[i].get()->GetMemoryFootprint();
    }
    for (int i=0; i < m_ext_data.size(); i++)
    {
        size+=m_ext_data[i].get()->GetMemoryFootprint();
    }
    for (int i=0; i < m_tables.size(); i++)
    {
        size+=m_tables[i].get()->GetMemoryFootprint();
    }
    for (int i=0; i < m_strings.size(); i++)
    {
        size+=m_strings[i].length()+1;
    }
    return size;
}

rocprofvis_dm_result_t RpvDmTrace::AddTrack(const rocprofvis_dm_trace_t object, rocprofvis_dm_track_params_t * params){
    ASSERT_MSG_RETURN(object, ERROR_TRACE_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
    RpvDmTrace* trace = (RpvDmTrace*)object;
    try{
        trace->m_tracks.push_back(std::make_unique<RpvDmTrack>(trace, params));
    }
    catch(std::exception ex)
    {
        ASSERT_ALWAYS_MSG_RETURN("Error! Failure allocating track", kRocProfVisDmResultAllocFailure);
    }
    return kRocProfVisDmResultSuccess;
}

rocprofvis_dm_slice_t RpvDmTrace::AddSlice(const rocprofvis_dm_trace_t object, const rocprofvis_dm_track_id_t track_id, const rocprofvis_dm_timestamp_t start, const rocprofvis_dm_timestamp_t end){
    ASSERT_MSG_RETURN(object, ERROR_TRACE_CANNOT_BE_NULL, nullptr);
    RpvDmTrace* trace = (RpvDmTrace*)object;
    rocprofvis_dm_track_t track = nullptr;
    rocprofvis_dm_result_t result = trace->GetTrackAtIndex(track_id, track);
    if (result == kRocProfVisDmResultSuccess)
    {
        ASSERT_MSG_RETURN(track, ERROR_TRACK_CANNOT_BE_NULL, nullptr);
        return ((RpvDmTrack*)track)->AddSlice(start, end);
    }
    else
        return nullptr;
}  

rocprofvis_dm_result_t RpvDmTrace::AddRecord(const rocprofvis_dm_slice_t object, rocprofvis_db_record_data_t & data){
    ASSERT_MSG_RETURN(object, ERROR_SLICE_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
    RpvDmTrackSlice* slice = (RpvDmTrackSlice*) object;
    return slice->AddRecord(data);
}      

rocprofvis_dm_index_t RpvDmTrace::AddString(const rocprofvis_dm_trace_t object,  const char* stringValue){
    ASSERT_MSG_RETURN(object, ERROR_TRACE_CANNOT_BE_NULL, INVALID_INDEX);
    RpvDmTrace* trace = (RpvDmTrace*)object;
    rocprofvis_dm_index_t current_index = (rocprofvis_dm_index_t)trace->m_strings.size();
    try{
        trace->m_strings.push_back(stringValue);
    }
    catch(std::exception ex)
    {
        ASSERT_ALWAYS_MSG_RETURN( "Error! Failure allocating string memory", INVALID_INDEX);
    }
    return current_index;
}

rocprofvis_dm_result_t RpvDmTrace::AddFlow(const rocprofvis_dm_flowtrace_t object, rocprofvis_db_flow_data_t & data){
    ASSERT_MSG_RETURN(object, ERROR_FLOW_TRACE_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
    RpvDmFlowTrace* flowtrace = (RpvDmFlowTrace*) object;
    return flowtrace->AddRecord(data);
}

rocprofvis_dm_flowtrace_t RpvDmTrace::AddFlowTrace(const rocprofvis_dm_trace_t object, const rocprofvis_dm_event_id_t event_id){
    ASSERT_MSG_RETURN(object, ERROR_TRACE_CANNOT_BE_NULL, nullptr);
    RpvDmTrace* trace = (RpvDmTrace*)object;
    try{
        trace->m_flow_traces.push_back(std::make_unique<RpvDmFlowTrace>(trace, event_id));
    }
    catch(std::exception ex)
    {
        ASSERT_ALWAYS_MSG_RETURN( "Error! Failure allocating flowtrace object", nullptr);
    }
    return trace->m_flow_traces.back().get();
}  

rocprofvis_dm_result_t RpvDmTrace::AddStackFrame(const rocprofvis_dm_stacktrace_t object, rocprofvis_db_stack_data_t & data){
    ASSERT_MSG_RETURN(object, ERROR_STACK_TRACE_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
    RpvDmStackTrace* stacktrace = (RpvDmStackTrace*) object;
    return stacktrace->AddRecord(data);
}

rocprofvis_dm_stacktrace_t RpvDmTrace::AddStackTrace(const rocprofvis_dm_trace_t object, const rocprofvis_dm_event_id_t event_id){
    ASSERT_MSG_RETURN(object, ERROR_TRACE_CANNOT_BE_NULL, nullptr);
    RpvDmTrace* trace = (RpvDmTrace*)object;
    try{
        trace->m_stack_traces.push_back(std::make_unique<RpvDmStackTrace>(trace, event_id));
    }
    catch(std::exception ex)
    {
        ASSERT_ALWAYS_MSG_RETURN( "Error! Failure allocating stacktrace object", nullptr);
    }
    return trace->m_stack_traces.back().get();
}  

rocprofvis_dm_extdata_t  RpvDmTrace::AddExtData(const rocprofvis_dm_trace_t object, const rocprofvis_dm_event_id_t event_id){
    ASSERT_MSG_RETURN(object, ERROR_TRACE_CANNOT_BE_NULL, nullptr);
    RpvDmTrace* trace = (RpvDmTrace*)object;
    try{
        trace->m_ext_data.push_back(std::make_unique<RpvDmExtData>(trace,event_id));
    }
    catch(std::exception ex)
    {
        ASSERT_ALWAYS_MSG_RETURN( "Error! Failure allocating extended data object", nullptr);
    }
    return trace->m_ext_data.back().get();
}

rocprofvis_dm_result_t  RpvDmTrace::AddExtDataRecord(const rocprofvis_dm_extdata_t object, rocprofvis_db_ext_data_t & data){
    ASSERT_MSG_RETURN(object, ERROR_EXT_DATA_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
    RpvDmExtData* ext_data = (RpvDmExtData*) object;
    if (!ext_data->HasRecord(data)){
        return ext_data->AddRecord(data);   
    }
    return kRocProfVisDmResultSuccess;
}


rocprofvis_dm_table_t RpvDmTrace::AddTable(const rocprofvis_dm_trace_t object, rocprofvis_dm_charptr_t query, rocprofvis_dm_charptr_t description){
    ASSERT_MSG_RETURN(object, ERROR_TRACE_CANNOT_BE_NULL, nullptr);
    RpvDmTrace* trace = (RpvDmTrace*) object;
    try{
        trace->m_tables.push_back(std::make_unique<RpvDmTable>(trace,query,description));
    }
    catch(std::exception ex)
    {
        ASSERT_ALWAYS_MSG_RETURN( "Error! Failure allocating table object", nullptr);
    }
    return trace->m_tables.back().get();
}

rocprofvis_dm_table_row_t RpvDmTrace::AddTableRow(const rocprofvis_dm_table_t object){
    ASSERT_MSG_RETURN(object, ERROR_TABLE_CANNOT_BE_NULL, nullptr);
    RpvDmTable* table = (RpvDmTable*) object;
    return table->AddRow();
}

rocprofvis_dm_result_t RpvDmTrace::AddTableColumn(const rocprofvis_dm_table_t object, rocprofvis_dm_charptr_t column_name){
    ASSERT_MSG_RETURN(object, ERROR_TABLE_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
    RpvDmTable* table = (RpvDmTable*) object;
    return table->AddColumn(column_name);
}

rocprofvis_dm_result_t RpvDmTrace::AddTableRowCell(const rocprofvis_dm_table_row_t object, rocprofvis_dm_charptr_t cell_value){
    ASSERT_MSG_RETURN(object, ERROR_TABLE_ROW_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
    RpvDmTableRow* table_row = (RpvDmTableRow*) object;
    return table_row->AddCellValue(cell_value);
}

rocprofvis_dm_charptr_t RpvDmTrace::GetStringAt(rocprofvis_dm_index_t index){
    ASSERT_MSG_RETURN(m_parameters.metadata_loaded, ERROR_METADATA_IS_NOT_LOADED, nullptr);
    ASSERT_MSG_RETURN((index + m_parameters.strings_offset) < m_strings.size(), ERROR_INDEX_OUT_OF_RANGE, nullptr);
    return m_strings[index + m_parameters.strings_offset].c_str();
}

rocprofvis_dm_charptr_t RpvDmTrace::GetSymbolAt(rocprofvis_dm_index_t index){
    ASSERT_MSG_RETURN(m_parameters.metadata_loaded, ERROR_METADATA_IS_NOT_LOADED, nullptr);
    ASSERT_MSG_RETURN((index + m_parameters.symbols_offset) < m_strings.size(), ERROR_INDEX_OUT_OF_RANGE, nullptr);
    return m_strings[index + m_parameters.symbols_offset].c_str();
}


rocprofvis_dm_result_t  RpvDmTrace::GetPropertyAsUint64(rocprofvis_dm_property_t property, rocprofvis_dm_property_index_t index, uint64_t* value){
    ASSERT_MSG_RETURN(value, ERROR_REFERENCE_POINTER_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
    switch(property)
    {
        case kRPVDMStartTimeUInt64:
            *value = StartTime();
            return kRocProfVisDmResultSuccess;
        case kRPVDMEndTimeUInt64:
            *value = EndTime();
            return kRocProfVisDmResultSuccess;
        case kRPVDMNumberOfTracksUInt64:
            *value = NumberOfTracks();
            return kRocProfVisDmResultSuccess;
        case kRPVDMNumberOfTablesUInt64:
            *value = NumberOfTables();
            return kRocProfVisDmResultSuccess;
        case kRPVDMTraceMemoryFootprintUInt64:
            *value = GetMemoryFootprint();
            return kRocProfVisDmResultSuccess;
        default:
            ASSERT_ALWAYS_MSG_RETURN(ERROR_INVALID_PROPERTY_GETTER, kRocProfVisDmResultInvalidProperty);
    }

}


rocprofvis_dm_result_t    RpvDmTrace::GetPropertyAsHandle(rocprofvis_dm_property_t property, rocprofvis_dm_property_index_t index, rocprofvis_dm_handle_t* value){
    ASSERT_MSG_RETURN(value, ERROR_REFERENCE_POINTER_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
    switch(property)
    {
        case kRPVDMTrackHandleIndexed:
            return GetTrackAtIndex(index, *value);
        case kRPVDMDatabaseHandle:
            *value = Database();
            return kRocProfVisDmResultSuccess;
        case kRPVDMFlowTraceHandleByEventID:
            return GetFlowTraceHandle(*(rocprofvis_dm_event_id_t*)&index, *value);
        case kRPVDMStackTraceHandleByEventID:
            return GetStackTraceHandle(*(rocprofvis_dm_event_id_t*)&index, *value);
        case kRPVDMExtInfoHandleByEventID:
            return GetExtInfoHandle(*(rocprofvis_dm_event_id_t*)&index, *value);
        case kRPVDMTableHandleIndexed:
            return GetTableHandle(index, *value);
        default:
            ASSERT_ALWAYS_MSG_RETURN(ERROR_INVALID_PROPERTY_GETTER, kRocProfVisDmResultInvalidProperty);
    }
}


rocprofvis_dm_result_t RpvDmTrace::GetExtInfoHandle(rocprofvis_dm_event_id_t event_id, rocprofvis_dm_extdata_t & extinfo){
    auto it = find_if(m_ext_data.begin(), m_ext_data.end(), [&](std::unique_ptr<RpvDmExtData>& ft){
        return ft.get()->EventId().value == event_id.value;
    });
    if (it != m_ext_data.end())
    {
        extinfo = it->get();
        return kRocProfVisDmResultSuccess;
    } 
    return kRocProfVisDmResultNotLoaded;
}

rocprofvis_dm_result_t RpvDmTrace::GetFlowTraceHandle(rocprofvis_dm_event_id_t event_id, rocprofvis_dm_flowtrace_t & flowtrace){
    auto it = find_if(m_flow_traces.begin(), m_flow_traces.end(), [&](std::unique_ptr<RpvDmFlowTrace>& ft){
        return ft.get()->EventId().value == event_id.value;
    });
    if (it != m_flow_traces.end())
    {
        flowtrace = it->get();
        return kRocProfVisDmResultSuccess;
    } 
    return kRocProfVisDmResultNotLoaded;
}

rocprofvis_dm_result_t RpvDmTrace::GetStackTraceHandle(rocprofvis_dm_event_id_t event_id, rocprofvis_dm_stacktrace_t & stacktrace){
    auto it = find_if(m_stack_traces.begin(), m_stack_traces.end(), [&](std::unique_ptr<RpvDmStackTrace>& ft) {
        return ft.get()->EventId().value == event_id.value;
        });
    if (it != m_stack_traces.end())
    {
        stacktrace = it->get();
        return kRocProfVisDmResultSuccess;
    }
    return kRocProfVisDmResultNotLoaded;
}

rocprofvis_dm_result_t RpvDmTrace::GetTableHandle(rocprofvis_dm_property_index_t index, rocprofvis_dm_table_t & table){
    ASSERT_MSG_RETURN(index < m_tables.size(), ERROR_INDEX_OUT_OF_RANGE, kRocProfVisDmResultNotLoaded);
    table = m_tables[index].get();
    return kRocProfVisDmResultSuccess;

}