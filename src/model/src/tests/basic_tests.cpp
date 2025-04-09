
// Copyright (c) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_c_interface.h"
#include "rocprofvis_error_handling.h"
#include "rocprofvis_db_future.h"
#include <iostream>
#include <cstdio>
#include <random>
#include <vector>
#if defined(_WIN32)
#include <conio.h>
#else
#include <curses.h>
#endif
#include <cstdarg>
#include <algorithm>
#include <string.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_session.hpp>

#include <iostream>

std::string g_input_file="../../../sample/trace_70b_1024_32.rpd";
bool        g_all_tracks=false;
bool        g_full_range=false;

int main(int argc, char** argv)
{
  Catch::Session session;

  using namespace Catch::Clara;
  auto cli
    = session.cli()
    | Opt( g_input_file, "input_file" )
         ["--input_file"]
         ("Path to input file")
    | Opt( g_all_tracks, "all_tracks" )
         ["--all_tracks"]
         ("Whether to load/query all tracks or only some")
    | Opt( g_full_range, "full_range" )
         ["--full_range"]
         ("Whether to load/query the full trace range or only a segment");

  // Now pass the new composite back to Catch2 so it uses that
  session.cli( cli );

  // Let Catch2 (using Clara) parse the command line
  int returnCode = session.applyCommandLine( argc, argv );
  if( returnCode != 0 ) // Indicates a command line error
      return returnCode;

  return session.run();
}

void CheckMemoryFootprint(rocprofvis_dm_trace_t trace)
{
    uint64_t memory_used = 0;
    if (trace)
    {
        memory_used += rocprofvis_dm_get_property_as_uint64(trace, kRPVDMTraceMemoryFootprintUInt64, 0);
        rocprofvis_dm_database_t db = rocprofvis_dm_get_property_as_handle(trace, kRPVDMDatabaseHandle, 0);
        if (db) {
            memory_used += rocprofvis_db_get_memory_footprint(db);
        }
    }

    printf("\x1b[0mTotal memory utilization so far = %lld\n",memory_used);
}

#define LIST_SIZE_LIMIT 2
#define HEADER_LEN 100

void PrintHeader(const char* fmt, ...) {
    std::string header;
    va_list argptr;
    char buffer[256];
    va_start(argptr, fmt);
    vsprintf(buffer, fmt, argptr);
    va_end(argptr);
    size_t text_len = strlen(buffer);
    if (HEADER_LEN > text_len)
        header.assign((HEADER_LEN - text_len) / 2, '*');
    printf("\x1b[0m%s%s%s\n", header.c_str(), buffer, header.c_str());
}

void db_progress(rocprofvis_db_filename_t db_name, rocprofvis_db_progress_percent_t progress, rocprofvis_db_status_t status, rocprofvis_db_status_message_t msg)
{
    const char* str = " ERROR ";
    const char* color = "\x1b[31m";
    if (status == kRPVDbSuccess) {
        color = "\x1b[32m";
        str = " DONE ";
    } else
        if (status == kRPVDbBusy) {
            color = "\x1b[33m";
            str = " BUSY ";
        }

    printf("%s[%s] %d%% - %s - %s\n",color, db_name, progress, str, msg);
}

void GenerateRandomSlice(   rocprofvis_dm_trace_t trace, 
                            rocprofvis_db_num_of_tracks_t& count, 
                            rocprofvis_db_track_selection_t& tracks, 
                            rocprofvis_dm_timestamp_t & start_time, 
                            rocprofvis_dm_timestamp_t & end_time)
{
    static std::vector<uint32_t> v;
    uint64_t num_tracks = rocprofvis_dm_get_property_as_uint64(trace, kRPVDMNumberOfTracksUInt64, 0);
    if (g_all_tracks) {
        for (int i = 0; i < num_tracks; i++) {
            v.push_back(i);
        }
        count = (rocprofvis_db_num_of_tracks_t)num_tracks;
    }
    else
    {
        int rand_num_tracks = 0;
        while (rand_num_tracks == 0) rand_num_tracks = std::rand() % num_tracks;
        for (int i = 0; i < rand_num_tracks; i++) {
            while (true) {
                uint32_t track_id1 = std::rand() % num_tracks;
                if (std::find_if(v.begin(), v.end(), [track_id1](uint32_t track_id2) {return track_id2 == track_id1; }) == v.end())
                {
                    v.push_back(track_id1);
                    break;
                }
            }
        }
        count = (rocprofvis_db_num_of_tracks_t)rand_num_tracks;
    }
    
    tracks = &v[0];
    rocprofvis_dm_timestamp_t tenth_time = (end_time - start_time) / 10;
    int pie1 = std::rand() % 10;
    int pie2 = std::rand() % 10;
    if ((pie1 + pie2) > 10) pie2 = 10 - pie1;
    if (!g_full_range) 
    {
        start_time = start_time + pie1 * tenth_time;
        end_time = start_time +  pie2 * tenth_time;
    }
    PrintHeader("Testing slice for %ld ns and %ld tracks", end_time - start_time, count);
    printf("Track indexes:\n");
    for (int i=0; i < count; i++) printf("%d,",tracks[i]);
    printf("]\n");
}

