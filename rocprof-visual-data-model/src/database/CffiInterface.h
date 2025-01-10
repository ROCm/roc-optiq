typedef void* DBHandler;
typedef void* ChunkConfig;
typedef void (*DBReadProgress)(const int, const char*);
void*   open_rocpd_database(const char*);                                                       // open database with given path
int     read_trace_properties(DBHandler, DBReadProgress);                                       // read trace properties
int     close_rocpd_database(DBHandler);                                                        // close database
int     read_trace_chunk_track_by_track(DBHandler, DBReadProgress);                             // read trace chuck running separate SQL query for every track
int     read_trace_chunk_all_tracks(DBHandler db, DBReadProgress);                              // read trace chunk for all tracks for configured time frame
void    configure_trace_read_time_period(DBHandler, unsigned long long, unsigned long long);    // configure trace chunk read timeframe
void    add_track_to_trace_read_config(DBHandler, unsigned short);                              // add track to trace chunk read configuration 
