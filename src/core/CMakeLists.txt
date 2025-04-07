cmake_minimum_required(VERSION 3.12)
project(rocprof-visualizer-core VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)

include_directories(inc)

file(GLOB COMMON_SRC CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

add_subdirectory(../../thirdparty/spdlog thirdparty/spdlog)

add_library(rocprof-visualizer-core ${COMMON_SRC})
target_link_libraries(rocprof-visualizer-core PRIVATE spdlog)
