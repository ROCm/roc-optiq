# Copyright Advanced Micro Devices, Inc.
# SPDX-License-Identifier: MIT

set(TARGET_NAME roc-optiq)

cmake_minimum_required(VERSION 3.15)
project(${TARGET_NAME})

# Define the version numbers
set(PROJECT_VERSION_MAJOR 1)
set(PROJECT_VERSION_MINOR 2)
set(PROJECT_VERSION_PATCH 0)
set(PROJECT_VERSION_BUILD 1)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# /Zi stores every file's debug info in one PDB. A parallel build runs many
# cl.exe processes against that file, and without /FS they fail with C1041.
if(MSVC)
    add_compile_options(/FS)
endif()
set(BUILD_SHARED_LIBS OFF)

option(BUILD_TESTING "Build the testing tree" ON)

# TEMPORARY: gate the in-development profiler-launch and remote/SSH features
# until they ship. Both default OFF so release builds exclude the dependent
# sources and (for remote) the libssh2/mbedtls stack and its submodules.
# Remove these options and unwrap their guards when the features graduate.
option(ROCPROFVIS_ENABLE_PROFILER "Build with profiler launch support (in development)" OFF)
option(ROCPROFVIS_ENABLE_REMOTE "Build with remote/SSH support (in development)" OFF)
option(ROCPROFVIS_ENABLE_SCRIPTING "Build with embedded Python analysis scripts (in development)" OFF)
# Only meaningful with scripting: downloads a pinned CPython and ships it with
# the build. OFF builds against this machine's Python, which then only runs here.
include(CMakeDependentOption)
cmake_dependent_option(ROCPROFVIS_ENABLE_VENDOR_PYTHON
    "Vendor a pinned CPython for embedded Python analysis scripts" ON
    "ROCPROFVIS_ENABLE_SCRIPTING" OFF)

# TEMPORARY: gate the in-development trace-compare feature until it ships.
option(ROCPROFVIS_ENABLE_TRACE_COMPARE "Build with trace comparison support (in development)" OFF)

if(ROCPROFVIS_ENABLE_PROFILER)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DROCPROFVIS_ENABLE_PROFILER")
endif(ROCPROFVIS_ENABLE_PROFILER)
if(ROCPROFVIS_ENABLE_REMOTE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DROCPROFVIS_ENABLE_REMOTE")
endif(ROCPROFVIS_ENABLE_REMOTE)
if(ROCPROFVIS_ENABLE_SCRIPTING)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DROCPROFVIS_ENABLE_SCRIPTING")
endif(ROCPROFVIS_ENABLE_SCRIPTING)
if(ROCPROFVIS_ENABLE_TRACE_COMPARE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DROCPROFVIS_ENABLE_TRACE_COMPARE")
endif(ROCPROFVIS_ENABLE_TRACE_COMPARE)

# Where an install puts executables and their notices. Windows installs flat;
# elsewhere GNUInstallDirs. The vendored Python's install rules read these.
include(GNUInstallDirs)
if(WIN32)
    set(ROCPROFVIS_INSTALL_BINDIR ".")
    set(ROCPROFVIS_INSTALL_DOCDIR ".")
else()
    set(ROCPROFVIS_INSTALL_BINDIR "${CMAKE_INSTALL_BINDIR}")
    set(ROCPROFVIS_INSTALL_DOCDIR "${CMAKE_INSTALL_DOCDIR}")
endif()

# Enable testing if BUILD_TESTING is ON
if(BUILD_TESTING)
    enable_testing()
    include(CTest)
endif()

# Add third-party libraries, core, and model BEFORE controller
add_subdirectory(thirdparty)
add_subdirectory(thirdparty/perfetto/trace_processor/build)
add_subdirectory(thirdparty/perfetto/trace_processor)
if(PERFETTO_AVAILABLE)
    add_compile_definitions(ROCPROFVIS_PERFETTO_ENABLED)
endif()
add_subdirectory(src/core)
add_subdirectory(src/model)
if(ROCPROFVIS_ENABLE_SCRIPTING)
    add_subdirectory(src/python)
endif()
add_subdirectory(src/controller)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/model/inc)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/capture_git_commit.cmake)

set(ROCPROFVIS_GENERATED_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/resources/version.h.in"
    "${ROCPROFVIS_GENERATED_INCLUDE_DIR}/rocprofvis_version.h"
    @ONLY
)

# Puts what an executable loads at startup beside it, so it runs from the build
# tree: the DLLs it links (a system Python's included), the Perfetto trace
# processor, and the vendored Python runtime.
function(rocprofvis_stage_runtime target)
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

