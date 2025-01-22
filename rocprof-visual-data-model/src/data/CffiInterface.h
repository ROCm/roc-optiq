typedef void* TraceHandler;
typedef void* TrackHandler;
typedef void* DatabaseHandler;
typedef void* FlowHandler;
typedef void* RecordArrayHandler;

#define INVALID_RECORD_INDEX 0xFFFFFFFF

TraceHandler        create_trace();                                                         // Creates trace object
int                 destroy_trace(TraceHandler);                                            // Deletes trace object with provided TraceHandler
int                 bind_trace_to_database(TraceHandler, DatabaseHandler);                  // Binds trace and database with provided TraceHandler and DatabaseHandler
unsigned long long  get_trace_min_time(TraceHandler);                                       // Returns minimum trace timestamp for TraceHandler
unsigned long long  get_trace_max_time(TraceHandler);                                       // returns maximum trace timestamp for TraceHandler
unsigned int        get_number_of_tracks(TraceHandler);                                     // returns number of tracks in trace 
void                delete_trace_chunks_at(TraceHandler, unsigned long long);               // delete trace chunks marked with timestamp 
TrackHandler        get_track_at(TraceHandler, unsigned int);                               // return track handler for provided index
DatabaseHandler     get_database_handler(TraceHandler);                                     // returns database handler bound to the trace

const char*         get_track_group_name(TrackHandler);                                     // returns name of group of tracks. Can be used for tracks grouping into a tree
const char*         get_track_name(TrackHandler);                                           // returns track name
int                 is_data_type_metric(TrackHandler);                                      // returns true if track contains metrics
int                 is_data_type_kernel_launch(TrackHandler);                               // returns true for CPU kenel launch track
int                 is_data_type_kernel_execute(TrackHandler);                              // returns true for GPU hernel execution track
RecordArrayHandler  get_track_last_acquaired_chunk(TrackHandler);                           // returns last aquired chunk. If chunk is meant to be used once, it needs to be deleted after been used
RecordArrayHandler  get_track_chunk_at_time(TrackHandler, unsigned long long);              // return chunck marked with specific timestamp
FlowHandler         get_flow_handler_at(TrackHandler, unsigned long long);                  // return flow handler for specified event id
unsigned int        get_number_of_chunks(TrackHandler);                                     // return number of chunks for specified track
int                 validate_track_handler_for_trace(TraceHandler, TrackHandler);           // return true if track belongs to trace

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
