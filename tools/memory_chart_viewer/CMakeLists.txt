# Copyright Advanced Micro Devices, Inc.
# SPDX-License-Identifier: MIT
#
# Standalone build for the Memory Chart Viewer. It reuses the repo's vendored
# thirdparty libraries (GLFW, ImGui, jsoncpp, SQLite) and the shared memory-chart
# JSON model, but does NOT depend on the main application, the model/controller
# libraries, or the Vulkan SDK.
#
# Build (from the repo root):
#   cmake -S tools/memory_chart_viewer -B tools/memory_chart_viewer/build
#   cmake --build tools/memory_chart_viewer/build --config Release
#
# Run:
#   tools/memory_chart_viewer/build/Release/memory_chart_viewer \
#       resources/memory_chart/memory_chart_default.json sample/rocprof_compute_23ed6f36.db

cmake_minimum_required(VERSION 3.15)

# Build a fully self-contained executable: statically link the MSVC runtime so
# the binary has NO Visual C++ Redistributable dependency (no VCRUNTIME140.dll /
# MSVCP140.dll). CMAKE_POLICY_DEFAULT_CMP0091=NEW forces the modern runtime-
# selection policy onto the vendored subprojects (sqlite3/glfw) that still
# declare older cmake minimums, so the whole build uses the same static runtime
# and avoids an /MT-vs-/MD mismatch at link time.
set(CMAKE_POLICY_DEFAULT_CMP0091 NEW)

project(memory_chart_viewer LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

get_filename_component(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
set(TP "${REPO_ROOT}/thirdparty")

# --- GLFW (vendored) ---
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
set(GLFW_LIBRARY_TYPE "STATIC" CACHE STRING "" FORCE)
add_subdirectory("${TP}/glfw" "${CMAKE_BINARY_DIR}/glfw")

# --- SQLite (vendored; extracts sqlite3.c from sqlite3.zip) ---
add_subdirectory("${TP}/sqlite3" "${CMAKE_BINARY_DIR}/sqlite3")

# --- OpenGL ---
find_package(OpenGL REQUIRED)

# --- ImGui (core + GLFW/OpenGL3 backends only; no Vulkan) ---
add_library(mcv_imgui STATIC
    "${TP}/imgui/imgui.cpp"
    "${TP}/imgui/imgui_draw.cpp"
    "${TP}/imgui/imgui_tables.cpp"
    "${TP}/imgui/imgui_widgets.cpp"
    "${TP}/imgui/backends/imgui_impl_glfw.cpp"
    "${TP}/imgui/backends/imgui_impl_opengl3.cpp"
)
target_compile_definitions(mcv_imgui PUBLIC IMGUI_DEFINE_MATH_OPERATORS)
target_include_directories(mcv_imgui PUBLIC
    "${TP}/imgui"
    "${TP}/imgui/backends"
)
target_link_libraries(mcv_imgui PUBLIC glfw OpenGL::GL)

# --- jsoncpp (JSON parser + double-conversion) ---
add_library(mcv_json STATIC
    "${TP}/jsoncpp/json.cpp"
    "${TP}/jsoncpp/double-conversion/bignum.cc"
    "${TP}/jsoncpp/double-conversion/bignum-dtoa.cc"
    "${TP}/jsoncpp/double-conversion/cached-powers.cc"
    "${TP}/jsoncpp/double-conversion/double-to-string.cc"
    "${TP}/jsoncpp/double-conversion/fast-dtoa.cc"
    "${TP}/jsoncpp/double-conversion/fixed-dtoa.cc"
    "${TP}/jsoncpp/double-conversion/string-to-double.cc"
    "${TP}/jsoncpp/double-conversion/strtod.cc"
)
target_include_directories(mcv_json
    PUBLIC "${TP}/jsoncpp"
    PRIVATE "${TP}/jsoncpp/double-conversion"
)

# --- The viewer ---
add_executable(memory_chart_viewer
    src/main.cpp
    src/metric_store.cpp
    src/memchart_renderer.cpp
    # Reuse the app's memory-chart JSON model (parser + recursive block tree).
    "${REPO_ROOT}/src/view/src/compute/rocprofvis_memory_chart_model.cpp"
)
target_include_directories(memory_chart_viewer PRIVATE
    src
    "${REPO_ROOT}/src/view/src/compute"
)
target_link_libraries(memory_chart_viewer PRIVATE mcv_imgui mcv_json sqlite3 OpenGL::GL)
