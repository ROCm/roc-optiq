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



#include "Interface.h"
#include "../database/RocpdDb.h"
#include "../datamodel/Trace.h"

/********************************DATABASE INDERFACE************************************/
rocprofvis_dm_database_t rocprofvis_db_open_database(
                                        rocprofvis_db_filename_t filename, 
                                        rocprofvis_db_type_t db_type){
    PRINT_TIME_USAGE;
    if (db_type == rocprofvis_db_type_t::kAutodetect) {
        db_type = Database::Autodetect(filename);
    } 
    if (db_type == rocprofvis_db_type_t::kRocpdSqlite)
    {
        try {
            Database* db = new RocpdDatabase(filename);
            ASSERT_MSG_RETURN(db, ERROR_DATABASE_CANNOT_BE_NULL, nullptr);
            if (kRocProfVisDmResultSuccess == db->Open()) {
                return db;
            } else {
                ASSERT_ALWAYS_MSG_RETURN("Error! Failed to open database!", nullptr);
            }
        }
        catch(std::exception ex)
        {
            ASSERT_ALWAYS_MSG_RETURN(ERROR_MEMORY_ALLOCATION_FAILURE, nullptr);
        }
    } else
    {
        LOG("Database type not supported!");
        return nullptr;
    }                                  
}

rocprofvis_db_future_t rocprofvis_db_future_alloc(
                                        rocprofvis_db_progress_callback_t callback){
    PRINT_TIME_USAGE;
    try{
        return new Future(callback);
    }
    catch(std::exception ex)
    {
        ASSERT_ALWAYS_MSG_RETURN(ERROR_MEMORY_ALLOCATION_FAILURE, nullptr);
    }
}

rocprofvis_dm_result_t rocprofvis_db_future_wait(
                                        rocprofvis_db_future_t object, 
                                        rocprofvis_db_timeout_ms_t timeout){
    PRINT_TIME_USAGE;
    Future * future = (Future*) object;
    return future->WaitForCompletion(timeout);                                 
}

void rocprofvis_db_future_free(
                                        rocprofvis_db_future_t object){
    PRINT_TIME_USAGE;
    delete (Future*) object;
}

rocprofvis_dm_result_t rocprofvis_db_read_metadata_async(
                                        rocprofvis_dm_database_t database, 
                                        rocprofvis_db_future_t object){
    PRINT_TIME_USAGE;
    Database* db = (Database*) database;
    return db->ReadTraceMetadataAsync(object);
}

rocprofvis_dm_result_t rocprofvis_db_read_trace_slice_async(
                                        rocprofvis_dm_database_t database,
                                        rocprofvis_dm_timestamp_t start,
                                        rocprofvis_dm_timestamp_t end,
                                        rocprofvis_db_num_of_tracks_t num,
                                        rocprofvis_db_track_selection_t tracks,
                                        rocprofvis_db_future_t object){
    PRINT_TIME_USAGE;
    Database* db = (Database*) database;
    return db->ReadTraceSliceAsync(start,end,num,tracks,object);
}


rocprofvis_dm_result_t  rocprofvis_db_read_event_property_async(
                                        rocprofvis_dm_database_t database,
                                        rocprofvis_dm_event_property_type_t type,
                                        rocprofvis_dm_event_id_t event_id,
                                        rocprofvis_db_future_t object){
    PRINT_TIME_USAGE;
    Database* db = (Database*) database;
    return db->ReadEventPropertyAsync(type, event_id, object);
}


rocprofvis_dm_result_t  rocprofvis_db_execute_query_async(
                                        rocprofvis_dm_database_t database,                                                                
                                        rocprofvis_dm_charptr_t query,
                                        rocprofvis_dm_charptr_t description,
                                        rocprofvis_db_future_t object){
    PRINT_TIME_USAGE;
    Database* db = (Database*) database;
    return db->ExecuteQueryAsync(query, description, object);
}

/************************************TRACE INTERFACE**************************************/

