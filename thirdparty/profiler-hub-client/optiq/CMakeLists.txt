cmake_minimum_required(VERSION 3.16)

project(optiq_interface VERSION 1.0.0 LANGUAGES CXX)

add_library(optiq_interface STATIC
    trace_context.cpp
)

target_include_directories(optiq_interface
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${PROFILER_HUB_PREBUILT_DIR}/include/profiler-hub
)

target_link_libraries(optiq_interface PUBLIC profiler-hub::profiler-hub)

target_link_options(optiq_interface INTERFACE
    "-Wl,--disable-new-dtags,-rpath,${PROFILER_HUB_PREBUILT_DIR}/lib")

target_compile_features(optiq_interface PRIVATE cxx_std_17)
