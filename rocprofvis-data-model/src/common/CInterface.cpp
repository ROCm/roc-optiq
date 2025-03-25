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



#include "CInterface.h"
#include "../database/RocpdDb.h"
#include "../database/RocprofDb.h"
#include "../datamodel/Trace.h"

/******************************************DATABASE INTERFACE***************************************/
/****************************************************************************************************
 * @brief Opens database of provided path and type
 *      kAutodetect = 0, 
 *	    kRocpdSqlite = 1,
 *	    kRocprofSqlite = 2
 * 
 * @param filename path of the database file
 * @param type  type enumeration, kAutodetect for automatic detection 
 * @return handler to database object
 * 
 * @note Currently only old rocpd schema fully supported. Working on rocprof schema
 * 
 ***************************************************************************************************/

rocprofvis_dm_database_t rocprofvis_db_open_database(
                                        rocprofvis_db_filename_t filename, 
                                        rocprofvis_db_type_t db_type){
    PROFILE;
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
        if (db_type == rocprofvis_db_type_t::kRocprofSqlite)
        {
            try {
                Database* db = new RocprofDatabase(filename);
                ASSERT_MSG_RETURN(db, ERROR_DATABASE_CANNOT_BE_NULL, nullptr);
                if (kRocProfVisDmResultSuccess == db->Open()) {
                    return db;
                }
                else {
                    ASSERT_ALWAYS_MSG_RETURN("Error! Failed to open database!", nullptr);
                }
            }
            catch (std::exception ex)
            {
                ASSERT_ALWAYS_MSG_RETURN(ERROR_MEMORY_ALLOCATION_FAILURE, nullptr);
            }
        }
        else
    {
        LOG("Database type not supported!");
        return nullptr;
    }                                  
}

/****************************************************************************************************
 * @brief Calculates size of memory used by database object
 * 
 * @param database database handle
 * @return size of used memory
 * 
 ***************************************************************************************************/