rocprofvis_dm_trace_t  rocprofvis_dm_create_trace(){
    PRINT_TIME_USAGE;
    try{
        return new RpvDmTrace();
    }
    catch(std::exception ex)
    {
        ASSERT_ALWAYS_MSG_RETURN(ERROR_MEMORY_ALLOCATION_FAILURE, nullptr);
    }
}

rocprofvis_dm_result_t rocprofvis_dm_delete_trace(
                                        rocprofvis_dm_trace_t trace)
{
    PRINT_TIME_USAGE;
    ASSERT_MSG_RETURN(trace, ERROR_TRACE_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
    Database* db = (Database*)((RpvDmTrace*)trace)->Database();
    if (db) delete db;
    delete trace;
    return kRocProfVisDmResultSuccess;
}                                      

rocprofvis_dm_result_t rocprofvis_dm_bind_trace_to_database(   rocprofvis_dm_trace_t trace,
                                        rocprofvis_dm_database_t database){
    PRINT_TIME_USAGE;
    ASSERT_MSG_RETURN(trace, ERROR_TRACE_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
    ASSERT_MSG_RETURN(database, ERROR_DATABASE_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
    rocprofvis_dm_db_bind_struct bind_data = {0};
    if  (kRocProfVisDmResultSuccess == ((RpvDmTrace*)trace)->BindDatabase(database, bind_data) && 
         kRocProfVisDmResultSuccess == ((Database*)database)->BindTrace(bind_data))
         return kRocProfVisDmResultSuccess;
    ASSERT_ALWAYS_MSG_RETURN("Error! Cannot bind trace to database", kRocProfVisDmResultUnknownError);
} 

rocprofvis_dm_result_t rocprofvis_dm_delete_time_slice( 
                                        rocprofvis_dm_trace_t trace,
                                        rocprofvis_dm_timestamp_t start,
                                        rocprofvis_dm_timestamp_t end){
    PRINT_TIME_USAGE;
    ASSERT_MSG_RETURN(trace, ERROR_TRACE_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
    return ((RpvDmTrace*)trace)->DeleteSliceAtTimeRange(start, end);
}   

rocprofvis_dm_result_t rocprofvis_dm_delete_all_time_slices( 
                                        rocprofvis_dm_trace_t trace){
    PRINT_TIME_USAGE;
    ASSERT_MSG_RETURN(trace, ERROR_TRACE_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
    return ((RpvDmTrace*)trace)->DeleteAllSlices();
}

rocprofvis_dm_result_t  rocprofvis_dm_delete_event_property_for(
                                        rocprofvis_dm_trace_t trace,
                                        rocprofvis_dm_event_property_type_t type,
                                        rocprofvis_dm_event_id_t event_id){
    PRINT_TIME_USAGE;
    ASSERT_MSG_RETURN(trace, ERROR_TRACE_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
    return ((RpvDmTrace*)trace)->DeleteEventPropertyFor(type, event_id);
}     

rocprofvis_dm_result_t  rocprofvis_dm_delete_all_event_properties_for(
                                        rocprofvis_dm_trace_t trace,
                                        rocprofvis_dm_event_property_type_t type){
    PRINT_TIME_USAGE;
    ASSERT_MSG_RETURN(trace, ERROR_TRACE_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
    return ((RpvDmTrace*)trace)->DeleteAllEventPropertiesFor(type);
}    

rocprofvis_dm_result_t  rocprofvis_dm_delete_table_at( 
                                        rocprofvis_dm_trace_t trace,
                                        rocprofvis_dm_index_t index){
    PRINT_TIME_USAGE;
    ASSERT_MSG_RETURN(trace, ERROR_TRACE_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
    return ((RpvDmTrace*)trace)->DeleteTableAt(index);
}

rocprofvis_dm_result_t  rocprofvis_dm_delete_all_tables( 
                                        rocprofvis_dm_trace_t trace){
    PRINT_TIME_USAGE;
    ASSERT_MSG_RETURN(trace, ERROR_TRACE_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
    return ((RpvDmTrace*)trace)->DeleteAllTables();
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
    PRINT_TIME_USAGE;
    return ((RpvObject*)handle)->GetPropertyAsUint64(property, index, value);
}                                      

rocprofvis_dm_result_t  rocprofvis_dm_get_property_as_int64(
                                        rocprofvis_dm_handle_t handle, 	
                                        rocprofvis_dm_property_t property,
                                        rocprofvis_dm_property_index_t index,
                                        int64_t* value){
    PRINT_TIME_USAGE;
    return ((RpvObject*)handle)->GetPropertyAsInt64(property, index, value);
}                                      

rocprofvis_dm_result_t  rocprofvis_dm_get_property_as_double(
                                        rocprofvis_dm_handle_t handle, 	
                                        rocprofvis_dm_property_t property,
                                        rocprofvis_dm_property_index_t index,
                                        double* value){
    PRINT_TIME_USAGE;
    return ((RpvObject*)handle)->GetPropertyAsDouble(property, index, value);
}                                       

rocprofvis_dm_result_t  rocprofvis_dm_get_property_as_charptr(
                                        rocprofvis_dm_handle_t handle, 	
                                        rocprofvis_dm_property_t property,
                                        rocprofvis_dm_property_index_t index,
                                        char** value){
    PRINT_TIME_USAGE;
    return ((RpvObject*)handle)->GetPropertyAsCharPtr(property, index, value);
}                                       

rocprofvis_dm_result_t  rocprofvis_dm_get_property_as_handle(
                                        rocprofvis_dm_handle_t handle, 	
                                        rocprofvis_dm_property_t property,
                                        rocprofvis_dm_property_index_t index,
                                        rocprofvis_dm_handle_t* value){
    PRINT_TIME_USAGE;
    return ((RpvObject*)handle)->GetPropertyAsHandle(property, index, value);
}  


uint64_t  rocprofvis_dm_get_property_as_uint64(
                                        rocprofvis_dm_handle_t handle, 	
                                        rocprofvis_dm_property_t property,
                                        rocprofvis_dm_property_index_t index){
    uint64_t value=0;
    if (kRocProfVisDmResultSuccess == rocprofvis_dm_get_property_as_uint64(handle,property,index,&value)) return value;
    return 0;
}                                     

int64_t   rocprofvis_dm_get_property_as_int64(
                                        rocprofvis_dm_handle_t handle, 	
                                        rocprofvis_dm_property_t property,
                                        rocprofvis_dm_property_index_t index){
    int64_t value=0;
    if (kRocProfVisDmResultSuccess == rocprofvis_dm_get_property_as_int64(handle,property,index,&value)) return value;
    return 0;
}

double  rocprofvis_dm_get_property_as_double(
                                        rocprofvis_dm_handle_t handle, 	
                                        rocprofvis_dm_property_t property,
                                        rocprofvis_dm_property_index_t index){
    double value=0;
    if (kRocProfVisDmResultSuccess == rocprofvis_dm_get_property_as_double(handle,property,index,&value)) return value;
    return 0.0;
} 

char*   rocprofvis_dm_get_property_as_charptr(
                                        rocprofvis_dm_handle_t handle, 	
                                        rocprofvis_dm_property_t property,
                                        rocprofvis_dm_property_index_t index){
    char* value=nullptr;
    if (kRocProfVisDmResultSuccess == rocprofvis_dm_get_property_as_charptr(handle,property,index,&value)) return value;
    return nullptr;
} 

rocprofvis_dm_handle_t  rocprofvis_dm_get_property_as_handle(
                                        rocprofvis_dm_handle_t handle, 	
                                        rocprofvis_dm_property_t property,
                                        rocprofvis_dm_property_index_t index){
    rocprofvis_dm_handle_t value=nullptr;
    if (kRocProfVisDmResultSuccess == rocprofvis_dm_get_property_as_handle(handle,property,index,&value)) return value;
    return nullptr;
} 