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

/*
* This header include internal data model types 
*/

#include "rocprofvis_c_interface_types.h"
#include "rocprofvis_error_handling.h"
#include <algorithm>
#include <vector>

/*******************************Types******************************/


typedef uint32_t                          rocprofvis_dm_node_id_t;
typedef uint64_t                          rocprofvis_dm_process_id_t;
typedef uint64_t                          rocprofvis_dm_stream_id_t;
typedef std::string                       rocprofvis_dm_string_t;
typedef uint32_t                          rocprofvis_dm_op_t;
typedef int64_t                           rocprofvis_dm_duration_t;
typedef uint64_t                          rocprofvis_dm_id_t;
typedef double                            rocprofvis_dm_value_t;
typedef uint64_t                          rocprofvis_db_timeout_ms_t;                    // asynchronous call wait timeout (milliseconds)                           


/*******************************Structures******************************/

// rocprofvis_db_record_data_t is used to pass record data from database to data model. Used by database query callbacks
typedef union{
    struct event_record_t
    {
        rocprofvis_dm_event_id_t id;                // 60-bit event id and 4-bit operation type
        rocprofvis_dm_timestamp_t timestamp;        // 64-bit timestamp 
        rocprofvis_dm_duration_t duration;          // signed 64-bit duration. Negative number should be invalidated by controller.
        rocprofvis_dm_id_t category;                // 32-bit category index of array of strings 
        rocprofvis_dm_id_t symbol;                  // 32-bit symbol index of array of strings 
    } event;
    struct pmc_record_t
    {
        rocprofvis_dm_timestamp_t timestamp;        // 64-bit timestamp
        rocprofvis_dm_value_t value;                // double precision performance counter value 
    } pmc;
} rocprofvis_db_record_data_t;

// rocprofvis_dm_track_params_t contains track parameters and shared between data model and database. Physically located in database object.
#define NUMBER_OF_TRACK_IDENTIFICATION_PARAMETERS 3
#define TRACK_ID_NODE 0
#define TRACK_ID_PID 1
#define TRACK_ID_AGENT 1
#define TRACK_ID_PID_OR_AGENT 1
#define TRACK_ID_TID 2
#define TRACK_ID_QUEUE 2
#define TRACK_ID_TID_OR_QUEUE 2
#define TRACK_ID_COUNTER 3
#define TRACK_ID_CATEGORY 3
typedef struct {
    // 32-bit track id
    rocprofvis_dm_track_id_t track_id;   
    // 32-bit process IDs
    rocprofvis_dm_process_id_t process_id[NUMBER_OF_TRACK_IDENTIFICATION_PARAMETERS];  
    // database column name for process id
    rocprofvis_dm_string_t process_tag[NUMBER_OF_TRACK_IDENTIFICATION_PARAMETERS]; 
    // process name string
    rocprofvis_dm_string_t process_name[NUMBER_OF_TRACK_IDENTIFICATION_PARAMETERS];
    // is identifier numeric or string
    bool process_id_numeric[NUMBER_OF_TRACK_IDENTIFICATION_PARAMETERS];
    // SQL query to get data for this track, may have multiple sub-queries
    std::vector<rocprofvis_dm_string_t> query;  
    // SQL query to get table data for this track, may have multiple sub-queries
    std::vector<rocprofvis_dm_string_t> table_query;  
    // track category enumeration (PMC, Region, Kernel, SQQT, NIC, etc)
    rocprofvis_dm_track_category_t track_category;   
    // handle of extended data object  
    rocprofvis_dm_extdata_t extdata;  
    // total number of records in track
    uint64_t record_count;
    // minimum timestamp
    rocprofvis_dm_timestamp_t min_ts;
    // maximum timestamp
    rocprofvis_dm_timestamp_t max_ts;
} rocprofvis_dm_track_params_t;

// rocprofvis_dm_trace_params_t contains trace parameters and shared between data model and database. Physically located in trace object and referenced by a pointer in binding structure.
typedef struct {
    rocprofvis_dm_timestamp_t start_time;           // trace start time
    rocprofvis_dm_timestamp_t end_time;             // trace end time
    bool metadata_loaded;                           // status of metadata being fully loaded
} rocprofvis_dm_trace_params_t;

// rocprofvis_db_flow_data_t is used to pass record data from database to data model. Used by database query callbacks
typedef struct {
    rocprofvis_dm_event_id_t id;                    // 60-bit endpoint event id and 4-bit operation type               
    rocprofvis_dm_timestamp_t time;                 // 64-bit endpoint timestamp
    rocprofvis_dm_track_id_t track_id;              // endpoint track index
}rocprofvis_db_flow_data_t;

// rocprofvis_db_flow_data_t is used to pass record data from database to data model. Used by database query callbacks
typedef struct {
    rocprofvis_dm_charptr_t symbol;                 // stacktrace call symbol
    rocprofvis_dm_charptr_t args;                   // stacktrace call arguments
    rocprofvis_dm_charptr_t line;                   // stacktrace code line
    uint32_t depth;                                 // stacktrace depth
}rocprofvis_db_stack_data_t;

