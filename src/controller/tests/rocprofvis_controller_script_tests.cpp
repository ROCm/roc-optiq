// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_controller.h"
#include "rocprofvis_controller_script.h"
#include "rocprofvis_core.h"
#include "rocprofvis_python_runtime.h"

#include <catch2/catch_session.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cfloat>
#include <functional>
#include <string>
#include <thread>

std::string g_input_file = "sample/trace_70b_1024_32.rpd";

namespace
{

std::string
get_handle_string(rocprofvis_handle_t* object, rocprofvis_property_t property)
{
    std::string text;
    uint32_t    length = 0;
    rocprofvis_result_t result =
        rocprofvis_controller_get_string(object, property, 0, nullptr, &length);
    if(result == kRocProfVisResultSuccess && length > 0)
    {
        text.resize(length);
        result = rocprofvis_controller_get_string(object, property, 0, text.data(),
                                                  &length);
        if(result != kRocProfVisResultSuccess)
        {
            text.clear();
        }
    }
    return text;
}

rocprofvis_result_t
wait_for_script(rocprofvis_controller_future_t* future)
{
    return rocprofvis_controller_future_wait(future, FLT_MAX);
}

rocprofvis_controller_t*
load_sample_controller()
{
    rocprofvis_controller_t* controller =
        rocprofvis_controller_alloc(g_input_file.c_str(), nullptr);
    if(!controller)
    {
        return nullptr;
    }
    rocprofvis_controller_future_t* future = rocprofvis_controller_future_alloc();
    if(!future)
    {
        rocprofvis_controller_free(controller);
        return nullptr;
    }
    rocprofvis_result_t error = rocprofvis_controller_load_async(controller, future);
    if(error == kRocProfVisResultSuccess)
    {
        error = rocprofvis_controller_future_wait(future, FLT_MAX);
    }
    uint64_t future_result = kRocProfVisResultUnknownError;
    if(error == kRocProfVisResultSuccess)
    {
        error = rocprofvis_controller_get_uint64(future, kRPVControllerFutureResult, 0,
                                                 &future_result);
    }
    rocprofvis_controller_future_free(future);
    if(error != kRocProfVisResultSuccess || future_result != kRocProfVisResultSuccess)
    {
        rocprofvis_controller_free(controller);
        return nullptr;
    }
    return controller;
}

rocprofvis_handle_t*
first_event_track(rocprofvis_controller_t* controller)
{
    uint64_t            num_tracks = 0;
    rocprofvis_result_t error      = rocprofvis_controller_get_uint64(
        controller, kRPVControllerSystemNumTracks, 0, &num_tracks);
    rocprofvis_handle_t* track = nullptr;
    if(error == kRocProfVisResultSuccess)
    {
        for(uint64_t i = 0; i < num_tracks && !track; i++)
        {
            rocprofvis_handle_t* candidate = nullptr;
            error = rocprofvis_controller_get_object(
                controller, kRPVControllerSystemTrackIndexed, i, &candidate);
            if(error == kRocProfVisResultSuccess && candidate)
            {
                uint64_t type = 0;
                error = rocprofvis_controller_get_uint64(candidate, kRPVControllerTrackType,
                                                         0, &type);
                if(error == kRocProfVisResultSuccess &&
                   type == kRPVControllerTrackTypeEvents)
                {
                    track = candidate;
                }
            }
        }
    }
    return track;
}

}  // namespace

int
main(int argc, char** argv)
{
    Catch::Session session;
    using namespace Catch::Clara;
    auto cli =
        session.cli() | Opt(g_input_file, "input_file")["--input_file"]("Path to input file");
    session.cli(cli);
    rocprofvis_core_enable_log(
        "Testing/Temporary/rocprofvis_controller_script_tests.txt",
        spdlog::level::trace);
    int return_code = session.applyCommandLine(argc, argv);
    if(return_code != 0)
    {
        return return_code;
    }
    return_code = session.run();
    rocprofvis_python_shutdown();
    return return_code;
}

