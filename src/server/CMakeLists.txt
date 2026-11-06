# Copyright Advanced Micro Devices, Inc.
# SPDX-License-Identifier: MIT

# The web wrapper around the app: the DataProvider and its models built with no
# UI, a JSON translation of what they hold, and an HTTP server that serves it.
# The desktop app compiles the same provider and model sources itself, against
# its UI, so this directory builds with or without ROCPROFVIS_ENABLE_GUI.

set(VIEW_SRC_DIR ${CMAKE_SOURCE_DIR}/src/view/src)

# Puts what an executable loads at startup beside it, so it runs from the build
# tree: the DLLs it links (a system Python's included), the Perfetto trace
# processor, and the vendored Python runtime.
function(rocprofvis_server_stage_runtime target)
    if(WIN32)
        add_custom_command(
            TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E
                $<IF:$<BOOL:$<TARGET_RUNTIME_DLLS:${target}>>,copy_if_different,true>
                $<TARGET_RUNTIME_DLLS:${target}>
                $<$<BOOL:$<TARGET_RUNTIME_DLLS:${target}>>:$<TARGET_FILE_DIR:${target}>>
            COMMAND_EXPAND_LISTS
            COMMENT "Copying runtime DLLs next to ${target}"
        )
    endif()
    if(PERFETTO_AVAILABLE)
        perfetto_copy_dll_to(${target})
    endif()
    if(ROCPROFVIS_ENABLE_VENDOR_PYTHON)
        rocprofvis_python_vendor_deploy(${target})
    endif()
endfunction()

# ROCPROFVIS_HEADLESS compiles out the two places the models reach into the UI:
# the user's time format setting and the timeline view's track items.
set(PROVIDER_TARGET ${TARGET_NAME}-provider)
add_library(${PROVIDER_TARGET} STATIC
    ${VIEW_SRC_DIR}/rocprofvis_data_provider.cpp
    ${VIEW_SRC_DIR}/rocprofvis_raw_track_data.cpp
    ${VIEW_SRC_DIR}/rocprofvis_utils.cpp
    ${VIEW_SRC_DIR}/model/rocprofvis_analysis_model.cpp
    ${VIEW_SRC_DIR}/model/rocprofvis_event_model.cpp
    ${VIEW_SRC_DIR}/model/rocprofvis_summary_model.cpp
    ${VIEW_SRC_DIR}/model/rocprofvis_tables_model.cpp
    ${VIEW_SRC_DIR}/model/rocprofvis_timeline_model.cpp
    ${VIEW_SRC_DIR}/model/rocprofvis_topology_model.cpp
    ${VIEW_SRC_DIR}/model/rocprofvis_trace_data_model.cpp
    ${VIEW_SRC_DIR}/model/compute/rocprofvis_compute_data_model.cpp
    ${VIEW_SRC_DIR}/model/compute/rocprofvis_memory_chart_model.cpp
    src/rocprofvis_data_provider_json.cpp
    src/rocprofvis_data_provider_json.h
)
if(ROCPROFVIS_ENABLE_SCRIPTING)
    target_sources(${PROVIDER_TARGET} PRIVATE
        ${VIEW_SRC_DIR}/rocprofvis_data_provider_script.cpp
        ${VIEW_SRC_DIR}/rocprofvis_event_manager.cpp
        ${VIEW_SRC_DIR}/rocprofvis_events.cpp
    )
endif()
target_compile_definitions(${PROVIDER_TARGET} PUBLIC ROCPROFVIS_HEADLESS)
target_include_directories(${PROVIDER_TARGET} PUBLIC
    ${VIEW_SRC_DIR}
    ${VIEW_SRC_DIR}/model
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(${PROVIDER_TARGET} PUBLIC ${TARGET_NAME}-controller json)

# TEMPORARY (remote/SSH): link the SSH/crypto stack the way the app does when
# remote is built. Remove this guard when the remote feature graduates.
if(ROCPROFVIS_ENABLE_REMOTE)
    target_link_libraries(${PROVIDER_TARGET} PUBLIC libssh2_static)
    if(CRYPTO_BACKEND STREQUAL "OpenSSL")
        target_link_libraries(${PROVIDER_TARGET} PUBLIC OpenSSL::SSL OpenSSL::Crypto)
    endif()
endif(ROCPROFVIS_ENABLE_REMOTE)

# cpp-httplib serves plain HTTP here and is used header-only. The cpp-httplib
# target in thirdparty/ is Ask Optiq's TLS client and exists only with that
# feature, so the server sets the header up itself.
if(NOT EXISTS ${CMAKE_SOURCE_DIR}/thirdparty/cpp-httplib/httplib.h)
    message(FATAL_ERROR
        "ROCPROFVIS_ENABLE_WEB_SERVER is ON but thirdparty/cpp-httplib is empty. "
        "Run: git submodule update --init thirdparty/cpp-httplib")
endif()
find_package(Threads REQUIRED)

include(${CMAKE_SOURCE_DIR}/cmake/capture_git_commit.cmake)
configure_file(
    "${CMAKE_SOURCE_DIR}/resources/version.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/rocprofvis_version.h"
    @ONLY
)

set(SERVER_TARGET ${TARGET_NAME}-server)
add_executable(${SERVER_TARGET}
    src/main.cpp
    src/rocprofvis_trace_server.cpp
    src/rocprofvis_trace_server.h
)
target_include_directories(${SERVER_TARGET} PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated)
target_include_directories(${SERVER_TARGET} SYSTEM PRIVATE
    ${CMAKE_SOURCE_DIR}/thirdparty/cpp-httplib)
target_compile_definitions(${SERVER_TARGET} PRIVATE CPPHTTPLIB_NO_EXCEPTIONS)
target_link_libraries(${SERVER_TARGET} PRIVATE ${PROVIDER_TARGET} Threads::Threads)
if(WIN32)
    target_link_libraries(${SERVER_TARGET} PRIVATE ws2_32)
endif()
# httplib.h is one 21k-line header, which overruns the default section limit.
target_compile_options(${SERVER_TARGET} PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
rocprofvis_server_stage_runtime(${SERVER_TARGET})

if(BUILD_TESTING)
    # The provider and its JSON, loading the sample traces with no UI attached.
    set(PROVIDER_TEST_NAME ${PROVIDER_TARGET}-tests)
    add_executable(${PROVIDER_TEST_NAME} test/rocprofvis_data_provider_json_tests.cpp)
    target_compile_definitions(${PROVIDER_TEST_NAME} PRIVATE
        ROCPROFVIS_TEST_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    target_link_libraries(${PROVIDER_TEST_NAME} PRIVATE ${PROVIDER_TARGET} Catch2::Catch2)
    rocprofvis_server_stage_runtime(${PROVIDER_TEST_NAME})
    add_test(NAME ${PROVIDER_TEST_NAME}-system COMMAND ${PROVIDER_TEST_NAME}
        --input_file "${CMAKE_SOURCE_DIR}/sample/rocpd-transpose.db" "[system]")
    set_tests_properties(${PROVIDER_TEST_NAME}-system PROPERTIES RUN_SERIAL ON)
    set_tests_properties(${PROVIDER_TEST_NAME}-system PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    add_test(NAME ${PROVIDER_TEST_NAME}-compute COMMAND ${PROVIDER_TEST_NAME}
        --input_file "${CMAKE_SOURCE_DIR}/sample/rocprof_compute_23ed6f36.db" "[compute]")
    set_tests_properties(${PROVIDER_TEST_NAME}-compute PROPERTIES RUN_SERIAL ON)
    set_tests_properties(${PROVIDER_TEST_NAME}-compute PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
endif()