// rocprofvis_db_flow_data_t is used to pass record data from database to data model. Used by database query callbacks
typedef struct {
    rocprofvis_dm_charptr_t category;               // extended data category
    rocprofvis_dm_charptr_t name;                   // extended data name
    rocprofvis_dm_charptr_t data;                   // extended data value
}rocprofvis_db_ext_data_t;

/***********************Trace to Database binding info******************************/


typedef rocprofvis_dm_result_t (*rocprofvis_dm_add_track_func_t) (const rocprofvis_dm_trace_t object, rocprofvis_dm_track_params_t * params);
typedef rocprofvis_dm_slice_t (*rocprofvis_dm_add_slice_func_t) (const rocprofvis_dm_trace_t object, const rocprofvis_dm_track_id_t track_id, 
                                                                    const rocprofvis_dm_timestamp_t start, const rocprofvis_dm_timestamp_t end);
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
typedef rocprofvis_dm_result_t (*rocprofvis_db_find_cached_table_value_func_t) (const rocprofvis_dm_database_t object, rocprofvis_dm_charptr_t table, 
                                                                                const rocprofvis_dm_id_t id, rocprofvis_dm_charptr_t column, rocprofvis_dm_charptr_t* value);
typedef rocprofvis_dm_result_t (*rocprofvis_dm_add_event_level_func_t) (const rocprofvis_dm_trace_t object, rocprofvis_dm_event_id_t event_id, uint8_t level);

typedef rocprofvis_dm_result_t (*rocprofvis_dm_check_slice_exists_t) (const rocprofvis_dm_trace_t object, 
                                                                    const rocprofvis_dm_timestamp_t start, const rocprofvis_dm_timestamp_t end);
typedef rocprofvis_dm_result_t (*rocprofvis_dm_check_event_property_exists_t) (const rocprofvis_dm_trace_t object, 
                                                                    rocprofvis_dm_event_property_type_t type, const rocprofvis_dm_event_id_t event_id);
typedef rocprofvis_dm_result_t (*rocprofvis_dm_check_table_exists_t) (const rocprofvis_dm_trace_t object,  const rocprofvis_dm_table_id_t table_id);

typedef struct 
{
        rocprofvis_dm_trace_t trace_object;                             // trace handle
        rocprofvis_dm_trace_params_t * trace_properties;                // pointer to trace parameters structure located in Trace object
        rocprofvis_dm_add_track_func_t FuncAddTrack;                    // Called by database query callback to add track item to a list of tracks located in trace object 
        rocprofvis_dm_add_slice_func_t FuncAddSlice;                    // Called by database query to add a time slice to a track object 
        rocprofvis_dm_add_record_func_t FuncAddRecord;                  // Called by database query callback to add a record to time slice
        rocprofvis_dm_add_string_func_t FuncAddString;                  // Called by database query callback to add string to a list of strings located in trace object
        rocprofvis_dm_add_flowtrace_func_t FuncAddFlowTrace;            // Called by database query to add a flow trace object to a list located in trace object
        rocprofvis_dm_add_flow_func_t FuncAddFlow;                      // Called by database query callback to add flow record to flow trace object
        rocprofvis_dm_add_stacktrace_func_t FuncAddStackTrace;          // Called by database query to add a stack trace object to a list located in trace object
        rocprofvis_dm_add_stack_frame_func_t FuncAddStackFrame;         // Called by database query callback to add stack frame record to stack trace object
        rocprofvis_dm_add_extdata_func_t FuncAddExtData;                // Called by database query to add a extended data object to a list located in trace object
        rocprofvis_dm_add_extdata_record_func_t FuncAddExtDataRecord;   // Called by database query callback to add ext data record to ext data object
        rocprofvis_dm_add_table_func_t FuncAddTable;                    // Called by database query to add a table object to a list located in trace object
        rocprofvis_dm_add_table_row_func_t FuncAddTableRow;             // Called by database query callback to add new row to a table object
        rocprofvis_dm_add_table_column_func_t FuncAddTableColumn;       // Called by database query callback to add new column name to a table object
        rocprofvis_dm_add_table_row_cell_func_t FuncAddTableRowCell;    // Called by database query callback to add new cell to a table row
        rocprofvis_db_find_cached_table_value_func_t FuncFindCachedTableValue; // Get value from tables cached in database component (tables like rocpd_node, rocpd_process, rocpd_thread, rocpd_agent, rocpd_queue, rocpd_stream, etc. )
        rocprofvis_dm_add_event_level_func_t FuncAddEventLevel;         // Called by database query callback to add event level to a map array located in trace object
        rocprofvis_dm_check_slice_exists_t FuncCheckSliceExists;        // Called by database async interface before quering a slice with the same parameters
        rocprofvis_dm_check_event_property_exists_t FuncCheckEventPropertyExists;        // Called by database async interface before quering an event property with the same parameters
        rocprofvis_dm_check_table_exists_t FuncCheckTableExists;        // Called by database async interface before quering a table with the same parameters
} rocprofvis_dm_db_bind_struct;