rocprofvis_dm_size_t rocprofvis_db_get_memory_footprint(rocprofvis_dm_database_t database){ 
    ASSERT_MSG_RETURN(database, ERROR_DATABASE_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
    Database* db = (Database*) database;
    return db->GetMemoryFootprint();
}

/****************************************************************************************************
 * @brief Allocates future object, to be used for asynchronous operations
 * 
 * @param callback callback method to report current database request progress and status. 
 *                  May be useful for command line tools and scripts
 * @return future object handle
 * 
 ***************************************************************************************************/
rocprofvis_db_future_t rocprofvis_db_future_alloc(
                                        rocprofvis_db_progress_callback_t callback){
    PROFILE;
    try{
        return new Future(callback);
    }
    catch(std::exception ex)
    {
        ASSERT_ALWAYS_MSG_RETURN(ERROR_MEMORY_ALLOCATION_FAILURE, nullptr);
    }
}

/****************************************************************************************************
 * @brief Waits until asynchronous operation is completed or timeout expires
 * 
 * @param object future handle allocated by rocprofvis_db_future_alloc
 * @param timeout timeout in seconds for the asyncronous call to expire
 * @return status of operation
 * 
 ***************************************************************************************************/
rocprofvis_dm_result_t rocprofvis_db_future_wait(
                                        rocprofvis_db_future_t object, 
                                        rocprofvis_db_timeout_sec_t timeout){
    PROFILE;
    Future * future = (Future*) object;
    ASSERT_MSG_RETURN(future, ERROR_FUTURE_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
#ifdef DEBUG
    return future->WaitForCompletion(timeout*100000); 
#else
    return future->WaitForCompletion(timeout*1000);
#endif                                
}

/****************************************************************************************************
 * @brief Free future object
 * 
 * @param object future handle allocated by rocprofvis_db_future_alloc
 * 
 ***************************************************************************************************/
void rocprofvis_db_future_free(
                                        rocprofvis_db_future_t object){
    PROFILE;
    ASSERT_MSG_RETURN(object, ERROR_FUTURE_CANNOT_BE_NULL, );
    delete (Future*) object;
}

/****************************************************************************************************
 * @brief Asynchronous call to read data model metadata 
 *              (static objects residing in trace class memory until trace is deleted)
 * 
 * @param database database handle
 * @param object future handle allocated by rocprofvis_db_future_alloc
 * @return status of operation
 * 
 ***************************************************************************************************/
rocprofvis_dm_result_t rocprofvis_db_read_metadata_async(
                                        rocprofvis_dm_database_t database, 
                                        rocprofvis_db_future_t object){
    PROFILE;
    ASSERT_MSG_RETURN(database, ERROR_DATABASE_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
    Database* db = (Database*) database;
    return db->ReadTraceMetadataAsync(object);
}

/****************************************************************************************************
 * @brief Asynchronous call to read time slice of records for provided time frame and tracks selection
 * 
 * @param database database handle
 * @param start begining of the time slice
 * @param start end of the time slice
 * @param num number of tracks in rocprofvis_db_track_selection_t array (uint32*)
 * @param track tracks selection array (uint32*)
 * @param object future handle allocated by rocprofvis_db_future_alloc
 * @return status of operation
 * 
 * @note Object will stay in trace memory until deleted. 
 *             Use rocprofvis_dm_delete_time_slice or rocprofvis_dm_delete_all_time_slices for deletion
 * 
 ***************************************************************************************************/
rocprofvis_dm_result_t rocprofvis_db_read_trace_slice_async(
                                        rocprofvis_dm_database_t database,
                                        rocprofvis_dm_timestamp_t start,
                                        rocprofvis_dm_timestamp_t end,
                                        rocprofvis_db_num_of_tracks_t num,
                                        rocprofvis_db_track_selection_t tracks,
                                        rocprofvis_db_future_t object){
    PROFILE;
    ASSERT_MSG_RETURN(database, ERROR_DATABASE_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
    Database* db = (Database*) database;
    return db->ReadTraceSliceAsync(start,end,num,tracks,object);
}

/****************************************************************************************************
 * @brief Asynchronous call to read event property of specific type
 *                                                     
 * @param database database handle
 * @param type type of propery:
 *                             kRPVDMEventFlowTrace,
 *                             kRPVDMEventStackTrace,
 *                             kRPVDMEventExtData,
 * @param event_id 60-bit event id and 4-bit operation type
 * @param object future handle allocated by rocprofvis_db_future_alloc
 * @return status of operation
 * 
 * @note Object will stay in trace memory until deleted. 
 *          Use rocprofvis_dm_delete_event_property_for or 
 *          rocprofvis_dm_delete_all_event_properties_for for deletion
 * 
 ***************************************************************************************************/
rocprofvis_dm_result_t  rocprofvis_db_read_event_property_async(
                                        rocprofvis_dm_database_t database,
                                        rocprofvis_dm_event_property_type_t type,
                                        rocprofvis_dm_event_id_t event_id,
                                        rocprofvis_db_future_t object){
    PROFILE;
    ASSERT_MSG_RETURN(database, ERROR_DATABASE_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
    Database* db = (Database*) database;
    return db->ReadEventPropertyAsync(type, event_id, object);
}

/****************************************************************************************************
 * @brief Asynchronous call to read a table result of specified SQL query
 *                                                     
 * @param database database object handle
 * @param query SQL query string
 * @param description description of a table
 * @param object future handle allocated by rocprofvis_db_future_alloc
 * @return status of operation
 * 
 * @note Object will stay in trace memory until deleted. 
 *          Use rocprofvis_dm_delete_table_at or rocprofvis_dm_delete_all_tables for deletion
 * 
 ***************************************************************************************************/
rocprofvis_dm_result_t  rocprofvis_db_execute_query_async(
                                        rocprofvis_dm_database_t database,                                                                
                                        rocprofvis_dm_charptr_t query,
                                        rocprofvis_dm_charptr_t description,
                                        rocprofvis_db_future_t object){
    PROFILE;
    ASSERT_MSG_RETURN(database, ERROR_DATABASE_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
    Database* db = (Database*) database;
    return db->ExecuteQueryAsync(query, description, object);
}

/*******************************************TRACE INTERFACE*****************************************/

/****************************************************************************************************
 * @brief Create trace object
 * 
 * @return trace object handle
 * 
 * @note trace object needs to bound to a database and database interface methods should be 
 *       called to fill trace with data, starting with rocprofvis_db_read_metadata_async  
 ***************************************************************************************************/
rocprofvis_dm_trace_t  rocprofvis_dm_create_trace(){
    PROFILE;
    try{
        return new RpvDmTrace();
    }
    catch(std::exception ex)
    {
        ASSERT_ALWAYS_MSG_RETURN(ERROR_MEMORY_ALLOCATION_FAILURE, nullptr);
    }
}

/****************************************************************************************************
 * @brief Deleting trace and all its resources including bound database
 *                                                     
 * @param trace trace object handle
 * 
 * @return status of operation
 * 
 ***************************************************************************************************/
rocprofvis_dm_result_t rocprofvis_dm_delete_trace(
                                        rocprofvis_dm_trace_t trace)
{
    PROFILE;
    ASSERT_MSG_RETURN(trace, ERROR_TRACE_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
    RpvDmTrace* _trace = (RpvDmTrace*)trace;
    Database* db = (Database*)((RpvDmTrace*)trace)->Database();
    if (db) delete db;
    delete _trace;
    return kRocProfVisDmResultSuccess;
}                                      

/****************************************************************************************************
 * @brief Binds trace to database
 *                                                     
 * @param trace trace object handle created with rocprofvis_dm_create_trace()
 * @param database database object handle created with rocprofvis_db_open_database()
 * 
 * @return status of operation
 * 
 ***************************************************************************************************/
rocprofvis_dm_result_t rocprofvis_dm_bind_trace_to_database(   rocprofvis_dm_trace_t trace,
                                        rocprofvis_dm_database_t database){
    PROFILE;
    ASSERT_MSG_RETURN(trace, ERROR_TRACE_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
    ASSERT_MSG_RETURN(database, ERROR_DATABASE_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
    rocprofvis_dm_db_bind_struct* bind_data=nullptr;
    if  (kRocProfVisDmResultSuccess == ((RpvDmTrace*)trace)->BindDatabase(database, bind_data) && 
         kRocProfVisDmResultSuccess == ((Database*)database)->BindTrace(bind_data))
         return kRocProfVisDmResultSuccess;
    ASSERT_ALWAYS_MSG_RETURN("Error! Cannot bind trace to database", kRocProfVisDmResultUnknownError);
} 

/****************************************************************************************************
 * @brief Delete time slice with specified start and end timestamps
 *                                                     
 * @param trace trace object handle created with rocprofvis_dm_create_trace()
 * @param start time slice start timestamp
 * @param end time slice end timestamp
 * 
 * @return status of operation
 * 
 ***************************************************************************************************/
rocprofvis_dm_result_t rocprofvis_dm_delete_time_slice( 
                                        rocprofvis_dm_trace_t trace,
                                        rocprofvis_dm_timestamp_t start,
                                        rocprofvis_dm_timestamp_t end){
    PROFILE;
    ASSERT_MSG_RETURN(trace, ERROR_TRACE_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
    return ((RpvDmTrace*)trace)->DeleteSliceAtTimeRange(start, end);
}   

/****************************************************************************************************
 * @brief Delete all time slices 
 *                                                     
 * @param trace trace object handle created with rocprofvis_dm_create_trace()
 * 
 * @return status of operation
 * 
 ***************************************************************************************************/
rocprofvis_dm_result_t rocprofvis_dm_delete_all_time_slices( 
                                        rocprofvis_dm_trace_t trace){
    PROFILE;
    ASSERT_MSG_RETURN(trace, ERROR_TRACE_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
    return ((RpvDmTrace*)trace)->DeleteAllSlices();
}

/****************************************************************************************************
 * @brief Delete event property object of specified type
 *                                                     
 * @param trace trace object handle created with rocprofvis_dm_create_trace()
 * @param type type of property
 *                             kRPVDMEventFlowTrace,
 *                             kRPVDMEventStackTrace,
 *                             kRPVDMEventExtData,
 * @param event_id 60-bit event id and 4-bit operation type
 * 
 * @return status of operation
 * 
 ***************************************************************************************************/
rocprofvis_dm_result_t  rocprofvis_dm_delete_event_property_for(
                                        rocprofvis_dm_trace_t trace,
                                        rocprofvis_dm_event_property_type_t type,
                                        rocprofvis_dm_event_id_t event_id){
    PROFILE;
    ASSERT_MSG_RETURN(trace, ERROR_TRACE_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
    return ((RpvDmTrace*)trace)->DeleteEventPropertyFor(type, event_id);
}     

/****************************************************************************************************
 * @brief Delete all event property objects of specified type
 *                                                     
 * @param trace trace object handle created with rocprofvis_dm_create_trace()
 * @param type type of property
 *                             kRPVDMEventFlowTrace,
 *                             kRPVDMEventStackTrace,
 *                             kRPVDMEventExtData,
 * 
 * @return status of operation
 * 
 ***************************************************************************************************/
rocprofvis_dm_result_t  rocprofvis_dm_delete_all_event_properties_for(
                                        rocprofvis_dm_trace_t trace,
                                        rocprofvis_dm_event_property_type_t type){
    PROFILE;
    ASSERT_MSG_RETURN(trace, ERROR_TRACE_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
    return ((RpvDmTrace*)trace)->DeleteAllEventPropertiesFor(type);
}    

/****************************************************************************************************
 * @brief Delete a table by specified table index. 
 *        Table is created when executed SQL query using rocprofvis_db_execute_query_async method
 *                                                     
 * @param trace trace object handle created with rocprofvis_dm_create_trace()
 * @param index index of a table. Number of existing tables can queried by reading Trace object
 * property kRPVDMNumberOfTablesUInt64   
 * 
 * @return status of operation
 * 
 ***************************************************************************************************/
rocprofvis_dm_result_t  rocprofvis_dm_delete_table_at( 
                                        rocprofvis_dm_trace_t trace,
                                        rocprofvis_dm_index_t index){
    PROFILE;
    ASSERT_MSG_RETURN(trace, ERROR_TRACE_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
    return ((RpvDmTrace*)trace)->DeleteTableAt(index);
}

/****************************************************************************************************
 * @brief Delete all tables. 
 *        Tables are created when executed SQL queries using rocprofvis_db_execute_query_async method
 *                                                     
 * @param trace trace object handle created with rocprofvis_dm_create_trace()
 * 
 * @return status of operation
 * 
 ***************************************************************************************************/
rocprofvis_dm_result_t  rocprofvis_dm_delete_all_tables( 
                                        rocprofvis_dm_trace_t trace){
    PROFILE;
    ASSERT_MSG_RETURN(trace, ERROR_TRACE_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
    return ((RpvDmTrace*)trace)->DeleteAllTables();
}

/***********************************Universal property getters**************************************
*
* There are 10 property getter methods. Two methods to get property of each type: 
*       uint64, int64, double, char* and rocprofvis_dm_handle_t
* First set of methods returns values as pointer references and result of operation as return value
* Second set returns property values directly. No result of operation is returned.
* In second case if operation fails, 0 or nullptr will be returned.   
****************************************************************************************************/

/****************************************************************************************************
 * @brief Return property value as uint64. 
 *                                                     
 * @param handle any object handle
 * @param property enumeration of properties for specified handle type
 * @param index index of any indexed property
 * @param value pointer to a uint64_t value
 * 
 * @return status of operation
 * 
 ***************************************************************************************************/
rocprofvis_dm_result_t  rocprofvis_dm_get_property_as_uint64(
                                        rocprofvis_dm_handle_t handle, 	
                                        rocprofvis_dm_property_t property,
                                        rocprofvis_dm_property_index_t index,
                                        uint64_t* value){
    PROFILE_PROP_ACCESS;
    return ((RpvObject*)handle)->GetPropertyAsUint64(property, index, value);
}                                      

/****************************************************************************************************
 * @brief Return property value as int64. 
 *                                                     
 * @param handle any object handle
 * @param property enumeration of properties for specified handle type
 * @param index index of any indexed property
 * @param value pointer to a int64_t value
 * 
 * @return status of operation
 * 
 ***************************************************************************************************/
rocprofvis_dm_result_t  rocprofvis_dm_get_property_as_int64(
                                        rocprofvis_dm_handle_t handle, 	
                                        rocprofvis_dm_property_t property,
                                        rocprofvis_dm_property_index_t index,
                                        int64_t* value){
    PROFILE_PROP_ACCESS;
    return ((RpvObject*)handle)->GetPropertyAsInt64(property, index, value);
}                                      

/****************************************************************************************************
 * @brief Return property value as double. 
 *                                                     
 * @param handle any object handle
 * @param property enumeration of properties for specified handle type
 * @param index index of any indexed property
 * @param value pointer to a double type value
 * 
 * @return status of operation
 * 
 ***************************************************************************************************/
rocprofvis_dm_result_t  rocprofvis_dm_get_property_as_double(
                                        rocprofvis_dm_handle_t handle, 	
                                        rocprofvis_dm_property_t property,
                                        rocprofvis_dm_property_index_t index,
                                        double* value){
    PROFILE_PROP_ACCESS;
    return ((RpvObject*)handle)->GetPropertyAsDouble(property, index, value);
}                                       

/****************************************************************************************************
 * @brief Return property value as array of characters. 
 *                                                     
 * @param handle any object handle
 * @param property enumeration of properties for specified handle type
 * @param index index of any indexed property
 * @param value pointer to an to array of characters
 * 
 * @return status of operation
 * 
 ***************************************************************************************************/
rocprofvis_dm_result_t  rocprofvis_dm_get_property_as_charptr(
                                        rocprofvis_dm_handle_t handle, 	
                                        rocprofvis_dm_property_t property,
                                        rocprofvis_dm_property_index_t index,
                                        char** value){
    PROFILE_PROP_ACCESS;
    return ((RpvObject*)handle)->GetPropertyAsCharPtr(property, index, value);
}                                       

/****************************************************************************************************
 * @brief Return property value as rocprofvis_dm_handle_t (void*). 
 *                                                     
 * @param handle any object handle
 * @param property enumeration of properties for specified handle type
 * @param index index of any indexed property
 * @param value pointer to rocprofvis_dm_handle_t 
 * 
 * @return status of operation
 * 
 ***************************************************************************************************/
rocprofvis_dm_result_t  rocprofvis_dm_get_property_as_handle(
                                        rocprofvis_dm_handle_t handle, 	
                                        rocprofvis_dm_property_t property,
                                        rocprofvis_dm_property_index_t index,
                                        rocprofvis_dm_handle_t* value){
    PROFILE_PROP_ACCESS;
    return ((RpvObject*)handle)->GetPropertyAsHandle(property, index, value);
}  


/****************************************************************************************************
 * @brief Return property value as uint64. 
 *                                                     
 * @param handle any object handle
 * @param property enumeration of properties for specified handle type
 * @param index index of any indexed property
 * 
 * @return uint64_t value
 * 
 ***************************************************************************************************/
uint64_t  rocprofvis_dm_get_property_as_uint64(
                                        rocprofvis_dm_handle_t handle, 	
                                        rocprofvis_dm_property_t property,
                                        rocprofvis_dm_property_index_t index){
    uint64_t value=0;
    if (kRocProfVisDmResultSuccess == rocprofvis_dm_get_property_as_uint64(handle,property,index,&value)) return value;
    return 0;
}                                     

/****************************************************************************************************
 * @brief Return property value as int64. 
 *                                                     
 * @param handle any object handle
 * @param property enumeration of properties for specified handle type
 * @param index index of any indexed property
 * 
 * @return int64_t value
 * 
 ***************************************************************************************************/
int64_t   rocprofvis_dm_get_property_as_int64(
                                        rocprofvis_dm_handle_t handle, 	
                                        rocprofvis_dm_property_t property,
                                        rocprofvis_dm_property_index_t index){
    int64_t value=0;
    if (kRocProfVisDmResultSuccess == rocprofvis_dm_get_property_as_int64(handle,property,index,&value)) return value;
    return 0;
}

/****************************************************************************************************
 * @brief Return property value as double. 
 *                                                     
 * @param handle any object handle
 * @param property enumeration of properties for specified handle type
 * @param index index of any indexed property
 * 
 * @return double value
 * 
 ***************************************************************************************************/
double  rocprofvis_dm_get_property_as_double(
                                        rocprofvis_dm_handle_t handle, 	
                                        rocprofvis_dm_property_t property,
                                        rocprofvis_dm_property_index_t index){
    double value=0;
    if (kRocProfVisDmResultSuccess == rocprofvis_dm_get_property_as_double(handle,property,index,&value)) return value;
    return 0.0;
} 

/****************************************************************************************************
 * @brief Return property value as char*. 
 *                                                     
 * @param handle any object handle
 * @param property enumeration of properties for specified handle type
 * @param index index of any indexed property
 * 
 * @return char* value
 * 
 ***************************************************************************************************/
char*   rocprofvis_dm_get_property_as_charptr(
                                        rocprofvis_dm_handle_t handle, 	
                                        rocprofvis_dm_property_t property,
                                        rocprofvis_dm_property_index_t index){
    char* value=nullptr;
    if (kRocProfVisDmResultSuccess == rocprofvis_dm_get_property_as_charptr(handle,property,index,&value)) return value;
    return nullptr;
} 

/****************************************************************************************************
 * @brief Return property value as rocprofvis_dm_handle_t. 
 *                                                     
 * @param handle any object handle
 * @param property enumeration of properties for specified handle type
 * @param index index of any indexed property
 * 
 * @return rocprofvis_dm_handle_t value
 * 
 ***************************************************************************************************/
rocprofvis_dm_handle_t  rocprofvis_dm_get_property_as_handle(
                                        rocprofvis_dm_handle_t handle, 	
                                        rocprofvis_dm_property_t property,
                                        rocprofvis_dm_property_index_t index){
    rocprofvis_dm_handle_t value=nullptr;
    if (kRocProfVisDmResultSuccess == rocprofvis_dm_get_property_as_handle(handle,property,index,&value)) return value;
    return nullptr;
} 