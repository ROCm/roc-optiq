#// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

set(TARGET_NAME roc-optiq-core)

cmake_minimum_required(VERSION 3.12)
project(${TARGET_NAME} VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)

include_directories(inc)

file(GLOB COMMON_SRC CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

add_library(${TARGET_NAME} ${COMMON_SRC})
target_link_libraries(${TARGET_NAME} PRIVATE spdlog)
