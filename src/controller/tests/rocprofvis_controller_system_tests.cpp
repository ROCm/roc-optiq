// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_c_interface.h"
#include "rocprofvis_controller.h"
#include "rocprofvis_core.h"
#include "system/rocprofvis_controller_event.h"
#include "system/rocprofvis_controller_mem_mgmt.h"
#include "system/rocprofvis_controller_segment.h"
#include <algorithm>
#include <catch2/catch_session.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cfloat>
#include <chrono>
#include <filesystem>
#include <thread>
#include <unordered_map>

std::string g_input_file = "sample/trace_70b_1024_32.rpd";

int
main(int argc, char** argv)
{
    Catch::Session session;

    using namespace Catch::Clara;
    auto cli = session.cli() |
               Opt(g_input_file, "input_file")["--input_file"]("Path to input file");

    // Now pass the new composite back to Catch2 so it uses that
    session.cli(cli);

    // Let Catch2 (using Clara) parse the command line
    int returnCode = session.applyCommandLine(argc, argv);
    if(returnCode != 0)  // Indicates a command line error
        return returnCode;

    std::string log_file = "Testing/Temporary/rocprofvis_controller_system_tests/" +
                           std::filesystem::path(g_input_file).filename().string() +
                           ".txt";
    rocprofvis_core_enable_log(log_file.c_str(), spdlog::level::trace);

    return session.run();
}

