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

#ifndef RPV_DATAMODEL_TYPES_H
#define RPV_DATAMODEL_TYPES_H

#include "InterfaceTypes.h"
#include "ErrorHandling.h"

/*******************************Types******************************/


typedef size_t                            rocprofvis_dm_size_t;
typedef uint32_t                          rocprofvis_dm_node_id_t;
typedef uint32_t                          rocprofvis_dm_pid_t;
typedef uint32_t                          rocprofvis_dm_tid_t;
typedef uint32_t                          rocprofvis_dm_string_index_t;
typedef std::string                       rocprofvis_dm_string_t;
typedef uint32_t                          rocprofvis_dm_op_t;
typedef int64_t                           rocprofvis_dm_duration_t;
typedef uint64_t                          rocprofvis_dm_id_t;
typedef double                            rocprofvis_dm_value_t;


/*******************************Structures******************************/

typedef union{
    struct event_record_t
    {
        roprofvis_dm_event_operation_t op;
        rocprofvis_dm_id_t id;
        rocprofvis_dm_timestamp_t timestamp;
        rocprofvis_dm_duration_t duration;
        rocprofvis_dm_string_index_t category;
        rocprofvis_dm_string_index_t symbol;
    } event;
    struct pmc_record_t
    {
        rocprofvis_dm_timestamp_t timestamp;
        rocprofvis_dm_value_t value;
    } pmc;
} rocprofvis_db_record_data_t;

typedef struct {
    rocprofvis_dm_track_id_t track_id;
    rocprofvis_dm_node_id_t node_id;
    rocprofvis_dm_string_t process;
    rocprofvis_dm_string_t name;
    roprofvis_dm_track_category_t track_category;
    rocprofvis_dm_extdata_t extdata;
} rocprofvis_dm_track_params_t;

typedef struct {
    rocprofvis_dm_timestamp_t start_time;
    rocprofvis_dm_timestamp_t end_time;
    rocprofvis_dm_string_index_t strings_offset;
    rocprofvis_dm_string_index_t symbols_offset;
    bool metadata_loaded;
} rocprofvis_dm_trace_params_t;

typedef struct {
    rocprofvis_dm_event_id_t id;
    rocprofvis_dm_timestamp_t time;
    rocprofvis_dm_track_id_t track_id;
}rocprofvis_db_flow_data_t;

typedef struct {
    rocprofvis_dm_charptr_t symbol;
    rocprofvis_dm_charptr_t args;
    rocprofvis_dm_charptr_t line;
    uint32_t depth;
}rocprofvis_db_stack_data_t;

typedef struct {
    rocprofvis_dm_charptr_t category;
    rocprofvis_dm_charptr_t name;
    rocprofvis_dm_charptr_t data;
}rocprofvis_db_ext_data_t;

/*******************************Callbacks******************************/


typedef rocprofvis_dm_result_t (*rocprofvis_dm_add_track_func_t) (const rocprofvis_dm_trace_t object, rocprofvis_dm_track_params_t * params);
typedef rocprofvis_dm_slice_t (*rocprofvis_dm_add_slice_func_t) (const rocprofvis_dm_trace_t object, const rocprofvis_dm_track_id_t track_id, const rocprofvis_dm_timestamp_t start, const rocprofvis_dm_timestamp_t end);
typedef rocprofvis_dm_result_t (*rocprofvis_dm_add_record_func_t) (const rocprofvis_dm_slice_t object, rocprofvis_db_record_data_t& data);
typedef rocprofvis_dm_index_t (*rocprofvis_dm_add_string_func_t) (const rocprofvis_dm_trace_t object, const char* stringValue);
typedef rocprofvis_dm_result_t (*rocprofvis_dm_add_flow_func_t) (const rocprofvis_dm_slice_t object, rocprofvis_db_flow_data_t& data);
typedef rocprofvis_dm_result_t (*rocprofvis_dm_add_stack_frame_func_t) (const rocprofvis_dm_stacktrace_t object, rocprofvis_db_stack_data_t& data);
typedef rocprofvis_dm_flowtrace_t (*rocprofvis_dm_add_flowtrace_func_t) (const rocprofvis_dm_trace_t object, rocprofvis_dm_event_id_t event_id);
typedef rocprofvis_dm_stacktrace_t (*rocprofvis_dm_add_stacktrace_func_t) (const rocprofvis_dm_trace_t object, rocprofvis_dm_event_id_t event_id);
typedef rocprofvis_dm_extdata_t (*rocprofvis_dm_add_extdata_func_t) (const rocprofvis_dm_trace_t object, rocprofvis_dm_event_id_t event_id);
typedef rocprofvis_dm_result_t (*rocprofvis_dm_add_extdata_record_func_t) (const rocprofvis_dm_extdata_t object, rocprofvis_db_ext_data_t& data);
typedef rocprofvis_dm_table_t (*rocprofvis_dm_add_table_func_t) (const rocprofvis_dm_trace_t object, rocprofvis_dm_charptr_t query, rocprofvis_dm_charptr_t description);
typedef rocprofvis_dm_table_row_t (*rocprofvis_dm_add_table_row_func_t) (const rocprofvis_dm_table_t object);
typedef rocprofvis_dm_result_t (*rocprofvis_dm_add_table_column_func_t) (const rocprofvis_dm_table_t object, rocprofvis_dm_charptr_t column_name);
typedef rocprofvis_dm_result_t (*rocprofvis_dm_add_table_row_cell_func_t) (const rocprofvis_dm_table_t object, rocprofvis_dm_charptr_t cell_value);

typedef struct 
{
        rocprofvis_dm_trace_t trace_object;
        rocprofvis_dm_trace_params_t * trace_parameters;
        rocprofvis_dm_add_track_func_t FuncAddTrack;
        rocprofvis_dm_add_slice_func_t FuncAddSlice;
        rocprofvis_dm_add_record_func_t FuncAddRecord;
        rocprofvis_dm_add_string_func_t FuncAddString;
        rocprofvis_dm_add_flowtrace_func_t FuncAddFlowTrace;
        rocprofvis_dm_add_flow_func_t FuncAddFlow;
        rocprofvis_dm_add_stacktrace_func_t FuncAddStackTrace;
        rocprofvis_dm_add_stack_frame_func_t FuncAddStackFrame;
        rocprofvis_dm_add_extdata_func_t FuncAddExtData;
        rocprofvis_dm_add_extdata_record_func_t FuncAddExtDataRecord;
        rocprofvis_dm_add_table_func_t FuncAddTable;
        rocprofvis_dm_add_table_row_func_t FuncAddTableRow;
        rocprofvis_dm_add_table_column_func_t FuncAddTableColumn;
        rocprofvis_dm_add_table_row_cell_func_t FuncAddTableRowCell;

} rocprofvis_dm_db_bind_struct;




#endif //RPV_DATAMODEL_TYPES_H