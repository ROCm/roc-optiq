#include "Interface.h"

void db_progress ( rocprofvis_dm_database_t db, rocprofvis_db_read_progress_t progress, rocprofvis_db_status_t status, db_status_message_t msg)
{
    std::cout << "\rLoaded : " << progress << "% - " << msg << std::string(20,' ') << std::flush;
}

int main()
{
    rocprofvis_dm_trace_handle_t trace = rocprofvis_dm_create_trace();
    if (nullptr != trace) 
    {
        rocprofvis_dm_database_t db = rocprofvis_db_open_database(“trace.rpd”, kRocpdSqlite, db_progress);
        if (nullptr != db && kRocProfVisResultSuccess == rocprofvis_bind_trace_to_database(trace, db))
        {
            rocprofvis_db_future_t object2wait = rocprofvis_db_future_alloc();
            if (nullptr != object) 
            {
                if (kRocProfVisResultSuccess == rocprofvis_db_read_trace_metadata_async(db, object2wait))
                {
	                if (kRocProfVisResultSuccess == rocprofvis_db_future_wait(object, 10000))
	                {          
	                    uint64_t startTime = rocprofvis_dm_get_property_as_uint64(trace, kRPVDMStartTimeUInt64, 0);
                        printf(“Trace start time=%lld\n”, startTime);
                        uint64_t numTracks = rocprofvis_dm_get_property_as_uint64(trace, kRPVDMNumberOfTracksUInt64, 0);
                        for (int i=0; i < numTracks; i++)
                        {
                            rocprofvis_dm_track_handle_t track = get_property_as_handler(trace, kRPVDMTrackObjectHandleIndexed, i);
                            printf(“Track type = %lld\n”, rocprofvis_dm_get_property_as_uint64(track, kRPVDMTrackType,0));
                        }
	                    const char* tracks = “1,2,3,4,5,50”; 
	                    if (kRocProfVisResultSuccess == rocprofvis_db_read_trace_slice(db, startTime, startTime+100000, tracks, object2wait))
	                    {
	                        if (kRocProfVisResultSuccess == rocprofvis_db_future_wait(object, 1.0))
                            {
                                const int trackIndex = 1; 
                                rocprofvis_dm_track_handle_t track = rocprofvis_dm_get_property_as_handler(trace, kTrackHandleIndexed, trackIndex);
                                rocprofvis_dm_slice_t slice = rocprofvis_dm_get_property_as_handler (track, kRPVDMSliceHandleIndexed, startTime);
                                if (nullptr != slice)
                                {
                                    uint64_t numRecords = rocprofvis_dm_get_property_as_uint64(track, kRPVDMNumberOfRecordsUInt64,0);
                                    printf(“Time slice for time %lld-%lld for track  %d has %lld records\n”, startTime, startTime+100000, trackIndex, numRecords);
                                }
                            }
                        }
                    }	
	            }
            }
            rocprofvis_db_future_free(object);
        }
    }		
    delete_trace(trace);
}
