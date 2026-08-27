// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_controller.h"
#include "rocprofvis_controller_script.h"
#include "rocprofvis_core.h"
#include "rocprofvis_python_runtime.h"

#include <catch2/catch_session.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cfloat>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
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

// What one direct-to-runtime exec reported. The deadline lives in the runtime,
// so these tests go straight at it rather than through the script ABI, which
// always asks for the production timeout.
struct exec_outcome_t
{
    std::mutex                 mutex;
    std::condition_variable    done_cv;
    bool                       done   = false;
    rocprofvis_python_result_t result = kRocProfVisPythonSuccess;
    std::string                error;
};

void
on_exec_done(void* user, rocprofvis_python_result_t result, char const* error_message)
{
    exec_outcome_t* outcome = static_cast<exec_outcome_t*>(user);
    {
        std::lock_guard<std::mutex> lock(outcome->mutex);
        outcome->result = result;
        outcome->error  = error_message ? error_message : "";
        outcome->done   = true;
    }
    outcome->done_cv.notify_all();
}

// Runs source with an explicit deadline and blocks until it reports back.
// Returns false when the run never completed, which is the failure this is
// looking for: a deadline that does not fire leaves the interpreter wedged.
bool
run_with_timeout(char const* source, uint64_t timeout_ms, exec_outcome_t& outcome,
                 uint64_t wait_budget_ms)
{
    if(rocprofvis_python_exec(source, nullptr, &outcome, on_exec_done, timeout_ms) !=
       kRocProfVisPythonSuccess)
    {
        return false;
    }
    std::unique_lock<std::mutex> lock(outcome.mutex);
    return outcome.done_cv.wait_for(lock, std::chrono::milliseconds(wait_budget_ms),
                                    [&outcome]() { return outcome.done; });
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

TEST_CASE("Script execute stops a script that never ends")
{
    // Nothing in this script yields, calls a binding, or raises. Only the
    // deadline can end it, so if the wait below expires the interpreter thread
    // is wedged for the life of the process.
    exec_outcome_t outcome;
    REQUIRE(run_with_timeout("while True:\n    pass\n", 500, outcome, 15000));

    std::lock_guard<std::mutex> lock(outcome.mutex);
    // A timeout is a script to go and fix, so it is an error rather than a
    // cancellation, and it says how long it was given.
    REQUIRE(outcome.result == kRocProfVisPythonError);
    REQUIRE(outcome.error.find("timed out") != std::string::npos);
}

TEST_CASE("Script execute stops a script that swallows the interrupt")
{
    // A bare except catches the first KeyboardInterrupt and keeps going, which
    // model-written code does. The interrupt has to be re-sent for the deadline
    // to mean anything.
    exec_outcome_t outcome;
    REQUIRE(run_with_timeout("while True:\n"
                             "    try:\n"
                             "        pass\n"
                             "    except Exception:\n"
                             "        pass\n",
                             500, outcome, 15000));

    std::lock_guard<std::mutex> lock(outcome.mutex);
    REQUIRE(outcome.result == kRocProfVisPythonError);
    REQUIRE(outcome.error.find("timed out") != std::string::npos);
}

TEST_CASE("Script execute survives a script that outstayed its deadline")
{
    // The stopped script must not leave a raise pending for whatever runs
    // next, which is what the clear at the top of each exec is for.
    exec_outcome_t timed_out;
    REQUIRE(run_with_timeout("while True:\n    pass\n", 500, timed_out, 15000));

    exec_outcome_t healthy;
    REQUIRE(run_with_timeout("x = 1 + 1\n", 0, healthy, 15000));

    std::lock_guard<std::mutex> lock(healthy.mutex);
    REQUIRE(healthy.result == kRocProfVisPythonSuccess);
    REQUIRE(healthy.error.empty());
}

TEST_CASE("Script error carries the traceback and the failing line")
{
    rocprofvis_controller_future_t*        future = rocprofvis_controller_future_alloc();
    rocprofvis_controller_script_result_t* result = nullptr;
    REQUIRE(future);

    rocprofvis_result_t error = rocprofvis_script_execute_async(
        nullptr, "def inner():\n    raise ValueError('boom')\ninner()\n", nullptr,
        future, &result);
    REQUIRE(error == kRocProfVisResultSuccess);
    REQUIRE(wait_for_script(future) == kRocProfVisResultSuccess);

    // Without the traceback the author only learns that something raised, which
    // is not enough to fix the line that did it.
    std::string message = get_handle_string(result, kRPVControllerScriptResultErrorMessage);
    REQUIRE(message.find("ValueError") != std::string::npos);
    REQUIRE(message.find("boom") != std::string::npos);
    REQUIRE(message.find("Traceback") != std::string::npos);
    REQUIRE(message.find("inner") != std::string::npos);

    rocprofvis_script_result_free(result);
    rocprofvis_controller_future_free(future);
}

// Runs source through the real script ABI and returns the result text, with
// any error message appended so a failure says why rather than just comparing
// unequal.
namespace
{
std::string
run_script_text(char const* source)
{
    rocprofvis_controller_future_t*        future = rocprofvis_controller_future_alloc();
    rocprofvis_controller_script_result_t* result = nullptr;
    if(!future)
    {
        return "no future";
    }
    std::string out;
    if(rocprofvis_script_execute_async(nullptr, source, nullptr, future, &result) ==
           kRocProfVisResultSuccess &&
       wait_for_script(future) == kRocProfVisResultSuccess)
    {
        out = get_handle_string(result, kRPVControllerScriptResultText);
        const std::string error =
            get_handle_string(result, kRPVControllerScriptResultErrorMessage);
        if(!error.empty())
        {
            out += "|ERROR:" + error;
        }
    }
    if(result)
    {
        rocprofvis_script_result_free(result);
    }
    rocprofvis_controller_future_free(future);
    return out;
}
}  // namespace

TEST_CASE("Script print writes into the result")
{
    // print is the first thing anyone writes, so it goes to the result rather
    // than failing on a sandbox with no stdout.
    REQUIRE(run_script_text("print('hello')") == "hello");
    REQUIRE(run_script_text("print('a', 1, 2.5)") == "a 1 2.5");
    REQUIRE(run_script_text("print('a', 'b', sep='-')") == "a-b");
}

TEST_CASE("Script can define classes and use the allowlisted modules")
{
    // A `class` statement compiles to __build_class__. Without it every class
    // fails, and dataclasses and enum go with it, since both are used by
    // declaring a class. The tool description promises these work.
    REQUIRE(run_script_text("class C:\n"
                            "    def __init__(self):\n"
                            "        self.x = 7\n"
                            "print(C().x)\n") == "7");
    REQUIRE(run_script_text("@dataclasses.dataclass\n"
                            "class P:\n"
                            "    x: int\n"
                            "print(P(3).x)\n") == "3");
    REQUIRE(run_script_text("class E(enum.Enum):\n"
                            "    A = 1\n"
                            "print(E.A.value)\n") == "1");
    REQUIRE(run_script_text("P = collections.namedtuple('P', 'a b')\n"
                            "print(P(1, 2).a)\n") == "1");
    REQUIRE(run_script_text("print(statistics.median([1, 3, 2]))") == "2");
    REQUIRE(run_script_text("print(json.dumps({'a': 1}))") == "{\"a\": 1}");
}

TEST_CASE("Script sandbox still refuses the dangerous builtins")
{
    // Adding the class machinery must not have opened anything else up.
    char const* const blocked[] = { "open('x')",    "eval('1')",  "exec('x=1')",
                                    "getattr(1,'real')", "globals()",  "input()",
                                    "compile('1','<s>','eval')" };
    for(char const* source : blocked)
    {
        const std::string out = run_script_text(source);
        INFO("source: " << source << " gave: " << out);
        REQUIRE(out.find("ERROR:") != std::string::npos);
    }
}

TEST_CASE("Script sandbox refuses the interpreter-internal attributes")
{
    // Neither the import allowlist nor the reduced builtins bound these on
    // their own: an allowlisted module is a real module object, so __globals__
    // walks from any function on it back to the unrestricted builtins, and
    // __class__ walks from any instance to object.__subclasses__(). The parse
    // tree screen is what refuses them, and each of these is a published
    // escape rather than a hypothetical one.
    char const* const blocked[] = {
        "json.dumps.__globals__['__builtins__']['__import__']('os')",
        "().__class__.__base__.__subclasses__()",
        "x = (1).__class__",
        "math.sqrt.__self__",
        "[].__dict__",
        "__builtins__",
        "def f(): pass\nf.__code__",
    };
    for(char const* source : blocked)
    {
        const std::string out = run_script_text(source);
        INFO("source: " << source << " gave: " << out);
        REQUIRE(out.find("ERROR:") != std::string::npos);
        REQUIRE(out.find("not available to optiq scripts") != std::string::npos);
    }
}

TEST_CASE("Script screen leaves ordinary dunder use alone")
{
    // The screen refuses names, so it must not refuse the ones a normal script
    // writes. Defining __init__ is how the allowlisted dataclasses and enum are
    // used at all, and __name__ is in the script globals on purpose.
    REQUIRE(run_script_text("class P:\n"
                            "    def __init__(self):\n"
                            "        self.a = 7\n"
                            "print(P().a)") == "7");
    REQUIRE(run_script_text("print(len([1, 2, 3]))") == "3");
    REQUIRE(run_script_text("print(__name__)") == "__optiq_script__");
}

TEST_CASE("Script screen reports a syntax error with its line")
{
    // Screening parses first, so a broken script is refused before it runs.
    // The parser's own message is what comes back, so it still names the line.
    const std::string out = run_script_text("def f(:\n    pass");
    INFO("got: " << out);
    REQUIRE(out.find("ERROR:") != std::string::npos);
    REQUIRE(out.find("SyntaxError") != std::string::npos);
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

TEST_CASE("Script events expose level, category and sample value")
{
    rocprofvis_controller_t* controller = load_sample_controller();
    REQUIRE(controller);

    // Interval events report a numeric level, a string category and no
    // counter reading. Samples report the reading and no category.
    char const* source =
        "levels = 'none'\n"
        "categories = 'none'\n"
        "event_value = 'none'\n"
        "sample_value = 'none'\n"
        "for t in optiq.trace.tracks:\n"
        "    if t.type == optiq.TRACK_TYPE_EVENTS and t.num_entries > 0:\n"
        "        events = t.events()\n"
        "        if events:\n"
        "            levels = str(max(e.level for e in events))\n"
        "            categories = str(all(isinstance(e.category, str) for e in events))\n"
        "            event_value = str(events[0].value is None)\n"
        "            break\n"
        "for t in optiq.trace.tracks:\n"
        "    if t.type == optiq.TRACK_TYPE_SAMPLES and t.num_entries > 0:\n"
        "        samples = t.events()\n"
        "        if samples:\n"
        "            sample_value = str(isinstance(samples[0].value, float))\n"
        "            break\n"
        "optiq.result.text(levels)\n"
        "optiq.result.text(categories)\n"
        "optiq.result.text(event_value)\n"
        "optiq.result.text(sample_value)\n";

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
    std::vector<std::string> lines;
    size_t                   start = 0;
    while(start <= text.size())
    {
        size_t split = text.find('\n', start);
        if(split == std::string::npos)
        {
            lines.push_back(text.substr(start));
            break;
        }
        lines.push_back(text.substr(start, split - start));
        start = split + 1;
    }
    REQUIRE(lines.size() == 4);
    // Max level is trace dependent, so only require that it parsed.
    REQUIRE(std::stoull(lines[0]) < 256);
    REQUIRE(lines[1] == "True");
    REQUIRE(lines[2] == "True");
    if(lines[3] != "none")
    {
        REQUIRE(lines[3] == "True");
    }

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
