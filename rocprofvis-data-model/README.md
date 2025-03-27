The Data Model package include two components:<br />
    1. Data Model - layered data storage, with public interface to access data properties and possibility of freeing redundant objects. <br />
    2. Database - database query manager and processor, with public interface to add data model objects. Supports asynchronous database access.<br />
They are interconnected via binding interface<br />


**Building project:**<br />

**To build library under Windows:**<br />
unzip src/database/sqlite3/sqlite3.zip -d src/database/sqlite3/<br />
mkdir build<br />
cd build<br />
cmake ..<br />
load rocprofvis-datamodel-lib.sln to Visual Studio <br />
build<br />

**To build library under Linux:**<br />
unzip src/database/sqlite3/sqlite3.zip -d src/database/sqlite3/<br />
mkdir build<br />
cd build<br />
cmake ..<br />
make <br />

**To build library for testing under Windows:**<br />
unzip src/database/sqlite3/sqlite3.zip -d src/database/sqlite3/<br />
mkdir build<br />
cd build<br />
cmake -DTEST=ON ..<br />
load rocprofvis-datamodel-lib.sln to Visual Studio <br />
build<br />

**To build library for testing under Linux:**<br />
unzip src/database/sqlite3/sqlite3.zip -d src/database/sqlite3/<br />
mkdir build<br />
cd build<br />
cmake -DTEST=ON ..<br />
make <br />



#Database interface
---
'''
  **Opens database of provided path and type**
       kAutodetect = 0, 
 	    kRocpdSqlite = 1,
 	    kRocprofSqlite = 2
'''  
  'param' filename path of the database file
  'param' type  type enumeration, kAutodetect for automatic detection 
  'return' handler to database object
  'note' Currently only old rocpd schema fully supported. Working on rocprof schema
  
''' 
rocprofvis_dm_database_t rocprofvis_db_open_database(
                                    rocprofvis_db_filename_t, 
                                    rocprofvis_db_type_t);
'''
---
  **Calculates size of memory used by database object**
  
  'param' database database handle
  'return' size of used memory
  
'''
rocprofvis_dm_size_t rocprofvis_db_get_memory_footprint(
                                    rocprofvis_dm_database_t);
'''
---
  **Allocates future object, to be used for asynchronous operations**
  
  'param' callback callback method to report current database request progress and status. 
                   May be useful for command line tools and scripts
  'return' future object handle
  
'''  
rocprofvis_db_future_t rocprofvis_db_future_alloc(
                                    rocprofvis_db_progress_callback_t );
'''  
---
'''  
  **Waits until asynchronous operation is completed or timeout expires**
  
  'param' object future handle allocated by rocprofvis_db_future_alloc
  'param' timeout timeout in seconds for the asynchronous call to expire
  'return' status of operation
'''  
  
'''  
rocprofvis_dm_result_t rocprofvis_db_future_wait(
                                    rocprofvis_db_future_t, 
                                    rocprofvis_db_timeout_sec_t);
'''  
---
'''  
  **Frees future object**
'''  
  
  'param' object future handle allocated by rocprofvis_db_future_alloc
  
'''
void rocprofvis_db_future_free(rocprofvis_db_future_t);
'''
---
'''  
  **Asynchronous call to read data model metadata** 
               (static objects residing in trace class memory until trace is deleted)
'''  
  
  'param' database database handle
  'param' object future handle allocated by rocprofvis_db_future_alloc
  'return' status of operation
  
'''  
rocprofvis_dm_result_t rocprofvis_db_read_metadata_async(
                                    rocprofvis_dm_database_t, 
                                    rocprofvis_db_future_t);
'''  
---
'''  
  **Asynchronous call to read time slice of records for provided time frame and tracks selection**
'''  
 
  'param' database database handle
  'param' start beginning of the time slice
  'param' start end of the time slice
  'param' num number of tracks in rocprofvis_db_track_selection_t array (uint32)
  'param' track tracks selection array (uint32)
  'param' object future handle allocated by rocprofvis_db_future_alloc
  'return' status of operation
 
  'note' Object will stay in trace memory until deleted.
              Use rocprofvis_dm_delete_time_slice or rocprofvis_dm_delete_all_time_slices for deletion 