# The DataProvider, the models it fills, and their JSON translation: everything
# a client needs to read a trace through the controller.
set(PROVIDER_TARGET ${TARGET_NAME}-provider)
add_library(${PROVIDER_TARGET} STATIC
    src/view/src/rocprofvis_data_provider.cpp
    src/view/src/rocprofvis_data_provider.h
    src/view/src/rocprofvis_data_provider_json.cpp
    src/view/src/rocprofvis_data_provider_json.h
    src/view/src/rocprofvis_raw_track_data.cpp
    src/view/src/rocprofvis_raw_track_data.h
    src/view/src/rocprofvis_requests.h
    src/view/src/rocprofvis_utils.cpp
    src/view/src/rocprofvis_utils.h
    src/view/src/model/rocprofvis_analysis_model.cpp
    src/view/src/model/rocprofvis_analysis_model.h
    src/view/src/model/rocprofvis_common_defs.h
    src/view/src/model/rocprofvis_event_model.cpp
    src/view/src/model/rocprofvis_event_model.h
    src/view/src/model/rocprofvis_model_types.h
    src/view/src/model/rocprofvis_summary_model.cpp
    src/view/src/model/rocprofvis_summary_model.h
    src/view/src/model/rocprofvis_tables_model.cpp
    src/view/src/model/rocprofvis_tables_model.h
    src/view/src/model/rocprofvis_timeline_model.cpp
    src/view/src/model/rocprofvis_timeline_model.h
    src/view/src/model/rocprofvis_topology_model.cpp
    src/view/src/model/rocprofvis_topology_model.h
    src/view/src/model/rocprofvis_trace_data_model.cpp
    src/view/src/model/rocprofvis_trace_data_model.h
    src/view/src/model/compute/rocprofvis_compute_data_model.cpp
    src/view/src/model/compute/rocprofvis_compute_data_model.h
    src/view/src/model/compute/rocprofvis_compute_model_types.h
    src/view/src/model/compute/rocprofvis_memory_chart_model.cpp
    src/view/src/model/compute/rocprofvis_memory_chart_model.h
)
if(ROCPROFVIS_ENABLE_SCRIPTING)
    target_sources(${PROVIDER_TARGET} PRIVATE
        src/view/src/rocprofvis_data_provider_script.cpp)
endif()
target_include_directories(${PROVIDER_TARGET} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/view/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/view/src/model
)
target_link_libraries(${PROVIDER_TARGET} PUBLIC ${TARGET_NAME}-controller json)

# TEMPORARY (remote/SSH): only link the SSH/crypto stack when remote is built.
# Remove this guard when the remote feature graduates.
if(ROCPROFVIS_ENABLE_REMOTE)
    target_link_libraries(${PROVIDER_TARGET} PUBLIC libssh2_static)
    if(CRYPTO_BACKEND STREQUAL "OpenSSL")
        target_link_libraries(${PROVIDER_TARGET} PUBLIC OpenSSL::SSL OpenSSL::Crypto)
    endif()
endif(ROCPROFVIS_ENABLE_REMOTE)

add_subdirectory(src/server)

if(BUILD_TESTING)
    # Parser-only Memory Chart tests: need no GPU or database, so every
    # architecture layout under resources/memory_chart is validated on each run.
    set(MEMCHART_TEST_NAME ${TARGET_NAME}-memory-chart-tests)
    add_executable(${MEMCHART_TEST_NAME}
        src/view/test/rocprofvis_memory_chart_layout_tests.cpp
        src/view/src/model/compute/rocprofvis_memory_chart_model.cpp)
    target_compile_definitions(${MEMCHART_TEST_NAME} PRIVATE
        ROCPROFVIS_TEST_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    target_include_directories(${MEMCHART_TEST_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/view/src)
    target_link_libraries(${MEMCHART_TEST_NAME} PRIVATE json Catch2::Catch2WithMain)
    add_test(NAME ${MEMCHART_TEST_NAME} COMMAND ${MEMCHART_TEST_NAME})

    # The provider and its JSON, loading the sample traces with no UI attached.
    set(PROVIDER_TEST_NAME ${PROVIDER_TARGET}-tests)
    add_executable(${PROVIDER_TEST_NAME} src/view/test/rocprofvis_data_provider_json_tests.cpp)
    target_compile_definitions(${PROVIDER_TEST_NAME} PRIVATE
        ROCPROFVIS_TEST_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    target_link_libraries(${PROVIDER_TEST_NAME} PRIVATE ${PROVIDER_TARGET} Catch2::Catch2)
    rocprofvis_stage_runtime(${PROVIDER_TEST_NAME})
    add_test(NAME ${PROVIDER_TEST_NAME}-system COMMAND ${PROVIDER_TEST_NAME}
        --input_file "${CMAKE_SOURCE_DIR}/sample/rocpd-transpose.db" "[system]")
    set_tests_properties(${PROVIDER_TEST_NAME}-system PROPERTIES RUN_SERIAL ON)
    set_tests_properties(${PROVIDER_TEST_NAME}-system PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    add_test(NAME ${PROVIDER_TEST_NAME}-compute COMMAND ${PROVIDER_TEST_NAME}
        --input_file "${CMAKE_SOURCE_DIR}/sample/rocprof_compute_23ed6f36.db" "[compute]")
    set_tests_properties(${PROVIDER_TEST_NAME}-compute PROPERTIES RUN_SERIAL ON)
    set_tests_properties(${PROVIDER_TEST_NAME}-compute PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
endif()
