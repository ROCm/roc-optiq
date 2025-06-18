cmake_minimum_required(VERSION 3.12)
project(rocprof-visualizer-controller VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)

include_directories(inc)

set(rocprofvis_controller_source_files)
list(APPEND rocprofvis_controller_source_files
    src/rocprofvis_controller_arguments.cpp
    src/rocprofvis_controller_array.cpp
    src/rocprofvis_controller_data.cpp
    src/rocprofvis_controller_event.cpp
    src/rocprofvis_controller_event_lod.cpp
    src/rocprofvis_controller_future.cpp
    src/rocprofvis_controller_graph.cpp
    src/rocprofvis_controller_sample.cpp
    src/rocprofvis_controller_sample_lod.cpp
    src/rocprofvis_controller_segment.cpp
    src/rocprofvis_controller_string_table.cpp
    src/rocprofvis_controller_timeline.cpp
    src/rocprofvis_controller_trace.cpp
	src/rocprofvis_controller_trace_compute.cpp
    src/rocprofvis_controller_track.cpp
    src/rocprofvis_controller_table.cpp
	src/rocprofvis_controller_table_system.cpp
	src/rocprofvis_controller_table_compute.cpp
    src/rocprofvis_controller.cpp
    src/rocprofvis_controller_flow_control.cpp
    src/rocprofvis_controller_call_stack.cpp
    src/rocprofvis_controller_ext_data.cpp
	src/rocprofvis_controller_plot.cpp
	src/rocprofvis_controller_plot_series.cpp
	src/rocprofvis_controller_plot_compute.cpp
)

if(JSON_SUPPORT) 
list(APPEND rocprofvis_controller_source_files
    src/rocprofvis_controller_json_trace.cpp
)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DVK_PROTOTYPES -DJSON_SUPPORT")
endif(JSON_SUPPORT)

add_library(rocprof-visualizer-controller ${rocprofvis_controller_source_files})

file(GLOB COMMON_SRC CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

if(JSON_SUPPORT) 
target_include_directories(rocprof-visualizer-controller PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../thirdparty/jsoncpp)
target_link_libraries(rocprof-visualizer-controller PUBLIC json)
endif(JSON_SUPPORT)

target_link_libraries(rocprof-visualizer-controller PUBLIC rocprof-visualizer-core)
target_link_libraries(rocprof-visualizer-controller PUBLIC datamodel)
target_link_libraries(rocprof-visualizer-controller PUBLIC spdlog)
target_link_libraries(rocprof-visualizer-controller PUBLIC csv)
target_include_directories(rocprof-visualizer-controller PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../model/inc)
target_include_directories(rocprof-visualizer-controller PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../core/inc)

add_executable(${PROJECT_NAME}-basic-tests tests/basic_tests.cpp)
target_link_libraries(${PROJECT_NAME}-basic-tests PRIVATE ${PROJECT_NAME})
target_link_libraries(${PROJECT_NAME}-basic-tests PRIVATE Catch2::Catch2WithMain)

# Csv parser enables coverage in debug builds so we need to enable it too
if((CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX) AND (CMAKE_BUILD_TYPE STREQUAL "Debug" OR NOT CMAKE_BUILD_TYPE))
    target_link_options(${PROJECT_NAME}-basic-tests PRIVATE --coverage)
endif()

add_test(NAME ${PROJECT_NAME}-basic-tests COMMAND ${PROJECT_NAME}-basic-tests)
set_tests_properties(${PROJECT_NAME}-basic-tests PROPERTIES RUN_SERIAL ON)
