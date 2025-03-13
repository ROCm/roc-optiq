typedef void* TraceHandler;
typedef void* TrackHandler;
typedef void* DatabaseHandler;
typedef void* FlowHandler;
typedef void* RecordArrayHandler;

#define INVALID_RECORD_INDEX 0xFFFFFFFF

TraceHandler        create_trace();                                                         // Creates trace object
int                 destroy_trace(TraceHandler);                                            // Deletes trace object with provided TraceHandler
int                 bind_trace_to_database(TraceHandler, DatabaseHandler);                  // Binds trace and database with provided TraceHandler and DatabaseHandler
uint64_t            get_trace_min_time(TraceHandler);                                       // Returns minimum trace timestamp for TraceHandler
uint64_t            get_trace_max_time(TraceHandler);                                       // returns maximum trace timestamp for TraceHandler
uint32_t            get_number_of_tracks(TraceHandler);                                     // returns number of tracks in trace 
void                delete_trace_chunks_at(TraceHandler, uint64_t);                         // delete trace chunks marked with timestamp 
TrackHandler        get_track_at(TraceHandler, uint32_t);                                    // return track handler for provided index
DatabaseHandler     get_database_handler(TraceHandler);                                     // returns database handler bound to the trace

const char*         get_track_group_name(TrackHandler);                                     // returns name of group of tracks. Can be used for tracks grouping into a tree
const char*         get_track_name(TrackHandler);                                           // returns track name
int                 is_data_type_metric(TrackHandler);                                      // returns true if track contains metrics
int                 is_data_type_kernel_launch(TrackHandler);                               // returns true for CPU kenel launch track
int                 is_data_type_kernel_execute(TrackHandler);                              // returns true for GPU hernel execution track
RecordArrayHandler  get_track_last_acquaired_chunk(TrackHandler);                           // returns last aquired chunk. If chunk is meant to be used once, it needs to be deleted after been used
RecordArrayHandler  get_track_chunk_at_time(TrackHandler, uint64_t);                        // return chunck marked with specific timestamp
FlowHandler         get_flow_handler_at(TrackHandler, uint64_t);                            // return flow handler for specified event id
uint32_t            get_number_of_chunks(TrackHandler);                                     // return number of chunks for specified track
int                 validate_track_handler_for_trace(TraceHandler, TrackHandler);           // return true if track belongs to trace

uint32_t            get_record_index_at_timestamp(RecordArrayHandler, uint64_t);            // converts timestamp to index of the record in specified record array (chunk) 
uint64_t            get_chunk_number_of_records(RecordArrayHandler);                        // return number of records in a chunk
uint64_t            get_timestamp_at(RecordArrayHandler, uint32_t);                     // returns timestamp of a record
double              get_metric_value_at(RecordArrayHandler, uint32_t);                  // return value of a metric record
uint64_t            get_kernel_id_at(RecordArrayHandler array, uint32_t index);         // returns ID of kernel event record
long long           get_kernel_duration_at(RecordArrayHandler, uint32_t);               // returns duration of kernel event
const char*         get_kernel_type_at(RecordArrayHandler, uint32_t);                   // returns type string of kernel event
const char*         get_kernel_description_at(RecordArrayHandler, uint32_t);            // returns description string of kernel event 

uint16_t            get_flow_endpoint_track_id(FlowHandler);                                // returns data flow endpoint track Id
uint64_t            get_flow_endpoint_timestamp(FlowHandler);                               // returns data flow endpoint timestamp
uint64_t            get_flow_endpoint_info(FlowHandler);                                    // returns data flow endpoint last timestamp for GPU endpoint (HipGraph case) or endpoint ID for CPU endpoint

// statiscial methods
uint64_t            get_flow_array_memory_footprint(TrackHandler);                          // returns amount of memory occupied by dataflow array
uint64_t            get_string_array_memory_footprint(TraceHandler);                        // returns amount of memory occupied by string array
uint64_t            get_track_memory_footprint(TrackHandler);                               // returns amount of memory occupied by track
uint64_t            get_chunk_memory_footprint(RecordArrayHandler);                         // returns amount of memory occupied chunk
uint32_t            get_string_item_count(TraceHandler);                                    // returns number of strings in string array
uint32_t            get_number_of_flow_records(TrackHandler);                               // returns number of flow records
