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



Database interface
---

  **Opens database of provided path and type**<br />
    kAutodetect = 0,<br />
 	kRocpdSqlite = 1, <br />
 	kRocprofSqlite = 2 <br />
  
  `param` filename path of the database file <br />
  `param` type  type enumeration, kAutodetect for automatic detection  <br />
  `return` handler to database object <br />
  `note` Currently only old rocpd schema fully supported. Working on rocprof schema <br />
  
``` 
rocprofvis_dm_database_t rocprofvis_db_open_database(
                                    rocprofvis_db_filename_t, 
                                    rocprofvis_db_type_t);
```

---


  **Calculates size of memory used by database object**<br />
  
  `param` database database handle<br />
  `return` size of used memory<br />
  
```
rocprofvis_dm_size_t rocprofvis_db_get_memory_footprint(
                                    rocprofvis_dm_database_t);
```

---


  **Allocates future object, to be used for asynchronous operations**<br />
  
  `param` callback callback method to report current database request progress and status. May be useful for command line tools and scripts<br />
  `return` future object handle<br />
  
```  
rocprofvis_db_future_t rocprofvis_db_future_alloc(
                                    rocprofvis_db_progress_callback_t );
```

---

  
  **Waits until asynchronous operation is completed or timeout expires**<br />
  
  `param` object future handle allocated by rocprofvis_db_future_alloc<br />
  `param` timeout timeout in seconds for the asynchronous call to expire<br />
  `return` status of operation<br />
  
  
```  
rocprofvis_dm_result_t rocprofvis_db_future_wait(
                                    rocprofvis_db_future_t, 
                                    rocprofvis_db_timeout_sec_t);
```  

---
  
  **Frees future object**<br />
  
  
  `param` object future handle allocated by rocprofvis_db_future_alloc<br />
  
```
void rocprofvis_db_future_free(rocprofvis_db_future_t);
```

---
  
  **Asynchronous call to read data model metadata** <br />
               (static objects residing in trace class memory until trace is deleted)  <br />
  
  `param` database database handle<br />
  `param` object future handle allocated by rocprofvis_db_future_alloc<br />
  `return` status of operation<br />
  
```  
rocprofvis_dm_result_t rocprofvis_db_read_metadata_async(
                                    rocprofvis_dm_database_t, 
                                    rocprofvis_db_future_t);
```  

---

  **Asynchronous call to read time slice of records for provided time frame and tracks selection**<br />
  
 
  `param` database database handle<br />
  `param` start beginning of the time slice<br />
  `param` start end of the time slice<br />
  `param` num number of tracks in rocprofvis_db_track_selection_t array (uint32)<br />
  `param` track tracks selection array (uint32)<br />
  `param` object future handle allocated by rocprofvis_db_future_alloc<br />
  `return` status of operation<br />
 
  `note` Object will stay in trace memory until deleted.<br />
              Use rocprofvis_dm_delete_time_slice or rocprofvis_dm_delete_all_time_slices for deletion <br />

```  
rocprofvis_dm_result_t rocprofvis_db_read_trace_slice_async( 
                                    rocprofvis_dm_database_t,
                                    rocprofvis_dm_timestamp_t,
                                    rocprofvis_dm_timestamp_t,
                                    rocprofvis_db_num_of_tracks_t,
                                    rocprofvis_db_track_selection_t,
                                    rocprofvis_db_future_t);                              
```  

---
  
  **Asynchronous call to read event property of specific type**  <br />
 
  `param` database database handle<br />
  `param` type type of property:<br />
                              kRPVDMEventFlowTrace,<br />
                              kRPVDMEventStackTrace,<br />
                              kRPVDMEventExtData,<br />
  `param` event_id 60-bit event id and 4-bit operation type<br />
  `param` object future handle allocated by rocprofvis_db_future_alloc<br />
  `return` status of operation<br />
 
  `note` Object will stay in trace memory until deleted.<br />
           Use rocprofvis_dm_delete_event_property_for or<br />
           rocprofvis_dm_delete_all_event_properties_for for deletion<br />
