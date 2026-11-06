# Copyright Advanced Micro Devices, Inc.
# SPDX-License-Identifier: MIT

# Serves DataProviderJson over HTTP for a web front end.
set(SERVER_TARGET ${TARGET_NAME}-server)

add_executable(${SERVER_TARGET}
    src/main.cpp
    src/rocprofvis_trace_server.cpp
    src/rocprofvis_trace_server.h
)
target_include_directories(${SERVER_TARGET} PRIVATE ${ROCPROFVIS_GENERATED_INCLUDE_DIR})
target_link_libraries(${SERVER_TARGET} PRIVATE ${TARGET_NAME}-provider cpp-httplib)
# httplib.h is one 21k-line header, which overruns the default section limit.
target_compile_options(${SERVER_TARGET} PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
rocprofvis_stage_runtime(${SERVER_TARGET})
