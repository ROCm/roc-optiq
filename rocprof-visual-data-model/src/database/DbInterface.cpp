#include "Database.h"
#include "DbInterface.h"
#include "sqlite/rocpd/RocpdDatabase.h"
#include <cassert>

int read_trace_properties(DBHandler db, DBReadProgress progressCallback)
{
    assert(db && "Database context cannot be NULL");
    ((Database*)db)->readTraceProperties(progressCallback);
    return true; 
}

int read_trace_chunk_track_by_track(DBHandler db,  DBReadProgress progressCallback)
{
    assert(db && "Database context cannot be NULL");
    ((Database*)db)->readTraceChunkTrackByTrack(progressCallback);
    ((Database*)db)->resetReadRequest();
    return true; 
}

int read_trace_chunk_all_tracks(DBHandler db, DBReadProgress progressCallback)
{
    assert(db && "Database context cannot be NULL");
    ((Database*)db)->readTraceChunkAllTracks(progressCallback);
    ((Database*)db)->resetReadRequest();
    return true; 
}

void*  open_rocpd_database(const char* path)
{
    RocpdDatabase* db = new RocpdDatabase(path);
    if (db != nullptr)
    {
        db->open();
    }
    return db;
}

int close_rocpd_database(DBHandler db)
{
    assert(db && "Database context cannot be NULL");
    ((Database*)db)->close();
    delete (RocpdDatabase*)db;
    return true;
}

void  configure_trace_read_time_period(DBHandler db, unsigned long long startTime, unsigned long long endTime)
{
    assert(db && "Database context cannot be NULL");
    ((Database*)db)->configureTraceReadTimePeriod(startTime, endTime);
}

void  add_track_to_trace_read_config(DBHandler db, unsigned short trackId)
{
    assert(db && "Database context cannot be NULL");
    ((Database*)db)->addTrackToTraceReadConfig(trackId);
}