```  
rocprofvis_dm_result_t  rocprofvis_db_read_event_property_async(
                                    rocprofvis_dm_database_t,
                                    rocprofvis_dm_event_property_type_t,
                                    rocprofvis_dm_event_id_t,
                                    rocprofvis_db_future_t);
```  

---
  
  **Asynchronous call to read a table result of specified SQL query**  <br />
 
  `param` database database handle<br />
  `param` query SQL query string<br />
  `param` description description of a table<br />
  `param` object future handle allocated by rocprofvis_db_future_alloc<br />
  `return` status of operation<br />
 
  `note` Object will stay in trace memory until deleted.<br />
           Use rocprofvis_dm_delete_table_at or rocprofvis_dm_delete_all_tables for deletion<br />
 
```  
rocprofvis_dm_result_t  rocprofvis_db_execute_query_async(
                                    rocprofvis_dm_database_t,                                                                
                                    rocprofvis_dm_charptr_t,
                                    rocprofvis_dm_charptr_t,
                                    rocprofvis_db_future_t);
```  

Data model interface
---

  
  **Create trace object**  <br />
 
  `return` trace object handle<br />
 
  `note` trace object needs to bound to a database and database interface methods should be called to fill trace with data, starting with rocprofvis_db_read_metadata_async<br />

```  
rocprofvis_dm_trace_t   rocprofvis_dm_create_trace(void); 
```  

---

  **Deleting trace and all its resources including bound database**  <br />
 
  `param` trace trace object handle<br />
 
  `return` status of operation<br />
 
```  
rocprofvis_dm_result_t  rocprofvis_dm_delete_trace( 
                                    rocprofvis_dm_trace_t);   
```  

---

  **Binds trace to database**  <br />
 
  `param` trace trace object handle created with rocprofvis_dm_create_trace()<br />
  `param` database database object handle created with rocprofvis_db_open_database()<br />
 
  `return` status of operation<br />
 
```  
rocprofvis_dm_result_t  rocprofvis_dm_bind_trace_to_database( 
                                    rocprofvis_dm_trace_t,
                                    rocprofvis_dm_database_t);                                      
```  

---
 
  **Delete time slice with specified start and end timestamps**<br />
 
  `param` trace trace object handle created with rocprofvis_dm_create_trace()<br />
  `param` start time slice start timestamp<br />
  `param` end time slice end timestamp<br />
 
  `return` status of operation<br />
 
```  
rocprofvis_dm_result_t  rocprofvis_dm_delete_time_slice( 
                                    rocprofvis_dm_trace_t,
                                    rocprofvis_dm_timestamp_t,
                                    rocprofvis_dm_timestamp_t);     
```  

---
  
  **Delete all time slices**  <br />
 
  `param` trace trace object handle created with rocprofvis_dm_create_trace()<br />
  `return` status of operation<br />
 
```  
rocprofvis_dm_result_t  rocprofvis_dm_delete_all_time_slices( 
                                    rocprofvis_dm_trace_t);                                      
```  

---

  **Delete event property object of specified type**  <br />
 
  `param` trace trace object handle created with rocprofvis_dm_create_trace()<br />
  `param` type type of property<br />
                              kRPVDMEventFlowTrace,<br />
                              kRPVDMEventStackTrace,<br />
                              kRPVDMEventExtData,<br />
  `param` event_id 60-bit event id and 4-bit operation type<br />
 
  `return` status of operation<br />
 
```  
rocprofvis_dm_result_t  rocprofvis_dm_delete_event_property_for( 
                                    rocprofvis_dm_trace_t,
                                    rocprofvis_dm_event_property_type_t,
                                    rocprofvis_dm_event_id_t);     
```  

---

  **Delete all event property objects of specified type**  <br />
 
  `param` trace trace object handle created with rocprofvis_dm_create_trace()<br />
  `param` type type of property<br />
                              kRPVDMEventFlowTrace,<br />
                              kRPVDMEventStackTrace,<br />
                              kRPVDMEventExtData,<br />
 
  `return` status of operation<br />
 
