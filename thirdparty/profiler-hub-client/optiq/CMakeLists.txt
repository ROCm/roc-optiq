cmake_minimum_required(VERSION 3.16)
 
project(optiq_interface VERSION 1.0.0 LANGUAGES CXX)

# Create the static library
add_library(optiq_interface STATIC
    profiler_hub_interface.cpp
    profiler_hub_trace.cpp
    profiler_hub_future.cpp
)
 
# Set include directories so consumers can find the headers
target_include_directories(optiq_interface
    PRIVATE
        ${PROFILER_HUB_INCLUDE_DIR}
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
 
# Require C++17 (adjust as needed)
target_compile_features(optiq_interface PUBLIC cxx_std_17)