// Creates two standalone MemoryManager instances and a SegmentTimeline, fills
// segments with events, then adjusts weight via Configure() to starve the first
// instance's memory limit and trigger LRU eviction.  Validates that segments are
// valid after insertion and invalid after eviction.
TEST_CASE("Memory Manager LRU Eviction")
{
    double   segment_duration = 1000.0;
    uint32_t num_segments     = 4;
    size_t   num_items        = 100000;
    size_t   events_per_seg   = 10;
    size_t   trace_size       = num_items * 128;

    RocProfVis::Controller::SegmentTimeline timeline;
    timeline.SetContext(nullptr);
    timeline.Init(0.0, segment_duration, num_segments, num_items);

    RocProfVis::Controller::MemoryManager mm_a(1);
    mm_a.Init(trace_size);
    mm_a.Configure(1.0);

    int dummy_array = 0;

    for(uint32_t i = 0; i < num_segments; i++)
    {
        double seg_start = i * segment_duration;
        double seg_end   = seg_start + segment_duration;

        auto segment = std::make_unique<RocProfVis::Controller::Segment>(
            kRPVControllerTrackTypeEvents, &timeline);
        segment->SetStartEndTimestamps(seg_start, seg_end);
        segment->SetMaxTimestamp(seg_end);

        for(size_t e = 0; e < events_per_seg; e++)
        {
            double                         ts = seg_start + e;
            RocProfVis::Controller::Event* event =
                mm_a.NewEvent(i * events_per_seg + e, ts, ts + 0.5, &timeline);
            REQUIRE(event != nullptr);
            segment->Insert(ts, 0, event);
        }

        RocProfVis::Controller::Segment* seg_ptr = segment.get();
        rocprofvis_result_t result = timeline.Insert(seg_start, std::move(segment));
        REQUIRE(result == kRocProfVisResultSuccess);

        timeline.SetValid(i, true);
        mm_a.AddLRUReference(&timeline, seg_ptr, 0, &dummy_array);
    }

    spdlog::info("Validating segments are valid after insertion");
    for(uint32_t i = 0; i < num_segments; i++)
    {
        REQUIRE(timeline.IsValid(i));
    }

    RocProfVis::Controller::MemoryManager mm_b(2);
    mm_b.Init(trace_size);
    mm_b.Configure(DBL_MAX);

    spdlog::info("Wait for LRU eviction");
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while(std::chrono::steady_clock::now() < deadline)
    {
        bool all_evicted = true;
        for(uint32_t i = 0; i < num_segments; i++)
        {
            if(timeline.IsValid(i))
            {
                all_evicted = false;
                break;
            }
        }
        if(all_evicted) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    spdlog::info("Validating segments are invalid after eviction");
    for(uint32_t i = 0; i < num_segments; i++)
    {
        REQUIRE_FALSE(timeline.IsValid(i));
    }
}

struct RocProfVisControllerFixture
{
    mutable rocprofvis_controller_t*                    m_controller = nullptr;
    mutable std::vector<rocprofvis_controller_array_t*> m_track_data;
};

TEST_CASE_PERSISTENT_FIXTURE(RocProfVisControllerFixture, "System Trace Controller Tests")
{
    // Allocates a controller handle for the input trace file.
    // Fixture Writes: m_controller
    SECTION("Create Controller")
    {
        spdlog::info("Allocating Controller");
        m_controller = rocprofvis_controller_alloc(g_input_file.c_str());
        REQUIRE(nullptr != m_controller);
    }

    // Loads the trace asynchronously and waits for completion.
    // Fixture Reads: m_controller
    SECTION("Initialisation")
    {
        spdlog::info("Allocating Future");
        rocprofvis_controller_future_t* future = rocprofvis_controller_future_alloc();
        REQUIRE(future != nullptr);

        spdlog::info("Load trace: {0}", g_input_file);
        rocprofvis_result_t result =
            rocprofvis_controller_load_async(m_controller, future);
        REQUIRE(result == kRocProfVisResultSuccess);

        spdlog::info("Wait for load");
        result = rocprofvis_controller_future_wait(future, FLT_MAX);
        REQUIRE(result == kRocProfVisResultSuccess);

        uint64_t future_result = 0;
        result = rocprofvis_controller_get_uint64(future, kRPVControllerFutureResult, 0,
                                                  &future_result);
        spdlog::info("Get future result: {0}", future_result);
        REQUIRE(result == kRocProfVisResultSuccess);
        REQUIRE(future_result == kRocProfVisResultSuccess);

        spdlog::info("Free Future");
        rocprofvis_controller_future_free(future);
    }

    // Iterates all tracks, fetches each track's data asynchronously, and validates
    // event and sample entries (id, timestamps, name, category, children, type, value).
    // Fixture Reads:  m_controller
    // Fixture Writes: m_track_data
    SECTION("Whole Track Data")
    {
        uint64_t            num_tracks = 0;
        rocprofvis_result_t result     = rocprofvis_controller_get_uint64(
            m_controller, kRPVControllerSystemNumTracks, 0, &num_tracks);
        spdlog::info("Get num tracks: {0}", num_tracks);
        REQUIRE(result == kRocProfVisResultSuccess);
        REQUIRE(num_tracks > 0);

        m_track_data.resize(num_tracks);

        for(uint32_t track_idx = 0; track_idx < num_tracks; track_idx++)
        {
            rocprofvis_handle_t* track_handle = nullptr;
            result                            = rocprofvis_controller_get_object(
                m_controller, kRPVControllerSystemTrackIndexed, track_idx, &track_handle);
            REQUIRE(result == kRocProfVisResultSuccess);
            REQUIRE(track_handle != nullptr);

            uint64_t track_id = 0;
            result = rocprofvis_controller_get_uint64(track_handle, kRPVControllerTrackId,
                                                      0, &track_id);
            REQUIRE(result == kRocProfVisResultSuccess);
            spdlog::info("Processing track {0}/{1} (id={2})", track_idx + 1, num_tracks,
                         track_id);

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
                m_track_data[track_idx] = rocprofvis_controller_array_alloc(0);
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
                REQUIRE(result == kRocProfVisResultSuccess);

                spdlog::info("Wait for future");
                result = rocprofvis_controller_future_wait(future, FLT_MAX);
                REQUIRE(result == kRocProfVisResultSuccess);

                uint64_t future_result = 0;
                result                 = rocprofvis_controller_get_uint64(
                    future, kRPVControllerFutureResult, 0, &future_result);
                spdlog::info("Get future result: {0}", future_result);
                REQUIRE(result == kRocProfVisResultSuccess);
                REQUIRE((future_result == kRocProfVisResultSuccess ||
                         future_result == kRocProfVisResultOutOfRange));

                uint64_t num_results = 0;
                result               = rocprofvis_controller_get_uint64(
                    track_data, kRPVControllerArrayNumEntries, 0, &num_results);
                spdlog::info("Get num elements loaded from track: {0}", num_results);
                REQUIRE(result == kRocProfVisResultSuccess);

                for(uint64_t i = 0; i < num_results; i++)
                {
                    spdlog::trace("Processing track entry {0}/{1}", i + 1, num_results);
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
                                entry, kRPVControllerEventCategory, 0, nullptr,
                                &cat_length);
                            spdlog::trace("Element {0} event category length: {1}", i,
                                          cat_length);
                            REQUIRE(result == kRocProfVisResultSuccess);

                            std::string cat;
                            cat.resize(cat_length);
                            result = rocprofvis_controller_get_string(
                                entry, kRPVControllerEventCategory, 0,
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

    // Fetches timeline graphs (line/flame) for the full time range, validates graph
    // type, timestamps, entry counts, and iterates each result entry.
    // Fixture Reads: m_controller
    SECTION("Whole Graph Data")
    {
        uint64_t            num_tracks = 0;
        rocprofvis_result_t result     = rocprofvis_controller_get_uint64(
            m_controller, kRPVControllerSystemNumTracks, 0, &num_tracks);
        spdlog::info("Get num tracks: {0}", num_tracks);
        REQUIRE(result == kRocProfVisResultSuccess);
        REQUIRE(num_tracks > 0);

        rocprofvis_handle_t* timeline_handle = nullptr;
        result                               = rocprofvis_controller_get_object(
            m_controller, kRPVControllerSystemTimeline, 0, &timeline_handle);
        spdlog::info("Get timeline: {0}", (void*) timeline_handle);
        REQUIRE(result == kRocProfVisResultSuccess);
        REQUIRE(timeline_handle);

        double start_ts = 0;
        result          = rocprofvis_controller_get_double(
            timeline_handle, kRPVControllerTimelineMinTimestamp, 0, &start_ts);
        REQUIRE(result == kRocProfVisResultSuccess);

        double end_ts = 0;
        result        = rocprofvis_controller_get_double(
            timeline_handle, kRPVControllerTimelineMaxTimestamp, 0, &end_ts);
        REQUIRE(result == kRocProfVisResultSuccess);

        uint64_t num_graphs = 0;
        result              = rocprofvis_controller_get_uint64(
            timeline_handle, kRPVControllerTimelineNumGraphs, 0, &num_graphs);
        REQUIRE(result == kRocProfVisResultSuccess);
        REQUIRE(num_graphs > 0);

        for(uint32_t i = 0; i < (uint32_t) num_graphs; i++)
        {
            rocprofvis_handle_t* graph_handle = nullptr;
            result                            = rocprofvis_controller_get_object(
                timeline_handle, kRPVControllerTimelineGraphIndexed, i, &graph_handle);
            REQUIRE(result == kRocProfVisResultSuccess);
            REQUIRE(graph_handle);

            uint64_t graph_id = 0;
            result = rocprofvis_controller_get_uint64(graph_handle, kRPVControllerGraphId,
                                                      0, &graph_id);
            REQUIRE(result == kRocProfVisResultSuccess);
            spdlog::info("Processing graph {0}/{1} (id={2})", i + 1, num_graphs,
                         graph_id);

            spdlog::info("Allocating Array");
            rocprofvis_controller_array_t* array = rocprofvis_controller_array_alloc(0);
            REQUIRE(array != nullptr);

            spdlog::info("Allocating Future");
            rocprofvis_controller_future_t* future = rocprofvis_controller_future_alloc();
            REQUIRE(future != nullptr);

            result = rocprofvis_controller_graph_fetch_async(
                m_controller, graph_handle, start_ts, end_ts, 1000, future, array);
            REQUIRE(result == kRocProfVisResultSuccess);

            spdlog::info("Wait for future");
            result = rocprofvis_controller_future_wait(future, FLT_MAX);
            REQUIRE(result == kRocProfVisResultSuccess);

            uint64_t future_result = 0;
            result = rocprofvis_controller_get_uint64(future, kRPVControllerFutureResult,
                                                      0, &future_result);
            spdlog::info("Get future result: {0}", future_result);
            REQUIRE(result == kRocProfVisResultSuccess);
            REQUIRE(future_result == kRocProfVisResultSuccess);

            uint64_t num_results = 0;
            result               = rocprofvis_controller_get_uint64(
                array, kRPVControllerArrayNumEntries, 0, &num_results);
            spdlog::info("Get num elements loaded from track: {0}", num_results);
            REQUIRE(result == kRocProfVisResultSuccess);

            uint64_t graph_type = 0;
            result              = rocprofvis_controller_get_uint64(
                graph_handle, kRPVControllerGraphType, 0, &graph_type);
            REQUIRE(result == kRocProfVisResultSuccess);
            REQUIRE((graph_type == kRPVControllerGraphTypeLine ||
                     graph_type == kRPVControllerGraphTypeFlame));
            spdlog::info("Graph type: {0}", graph_type);

            double graph_start_ts = 0;
            result                = rocprofvis_controller_get_double(
                graph_handle, kRPVControllerGraphStartTimestamp, 0, &graph_start_ts);
            REQUIRE(result == kRocProfVisResultSuccess);

            double graph_end_ts = 0;
            result              = rocprofvis_controller_get_double(
                graph_handle, kRPVControllerGraphEndTimestamp, 0, &graph_end_ts);
            REQUIRE(result == kRocProfVisResultSuccess);
            spdlog::info("Graph timestamps: {0} - {1}", graph_start_ts, graph_end_ts);

            uint64_t graph_num_entries = 0;
            result                     = rocprofvis_controller_get_uint64(
                graph_handle, kRPVControllerGraphNumEntries, 0, &graph_num_entries);
            REQUIRE(result == kRocProfVisResultSuccess);
            spdlog::info("Graph num entries: {0}", graph_num_entries);

            for(uint32_t idx = 0; idx < num_results; idx++)
            {
                spdlog::trace("Processing graph entry {0}/{1}", idx + 1, num_results);
                rocprofvis_handle_t* entry = nullptr;
                result                     = rocprofvis_controller_get_object(
                    array, kRPVControllerArrayEntryIndexed, idx, &entry);
                REQUIRE(result == kRocProfVisResultSuccess);
                REQUIRE(entry);
            }

            rocprofvis_controller_array_free(array);
            rocprofvis_controller_future_free(future);
        }
    }

    // Fetches event-type tracks into a table in paginated chunks, validates column
    // headers, column types, and reads cell values for every row. Exports the last chunk
    // to CSV.
    // Fixture Reads: m_controller
    SECTION("Event Track Table Data")
    {
        rocprofvis_handle_t* table_handle = nullptr;
        rocprofvis_result_t  result       = rocprofvis_controller_get_object(
            m_controller, kRPVControllerSystemEventTable, 0, &table_handle);
        REQUIRE(result == kRocProfVisResultSuccess);
        REQUIRE(table_handle);

        uint64_t num_tracks = 0;
        result              = rocprofvis_controller_get_uint64(
            m_controller, kRPVControllerSystemNumTracks, 0, &num_tracks);
        spdlog::info("Get num tracks: {0}", num_tracks);
        REQUIRE(result == kRocProfVisResultSuccess);
        REQUIRE(num_tracks > 0);

        double min_ts = DBL_MAX;
        double max_ts = DBL_MIN;

        rocprofvis_controller_arguments_t* args = rocprofvis_controller_arguments_alloc();
        REQUIRE(args != nullptr);

        result = rocprofvis_controller_set_uint64(args, kRPVControllerTableArgsType, 0,
                                                  kRPVControllerTableTypeEvents);
        REQUIRE(result == kRocProfVisResultSuccess);

        uint32_t num_event_tracks = 0;
        uint64_t num_entries      = 0;
        for(uint32_t i = 0; i < num_tracks; i++)
        {
            rocprofvis_handle_t* track_handle = nullptr;
            result                            = rocprofvis_controller_get_object(
                m_controller, kRPVControllerSystemTrackIndexed, i, &track_handle);
            REQUIRE(result == kRocProfVisResultSuccess);
            REQUIRE(track_handle != nullptr);

            uint64_t track_id = 0;
            result = rocprofvis_controller_get_uint64(track_handle, kRPVControllerTrackId,
                                                      0, &track_id);
            REQUIRE(result == kRocProfVisResultSuccess);
            spdlog::info("Processing event track {0}/{1} (id={2})", i + 1, num_tracks,
                         track_id);

            uint64_t track_type = 0;
            result              = rocprofvis_controller_get_uint64(
                track_handle, kRPVControllerTrackType, 0, &track_type);
            REQUIRE(result == kRocProfVisResultSuccess);

            if(track_type == kRPVControllerTrackTypeEvents)
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
                    args, kRPVControllerTableArgsNumTracks, 0, num_event_tracks + 1);
                REQUIRE(result == kRocProfVisResultSuccess);

                result = rocprofvis_controller_set_object(
                    args, kRPVControllerTableArgsTracksIndexed, num_event_tracks++,
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
                                                  0, 0);
        REQUIRE(result == kRocProfVisResultSuccess);

        result = rocprofvis_controller_set_uint64(args, kRPVControllerTableArgsSortOrder,
                                                  0, kRPVControllerSortOrderAscending);
        REQUIRE(result == kRocProfVisResultSuccess);

        result = rocprofvis_controller_set_uint64(args, kRPVControllerTableArgsNumOpTypes,
                                                  0, 0);
        REQUIRE(result == kRocProfVisResultSuccess);

        result =
            rocprofvis_controller_set_string(args, kRPVControllerTableArgsWhere, 0, "");
        REQUIRE(result == kRocProfVisResultSuccess);

        result =
            rocprofvis_controller_set_string(args, kRPVControllerTableArgsFilter, 0, "");
        REQUIRE(result == kRocProfVisResultSuccess);

        result =
            rocprofvis_controller_set_string(args, kRPVControllerTableArgsGroup, 0, "");
        REQUIRE(result == kRocProfVisResultSuccess);

        result = rocprofvis_controller_set_string(
            args, kRPVControllerTableArgsGroupColumns, 0, "");
        REQUIRE(result == kRocProfVisResultSuccess);

        result =
            rocprofvis_controller_set_uint64(args, kRPVControllerTableArgsSummary, 0, 0);
        REQUIRE(result == kRocProfVisResultSuccess);

        const uint64_t chunk_size  = 10000;
        uint64_t       num_columns = 0;
        uint64_t       num_rows    = 0;
        uint64_t       start_index = 0;

        do
        {
            spdlog::info("Fetching event table chunk (start_index={0}, num_rows={1})",
                         start_index, num_rows);
            result = rocprofvis_controller_set_uint64(
                args, kRPVControllerTableArgsStartIndex, 0, start_index);
            REQUIRE(result == kRocProfVisResultSuccess);

            result = rocprofvis_controller_set_uint64(
                args, kRPVControllerTableArgsStartCount, 0, chunk_size);
            REQUIRE(result == kRocProfVisResultSuccess);

            spdlog::info("Allocating Array");
            rocprofvis_controller_array_t* array = rocprofvis_controller_array_alloc(0);
            REQUIRE(array != nullptr);

            spdlog::info("Allocating Future");
            rocprofvis_controller_future_t* future = rocprofvis_controller_future_alloc();
            REQUIRE(future != nullptr);

            result = rocprofvis_controller_table_fetch_async(m_controller, table_handle,
                                                             args, future, array);
            REQUIRE(result == kRocProfVisResultSuccess);

            spdlog::info("Wait for future");
            result = rocprofvis_controller_future_wait(future, FLT_MAX);
            REQUIRE(result == kRocProfVisResultSuccess);

            uint64_t future_result = 0;
            result = rocprofvis_controller_get_uint64(future, kRPVControllerFutureResult,
                                                      0, &future_result);
            spdlog::info("Get future result: {0}", future_result);
            REQUIRE(result == kRocProfVisResultSuccess);
            REQUIRE(future_result == kRocProfVisResultSuccess);

            if(start_index == 0)
            {
                result = rocprofvis_controller_get_uint64(
                    table_handle, kRPVControllerTableNumColumns, 0, &num_columns);
                REQUIRE(result == kRocProfVisResultSuccess);

                result = rocprofvis_controller_get_uint64(
                    table_handle, kRPVControllerTableNumRows, 0, &num_rows);
                REQUIRE(result == kRocProfVisResultSuccess);

                for(uint64_t i = 0; i < num_columns; i++)
                {
                    spdlog::trace("Reading column header {0}/{1}", i + 1, num_columns);
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
                }
            }

            uint64_t chunk_rows = 0;
            result              = rocprofvis_controller_get_uint64(
                array, kRPVControllerArrayNumEntries, 0, &chunk_rows);
            REQUIRE(result == kRocProfVisResultSuccess);

            for(uint32_t i = 0; i < chunk_rows; i++)
            {
                spdlog::trace("Processing event table row {0}/{1}", i + 1, chunk_rows);
                rocprofvis_handle_t* row_array = nullptr;
                result                         = rocprofvis_controller_get_object(
                    array, kRPVControllerArrayEntryIndexed, i, &row_array);
                REQUIRE(result == kRocProfVisResultSuccess);
                REQUIRE(row_array);
                for(uint32_t j = 0; j < num_columns; j++)
                {
                    spdlog::trace("Processing event table column {0}/{1}", j + 1,
                                  num_columns);
                    uint64_t column_type = 0;
                    result               = rocprofvis_controller_get_uint64(
                        table_handle, kRPVControllerTableColumnTypeIndexed, j,
                        &column_type);
                    REQUIRE(result == kRocProfVisResultSuccess);

                    switch(column_type)
                    {
                        case kRPVControllerPrimitiveTypeUInt64:
                        {
                            uint64_t value = 0;
                            result         = rocprofvis_controller_get_uint64(
                                row_array, kRPVControllerArrayEntryIndexed, j, &value);
                            REQUIRE(result == kRocProfVisResultSuccess);
                            break;
                        }
                        case kRPVControllerPrimitiveTypeDouble:
                        {
                            double value = 0;
                            result       = rocprofvis_controller_get_double(
                                row_array, kRPVControllerArrayEntryIndexed, j, &value);
                            REQUIRE(result == kRocProfVisResultSuccess);
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
                            break;
                        }
                        case kRPVControllerPrimitiveTypeObject:
                        default:
                        {
                            break;
                        }
                    }
                }
            }

            spdlog::info("Free Future");
            rocprofvis_controller_future_free(future);

            spdlog::info("Free Array");
            rocprofvis_controller_array_free(array);

            start_index += chunk_size;
        } while(start_index < num_rows);

        const char* csv_path = "sample/test_export.csv";

        spdlog::info("Allocating Future");
        rocprofvis_controller_future_t* future = rocprofvis_controller_future_alloc();
        REQUIRE(future != nullptr);

        spdlog::info("Exporting event table to CSV: {0}", csv_path);
        result = rocprofvis_controller_table_export_csv(m_controller, table_handle, args,
                                                        future, csv_path);
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

        REQUIRE(std::filesystem::exists(csv_path));
        uint64_t file_size = std::filesystem::file_size(csv_path);
        spdlog::info("CSV export file size: {0} bytes", file_size);
        REQUIRE(file_size > 0);

        std::filesystem::remove(csv_path);

        spdlog::info("Free Args");
        rocprofvis_controller_arguments_free(args);
    }

    // Fetches sample-type tracks into a table in paginated chunks, validates column
    // headers, column types, and reads cell values for every row. Exports the last chunk
    // to CSV.
    // Fixture Reads: m_controller
    SECTION("Sample Track Table Data")
    {
        rocprofvis_handle_t* table_handle = nullptr;
        rocprofvis_result_t  result       = rocprofvis_controller_get_object(
            m_controller, kRPVControllerSystemSampleTable, 0, &table_handle);
        REQUIRE(result == kRocProfVisResultSuccess);
        REQUIRE(table_handle);

        uint64_t num_tracks = 0;
        result              = rocprofvis_controller_get_uint64(
            m_controller, kRPVControllerSystemNumTracks, 0, &num_tracks);
        spdlog::info("Get num tracks: {0}", num_tracks);
        REQUIRE(result == kRocProfVisResultSuccess);
        REQUIRE(num_tracks > 0);

        double min_ts = DBL_MAX;
        double max_ts = DBL_MIN;

        rocprofvis_controller_arguments_t* args = rocprofvis_controller_arguments_alloc();
        REQUIRE(args != nullptr);

        result = rocprofvis_controller_set_uint64(args, kRPVControllerTableArgsType, 0,
                                                  kRPVControllerTableTypeSamples);
        REQUIRE(result == kRocProfVisResultSuccess);

        uint32_t num_sample_tracks = 0;
        uint64_t num_entries       = 0;
        for(uint32_t i = 0; i < num_tracks; i++)
        {
            rocprofvis_handle_t* track_handle = nullptr;
            result                            = rocprofvis_controller_get_object(
                m_controller, kRPVControllerSystemTrackIndexed, i, &track_handle);
            REQUIRE(result == kRocProfVisResultSuccess);
            REQUIRE(track_handle != nullptr);

            uint64_t track_id = 0;
            result = rocprofvis_controller_get_uint64(track_handle, kRPVControllerTrackId,
                                                      0, &track_id);
            REQUIRE(result == kRocProfVisResultSuccess);
            spdlog::info("Processing sample track {0}/{1} (id={2})", i + 1, num_tracks,
                         track_id);

            uint64_t track_type = 0;
            result              = rocprofvis_controller_get_uint64(
                track_handle, kRPVControllerTrackType, 0, &track_type);
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
                                                  0, 0);
        REQUIRE(result == kRocProfVisResultSuccess);

        result = rocprofvis_controller_set_uint64(args, kRPVControllerTableArgsSortOrder,
                                                  0, kRPVControllerSortOrderAscending);
        REQUIRE(result == kRocProfVisResultSuccess);

        result = rocprofvis_controller_set_uint64(args, kRPVControllerTableArgsNumOpTypes,
                                                  0, 0);
        REQUIRE(result == kRocProfVisResultSuccess);

        result =
            rocprofvis_controller_set_string(args, kRPVControllerTableArgsWhere, 0, "");
        REQUIRE(result == kRocProfVisResultSuccess);

        result =
            rocprofvis_controller_set_string(args, kRPVControllerTableArgsFilter, 0, "");
        REQUIRE(result == kRocProfVisResultSuccess);

        result =
            rocprofvis_controller_set_string(args, kRPVControllerTableArgsGroup, 0, "");
        REQUIRE(result == kRocProfVisResultSuccess);

        result = rocprofvis_controller_set_string(
            args, kRPVControllerTableArgsGroupColumns, 0, "");
        REQUIRE(result == kRocProfVisResultSuccess);

        result =
            rocprofvis_controller_set_uint64(args, kRPVControllerTableArgsSummary, 0, 0);
        REQUIRE(result == kRocProfVisResultSuccess);

        const uint64_t chunk_size  = 10000;
        uint64_t       num_columns = 0;
        uint64_t       num_rows    = 0;
        uint64_t       start_index = 0;

        do
        {
            spdlog::info("Fetching sample table chunk (start_index={0}, num_rows={1})",
                         start_index, num_rows);
            result = rocprofvis_controller_set_uint64(
                args, kRPVControllerTableArgsStartIndex, 0, start_index);
            REQUIRE(result == kRocProfVisResultSuccess);

            result = rocprofvis_controller_set_uint64(
                args, kRPVControllerTableArgsStartCount, 0, chunk_size);
            REQUIRE(result == kRocProfVisResultSuccess);

            spdlog::info("Allocating Array");
            rocprofvis_controller_array_t* array = rocprofvis_controller_array_alloc(0);
            REQUIRE(array != nullptr);

            spdlog::info("Allocating Future");
            rocprofvis_controller_future_t* future = rocprofvis_controller_future_alloc();
            REQUIRE(future != nullptr);

            result = rocprofvis_controller_table_fetch_async(m_controller, table_handle,
                                                             args, future, array);
            REQUIRE(result == kRocProfVisResultSuccess);

            spdlog::info("Wait for future");
            result = rocprofvis_controller_future_wait(future, FLT_MAX);
            REQUIRE(result == kRocProfVisResultSuccess);

            uint64_t future_result = 0;
            result = rocprofvis_controller_get_uint64(future, kRPVControllerFutureResult,
                                                      0, &future_result);
            spdlog::info("Get future result: {0}", future_result);
            REQUIRE(result == kRocProfVisResultSuccess);
            REQUIRE(future_result == kRocProfVisResultSuccess);

            if(start_index == 0)
            {
                result = rocprofvis_controller_get_uint64(
                    table_handle, kRPVControllerTableNumColumns, 0, &num_columns);
                REQUIRE(result == kRocProfVisResultSuccess);

                result = rocprofvis_controller_get_uint64(
                    table_handle, kRPVControllerTableNumRows, 0, &num_rows);
                REQUIRE(result == kRocProfVisResultSuccess);

                for(int i = 0; i < num_columns; i++)
                {
                    spdlog::trace("Reading column header {0}/{1}", i + 1, num_columns);
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
                }
            }

            uint64_t chunk_rows = 0;
            result              = rocprofvis_controller_get_uint64(
                array, kRPVControllerArrayNumEntries, 0, &chunk_rows);
            REQUIRE(result == kRocProfVisResultSuccess);

            for(uint32_t i = 0; i < chunk_rows; i++)
            {
                spdlog::trace("Processing sample table row {0}/{1}", i + 1, chunk_rows);
                rocprofvis_handle_t* row_array = nullptr;
                result                         = rocprofvis_controller_get_object(
                    array, kRPVControllerArrayEntryIndexed, i, &row_array);
                REQUIRE(result == kRocProfVisResultSuccess);
                REQUIRE(row_array);
                for(uint32_t j = 0; j < num_columns; j++)
                {
                    spdlog::trace("Processing sample table column {0}/{1}", j + 1,
                                  num_columns);
                    uint64_t column_type = 0;
                    result               = rocprofvis_controller_get_uint64(
                        table_handle, kRPVControllerTableColumnTypeIndexed, j,
                        &column_type);
                    REQUIRE(result == kRocProfVisResultSuccess);

                    switch(column_type)
                    {
                        case kRPVControllerPrimitiveTypeUInt64:
                        {
                            uint64_t value = 0;
                            result         = rocprofvis_controller_get_uint64(
                                row_array, kRPVControllerArrayEntryIndexed, j, &value);
                            REQUIRE(result == kRocProfVisResultSuccess);
                            break;
                        }
                        case kRPVControllerPrimitiveTypeDouble:
                        {
                            double value = 0;
                            result       = rocprofvis_controller_get_double(
                                row_array, kRPVControllerArrayEntryIndexed, j, &value);
                            REQUIRE(result == kRocProfVisResultSuccess);
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
                            break;
                        }
                        case kRPVControllerPrimitiveTypeObject:
                        default:
                        {
                            break;
                        }
                    }
                }
            }

            spdlog::info("Free Future");
            rocprofvis_controller_future_free(future);

            spdlog::info("Free Array");
            rocprofvis_controller_array_free(array);

            start_index += chunk_size;
        } while(start_index < num_rows);

        const char* csv_path = "sample/test_export.csv";

        spdlog::info("Allocating Future");
        rocprofvis_controller_future_t* future = rocprofvis_controller_future_alloc();
        REQUIRE(future != nullptr);

        spdlog::info("Exporting sample table to CSV: {0}", csv_path);
        result = rocprofvis_controller_table_export_csv(m_controller, table_handle, args,
                                                        future, csv_path);
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

        REQUIRE(std::filesystem::exists(csv_path));
        uint64_t file_size = std::filesystem::file_size(csv_path);
        spdlog::info("CSV export file size: {0} bytes", file_size);
        REQUIRE(file_size > 0);

        std::filesystem::remove(csv_path);

        spdlog::info("Free Args");
        rocprofvis_controller_arguments_free(args);
    }

    // Loads each track in sliding-window time ranges and validates that returned
    // entries fall within the requested range, cross-checking against the full
    // track data loaded earlier.
    // Fixture Reads: m_controller, m_track_data
    SECTION("Track Data Range Read")
    {
        uint64_t            num_tracks = 0;
        rocprofvis_result_t result     = rocprofvis_controller_get_uint64(
            m_controller, kRPVControllerSystemNumTracks, 0, &num_tracks);
        spdlog::info("Get num tracks: {0}", num_tracks);
        REQUIRE(result == kRocProfVisResultSuccess);
        REQUIRE(num_tracks > 0);

        for(uint32_t track_idx = 0; track_idx < num_tracks; track_idx++)
        {
            rocprofvis_handle_t* track_handle = nullptr;
            rocprofvis_result_t  result       = rocprofvis_controller_get_object(
                m_controller, kRPVControllerSystemTrackIndexed, track_idx, &track_handle);
            REQUIRE(result == kRocProfVisResultSuccess);
            REQUIRE(track_handle != nullptr);

            uint64_t track_id = 0;
            result = rocprofvis_controller_get_uint64(track_handle, kRPVControllerTrackId,
                                                      0, &track_id);
            REQUIRE(result == kRocProfVisResultSuccess);
            spdlog::info("Processing range track {0}/{1} (id={2})", track_idx + 1,
                         num_tracks, track_id);

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
                spdlog::info("Get track type: {0}",
                             track_type == kRPVControllerTrackTypeEvents ? "Events"
                                                                         : "Samples");
                REQUIRE(result == kRocProfVisResultSuccess);
                REQUIRE((track_type == kRPVControllerTrackTypeEvents ||
                         track_type == kRPVControllerTrackTypeSamples));

                double start_ts = min_time;
                double RANGE    = (max_time - min_time) / 10;

                uint64_t total = 0;
                while(start_ts < max_time)
                {
                    double end_ts = std::min(start_ts + RANGE, max_time);
                    spdlog::info("Fetching Range {0}-{1}", start_ts, end_ts);

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
                    REQUIRE(result == kRocProfVisResultSuccess);

                    spdlog::trace("Wait for future");
                    result = rocprofvis_controller_future_wait(future, FLT_MAX);
                    REQUIRE(result == kRocProfVisResultSuccess);

                    uint64_t future_result = 0;
                    result                 = rocprofvis_controller_get_uint64(
                        future, kRPVControllerFutureResult, 0, &future_result);
                    spdlog::trace("Get future result: {0}", future_result);
                    REQUIRE(result == kRocProfVisResultSuccess);
                    REQUIRE((future_result == kRocProfVisResultSuccess ||
                             future_result == kRocProfVisResultOutOfRange));

                    uint64_t num_results = 0;
                    result               = rocprofvis_controller_get_uint64(
                        array, kRPVControllerArrayNumEntries, 0, &num_results);
                    spdlog::trace("Get num elements loaded from track: {0}", num_results);
                    REQUIRE(result == kRocProfVisResultSuccess);

                    for(uint64_t i = 0; i < num_results; i++)
                    {
                        spdlog::trace("Validating range entry {0}/{1}", i + 1,
                                      num_results);
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

                                result = rocprofvis_controller_get_double(
                                    entry, kRPVControllerSampleEndTimestamp, 0, &stop_ts);
                                REQUIRE(result == kRocProfVisResultSuccess);
                                REQUIRE(stop_ts >= min_time);
                                REQUIRE(stop_ts <= max_time);
                                break;
                            }
                            default:
                            {
                                break;
                            }
                        }

                        if(!(begin_ts <= end_ts && stop_ts >= start_ts))
                        {
                            spdlog::trace("Idx: {0} Range: {1}-{2} Entry: {3}-{4}", i,
                                          start_ts, end_ts, begin_ts, stop_ts);
                        }
                        REQUIRE((begin_ts <= end_ts && stop_ts >= start_ts));
                    }

                    rocprofvis_controller_array_t* track_data = m_track_data[track_idx];
                    REQUIRE(track_data != nullptr);

                    result = rocprofvis_controller_get_uint64(
                        track_data, kRPVControllerArrayNumEntries, 0, &track_num_entries);
                    REQUIRE(result == kRocProfVisResultSuccess);

                    uint64_t range_idx = 0;
                    for(uint64_t i = 0; i < track_num_entries; i++)
                    {
                        spdlog::trace("Checking track entry against range {0}/{1}", i + 1,
                                      track_num_entries);
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

                                result = rocprofvis_controller_get_double(
                                    entry, kRPVControllerSampleEndTimestamp, 0, &stop_ts);
                                REQUIRE(result == kRocProfVisResultSuccess);
                                REQUIRE(stop_ts >= min_time);
                                REQUIRE(stop_ts <= max_time);
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
                            spdlog::info("Dumping mismatched range result {0}/{1}", i + 1,
                                         num_results);
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

                                    result = rocprofvis_controller_get_double(
                                        entry, kRPVControllerSampleEndTimestamp, 0,
                                        &stop_ts);
                                    REQUIRE(result == kRocProfVisResultSuccess);
                                    REQUIRE(stop_ts >= min_time);
                                    REQUIRE(stop_ts <= max_time);
                                    break;
                                }
                                default:
                                {
                                    break;
                                }
                            }

                            spdlog::trace("Idx: {0} Range: {1}-{2} Entry: {3}-{4}", i,
                                          start_ts, end_ts, begin_ts, stop_ts);
                        }

                        for(uint64_t i = 0; i < track_num_entries; i++)
                        {
                            spdlog::trace("Dumping raw track entry {0}/{1}", i + 1,
                                          track_num_entries);
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

                                    result = rocprofvis_controller_get_double(
                                        entry, kRPVControllerSampleEndTimestamp, 0,
                                        &stop_ts);
                                    REQUIRE(result == kRocProfVisResultSuccess);
                                    REQUIRE(stop_ts >= min_time);
                                    REQUIRE(stop_ts <= max_time);
                                    break;
                                }
                                default:
                                {
                                    break;
                                }
                            }

                            if(begin_ts <= end_ts && stop_ts >= start_ts)
                            {
                                spdlog::trace(
                                    "Raw Track Idx: {0} Range: {1}-{2} Entry: {3}-{4}", i,
                                    start_ts, end_ts, begin_ts, stop_ts);
                            }
                        }
                    }
                    REQUIRE(range_idx == num_results);

                    spdlog::trace("Free Array");
                    rocprofvis_controller_array_free(array);

                    spdlog::trace("Free Future");
                    rocprofvis_controller_future_free(future);

                    start_ts = end_ts;
                }
            }
        }
    }

    // Reads the full system topology: nodes (hostname, OS info), processors (product,
    // type, queues, counters), and processes (command, threads, streams).
    // Fixture Reads: m_controller
    SECTION("Read Topology")
    {
        uint64_t            num_nodes = 0;
        rocprofvis_result_t result    = rocprofvis_controller_get_uint64(
            m_controller, kRPVControllerSystemNumNodes, 0, &num_nodes);
        spdlog::info("Get num nodes: {0}", num_nodes);
        REQUIRE(result == kRocProfVisResultSuccess);
        REQUIRE(num_nodes > 0);

        for(uint64_t ni = 0; ni < num_nodes; ni++)
        {
            rocprofvis_handle_t* node_handle = nullptr;
            result                           = rocprofvis_controller_get_object(
                m_controller, kRPVControllerSystemNodeIndexed, ni, &node_handle);
            REQUIRE(result == kRocProfVisResultSuccess);
            REQUIRE(node_handle != nullptr);

            uint64_t node_id = 0;
            result = rocprofvis_controller_get_uint64(node_handle, kRPVControllerNodeId,
                                                      0, &node_id);
            REQUIRE(result == kRocProfVisResultSuccess);
            spdlog::info("Processing node {0}/{1} (id={2})", ni + 1, num_nodes, node_id);

            uint32_t len = 0;
            result       = rocprofvis_controller_get_string(
                node_handle, kRPVControllerNodeHostName, 0, nullptr, &len);
            REQUIRE(result == kRocProfVisResultSuccess);
            std::string host_name;
            host_name.resize(len);
            result = rocprofvis_controller_get_string(
                node_handle, kRPVControllerNodeHostName, 0,
                const_cast<char*>(host_name.c_str()), &len);
            REQUIRE(result == kRocProfVisResultSuccess);
            spdlog::info("  Node hostname: {0}", host_name);

            len    = 0;
            result = rocprofvis_controller_get_string(
                node_handle, kRPVControllerNodeOSName, 0, nullptr, &len);
            REQUIRE(result == kRocProfVisResultSuccess);
            std::string os_name;
            os_name.resize(len);
            result = rocprofvis_controller_get_string(
                node_handle, kRPVControllerNodeOSName, 0,
                const_cast<char*>(os_name.c_str()), &len);
            REQUIRE(result == kRocProfVisResultSuccess);

            len    = 0;
            result = rocprofvis_controller_get_string(
                node_handle, kRPVControllerNodeOSRelease, 0, nullptr, &len);
            REQUIRE(result == kRocProfVisResultSuccess);
            std::string os_release;
            os_release.resize(len);
            result = rocprofvis_controller_get_string(
                node_handle, kRPVControllerNodeOSRelease, 0,
                const_cast<char*>(os_release.c_str()), &len);
            REQUIRE(result == kRocProfVisResultSuccess);

            len    = 0;
            result = rocprofvis_controller_get_string(
                node_handle, kRPVControllerNodeOSVersion, 0, nullptr, &len);
            REQUIRE(result == kRocProfVisResultSuccess);
            std::string os_version;
            os_version.resize(len);
            result = rocprofvis_controller_get_string(
                node_handle, kRPVControllerNodeOSVersion, 0,
                const_cast<char*>(os_version.c_str()), &len);
            REQUIRE(result == kRocProfVisResultSuccess);
            spdlog::info("  OS: {0} {1} {2}", os_name, os_release, os_version);

            uint64_t num_processors = 0;
            result                  = rocprofvis_controller_get_uint64(
                node_handle, kRPVControllerNodeNumProcessors, 0, &num_processors);
            REQUIRE(result == kRocProfVisResultSuccess);
            spdlog::info("  Num processors: {0}", num_processors);

            for(uint64_t pi = 0; pi < num_processors; pi++)
            {
                rocprofvis_handle_t* processor_handle = nullptr;
                result                                = rocprofvis_controller_get_object(
                    node_handle, kRPVControllerNodeProcessorIndexed, pi,
                    &processor_handle);
                REQUIRE(result == kRocProfVisResultSuccess);
                REQUIRE(processor_handle != nullptr);

                uint64_t proc_id = 0;
                result           = rocprofvis_controller_get_uint64(
                    processor_handle, kRPVControllerProcessorId, 0, &proc_id);
                REQUIRE(result == kRocProfVisResultSuccess);
                spdlog::info("  Processing processor {0}/{1} (id={2})", pi + 1,
                             num_processors, proc_id);

                len    = 0;
                result = rocprofvis_controller_get_string(
                    processor_handle, kRPVControllerProcessorProductName, 0, nullptr,
                    &len);
                REQUIRE(result == kRocProfVisResultSuccess);
                std::string product_name;
                product_name.resize(len);
                result = rocprofvis_controller_get_string(
                    processor_handle, kRPVControllerProcessorProductName, 0,
                    const_cast<char*>(product_name.c_str()), &len);
                REQUIRE(result == kRocProfVisResultSuccess);
                spdlog::info("    Product: {0}", product_name);

                uint64_t proc_type = 0;
                result             = rocprofvis_controller_get_uint64(
                    processor_handle, kRPVControllerProcessorType, 0, &proc_type);
                REQUIRE(result == kRocProfVisResultSuccess);

                uint64_t type_index = 0;
                result              = rocprofvis_controller_get_uint64(
                    processor_handle, kRPVControllerProcessorTypeIndex, 0, &type_index);
                REQUIRE(result == kRocProfVisResultSuccess);
                spdlog::info("    Type: {0} TypeIndex: {1}", proc_type, type_index);

                uint64_t num_queues = 0;
                result              = rocprofvis_controller_get_uint64(
                    processor_handle, kRPVControllerProcessorNumQueues, 0, &num_queues);
                REQUIRE(result == kRocProfVisResultSuccess);

                uint64_t num_counters = 0;
                result                = rocprofvis_controller_get_uint64(
                    processor_handle, kRPVControllerProcessorNumCounters, 0,
                    &num_counters);
                REQUIRE(result == kRocProfVisResultSuccess);
                spdlog::info("    Queues: {0} Counters: {1}", num_queues, num_counters);

                for(uint64_t qi = 0; qi < num_queues; qi++)
                {
                    rocprofvis_handle_t* queue_handle = nullptr;
                    result                            = rocprofvis_controller_get_object(
                        processor_handle, kRPVControllerProcessorQueueIndexed, qi,
                        &queue_handle);
                    REQUIRE(result == kRocProfVisResultSuccess);
                    REQUIRE(queue_handle != nullptr);

                    uint64_t queue_id = 0;
                    result            = rocprofvis_controller_get_uint64(
                        queue_handle, kRPVControllerQueueId, 0, &queue_id);
                    REQUIRE(result == kRocProfVisResultSuccess);

                    len    = 0;
                    result = rocprofvis_controller_get_string(
                        queue_handle, kRPVControllerQueueName, 0, nullptr, &len);
                    REQUIRE(result == kRocProfVisResultSuccess);
                    std::string queue_name;
                    queue_name.resize(len);
                    result = rocprofvis_controller_get_string(
                        queue_handle, kRPVControllerQueueName, 0,
                        const_cast<char*>(queue_name.c_str()), &len);
                    REQUIRE(result == kRocProfVisResultSuccess);

                    rocprofvis_handle_t* queue_track = nullptr;
                    result                           = rocprofvis_controller_get_object(
                        queue_handle, kRPVControllerQueueTrack, 0, &queue_track);
                    REQUIRE(result == kRocProfVisResultSuccess);

                    rocprofvis_handle_t* queue_proc = nullptr;
                    result                          = rocprofvis_controller_get_object(
                        queue_handle, kRPVControllerQueueProcessor, 0, &queue_proc);
                    REQUIRE(result == kRocProfVisResultSuccess);

                    spdlog::trace("    Queue {0}/{1}: id={2} name={3}", qi + 1,
                                  num_queues, queue_id, queue_name);
                }

                for(uint64_t ci = 0; ci < num_counters; ci++)
                {
                    rocprofvis_handle_t* counter_handle = nullptr;
                    result = rocprofvis_controller_get_object(
                        processor_handle, kRPVControllerProcessorCounterIndexed, ci,
                        &counter_handle);
                    REQUIRE(result == kRocProfVisResultSuccess);
                    REQUIRE(counter_handle != nullptr);

                    uint64_t counter_id = 0;
                    result              = rocprofvis_controller_get_uint64(
                        counter_handle, kRPVControllerCounterId, 0, &counter_id);
                    REQUIRE(result == kRocProfVisResultSuccess);

                    len    = 0;
                    result = rocprofvis_controller_get_string(
                        counter_handle, kRPVControllerCounterName, 0, nullptr, &len);
                    REQUIRE(result == kRocProfVisResultSuccess);
                    std::string counter_name;
                    counter_name.resize(len);
                    result = rocprofvis_controller_get_string(
                        counter_handle, kRPVControllerCounterName, 0,
                        const_cast<char*>(counter_name.c_str()), &len);
                    REQUIRE(result == kRocProfVisResultSuccess);

                    len    = 0;
                    result = rocprofvis_controller_get_string(
                        counter_handle, kRPVControllerCounterDescription, 0, nullptr,
                        &len);
                    REQUIRE(result == kRocProfVisResultSuccess);
                    std::string counter_desc;
                    counter_desc.resize(len);
                    result = rocprofvis_controller_get_string(
                        counter_handle, kRPVControllerCounterDescription, 0,
                        const_cast<char*>(counter_desc.c_str()), &len);
                    REQUIRE(result == kRocProfVisResultSuccess);

                    len    = 0;
                    result = rocprofvis_controller_get_string(
                        counter_handle, kRPVControllerCounterUnits, 0, nullptr, &len);
                    REQUIRE(result == kRocProfVisResultSuccess);
                    std::string counter_units;
                    counter_units.resize(len);
                    result = rocprofvis_controller_get_string(
                        counter_handle, kRPVControllerCounterUnits, 0,
                        const_cast<char*>(counter_units.c_str()), &len);
                    REQUIRE(result == kRocProfVisResultSuccess);

                    len    = 0;
                    result = rocprofvis_controller_get_string(
                        counter_handle, kRPVControllerCounterValueType, 0, nullptr, &len);
                    REQUIRE(result == kRocProfVisResultSuccess);
                    std::string counter_vtype;
                    counter_vtype.resize(len);
                    result = rocprofvis_controller_get_string(
                        counter_handle, kRPVControllerCounterValueType, 0,
                        const_cast<char*>(counter_vtype.c_str()), &len);
                    REQUIRE(result == kRocProfVisResultSuccess);

                    rocprofvis_handle_t* counter_track = nullptr;
                    result                             = rocprofvis_controller_get_object(
                        counter_handle, kRPVControllerCounterTrack, 0, &counter_track);
                    REQUIRE(result == kRocProfVisResultSuccess);

                    rocprofvis_handle_t* counter_proc = nullptr;
                    result                            = rocprofvis_controller_get_object(
                        counter_handle, kRPVControllerCounterProcessor, 0, &counter_proc);
                    REQUIRE(result == kRocProfVisResultSuccess);

                    spdlog::trace("    Counter {0}/{1}: id={2} name={3} units={4}",
                                  ci + 1, num_counters, counter_id, counter_name,
                                  counter_units);
                }
            }

            uint64_t num_processes = 0;
            result                 = rocprofvis_controller_get_uint64(
                node_handle, kRPVControllerNodeNumProcesses, 0, &num_processes);
            REQUIRE(result == kRocProfVisResultSuccess);
            spdlog::info("  Num processes: {0}", num_processes);

            for(uint64_t pi = 0; pi < num_processes; pi++)
            {
                rocprofvis_handle_t* process_handle = nullptr;
                result                              = rocprofvis_controller_get_object(
                    node_handle, kRPVControllerNodeProcessIndexed, pi, &process_handle);
                REQUIRE(result == kRocProfVisResultSuccess);
                REQUIRE(process_handle != nullptr);

                uint64_t process_id = 0;
                result              = rocprofvis_controller_get_uint64(
                    process_handle, kRPVControllerProcessId, 0, &process_id);
                REQUIRE(result == kRocProfVisResultSuccess);
                spdlog::info("  Processing process {0}/{1} (id={2})", pi + 1,
                             num_processes, process_id);

                double start_time = 0;
                result            = rocprofvis_controller_get_double(
                    process_handle, kRPVControllerProcessStartTime, 0, &start_time);
                REQUIRE(result == kRocProfVisResultSuccess);

                double end_time = 0;
                result          = rocprofvis_controller_get_double(
                    process_handle, kRPVControllerProcessEndTime, 0, &end_time);
                REQUIRE(result == kRocProfVisResultSuccess);

                len    = 0;
                result = rocprofvis_controller_get_string(
                    process_handle, kRPVControllerProcessCommand, 0, nullptr, &len);
                REQUIRE(result == kRocProfVisResultSuccess);
                std::string command;
                command.resize(len);
                result = rocprofvis_controller_get_string(
                    process_handle, kRPVControllerProcessCommand, 0,
                    const_cast<char*>(command.c_str()), &len);
                REQUIRE(result == kRocProfVisResultSuccess);

                len    = 0;
                result = rocprofvis_controller_get_string(
                    process_handle, kRPVControllerProcessEnvironment, 0, nullptr, &len);
                REQUIRE(result == kRocProfVisResultSuccess);
                std::string environment;
                environment.resize(len);
                result = rocprofvis_controller_get_string(
                    process_handle, kRPVControllerProcessEnvironment, 0,
                    const_cast<char*>(environment.c_str()), &len);
                REQUIRE(result == kRocProfVisResultSuccess);

                spdlog::info("    Command: {0} Time: {1}-{2}", command, start_time,
                             end_time);

                uint64_t num_threads = 0;
                result               = rocprofvis_controller_get_uint64(
                    process_handle, kRPVControllerProcessNumThreads, 0, &num_threads);
                REQUIRE(result == kRocProfVisResultSuccess);

                uint64_t num_streams = 0;
                result               = rocprofvis_controller_get_uint64(
                    process_handle, kRPVControllerProcessNumStreams, 0, &num_streams);
                REQUIRE(result == kRocProfVisResultSuccess);
                spdlog::info("    Threads: {0} Streams: {1}", num_threads, num_streams);

                for(uint64_t ti = 0; ti < num_threads; ti++)
                {
                    rocprofvis_handle_t* thread_handle = nullptr;
                    result                             = rocprofvis_controller_get_object(
                        process_handle, kRPVControllerProcessThreadIndexed, ti,
                        &thread_handle);
                    REQUIRE(result == kRocProfVisResultSuccess);
                    REQUIRE(thread_handle != nullptr);

                    uint64_t thread_type = kRPVControllerThreadTypeUndefined;
                    result               = rocprofvis_controller_get_uint64(
                        thread_handle, kRPVControllerThreadType, 0, &thread_type);
                    REQUIRE(result == kRocProfVisResultSuccess);

                    if(thread_type != kRPVControllerThreadTypeUndefined)
                    {
                        uint64_t thread_id = 0;
                        result             = rocprofvis_controller_get_uint64(
                            thread_handle, kRPVControllerThreadId, 0, &thread_id);
                        REQUIRE(result == kRocProfVisResultSuccess);

                        uint64_t thread_tid = 0;
                        result              = rocprofvis_controller_get_uint64(
                            thread_handle, kRPVControllerThreadTid, 0, &thread_tid);
                        REQUIRE(result == kRocProfVisResultSuccess);

                        len    = 0;
                        result = rocprofvis_controller_get_string(
                            thread_handle, kRPVControllerThreadName, 0, nullptr, &len);
                        REQUIRE(result == kRocProfVisResultSuccess);
                        std::string thread_name;
                        thread_name.resize(len);
                        result = rocprofvis_controller_get_string(
                            thread_handle, kRPVControllerThreadName, 0,
                            const_cast<char*>(thread_name.c_str()), &len);
                        REQUIRE(result == kRocProfVisResultSuccess);

                        double t_start = 0;
                        result         = rocprofvis_controller_get_double(
                            thread_handle, kRPVControllerThreadStartTime, 0, &t_start);
                        REQUIRE(result == kRocProfVisResultSuccess);

                        double t_end = 0;
                        result       = rocprofvis_controller_get_double(
                            thread_handle, kRPVControllerThreadEndTime, 0, &t_end);
                        REQUIRE(result == kRocProfVisResultSuccess);

                        spdlog::trace("    Thread {0}/{1}: id={2} tid={3} name={4}",
                                      ti + 1, num_threads, thread_id, thread_tid,
                                      thread_name);
                    }
                }

                for(uint64_t si = 0; si < num_streams; si++)
                {
                    rocprofvis_handle_t* stream_handle = nullptr;
                    result                             = rocprofvis_controller_get_object(
                        process_handle, kRPVControllerProcessStreamIndexed, si,
                        &stream_handle);
                    REQUIRE(result == kRocProfVisResultSuccess);
                    REQUIRE(stream_handle != nullptr);

                    uint64_t stream_id = 0;
                    result             = rocprofvis_controller_get_uint64(
                        stream_handle, kRPVControllerStreamId, 0, &stream_id);
                    REQUIRE(result == kRocProfVisResultSuccess);

                    len    = 0;
                    result = rocprofvis_controller_get_string(
                        stream_handle, kRPVControllerStreamName, 0, nullptr, &len);
                    REQUIRE(result == kRocProfVisResultSuccess);
                    std::string stream_name;
                    stream_name.resize(len);
                    result = rocprofvis_controller_get_string(
                        stream_handle, kRPVControllerStreamName, 0,
                        const_cast<char*>(stream_name.c_str()), &len);
                    REQUIRE(result == kRocProfVisResultSuccess);

                    rocprofvis_handle_t* stream_track = nullptr;
                    result                            = rocprofvis_controller_get_object(
                        stream_handle, kRPVControllerStreamTrack, 0, &stream_track);
                    REQUIRE(result == kRocProfVisResultSuccess);

                    uint64_t stream_num_procs = 0;
                    result                    = rocprofvis_controller_get_uint64(
                        stream_handle, kRPVControllerStreamNumProcessors, 0,
                        &stream_num_procs);
                    REQUIRE(result == kRocProfVisResultSuccess);

                    for(uint64_t spi = 0; spi < stream_num_procs; spi++)
                    {
                        rocprofvis_handle_t* s_proc = nullptr;
                        result                      = rocprofvis_controller_get_object(
                            stream_handle, kRPVControllerStreamProcessorIndexed, spi,
                            &s_proc);
                        REQUIRE(result == kRocProfVisResultSuccess);
                        REQUIRE(s_proc != nullptr);
                    }

                    spdlog::trace("    Stream {0}/{1}: id={2} name={3} procs={4}", si + 1,
                                  num_streams, stream_id, stream_name, stream_num_procs);
                }
            }
        }
    }

    // Reads per-track metadata via timeline graphs: category, names, timestamps,
    // value range, entry count, and topology links (queue/stream/thread/counter).
    // Fixture Reads: m_controller
    SECTION("Read Track Metadata")
    {
        rocprofvis_handle_t* timeline_handle = nullptr;
        rocprofvis_result_t  result          = rocprofvis_controller_get_object(
            m_controller, kRPVControllerSystemTimeline, 0, &timeline_handle);
        REQUIRE(result == kRocProfVisResultSuccess);
        REQUIRE(timeline_handle != nullptr);

        uint64_t num_graphs = 0;
        result              = rocprofvis_controller_get_uint64(
            timeline_handle, kRPVControllerTimelineNumGraphs, 0, &num_graphs);
        REQUIRE(result == kRocProfVisResultSuccess);
        REQUIRE(num_graphs > 0);

        for(uint64_t gi = 0; gi < num_graphs; gi++)
        {
            rocprofvis_handle_t* graph_handle = nullptr;
            result                            = rocprofvis_controller_get_object(
                timeline_handle, kRPVControllerTimelineGraphIndexed, gi, &graph_handle);
            REQUIRE(result == kRocProfVisResultSuccess);
            REQUIRE(graph_handle != nullptr);

            rocprofvis_handle_t* track_handle = nullptr;
            result                            = rocprofvis_controller_get_object(
                graph_handle, kRPVControllerGraphTrack, 0, &track_handle);
            REQUIRE(result == kRocProfVisResultSuccess);
            REQUIRE(track_handle != nullptr);

            uint64_t track_id = 0;
            result = rocprofvis_controller_get_uint64(track_handle, kRPVControllerTrackId,
                                                      0, &track_id);
            REQUIRE(result == kRocProfVisResultSuccess);

            uint64_t track_type = 0;
            result              = rocprofvis_controller_get_uint64(
                track_handle, kRPVControllerTrackType, 0, &track_type);
            REQUIRE(result == kRocProfVisResultSuccess);
            spdlog::info("Processing track metadata {0}/{1} (id={2})", gi + 1, num_graphs,
                         track_id);

            uint32_t len = 0;
            result       = rocprofvis_controller_get_string(
                track_handle, kRPVControllerCategory, 0, nullptr, &len);
            REQUIRE(result == kRocProfVisResultSuccess);
            std::string category;
            category.resize(len);
            result = rocprofvis_controller_get_string(
                track_handle, kRPVControllerCategory, 0,
                const_cast<char*>(category.c_str()), &len);
            REQUIRE(result == kRocProfVisResultSuccess);

            len    = 0;
            result = rocprofvis_controller_get_string(
                track_handle, kRPVControllerMainName, 0, nullptr, &len);
            REQUIRE(result == kRocProfVisResultSuccess);
            std::string main_name;
            main_name.resize(len);
            result = rocprofvis_controller_get_string(
                track_handle, kRPVControllerMainName, 0,
                const_cast<char*>(main_name.c_str()), &len);
            REQUIRE(result == kRocProfVisResultSuccess);

            len    = 0;
            result = rocprofvis_controller_get_string(track_handle, kRPVControllerSubName,
                                                      0, nullptr, &len);
            REQUIRE(result == kRocProfVisResultSuccess);
            std::string sub_name;
            sub_name.resize(len);
            result = rocprofvis_controller_get_string(
                track_handle, kRPVControllerSubName, 0,
                const_cast<char*>(sub_name.c_str()), &len);
            REQUIRE(result == kRocProfVisResultSuccess);

            spdlog::info("  Category: {0} Main: {1} Sub: {2}", category, main_name,
                         sub_name);

            double min_ts = 0;
            result        = rocprofvis_controller_get_double(
                track_handle, kRPVControllerTrackMinTimestamp, 0, &min_ts);
            REQUIRE(result == kRocProfVisResultSuccess);

            double max_ts = 0;
            result        = rocprofvis_controller_get_double(
                track_handle, kRPVControllerTrackMaxTimestamp, 0, &max_ts);
            REQUIRE(result == kRocProfVisResultSuccess);

            double min_val = 0;
            result         = rocprofvis_controller_get_double(
                track_handle, kRPVControllerTrackMinValue, 0, &min_val);
            REQUIRE(result == kRocProfVisResultSuccess);

            double max_val = 0;
            result         = rocprofvis_controller_get_double(
                track_handle, kRPVControllerTrackMaxValue, 0, &max_val);
            REQUIRE(result == kRocProfVisResultSuccess);

            uint64_t num_entries = 0;
            result               = rocprofvis_controller_get_uint64(
                track_handle, kRPVControllerTrackNumberOfEntries, 0, &num_entries);
            REQUIRE(result == kRocProfVisResultSuccess);

            uint64_t agent_or_pid = 0;
            result                = rocprofvis_controller_get_uint64(
                track_handle, kRPVControllerTrackAgentIdOrPid, 0, &agent_or_pid);
            REQUIRE(result == kRocProfVisResultSuccess);

            uint64_t queue_or_tid = 0;
            result                = rocprofvis_controller_get_uint64(
                track_handle, kRPVControllerTrackQueueIdOrTid, 0, &queue_or_tid);
            REQUIRE(result == kRocProfVisResultSuccess);

            spdlog::info("  Entries: {0} AgentOrPid: {1} QueueOrTid: {2}", num_entries,
                         agent_or_pid, queue_or_tid);

            rocprofvis_handle_t* topo_obj  = nullptr;
            rocprofvis_handle_t* process   = nullptr;
            rocprofvis_handle_t* node      = nullptr;
            rocprofvis_handle_t* processor = nullptr;

            result = rocprofvis_controller_get_object(
                track_handle, kRPVControllerTrackQueue, 0, &topo_obj);
            if(result == kRocProfVisResultSuccess && topo_obj != nullptr)
            {
                uint64_t q_id = 0;
                result = rocprofvis_controller_get_uint64(topo_obj, kRPVControllerQueueId,
                                                          0, &q_id);
                REQUIRE(result == kRocProfVisResultSuccess);
                result = rocprofvis_controller_get_object(
                    topo_obj, kRPVControllerQueueProcess, 0, &process);
                REQUIRE(result == kRocProfVisResultSuccess);
                result = rocprofvis_controller_get_object(
                    topo_obj, kRPVControllerQueueNode, 0, &node);
                REQUIRE(result == kRocProfVisResultSuccess);
                result = rocprofvis_controller_get_object(
                    topo_obj, kRPVControllerQueueProcessor, 0, &processor);
                REQUIRE(result == kRocProfVisResultSuccess);
                spdlog::trace("  Track linked to queue id={0}", q_id);
            }

            topo_obj = nullptr;
            result   = rocprofvis_controller_get_object(
                track_handle, kRPVControllerTrackStream, 0, &topo_obj);
            if(result == kRocProfVisResultSuccess && topo_obj != nullptr)
            {
                uint64_t s_id = 0;
                result        = rocprofvis_controller_get_uint64(
                    topo_obj, kRPVControllerStreamId, 0, &s_id);
                REQUIRE(result == kRocProfVisResultSuccess);
                result = rocprofvis_controller_get_object(
                    topo_obj, kRPVControllerStreamProcess, 0, &process);
                REQUIRE(result == kRocProfVisResultSuccess);
                result = rocprofvis_controller_get_object(
                    topo_obj, kRPVControllerStreamNode, 0, &node);
                REQUIRE(result == kRocProfVisResultSuccess);
                result = rocprofvis_controller_get_object(
                    topo_obj, kRPVControllerStreamProcessor, 0, &processor);
                REQUIRE(result == kRocProfVisResultSuccess);
                spdlog::trace("  Track linked to stream id={0}", s_id);
            }

            topo_obj = nullptr;
            result   = rocprofvis_controller_get_object(
                track_handle, kRPVControllerTrackThread, 0, &topo_obj);
            if(result == kRocProfVisResultSuccess && topo_obj != nullptr)
            {
                uint64_t t_id = 0;
                result        = rocprofvis_controller_get_uint64(
                    topo_obj, kRPVControllerThreadId, 0, &t_id);
                REQUIRE(result == kRocProfVisResultSuccess);
                result = rocprofvis_controller_get_object(
                    topo_obj, kRPVControllerThreadProcess, 0, &process);
                REQUIRE(result == kRocProfVisResultSuccess);
                result = rocprofvis_controller_get_object(
                    topo_obj, kRPVControllerThreadNode, 0, &node);
                REQUIRE(result == kRocProfVisResultSuccess);
                uint64_t t_type = 0;
                result          = rocprofvis_controller_get_uint64(
                    topo_obj, kRPVControllerThreadType, 0, &t_type);
                REQUIRE(result == kRocProfVisResultSuccess);
                spdlog::trace("  Track linked to thread id={0} type={1}", t_id, t_type);
            }

            topo_obj = nullptr;
            result   = rocprofvis_controller_get_object(
                track_handle, kRPVControllerTrackCounter, 0, &topo_obj);
            if(result == kRocProfVisResultSuccess && topo_obj != nullptr)
            {
                uint64_t c_id = 0;
                result        = rocprofvis_controller_get_uint64(
                    topo_obj, kRPVControllerCounterId, 0, &c_id);
                REQUIRE(result == kRocProfVisResultSuccess);
                result = rocprofvis_controller_get_object(
                    topo_obj, kRPVControllerCounterProcess, 0, &process);
                REQUIRE(result == kRocProfVisResultSuccess);
                result = rocprofvis_controller_get_object(
                    topo_obj, kRPVControllerCounterNode, 0, &node);
                REQUIRE(result == kRocProfVisResultSuccess);
                result = rocprofvis_controller_get_object(
                    topo_obj, kRPVControllerCounterProcessor, 0, &processor);
                REQUIRE(result == kRocProfVisResultSuccess);
                spdlog::trace("  Track linked to counter id={0}", c_id);
            }

            if(process != nullptr)
            {
                uint64_t p_id = 0;
                result        = rocprofvis_controller_get_uint64(
                    process, kRPVControllerProcessId, 0, &p_id);
                REQUIRE(result == kRocProfVisResultSuccess);
            }
            if(node != nullptr)
            {
                uint64_t n_id = 0;
                result = rocprofvis_controller_get_uint64(node, kRPVControllerNodeId, 0,
                                                          &n_id);
                REQUIRE(result == kRocProfVisResultSuccess);
            }
            if(processor != nullptr)
            {
                uint64_t d_id = 0;
                result        = rocprofvis_controller_get_uint64(
                    processor, kRPVControllerProcessorId, 0, &d_id);
                REQUIRE(result == kRocProfVisResultSuccess);
            }
        }
    }

    // Reads histogram bucket data for each track: sample bucket values for
    // sample-type tracks, event bucket densities for event-type tracks.
    // Fixture Reads: m_controller
    SECTION("Read Histogram Data")
    {
        uint64_t            num_bins = 0;
        rocprofvis_result_t result   = rocprofvis_controller_get_uint64(
            m_controller, kRPVControllerSystemGetHistogramBucketsNumber, 0, &num_bins);
        spdlog::info("Get histogram bucket count: {0}", num_bins);
        REQUIRE(result == kRocProfVisResultSuccess);

        uint64_t num_tracks = 0;
        result              = rocprofvis_controller_get_uint64(
            m_controller, kRPVControllerSystemNumTracks, 0, &num_tracks);
        REQUIRE(result == kRocProfVisResultSuccess);
        REQUIRE(num_tracks > 0);

        for(uint32_t ti = 0; ti < num_tracks; ti++)
        {
            rocprofvis_handle_t* track_handle = nullptr;
            result                            = rocprofvis_controller_get_object(
                m_controller, kRPVControllerSystemTrackIndexed, ti, &track_handle);
            REQUIRE(result == kRocProfVisResultSuccess);
            REQUIRE(track_handle != nullptr);

            uint64_t track_id = 0;
            result = rocprofvis_controller_get_uint64(track_handle, kRPVControllerTrackId,
                                                      0, &track_id);
            REQUIRE(result == kRocProfVisResultSuccess);

            uint64_t track_type = 0;
            result              = rocprofvis_controller_get_uint64(
                track_handle, kRPVControllerTrackType, 0, &track_type);
            REQUIRE(result == kRocProfVisResultSuccess);
            spdlog::info("Processing histogram for track {0}/{1} (id={2})", ti + 1,
                         num_tracks, track_id);

            for(uint64_t bin = 0; bin < num_bins; bin++)
            {
                if(track_type == kRPVControllerTrackTypeSamples)
                {
                    double value = 0;
                    result       = rocprofvis_controller_get_double(
                        track_handle, kRPVControllerTrackHistogramBucketValueIndexed, bin,
                        &value);
                    REQUIRE(result == kRocProfVisResultSuccess);
                }
                else
                {
                    double density = 0;
                    result         = rocprofvis_controller_get_double(
                        track_handle, kRPVControllerTrackHistogramBucketDensityIndexed,
                        bin, &density);
                    REQUIRE(result == kRocProfVisResultSuccess);
                }
            }
        }
    }

    // Finds the first event from loaded track data and fetches its extended data
    // properties (type, category enum, category name, name) asynchronously.
    // Fixture Reads: m_controller, m_track_data
    SECTION("Event Extended Data")
    {
        uint64_t            num_tracks = 0;
        rocprofvis_result_t result     = rocprofvis_controller_get_uint64(
            m_controller, kRPVControllerSystemNumTracks, 0, &num_tracks);
        REQUIRE(result == kRocProfVisResultSuccess);

        uint64_t event_id = 0;
        bool     found    = false;
        for(uint32_t ti = 0; ti < num_tracks && !found; ti++)
        {
            rocprofvis_handle_t* track_handle = nullptr;
            result                            = rocprofvis_controller_get_object(
                m_controller, kRPVControllerSystemTrackIndexed, ti, &track_handle);
            REQUIRE(result == kRocProfVisResultSuccess);

            uint64_t track_type = 0;
            result              = rocprofvis_controller_get_uint64(
                track_handle, kRPVControllerTrackType, 0, &track_type);
            REQUIRE(result == kRocProfVisResultSuccess);

            if(track_type == kRPVControllerTrackTypeEvents && m_track_data[ti] != nullptr)
            {
                uint64_t num_entries = 0;
                result               = rocprofvis_controller_get_uint64(
                    m_track_data[ti], kRPVControllerArrayNumEntries, 0, &num_entries);
                REQUIRE(result == kRocProfVisResultSuccess);

                if(num_entries > 0)
                {
                    rocprofvis_handle_t* entry = nullptr;
                    result                     = rocprofvis_controller_get_object(
                        m_track_data[ti], kRPVControllerArrayEntryIndexed, 0, &entry);
                    REQUIRE(result == kRocProfVisResultSuccess);

                    result = rocprofvis_controller_get_uint64(
                        entry, kRPVControllerEventId, 0, &event_id);
                    REQUIRE(result == kRocProfVisResultSuccess);
                    found = true;
                }
            }
        }

        if(found)
        {
            spdlog::info("Fetching extended data for event id={0}", event_id);

            rocprofvis_controller_future_t* future = rocprofvis_controller_future_alloc();
            REQUIRE(future != nullptr);
            rocprofvis_controller_array_t* array = rocprofvis_controller_array_alloc(0);
            REQUIRE(array != nullptr);

            result = rocprofvis_controller_get_indexed_property_async(
                m_controller, m_controller, kRPVControllerSystemEventDataExtDataIndexed,
                event_id, 1, future, array);
            REQUIRE(result == kRocProfVisResultSuccess);

            result = rocprofvis_controller_future_wait(future, FLT_MAX);
            REQUIRE(result == kRocProfVisResultSuccess);

            uint64_t future_result = 0;
            result = rocprofvis_controller_get_uint64(future, kRPVControllerFutureResult,
                                                      0, &future_result);
            REQUIRE(result == kRocProfVisResultSuccess);
            REQUIRE(future_result == kRocProfVisResultSuccess);

            uint64_t num_ext = 0;
            result           = rocprofvis_controller_get_uint64(
                array, kRPVControllerArrayNumEntries, 0, &num_ext);
            REQUIRE(result == kRocProfVisResultSuccess);
            spdlog::info("Got {0} extended data entries", num_ext);

            for(uint64_t i = 0; i < num_ext; i++)
            {
                rocprofvis_handle_t* ext_handle = nullptr;
                result                          = rocprofvis_controller_get_object(
                    array, kRPVControllerArrayEntryIndexed, i, &ext_handle);
                REQUIRE(result == kRocProfVisResultSuccess);
                REQUIRE(ext_handle != nullptr);

                uint64_t ext_type = 0;
                result            = rocprofvis_controller_get_uint64(
                    ext_handle, kRPVControllerExtDataType, 0, &ext_type);
                REQUIRE(result == kRocProfVisResultSuccess);

                uint64_t ext_cat_enum = 0;
                result                = rocprofvis_controller_get_uint64(
                    ext_handle, kRPVControllerExtDataCategoryEnum, 0, &ext_cat_enum);
                REQUIRE(result == kRocProfVisResultSuccess);

                uint32_t len = 0;
                result       = rocprofvis_controller_get_string(
                    ext_handle, kRPVControllerExtDataCategory, 0, nullptr, &len);
                REQUIRE(result == kRocProfVisResultSuccess);
                std::string ext_category;
                ext_category.resize(len);
                result = rocprofvis_controller_get_string(
                    ext_handle, kRPVControllerExtDataCategory, 0,
                    const_cast<char*>(ext_category.c_str()), &len);
                REQUIRE(result == kRocProfVisResultSuccess);

                len    = 0;
                result = rocprofvis_controller_get_string(
                    ext_handle, kRPVControllerExtDataName, 0, nullptr, &len);
                REQUIRE(result == kRocProfVisResultSuccess);
                std::string ext_name;
                ext_name.resize(len);
                result = rocprofvis_controller_get_string(
                    ext_handle, kRPVControllerExtDataName, 0,
                    const_cast<char*>(ext_name.c_str()), &len);
                REQUIRE(result == kRocProfVisResultSuccess);

                spdlog::trace("  ExtData {0}/{1}: type={2} cat={3} name={4}", i + 1,
                              num_ext, ext_type, ext_category, ext_name);
            }

            rocprofvis_controller_array_free(array);
            rocprofvis_controller_future_free(future);
        }
    }

    // Finds the first event from loaded track data and fetches its flow control
    // links (id, timestamps, track, direction, level, name) asynchronously.
    // Fixture Reads: m_controller, m_track_data
    SECTION("Event Flow Control")
    {
        uint64_t            num_tracks = 0;
        rocprofvis_result_t result     = rocprofvis_controller_get_uint64(
            m_controller, kRPVControllerSystemNumTracks, 0, &num_tracks);
        REQUIRE(result == kRocProfVisResultSuccess);

        uint64_t event_id = 0;
        bool     found    = false;
        for(uint32_t ti = 0; ti < num_tracks && !found; ti++)
        {
            rocprofvis_handle_t* track_handle = nullptr;
            result                            = rocprofvis_controller_get_object(
                m_controller, kRPVControllerSystemTrackIndexed, ti, &track_handle);
            REQUIRE(result == kRocProfVisResultSuccess);

            uint64_t track_type = 0;
            result              = rocprofvis_controller_get_uint64(
                track_handle, kRPVControllerTrackType, 0, &track_type);
            REQUIRE(result == kRocProfVisResultSuccess);

            if(track_type == kRPVControllerTrackTypeEvents && m_track_data[ti] != nullptr)
            {
                uint64_t num_entries = 0;
                result               = rocprofvis_controller_get_uint64(
                    m_track_data[ti], kRPVControllerArrayNumEntries, 0, &num_entries);
                REQUIRE(result == kRocProfVisResultSuccess);

                if(num_entries > 0)
                {
                    rocprofvis_handle_t* entry = nullptr;
                    result                     = rocprofvis_controller_get_object(
                        m_track_data[ti], kRPVControllerArrayEntryIndexed, 0, &entry);
                    REQUIRE(result == kRocProfVisResultSuccess);

                    result = rocprofvis_controller_get_uint64(
                        entry, kRPVControllerEventId, 0, &event_id);
                    REQUIRE(result == kRocProfVisResultSuccess);
                    found = true;
                }
            }
        }

        if(found)
        {
            spdlog::info("Fetching flow control for event id={0}", event_id);

            rocprofvis_controller_future_t* future = rocprofvis_controller_future_alloc();
            REQUIRE(future != nullptr);
            rocprofvis_controller_array_t* array = rocprofvis_controller_array_alloc(0);
            REQUIRE(array != nullptr);

            result = rocprofvis_controller_get_indexed_property_async(
                m_controller, m_controller,
                kRPVControllerSystemEventDataFlowControlIndexed, event_id, 1, future,
                array);
            REQUIRE(result == kRocProfVisResultSuccess);

            result = rocprofvis_controller_future_wait(future, FLT_MAX);
            REQUIRE(result == kRocProfVisResultSuccess);

            uint64_t future_result = 0;
            result = rocprofvis_controller_get_uint64(future, kRPVControllerFutureResult,
                                                      0, &future_result);
            REQUIRE(result == kRocProfVisResultSuccess);
            REQUIRE(future_result == kRocProfVisResultSuccess);

            uint64_t num_flow = 0;
            result            = rocprofvis_controller_get_uint64(
                array, kRPVControllerArrayNumEntries, 0, &num_flow);
            REQUIRE(result == kRocProfVisResultSuccess);
            spdlog::info("Got {0} flow control entries", num_flow);

            for(uint64_t i = 0; i < num_flow; i++)
            {
                rocprofvis_handle_t* flow_handle = nullptr;
                result                           = rocprofvis_controller_get_object(
                    array, kRPVControllerArrayEntryIndexed, i, &flow_handle);
                REQUIRE(result == kRocProfVisResultSuccess);
                REQUIRE(flow_handle != nullptr);

                uint64_t flow_id = 0;
                result           = rocprofvis_controller_get_uint64(
                    flow_handle, kRPVControllerFlowControltId, 0, &flow_id);
                REQUIRE(result == kRocProfVisResultSuccess);

                uint64_t flow_ts = 0;
                result           = rocprofvis_controller_get_uint64(
                    flow_handle, kRPVControllerFlowControlTimestamp, 0, &flow_ts);
                REQUIRE(result == kRocProfVisResultSuccess);

                uint64_t flow_end_ts = 0;
                result               = rocprofvis_controller_get_uint64(
                    flow_handle, kRPVControllerFlowControlEndTimestamp, 0, &flow_end_ts);
                REQUIRE(result == kRocProfVisResultSuccess);

                uint64_t flow_track_id = 0;
                result                 = rocprofvis_controller_get_uint64(
                    flow_handle, kRPVControllerFlowControlTrackId, 0, &flow_track_id);
                REQUIRE(result == kRocProfVisResultSuccess);

                uint64_t flow_dir = 0;
                result            = rocprofvis_controller_get_uint64(
                    flow_handle, kRPVControllerFlowControlDirection, 0, &flow_dir);
                REQUIRE(result == kRocProfVisResultSuccess);

                uint64_t flow_level = 0;
                result              = rocprofvis_controller_get_uint64(
                    flow_handle, kRPVControllerFlowControlLevel, 0, &flow_level);
                REQUIRE(result == kRocProfVisResultSuccess);

                uint32_t len = 0;
                result       = rocprofvis_controller_get_string(
                    flow_handle, kRPVControllerFlowControlName, 0, nullptr, &len);
                REQUIRE(result == kRocProfVisResultSuccess);
                std::string flow_name;
                flow_name.resize(len);
                result = rocprofvis_controller_get_string(
                    flow_handle, kRPVControllerFlowControlName, 0,
                    const_cast<char*>(flow_name.c_str()), &len);
                REQUIRE(result == kRocProfVisResultSuccess);

                spdlog::trace("  Flow {0}/{1}: id={2} name={3} dir={4} track={5}", i + 1,
                              num_flow, flow_id, flow_name, flow_dir, flow_track_id);
            }

            rocprofvis_controller_array_free(array);
            rocprofvis_controller_future_free(future);
        }
    }

    // Finds the first event from loaded track data and fetches its call stack
    // frames (file, pc, name, line address) asynchronously.
    // Fixture Reads: m_controller, m_track_data
    SECTION("Event Call Stack")
    {
        uint64_t            num_tracks = 0;
        rocprofvis_result_t result     = rocprofvis_controller_get_uint64(
            m_controller, kRPVControllerSystemNumTracks, 0, &num_tracks);
        REQUIRE(result == kRocProfVisResultSuccess);

        uint64_t event_id = 0;
        bool     found    = false;
        for(uint32_t ti = 0; ti < num_tracks && !found; ti++)
        {
            rocprofvis_handle_t* track_handle = nullptr;
            result                            = rocprofvis_controller_get_object(
                m_controller, kRPVControllerSystemTrackIndexed, ti, &track_handle);
            REQUIRE(result == kRocProfVisResultSuccess);

            uint64_t track_type = 0;
            result              = rocprofvis_controller_get_uint64(
                track_handle, kRPVControllerTrackType, 0, &track_type);
            REQUIRE(result == kRocProfVisResultSuccess);

            if(track_type == kRPVControllerTrackTypeEvents && m_track_data[ti] != nullptr)
            {
                uint64_t num_entries = 0;
                result               = rocprofvis_controller_get_uint64(
                    m_track_data[ti], kRPVControllerArrayNumEntries, 0, &num_entries);
                REQUIRE(result == kRocProfVisResultSuccess);

                if(num_entries > 0)
                {
                    rocprofvis_handle_t* entry = nullptr;
                    result                     = rocprofvis_controller_get_object(
                        m_track_data[ti], kRPVControllerArrayEntryIndexed, 0, &entry);
                    REQUIRE(result == kRocProfVisResultSuccess);

                    result = rocprofvis_controller_get_uint64(
                        entry, kRPVControllerEventId, 0, &event_id);
                    REQUIRE(result == kRocProfVisResultSuccess);
                    found = true;
                }
            }
        }

        if(found)
        {
            spdlog::info("Fetching call stack for event id={0}", event_id);

            rocprofvis_controller_future_t* future = rocprofvis_controller_future_alloc();
            REQUIRE(future != nullptr);
            rocprofvis_controller_array_t* array = rocprofvis_controller_array_alloc(0);
            REQUIRE(array != nullptr);

            result = rocprofvis_controller_get_indexed_property_async(
                m_controller, m_controller, kRPVControllerSystemEventDataCallStackIndexed,
                event_id, 1, future, array);
            REQUIRE(result == kRocProfVisResultSuccess);

            result = rocprofvis_controller_future_wait(future, FLT_MAX);
            REQUIRE(result == kRocProfVisResultSuccess);

            uint64_t future_result = 0;
            result = rocprofvis_controller_get_uint64(future, kRPVControllerFutureResult,
                                                      0, &future_result);
            REQUIRE(result == kRocProfVisResultSuccess);
            REQUIRE(future_result == kRocProfVisResultSuccess);

            uint64_t num_frames = 0;
            result              = rocprofvis_controller_get_uint64(
                array, kRPVControllerArrayNumEntries, 0, &num_frames);
            REQUIRE(result == kRocProfVisResultSuccess);
            spdlog::info("Got {0} call stack frames", num_frames);

            for(uint64_t i = 0; i < num_frames; i++)
            {
                rocprofvis_handle_t* frame = nullptr;
                result                     = rocprofvis_controller_get_object(
                    array, kRPVControllerArrayEntryIndexed, i, &frame);
                REQUIRE(result == kRocProfVisResultSuccess);
                REQUIRE(frame != nullptr);

                uint32_t len = 0;
                result       = rocprofvis_controller_get_string(
                    frame, kRPVControllerCallstackFile, i, nullptr, &len);
                REQUIRE(result == kRocProfVisResultSuccess);
                std::string file;
                file.resize(len);
                result = rocprofvis_controller_get_string(
                    frame, kRPVControllerCallstackFile, i,
                    const_cast<char*>(file.c_str()), &len);
                REQUIRE(result == kRocProfVisResultSuccess);

                len    = 0;
                result = rocprofvis_controller_get_string(
                    frame, kRPVControllerCallstackPc, i, nullptr, &len);
                REQUIRE(result == kRocProfVisResultSuccess);
                std::string pc;
                pc.resize(len);
                result =
                    rocprofvis_controller_get_string(frame, kRPVControllerCallstackPc, i,
                                                     const_cast<char*>(pc.c_str()), &len);
                REQUIRE(result == kRocProfVisResultSuccess);

                len    = 0;
                result = rocprofvis_controller_get_string(
                    frame, kRPVControllerCallstackName, i, nullptr, &len);
                REQUIRE(result == kRocProfVisResultSuccess);
                std::string name;
                name.resize(len);
                result = rocprofvis_controller_get_string(
                    frame, kRPVControllerCallstackName, i,
                    const_cast<char*>(name.c_str()), &len);
                REQUIRE(result == kRocProfVisResultSuccess);

                len    = 0;
                result = rocprofvis_controller_get_string(
                    frame, kRPVControllerCallstackLineAddress, i, nullptr, &len);
                REQUIRE(result == kRocProfVisResultSuccess);
                std::string line_addr;
                line_addr.resize(len);
                result = rocprofvis_controller_get_string(
                    frame, kRPVControllerCallstackLineAddress, i,
                    const_cast<char*>(line_addr.c_str()), &len);
                REQUIRE(result == kRocProfVisResultSuccess);

                spdlog::trace("  Frame {0}/{1}: file={2} name={3}", i + 1, num_frames,
                              file, name);
            }

            rocprofvis_controller_array_free(array);
            rocprofvis_controller_future_free(future);
        }
    }

    // Fetches summary metrics for the full timeline range including GPU utilization,
    // bandwidth, compute throughput, and per-kernel aggregated data.
    // Fixture Reads: m_controller
    SECTION("Summary Metrics")
    {
        rocprofvis_handle_t* summary_handle = nullptr;
        rocprofvis_result_t  result         = rocprofvis_controller_get_object(
            m_controller, kRPVControllerSystemSummary, 0, &summary_handle);
        REQUIRE(result == kRocProfVisResultSuccess);
        REQUIRE(summary_handle != nullptr);

        rocprofvis_handle_t* timeline_handle = nullptr;
        result                               = rocprofvis_controller_get_object(
            m_controller, kRPVControllerSystemTimeline, 0, &timeline_handle);
        REQUIRE(result == kRocProfVisResultSuccess);

        double start_ts = 0;
        result          = rocprofvis_controller_get_double(
            timeline_handle, kRPVControllerTimelineMinTimestamp, 0, &start_ts);
        REQUIRE(result == kRocProfVisResultSuccess);

        double end_ts = 0;
        result        = rocprofvis_controller_get_double(
            timeline_handle, kRPVControllerTimelineMaxTimestamp, 0, &end_ts);
        REQUIRE(result == kRocProfVisResultSuccess);

        rocprofvis_controller_arguments_t* args = rocprofvis_controller_arguments_alloc();
        REQUIRE(args != nullptr);

        result = rocprofvis_controller_set_double(
            args, kRPVControllerSummaryArgsStartTimestamp, 0, start_ts);
        REQUIRE(result == kRocProfVisResultSuccess);

        result = rocprofvis_controller_set_double(
            args, kRPVControllerSummaryArgsEndTimestamp, 0, end_ts);
        REQUIRE(result == kRocProfVisResultSuccess);

        rocprofvis_controller_summary_metrics_t* metrics =
            rocprofvis_controller_summary_metrics_alloc();
        REQUIRE(metrics != nullptr);

        rocprofvis_controller_future_t* future = rocprofvis_controller_future_alloc();
        REQUIRE(future != nullptr);

        spdlog::info("Fetching summary metrics for range {0}-{1}", start_ts, end_ts);
        result = rocprofvis_controller_summary_fetch_async(
            m_controller, (rocprofvis_controller_summary_t*) summary_handle, args, future,
            metrics);
        REQUIRE(result == kRocProfVisResultSuccess);

        result = rocprofvis_controller_future_wait(future, FLT_MAX);
        REQUIRE(result == kRocProfVisResultSuccess);

        uint64_t future_result = 0;
        result = rocprofvis_controller_get_uint64(future, kRPVControllerFutureResult, 0,
                                                  &future_result);
        REQUIRE(result == kRocProfVisResultSuccess);
        REQUIRE(future_result == kRocProfVisResultSuccess);

        struct GpuFlags
        {
            bool has_gpu_util    = false;
            bool has_kernel_data = false;
        };

        auto require_gpu_util = [&](rocprofvis_handle_t* metrics_handle,
                                    const std::string&   indent) {
            double gfx_util = 0;
            result          = rocprofvis_controller_get_double(
                metrics_handle, kRPVControllerSummaryMetricPropertyGpuGfxUtil, 0,
                &gfx_util);
            REQUIRE(result == kRocProfVisResultSuccess);

            double mem_util = 0;
            result          = rocprofvis_controller_get_double(
                metrics_handle, kRPVControllerSummaryMetricPropertyGpuMemUtil, 0,
                &mem_util);
            REQUIRE(result == kRocProfVisResultSuccess);

            spdlog::info("{0}GfxUtil: {1} MemUtil: {2}", indent, gfx_util, mem_util);
        };

        auto require_kernel_data = [&](rocprofvis_handle_t* metrics_handle,
                                       const std::string&   indent) {
            uint64_t num_kernels = 0;
            result               = rocprofvis_controller_get_uint64(
                metrics_handle, kRPVControllerSummaryMetricPropertyNumKernels, 0,
                &num_kernels);
            REQUIRE(result == kRocProfVisResultSuccess);

            double exec_total = 0;
            result            = rocprofvis_controller_get_double(
                metrics_handle, kRPVControllerSummaryMetricPropertyKernelsExecTimeTotal,
                0, &exec_total);
            REQUIRE(result == kRocProfVisResultSuccess);

            spdlog::info("{0}Kernels: {1} ExecTotal: {2}", indent, num_kernels,
                         exec_total);

            for(uint64_t ki = 0; ki < num_kernels; ki++)
            {
                uint32_t len = 0;
                result       = rocprofvis_controller_get_string(
                    metrics_handle, kRPVControllerSummaryMetricPropertyKernelNameIndexed,
                    ki, nullptr, &len);
                REQUIRE(result == kRocProfVisResultSuccess);
                std::string kernel_name;
                kernel_name.resize(len);
                result = rocprofvis_controller_get_string(
                    metrics_handle, kRPVControllerSummaryMetricPropertyKernelNameIndexed,
                    ki, const_cast<char*>(kernel_name.c_str()), &len);
                REQUIRE(result == kRocProfVisResultSuccess);

                uint64_t invocations = 0;
                result               = rocprofvis_controller_get_uint64(
                    metrics_handle,
                    kRPVControllerSummaryMetricPropertyKernelInvocationsIndexed, ki,
                    &invocations);
                REQUIRE(result == kRocProfVisResultSuccess);

                double exec_sum = 0;
                result          = rocprofvis_controller_get_double(
                    metrics_handle,
                    kRPVControllerSummaryMetricPropertyKernelExecTimeSumIndexed, ki,
                    &exec_sum);
                REQUIRE(result == kRocProfVisResultSuccess);

                double exec_min = 0;
                result          = rocprofvis_controller_get_double(
                    metrics_handle,
                    kRPVControllerSummaryMetricPropertyKernelExecTimeMinIndexed, ki,
                    &exec_min);
                REQUIRE(result == kRocProfVisResultSuccess);

                double exec_max = 0;
                result          = rocprofvis_controller_get_double(
                    metrics_handle,
                    kRPVControllerSummaryMetricPropertyKernelExecTimeMaxIndexed, ki,
                    &exec_max);
                REQUIRE(result == kRocProfVisResultSuccess);

                double exec_pct = 0;
                result          = rocprofvis_controller_get_double(
                    metrics_handle,
                    kRPVControllerSummaryMetricPropertyKernelExecTimePctIndexed, ki,
                    &exec_pct);
                REQUIRE(result == kRocProfVisResultSuccess);

                spdlog::info("{0}  Kernel {1}/{2}: name={3} inv={4} sum={5}", indent,
                             ki + 1, num_kernels, kernel_name, invocations, exec_sum);
            }
        };

        // Returns flags indicating which GPU property groups were found
        // at this level or any descendant. Parent levels REQUIRE the same
        // groups that any child had.
        auto read_summary_metrics = [&](rocprofvis_handle_t* metrics_handle,
                                        uint32_t depth, auto& self) -> GpuFlags {
            std::string indent(depth * 2, ' ');

            uint64_t agg_level = 0;
            result             = rocprofvis_controller_get_uint64(
                metrics_handle, kRPVControllerSummaryMetricPropertyAggregationLevel, 0,
                &agg_level);
            REQUIRE(result == kRocProfVisResultSuccess);
            spdlog::info("{0}Aggregation level: {1}", indent, agg_level);

            bool is_node_or_processor =
                (agg_level == kRPVControllerSummaryAggregationLevelNode ||
                 agg_level == kRPVControllerSummaryAggregationLevelProcessor);
            bool is_processor =
                (agg_level == kRPVControllerSummaryAggregationLevelProcessor);

            if(is_node_or_processor)
            {
                uint64_t metric_id = 0;
                result             = rocprofvis_controller_get_uint64(
                    metrics_handle, kRPVControllerSummaryMetricPropertyId, 0, &metric_id);
                REQUIRE(result == kRocProfVisResultSuccess);

                uint32_t len = 0;
                result       = rocprofvis_controller_get_string(
                    metrics_handle, kRPVControllerSummaryMetricPropertyName, 0, nullptr,
                    &len);
                REQUIRE(result == kRocProfVisResultSuccess);
                std::string metric_name;
                metric_name.resize(len);
                result = rocprofvis_controller_get_string(
                    metrics_handle, kRPVControllerSummaryMetricPropertyName, 0,
                    const_cast<char*>(metric_name.c_str()), &len);
                REQUIRE(result == kRocProfVisResultSuccess);
                spdlog::info("{0}Metric id={1} name={2}", indent, metric_id, metric_name);
            }

            uint64_t proc_type = 0;
            if(is_processor)
            {
                result = rocprofvis_controller_get_uint64(
                    metrics_handle, kRPVControllerSummaryMetricPropertyProcessorType, 0,
                    &proc_type);
                REQUIRE(result == kRocProfVisResultSuccess);

                uint64_t proc_type_idx = 0;
                result                 = rocprofvis_controller_get_uint64(
                    metrics_handle, kRPVControllerSummaryMetricPropertyProcessorTypeIndex,
                    0, &proc_type_idx);
                REQUIRE(result == kRocProfVisResultSuccess);
                spdlog::info("{0}ProcType: {1} TypeIndex: {2}", indent, proc_type,
                             proc_type_idx);
            }

            GpuFlags flags;

            if(is_processor && proc_type == kRPVControllerProcessorTypeGPU)
            {
                double gfx_util = 0;
                result          = rocprofvis_controller_get_double(
                    metrics_handle, kRPVControllerSummaryMetricPropertyGpuGfxUtil, 0,
                    &gfx_util);
                if(result == kRocProfVisResultSuccess)
                {
                    flags.has_gpu_util = true;
                    double mem_util    = 0;
                    result             = rocprofvis_controller_get_double(
                        metrics_handle, kRPVControllerSummaryMetricPropertyGpuMemUtil, 0,
                        &mem_util);
                    REQUIRE(result == kRocProfVisResultSuccess);
                    spdlog::info("{0}GfxUtil: {1} MemUtil: {2}", indent, gfx_util,
                                 mem_util);
                }

                uint64_t num_kernels = 0;
                result               = rocprofvis_controller_get_uint64(
                    metrics_handle, kRPVControllerSummaryMetricPropertyNumKernels, 0,
                    &num_kernels);
                if(result == kRocProfVisResultSuccess && num_kernels > 0)
                {
                    flags.has_kernel_data = true;
                    require_kernel_data(metrics_handle, indent);
                }
            }

            uint64_t num_sub = 0;
            result           = rocprofvis_controller_get_uint64(
                metrics_handle, kRPVControllerSummaryMetricPropertyNumSubMetrics, 0,
                &num_sub);
            REQUIRE(result == kRocProfVisResultSuccess);
            spdlog::info("{0}Sub-metrics: {1}", indent, num_sub);

            for(uint64_t si = 0; si < num_sub; si++)
            {
                rocprofvis_handle_t* sub_metric = nullptr;
                result                          = rocprofvis_controller_get_object(
                    metrics_handle, kRPVControllerSummaryMetricPropertySubMetricsIndexed,
                    si, &sub_metric);
                REQUIRE(result == kRocProfVisResultSuccess);
                REQUIRE(sub_metric != nullptr);
                spdlog::info("{0}Processing sub-metric {1}/{2}", indent, si + 1, num_sub);
                GpuFlags child = self(sub_metric, depth + 1, self);
                if(child.has_gpu_util) flags.has_gpu_util = true;
                if(child.has_kernel_data) flags.has_kernel_data = true;
            }

            if(!is_processor)
            {
                if(flags.has_gpu_util)
                {
                    require_gpu_util(metrics_handle, indent);
                }
                if(flags.has_kernel_data)
                {
                    require_kernel_data(metrics_handle, indent);
                }
            }

            return flags;
        };

        read_summary_metrics(metrics, 0, read_summary_metrics);

        spdlog::info("Free summary resources");
        rocprofvis_controller_summary_metric_free(metrics);
        rocprofvis_controller_arguments_free(args);
        rocprofvis_controller_future_free(future);
    }

    // Swaps two graph entries in the timeline, verifies the reorder by reading back
    // the graph IDs, then restores the original order.
    // Fixture Reads: m_controller
    SECTION("Reorder Graph Index")
    {
        rocprofvis_handle_t* timeline_handle = nullptr;
        rocprofvis_result_t  result          = rocprofvis_controller_get_object(
            m_controller, kRPVControllerSystemTimeline, 0, &timeline_handle);
        REQUIRE(result == kRocProfVisResultSuccess);
        REQUIRE(timeline_handle != nullptr);

        uint64_t num_graphs = 0;
        result              = rocprofvis_controller_get_uint64(
            timeline_handle, kRPVControllerTimelineNumGraphs, 0, &num_graphs);
        REQUIRE(result == kRocProfVisResultSuccess);

        if(num_graphs >= 2)
        {
            rocprofvis_handle_t* graph_0 = nullptr;
            result                       = rocprofvis_controller_get_object(
                timeline_handle, kRPVControllerTimelineGraphIndexed, 0, &graph_0);
            REQUIRE(result == kRocProfVisResultSuccess);

            uint64_t id_0 = 0;
            result = rocprofvis_controller_get_uint64(graph_0, kRPVControllerGraphId, 0,
                                                      &id_0);
            REQUIRE(result == kRocProfVisResultSuccess);

            rocprofvis_handle_t* graph_1 = nullptr;
            result                       = rocprofvis_controller_get_object(
                timeline_handle, kRPVControllerTimelineGraphIndexed, 1, &graph_1);
            REQUIRE(result == kRocProfVisResultSuccess);

            uint64_t id_1 = 0;
            result = rocprofvis_controller_get_uint64(graph_1, kRPVControllerGraphId, 0,
                                                      &id_1);
            REQUIRE(result == kRocProfVisResultSuccess);

            spdlog::info("Swapping graph at index 0 (id={0}) with index 1 (id={1})", id_0,
                         id_1);

            result = rocprofvis_controller_set_object(
                timeline_handle, kRPVControllerTimelineGraphIndexed, 0, graph_1);
            REQUIRE(result == kRocProfVisResultSuccess);

            result = rocprofvis_controller_set_object(
                timeline_handle, kRPVControllerTimelineGraphIndexed, 1, graph_0);
            REQUIRE(result == kRocProfVisResultSuccess);

            rocprofvis_handle_t* verify_0 = nullptr;
            result                        = rocprofvis_controller_get_object(
                timeline_handle, kRPVControllerTimelineGraphIndexed, 0, &verify_0);
            REQUIRE(result == kRocProfVisResultSuccess);
            uint64_t verify_id_0 = 0;
            result = rocprofvis_controller_get_uint64(verify_0, kRPVControllerGraphId, 0,
                                                      &verify_id_0);
            REQUIRE(result == kRocProfVisResultSuccess);
            REQUIRE(verify_id_0 == id_1);

            rocprofvis_handle_t* verify_1 = nullptr;
            result                        = rocprofvis_controller_get_object(
                timeline_handle, kRPVControllerTimelineGraphIndexed, 1, &verify_1);
            REQUIRE(result == kRocProfVisResultSuccess);
            uint64_t verify_id_1 = 0;
            result = rocprofvis_controller_get_uint64(verify_1, kRPVControllerGraphId, 0,
                                                      &verify_id_1);
            REQUIRE(result == kRocProfVisResultSuccess);
            REQUIRE(verify_id_1 == id_0);

            spdlog::info("Graph reorder verified: index 0 now id={0}, index 1 now id={1}",
                         verify_id_0, verify_id_1);

            result = rocprofvis_controller_set_object(
                timeline_handle, kRPVControllerTimelineGraphIndexed, 0, graph_0);
            REQUIRE(result == kRocProfVisResultSuccess);
            result = rocprofvis_controller_set_object(
                timeline_handle, kRPVControllerTimelineGraphIndexed, 1, graph_1);
            REQUIRE(result == kRocProfVisResultSuccess);
        }
    }

    // Searches for events matching the term "hip" using the global search results
    // table, validates that at least one result is returned.
    // Fixture Reads: m_controller
    SECTION("Event Search")
    {
        rocprofvis_handle_t* table_handle = nullptr;
        rocprofvis_result_t  result       = rocprofvis_controller_get_object(
            m_controller, kRPVControllerSystemSearchResultsTable, 0, &table_handle);
        REQUIRE(result == kRocProfVisResultSuccess);
        REQUIRE(table_handle);

        rocprofvis_handle_t* timeline_handle = nullptr;
        result                               = rocprofvis_controller_get_object(
            m_controller, kRPVControllerSystemTimeline, 0, &timeline_handle);
        REQUIRE(result == kRocProfVisResultSuccess);
        REQUIRE(timeline_handle);

        double start_ts = 0;
        result          = rocprofvis_controller_get_double(
            timeline_handle, kRPVControllerTimelineMinTimestamp, 0, &start_ts);
        REQUIRE(result == kRocProfVisResultSuccess);

        double end_ts = 0;
        result        = rocprofvis_controller_get_double(
            timeline_handle, kRPVControllerTimelineMaxTimestamp, 0, &end_ts);
        REQUIRE(result == kRocProfVisResultSuccess);

        rocprofvis_controller_arguments_t* args = rocprofvis_controller_arguments_alloc();
        REQUIRE(args != nullptr);

        result = rocprofvis_controller_set_uint64(args, kRPVControllerTableArgsType, 0,
                                                  kRPVControllerTableTypeSearchResults);
        REQUIRE(result == kRocProfVisResultSuccess);

        result = rocprofvis_controller_set_double(args, kRPVControllerTableArgsStartTime,
                                                  0, start_ts);
        REQUIRE(result == kRocProfVisResultSuccess);

        result = rocprofvis_controller_set_double(args, kRPVControllerTableArgsEndTime, 0,
                                                  end_ts);
        REQUIRE(result == kRocProfVisResultSuccess);

        result = rocprofvis_controller_set_uint64(args, kRPVControllerTableArgsNumTracks,
                                                  0, 0);
        REQUIRE(result == kRocProfVisResultSuccess);

        result = rocprofvis_controller_set_uint64(
            args, kRPVControllerTableArgsOpTypesIndexed, 0, kRocProfVisDmOperationLaunch);
        REQUIRE(result == kRocProfVisResultSuccess);

        result =
            rocprofvis_controller_set_uint64(args, kRPVControllerTableArgsOpTypesIndexed,
                                             1, kRocProfVisDmOperationDispatch);
        REQUIRE(result == kRocProfVisResultSuccess);

        result =
            rocprofvis_controller_set_uint64(args, kRPVControllerTableArgsOpTypesIndexed,
                                             2, kRocProfVisDmOperationLaunchSample);
        REQUIRE(result == kRocProfVisResultSuccess);

        result = rocprofvis_controller_set_uint64(args, kRPVControllerTableArgsNumOpTypes,
                                                  0, 3);
        REQUIRE(result == kRocProfVisResultSuccess);

        result = rocprofvis_controller_set_string(
            args, kRPVControllerTableArgsStringTableFiltersIndexed, 0, "hip");
        REQUIRE(result == kRocProfVisResultSuccess);

        result = rocprofvis_controller_set_uint64(
            args, kRPVControllerTableArgsNumStringTableFilters, 0, 1);
        REQUIRE(result == kRocProfVisResultSuccess);

        result = rocprofvis_controller_set_uint64(args, kRPVControllerTableArgsSortColumn,
                                                  0, 0);
        REQUIRE(result == kRocProfVisResultSuccess);

        result = rocprofvis_controller_set_uint64(args, kRPVControllerTableArgsSortOrder,
                                                  0, kRPVControllerSortOrderAscending);
        REQUIRE(result == kRocProfVisResultSuccess);

        result =
            rocprofvis_controller_set_string(args, kRPVControllerTableArgsWhere, 0, "");
        REQUIRE(result == kRocProfVisResultSuccess);

        result =
            rocprofvis_controller_set_string(args, kRPVControllerTableArgsFilter, 0, "");
        REQUIRE(result == kRocProfVisResultSuccess);

        result =
            rocprofvis_controller_set_string(args, kRPVControllerTableArgsGroup, 0, "");
        REQUIRE(result == kRocProfVisResultSuccess);

        result = rocprofvis_controller_set_string(
            args, kRPVControllerTableArgsGroupColumns, 0, "");
        REQUIRE(result == kRocProfVisResultSuccess);

        result =
            rocprofvis_controller_set_uint64(args, kRPVControllerTableArgsSummary, 0, 0);
        REQUIRE(result == kRocProfVisResultSuccess);

        result = rocprofvis_controller_set_uint64(args, kRPVControllerTableArgsStartIndex,
                                                  0, 0);
        REQUIRE(result == kRocProfVisResultSuccess);

        result = rocprofvis_controller_set_uint64(args, kRPVControllerTableArgsStartCount,
                                                  0, 1);
        REQUIRE(result == kRocProfVisResultSuccess);

        spdlog::info("Allocating Array");
        rocprofvis_controller_array_t* array = rocprofvis_controller_array_alloc(0);
        REQUIRE(array != nullptr);

        spdlog::info("Allocating Future");
        rocprofvis_controller_future_t* future = rocprofvis_controller_future_alloc();
        REQUIRE(future != nullptr);

        spdlog::info("Searching for events matching 'hip'");
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

        uint64_t num_rows = 0;
        result            = rocprofvis_controller_get_uint64(
            table_handle, kRPVControllerTableNumRows, 0, &num_rows);
        REQUIRE(result == kRocProfVisResultSuccess);
        spdlog::info("Search returned {0} result(s)", num_rows);
        REQUIRE(num_rows >= 1);

        spdlog::info("Free Future");
        rocprofvis_controller_future_free(future);

        spdlog::info("Free Array");
        rocprofvis_controller_array_free(array);

        spdlog::info("Free Args");
        rocprofvis_controller_arguments_free(args);
    }

    // Trims the first half of the trace to a temporary file, validates the
    // output file exists and has content, then removes it.
    // Fixture Reads: m_controller
    SECTION("Trim Trace")
    {
        rocprofvis_handle_t* timeline_handle = nullptr;
        rocprofvis_result_t  result          = rocprofvis_controller_get_object(
            m_controller, kRPVControllerSystemTimeline, 0, &timeline_handle);
        REQUIRE(result == kRocProfVisResultSuccess);
        REQUIRE(timeline_handle);

        double start_ts = 0;
        result          = rocprofvis_controller_get_double(
            timeline_handle, kRPVControllerTimelineMinTimestamp, 0, &start_ts);
        REQUIRE(result == kRocProfVisResultSuccess);

        double end_ts = 0;
        result        = rocprofvis_controller_get_double(
            timeline_handle, kRPVControllerTimelineMaxTimestamp, 0, &end_ts);
        REQUIRE(result == kRocProfVisResultSuccess);

        double midpoint = start_ts + (end_ts - start_ts) / 2.0;

        const char* trim_path = "sample/trimmed.db";

        rocprofvis_controller_future_t* future = rocprofvis_controller_future_alloc();
        REQUIRE(future != nullptr);

        result = rocprofvis_controller_save_trimmed_trace(m_controller, start_ts,
                                                          midpoint, trim_path, future);
        REQUIRE(result == kRocProfVisResultSuccess);

        result = rocprofvis_controller_future_wait(future, FLT_MAX);
        REQUIRE(result == kRocProfVisResultSuccess);

        uint64_t future_result = 0;
        result = rocprofvis_controller_get_uint64(future, kRPVControllerFutureResult, 0,
                                                  &future_result);
        REQUIRE(result == kRocProfVisResultSuccess);
        REQUIRE(future_result == kRocProfVisResultSuccess);

        REQUIRE(std::filesystem::exists(trim_path));
        uint64_t file_size = std::filesystem::file_size(trim_path);
        spdlog::info("Trimmed trace file size: {0} bytes", file_size);
        REQUIRE(file_size > 0);

        std::filesystem::remove(trim_path);
        rocprofvis_controller_future_free(future);
    }

    // Cleans the database with rebuild enabled.
    // Fixture Reads: m_controller
    SECTION("Database Cleanup")
    {
        uint64_t initial_size = std::filesystem::file_size(g_input_file);
        spdlog::info("Initial DB file size: {0} bytes", initial_size);
        REQUIRE(initial_size > 0);

        rocprofvis_controller_future_t* future = rocprofvis_controller_future_alloc();
        REQUIRE(future != nullptr);

        rocprofvis_result_t result =
            rocprofvis_controller_cleanup_trace_database(m_controller, true, future);
        REQUIRE(result == kRocProfVisResultSuccess);

        result = rocprofvis_controller_future_wait(future, FLT_MAX);
        REQUIRE(result == kRocProfVisResultSuccess);

        uint64_t future_result = 0;
        result = rocprofvis_controller_get_uint64(future, kRPVControllerFutureResult, 0,
                                                  &future_result);
        REQUIRE(result == kRocProfVisResultSuccess);
        REQUIRE(future_result == kRocProfVisResultSuccess);

        uint64_t final_size = std::filesystem::file_size(g_input_file);
        spdlog::info("Final DB file size: {0} bytes", final_size);

        rocprofvis_controller_future_free(future);
    }

    // Frees the track data arrays and the controller handle.
    // Fixture Reads: m_controller, m_track_data
    SECTION("Delete")
    {
        uint64_t            num_tracks = 0;
        rocprofvis_result_t result     = rocprofvis_controller_get_uint64(
            m_controller, kRPVControllerSystemNumTracks, 0, &num_tracks);
        REQUIRE(result == kRocProfVisResultSuccess);
        REQUIRE(num_tracks > 0);

        for(uint32_t track_idx = 0; track_idx < num_tracks; track_idx++)
        {
            spdlog::info("Freeing track {0}/{1}", track_idx + 1, num_tracks);
            rocprofvis_controller_array_free(m_track_data[track_idx]);
        }

        spdlog::info("Free Controller");
        rocprofvis_controller_free(m_controller);
    }
}