This project implements a data model for profiling data based on existing Rocpd profiler schema. Will require changes
when new Rocprof-system output data schema is delivered.
Prototyped in C++ using STL, with pure C interface compilable to Python library using CFFI. POD (plain old data)
classes are used for data records for better memory footprint. More object oriented features are used in tracks and
chunks management.

Data Model includes following classes:
    MetricRecord - POD class holding metric data parameters
    EventRecord - POD class holding event data parameters (indexes to string collection replaces actual strings)
    TrackArray - virtual base class for MetricArray and EventArray. Reference type for data record getter interface functions.
        Object of TrackArray derivatives represent a chunk entity. 
    MetricArray - owner of vector<MetricRecord> array. Inherits TrackArray. 
    EventArray - owner of vector<EventRecord> array. Inherits TrackArray.
    Track - virtual base class for MetricTrack, CpuTrack, GpuTrack, [more in future] classes for track and chunk management.
        Owner of vector<TrackArray*> array
    EventTrack - base class for CpuTrack and GpuTrack objects. Inherits Track class. Owner of map<DataFlowRecord> map array. 
        Map array is used for faster data flow record search by 64 bit ID. 
    MetricTrack - class based on Track class. Inherits Track class. Describes Metric data track object.
    CpuTrack - describes Cpu data related track object. Inherits Event Track class.
    GpuTrack - describes Gpu data related track object. Inherits Event Track class.
    DataFlowRecord - contains information about data flow endpoints. Both GPU and CPU tracks have this information. CPU for 
        outgoing connection, GPU for incoming. The specifics of outgoing connection is it can be one to multiple data flow type,
        example is HipGraph object. On GPU side it's always one to one incoming flow. HipGraph on GPU side will have all neighboring 
        events from timeA to timeB pointing to the same HipGraph object. Data flow management may need further thoughts and optimization.
    Trace and TraceInternal - was separated to hide direct access to std::vector<std::unique_ptr<Track>>, which cannot be private 
        because it needs to accessed from static methods, which are part of binding interface between trace and database.

Almost all the data requested by higher level algorithms will be dispatched through TrackHandler, which has knowledge about type
of data this track holds and keeps collection of data chunks. Data chunks are marked with start and end timestamp and hold
array of records of specific type.

Database class and it's dervatives are kinda outside of data model, but it was needed to load the data model with data.
    Database is a virtual base class of all database objects, including SqliteDatabase. 
    SqliteDatabase provide methods common for all Sqlite databases, 
    RocpdDatabase derrives from SqliteDatabase and provides methods specific to Rocpd database schema.

The interface methods accesses data model using handlers. The methods pass only C types. No structures are passed or returned.
This elimitates hassles aligning data in data structures when passed from one language to another, as well as 
keeping multiple versions of the same structure for backward compatibility.
The interface mathods are tested by being called from C++ code (main.cpp) and Python code (cffi_invoke.py) using CFFI. 
cffi_build.py script is used to build cffi_lib.xxxx.pyd python module.
Here is a block diagram of initial data model version:
                                                                                                                                                                                                                                        
