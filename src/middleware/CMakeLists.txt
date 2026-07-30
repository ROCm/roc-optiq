# Copyright Advanced Micro Devices, Inc.
# SPDX-License-Identifier: MIT


set(TARGET_NAME roc-optiq-middleware)

cmake_minimum_required(VERSION 3.12)
project(${TARGET_NAME} VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)

include_directories(inc)

set(rocprofvis_middleware_source_files)
list(APPEND rocprofvis_middleware_source_files
    src/rocprofvis_middleware.cpp
    src/rocprofvis_mw_enums.cpp
    src/rocprofvis_mw_json.cpp
    src/rocprofvis_mw_methods.cpp
    src/rocprofvis_mw_serialize.cpp
    src/rocprofvis_mw_session.cpp
)

add_library(${TARGET_NAME} ${rocprofvis_middleware_source_files})

target_include_directories(${TARGET_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/inc)
target_include_directories(${TARGET_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_include_directories(${TARGET_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../controller/inc)
target_include_directories(${TARGET_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../thirdparty/jsoncpp)

target_link_libraries(${TARGET_NAME} PUBLIC roc-optiq-controller)
target_link_libraries(${TARGET_NAME} PRIVATE json)
target_link_libraries(${TARGET_NAME} PRIVATE spdlog)

# The middleware core is transport agnostic. Two adapters ship with it, both
# built from platform facilities alone so neither adds a third-party
# dependency.

# Newline-delimited JSON-RPC over stdin/stdout, for a client that can spawn a
# child process.
set(STDIO_TARGET_NAME ${TARGET_NAME}-stdio)
add_executable(${STDIO_TARGET_NAME} tools/rocprofvis_mw_stdio_main.cpp)
target_link_libraries(${STDIO_TARGET_NAME} PRIVATE ${TARGET_NAME})
target_link_libraries(${STDIO_TARGET_NAME} PRIVATE spdlog)

# The same protocol over HTTP and WebSocket, for a browser or a client on
# another machine.
set(HTTP_TARGET_NAME ${TARGET_NAME}-http)
set(rocprofvis_middleware_http_source_files)
list(APPEND rocprofvis_middleware_http_source_files
    tools/http/rocprofvis_mw_http.cpp
    tools/http/rocprofvis_mw_http_main.cpp
    tools/http/rocprofvis_mw_net.cpp
    tools/http/rocprofvis_mw_websocket.cpp
)
add_executable(${HTTP_TARGET_NAME} ${rocprofvis_middleware_http_source_files})
target_include_directories(${HTTP_TARGET_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tools/http)
target_link_libraries(${HTTP_TARGET_NAME} PRIVATE ${TARGET_NAME})
target_link_libraries(${HTTP_TARGET_NAME} PRIVATE spdlog)
if(WIN32)
    target_link_libraries(${HTTP_TARGET_NAME} PRIVATE ws2_32)
endif()

if(BUILD_TESTING)
    set(MIDDLEWARE_TEST_NAME ${TARGET_NAME}-tests)
    add_executable(${MIDDLEWARE_TEST_NAME} tests/rocprofvis_middleware_tests.cpp)
    target_link_libraries(${MIDDLEWARE_TEST_NAME} PRIVATE ${TARGET_NAME})
    target_link_libraries(${MIDDLEWARE_TEST_NAME} PRIVATE Catch2::Catch2WithMain)
    target_include_directories(${MIDDLEWARE_TEST_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../thirdparty/jsoncpp)
    add_test(NAME ${MIDDLEWARE_TEST_NAME}-RPD
             COMMAND ${MIDDLEWARE_TEST_NAME} --input_file "${CMAKE_SOURCE_DIR}/sample/trace_70b_1024_32.rpd")
    set_tests_properties(${MIDDLEWARE_TEST_NAME}-RPD PROPERTIES RUN_SERIAL ON)
    set_tests_properties(${MIDDLEWARE_TEST_NAME}-RPD PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    add_test(NAME ${MIDDLEWARE_TEST_NAME}-DB
             COMMAND ${MIDDLEWARE_TEST_NAME} --input_file "${CMAKE_SOURCE_DIR}/sample/rocpd-transpose.db")
    set_tests_properties(${MIDDLEWARE_TEST_NAME}-DB PROPERTIES RUN_SERIAL ON)
    set_tests_properties(${MIDDLEWARE_TEST_NAME}-DB PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

    # The transport codecs need no trace, so they are a separate target that
    # runs in a fraction of a second.
    set(TRANSPORT_TEST_NAME ${TARGET_NAME}-transport-tests)
    add_executable(${TRANSPORT_TEST_NAME}
        tests/rocprofvis_middleware_transport_tests.cpp
        tools/http/rocprofvis_mw_http.cpp
        tools/http/rocprofvis_mw_websocket.cpp
    )
    target_include_directories(${TRANSPORT_TEST_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tools/http)
    target_link_libraries(${TRANSPORT_TEST_NAME} PRIVATE Catch2::Catch2WithMain)
    add_test(NAME ${TRANSPORT_TEST_NAME} COMMAND ${TRANSPORT_TEST_NAME})
endif()