'''  
rocprofvis_dm_result_t rocprofvis_db_read_trace_slice_async( 
                                    rocprofvis_dm_database_t,
                                    rocprofvis_dm_timestamp_t,
                                    rocprofvis_dm_timestamp_t,
                                    rocprofvis_db_num_of_tracks_t,
                                    rocprofvis_db_track_selection_t,
                                    rocprofvis_db_future_t);                              
'''  
---
'''  
  **Asynchronous call to read event property of specific type**
'''  
 
  'param' database database handle
  'param' type type of property:
                              kRPVDMEventFlowTrace,
                              kRPVDMEventStackTrace,
                              kRPVDMEventExtData,
  'param' event_id 60-bit event id and 4-bit operation type
  'param' object future handle allocated by rocprofvis_db_future_alloc
  'return' status of operation
 
  'note' Object will stay in trace memory until deleted.
           Use rocprofvis_dm_delete_event_property_for or
           rocprofvis_dm_delete_all_event_properties_for for deletion
'''  
rocprofvis_dm_result_t  rocprofvis_db_read_event_property_async(
                                    rocprofvis_dm_database_t,
                                    rocprofvis_dm_event_property_type_t,
                                    rocprofvis_dm_event_id_t,
                                    rocprofvis_db_future_t);
'''  
---
'''  
  **Asynchronous call to read a table result of specified SQL query**
'''  
 
  'param' database database handle
  'param' query SQL query string
  'param' description description of a table
  'param' object future handle allocated by rocprofvis_db_future_alloc
  'return' status of operation
 
  'note' Object will stay in trace memory until deleted.
           Use rocprofvis_dm_delete_table_at or rocprofvis_dm_delete_all_tables for deletion
 
'''  
rocprofvis_dm_result_t  rocprofvis_db_execute_query_async(
                                    rocprofvis_dm_database_t,                                                                
                                    rocprofvis_dm_charptr_t,
                                    rocprofvis_dm_charptr_t,
                                    rocprofvis_db_future_t);
'''  

#Data model interface
---
'''  
  **Create trace object**
'''  
 
  'return' trace object handle
 
  'note' trace object needs to bound to a database and database interface methods should be
        called to fill trace with data, starting with rocprofvis_db_read_metadata_async

'''  
rocprofvis_dm_trace_t   rocprofvis_dm_create_trace(void); 
'''  
---
'''  
  **Deleting trace and all its resources including bound database**
'''  
 
  'param' trace trace object handle
 
  'return' status of operation
 
'''  
rocprofvis_dm_result_t  rocprofvis_dm_delete_trace( 
                                    rocprofvis_dm_trace_t);   
'''  
---
'''  
  **Binds trace to database**
'''  
 
  'param' trace trace object handle created with rocprofvis_dm_create_trace()
  'param' database database object handle created with rocprofvis_db_open_database()
 
  'return' status of operation
 
'''  
rocprofvis_dm_result_t  rocprofvis_dm_bind_trace_to_database( 
                                    rocprofvis_dm_trace_t,
                                    rocprofvis_dm_database_t);                                      
'''  
---
'''  
  **Delete time slice with specified start and end timestamps**
 
  'param' trace trace object handle created with rocprofvis_dm_create_trace()
  'param' start time slice start timestamp
  'param' end time slice end timestamp
 
  'return' status of operation
 
'''  
rocprofvis_dm_result_t  rocprofvis_dm_delete_time_slice( 
                                    rocprofvis_dm_trace_t,
                                    rocprofvis_dm_timestamp_t,
                                    rocprofvis_dm_timestamp_t);     
'''  
---
'''  
  **Delete all time slices**
'''  
 
  'param' trace trace object handle created with rocprofvis_dm_create_trace()
 
  'return' status of operation
 
'''  
rocprofvis_dm_result_t  rocprofvis_dm_delete_all_time_slices( 
                                    rocprofvis_dm_trace_t);                                      
'''  
---
'''  
  **Delete event property object of specified type**
'''  
 
  'param' trace trace object handle created with rocprofvis_dm_create_trace()
  'param' type type of property
                              kRPVDMEventFlowTrace,
                              kRPVDMEventStackTrace,
                              kRPVDMEventExtData,
  'param' event_id 60-bit event id and 4-bit operation type
 
  'return' status of operation
 
'''  
rocprofvis_dm_result_t  rocprofvis_dm_delete_event_property_for( 
                                    rocprofvis_dm_trace_t,
                                    rocprofvis_dm_event_property_type_t,
                                    rocprofvis_dm_event_id_t);     