TEST_CASE("Script execute returns text from optiq.result.text")
{
    rocprofvis_controller_future_t*        future = rocprofvis_controller_future_alloc();
    rocprofvis_controller_script_result_t* result = nullptr;
    REQUIRE(future);

    rocprofvis_result_t error = rocprofvis_script_execute_async(
        nullptr, "optiq.result.text('hello')", nullptr, future, &result);
    REQUIRE(error == kRocProfVisResultSuccess);
    REQUIRE(result);

    error = wait_for_script(future);
    REQUIRE(error == kRocProfVisResultSuccess);

    uint64_t future_result = kRocProfVisResultUnknownError;
    error = rocprofvis_controller_get_uint64(future, kRPVControllerFutureResult, 0,
                                             &future_result);
    REQUIRE(error == kRocProfVisResultSuccess);
    REQUIRE(future_result == kRocProfVisResultSuccess);

    REQUIRE(get_handle_string(result, kRPVControllerScriptResultText) == "hello");

    rocprofvis_controller_object_type_t type = kRPVControllerObjectTypeControllerSystem;
    error = rocprofvis_controller_get_object_type(result, &type);
    REQUIRE(error == kRocProfVisResultSuccess);
    REQUIRE(type == kRPVControllerObjectTypeScriptResult);

    rocprofvis_script_result_free(result);
    rocprofvis_controller_future_free(future);
}

TEST_CASE("Script execute runs on the interpreter thread")
{
    unsigned long long caller_id =
        static_cast<unsigned long long>(
            std::hash<std::thread::id>{}(std::this_thread::get_id()));

    rocprofvis_controller_future_t*        future = rocprofvis_controller_future_alloc();
    rocprofvis_controller_script_result_t* result = nullptr;
    REQUIRE(future);

    rocprofvis_result_t error = rocprofvis_script_execute_async(
        nullptr, "optiq.result.text('threaded')", nullptr, future, &result);
    REQUIRE(error == kRocProfVisResultSuccess);
    REQUIRE(wait_for_script(future) == kRocProfVisResultSuccess);

    unsigned long long interpreter_id = rocprofvis_python_interpreter_thread_id();
    REQUIRE(interpreter_id != 0);
    REQUIRE(interpreter_id != caller_id);
    REQUIRE(get_handle_string(result, kRPVControllerScriptResultText) == "threaded");

    rocprofvis_script_result_free(result);
    rocprofvis_controller_future_free(future);
}

TEST_CASE("Script execute rejects disallowed imports")
{
    rocprofvis_controller_future_t*        future = rocprofvis_controller_future_alloc();
    rocprofvis_controller_script_result_t* result = nullptr;
    REQUIRE(future);

    rocprofvis_result_t error = rocprofvis_script_execute_async(
        nullptr, "import os\noptiq.result.text('should not run')", nullptr, future,
        &result);
    REQUIRE(error == kRocProfVisResultSuccess);
    REQUIRE(wait_for_script(future) == kRocProfVisResultSuccess);

    uint64_t future_result = kRocProfVisResultSuccess;
    error = rocprofvis_controller_get_uint64(future, kRPVControllerFutureResult, 0,
                                             &future_result);
    REQUIRE(error == kRocProfVisResultSuccess);
    REQUIRE(future_result == kRocProfVisResultUnknownError);

    std::string message = get_handle_string(result, kRPVControllerScriptResultErrorMessage);
    REQUIRE(message.find("os") != std::string::npos);
    REQUIRE(get_handle_string(result, kRPVControllerScriptResultText).empty());

    rocprofvis_script_result_free(result);
    rocprofvis_controller_future_free(future);
}

TEST_CASE("Script execute allows math from the import allowlist")
{
    rocprofvis_controller_future_t*        future = rocprofvis_controller_future_alloc();
    rocprofvis_controller_script_result_t* result = nullptr;
    REQUIRE(future);

    rocprofvis_result_t error = rocprofvis_script_execute_async(
        nullptr, "optiq.result.text(str(int(math.sqrt(16))))", nullptr, future, &result);
    REQUIRE(error == kRocProfVisResultSuccess);
    REQUIRE(wait_for_script(future) == kRocProfVisResultSuccess);

    uint64_t future_result = kRocProfVisResultUnknownError;
    error = rocprofvis_controller_get_uint64(future, kRPVControllerFutureResult, 0,
                                             &future_result);
    REQUIRE(error == kRocProfVisResultSuccess);
    REQUIRE(future_result == kRocProfVisResultSuccess);
    REQUIRE(get_handle_string(result, kRPVControllerScriptResultText) == "4");

    rocprofvis_script_result_free(result);
    rocprofvis_controller_future_free(future);
}

