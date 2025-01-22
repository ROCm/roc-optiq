   #include "TraceInterface.h"
   #include "Trace.h"
   #include "MetricRecord.h"
   #include "EventRecord.h"
   #include <cassert>

const char* traceAndDatabaseContextCannotBeNull = "Trace and database context cannot be NULL";
const char* traceContextCannotBeNull = "Trace context cannot be NULL";
const char* trackContextCannotBeNull = "Track context cannot be NULL";
const char* flowContextCannotBeNull  = "Flow data context cannot be NULL";
const char* arrayContextCannotBeNull = "Record array context cannot be NULL";

    TraceHandler  create_trace()
    {
        return new Trace();
    }

    int  destroy_trace(TraceHandler trace)
    {
        if (trace != NULL)
        {
            delete trace;
            return true;
        }
        return false;
    }

    int  bind_trace_to_database(TraceHandler trace, DatabaseHandler database)
    {
        assert(trace && database && traceAndDatabaseContextCannotBeNull);
        return ((Trace*)trace)->bindDatabase((Database*)database); 
    }

    DatabaseHandler get_database_handler(TraceHandler trace)
    {
        assert(trace &&  traceContextCannotBeNull);
        return ((Trace*)trace)->getDbHandler();       
    }

    unsigned long long  get_trace_min_time(TraceHandler trace)
    {
        assert(trace &&  traceContextCannotBeNull);
        return ((Trace*)trace)->getMinTime();
    }

    unsigned long long  get_trace_max_time(TraceHandler trace)
    {
        assert(trace &&  traceContextCannotBeNull);
        return ((Trace*)trace)->getMaxTime();
    }

    unsigned int  get_number_of_tracks(TraceHandler trace)
    {
        assert(trace &&  traceContextCannotBeNull);
        return (unsigned int)((Trace*)trace)->getNumberOfTracks();
    }

    unsigned long long  get_string_array_memory_footprint(TraceHandler trace)
    {
        assert(trace &&  traceContextCannotBeNull);
        return (unsigned long long)((Trace*)trace)->getStringArrayMemoryFootprint();      
    }

    unsigned int  get_string_item_count(TraceHandler trace)
    {
        assert(trace &&  traceContextCannotBeNull);
        return (unsigned int)((Trace*)trace)->getStringItemCount();      
    }

    void delete_trace_chunks_at(TraceHandler trace, unsigned long long timestamp)
    {
         assert(trace &&  traceContextCannotBeNull);
         ((Trace*)trace)->deleteTraceChunksAt(timestamp);
    }

    int  validate_track_handler_for_trace(TraceHandler trace, TrackHandler track)
    {
        assert(trace &&  traceContextCannotBeNull);
        assert(track &&  trackContextCannotBeNull);
        return ((Trace*)trace)->validateTrackHandler(track);
    }

    TrackHandler   get_track_at(TraceHandler trace, unsigned int index)
    {
        assert(trace &&  traceContextCannotBeNull);
        return ((Trace*)trace)->getTrackAt(index);
    }
  
    unsigned long long  get_track_memory_footprint(TrackHandler track)
    {
        assert(track &&  trackContextCannotBeNull);
        return ((Track*)track)->getTrackMemoryFootprint();
    }

    const char*  get_track_group_name(TrackHandler track)
    {
        assert(track &&  trackContextCannotBeNull);
        return ((Track*)track)->getGroupName();
    }

    const char*  get_track_name(TrackHandler track)
    {
        assert(track &&  trackContextCannotBeNull);
        return ((Track*)track)->getName();
    }

    int is_data_type_metric(TrackHandler track)
    {
        assert(track &&  trackContextCannotBeNull);
        return ((Track*)track)->getType() == TrackType::METRICS;
    }

    int is_data_type_kernel_launch(TrackHandler track)
    {
        assert(track &&  trackContextCannotBeNull);
        return ((Track*)track)->getType() == TrackType::CPU;
    }

    int is_data_type_kernel_execute(TrackHandler track)
    {
        assert(track &&  trackContextCannotBeNull);
        return ((Track*)track)->getType() == TrackType::GPU;
    }

    unsigned int  get_number_of_chunks(TrackHandler track)
    {
        assert(track &&  trackContextCannotBeNull);
        return (unsigned int)((Track*)track)->getNumberOfChunks();
    }

    RecordArrayHandler get_track_last_acquaired_chunk(TrackHandler track)
    {
        assert(track &&  trackContextCannotBeNull);
        return ((Track*)track)->getTrackLastChunk();
    }

    RecordArrayHandler get_track_chunk_at_time(TrackHandler track, unsigned long long timestamp)
    {
        assert(track &&  trackContextCannotBeNull);
        return ((Track*)track)->getTrackArrayAtTimestamp(timestamp);
    }

    unsigned int get_record_index_at_timestamp(RecordArrayHandler array, unsigned long long timestamp)
    {
        assert(array &&  arrayContextCannotBeNull);
        return ((TrackArray*)array)->timestampToIndex(timestamp);
    }

    unsigned long long get_chunk_memory_footprint(RecordArrayHandler array)
    {
        assert(array &&  arrayContextCannotBeNull);
        return ((TrackArray*)array)->getRecordArrayMemoryFootprint();
    }

    unsigned long long get_chunk_number_of_records(RecordArrayHandler array)
    {
        assert(array &&  arrayContextCannotBeNull);
        return ((TrackArray*)array)->getNumberOfRecords();
    }


    unsigned long long  get_timestamp_at(RecordArrayHandler array, unsigned int index)
    {
        assert(array &&  arrayContextCannotBeNull);
        return ((TrackArray*)array)->getRecordTimestampAt(index);     
    }

    double  get_metric_value_at(RecordArrayHandler array, unsigned int index)
    {
        assert(array &&  arrayContextCannotBeNull);
        return ((TrackArray*)array)->getRecordValueAt(index);    
    }

    unsigned long long  get_kernel_id_at(RecordArrayHandler array, unsigned int index)
    {
        assert(array &&  arrayContextCannotBeNull);
        return ((TrackArray*)array)->getRecordIdAt(index);   
    }

    long long  get_kernel_duration_at(RecordArrayHandler array, unsigned int index)
    {
        assert(array &&  arrayContextCannotBeNull);
        return ((TrackArray*)array)->getRecordDurationAt(index);   
    }

    const char*  get_kernel_type_at(RecordArrayHandler array, unsigned int index)
    {
        assert(array &&  arrayContextCannotBeNull);
        return ((TrackArray*)array)->getRecordTypeNameAt(index);   
    }

    const char*  get_kernel_description_at(RecordArrayHandler array, unsigned int index)
    {
        assert(array &&  arrayContextCannotBeNull);
        return ((TrackArray*)array)->getRecordDescriptionAt(index);   
    }

    unsigned int  get_number_of_flow_records(TrackHandler track)
    {
        assert(track && trackContextCannotBeNull);
        return (unsigned int)((Track*)track)->getNumberOfFlowRecords();
    }

    unsigned long long  get_flow_array_memory_footprint(TrackHandler track)
    {
        assert(track && trackContextCannotBeNull);
        return ((Track*)track)->getFlowArrayMemoryFootprint();
    }

    FlowHandler  get_flow_handler_at(TrackHandler track, unsigned long long id)
    {
        assert(track && trackContextCannotBeNull);
        return ((Track*)track)->getFlowRecordAt(id);
    }

    unsigned short  get_flow_endpoint_track_id(FlowHandler flow)
    {
        assert(flow && flowContextCannotBeNull);
        return ((DataFlowRecord*)flow)->getTrackId();
    }

    unsigned long long get_flow_endpoint_timestamp(FlowHandler flow)
    {
        assert(flow && flowContextCannotBeNull);
        return ((DataFlowRecord*)flow)->getEndpointTimeStamp();
    }

    unsigned long long get_flow_endpoint_info(FlowHandler flow)
    {
        assert(flow && flowContextCannotBeNull);
        return ((DataFlowRecord*)flow)->getEndpointInfo().value;
    }


