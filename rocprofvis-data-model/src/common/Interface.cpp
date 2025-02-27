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


#include "../database/RocpdDb.h"
#include "../datamodel/Trace.h"

/********************************DATABASE INDERFACE************************************/
rocprofvis_dm_database_t open_database(
                                    rocprofvis_db_filename_t filename, 
                                    rocprofvis_db_type_t db_type, 
                                    rocprofvis_db_read_progress progress_callback){
    if (db_type == rocprofvis_db_type_t::kRocpdSqlite)
    {
        try {
            return new RocpdDatabase(filename, progress_callback);
        }
        catch(std::exception ex)
        {
            ASSERT_ALWAYS_MSG_RETURN(ERROR_MEMORY_ALLOCATION_FAILURE, nullptr);
        }
    } else
    {
        LOG("Database type not yet supported!");
        return nullptr;
    }                                  
}

rocprofvis_db_future_t rocprofvis_db_future_alloc(void){
    try{
        return new Future();
    }
    catch(std::exception ex)
    {
        ASSERT_ALWAYS_MSG_RETURN(ERROR_MEMORY_ALLOCATION_FAILURE, nullptr);
    }
}

rocprofvis_result_t rocprofvis_db_future_wait(
                                    rocprofvis_db_future_t object, 
                                    rocprofvis_db_timeout_ms_t timeout){
    Future * future = (Future*) object;
    return future->WaitForCompletion(timeout);                                 
}

void rocprofvis_db_future_free(rocprofvis_db_future_t object){
    delete (Future*) object;
}

rocprofvis_result_t rocprofvis_db_read_metadata_async(
                                    rocprofvis_dm_database_t database, 
                                    rocprofvis_db_future_t object){
    Database* db = (Database*) database;
    return db->ReadTraceMetadataAsync(object);
}

rocprofvis_result_t rocprofvis_db_read_trace_slice_async( 
                                    rocprofvis_dm_database_t db,
                                    rocprofvis_dm_timestamp_t start,
                                    rocprofvis_dm_timestamp_t end,
                                    rocprofvis_db_num_of_tracks_t num,
                                    rocprofvis_db_track_selection_t tracks,
                                    rocprofvis_db_future_t object){
    Database* db = (Database*) database;
    return db->ReadTraceSliceAsync(start,end,tracks,object);
}

rocprofvis_dm_result_t  rocprofvis_db_read_track_extended_info(
                                    rocprofvis_dm_database_t database,
                                    rocprofvis_dm_index_t track_id){
    Database* db = (Database*) database;
    return db->ReadExtendedTrackInfo(track_id);
}

rocprofvis_dm_result_t  rocprofvis_db_read_event_extended_info(
                                    rocprofvis_dm_database_t database,
                                    rocprofvis_dm_event_id_t event_id){
    Database* db = (Database*) database;
    return db->ReadExtendedEventInfo(event_id);
}

rocprofvis_dm_result_t  rocprofvis_db_read_flow_trace_info_async(
                                    rocprofvis_dm_database_t database,
                                    rocprofvis_dm_event_id_t event_id,
                                    rocprofvis_db_future_t object){
    Database* db = (Database*) database;
    return db->ReadFlowTraceInfoAsync(event_id, object);               
}

rocprofvis_dm_result_t  rocprofvis_db_read_stack_trace_info_async(
                                    rocprofvis_dm_database_t database,
                                    rocprofvis_dm_event_id_t event_id,
                                    rocprofvis_db_future_t object){
    Database* db = (Database*) database;
    return db->ReadStackTraceInfoAsync(event_id, object);
}

/************************************TRACE INTERFACE**************************************/

rocprofvis_dm_trace_t  rocprofvis_dm_create_trace(){
    try{
        return new RpvDmTrace();
    }
    catch(std::exception ex)
    {
        ASSERT_ALWAYS_MSG_RETURN(ERROR_MEMORY_ALLOCATION_FAILURE, nullptr);
    }
}

void rocprofvis_dm_delete_trace(rocprofvis_dm_trace_t trace)
{
    delete trace;
}                                      


rocprofvis_dm_bind_trace_to_database(   rocprofvis_dm_trace_t trace,
                                        rocprofvis_dm_database_t database){
    ASSERT_MSG_RETURN(trace, ERROR_TRACE_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
    return ((RpvDmTrace*)trace)->BindDatabase((Database*)database);
} 

void rocprofvis_dm_delete_time_slice( 
                                        rocprofvis_dm_trace_t trace,
                                        rocprofvis_dm_timestamp_t start,
                                        rocprofvis_dm_timestamp_t end){
    ASSERT_MSG_RETURN(trace, ERROR_TRACE_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
    ((RpvDmTrace*)trace)->DeleteSliceAtTimeRange(start, end);
}   

void rocprofvis_dm_delete_all_time_slices( 
                                        rocprofvis_dm_trace_t trace){
    ASSERT_MSG_RETURN(trace, ERROR_TRACE_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
    ((RpvDmTrace*)trace)->DeleteAllSlices();
}


/*********************************Universal property getters************************************/
/*
** Note, I have switched to returning property data using pointer references
** Return value of each getter function is status of the execution
** This wasn'y tested with CFFI, but there are some samples on the web showing it's possible  
*/

rocprofvis_dm_result_t  rocprofvis_dm_get_property_as_uint64(
                                    rocprofvis_dm_handle_t handle, 	
                                    rocprofvis_dm_property_t property,
                                    rocprofvis_dm_property_index_t index,
                                    uint64_t* value){
    ((RpvObject*)handle)->GetPropertyAsUint64(property, index, value);
}                                      

rocprofvis_dm_result_t  rocprofvis_dm_get_property_as_int64(
                                    rocprofvis_dm_handle_t handle, 	
                                    rocprofvis_dm_property_t property,
                                    rocprofvis_dm_property_index_t index,
                                    int64_t* value){
    ((RpvObject*)handle)->GetPropertyAsInt64(property, index, value);
}                                      

rocprofvis_dm_result_t  rocprofvis_dm_get_property_as_double(
                                    rocprofvis_dm_handle_t handle, 	
                                    rocprofvis_dm_property_t property,
                                    rocprofvis_dm_property_index_t index,
                                    double* value){
    ((RpvObject*)handle)->GetPropertyAsDouble(property, index, value);
}                                       

rocprofvis_dm_result_t  rocprofvis_dm_get_property_as_charptr(
                                    rocprofvis_dm_handle_t handle, 	
                                    rocprofvis_dm_property_t property,
                                    rocprofvis_dm_property_index_t index,
                                    char** value){
    ((RpvObject*)handle)->GetPropertyAsCharPtr(property, index, value);
}                                       

rocprofvis_dm_result_t  rocprofvis_dm_get_property_as_handle(
                                    rocprofvis_dm_handle_t handle, 	
                                    rocprofvis_dm_property_t property,
                                    rocprofvis_dm_property_index_t index,
                                    rocprofvis_dm_handle* value){
    ((RpvObject*)handle)->GetPropertyAsHandle(property, index, value);
}                                      