TEST_CASE("table_alloc is not the UI event table singleton")
{
    rocprofvis_controller_t* controller = load_sample_controller();
    REQUIRE(controller);

    rocprofvis_controller_table_t* table = rocprofvis_controller_table_alloc();
    REQUIRE(table);

    rocprofvis_controller_object_type_t type = kRPVControllerObjectTypeControllerSystem;
    REQUIRE(rocprofvis_controller_get_object_type(table, &type) == kRocProfVisResultSuccess);
    REQUIRE(type == kRPVControllerObjectTypeTable);

    rocprofvis_handle_t* event_table = nullptr;
    REQUIRE(rocprofvis_controller_get_object(controller, kRPVControllerSystemEventTable, 0,
                                             &event_table) == kRocProfVisResultSuccess);
    REQUIRE(event_table);
    REQUIRE(table != event_table);

    rocprofvis_controller_table_free(table);
    rocprofvis_controller_free(controller);
}

TEST_CASE("Script reads track count from a loaded controller")
{
    rocprofvis_controller_t* controller = load_sample_controller();
    REQUIRE(controller);

    rocprofvis_controller_future_t*        future = rocprofvis_controller_future_alloc();
    rocprofvis_controller_script_result_t* result = nullptr;
    REQUIRE(future);

    rocprofvis_result_t error = rocprofvis_script_execute_async(
        controller, "optiq.result.text(str(len(optiq.trace.tracks)))", nullptr, future,
        &result);
    REQUIRE(error == kRocProfVisResultSuccess);
    REQUIRE(wait_for_script(future) == kRocProfVisResultSuccess);

    uint64_t future_result = kRocProfVisResultUnknownError;
    error = rocprofvis_controller_get_uint64(future, kRPVControllerFutureResult, 0,
                                             &future_result);
    REQUIRE(error == kRocProfVisResultSuccess);
    REQUIRE(future_result == kRocProfVisResultSuccess);

    uint64_t reported = std::stoull(get_handle_string(result, kRPVControllerScriptResultText));
    REQUIRE(reported > 0);

    rocprofvis_script_result_free(result);
    rocprofvis_controller_future_free(future);
    rocprofvis_controller_free(controller);
}

TEST_CASE("Script fetches events and measures spacing")
{
    rocprofvis_controller_t* controller = load_sample_controller();
    REQUIRE(controller);

    char const* source =
        "track = None\n"
        "for t in optiq.selection.tracks:\n"
        "    if t.type == optiq.TRACK_TYPE_EVENTS and t.num_entries > 0:\n"
        "        track = t\n"
        "        break\n"
        "if track is None:\n"
        "    optiq.result.text('0')\n"
        "else:\n"
        "    events = track.events(start=optiq.selection.start, end=optiq.selection.end)\n"
        "    if len(events) < 2:\n"
        "        optiq.result.text(str(len(events)))\n"
        "    else:\n"
        "        gaps = [events[i].start - events[i-1].end for i in range(1, len(events))]\n"
        "        mean = sum(gaps) / len(gaps)\n"
        "        max_dev = max(abs(g - mean) for g in gaps)\n"
        "        optiq.result.text(str(len(events)))\n";

    rocprofvis_controller_future_t*        future = rocprofvis_controller_future_alloc();
    rocprofvis_controller_script_result_t* result = nullptr;
    REQUIRE(future);

    rocprofvis_result_t error =
        rocprofvis_script_execute_async(controller, source, nullptr, future, &result);
    REQUIRE(error == kRocProfVisResultSuccess);
    REQUIRE(wait_for_script(future) == kRocProfVisResultSuccess);

    uint64_t future_result = kRocProfVisResultUnknownError;
    error = rocprofvis_controller_get_uint64(future, kRPVControllerFutureResult, 0,
                                             &future_result);
    REQUIRE(error == kRocProfVisResultSuccess);
    REQUIRE(future_result == kRocProfVisResultSuccess);

    uint64_t reported = std::stoull(get_handle_string(result, kRPVControllerScriptResultText));
    REQUIRE(reported > 0);

    rocprofvis_script_result_free(result);
    rocprofvis_controller_future_free(future);
    rocprofvis_controller_free(controller);
}