TEST_CASE( "Future Initialisation", "[require]" ) {
    PrintHeader("Allocate future");
    rocprofvis_db_future_t object2wait = rocprofvis_db_future_alloc(db_progress);
    REQUIRE (nullptr != object2wait);
    PrintHeader("Delete future");
    rocprofvis_db_future_free(object2wait);
}

struct RocProfVisDMFixture
{
    mutable rocprofvis_dm_trace_t    m_trace = nullptr;
    mutable rocprofvis_dm_database_t m_db    = nullptr;
    mutable rocprofvis_dm_timestamp_t start_time;
    mutable rocprofvis_dm_timestamp_t end_time;
    mutable rocprofvis_db_num_of_tracks_t num_tracks;
    mutable rocprofvis_db_track_selection_t tracks_selection;
};

TEST_CASE_PERSISTENT_FIXTURE(RocProfVisDMFixture, "Tests for the Data-Model")
{
    SECTION("Create Database")
    {
        PrintHeader("Create trace");
        m_trace = rocprofvis_dm_create_trace();
        REQUIRE(nullptr != m_trace);
    }

    SECTION("Data-Model Initialisation")
    {
        CheckMemoryFootprint(m_trace);
        PrintHeader("Open database %s", g_input_file.c_str());
        m_db =
            rocprofvis_db_open_database(g_input_file.c_str(), kAutodetect);
        REQUIRE(nullptr != m_db);
        if(m_db)
        {
            auto bind_result = rocprofvis_dm_bind_trace_to_database(m_trace, m_db);
            REQUIRE(kRocProfVisDmResultSuccess == bind_result);
        }
    }

    SECTION("Issue Read Metadata")
    {
        PrintHeader("Allocate future");
        rocprofvis_db_future_t object2wait = rocprofvis_db_future_alloc(db_progress);
        REQUIRE(object2wait);
        if(nullptr != object2wait)
        {
            PrintHeader("Issue metadata read");
            auto meta_result = rocprofvis_db_read_metadata_async(m_db, object2wait);
            REQUIRE(kRocProfVisDmResultSuccess == meta_result);
            if(kRocProfVisDmResultSuccess == meta_result)
            {
                PrintHeader("Wait for metadata");
                auto wait_result = rocprofvis_db_future_wait(object2wait, UINT64_MAX);
                REQUIRE(kRocProfVisDmResultSuccess == wait_result);
            }
            rocprofvis_db_future_free(object2wait);
        }
    }

    SECTION("Read Track Metadata")
    {
        CheckMemoryFootprint(m_trace);

        start_time =
            rocprofvis_dm_get_property_as_uint64(m_trace, kRPVDMStartTimeUInt64, 0);
        end_time = rocprofvis_dm_get_property_as_uint64(m_trace, kRPVDMEndTimeUInt64, 0);
        printf(ANSI_COLOR_GREEN "Trace start time=%lld, end time = %lld\n", start_time,
               end_time);
        num_tracks = (rocprofvis_db_num_of_tracks_t) rocprofvis_dm_get_property_as_uint64(
            m_trace, kRPVDMNumberOfTracksUInt64, 0);
        for(int i = 0; i < num_tracks; i++)
        {
            rocprofvis_dm_track_t track =
                rocprofvis_dm_get_property_as_handle(m_trace, kRPVDMTrackHandleIndexed, i);
            REQUIRE(track != nullptr);

            char const* category = rocprofvis_dm_get_property_as_charptr(
                track, kRPVDMTrackCategoryEnumCharPtr, 0);
            char const* process = rocprofvis_dm_get_property_as_charptr(
                track, kRPVDMTrackMainProcessNameCharPtr, 0);
            char const* subname = rocprofvis_dm_get_property_as_charptr(
                track, kRPVDMTrackSubProcessNameCharPtr, 0);
            REQUIRE(category);
            REQUIRE(process);
            REQUIRE(subname);

            printf(
                ANSI_COLOR_CYAN
                "Track id=%lld node=%lld category=%s process=%s subprocess=%s\n",
                rocprofvis_dm_get_property_as_uint64(track, kRPVDMTrackIdUInt64, 0),
                rocprofvis_dm_get_property_as_uint64(track, kRPVDMTrackNodeIdUInt64, 0),
                rocprofvis_dm_get_property_as_charptr(track,
                                                      kRPVDMTrackCategoryEnumCharPtr, 0),
                rocprofvis_dm_get_property_as_charptr(
                    track, kRPVDMTrackMainProcessNameCharPtr, 0),
                rocprofvis_dm_get_property_as_charptr(
                    track, kRPVDMTrackSubProcessNameCharPtr, 0));
        }
    }

    SECTION("Whole Trace Read")
    {
        double   whole_trace_readtime = 0;
        uint32_t total_num_rows       = 0;
        uint32_t total_num_tracks     = num_tracks;
        GenerateRandomSlice(m_trace, num_tracks, tracks_selection, start_time, end_time);
        for(int i = 0; i < total_num_tracks; i++)
        {
            rocprofvis_dm_track_t track = rocprofvis_dm_get_property_as_handle(
                m_trace, kRPVDMTrackHandleIndexed, i);
            REQUIRE(track != nullptr);

            bool load_slice = false;
            for(int j = 0; j < num_tracks; j++)
            {
                if(tracks_selection[j] == i)
                {
                    load_slice = true;
                    break;
                }
            }
            if(load_slice)
            {
                PrintHeader("Allocate future");
                rocprofvis_db_future_t object2wait =
                    rocprofvis_db_future_alloc(db_progress);
                REQUIRE(nullptr != object2wait);

                auto t1               = std::chrono::steady_clock::now();
                auto read_slice_issue = rocprofvis_db_read_trace_slice_async(
                    m_db, start_time, end_time, 1, (uint32_t*) &i, object2wait);
                REQUIRE(kRocProfVisDmResultSuccess == read_slice_issue);
                if(kRocProfVisDmResultSuccess == read_slice_issue)
                {
                    auto read_wait = rocprofvis_db_future_wait(object2wait, UINT64_MAX);
                    REQUIRE(kRocProfVisDmResultSuccess == read_wait);
                    if(kRocProfVisDmResultSuccess == read_wait)
                    {
                        auto t2 = std::chrono::steady_clock::now();
                        std::chrono::duration<double> diff = t2 - t1;
                        whole_trace_readtime += diff.count();
                        uint32_t num_rows = ((RocProfVis::DataModel::Future*) object2wait)
                                                ->GetProcessedRowsCount();
                        total_num_rows += num_rows;
                        rocprofvis_dm_slice_t slice =
                            rocprofvis_dm_get_property_as_handle(
                                track, kRPVDMSliceHandleTimed, start_time);
                        REQUIRE(slice);
                        uint64_t num_records = rocprofvis_dm_get_property_as_uint64(
                            slice, kRPVDMNumberOfRecordsUInt64, 0);
                        printf(ANSI_COLOR_MAGENTA
                               "Track %d has %lld records, read time - %13.9f, number of "
                               "rows processed = %ld\n" ANSI_COLOR_RESET,
                               i, num_records, diff.count(), num_rows);
                        rocprofvis_dm_delete_all_time_slices(m_trace);
                    }
                }

                PrintHeader("Free future");
                rocprofvis_db_future_free(object2wait);
            }
        }

        printf(ANSI_COLOR_MAGENTA "Whole trace read time - %13.9f,  number of rows "
                                  "processed = %ld\n" ANSI_COLOR_RESET,
               whole_trace_readtime, total_num_rows);
    }

    SECTION("Read Random Slice Data")
    {
        if(num_tracks > 0)
        {
            rocprofvis_db_future_t object2wait = rocprofvis_db_future_alloc(db_progress);
            REQUIRE(nullptr != object2wait);

            // Read time slice a process data
            PrintHeader("Read time slice for all tracks");
            auto t1                = std::chrono::steady_clock::now();
            auto read_slice_result = rocprofvis_db_read_trace_slice_async(
                m_db, start_time, end_time, num_tracks, tracks_selection, object2wait);
            REQUIRE(kRocProfVisDmResultSuccess == read_slice_result);
            if(kRocProfVisDmResultSuccess == read_slice_result)
            {
                auto slice_wait_result =
                    rocprofvis_db_future_wait(object2wait, UINT64_MAX);
                REQUIRE(kRocProfVisDmResultSuccess == slice_wait_result);
                if(kRocProfVisDmResultSuccess == slice_wait_result)
                {
                    auto                          t2   = std::chrono::steady_clock::now();
                    std::chrono::duration<double> diff = t2 - t1;
                    uint32_t num_rows = ((RocProfVis::DataModel::Future*) object2wait)
                                            ->GetProcessedRowsCount();
                    printf(ANSI_COLOR_MAGENTA "Whole trace read time - %13.9f, number of "
                                              "rows processed = %ld\n" ANSI_COLOR_RESET,
                           diff.count(), num_rows);
                    CheckMemoryFootprint(m_trace);
                    PrintHeader("Time slice content, up to %d records", LIST_SIZE_LIMIT);
                    int first_track = std::rand() % num_tracks;
                    for(int i = first_track;
                        (i < num_tracks) && (i < first_track + LIST_SIZE_LIMIT); i++)
                    {
                        rocprofvis_dm_track_t track =
                            rocprofvis_dm_get_property_as_handle(
                                m_trace, kRPVDMTrackHandleIndexed, tracks_selection[i]);
                        REQUIRE(track != nullptr);
                        char* track_category_name = rocprofvis_dm_get_property_as_charptr(
                            track, kRPVDMTrackCategoryEnumCharPtr, 0);
                        char* track_process_name = rocprofvis_dm_get_property_as_charptr(
                            track, kRPVDMTrackMainProcessNameCharPtr, 0);
                        char* track_sub_process_name =
                            rocprofvis_dm_get_property_as_charptr(
                                track, kRPVDMTrackSubProcessNameCharPtr, 0);
                        REQUIRE(track_category_name);
                        REQUIRE(track_process_name);
                        REQUIRE(track_sub_process_name);
                        rocprofvis_dm_track_category_t track_category =
                            (rocprofvis_dm_track_category_t)
                                rocprofvis_dm_get_property_as_uint64(
                                    track, kRPVDMTrackCategoryEnumUInt64, 0);
                        rocprofvis_dm_slice_t slice =
                            rocprofvis_dm_get_property_as_handle(
                                track, kRPVDMSliceHandleTimed, start_time);
                        REQUIRE(slice != nullptr);
                        uint64_t id = rocprofvis_dm_get_property_as_uint64(
                            track, kRPVDMTrackIdUInt64, 0);
                        uint64_t node_id = rocprofvis_dm_get_property_as_uint64(
                            track, kRPVDMTrackNodeIdUInt64, 0);
                        uint64_t memory_usage = rocprofvis_dm_get_property_as_uint64(
                            track, kRPVDMTrackMemoryFootprintUInt64, 0);
                        uint64_t num_ext_data = rocprofvis_dm_get_property_as_uint64(
                            track, kRPVDMNumberOfTrackExtDataRecordsUInt64, 0);
                        PrintHeader(
                            "Track id=%ld node=%ld category=%s process=%s subprocess=%s",
                            id, node_id, track_category_name, track_process_name,
                            track_sub_process_name);

                        printf(ANSI_COLOR_CYAN "\t%s : %s : %lld\n", "Properties",
                               "Memory usage", memory_usage);
                        for(int i = 0; i < num_ext_data; i++)
                        {
                            char* ext_data_category =
                                rocprofvis_dm_get_property_as_charptr(
                                    track, kRPVDMTrackExtDataCategoryCharPtrIndexed, i);
                            char* ext_data_name = rocprofvis_dm_get_property_as_charptr(
                                track, kRPVDMTrackExtDataNameCharPtrIndexed, i);
                            char* ext_data_value = rocprofvis_dm_get_property_as_charptr(
                                track, kRPVDMTrackExtDataValueCharPtrIndexed, i);
                            REQUIRE(ext_data_category);
                            REQUIRE(ext_data_name);
                            REQUIRE(ext_data_value);
                            printf(ANSI_COLOR_CYAN "\t%s : %s : %s\n", ext_data_category,
                                   ext_data_name, ext_data_value);
                        }
                        char* ext_data_json = rocprofvis_dm_get_property_as_charptr(
                            track, kRPVDMTrackInfoJsonCharPtr, 0);
                        REQUIRE(ext_data_json);
                        printf(ANSI_COLOR_CYAN "Extended data as JSON:\n%s\n",
                               ext_data_json);
                        if(nullptr != slice)
                        {
                            uint64_t num_records = rocprofvis_dm_get_property_as_uint64(
                                slice, kRPVDMNumberOfRecordsUInt64, 0);
                            printf(ANSI_COLOR_BLUE "Time slice for time%lld - %lld for "
                                                   "track %d [%s:%s:%s] has%lld records\n",
                                   start_time, end_time, tracks_selection[i],
                                   track_category_name, track_process_name,
                                   track_sub_process_name, num_records);
                            if(num_records == 0) continue;
                            int first_record = std::rand() % num_records;
                            for(int j = first_record;
                                (j < num_records) && (j < first_record + LIST_SIZE_LIMIT);
                                j++)
                            {
                                uint64_t timestamp = rocprofvis_dm_get_property_as_uint64(
                                    slice, kRPVDMTimestampUInt64Indexed, j);
                                if(track_category == rocprofvis_dm_track_category_t::
                                                         kRocProfVisDmRegionTrack ||
                                   track_category == rocprofvis_dm_track_category_t::
                                                         kRocProfVisDmKernelTrack)
                                {
                                    int64_t duration =
                                        rocprofvis_dm_get_property_as_int64(
                                            slice, kRPVDMEventDurationInt64Indexed, j);
                                    if(duration < 0)
                                    {
                                        printf(ANSI_COLOR_RED
                                               "Record %d has invalid duration "
                                               "%lld\n" ANSI_COLOR_RESET,
                                               j, duration);
                                    }
                                    uint64_t id = rocprofvis_dm_get_property_as_uint64(
                                        slice, kRPVDMEventIdUInt64Indexed, j);
                                    uint64_t op = rocprofvis_dm_get_property_as_uint64(
                                        slice, kRPVDMEventIdOperationEnumIndexed, j);
                                    char* op_str = rocprofvis_dm_get_property_as_charptr(
                                        slice, kRPVDMEventIdOperationCharPtrIndexed, j);
                                    char* type_str =
                                        rocprofvis_dm_get_property_as_charptr(
                                            slice, kRPVDMEventTypeStringCharPtrIndexed,
                                            j);
                                    char* symbol_str =
                                        rocprofvis_dm_get_property_as_charptr(
                                            slice, kRPVDMEventSymbolStringCharPtrIndexed,
                                            j);
                                    REQUIRE(op_str);
                                    REQUIRE(type_str);
                                    REQUIRE(symbol_str);
                                    printf(ANSI_COLOR_BLUE
                                           "Record id=%lld, timestamp=%lld, op=%lld, "
                                           "op_str=%s, type=%s, symbol=%s\n",
                                           id, timestamp, op, op_str, type_str,
                                           symbol_str);

                                    rocprofvis_dm_event_id_t event_id = { id, op };
                                    PrintHeader("Testing multithreaded access to "
                                                "database retrieving flow, stack and "
                                                "extended data for event id = %lld",
                                                event_id.bitfield.event_id);
                                    rocprofvis_db_future_t object2wait4flowtrace =
                                        rocprofvis_db_future_alloc(db_progress);
                                    REQUIRE(object2wait4flowtrace);
                                    rocprofvis_db_future_t object2wait4stacktrace =
                                        rocprofvis_db_future_alloc(db_progress);
                                    REQUIRE(object2wait4stacktrace);
                                    rocprofvis_db_future_t object2wait4extdata =
                                        rocprofvis_db_future_alloc(db_progress);
                                    REQUIRE(object2wait4extdata);
                                    rocprofvis_dm_result_t result4flowtrace =
                                        rocprofvis_db_read_event_property_async(
                                            m_db, kRPVDMEventFlowTrace, event_id,
                                            object2wait4flowtrace);
                                    REQUIRE(kRocProfVisDmResultSuccess ==
                                            result4flowtrace);
                                    rocprofvis_dm_result_t result4stacktrace =
                                        rocprofvis_db_read_event_property_async(
                                            m_db, kRPVDMEventStackTrace, event_id,
                                            object2wait4stacktrace);
                                    REQUIRE(kRocProfVisDmResultSuccess ==
                                            result4stacktrace);
                                    rocprofvis_dm_result_t result4extdata =
                                        rocprofvis_db_read_event_property_async(
                                            m_db, kRPVDMEventExtData, event_id,
                                            object2wait4extdata);
                                    REQUIRE(kRocProfVisDmResultSuccess == result4extdata);

                                    auto result4flowtrace_wait =
                                        rocprofvis_db_future_wait(object2wait4flowtrace,
                                                                  UINT64_MAX);
                                    REQUIRE(kRocProfVisDmResultSuccess ==
                                            result4flowtrace_wait);

                                    if(kRocProfVisDmResultSuccess == result4flowtrace &&
                                       kRocProfVisDmResultSuccess ==
                                           result4flowtrace_wait)
                                    {
                                        rocprofvis_dm_flowtrace_t flowtrace =
                                            rocprofvis_dm_get_property_as_handle(
                                                m_trace, kRPVDMFlowTraceHandleByEventID,
                                                event_id.value);
                                        REQUIRE(flowtrace != nullptr);
                                        if(flowtrace != nullptr)
                                        {
                                            PrintHeader(
                                                "Data flow trace for event id = %ld",
                                                event_id.bitfield.event_id);
                                            uint64_t num_endpoints =
                                                rocprofvis_dm_get_property_as_uint64(
                                                    flowtrace,
                                                    kRPVDMNumberOfEndpointsUInt64, 0);
                                            printf(ANSI_COLOR_MAGENTA
                                                   "Event has %lld data flow link:\n",
                                                   num_endpoints);
                                            for(int k = 0; k < num_endpoints; k++)
                                            {
                                                uint64_t track_id =
                                                    rocprofvis_dm_get_property_as_uint64(
                                                        flowtrace,
                                                        kRPVDMEndpointTrackIDUInt64Indexed,
                                                        k);
                                                rocprofvis_dm_event_id_t event_id;
                                                event_id.value =
                                                    rocprofvis_dm_get_property_as_uint64(
                                                        flowtrace,
                                                        kRPVDMEndpointIDUInt64Indexed, k);
                                                uint64_t event_timestamp =
                                                    rocprofvis_dm_get_property_as_uint64(
                                                        flowtrace,
                                                        kRPVDMEndpointTimestampUInt64Indexed,
                                                        k);
                                                printf(ANSI_COLOR_MAGENTA
                                                       "\tEndpoint %d at track=%lld, "
                                                       "event_id=%lld, timestamp=%lld\n",
                                                       k, track_id,
                                                       event_id.bitfield.event_id,
                                                       event_timestamp);
                                            }
                                            rocprofvis_dm_delete_event_property_for(
                                                m_trace, kRPVDMEventFlowTrace, event_id);
                                        }
                                    }
                                    rocprofvis_db_future_free(object2wait4flowtrace);

                                    auto stacktrack_wait = rocprofvis_db_future_wait(
                                        object2wait4stacktrace, UINT64_MAX);
                                    REQUIRE(kRocProfVisDmResultSuccess ==
                                            stacktrack_wait);
                                    if(kRocProfVisDmResultSuccess == result4stacktrace &&
                                       kRocProfVisDmResultSuccess == stacktrack_wait)
                                    {
                                        rocprofvis_dm_stacktrace_t stacktrace =
                                            rocprofvis_dm_get_property_as_handle(
                                                m_trace, kRPVDMStackTraceHandleByEventID,
                                                event_id.value);
                                        REQUIRE(stacktrace != nullptr);
                                        if(stacktrace != nullptr)
                                        {
                                            PrintHeader("Stack trace for event id = %ld",
                                                        event_id.bitfield.event_id);
                                            uint64_t num_frames =
                                                rocprofvis_dm_get_property_as_uint64(
                                                    stacktrace,
                                                    kRPVDMNumberOfFramesUInt64, 0);
                                            printf(ANSI_COLOR_MAGENTA
                                                   "Event has %lld stack frames:\n",
                                                   num_frames);
                                            for(int k = 0; k < num_frames; k++)
                                            {
                                                uint64_t frame_depth =
                                                    rocprofvis_dm_get_property_as_uint64(
                                                        stacktrace,
                                                        kRPVDMFrameDepthUInt64Indexed, k);
                                                char* frame_symbol =
                                                    rocprofvis_dm_get_property_as_charptr(
                                                        stacktrace,
                                                        kRPVDMFrameSymbolCharPtrIndexed,
                                                        k);
                                                char* frame_args =
                                                    rocprofvis_dm_get_property_as_charptr(
                                                        stacktrace,
                                                        kRPVDMFrameArgsCharPtrIndexed, k);
                                                char* frame_code =
                                                    rocprofvis_dm_get_property_as_charptr(
                                                        stacktrace,
                                                        kRPVDMFrameCodeLineCharPtrIndexed,
                                                        k);
                                                REQUIRE(frame_symbol);
                                                REQUIRE(frame_args);
                                                REQUIRE(frame_code);
                                                printf(ANSI_COLOR_MAGENTA
                                                       "\tFrame %d : depth=%lld, "
                                                       "symbol=%s, args=%s, code=%s \n",
                                                       k, frame_depth, frame_symbol,
                                                       frame_args, frame_code);
                                            }
                                            rocprofvis_dm_delete_event_property_for(
                                                m_trace, kRPVDMEventStackTrace, event_id);
                                        }
                                    }
                                    rocprofvis_db_future_free(object2wait4stacktrace);

                                    auto extdata_wait = rocprofvis_db_future_wait(
                                        object2wait4extdata, UINT64_MAX);
                                    REQUIRE(kRocProfVisDmResultSuccess == extdata_wait);
                                    if(kRocProfVisDmResultSuccess == result4extdata &&
                                       kRocProfVisDmResultSuccess == extdata_wait)
                                    {
                                        rocprofvis_dm_extdata_t extdata =
                                            rocprofvis_dm_get_property_as_handle(
                                                m_trace, kRPVDMExtInfoHandleByEventID,
                                                event_id.value);
                                        REQUIRE(extdata != nullptr);
                                        if(extdata != nullptr)
                                        {
                                            PrintHeader(
                                                "Extended data for event id = %ld",
                                                event_id.bitfield.event_id);
                                            uint64_t num_records =
                                                rocprofvis_dm_get_property_as_uint64(
                                                    extdata,
                                                    kRPVDMNumberOfExtDataRecordsUInt64,
                                                    0);
                                            printf(ANSI_COLOR_MAGENTA
                                                   "Event has %lld extended data "
                                                   "properties:\n",
                                                   num_records);
                                            for(int k = 0; k < num_records; k++)
                                            {
                                                char* data_category =
                                                    rocprofvis_dm_get_property_as_charptr(
                                                        extdata,
                                                        kRPVDMExtDataCategoryCharPtrIndexed,
                                                        k);
                                                char* data_name =
                                                    rocprofvis_dm_get_property_as_charptr(
                                                        extdata,
                                                        kRPVDMExtDataNameCharPtrIndexed,
                                                        k);
                                                char* data_value =
                                                    rocprofvis_dm_get_property_as_charptr(
                                                        extdata,
                                                        kRPVDMExtDataValueCharPtrIndexed,
                                                        k);
                                                REQUIRE(data_category);
                                                REQUIRE(data_name);
                                                REQUIRE(data_value);
                                                printf(ANSI_COLOR_MAGENTA
                                                       "\tItem %d : category=%s, "
                                                       "name=%s, value=%s \n",
                                                       k, data_category, data_name,
                                                       data_value);
                                            }
                                            char* data_json_blob =
                                                rocprofvis_dm_get_property_as_charptr(
                                                    extdata, kRPVDMExtDataJsonBlobCharPtr,
                                                    0);
                                            REQUIRE(data_json_blob);
                                            printf("\x1b[35mExtended data as JSON:\n%s\n",
                                                   data_json_blob);
                                            rocprofvis_dm_delete_event_property_for(
                                                m_trace, kRPVDMEventExtData, event_id);
                                        }
                                    }
                                    rocprofvis_db_future_free(object2wait4extdata);
                                }
                                else if(track_category == rocprofvis_dm_track_category_t::
                                                              kRocProfVisDmPmcTrack)
                                {
                                    double value = rocprofvis_dm_get_property_as_double(
                                        slice, kRPVDMPmcValueDoubleIndexed, j);
                                    printf(ANSI_COLOR_BLUE
                                           "Record timestamp=%lld, value=%g\n",
                                           timestamp, value);
                                }
                            }
                        }
                        else
                        {
                            printf(ANSI_COLOR_RED
                                   "No time slice at %lld loaded for track %d\n",
                                   start_time, tracks_selection[i]);
                        }
                    }
                }
            }
            PrintHeader("Delete all slices");
            rocprofvis_dm_delete_all_time_slices(m_trace);

            PrintHeader("Delete future");
            rocprofvis_db_future_free(object2wait);
        }
    }
    
    SECTION("Test SQL Query")
    {
        CheckMemoryFootprint(m_trace);

        PrintHeader("Allocate future");
        rocprofvis_db_future_t object2wait = rocprofvis_db_future_alloc(db_progress);
        REQUIRE(nullptr != object2wait);
        PrintHeader("Test SQL table read");
        auto query_result = rocprofvis_db_execute_query_async(
            m_db, "select * from top;", "Kernel execution summary", object2wait);
        REQUIRE(kRocProfVisDmResultSuccess == query_result);
        if(kRocProfVisDmResultSuccess == query_result)
        {
            auto query_wait = rocprofvis_db_future_wait(object2wait, UINT64_MAX);
            REQUIRE(kRocProfVisDmResultSuccess == query_wait);
            if(kRocProfVisDmResultSuccess == query_wait)
            {
                uint64_t num_tables = rocprofvis_dm_get_property_as_uint64(
                    m_trace, kRPVDMNumberOfTablesUInt64, 0);
                if(num_tables > 0)
                {
                    rocprofvis_dm_table_t table = rocprofvis_dm_get_property_as_handle(
                        m_trace, kRPVDMTableHandleIndexed, 0);
                    REQUIRE(table);
                    if(nullptr != table)
                    {
                        char* table_description = rocprofvis_dm_get_property_as_charptr(
                            table, kRPVDMExtTableDescriptionCharPtr, 0);
                        REQUIRE(table_description);
                        char* table_query = rocprofvis_dm_get_property_as_charptr(
                            table, kRPVDMExtTableQueryCharPtr, 0);
                        REQUIRE(table_query);
                        uint64_t num_columns = rocprofvis_dm_get_property_as_uint64(
                            table, kRPVDMNumberOfTableColumnsUInt64, 0);
                        uint64_t num_rows = rocprofvis_dm_get_property_as_uint64(
                            table, kRPVDMNumberOfTableRowsUInt64, 0);
                        printf("\x1b[0m%s - %s\n", table_description, table_query);
                        std::vector<std::string> columns;
                        for(int i = 0; i < num_columns; i++)
                        {
                            columns.push_back(rocprofvis_dm_get_property_as_charptr(
                                table, kRPVDMExtTableColumnNameCharPtrIndexed, i));
                        }
                        for(int i = 0; i < num_columns; i++)
                        {
                            printf(" %20s |", columns[i].c_str());
                        }
                        printf("\n");
                        for(int i = 0; i < num_rows; i++)
                        {
                            rocprofvis_dm_table_row_t table_row =
                                rocprofvis_dm_get_property_as_handle(
                                    table, kRPVDMExtTableRowHandleIndexed, i);
                            REQUIRE(table_row);
                            if(table_row != nullptr)
                            {
                                uint64_t num_cells = rocprofvis_dm_get_property_as_uint64(
                                    table_row, kRPVDMNumberOfTableRowCellsUInt64, 0);
                                if(num_cells == num_columns)
                                {
                                    std::vector<std::string> row;
                                    for(int j = 0; j < num_cells; j++)
                                    {
                                        row.push_back(
                                            rocprofvis_dm_get_property_as_charptr(
                                                table_row,
                                                kRPVDMExtTableRowCellValueCharPtrIndexed,
                                                j));
                                    }
                                    for(int j = 0; j < num_cells; j++)
                                    {
                                        printf("%20s", row[j].substr(0, 20).c_str());
                                    }
                                }
                                else
                                {
                                    printf("\x1b[31mError! Number of colums does nt "
                                           "match number of cells in row %d\n",
                                           i);
                                    break;
                                }
                            }
                            printf("\n");
                        }
                        rocprofvis_dm_delete_all_tables(m_trace);
                    }
                }
            }
        }
        PrintHeader("Delete future");
        rocprofvis_db_future_free(object2wait);
    }

    SECTION("Delete trace")
    {
        CheckMemoryFootprint(m_trace);
        PrintHeader("Delete trace");
        rocprofvis_dm_delete_trace(m_trace);
    }
}
