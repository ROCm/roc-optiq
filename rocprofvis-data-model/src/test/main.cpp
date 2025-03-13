
#include "../common/Interface.h"
#include <iostream>
#include <cstdio>
#include <random>
#include <vector>


void db_progress ( rocprofvis_db_filename_t db_name, rocprofvis_db_progress_percent_t progress, rocprofvis_db_status_t status, rocprofvis_db_status_message_t msg)
{
    std::cout << "\r Dtatabase " << db_name << " : " << progress << "% - " << (status == kRPVDbSuccess ? " DONE " : status == kRPVDbError ? " ERROR " : " BUSY ") << msg << std::string(20,' ') << std::endl;
}

void PrintTracks(rocprofvis_dm_trace_t trace, bool print_any=false)
{
    uint64_t num_tracks = rocprofvis_dm_get_property_as_uint64(trace, kRPVDMNumberOfTracksUInt64, 0);
    for (int i=0; i < num_tracks; i++)
    {
        rocprofvis_dm_track_t track = rocprofvis_dm_get_property_as_handle(trace, kRPVDMTrackHandleIndexed, i);
        uint64_t num_slices = rocprofvis_dm_get_property_as_uint64(track, kRPVDMTNumberOfSlicesUInt64,0);
        if (print_any || (num_slices > 0)) {
        printf("Track type = % lld, name = % s: % s, slices = % lld\n",
            rocprofvis_dm_get_property_as_uint64(track, kRPVDMTrackCategoryEnumUInt64,0), 
            rocprofvis_dm_get_property_as_charptr(track, kRPVDMTrackMainProcessNameCharPtr,0), 
            rocprofvis_dm_get_property_as_charptr(track, kRPVDMTrackMainProcessNameCharPtr,0), 
            num_slices
            );
        }
    }
}

void PrintGenericMetadataProperties(rocprofvis_dm_trace_t trace, rocprofvis_dm_timestamp_t& start_time, rocprofvis_dm_timestamp_t& end_time)
{
    start_time = rocprofvis_dm_get_property_as_uint64(trace, kRPVDMStartTimeUInt64, 0);
    end_time = rocprofvis_dm_get_property_as_uint64(trace, kRPVDMEndTimeUInt64, 0);
    printf("Trace start time=%lld, end time = %lld\n", start_time, end_time);
}

void GenerateRandomSlice(rocprofvis_dm_trace_t trace, rocprofvis_db_num_of_tracks_t& count, rocprofvis_db_track_selection_t& tracks, rocprofvis_dm_timestamp_t & start_time, rocprofvis_dm_timestamp_t & end_time, rocprofvis_dm_index_t & sample_track)
{
    static std::vector<uint32_t> v;
    sample_track = INVALID_INDEX;
    uint64_t num_tracks = rocprofvis_dm_get_property_as_uint64(trace, kRPVDMNumberOfTracksUInt64, 0);
    int rand_num_tracks = 0;
    while (rand_num_tracks == 0) rand_num_tracks = std::rand() % num_tracks;
    int rand_sample_track_index = std::rand() % rand_num_tracks;
    for (int i = 0; i < rand_num_tracks; i++) {
        while(true) {
            uint32_t track_id1 = std::rand() % num_tracks;
            if (std::find_if(v.begin(), v.end(), [track_id1](uint32_t track_id2) {return track_id2 == track_id1; }) == v.end())
            {
                v.push_back(track_id1);
                if (i == rand_sample_track_index) sample_track = track_id1;
                break;
            }
        }
    }
    count = (rocprofvis_db_num_of_tracks_t)rand_num_tracks;
    tracks = &v[0];
    rocprofvis_dm_timestamp_t tenth_time = (end_time - start_time) / 10;
    int pie1 = std::rand() % 10;
    int pie2 = std::rand() % 10;
    if (pie1 + pie2 > 10) pie2 = 10 - pie1;
    start_time = start_time + pie1 * tenth_time;
    end_time = start_time +  pie2 * tenth_time;
    printf("Testing slice for time = %lld, number of tracks = %ld [", end_time - start_time, count);
    for (int i=0; i < count; i++) printf("%d,",tracks[i]);
    printf("]\n");
}

int main()
{
    std::srand(std::time(nullptr));
    rocprofvis_dm_trace_t trace = rocprofvis_dm_create_trace();
    if (nullptr != trace) 
    {
        rocprofvis_dm_database_t db = rocprofvis_db_open_database("trace.rpd", kAutodetect);
        if (nullptr != db && kRocProfVisDmResultSuccess == rocprofvis_dm_bind_trace_to_database(trace, db))
        {
            rocprofvis_db_future_t object2wait = rocprofvis_db_future_alloc(db_progress);
            if (nullptr != object2wait) 
            {
                if (kRocProfVisDmResultSuccess == rocprofvis_db_read_metadata_async(db, object2wait))
                {
	                if (kRocProfVisDmResultSuccess == rocprofvis_db_future_wait(object2wait, 10000))
	                {   
                        rocprofvis_dm_timestamp_t start_time;
                        rocprofvis_dm_timestamp_t end_time;
                        rocprofvis_db_num_of_tracks_t num_tracks;
                        rocprofvis_db_track_selection_t tracks_selection;
                        uint32_t test_track = 0;
                        PrintGenericMetadataProperties(trace, start_time, end_time);
                        PrintTracks(trace);
                        GenerateRandomSlice(trace, num_tracks, tracks_selection, start_time, end_time, test_track);
                        if (test_track != INVALID_INDEX && num_tracks > 0)
                        {
                            if (kRocProfVisDmResultSuccess == rocprofvis_db_read_trace_slice_async(db, start_time, end_time, num_tracks, tracks_selection, object2wait))
                            {
                                if (kRocProfVisDmResultSuccess == rocprofvis_db_future_wait(object2wait, 20000))
                                {
                                    rocprofvis_dm_track_t track = rocprofvis_dm_get_property_as_handle(trace, kRPVDMTrackHandleIndexed, test_track);
                                    rocprofvis_dm_slice_t slice = rocprofvis_dm_get_property_as_handle(track, kRPVDMSliceHandleTimed, start_time);
                                    if (nullptr != slice)
                                    {
                                        uint64_t numRecords = rocprofvis_dm_get_property_as_uint64(track, kRPVDMNumberOfRecordsUInt64, 0);
                                        printf("Time slice for time% lld - % lld for track% d has% lld records\n", start_time, end_time, test_track, numRecords);
                                    }
                                }
                            }
                        }
                    }	
	            }
            }
            rocprofvis_db_future_free(object2wait);
        }
    }		
    rocprofvis_dm_delete_trace(trace);
}
