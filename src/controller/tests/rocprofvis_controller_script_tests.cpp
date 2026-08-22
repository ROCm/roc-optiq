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

}  // namespace

int
main(int argc, char** argv)
{
    Catch::Session session;
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
