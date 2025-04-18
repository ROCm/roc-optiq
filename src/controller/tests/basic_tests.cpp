
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

#include <iostream>

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

        spdlog::info("Get track 0");
        rocprofvis_handle_t* track_handle = nullptr;
        result = rocprofvis_controller_get_object(m_controller, kRPVControllerTrackIndexed, 0, &track_handle);
        REQUIRE(result == kRocProfVisResultSuccess);
        REQUIRE(track_handle != nullptr);

        double min_time = 0;
        result = rocprofvis_controller_get_double(track_handle, kRPVControllerTrackMinTimestamp, 0, &min_time);
        spdlog::info("Get track min time: {0}", min_time);
        REQUIRE(result == kRocProfVisResultSuccess);
        
        double max_time = 0;
        result = rocprofvis_controller_get_double(track_handle, kRPVControllerTrackMaxTimestamp, 0, &max_time);
        spdlog::info("Get track max time: {0}", max_time);
        REQUIRE(result == kRocProfVisResultSuccess);

        uint64_t track_num_entries = 0;
        result = rocprofvis_controller_get_uint64(track_handle, kRPVControllerTrackNumberOfEntries, 0, &track_num_entries);
        spdlog::info("Get track num entries: {0}", track_num_entries);
        REQUIRE(result == kRocProfVisResultSuccess);
        REQUIRE(track_num_entries > 0);

        uint64_t track_type = 0;
        result = rocprofvis_controller_get_uint64(track_handle, kRPVControllerTrackType, 0, &track_type);
        spdlog::info("Get track type: {0} {1}", track_type, track_type == kRPVControllerTrackTypeEvents ? "Events" : "Samples");
        REQUIRE(result == kRocProfVisResultSuccess);
        REQUIRE((track_type == kRPVControllerTrackTypeEvents ||
                track_type == kRPVControllerTrackTypeSamples));

        spdlog::info("Allocating Array");
        rocprofvis_controller_array_t* array = rocprofvis_controller_array_alloc(100);
        REQUIRE(array != nullptr);

        spdlog::info("Allocating Future");
        rocprofvis_controller_future_t* future = rocprofvis_controller_future_alloc();
        REQUIRE(future != nullptr);

        spdlog::info("Fetch track data");
        result = rocprofvis_controller_track_fetch_async(m_controller, (rocprofvis_controller_track_t*)track_handle, min_time, max_time, future, array);
        REQUIRE(array != nullptr);

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
        REQUIRE(num_results == track_num_entries);

        for (uint64_t i = 0; i < num_results; i++)
        {
            spdlog::info("Get elements at index: {0}", i);
            rocprofvis_handle_t* entry = nullptr;
            result = rocprofvis_controller_get_object(array, kRPVControllerArrayEntryIndexed, i, &entry);
            REQUIRE(result == kRocProfVisResultSuccess);
            REQUIRE(entry != nullptr);

            switch (track_type)
            {
                case kRPVControllerTrackTypeEvents:
                {
                    uint64_t id = 0;
                    result = rocprofvis_controller_get_uint64(entry, kRPVControllerEventId, 0, &id);
                    spdlog::info("Element {0} event id: {1}", i, id);
                    REQUIRE(result == kRocProfVisResultSuccess);

                    double start_ts = 0;
                    result = rocprofvis_controller_get_double(entry, kRPVControllerEventStartTimestamp, 0, &start_ts);
                    spdlog::info("Element {0} event start time: {1}", i, start_ts);
                    REQUIRE(result == kRocProfVisResultSuccess);
                    REQUIRE(start_ts >= min_time);
                    REQUIRE(start_ts <= max_time);

                    double end_ts = 0;
                    result = rocprofvis_controller_get_double(entry, kRPVControllerEventEndTimestamp, 0, &end_ts);
                    spdlog::info("Element {0} event end time: {1}", i, end_ts);
                    REQUIRE(result == kRocProfVisResultSuccess);
                    REQUIRE(end_ts >= min_time);
                    REQUIRE(end_ts <= max_time);

                    uint32_t name_length = 0;
                    result = rocprofvis_controller_get_string(entry, kRPVControllerEventName, 0, nullptr, &name_length);
                    spdlog::info("Element {0} event name length: {1}", i, name_length);
                    REQUIRE(result == kRocProfVisResultSuccess);
                    REQUIRE(name_length > 0);

                    std::string name;
                    name.resize(name_length);
                    result = rocprofvis_controller_get_string(entry, kRPVControllerEventName, 0, const_cast<char*>(name.c_str()), &name_length);
                    spdlog::info("Element {0} event name: {1}", i, name);
                    REQUIRE(result == kRocProfVisResultSuccess);
                    REQUIRE(name_length == name.size());

                    uint32_t cat_length = 0;
                    result = rocprofvis_controller_get_string(entry, kRPVControllerEventName, 0, nullptr, &cat_length);
                    spdlog::info("Element {0} event category length: {1}", i, cat_length);
                    REQUIRE(result == kRocProfVisResultSuccess);
                    REQUIRE(cat_length > 0);

                    std::string cat;
                    cat.resize(cat_length);
                    result = rocprofvis_controller_get_string(entry, kRPVControllerEventName, 0, const_cast<char*>(cat.c_str()), &cat_length);
                    spdlog::info("Element {0} event category: {1}", i, cat);
                    REQUIRE(result == kRocProfVisResultSuccess);
                    REQUIRE(cat_length == cat.size());

                    uint64_t num_child = 0;
                    result = rocprofvis_controller_get_uint64(entry, kRPVControllerEventNumChildren, 0, &num_child);
                    spdlog::info("Element {0} event num children: {1}", i, num_child);
                    REQUIRE(result == kRocProfVisResultSuccess);
                    REQUIRE(num_child == 0);
                    break;
                }
                case kRPVControllerTrackTypeSamples:
                {
                    uint64_t id = 0;
                    result = rocprofvis_controller_get_uint64(entry, kRPVControllerSampleId, 0, &id);
                    spdlog::info("Element {0} sample id: {1}", i, id);
                    REQUIRE(result == kRocProfVisResultSuccess);

                    uint64_t sample_type = 0;
                    result = rocprofvis_controller_get_uint64(entry, kRPVControllerSampleType, 0, &sample_type);
                    spdlog::info("Element {0} sample type: {1}", i, sample_type);
                    REQUIRE(result == kRocProfVisResultSuccess);
                    REQUIRE(sample_type >= kRPVControllerPrimitiveTypeUInt64);
                    REQUIRE(sample_type <= kRPVControllerPrimitiveTypeDouble);

                    double timestamp = 0;
                    result = rocprofvis_controller_get_double(entry, kRPVControllerSampleTimestamp, 0, &timestamp);
                    spdlog::info("Element {0} sample time: {1}", i, timestamp);
                    REQUIRE(result == kRocProfVisResultSuccess);
                    REQUIRE(timestamp >= min_time);
                    REQUIRE(timestamp <= max_time);

                    switch (sample_type)
                    {
                        case kRPVControllerPrimitiveTypeUInt64:
                        {
                            uint64_t value = 0;
                            result = rocprofvis_controller_get_uint64(entry, kRPVControllerSampleValue, 0, &value);
                            spdlog::info("Element {0} sample value: {1}", i, value);
                            REQUIRE(result == kRocProfVisResultSuccess);
                            break;
                        }
                        case kRPVControllerPrimitiveTypeDouble:
                        {
                            double value = 0;
                            result = rocprofvis_controller_get_double(entry, kRPVControllerSampleValue, 0, &value);
                            spdlog::info("Element {0} sample value: {1}", i, value);
                            REQUIRE(result == kRocProfVisResultSuccess);
                            break;
                        }
                        default:
                        {
                            break;
                        }
                    }

                    uint64_t num_child = 0;
                    result = rocprofvis_controller_get_uint64(entry, kRPVControllerSampleNumChildren, 0, &num_child);
                    spdlog::info("Element {0} sample num children: {1}", i, num_child);
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

        spdlog::info("Free Array");
        rocprofvis_controller_array_free(array);
    }

    SECTION("Delete controller")
    {
        spdlog::info("Free Controller");
        rocprofvis_controller_free(m_controller);
    }
}
