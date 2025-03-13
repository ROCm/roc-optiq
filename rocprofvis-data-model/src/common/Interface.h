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

#ifndef RPV_DATAMODEL_INTERFACE_H
#define RPV_DATAMODEL_INTERFACE_H

#include "InterfaceTypes.h"

#ifdef CPPBUILD
extern "C"
{
#endif
/******************************Database interface methods*****************************/
rocprofvis_dm_database_t rocprofvis_db_open_database(
                                    rocprofvis_db_filename_t, 
                                    rocprofvis_db_type_t);

rocprofvis_db_future_t rocprofvis_db_future_alloc(
                                    rocprofvis_db_progress_callback_t callback = nullptr);

rocprofvis_dm_result_t rocprofvis_db_future_wait(
                                    rocprofvis_db_future_t, 
                                    rocprofvis_db_timeout_ms_t);

void rocprofvis_db_future_free(rocprofvis_db_future_t);

rocprofvis_dm_result_t rocprofvis_db_read_metadata_async(
                                    rocprofvis_dm_database_t, 
                                    rocprofvis_db_future_t);

rocprofvis_dm_result_t rocprofvis_db_read_trace_slice_async( 
                                    rocprofvis_dm_database_t,
                                    rocprofvis_dm_timestamp_t,
                                    rocprofvis_dm_timestamp_t,
                                    rocprofvis_db_num_of_tracks_t,
                                    rocprofvis_db_track_selection_t,
                                    rocprofvis_db_future_t);                              

rocprofvis_dm_result_t  rocprofvis_db_read_event_property_async(
                                    rocprofvis_dm_database_t,
                                    rocprofvis_dm_event_property_type_t,
                                    rocprofvis_dm_event_id_t,
                                    rocprofvis_db_future_t);

rocprofvis_dm_result_t  rocprofvis_db_execute_query_async(
                                    rocprofvis_dm_database_t,                                                                
                                    rocprofvis_dm_charptr_t,
                                    rocprofvis_dm_charptr_t,
                                    rocprofvis_db_future_t);

/***************************Data model trace interface*****************************/

rocprofvis_dm_trace_t   rocprofvis_dm_create_trace(void); 

rocprofvis_dm_result_t  rocprofvis_dm_delete_trace( 
                                    rocprofvis_dm_trace_t);   

rocprofvis_dm_result_t  rocprofvis_dm_bind_trace_to_database( 
                                    rocprofvis_dm_trace_t,
                                    rocprofvis_dm_database_t);                                      

rocprofvis_dm_result_t  rocprofvis_dm_delete_time_slice( 
                                    rocprofvis_dm_trace_t,
                                    rocprofvis_dm_timestamp_t,
                                    rocprofvis_dm_timestamp_t);     

rocprofvis_dm_result_t  rocprofvis_dm_delete_all_time_slices( 
                                    rocprofvis_dm_trace_t);                                      

rocprofvis_dm_result_t  rocprofvis_dm_delete_event_property_for( 
                                    rocprofvis_dm_trace_t,
                                    rocprofvis_dm_event_property_type_t,
                                    rocprofvis_dm_event_id_t);     

rocprofvis_dm_result_t  rocprofvis_dm_delete_all_event_properties_for( 
                                    rocprofvis_dm_trace_t,
                                    rocprofvis_dm_event_property_type_t);        

rocprofvis_dm_result_t  rocprofvis_dm_delete_table_at( 
                                    rocprofvis_dm_trace_t,
                                    rocprofvis_dm_index_t); 

rocprofvis_dm_result_t  rocprofvis_dm_delete_all_tables( 
                                    rocprofvis_dm_trace_t);  
/***************************Data model universal getters*****************************/


uint64_t  rocprofvis_dm_get_property_as_uint64(
                                    rocprofvis_dm_handle_t, 	
                                    rocprofvis_dm_property_t,
                                    rocprofvis_dm_property_index_t);                                      

int64_t   rocprofvis_dm_get_property_as_int64(
                                    rocprofvis_dm_handle_t, 	
                                    rocprofvis_dm_property_t,
                                    rocprofvis_dm_property_index_t); 

double  rocprofvis_dm_get_property_as_double(
                                    rocprofvis_dm_handle_t, 	
                                    rocprofvis_dm_property_t,
                                    rocprofvis_dm_property_index_t); 

char*   rocprofvis_dm_get_property_as_charptr(
                                    rocprofvis_dm_handle_t, 	
                                    rocprofvis_dm_property_t,
                                    rocprofvis_dm_property_index_t); 

rocprofvis_dm_handle_t  rocprofvis_dm_get_property_as_handle(
                                    rocprofvis_dm_handle_t, 	
                                    rocprofvis_dm_property_t,
                                    rocprofvis_dm_property_index_t); 
#ifdef CPPBUILD
}
#endif

rocprofvis_dm_result_t  rocprofvis_dm_get_property_as_uint64(
                                    rocprofvis_dm_handle_t, 	
                                    rocprofvis_dm_property_t,
                                    rocprofvis_dm_property_index_t,
                                    uint64_t*);                                      

rocprofvis_dm_result_t  rocprofvis_dm_get_property_as_int64(
                                    rocprofvis_dm_handle_t, 	
                                    rocprofvis_dm_property_t,
                                    rocprofvis_dm_property_index_t,
                                    int64_t*); 

rocprofvis_dm_result_t  rocprofvis_dm_get_property_as_double(
                                    rocprofvis_dm_handle_t, 	
                                    rocprofvis_dm_property_t,
                                    rocprofvis_dm_property_index_t,
                                    double*); 

rocprofvis_dm_result_t  rocprofvis_dm_get_property_as_charptr(
                                    rocprofvis_dm_handle_t, 	
                                    rocprofvis_dm_property_t,
                                    rocprofvis_dm_property_index_t,
                                    char**); 

rocprofvis_dm_result_t  rocprofvis_dm_get_property_as_handle(
                                    rocprofvis_dm_handle_t, 	
                                    rocprofvis_dm_property_t,
                                    rocprofvis_dm_property_index_t,
                                    rocprofvis_dm_handle_t*); 
                           
#endif //RPV_DATAMODEL_INTERFACE_H