```  
rocprofvis_dm_result_t  rocprofvis_dm_delete_all_event_properties_for( 
                                    rocprofvis_dm_trace_t,
                                    rocprofvis_dm_event_property_type_t);        
``` 

---

  **Delete a table by specified table index**<br />
        Table is created when executed SQL query using rocprofvis_db_execute_query_async method  <br />
 
  `param` trace trace object handle created with rocprofvis_dm_create_trace()<br />
  `param` index index of a table. Number of existing tables can queried by reading Trace object property kRPVDMNumberOfTablesUInt64<br />
 
  `return` status of operation<br />
 
```  
rocprofvis_dm_result_t  rocprofvis_dm_delete_table_at( 
                                    rocprofvis_dm_trace_t,
                                    rocprofvis_dm_index_t); 
```  

---

  **Delete all tables**<br />
         Tables are created when executed SQL queries using rocprofvis_db_execute_query_async method  <br />
 
  `param` trace trace object handle created with rocprofvis_dm_create_trace()<br />
  `return` status of operation<br />
 
```  
rocprofvis_dm_result_t  rocprofvis_dm_delete_all_tables( 
                                    rocprofvis_dm_trace_t);  
```  

Universal property getters
---

 There are 10 property getter methods. Two methods to get property of each type:<br />
       uint64, int64, double, char and rocprofvis_dm_handle_t<br />
 First set of methods returns values as pointer references and result of operation as return value<br />
 Second set returns property values directly. No result of operation is returned.<br />
 In second case if operation fails, 0 or nullptr will be returned.<br />

---
 
  **Return property value as uint64**  <br />
 
  `param` handle any object handle<br />
  `param` property enumeration of properties for specified handle type<br />
  `param` index index of any indexed property<br />
  `return` uint64_t value<br />
 
```  
uint64_t  rocprofvis_dm_get_property_as_uint64(
                                    rocprofvis_dm_handle_t, 	
                                    rocprofvis_dm_property_t,
                                    rocprofvis_dm_property_index_t);                                      
``` 

---
  
  **Return property value as int64**  <br />
 
  `param` handle any object handle<br />
  `param` property enumeration of properties for specified handle type<br />
  `param` index index of any indexed property<br />
  `return` int64_t value<br />
 
```  
int64_t   rocprofvis_dm_get_property_as_int64(
                                    rocprofvis_dm_handle_t, 	
                                    rocprofvis_dm_property_t,
                                    rocprofvis_dm_property_index_t); 
``` 

---

  **Return property value as double**  <br />
 
  `param` handle any object handle<br />
  `param` property enumeration of properties for specified handle type<br />
  `param` index index of any indexed property<br />
  `return` double value<br />
 
```  
double  rocprofvis_dm_get_property_as_double(
                                    rocprofvis_dm_handle_t, 	
                                    rocprofvis_dm_property_t,
                                    rocprofvis_dm_property_index_t); 
```  

---
  
  **Return property value as char**  <br />
 
  `param` handle any object handle<br />
  `param` property enumeration of properties for specified handle type<br />
  `param` index index of any indexed property<br />
  `return` char value<br />
 
```  
char   rocprofvis_dm_get_property_as_charptr(
                                    rocprofvis_dm_handle_t, 	
                                    rocprofvis_dm_property_t,
                                    rocprofvis_dm_property_index_t); 
```  

---

  **Return property value as rocprofvis_dm_handle_t**  <br />
 
  `param` handle any object handle<br />
  `param` property enumeration of properties for specified handle type<br />
  `param` index index of any indexed property<br />
  `return` rocprofvis_dm_handle_t value<br />
 

```  
rocprofvis_dm_handle_t  rocprofvis_dm_get_property_as_handle(
                                    rocprofvis_dm_handle_t, 	
                                    rocprofvis_dm_property_t,
                                    rocprofvis_dm_property_index_t); 
```  

---
                          