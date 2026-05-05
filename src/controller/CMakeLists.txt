# Copyright Advanced Micro Devices, Inc.
# SPDX-License-Identifier: MIT


set(TARGET_NAME roc-optiq-controller)

cmake_minimum_required(VERSION 3.12)
project(${TARGET_NAME} VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)

include_directories(inc)

set(rocprofvis_controller_source_files)
list(APPEND rocprofvis_controller_source_files
    src/rocprofvis_controller_arguments.cpp
    src/rocprofvis_controller_array.cpp
    src/rocprofvis_controller_data.cpp
    src/rocprofvis_controller_future.cpp    
    src/rocprofvis_controller_handle.cpp    
    src/rocprofvis_controller_string_table.cpp    
    src/rocprofvis_controller_trace.cpp    
    src/rocprofvis_controller_table.cpp    
    src/rocprofvis_controller.cpp
    src/rocprofvis_controller_job_system.cpp
    src/rocprofvis_controller_profiler_process.cpp
    src/rocprofvis_profiler.cpp
    src/system/rocprofvis_controller_event.cpp
    src/system/rocprofvis_controller_graph.cpp
    src/system/rocprofvis_controller_sample.cpp
    src/system/rocprofvis_controller_sample_lod.cpp
    src/system/rocprofvis_controller_segment.cpp
    src/system/rocprofvis_controller_timeline.cpp
    src/system/rocprofvis_controller_trace_system.cpp
    src/system/rocprofvis_controller_track.cpp
    src/system/rocprofvis_controller_table_system.cpp	
    src/system/rocprofvis_controller_flow_control.cpp
    src/system/rocprofvis_controller_call_stack.cpp
    src/system/rocprofvis_controller_ext_data.cpp
    src/system/rocprofvis_controller_mem_mgmt.cpp
    src/system/rocprofvis_controller_summary.cpp
    src/system/rocprofvis_controller_summary_metrics.cpp
    src/system/rocprofvis_controller_topology.cpp
)

if(COMPUTE_UI_SUPPORT) 
list(APPEND rocprofvis_controller_source_files
    src/compute/rocprofvis_controller_trace_compute.cpp
    src/compute/rocprofvis_controller_workload.cpp
    src/compute/rocprofvis_controller_kernel.cpp
    src/compute/rocprofvis_controller_metrics_container.cpp
    src/compute/rocprofvis_controller_roofline.cpp
    src/compute/rocprofvis_controller_table_compute_pivot.cpp
)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DVK_PROTOTYPES -DCOMPUTE_UI_SUPPORT")
endif(COMPUTE_UI_SUPPORT)

add_library(${TARGET_NAME} ${rocprofvis_controller_source_files})

file(GLOB COMMON_SRC CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

target_include_directories(${TARGET_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../thirdparty/jsoncpp)
target_link_libraries(${TARGET_NAME} PRIVATE json)

target_link_libraries(${TARGET_NAME} PUBLIC roc-optiq-core)
target_link_libraries(${TARGET_NAME} PUBLIC datamodel)
target_link_libraries(${TARGET_NAME} PUBLIC spdlog)
target_include_directories(${TARGET_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../model/inc)
target_include_directories(${TARGET_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../core/inc)
target_include_directories(${TARGET_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

if(BUILD_TESTING)
	set(SYSTEM_TEST_NAME ${PROJECT_NAME}-system-tests)
	set(COMPUTE_TEST_NAME ${PROJECT_NAME}-compute-tests)
    add_executable(${SYSTEM_TEST_NAME} tests/rocprofvis_controller_system_tests.cpp)
    target_link_libraries(${SYSTEM_TEST_NAME} PRIVATE ${PROJECT_NAME})
    target_link_libraries(${SYSTEM_TEST_NAME} PRIVATE Catch2::Catch2WithMain)
    add_test(NAME ${SYSTEM_TEST_NAME}-RPD COMMAND ${SYSTEM_TEST_NAME} --input_file "${CMAKE_SOURCE_DIR}/sample/trace_70b_1024_32.rpd")
    set_tests_properties(${SYSTEM_TEST_NAME}-RPD PROPERTIES RUN_SERIAL ON)
    set_tests_properties(${SYSTEM_TEST_NAME}-RPD PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    add_test(NAME ${SYSTEM_TEST_NAME}-DB COMMAND ${SYSTEM_TEST_NAME} --input_file "${CMAKE_SOURCE_DIR}/sample/rocpd-transpose.db")
    set_tests_properties(${SYSTEM_TEST_NAME}-DB PROPERTIES RUN_SERIAL ON)
    set_tests_properties(${SYSTEM_TEST_NAME}-DB PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
	add_executable(${COMPUTE_TEST_NAME} tests/rocprofvis_controller_compute_tests.cpp)
	target_link_libraries(${COMPUTE_TEST_NAME} PRIVATE ${PROJECT_NAME})
    target_link_libraries(${COMPUTE_TEST_NAME} PRIVATE Catch2::Catch2WithMain)
    add_test(NAME ${COMPUTE_TEST_NAME} COMMAND ${COMPUTE_TEST_NAME} --input_file "${CMAKE_SOURCE_DIR}/sample/rocprof_compute_23ed6f36.db")
    set_tests_properties(${COMPUTE_TEST_NAME} PROPERTIES RUN_SERIAL ON)
    set_tests_properties(${COMPUTE_TEST_NAME} PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
endif()