'''  
---
'''  
  **Delete all event property objects of specified type**
'''  
 
  'param' trace trace object handle created with rocprofvis_dm_create_trace()
  'param' type type of property
                              kRPVDMEventFlowTrace,
                              kRPVDMEventStackTrace,
                              kRPVDMEventExtData,
 
  'return' status of operation
 
'''  
rocprofvis_dm_result_t  rocprofvis_dm_delete_all_event_properties_for( 
                                    rocprofvis_dm_trace_t,
                                    rocprofvis_dm_event_property_type_t);        
'''  
---
'''  
  **Delete a table by specified table index**
         Table is created when executed SQL query using rocprofvis_db_execute_query_async method
'''  
 
  'param' trace trace object handle created with rocprofvis_dm_create_trace()
  'param' index index of a table. Number of existing tables can queried by reading Trace object property kRPVDMNumberOfTablesUInt64
 
  'return' status of operation
 
'''  
rocprofvis_dm_result_t  rocprofvis_dm_delete_table_at( 
                                    rocprofvis_dm_trace_t,
                                    rocprofvis_dm_index_t); 
'''  
---
'''  
  **Delete all tables**
         Tables are created when executed SQL queries using rocprofvis_db_execute_query_async method
'''  
 
  'param' trace trace object handle created with rocprofvis_dm_create_trace()
 
  'return' status of operation
 
'''  
rocprofvis_dm_result_t  rocprofvis_dm_delete_all_tables( 
                                    rocprofvis_dm_trace_t);  
'''  

#Universal property getters
---
 There are 10 property getter methods. Two methods to get property of each type:
       uint64, int64, double, char and rocprofvis_dm_handle_t
 First set of methods returns values as pointer references and result of operation as return value
 Second set returns property values directly. No result of operation is returned.
 In second case if operation fails, 0 or nullptr will be returned.
---
''' 
  **Return property value as uint64**
'''  
 
  'param' handle any object handle
  'param' property enumeration of properties for specified handle type
  'param' index index of any indexed property
 
  'return' uint64_t value
 
'''  
uint64_t  rocprofvis_dm_get_property_as_uint64(
                                    rocprofvis_dm_handle_t, 	
                                    rocprofvis_dm_property_t,
                                    rocprofvis_dm_property_index_t);                                      
'''  
---
'''  
  **Return property value as int64**
'''  
 
  'param' handle any object handle
  'param' property enumeration of properties for specified handle type
  'param' index index of any indexed property
 
  'return' int64_t value
 
'''  
int64_t   rocprofvis_dm_get_property_as_int64(
                                    rocprofvis_dm_handle_t, 	
                                    rocprofvis_dm_property_t,
                                    rocprofvis_dm_property_index_t); 
'''  
---
'''  
  **Return property value as double**
'''  
 
  'param' handle any object handle
  'param' property enumeration of properties for specified handle type
  'param' index index of any indexed property
 
  'return' double value
 
'''  
double  rocprofvis_dm_get_property_as_double(
                                    rocprofvis_dm_handle_t, 	
                                    rocprofvis_dm_property_t,
                                    rocprofvis_dm_property_index_t); 
'''  
---
'''  
  **Return property value as char**
'''  
 
  'param' handle any object handle
  'param' property enumeration of properties for specified handle type
  'param' index index of any indexed property
 
  'return' char value
 
'''  
char   rocprofvis_dm_get_property_as_charptr(
                                    rocprofvis_dm_handle_t, 	
                                    rocprofvis_dm_property_t,
                                    rocprofvis_dm_property_index_t); 
'''  
---
'''  
  **Return property value as rocprofvis_dm_handle_t**
'''  
 
  'param' handle any object handle
  'param' property enumeration of properties for specified handle type
  'param' index index of any indexed property
 
  'return' rocprofvis_dm_handle_t value
 

'''  
rocprofvis_dm_handle_t  rocprofvis_dm_get_property_as_handle(
                                    rocprofvis_dm_handle_t, 	
                                    rocprofvis_dm_property_t,
                                    rocprofvis_dm_property_index_t); 
'''  
---
                          