TEST_CASE("Script table.fetch does not use the UI event table")
{
    rocprofvis_controller_t* controller = load_sample_controller();
    REQUIRE(controller);

    char const* source =
        "event_tracks = [t for t in optiq.trace.tracks if t.type == optiq.TRACK_TYPE_EVENTS]\n"
        "t = optiq.table()\n"
        "rows = []\n"
        "for tr in event_tracks:\n"
        "    rows = t.fetch(tracks=[tr], start=tr.min_time, end=tr.max_time, count=32)\n"
        "    if rows:\n"
        "        break\n"
        "optiq.result.text(str(len(rows)))\n"
        "optiq.result.text(str(len(t.rows())))\n";

    rocprofvis_controller_future_t*        future = rocprofvis_controller_future_alloc();
    rocprofvis_controller_script_result_t* result = nullptr;
    REQUIRE(future);

    rocprofvis_result_t error =
        rocprofvis_script_execute_async(controller, source, nullptr, future, &result);
    REQUIRE(error == kRocProfVisResultSuccess);
    REQUIRE(wait_for_script(future) == kRocProfVisResultSuccess);

    uint64_t future_result = kRocProfVisResultUnknownError;
    error = rocprofvis_controller_get_uint64(future, kRPVControllerFutureResult, 0,
                                             &future_result);
    REQUIRE(error == kRocProfVisResultSuccess);
    REQUIRE(future_result == kRocProfVisResultSuccess);

    std::string text = get_handle_string(result, kRPVControllerScriptResultText);
    REQUIRE(text.find('\n') != std::string::npos);
    size_t split = text.find('\n');
    uint64_t fetched = std::stoull(text.substr(0, split));
    uint64_t cached  = std::stoull(text.substr(split + 1));
    REQUIRE(fetched > 0);
    REQUIRE(fetched == cached);

    rocprofvis_script_result_free(result);
    rocprofvis_controller_future_free(future);
    rocprofvis_controller_free(controller);
}

TEST_CASE("Script selection context restricts tracks")
{
    rocprofvis_controller_t* controller = load_sample_controller();
    REQUIRE(controller);

    rocprofvis_handle_t* track = first_event_track(controller);
    REQUIRE(track);

    rocprofvis_controller_arguments_t* context = rocprofvis_controller_arguments_alloc();
    REQUIRE(context);
    REQUIRE(rocprofvis_controller_set_uint64(context, kRPVControllerScriptContextNumTracks,
                                             0, 1) == kRocProfVisResultSuccess);
    REQUIRE(rocprofvis_controller_set_object(context,
                                             kRPVControllerScriptContextTracksIndexed, 0,
                                             track) == kRocProfVisResultSuccess);

    double min_time = 0.0;
    double max_time = 0.0;
    REQUIRE(rocprofvis_controller_get_double(track, kRPVControllerTrackMinTimestamp, 0,
                                             &min_time) == kRocProfVisResultSuccess);
    REQUIRE(rocprofvis_controller_get_double(track, kRPVControllerTrackMaxTimestamp, 0,
                                             &max_time) == kRocProfVisResultSuccess);
    REQUIRE(rocprofvis_controller_set_double(context,
                                             kRPVControllerScriptContextTimeRangeStart, 0,
                                             min_time) == kRocProfVisResultSuccess);
    REQUIRE(rocprofvis_controller_set_double(context,
                                             kRPVControllerScriptContextTimeRangeEnd, 0,
                                             max_time) == kRocProfVisResultSuccess);

    rocprofvis_controller_future_t*        future = rocprofvis_controller_future_alloc();
    rocprofvis_controller_script_result_t* result = nullptr;
    REQUIRE(future);

    rocprofvis_result_t error = rocprofvis_script_execute_async(
        controller, "optiq.result.text(str(len(optiq.selection.tracks)))", context, future,
        &result);
    REQUIRE(error == kRocProfVisResultSuccess);
    REQUIRE(wait_for_script(future) == kRocProfVisResultSuccess);

    uint64_t future_result = kRocProfVisResultUnknownError;
    error = rocprofvis_controller_get_uint64(future, kRPVControllerFutureResult, 0,
                                             &future_result);
    REQUIRE(error == kRocProfVisResultSuccess);
    REQUIRE(future_result == kRocProfVisResultSuccess);
    REQUIRE(get_handle_string(result, kRPVControllerScriptResultText) == "1");

    rocprofvis_script_result_free(result);
    rocprofvis_controller_future_free(future);
    rocprofvis_controller_arguments_free(context);
    rocprofvis_controller_free(controller);
}
