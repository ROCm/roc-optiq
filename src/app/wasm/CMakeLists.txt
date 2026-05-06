# Copyright Advanced Micro Devices, Inc.
# SPDX-License-Identifier: MIT
#
# Emscripten / WebAssembly build for the ROCm Optiq visualizer.
#
# This is a STANDALONE CMake project, intentionally separate from the root
# CMakeLists.txt so the existing native desktop build is completely
# unaffected. It must be configured with `emcmake cmake` (see build.ps1).
#
# It pulls in the project's own thirdparty/ tree (with EMSCRIPTEN-gated
# parts skipped) and progressively adds the project's libraries as each
# milestone is brought online:
#
#   M1 (done): ImGui + GLFW (contrib.glfw3 port) + WebGL2 stand-alone demo.
#   M2 (done): Loaded inside a VS Code/Cursor webview panel.
#   M3 (current): Compile and link `roc-optiq-core` so the WASM target
#                 starts depending on real project libraries. Subsequent
#                 substeps will add datamodel, controller, and the view.
#   M4: datamodel + sqlite + threads.
#   M5: file ingestion from VS Code into the WASM virtual FS.

cmake_minimum_required(VERSION 3.22)
project(roc-optiq-wasm CXX C)

if(NOT EMSCRIPTEN)
    message(FATAL_ERROR
        "src/app/wasm/CMakeLists.txt must be configured with emcmake cmake.\n"
        "See src/app/wasm/build.ps1 for the recommended invocation.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

# Resolve repo root so we can pull in shared subdirectories.
get_filename_component(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)

# Disable bits we don't want pulled into the WASM build.
set(BUILD_TESTING        OFF CACHE BOOL "" FORCE)
# Compute support pulls in extra view + controller sources so .db traces
# from Rocprof Compute (`compute_metadata` schema) can be opened in the
# WASM panel as well.
set(COMPUTE_UI_SUPPORT   ON  CACHE BOOL "" FORCE)
set(USE_NATIVE_FILE_DIALOG OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_TESTS   OFF CACHE BOOL "" FORCE)
set(CATCH_BUILD_TESTING  OFF CACHE BOOL "" FORCE)

# thirdparty/CMakeLists.txt expects ICON_HEADER from the parent scope so it
# can wire `imgui_binary_to_compressed_c` to regenerate the icon font
# header. That host tool can't be cross-compiled to wasm32, so:
#   - We point ICON_HEADER at the path the native build already uses, and
#   - We rely on the pre-existing checked-out (or previously generated)
#     header. View files that #include "rocprofvis_icon_data.h" therefore
#     need a one-time native build before the WASM target can build.
set(ICON_HEADER "${REPO_ROOT}/src/view/src/icons/rocprofvis_icon_data.h" CACHE INTERNAL "")

# Reuse the project's own thirdparty/ tree. The thirdparty CMakeLists has
# small `if(NOT EMSCRIPTEN)` gates around Vulkan and the vendored GLFW so
# the rest (imgui, spdlog, yaml-cpp, json, ImGuiFileDialog) builds for
# wasm32 without further changes.
add_subdirectory("${REPO_ROOT}/thirdparty" thirdparty)

# Project libraries.
add_subdirectory("${REPO_ROOT}/src/core"       src/core)
add_subdirectory("${REPO_ROOT}/src/model"      src/model)
add_subdirectory("${REPO_ROOT}/src/controller" src/controller)

# View source list. This is intentionally duplicated from the root
# CMakeLists.txt VIEW_FILES list; it should be factored into a shared
# .cmake file in the M6 polish pass so both targets share one source of
# truth. For now the native build remains untouched.
set(ROCPROFVIS_VIEW_SOURCES
    "${REPO_ROOT}/src/view/src/rocprofvis_appwindow.cpp"
    "${REPO_ROOT}/src/view/src/rocprofvis_annotation_view.cpp"
    "${REPO_ROOT}/src/view/src/rocprofvis_track_item.cpp"
    "${REPO_ROOT}/src/view/src/rocprofvis_flame_track_item.cpp"
    "${REPO_ROOT}/src/view/src/rocprofvis_timeline_arrow.cpp"
    "${REPO_ROOT}/src/view/src/rocprofvis_events_view.cpp"
    "${REPO_ROOT}/src/view/src/rocprofvis_settings_manager.cpp"
    "${REPO_ROOT}/src/view/src/rocprofvis_font_manager.cpp"
    "${REPO_ROOT}/src/view/src/rocprofvis_annotations.cpp"
    "${REPO_ROOT}/src/view/src/rocprofvis_settings_panel.cpp"
    "${REPO_ROOT}/src/view/src/rocprofvis_hotkey_manager.cpp"
    "${REPO_ROOT}/src/view/src/rocprofvis_stickynote.cpp"
    "${REPO_ROOT}/src/view/src/rocprofvis_click_manager.cpp"
    "${REPO_ROOT}/src/view/src/rocprofvis_analysis_view.cpp"
    "${REPO_ROOT}/src/view/src/rocprofvis_trace_view.cpp"
    "${REPO_ROOT}/src/view/src/rocprofvis_line_track_item.cpp"
    "${REPO_ROOT}/src/view/src/rocprofvis_timeline_view.cpp"
    "${REPO_ROOT}/src/view/src/rocprofvis_sidebar.cpp"
    "${REPO_ROOT}/src/view/src/rocprofvis_view_module.cpp"
    "${REPO_ROOT}/src/view/src/rocprofvis_data_provider.cpp"
    "${REPO_ROOT}/src/view/src/rocprofvis_root_view.cpp"
    "${REPO_ROOT}/src/view/src/model/rocprofvis_topology_model.cpp"
    "${REPO_ROOT}/src/view/src/model/rocprofvis_timeline_model.cpp"
    "${REPO_ROOT}/src/view/src/model/rocprofvis_tables_model.cpp"
    "${REPO_ROOT}/src/view/src/model/rocprofvis_summary_model.cpp"
    "${REPO_ROOT}/src/view/src/model/rocprofvis_event_model.cpp"
    "${REPO_ROOT}/src/view/src/model/rocprofvis_trace_data_model.cpp"
    "${REPO_ROOT}/src/view/src/rocprofvis_raw_track_data.cpp"
    "${REPO_ROOT}/src/view/src/rocprofvis_events.cpp"
    "${REPO_ROOT}/src/view/src/rocprofvis_event_manager.cpp"
    "${REPO_ROOT}/src/view/src/rocprofvis_utils.cpp"
    "${REPO_ROOT}/src/view/src/rocprofvis_timeline_selection.cpp"
    "${REPO_ROOT}/src/view/src/rocprofvis_track_topology.cpp"
    "${REPO_ROOT}/src/view/src/rocprofvis_track_details.cpp"
    "${REPO_ROOT}/src/view/src/rocprofvis_project.cpp"
    "${REPO_ROOT}/src/view/src/rocprofvis_multi_track_table.cpp"
    "${REPO_ROOT}/src/view/src/rocprofvis_event_search.cpp"
    "${REPO_ROOT}/src/view/src/rocprofvis_summary_view.cpp"
    "${REPO_ROOT}/src/view/src/rocprofvis_time_to_pixel.cpp"
    "${REPO_ROOT}/src/view/src/rocprofvis_minimap.cpp"
    "${REPO_ROOT}/src/view/src/rocprofvis_presets.cpp"
    "${REPO_ROOT}/src/view/src/widgets/rocprofvis_widget.cpp"
    "${REPO_ROOT}/src/view/src/widgets/rocprofvis_split_containers.cpp"
    "${REPO_ROOT}/src/view/src/widgets/rocprofvis_flex_container.cpp"
    "${REPO_ROOT}/src/view/src/widgets/rocprofvis_tab_container.cpp"
    "${REPO_ROOT}/src/view/src/widgets/rocprofvis_editable_textfield.cpp"
    "${REPO_ROOT}/src/view/src/widgets/rocprofvis_debug_window.cpp"
    "${REPO_ROOT}/src/view/src/widgets/rocprofvis_gui_helpers.cpp"
    "${REPO_ROOT}/src/view/src/widgets/rocprofvis_infinite_scroll_table.cpp"
    "${REPO_ROOT}/src/view/src/widgets/rocprofvis_dialog.cpp"
    "${REPO_ROOT}/src/view/src/widgets/rocprofvis_notification_manager.cpp"
    # Compute support
    "${REPO_ROOT}/src/view/src/compute/rocprofvis_compute_view.cpp"
    "${REPO_ROOT}/src/view/src/compute/rocprofvis_compute_table_view.cpp"
    "${REPO_ROOT}/src/view/src/compute/rocprofvis_compute_kernel_details.cpp"
    "${REPO_ROOT}/src/view/src/compute/rocprofvis_compute_memory_chart.cpp"
    "${REPO_ROOT}/src/view/src/compute/rocprofvis_compute_selection.cpp"
    "${REPO_ROOT}/src/view/src/compute/rocprofvis_compute_summary.cpp"
    "${REPO_ROOT}/src/view/src/compute/rocprofvis_compute_roofline.cpp"
    "${REPO_ROOT}/src/view/src/compute/rocprofvis_compute_workload_view.cpp"
    "${REPO_ROOT}/src/view/src/compute/rocprofvis_compute_kernel_metric_table.cpp"
    "${REPO_ROOT}/src/view/src/compute/rocprofvis_compute_comparison.cpp"
    "${REPO_ROOT}/src/view/src/compute/rocprofvis_compute_tester.cpp"
    "${REPO_ROOT}/src/view/src/model/compute/rocprofvis_compute_data_model.cpp"
    "${REPO_ROOT}/src/view/src/widgets/rocprofvis_compute_widget.cpp"
    "${REPO_ROOT}/src/view/src/widgets/rocprofvis_query_builder.cpp"
)

add_library(roc-optiq-view STATIC ${ROCPROFVIS_VIEW_SOURCES})
target_include_directories(roc-optiq-view PUBLIC
    "${REPO_ROOT}/src/view/inc"
    "${REPO_ROOT}/src/view/src"
    "${REPO_ROOT}/src/view/src/model"
    "${REPO_ROOT}/src/view/src/icons"
    "${REPO_ROOT}/src/app/inc"
    "${REPO_ROOT}/src/controller/inc"
    "${REPO_ROOT}/src/model/inc"
    "${REPO_ROOT}/src/core/inc"
    "${REPO_ROOT}/resources"
)
target_link_libraries(roc-optiq-view PUBLIC
    imgui
    implot
    ImGuiFileDialog
    roc-optiq-controller
    datamodel
    roc-optiq-core
    spdlog
    json
)
target_compile_options(roc-optiq-view PRIVATE
    ${EMS_PORT_FLAGS}
    -pthread
    # The view code is written assuming size_t is 64-bit (LLP64 on Windows,
    # LP64 on Linux). On wasm32 size_t is 32-bit, so brace-initialising
    # std::vector<size_t> with UINT64_MAX-style constants triggers a
    # C++11 narrowing error. Demote to a warning here so the existing
    # source compiles unchanged. M6 should add explicit static_casts.
    -Wno-c++11-narrowing
    -Wno-narrowing
)
target_compile_definitions(roc-optiq-view PUBLIC IMGUI_DEFINE_MATH_OPERATORS COMPUTE_UI_SUPPORT)

add_executable(roc-optiq-wasm
    wasm_main.cpp
)

target_compile_definitions(roc-optiq-wasm PRIVATE
    IMGUI_DEFINE_MATH_OPERATORS
    IMGUI_IMPL_OPENGL_ES3
    IMGUI_DISABLE_FILE_FUNCTIONS
)

target_include_directories(roc-optiq-wasm PRIVATE
    "${REPO_ROOT}/src/core/inc"
)

target_link_libraries(roc-optiq-wasm PRIVATE
    roc-optiq-view
    implot
    ImGuiFileDialog
    imgui
    roc-optiq-controller
    datamodel
    roc-optiq-core
    spdlog
    sqlite3
    json
)

# Emscripten port flags (compile + link). The contrib.glfw3 port has better
# DPI/canvas-resize support than -sUSE_GLFW=3.
set(EMS_PORT_FLAGS "--use-port=contrib.glfw3")

target_compile_options(roc-optiq-wasm PRIVATE
    ${EMS_PORT_FLAGS}
)

# Apply the GLFW port flag to imgui too (it includes GLFW/glfw3.h).
target_compile_options(imgui PRIVATE ${EMS_PORT_FLAGS})

target_link_options(roc-optiq-wasm PRIVATE
    ${EMS_PORT_FLAGS}
    "SHELL:-s WASM=1"
    "SHELL:-s FULL_ES3=1"
    "SHELL:-s MIN_WEBGL_VERSION=2"
    "SHELL:-s MAX_WEBGL_VERSION=2"
    "SHELL:-s ALLOW_MEMORY_GROWTH=1"
    "SHELL:-s INITIAL_MEMORY=64MB"
    "SHELL:-s ASYNCIFY=1"
    "SHELL:-s NO_EXIT_RUNTIME=0"
    "SHELL:-s ASSERTIONS=1"
    "SHELL:-s ENVIRONMENT=web"
    "SHELL:-s EXPORTED_FUNCTIONS=['_main','_rocprofvis_wasm_open_file']"
    "SHELL:-s EXPORTED_RUNTIME_METHODS=['ccall','cwrap','FS']"
    "--shell-file=${CMAKE_CURRENT_SOURCE_DIR}/shell.html"
)

# Produce index.html / index.js / index.wasm in the build dir.
set(CMAKE_EXECUTABLE_SUFFIX ".html")
set_target_properties(roc-optiq-wasm PROPERTIES OUTPUT_NAME "index")
