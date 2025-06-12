
// Copyright (c) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_controller.h"
#include "rocprofvis_core.h"
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
#include <cfloat>
#include <iostream>

#define MULTI_LINE_LOG_START auto multi_line_log = fmt::memory_buffer()
#define MULTI_LINE_LOG(format, ...) fmt::format_to(std::back_inserter(multi_line_log), format, __VA_ARGS__)
#define MULTI_LINE_LOG_ARGS "{:.{}}",multi_line_log.data(),multi_line_log.size()


std::string g_input_file="../../../sample/trace_70b_1024_32.rpd";
bool        g_all_tracks=false;
bool        g_full_range=false;

int main(int argc, char** argv)
{
  rocprofvis_core_enable_log();
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

TEST_CASE( "Future Initialisation", "[require]" ) {

    spdlog::info("Allocating Future");
    rocprofvis_controller_future_t* future = rocprofvis_controller_future_alloc();
    REQUIRE(future != nullptr);
    spdlog::info("Free Future");
    rocprofvis_controller_future_free(future);
}

struct RocProfVisControllerFixture
{
    mutable rocprofvis_controller_t* m_controller = nullptr;
    mutable std::vector<rocprofvis_controller_array_t*> m_track_data;
};

TEST_CASE_PERSISTENT_FIXTURE(RocProfVisControllerFixture, "Tests for the Controller")
{
    SECTION("Create Controller")
    {
        spdlog::info("Allocating Controller");
        m_controller = rocprofvis_controller_alloc();
        REQUIRE(nullptr != m_controller);
    }

    SECTION("Controller Initialisation")
    {
        spdlog::info("Allocating Future");
        rocprofvis_controller_future_t* future = rocprofvis_controller_future_alloc();
        REQUIRE(future != nullptr);

        spdlog::info("Load trace: {0}", g_input_file);
        auto result = rocprofvis_controller_load_async(m_controller, g_input_file.c_str(), future);
        REQUIRE(result == kRocProfVisResultSuccess);

        spdlog::info("Wait for load");
        result = rocprofvis_controller_future_wait(future, FLT_MAX);
        REQUIRE(result == kRocProfVisResultSuccess);

        uint64_t future_result = 0;
        result = rocprofvis_controller_get_uint64(future, kRPVControllerFutureResult, 0, &future_result);
        spdlog::info("Get future result: {0}", future_result);
        REQUIRE(result == kRocProfVisResultSuccess);
        REQUIRE(future_result == kRocProfVisResultSuccess);

        spdlog::info("Free Future");
        rocprofvis_controller_future_free(future);
    }

    SECTION("Controller Load Whole Track Data")
    {
        uint64_t num_tracks = 0;
        auto result = rocprofvis_controller_get_uint64(m_controller, kRPVControllerNumTracks, 0, &num_tracks);
        spdlog::info("Get num tracks: {0}", num_tracks);
        REQUIRE(result == kRocProfVisResultSuccess);
        REQUIRE(num_tracks > 0);

        m_track_data.resize(num_tracks);

        for(uint32_t track_idx = 0; track_idx < num_tracks; track_idx++)
        {
            spdlog::info("Get track {0}", track_idx);
            rocprofvis_handle_t* track_handle = nullptr;
            result                            = rocprofvis_controller_get_object(
                m_controller, kRPVControllerTrackIndexed, track_idx, &track_handle);
            REQUIRE(result == kRocProfVisResultSuccess);
            REQUIRE(track_handle != nullptr);

            double min_time = 0;
            result          = rocprofvis_controller_get_double(
                track_handle, kRPVControllerTrackMinTimestamp, 0, &min_time);
            spdlog::info("Get track min time: {0}", min_time);
            REQUIRE(result == kRocProfVisResultSuccess);

            double max_time = 0;
            result          = rocprofvis_controller_get_double(
                track_handle, kRPVControllerTrackMaxTimestamp, 0, &max_time);
            spdlog::info("Get track max time: {0}", max_time);
            REQUIRE(result == kRocProfVisResultSuccess);

            uint64_t track_num_entries = 0;
            result                     = rocprofvis_controller_get_uint64(
                track_handle, kRPVControllerTrackNumberOfEntries, 0, &track_num_entries);
            spdlog::info("Get track num entries: {0}", track_num_entries);
            REQUIRE(result == kRocProfVisResultSuccess);
            if(track_num_entries > 0)
            {
                uint64_t track_type = 0;
                result              = rocprofvis_controller_get_uint64(
                    track_handle, kRPVControllerTrackType, 0, &track_type);
                spdlog::info("Get track type: {0} {1}", track_type,
                             track_type == kRPVControllerTrackTypeEvents ? "Events"
                                                                         : "Samples");
                REQUIRE(result == kRocProfVisResultSuccess);
                REQUIRE((track_type == kRPVControllerTrackTypeEvents ||
                         track_type == kRPVControllerTrackTypeSamples));

                spdlog::info("Allocating Array");
                m_track_data[track_idx] =
                    rocprofvis_controller_array_alloc(track_num_entries);
                rocprofvis_controller_array_t* track_data = m_track_data[track_idx];
                REQUIRE(track_data != nullptr);

                spdlog::info("Allocating Future");
                rocprofvis_controller_future_t* future =
                    rocprofvis_controller_future_alloc();
                REQUIRE(future != nullptr);

                spdlog::info("Fetch track data");
                result = rocprofvis_controller_track_fetch_async(
                    m_controller, (rocprofvis_controller_track_t*) track_handle, min_time,
                    max_time, future, track_data);
                REQUIRE(track_data != nullptr);

                spdlog::info("Wait for future");
                result = rocprofvis_controller_future_wait(future, FLT_MAX);
                REQUIRE(result == kRocProfVisResultSuccess);

                uint64_t future_result = 0;
                result                 = rocprofvis_controller_get_uint64(
                    future, kRPVControllerFutureResult, 0, &future_result);
                spdlog::info("Get future result: {0}", future_result);
                REQUIRE(result == kRocProfVisResultSuccess);
                REQUIRE(future_result == kRocProfVisResultSuccess);

                uint64_t num_results = 0;
                result               = rocprofvis_controller_get_uint64(
                    track_data, kRPVControllerArrayNumEntries, 0, &num_results);
                spdlog::info("Get num elements loaded from track: {0}", num_results);
                REQUIRE(result == kRocProfVisResultSuccess);
                REQUIRE(num_results == track_num_entries);

                for(uint64_t i = 0; i < num_results; i++)
                {
                    spdlog::trace("Get elements at index: {0}", i);
                    rocprofvis_handle_t* entry = nullptr;
                    result                     = rocprofvis_controller_get_object(
                        track_data, kRPVControllerArrayEntryIndexed, i, &entry);
                    REQUIRE(result == kRocProfVisResultSuccess);
                    REQUIRE(entry != nullptr);

                    switch(track_type)
                    {
                        case kRPVControllerTrackTypeEvents:
                        {
                            uint64_t id = 0;
                            result      = rocprofvis_controller_get_uint64(
                                entry, kRPVControllerEventId, 0, &id);
                            spdlog::trace("Element {0} event id: {1}", i, id);
                            REQUIRE(result == kRocProfVisResultSuccess);

                            double start_ts = 0;
                            result          = rocprofvis_controller_get_double(
                                entry, kRPVControllerEventStartTimestamp, 0, &start_ts);
                            spdlog::trace("Element {0} event start time: {1}", i,
                                          start_ts);
                            REQUIRE(result == kRocProfVisResultSuccess);
                            REQUIRE(start_ts >= min_time);
                            REQUIRE(start_ts <= max_time);

                            double end_ts = 0;
                            result        = rocprofvis_controller_get_double(
                                entry, kRPVControllerEventEndTimestamp, 0, &end_ts);
                            spdlog::trace("Element {0} event end time: {1}", i, end_ts);
                            REQUIRE(result == kRocProfVisResultSuccess);
                            REQUIRE(end_ts >= min_time);
                            REQUIRE(end_ts <= max_time);

                            uint32_t name_length = 0;
                            result               = rocprofvis_controller_get_string(
                                entry, kRPVControllerEventName, 0, nullptr, &name_length);
                            spdlog::trace("Element {0} event name length: {1}", i,
                                          name_length);
                            REQUIRE(result == kRocProfVisResultSuccess);
                            REQUIRE(name_length > 0);

                            std::string name;
                            name.resize(name_length);
                            result = rocprofvis_controller_get_string(
                                entry, kRPVControllerEventName, 0,
                                const_cast<char*>(name.c_str()), &name_length);
                            spdlog::trace("Element {0} event name: {1}", i, name);
                            REQUIRE(result == kRocProfVisResultSuccess);
                            REQUIRE(name_length == name.size());

                            uint32_t cat_length = 0;
                            result              = rocprofvis_controller_get_string(
                                entry, kRPVControllerEventName, 0, nullptr, &cat_length);
                            spdlog::trace("Element {0} event category length: {1}", i,
                                          cat_length);
                            REQUIRE(result == kRocProfVisResultSuccess);
                            REQUIRE(cat_length > 0);

                            std::string cat;
                            cat.resize(cat_length);
                            result = rocprofvis_controller_get_string(
                                entry, kRPVControllerEventName, 0,
                                const_cast<char*>(cat.c_str()), &cat_length);
                            spdlog::trace("Element {0} event category: {1}", i, cat);
                            REQUIRE(result == kRocProfVisResultSuccess);
                            REQUIRE(cat_length == cat.size());

                            uint64_t num_child = 0;
                            result             = rocprofvis_controller_get_uint64(
                                entry, kRPVControllerEventNumChildren, 0, &num_child);
                            spdlog::trace("Element {0} event num children: {1}", i,
                                          num_child);
                            REQUIRE(result == kRocProfVisResultSuccess);
                            REQUIRE(num_child == 0);
                            break;
                        }
                        case kRPVControllerTrackTypeSamples:
                        {
                            uint64_t id = 0;
                            result      = rocprofvis_controller_get_uint64(
                                entry, kRPVControllerSampleId, 0, &id);
                            spdlog::trace("Element {0} sample id: {1}", i, id);
                            REQUIRE(result == kRocProfVisResultSuccess);

                            uint64_t sample_type = 0;
                            result               = rocprofvis_controller_get_uint64(
                                entry, kRPVControllerSampleType, 0, &sample_type);
                            spdlog::trace("Element {0} sample type: {1}", i, sample_type);
                            REQUIRE(result == kRocProfVisResultSuccess);
                            REQUIRE(sample_type >= kRPVControllerPrimitiveTypeUInt64);
                            REQUIRE(sample_type <= kRPVControllerPrimitiveTypeDouble);

                            double timestamp = 0;
                            result           = rocprofvis_controller_get_double(
                                entry, kRPVControllerSampleTimestamp, 0, &timestamp);
                            spdlog::trace("Element {0} sample time: {1}", i, timestamp);
                            REQUIRE(result == kRocProfVisResultSuccess);
                            REQUIRE(timestamp >= min_time);
                            REQUIRE(timestamp <= max_time);

                            switch(sample_type)
                            {
                                case kRPVControllerPrimitiveTypeUInt64:
                                {
                                    uint64_t value = 0;
                                    result         = rocprofvis_controller_get_uint64(
                                        entry, kRPVControllerSampleValue, 0, &value);
                                    spdlog::trace("Element {0} sample value: {1}", i,
                                                  value);
                                    REQUIRE(result == kRocProfVisResultSuccess);
                                    break;
                                }
                                case kRPVControllerPrimitiveTypeDouble:
                                {
                                    double value = 0;
                                    result       = rocprofvis_controller_get_double(
                                        entry, kRPVControllerSampleValue, 0, &value);
                                    spdlog::trace("Element {0} sample value: {1}", i,
                                                  value);
                                    REQUIRE(result == kRocProfVisResultSuccess);
                                    break;
                                }
                                default:
                                {
                                    break;
                                }
                            }

                            uint64_t num_child = 0;
                            result             = rocprofvis_controller_get_uint64(
                                entry, kRPVControllerSampleNumChildren, 0, &num_child);
                            spdlog::trace("Element {0} sample num children: {1}", i,
                                          num_child);
                            REQUIRE(result == kRocProfVisResultSuccess);
                            REQUIRE(num_child == 0);
                            break;
                        }
                        default:
                        {
                            break;
                        }
                    }
                }

                spdlog::info("Free Future");
                rocprofvis_controller_future_free(future);
            }
        }
    }

    SECTION("Controller Load Whole Graph Data")
    { 
        uint64_t num_tracks = 0;
        auto result = rocprofvis_controller_get_uint64(m_controller, kRPVControllerNumTracks, 0, &num_tracks);
        spdlog::info("Get num tracks: {0}", num_tracks);
        REQUIRE(result == kRocProfVisResultSuccess);
        REQUIRE(num_tracks > 0);

        rocprofvis_handle_t* timeline_handle = nullptr;
        result = rocprofvis_controller_get_object(m_controller, kRPVControllerTimeline, 0, &timeline_handle);
        spdlog::info("Get timeline: {0}", (void*)timeline_handle);
        REQUIRE(result == kRocProfVisResultSuccess);
        REQUIRE(timeline_handle);

        double start_ts = 0;
        result = rocprofvis_controller_get_double(timeline_handle, kRPVControllerTimelineMinTimestamp, 0, &start_ts);
        REQUIRE(result == kRocProfVisResultSuccess);

        double end_ts = 0;
        result = rocprofvis_controller_get_double(timeline_handle, kRPVControllerTimelineMaxTimestamp, 0, &end_ts);
        REQUIRE(result == kRocProfVisResultSuccess);

        uint64_t num_graphs = 0;
        result = rocprofvis_controller_get_uint64(timeline_handle, kRPVControllerTimelineNumGraphs, 0, &num_graphs);
        REQUIRE(result == kRocProfVisResultSuccess);
        REQUIRE(num_graphs > 0);

        for (uint32_t i = 0; i < (uint32_t)num_graphs; i++)
        {
            rocprofvis_handle_t* graph_handle = nullptr;
            result = rocprofvis_controller_get_object(timeline_handle, kRPVControllerTimelineGraphIndexed, i, &graph_handle);
            REQUIRE(result == kRocProfVisResultSuccess);
            REQUIRE(graph_handle);

            spdlog::info("Allocating Array");
            rocprofvis_controller_array_t* array = rocprofvis_controller_array_alloc(0);
            REQUIRE(array != nullptr);

            spdlog::info("Allocating Future");
            rocprofvis_controller_future_t* future = rocprofvis_controller_future_alloc();
            REQUIRE(future != nullptr);

            result = rocprofvis_controller_graph_fetch_async(m_controller, graph_handle, start_ts, end_ts, 1000, future, array);
            REQUIRE(result == kRocProfVisResultSuccess);

            spdlog::info("Wait for future");
            result = rocprofvis_controller_future_wait(future, FLT_MAX);
            REQUIRE(result == kRocProfVisResultSuccess);

            uint64_t future_result = 0;
            result = rocprofvis_controller_get_uint64(future, kRPVControllerFutureResult, 0, &future_result);
            spdlog::info("Get future result: {0}", future_result);
            REQUIRE(result == kRocProfVisResultSuccess);
            REQUIRE(future_result == kRocProfVisResultSuccess);

            uint64_t num_results = 0;
            result = rocprofvis_controller_get_uint64(array, kRPVControllerArrayNumEntries, 0, &num_results);
            spdlog::info("Get num elements loaded from track: {0}", num_results);
            REQUIRE(result == kRocProfVisResultSuccess);

            for (uint32_t idx = 0; idx < num_results; idx++)
            {
                rocprofvis_handle_t* entry = nullptr;
                result = rocprofvis_controller_get_object(array, kRPVControllerArrayEntryIndexed, idx, &entry);
                REQUIRE(result == kRocProfVisResultSuccess);
                REQUIRE(entry);
            }

            rocprofvis_controller_array_free(array);
            rocprofvis_controller_future_free(future);
        }
    }

    SECTION("Controller Load Single Track Table Data")
    {
        rocprofvis_handle_t* table_handle = nullptr;
        auto result = rocprofvis_controller_get_object(m_controller, kRPVControllerEventTable, 0, &table_handle);
        REQUIRE(result == kRocProfVisResultSuccess);
        REQUIRE(table_handle);

        spdlog::info("Get track 0");
        rocprofvis_handle_t* track_handle = nullptr;
        result                            = rocprofvis_controller_get_object(
            m_controller, kRPVControllerTrackIndexed, 0, &track_handle);
        REQUIRE(result == kRocProfVisResultSuccess);
        REQUIRE(track_handle != nullptr);

        double min_time = 0;
        result          = rocprofvis_controller_get_double(
            track_handle, kRPVControllerTrackMinTimestamp, 0, &min_time);
        spdlog::info("Get track min time: {0}", min_time);
        REQUIRE(result == kRocProfVisResultSuccess);

        double max_time = 0;
        result          = rocprofvis_controller_get_double(
            track_handle, kRPVControllerTrackMaxTimestamp, 0, &max_time);
        spdlog::info("Get track max time: {0}", max_time);
        REQUIRE(result == kRocProfVisResultSuccess);

        uint64_t track_num_entries = 0;
        result                     = rocprofvis_controller_get_uint64(
            track_handle, kRPVControllerTrackNumberOfEntries, 0, &track_num_entries);
        spdlog::info("Get track num entries: {0}", track_num_entries);
        REQUIRE(result == kRocProfVisResultSuccess);

        auto args = rocprofvis_controller_arguments_alloc();
        REQUIRE(args != nullptr);

        result = rocprofvis_controller_set_uint64(args, kRPVControllerTableArgsType, 0,
                                                  kRPVControllerTableTypeEvents);
        REQUIRE(result == kRocProfVisResultSuccess);

        result = rocprofvis_controller_set_uint64(args, kRPVControllerTableArgsNumTracks,
                                                  0, 1);
        REQUIRE(result == kRocProfVisResultSuccess);

        result = rocprofvis_controller_set_object(args, kRPVControllerTableArgsTracksIndexed, 0, track_handle);
        REQUIRE(result == kRocProfVisResultSuccess);

        result = rocprofvis_controller_set_double(args, kRPVControllerTableArgsStartTime, 0,
                                                  min_time);
        REQUIRE(result == kRocProfVisResultSuccess);

        result = rocprofvis_controller_set_double(args, kRPVControllerTableArgsEndTime,
                                                  0, max_time);
        REQUIRE(result == kRocProfVisResultSuccess);

        result = rocprofvis_controller_set_uint64(args, kRPVControllerTableArgsSortColumn,
                                                  0, 0);
        REQUIRE(result == kRocProfVisResultSuccess);

        result = rocprofvis_controller_set_uint64(args, kRPVControllerTableArgsSortOrder,
                                                  0, kRPVControllerSortOrderAscending);
        REQUIRE(result == kRocProfVisResultSuccess);

        spdlog::info("Allocating Array");
        auto array = rocprofvis_controller_array_alloc(0);
        REQUIRE(array != nullptr);

        spdlog::info("Allocating Future");
        rocprofvis_controller_future_t* future = rocprofvis_controller_future_alloc();
        REQUIRE(future != nullptr);

        result = rocprofvis_controller_table_fetch_async(m_controller, table_handle, args, future, array);
        REQUIRE(result == kRocProfVisResultSuccess);

        spdlog::info("Wait for future");
        result = rocprofvis_controller_future_wait(future, FLT_MAX);
        REQUIRE(result == kRocProfVisResultSuccess);

        uint64_t future_result = 0;
        result = rocprofvis_controller_get_uint64(future, kRPVControllerFutureResult, 0,
                                                  &future_result);
        spdlog::info("Get future result: {0}", future_result);
        REQUIRE(result == kRocProfVisResultSuccess);
        REQUIRE(future_result == kRocProfVisResultSuccess);

        uint64_t num_columns = 0;
        result               = rocprofvis_controller_get_uint64(
            table_handle, kRPVControllerTableNumColumns, 0, &num_columns);
        REQUIRE(result == kRocProfVisResultSuccess);

        uint64_t num_rows = 0;
        result            = rocprofvis_controller_get_uint64(
            table_handle, kRPVControllerTableNumRows, 0, &num_rows);
        REQUIRE(result == kRocProfVisResultSuccess);

        {
            MULTI_LINE_LOG_START;
            for(int i = 0; i < num_columns; i++)
            {
                uint32_t len = 0;
                result       = rocprofvis_controller_get_string(
                    table_handle, kRPVControllerTableColumnHeaderIndexed, i, nullptr,
                    &len);
                REQUIRE(result == kRocProfVisResultSuccess);

                std::string name;
                name.resize(len);

                result = rocprofvis_controller_get_string(
                    table_handle, kRPVControllerTableColumnHeaderIndexed, i,
                    const_cast<char*>(name.c_str()), &len);
                REQUIRE(result == kRocProfVisResultSuccess);

                MULTI_LINE_LOG(" {0:20s} |", name.c_str());
            }
            spdlog::info(MULTI_LINE_LOG_ARGS);
        }

        for (uint32_t i = 0; i < num_rows; i++)
        {
            MULTI_LINE_LOG_START;
            rocprofvis_handle_t* row_array = nullptr;
            result = rocprofvis_controller_get_object(array, kRPVControllerArrayEntryIndexed, i, &row_array);
            REQUIRE(result == kRocProfVisResultSuccess);
            REQUIRE(row_array);
            for (uint32_t j = 0; j < num_columns; j++)
            {

                uint64_t column_type = 0;
                result = rocprofvis_controller_get_uint64(table_handle, kRPVControllerTableColumnTypeIndexed, j, &column_type);
                REQUIRE(result == kRocProfVisResultSuccess);

                switch (column_type)
                {
                    case kRPVControllerPrimitiveTypeUInt64:
                    {
                        uint64_t value = 0;
                        result       = rocprofvis_controller_get_uint64(
                            row_array, kRPVControllerArrayEntryIndexed, j, &value);
                        REQUIRE(result == kRocProfVisResultSuccess);

                        MULTI_LINE_LOG(" {0} |", value);
                        break;
                    }
                    case kRPVControllerPrimitiveTypeDouble:
                    {
                        double value = 0;
                        result         = rocprofvis_controller_get_double(
                            row_array, kRPVControllerArrayEntryIndexed, j, &value);
                        REQUIRE(result == kRocProfVisResultSuccess);

                        MULTI_LINE_LOG(" {0} |", value);
                        break;
                    }
                    case kRPVControllerPrimitiveTypeString:
                    {
                        uint32_t len = 0;
                        result       = rocprofvis_controller_get_string(
                            row_array, kRPVControllerArrayEntryIndexed, j, nullptr,
                            &len);
                        REQUIRE(result == kRocProfVisResultSuccess);

                        std::string name;
                        name.resize(len);

                        result = rocprofvis_controller_get_string(
                            row_array, kRPVControllerArrayEntryIndexed, j,
                            const_cast<char*>(name.c_str()), &len);
                        REQUIRE(result == kRocProfVisResultSuccess);

                        MULTI_LINE_LOG(" {0:20s} |", name.c_str());
                        break;
                    }
                    case kRPVControllerPrimitiveTypeObject:
                    default:
                    {
                        break;
                    }
                }
            }
            spdlog::info(MULTI_LINE_LOG_ARGS);
        }

        spdlog::info("Free Future");
        rocprofvis_controller_future_free(future);

        spdlog::info("Free Array");
        rocprofvis_controller_array_free(array);

        spdlog::info("Free Args");
        rocprofvis_controller_arguments_free(args);
    }

    SECTION("Controller Load Multiple Sample Track Table Data")
    {
        rocprofvis_handle_t* table_handle = nullptr;
        auto                 result       = rocprofvis_controller_get_object(
            m_controller, kRPVControllerSampleTable, 0, &table_handle);
        REQUIRE(result == kRocProfVisResultSuccess);
        REQUIRE(table_handle);

        uint64_t num_tracks = 0;
        result = rocprofvis_controller_get_uint64(m_controller, kRPVControllerNumTracks, 0, &num_tracks);
        spdlog::info("Get num tracks: {0}", num_tracks);
        REQUIRE(result == kRocProfVisResultSuccess);
        REQUIRE(num_tracks > 0);

        double min_ts = DBL_MAX;
        double max_ts = DBL_MIN;

        auto args = rocprofvis_controller_arguments_alloc();
        REQUIRE(args != nullptr);

        result = rocprofvis_controller_set_uint64(args, kRPVControllerTableArgsType, 0,
                                                  kRPVControllerTableTypeSamples);
        REQUIRE(result == kRocProfVisResultSuccess);

        uint32_t num_sample_tracks = 0;
        uint64_t num_entries = 0;
        for(uint32_t i = 0; i < num_tracks; i++)
        {
            spdlog::info("Get track {0}", i);
            rocprofvis_handle_t* track_handle = nullptr;
            result                            = rocprofvis_controller_get_object(
                m_controller, kRPVControllerTrackIndexed, i, &track_handle);
            REQUIRE(result == kRocProfVisResultSuccess);
            REQUIRE(track_handle != nullptr);

            uint64_t track_type = 0;
            result = rocprofvis_controller_get_uint64(track_handle, kRPVControllerTrackType,
                                                      0, &track_type);
            REQUIRE(result == kRocProfVisResultSuccess);

            if(track_type == kRPVControllerTrackTypeSamples)
            {
                double min_time = 0;
                result          = rocprofvis_controller_get_double(
                    track_handle, kRPVControllerTrackMinTimestamp, 0, &min_time);
                spdlog::info("Get track min time: {0}", min_time);
                REQUIRE(result == kRocProfVisResultSuccess);
                min_ts = std::min(min_time, min_ts);

                double max_time = 0;
                result          = rocprofvis_controller_get_double(
                    track_handle, kRPVControllerTrackMaxTimestamp, 0, &max_time);
                spdlog::info("Get track max time: {0}", max_time);
                REQUIRE(result == kRocProfVisResultSuccess);
                max_ts = std::max(max_time, max_ts);

                uint64_t track_num_entries = 0;
                result                     = rocprofvis_controller_get_uint64(
                    track_handle, kRPVControllerTrackNumberOfEntries, 0,
                    &track_num_entries);
                spdlog::info("Get track num entries: {0}", track_num_entries);
                REQUIRE(result == kRocProfVisResultSuccess);
                num_entries += track_num_entries;

                result = rocprofvis_controller_set_uint64(
                    args, kRPVControllerTableArgsNumTracks, 0, num_sample_tracks + 1);
                REQUIRE(result == kRocProfVisResultSuccess);

                result = rocprofvis_controller_set_object(
                    args, kRPVControllerTableArgsTracksIndexed, num_sample_tracks++,
                    track_handle);
                REQUIRE(result == kRocProfVisResultSuccess);
            }
        }

        result = rocprofvis_controller_set_double(args, kRPVControllerTableArgsStartTime,
                                                  0, min_ts);
        REQUIRE(result == kRocProfVisResultSuccess);

        result = rocprofvis_controller_set_double(args, kRPVControllerTableArgsEndTime, 0,
                                                  max_ts);
        REQUIRE(result == kRocProfVisResultSuccess);

        result = rocprofvis_controller_set_uint64(args, kRPVControllerTableArgsSortColumn,
                                                  0,
                                                  0);
        REQUIRE(result == kRocProfVisResultSuccess);

        result = rocprofvis_controller_set_uint64(args, kRPVControllerTableArgsSortOrder,
                                                  0, kRPVControllerSortOrderAscending);
        REQUIRE(result == kRocProfVisResultSuccess);

        spdlog::info("Allocating Array");
        auto array = rocprofvis_controller_array_alloc(0);
        REQUIRE(array != nullptr);

        spdlog::info("Allocating Future");
        rocprofvis_controller_future_t* future = rocprofvis_controller_future_alloc();
        REQUIRE(future != nullptr);

        result = rocprofvis_controller_table_fetch_async(m_controller, table_handle, args,
                                                         future, array);
        REQUIRE(result == kRocProfVisResultSuccess);

        spdlog::info("Wait for future");
        result = rocprofvis_controller_future_wait(future, FLT_MAX);
        REQUIRE(result == kRocProfVisResultSuccess);

        uint64_t future_result = 0;
        result = rocprofvis_controller_get_uint64(future, kRPVControllerFutureResult, 0,
                                                  &future_result);
        spdlog::info("Get future result: {0}", future_result);
        REQUIRE(result == kRocProfVisResultSuccess);
        REQUIRE(future_result == kRocProfVisResultSuccess);

        uint64_t num_columns = 0;
        result               = rocprofvis_controller_get_uint64(
            table_handle, kRPVControllerTableNumColumns, 0, &num_columns);
        REQUIRE(result == kRocProfVisResultSuccess);

        uint64_t num_rows = 0;
        result            = rocprofvis_controller_get_uint64(
            table_handle, kRPVControllerTableNumRows, 0, &num_rows);
        REQUIRE(result == kRocProfVisResultSuccess);

        {
            MULTI_LINE_LOG_START;
            for(int i = 0; i < num_columns; i++)
            {
                uint32_t len = 0;
                result       = rocprofvis_controller_get_string(
                    table_handle, kRPVControllerTableColumnHeaderIndexed, i, nullptr,
                    &len);
                REQUIRE(result == kRocProfVisResultSuccess);

                std::string name;
                name.resize(len);

                result = rocprofvis_controller_get_string(
                    table_handle, kRPVControllerTableColumnHeaderIndexed, i,
                    const_cast<char*>(name.c_str()), &len);
                REQUIRE(result == kRocProfVisResultSuccess);

                MULTI_LINE_LOG(" {0:20s} |", name.c_str());
            }
            spdlog::info(MULTI_LINE_LOG_ARGS);
        }

        for(uint32_t i = 0; i < num_rows; i++)
        {
            MULTI_LINE_LOG_START;
            rocprofvis_handle_t* row_array = nullptr;
            result                         = rocprofvis_controller_get_object(
                array, kRPVControllerArrayEntryIndexed, i, &row_array);
            REQUIRE(result == kRocProfVisResultSuccess);
            REQUIRE(row_array);
            for(uint32_t j = 0; j < num_columns; j++)
            {
                uint64_t column_type = 0;
                result               = rocprofvis_controller_get_uint64(
                    table_handle, kRPVControllerTableColumnTypeIndexed, j, &column_type);
                REQUIRE(result == kRocProfVisResultSuccess);

                switch(column_type)
                {
                    case kRPVControllerPrimitiveTypeUInt64:
                    {
                        uint64_t value = 0;
                        result         = rocprofvis_controller_get_uint64(
                            row_array, kRPVControllerArrayEntryIndexed, j, &value);
                        REQUIRE(result == kRocProfVisResultSuccess);

                        MULTI_LINE_LOG(" {0} |", value);
                        break;
                    }
                    case kRPVControllerPrimitiveTypeDouble:
                    {
                        double value = 0;
                        result       = rocprofvis_controller_get_double(
                            row_array, kRPVControllerArrayEntryIndexed, j, &value);
                        REQUIRE(result == kRocProfVisResultSuccess);

                        MULTI_LINE_LOG(" {0} |", value);
                        break;
                    }
                    case kRPVControllerPrimitiveTypeString:
                    {
                        uint32_t len = 0;
                        result       = rocprofvis_controller_get_string(
                            row_array, kRPVControllerArrayEntryIndexed, j, nullptr, &len);
                        REQUIRE(result == kRocProfVisResultSuccess);

                        std::string name;
                        name.resize(len);

                        result = rocprofvis_controller_get_string(
                            row_array, kRPVControllerArrayEntryIndexed, j,
                            const_cast<char*>(name.c_str()), &len);
                        REQUIRE(result == kRocProfVisResultSuccess);

                        MULTI_LINE_LOG(" {0:20s} |", name.c_str());
                        break;
                    }
                    case kRPVControllerPrimitiveTypeObject:
                    default:
                    {
                        break;
                    }
                }
            }
            spdlog::info(MULTI_LINE_LOG_ARGS);
        }

        spdlog::info("Free Future");
        rocprofvis_controller_future_free(future);

        spdlog::info("Free Array");
        rocprofvis_controller_array_free(array);

        spdlog::info("Free Args");
        rocprofvis_controller_arguments_free(args);
    }

    SECTION("Controller Load Ranges Of Track Data")
    {
        uint64_t num_tracks = 0;
        auto result = rocprofvis_controller_get_uint64(m_controller, kRPVControllerNumTracks, 0, &num_tracks);
        spdlog::info("Get num tracks: {0}", num_tracks);
        REQUIRE(result == kRocProfVisResultSuccess);
        REQUIRE(num_tracks > 0);

        for(uint32_t track_idx = 0; track_idx < num_tracks; track_idx++)
        {
            spdlog::info("Get track {0}", track_idx);
            rocprofvis_handle_t* track_handle = nullptr;
            auto                 result       = rocprofvis_controller_get_object(
                m_controller, kRPVControllerTrackIndexed, track_idx, &track_handle);
            REQUIRE(result == kRocProfVisResultSuccess);
            REQUIRE(track_handle != nullptr);

            double min_time = 0;
            result          = rocprofvis_controller_get_double(
                track_handle, kRPVControllerTrackMinTimestamp, 0, &min_time);
            spdlog::info("Get track min time: {0}", min_time);
            REQUIRE(result == kRocProfVisResultSuccess);

            double max_time = 0;
            result          = rocprofvis_controller_get_double(
                track_handle, kRPVControllerTrackMaxTimestamp, 0, &max_time);
            spdlog::info("Get track max time: {0}", max_time);
            REQUIRE(result == kRocProfVisResultSuccess);

            uint64_t track_num_entries = 0;
            result                     = rocprofvis_controller_get_uint64(
                track_handle, kRPVControllerTrackNumberOfEntries, 0, &track_num_entries);
            spdlog::info("Get track num entries: {0}", track_num_entries);
            REQUIRE(result == kRocProfVisResultSuccess);
            if(track_num_entries > 0)
            {
                uint64_t track_type = 0;
                result              = rocprofvis_controller_get_uint64(
                    track_handle, kRPVControllerTrackType, 0, &track_type);
                spdlog::info("Get track type: {0} {1}", track_type,
                             track_type == kRPVControllerTrackTypeEvents ? "Events"
                                                                         : "Samples");
                REQUIRE(result == kRocProfVisResultSuccess);
                REQUIRE((track_type == kRPVControllerTrackTypeEvents ||
                         track_type == kRPVControllerTrackTypeSamples));

                double start_ts = min_time;
                double RANGE    = 1000 * 1000;

                uint64_t total = 0;

                while(start_ts < max_time && total < track_num_entries)
                {
                    double end_ts = std::min(start_ts + RANGE, max_time);
                    spdlog::trace("Fetching Range {0}-{1} (Total: {2}, Fetched: {3})",
                                 start_ts, end_ts, track_num_entries, total);

                    spdlog::trace("Allocating Array");
                    rocprofvis_controller_array_t* array =
                        rocprofvis_controller_array_alloc(0);
                    REQUIRE(array != nullptr);

                    spdlog::trace("Allocating Future");
                    rocprofvis_controller_future_t* future =
                        rocprofvis_controller_future_alloc();
                    REQUIRE(future != nullptr);

                    spdlog::trace("Fetch track data");
                    result = rocprofvis_controller_track_fetch_async(
                        m_controller, (rocprofvis_controller_track_t*) track_handle,
                        start_ts, end_ts, future, array);
                    REQUIRE(array != nullptr);

                    spdlog::trace("Wait for future");
                    result = rocprofvis_controller_future_wait(future, FLT_MAX);
                    REQUIRE(result == kRocProfVisResultSuccess);

                    uint64_t future_result = 0;
                    result                 = rocprofvis_controller_get_uint64(
                        future, kRPVControllerFutureResult, 0, &future_result);
                    spdlog::trace("Get future result: {0}", future_result);
                    REQUIRE(result == kRocProfVisResultSuccess);
                    REQUIRE(future_result == kRocProfVisResultSuccess);

                    uint64_t num_results = 0;
                    result               = rocprofvis_controller_get_uint64(
                        array, kRPVControllerArrayNumEntries, 0, &num_results);
                    spdlog::trace("Get num elements loaded from track: {0}", num_results);
                    REQUIRE(result == kRocProfVisResultSuccess);

                    for(uint64_t i = 0; i < num_results; i++)
                    {
                        rocprofvis_handle_t* entry = nullptr;
                        result                     = rocprofvis_controller_get_object(
                            array, kRPVControllerArrayEntryIndexed, i, &entry);
                        REQUIRE(result == kRocProfVisResultSuccess);
                        REQUIRE(entry != nullptr);

                        double begin_ts = 0;
                        double stop_ts  = 0;
                        switch(track_type)
                        {
                            case kRPVControllerTrackTypeEvents:
                            {
                                result = rocprofvis_controller_get_double(
                                    entry, kRPVControllerEventStartTimestamp, 0,
                                    &begin_ts);
                                REQUIRE(result == kRocProfVisResultSuccess);
                                REQUIRE(begin_ts >= min_time);
                                REQUIRE(begin_ts <= max_time);

                                result = rocprofvis_controller_get_double(
                                    entry, kRPVControllerEventEndTimestamp, 0, &stop_ts);
                                REQUIRE(result == kRocProfVisResultSuccess);
                                REQUIRE(stop_ts >= min_time);
                                REQUIRE(stop_ts <= max_time);
                                break;
                            }
                            case kRPVControllerTrackTypeSamples:
                            {
                                result = rocprofvis_controller_get_double(
                                    entry, kRPVControllerSampleTimestamp, 0, &begin_ts);
                                REQUIRE(result == kRocProfVisResultSuccess);
                                REQUIRE(begin_ts >= min_time);
                                REQUIRE(begin_ts <= max_time);

                                stop_ts = begin_ts;
                                break;
                            }
                            default:
                            {
                                break;
                            }
                        }

                        if(!(begin_ts <= end_ts && stop_ts >= start_ts))
                        {
                            spdlog::info("Idx: {0} Range: {1}-{2} Entry: {3}-{4}", i,
                                         start_ts, end_ts, begin_ts, stop_ts);
                        }
                        REQUIRE((begin_ts <= end_ts && stop_ts >= start_ts));
                    }

                    rocprofvis_controller_array_t* track_data = m_track_data[track_idx];
                    REQUIRE(track_data != nullptr);

                    uint64_t range_idx = 0;
                    for(uint64_t i = 0; i < track_num_entries; i++)
                    {
                        rocprofvis_handle_t* entry = nullptr;
                        result                     = rocprofvis_controller_get_object(
                            track_data, kRPVControllerArrayEntryIndexed, i, &entry);
                        REQUIRE(result == kRocProfVisResultSuccess);
                        REQUIRE(entry != nullptr);

                        double begin_ts = 0;
                        double stop_ts  = 0;
                        switch(track_type)
                        {
                            case kRPVControllerTrackTypeEvents:
                            {
                                result = rocprofvis_controller_get_double(
                                    entry, kRPVControllerEventStartTimestamp, 0,
                                    &begin_ts);
                                REQUIRE(result == kRocProfVisResultSuccess);
                                REQUIRE(begin_ts >= min_time);
                                REQUIRE(begin_ts <= max_time);

                                result = rocprofvis_controller_get_double(
                                    entry, kRPVControllerEventEndTimestamp, 0, &stop_ts);
                                REQUIRE(result == kRocProfVisResultSuccess);
                                REQUIRE(stop_ts >= min_time);
                                REQUIRE(stop_ts <= max_time);
                                break;
                            }
                            case kRPVControllerTrackTypeSamples:
                            {
                                result = rocprofvis_controller_get_double(
                                    entry, kRPVControllerSampleTimestamp, 0, &begin_ts);
                                REQUIRE(result == kRocProfVisResultSuccess);
                                REQUIRE(begin_ts >= min_time);
                                REQUIRE(begin_ts <= max_time);

                                stop_ts = begin_ts;
                                break;
                            }
                            default:
                            {
                                break;
                            }
                        }

                        if(begin_ts <= end_ts && stop_ts >= start_ts)
                        {
                            range_idx++;
                        }
                    }

                    if(range_idx != num_results)
                    {
                        for(uint64_t i = 0; i < num_results; i++)
                        {
                            rocprofvis_handle_t* entry = nullptr;
                            result                     = rocprofvis_controller_get_object(
                                array, kRPVControllerArrayEntryIndexed, i, &entry);
                            REQUIRE(result == kRocProfVisResultSuccess);
                            REQUIRE(entry != nullptr);

                            double begin_ts = 0;
                            double stop_ts  = 0;
                            switch(track_type)
                            {
                                case kRPVControllerTrackTypeEvents:
                                {
                                    result = rocprofvis_controller_get_double(
                                        entry, kRPVControllerEventStartTimestamp, 0,
                                        &begin_ts);
                                    REQUIRE(result == kRocProfVisResultSuccess);
                                    REQUIRE(begin_ts >= min_time);
                                    REQUIRE(begin_ts <= max_time);

                                    result = rocprofvis_controller_get_double(
                                        entry, kRPVControllerEventEndTimestamp, 0,
                                        &stop_ts);
                                    REQUIRE(result == kRocProfVisResultSuccess);
                                    REQUIRE(stop_ts >= min_time);
                                    REQUIRE(stop_ts <= max_time);
                                    break;
                                }
                                case kRPVControllerTrackTypeSamples:
                                {
                                    result = rocprofvis_controller_get_double(
                                        entry, kRPVControllerSampleTimestamp, 0,
                                        &begin_ts);
                                    REQUIRE(result == kRocProfVisResultSuccess);
                                    REQUIRE(begin_ts >= min_time);
                                    REQUIRE(begin_ts <= max_time);

                                    stop_ts = begin_ts;
                                    break;
                                }
                                default:
                                {
                                    break;
                                }
                            }

                            spdlog::info("Idx: {0} Range: {1}-{2} Entry: {3}-{4}", i,
                                         start_ts, end_ts, begin_ts, stop_ts);
                        }

                        for(uint64_t i = 0; i < track_num_entries; i++)
                        {
                            rocprofvis_handle_t* entry = nullptr;
                            result                     = rocprofvis_controller_get_object(
                                track_data, kRPVControllerArrayEntryIndexed, i, &entry);
                            REQUIRE(result == kRocProfVisResultSuccess);
                            REQUIRE(entry != nullptr);

                            double begin_ts = 0;
                            double stop_ts  = 0;
                            switch(track_type)
                            {
                                case kRPVControllerTrackTypeEvents:
                                {
                                    result = rocprofvis_controller_get_double(
                                        entry, kRPVControllerEventStartTimestamp, 0,
                                        &begin_ts);
                                    REQUIRE(result == kRocProfVisResultSuccess);
                                    REQUIRE(begin_ts >= min_time);
                                    REQUIRE(begin_ts <= max_time);

                                    result = rocprofvis_controller_get_double(
                                        entry, kRPVControllerEventEndTimestamp, 0,
                                        &stop_ts);
                                    REQUIRE(result == kRocProfVisResultSuccess);
                                    REQUIRE(stop_ts >= min_time);
                                    REQUIRE(stop_ts <= max_time);
                                    break;
                                }
                                case kRPVControllerTrackTypeSamples:
                                {
                                    result = rocprofvis_controller_get_double(
                                        entry, kRPVControllerSampleTimestamp, 0,
                                        &begin_ts);
                                    REQUIRE(result == kRocProfVisResultSuccess);
                                    REQUIRE(begin_ts >= min_time);
                                    REQUIRE(begin_ts <= max_time);

                                    stop_ts = begin_ts;
                                    break;
                                }
                                default:
                                {
                                    break;
                                }
                            }

                            if(begin_ts <= end_ts && stop_ts >= start_ts)
                            {
                                spdlog::info(
                                    "Raw Track Idx: {0} Range: {1}-{2} Entry: {3}-{4}", i,
                                    start_ts, end_ts, begin_ts, stop_ts);
                            }
                        }
                    }
                    REQUIRE(range_idx == num_results);
                    total += num_results;

                    spdlog::trace("Free Array");
                    rocprofvis_controller_array_free(array);

                    spdlog::trace("Free Future");
                    rocprofvis_controller_future_free(future);

                    start_ts = end_ts;
                }

                REQUIRE(total >= track_num_entries);
            }
        }
    }

    SECTION("Delete controller")
    {
        uint64_t num_tracks = 0;
        auto result = rocprofvis_controller_get_uint64(m_controller, kRPVControllerNumTracks, 0, &num_tracks);
        spdlog::info("Get num tracks: {0}", num_tracks);
        REQUIRE(result == kRocProfVisResultSuccess);
        REQUIRE(num_tracks > 0);

        for(uint32_t track_idx = 0; track_idx < num_tracks; track_idx++)
        {
            spdlog::info("Free track {0} Array");
            rocprofvis_controller_array_free(m_track_data[track_idx]);
        }

        spdlog::info("Free Controller");
        rocprofvis_controller_free(m_controller);
    }
}
