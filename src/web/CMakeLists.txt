# Copyright Advanced Micro Devices, Inc.
# SPDX-License-Identifier: MIT

set(WEB_TARGET_NAME roc-optiq-web)

cmake_minimum_required(VERSION 3.15)

include(FetchContent)

FetchContent_Declare(
    httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG        v0.18.3
    GIT_SHALLOW    TRUE
)
set(HTTPLIB_COMPILE OFF CACHE BOOL "")
FetchContent_MakeAvailable(httplib)

add_executable(${WEB_TARGET_NAME}
    src/main.cpp
    src/rocprofvis_json_middleware.cpp
    src/rocprofvis_web_server.cpp
)

target_include_directories(${WEB_TARGET_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../controller/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../model/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../../thirdparty/jsoncpp
)

target_link_libraries(${WEB_TARGET_NAME} PRIVATE
    roc-optiq-controller
    datamodel
    roc-optiq-core
    json
    spdlog
    httplib::httplib
)

# Copy the frontend files next to the executable
add_custom_command(TARGET ${WEB_TARGET_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${WEB_TARGET_NAME}>/frontend"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/frontend"
        "$<TARGET_FILE_DIR:${WEB_TARGET_NAME}>/frontend"
    COMMENT "Copying web frontend to build directory"
)