![alt text](https://github.amd.com/AGS-ROCm-CI/rocprofiler-visualizer/blob/vstempen/rocprof-visual-data-model/diagram.PNG)

The test code implemented in main.cpp and cffi_invoke.py creates Trace and Database objects, binds them by providing set of 
callbacks to store the records, strings, dataflow links and some miscellaneous data to the data model. 
After database properties are loaded, the test application requests number of chunks user wants to split the trace horizontally and 
vertically. Then chunks of the data are being read from the database and recorded time differences are displayed, following by amout of 
records acquiared per track.   

Previous version of main.cpp also had code to test data access. It used to run through all the tracks displaying data from random record.
It also searched for random event records which have flow data and displays source and destination of the data flow.
This is still to be implemented in the current version.

So far the code was only built in Windows environment. 
To build the project run from the project root:

unzip .\src\database\sqlite\sqlite3.zip - the file is bigger that 5MB, cannot submit directly
mkdir build
cd build
cmake ..

At this point Microsoft Visual C projects and solution will be generated in build directory. 
Build solution with MSBuild tools or within Visual studio IDE.

For CFFI, first build library project in Visual Studio, the go to the project's src/python/ directory and run:
python cffi_build.py

After build is finished, run 
python cffi_invoke.py [database full path]

For windows executable, build executable project in Visual Studio, then go to build/Release folder and run executable 
with [database full path] as a parameter


The rocpd database class has two methods to read the code. First reads time chunk per track, second reads all tracks/tables within 
time frame. Second way is faster if we split horizontally, and actually faster overall, but it has overhead processing absolutely all 
tracks and tables for specified timeframe, even when those are not requested. First way seems slower because we use track parameters in SQL
query to select desired track.
After running some tests, I see following results: 
For the whole trace data records loading takes 26 seconds using method 1
For the whole trace data records loading takes 11 seconds using method 2
10x1 - 1.3 seconds per chunk, 13 seconds overall
20x1 = 0.9 seconds per chunk, 18 seconds overall
5x5 = 2 seconds per chunk, 52 seconds overall
1x10 - uisng method 1, different times per chunk, 17s overall, with heaviest track taking about 0.7s 

There are still room for optimization SQL queries, but probably the best would be serializing/deserializing loaded trace to/from file system. 
This may be another test for the data model design process. 

Interface functions:

typedef void* TraceHandler;
typedef void* TrackHandler;
typedef void* DatabaseHandler;
typedef void* FlowHandler;
typedef void* RecordArrayHandler;
typedef void* DBHandler;

TraceHandler        create_trace();                                                         // Creates trace object
int                 destroy_trace(TraceHandler);                                            // Deletes trace object with provided TraceHandler
int                 bind_trace_to_database(TraceHandler, DatabaseHandler);                  // Binds trace and database with provided TraceHandler and DatabaseHandler
unsigned long long  get_trace_min_time(TraceHandler);                                       // Returns minimum trace timestamp for TraceHandler
unsigned long long  get_trace_max_time(TraceHandler);                                       // returns maximum trace timestamp for TraceHandler
unsigned int        get_number_of_tracks(TraceHandler);                                     // returns number of tracks in trace 
void                delete_trace_chunks_at(TraceHandler, unsigned long long);               // delete trace chunks marked with timestamp 
TrackHandler        get_track_at(TraceHandler, unsigned int);                               // return track handler for provided index

const char*         get_track_group_name(TrackHandler);                                     // returns name of group of tracks. Can be used for tracks grouping into a tree
const char*         get_track_name(TrackHandler);                                           // returns track name
int                 is_data_type_metric(TrackHandler);                                      // returns true if track contains metrics
int                 is_data_type_kernel_launch(TrackHandler);                               // returns true for CPU kenel launch track
int                 is_data_type_kernel_execute(TrackHandler);                              // returns true for GPU hernel execution track
RecordArrayHandler  get_track_last_acquaired_chunk(TrackHandler);                           // returns last aquired chunk. If chunk is meant to be used once, it needs to be deleted after been used
RecordArrayHandler  get_track_chunk_at_time(TrackHandler, unsigned long long);              // return chunck marked with specific timestamp
FlowHandler         get_flow_handler_at(TrackHandler, unsigned long long);                  // return flow handler for specified event id
unsigned int        get_number_of_chunks(TrackHandler);                                     // return number of chunks for specified track

unsigned int        get_record_index_at_timestamp(RecordArrayHandler, unsigned long long);  // converts timestamp to index of the record in specified record array (chunk) 
unsigned long long  get_chunk_number_of_records(RecordArrayHandler);                        // return number of records in a chunk
unsigned long long  get_timestamp_at(RecordArrayHandler, unsigned int);                     // returns timestamp of a record
double              get_metric_value_at(RecordArrayHandler, unsigned int);                  // return value of a metric record
unsigned long long  get_kernel_id_at(RecordArrayHandler array, unsigned int index);         // returns ID of kernel event record
long long           get_kernel_duration_at(RecordArrayHandler, unsigned int);               // returns duration of kernel event
const char*         get_kernel_type_at(RecordArrayHandler, unsigned int);                   // returns type string of kernel event
const char*         get_kernel_description_at(RecordArrayHandler, unsigned int);            // returns description string of kernel event 

unsigned short      get_flow_endpoint_track_id(FlowHandler);                                // returns data flow endpoint track Id
unsigned long long  get_flow_endpoint_timestamp(FlowHandler);                               // returns data flow endpoint timestamp
unsigned long long  get_flow_endpoint_info(FlowHandler);                                    // returns data flow endpoint last timestamp for GPU endpoint (HipGraph case) or endpoint ID for CPU endpoint

// statiscial methods
unsigned long long  get_flow_array_memory_footprint(TrackHandler);                          // returns amount of memory occupied by dataflow array
unsigned long long  get_string_array_memory_footprint(TraceHandler);                        // returns amount of memory occupied by string array
unsigned long long  get_track_memory_footprint(TrackHandler);                               // returns amount of memory occupied by track
unsigned long long  get_chunk_memory_footprint(RecordArrayHandler);                         // returns amount of memory occupied chunk
unsigned int        get_string_item_count(TraceHandler);                                    // returns number of strings in string array
unsigned int        get_number_of_flow_records(TrackHandler);                               // returns number of flow records

// database methods

void*   open_rocpd_database(const char*);                                                       // open database with given path
int     read_trace_properties(DBHandler, DBReadProgress);                                       // read trace properties
int     close_rocpd_database(DBHandler);                                                        // close database
int     read_trace_chunk_track_by_track(DBHandler, DBReadProgress);                             // read trace chuck running separate SQL query for every track
int     read_trace_chunk_all_tracks(DBHandler db, DBReadProgress);                              // read trace chunk for all tracks for configured time frame
void    configure_trace_read_time_period(DBHandler, unsigned long long, unsigned long long);    // configure trace chunk read timeframe
void    add_track_to_trace_read_config(DBHandler, unsigned short);                              // add track to trace chunk read configuration 
                