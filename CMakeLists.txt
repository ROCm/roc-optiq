# Copyright Advanced Micro Devices, Inc.
# SPDX-License-Identifier: MIT

set(TARGET_NAME roc-optiq)

cmake_minimum_required(VERSION 3.15)
project(${TARGET_NAME})

# Define the version numbers
set(PROJECT_VERSION_MAJOR 1)
set(PROJECT_VERSION_MINOR 0)
set(PROJECT_VERSION_PATCH 0)
set(PROJECT_VERSION_BUILD 2)

# Name and path of the icon font header
set(ICON_HEADER ${CMAKE_SOURCE_DIR}/src/view/src/icons/rocprofvis_icon_data.h)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DVK_PROTOTYPES")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DVK_PROTOTYPES")
set(BUILD_SHARED_LIBS OFF)

option(ROCPROFVIS_DEVELOPER_MODE "Enable developer mode features" OFF)
option(ROCPROFVIS_ENABLE_INTERNAL_BANNER "Show diagonal internal build banner" OFF)
option(USE_NATIVE_FILE_DIALOG "Use the OS native file dialog instead of ImGui file dialog" ON)
option(GLFW_LINK_STATIC "Link GLFW statically instead of dynamically" ON)
option(BUILD_TESTING "Build the testing tree" ON)
option(ROCPROFVIS_ENABLE_UI_TESTS "Build view-layer Dear ImGui UI tests" OFF)

# TEMPORARY: gate the in-development profiler-launch and remote/SSH features
# until they ship. Both default OFF so release builds exclude the UI, the
# dependent sources, and (for remote) the libssh2/mbedtls stack and its
# submodules. Remove these options and unwrap their guards when the features
# graduate.
option(ROCPROFVIS_ENABLE_PROFILER "Build with in-app profiler launch support (in development)" OFF)
option(ROCPROFVIS_ENABLE_REMOTE "Build with remote/SSH support (in development)" OFF)
option(ROCPROFVIS_ENABLE_AGENTIC_PROFILING "Build with the Ask Optiq assistant (in development)" OFF)
option(ROCPROFVIS_ENABLE_SCRIPTING "Build with embedded Python analysis scripts (in development)" OFF)

# The two flags are independent: profiler launch builds without remote (the
# SSH connection UI inside the launcher is compiled out via
# ROCPROFVIS_ENABLE_REMOTE guards), and remote can be enabled on its own. The
# remote-profiling glue (RemoteProfilerSession / SshProfilerExecutor) is only
# built when BOTH are enabled.

# TEMPORARY: gate the in-development trace-compare and multi-window features
# until they ship. Both default OFF so release builds exclude their UI and
# dependent sources. Remove these options and unwrap their guards when the
# features graduate.
option(ROCPROFVIS_ENABLE_TRACE_COMPARE "Build with trace comparison support (in development)" OFF)
option(ROCPROFVIS_MULTI_WINDOW "Build with multi-window support (in development)" OFF)

if(ROCPROFVIS_DEVELOPER_MODE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DROCPROFVIS_DEVELOPER_MODE")
endif(ROCPROFVIS_DEVELOPER_MODE)
if(ROCPROFVIS_ENABLE_PROFILER)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DROCPROFVIS_ENABLE_PROFILER")
endif(ROCPROFVIS_ENABLE_PROFILER)
if(ROCPROFVIS_ENABLE_REMOTE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DROCPROFVIS_ENABLE_REMOTE")
endif(ROCPROFVIS_ENABLE_REMOTE)
if(ROCPROFVIS_ENABLE_AGENTIC_PROFILING)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DROCPROFVIS_ENABLE_AGENTIC_PROFILING")
endif(ROCPROFVIS_ENABLE_AGENTIC_PROFILING)
if(ROCPROFVIS_ENABLE_SCRIPTING)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DROCPROFVIS_ENABLE_SCRIPTING")
endif(ROCPROFVIS_ENABLE_SCRIPTING)
if(ROCPROFVIS_ENABLE_INTERNAL_BANNER)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DROCPROFVIS_ENABLE_INTERNAL_BANNER")
endif(ROCPROFVIS_ENABLE_INTERNAL_BANNER)
if(ROCPROFVIS_ENABLE_TRACE_COMPARE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DROCPROFVIS_ENABLE_TRACE_COMPARE")
endif(ROCPROFVIS_ENABLE_TRACE_COMPARE)
if(ROCPROFVIS_MULTI_WINDOW)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DROCPROFVIS_MULTI_WINDOW")
endif(ROCPROFVIS_MULTI_WINDOW)

# Set GLFW library type based on option
if(GLFW_LINK_STATIC)
    set(GLFW_LIBRARY_TYPE "STATIC" CACHE STRING "GLFW library type" FORCE)
else()
    set(GLFW_LIBRARY_TYPE "SHARED" CACHE STRING "GLFW library type" FORCE)
endif()

# Enable testing if BUILD_TESTING is ON
if(BUILD_TESTING)
    enable_testing()
    include(CTest)
endif()

# Add third-party libraries, core, and model BEFORE controller
add_subdirectory(thirdparty)
add_subdirectory(thirdparty/perfetto/trace_processor/build)
add_subdirectory(thirdparty/perfetto/trace_processor)
if(PERFETTO_AVAILABLE)
    add_compile_definitions(ROCPROFVIS_PERFETTO_ENABLED)
endif()
add_subdirectory(src/core)
add_subdirectory(src/model)
if(ROCPROFVIS_ENABLE_SCRIPTING)
    add_subdirectory(src/python)
endif()
add_subdirectory(src/controller)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/model/inc)

find_package(Vulkan REQUIRED)

# View related src files
set(VIEW_FILES
    src/view/src/rocprofvis_appwindow.cpp
    src/view/src/rocprofvis_appwindow.h
    src/view/src/rocprofvis_compare_files_dialog.cpp
    src/view/src/rocprofvis_compare_files_dialog.h
    src/view/src/welcome/rocprofvis_welcome_page.cpp
    src/view/src/welcome/rocprofvis_welcome_page.h
    src/view/src/rocprofvis_annotation_view.cpp
    src/view/src/rocprofvis_track_item.cpp 
    src/view/src/rocprofvis_flame_track_item.cpp 
    src/view/src/rocprofvis_timeline_arrow.cpp 
    src/view/src/rocprofvis_events_view.cpp 
    src/view/src/rocprofvis_settings_manager.cpp
    src/view/src/rocprofvis_font_manager.cpp 
    src/view/src/rocprofvis_annotations.cpp
    src/view/src/rocprofvis_settings_panel.cpp
    src/view/src/rocprofvis_hotkey_manager.cpp 
    src/view/src/rocprofvis_stickynote.cpp
    src/view/src/rocprofvis_click_manager.cpp
    src/view/src/rocprofvis_measurement_controller.cpp
    src/view/src/rocprofvis_analysis_view.cpp
    src/view/src/rocprofvis_trace_view.cpp
    src/view/src/rocprofvis_top_events_view.cpp
    src/view/src/rocprofvis_line_track_item.cpp 
    src/view/src/rocprofvis_timeline_view.cpp 
    src/view/src/rocprofvis_sidebar.cpp 
    src/view/src/rocprofvis_view_module.cpp
    src/view/src/rocprofvis_data_provider.cpp
    src/view/src/rocprofvis_root_view.cpp
    src/view/src/model/rocprofvis_topology_model.cpp
    src/view/src/model/rocprofvis_timeline_model.cpp
    src/view/src/model/rocprofvis_tables_model.cpp
    src/view/src/model/rocprofvis_summary_model.cpp
    src/view/src/model/rocprofvis_event_model.cpp
    src/view/src/model/rocprofvis_analysis_model.cpp
    src/view/src/model/rocprofvis_trace_data_model.cpp
    src/view/src/rocprofvis_raw_track_data.cpp
    src/view/src/rocprofvis_events.cpp
    src/view/src/rocprofvis_event_manager.cpp
    src/view/src/rocprofvis_utils.cpp
    src/view/src/rocprofvis_timeline_selection.cpp
    src/view/src/rocprofvis_timeline_track_options.cpp
    src/view/src/rocprofvis_track_topology.cpp
    src/view/src/rocprofvis_track_details.cpp
    src/view/src/rocprofvis_project.cpp
    src/view/src/rocprofvis_multi_track_table.cpp
    src/view/src/rocprofvis_event_search.cpp
    src/view/src/rocprofvis_summary_view.cpp
    src/view/src/rocprofvis_time_to_pixel.cpp
    src/view/src/rocprofvis_minimap.cpp
    src/view/src/rocprofvis_presets.cpp
    src/view/src/rocprofvis_json_utils.cpp
    src/view/src/rocprofvis_json_utils.h
    src/view/src/rocprofvis_profiles_document.cpp
    src/view/src/rocprofvis_profiles_document.h
    src/view/src/rocprofvis_appmonitor.cpp
    src/view/src/rocprofvis_appmonitor.h
    src/view/src/widgets/rocprofvis_widget.cpp
    src/view/src/widgets/rocprofvis_split_containers.cpp
    src/view/src/widgets/rocprofvis_flex_container.cpp
    src/view/src/widgets/rocprofvis_tab_container.cpp
    src/view/src/widgets/rocprofvis_editable_textfield.cpp
    src/view/src/widgets/rocprofvis_debug_window.cpp
    src/view/src/widgets/rocprofvis_log_viewer.cpp
    src/view/src/widgets/rocprofvis_gui_helpers.cpp
    src/view/src/widgets/rocprofvis_image_helpers.cpp
    src/view/src/widgets/rocprofvis_infinite_scroll_table.cpp
    src/view/src/widgets/rocprofvis_dialog.cpp
    src/view/src/widgets/rocprofvis_notification_manager.cpp
    src/view/src/compute/rocprofvis_compute_view.cpp
    src/view/src/compute/rocprofvis_compute_table_view.cpp
    src/view/src/compute/rocprofvis_compute_kernel_details.cpp
    src/view/src/compute/rocprofvis_compute_memory_chart.cpp
    src/view/src/compute/rocprofvis_compute_selection.cpp
    src/view/src/compute/rocprofvis_compute_summary.cpp
    src/view/src/compute/rocprofvis_compute_roofline.cpp
    src/view/src/compute/rocprofvis_compute_workload_view.cpp
    src/view/src/compute/rocprofvis_compute_kernel_metric_table.cpp
    src/view/src/compute/rocprofvis_compute_comparison.cpp
    src/view/src/compute/rocprofvis_compute_isa_view.cpp
    src/view/src/compute/rocprofvis_compute_tester.cpp
    src/view/src/model/compute/rocprofvis_compute_data_model.cpp
    src/view/src/widgets/rocprofvis_compute_widget.cpp
    src/view/src/widgets/rocprofvis_query_builder.cpp
)

# SecretStore is shared: SSH passwords need it, and so does the assistant's API
# token, so it is compiled when either feature is on and left out otherwise.
if(ROCPROFVIS_ENABLE_REMOTE OR ROCPROFVIS_ENABLE_AGENTIC_PROFILING)
list(APPEND VIEW_FILES
    src/view/src/remote/rocprofvis_secret_store.cpp
    src/view/src/remote/rocprofvis_secret_store.h
)
endif()


# TEMPORARY (profiler launch): profiler UI + backends + app monitor. Remove
# this guard when the profiler feature graduates.
if(ROCPROFVIS_ENABLE_PROFILER)
list(APPEND VIEW_FILES
    src/view/src/profiler/rocprofvis_profiler_launcher_dialog.cpp
    src/view/src/profiler/rocprofvis_profiler_launcher_dialog.h
    src/view/src/profiler/rocprofvis_profiler_session_base.cpp
    src/view/src/profiler/rocprofvis_profiler_session_base.h
    src/view/src/profiler/rocprofvis_profiler_session.cpp
    src/view/src/profiler/rocprofvis_profiler_session.h
    src/view/src/profiler/rocprofvis_profiler_launch_orchestrator.cpp
    src/view/src/profiler/rocprofvis_profiler_launch_orchestrator.h
    src/view/src/profiler/rocprofvis_launch_config.cpp
    src/view/src/profiler/rocprofvis_launch_config.h
    src/view/src/profiler/rocprofvis_profiler_backend.h
    src/view/src/profiler/rocprofvis_rocprof_sys_backend.cpp
    src/view/src/profiler/rocprofvis_rocprof_sys_backend.h
    src/view/src/profiler/rocprofvis_launch_preset_manager.cpp
    src/view/src/profiler/rocprofvis_launch_preset_manager.h
    src/view/src/profiler/rocprofvis_launch_shared_tabs.cpp
    src/view/src/profiler/rocprofvis_launch_shared_tabs.h
)
endif(ROCPROFVIS_ENABLE_PROFILER)

# TEMPORARY (remote/SSH): remote connection UI, fetch, and trace orchestration.
# Remove this guard when the remote feature graduates.
if(ROCPROFVIS_ENABLE_REMOTE)
list(APPEND VIEW_FILES
    src/view/src/remote/rocprofvis_ssh_uri.cpp
    src/view/src/remote/rocprofvis_ssh_connection_config.cpp
    src/view/src/remote/rocprofvis_ssh_connection_config.h
    src/view/src/remote/rocprofvis_ssh_connection_store.cpp
    src/view/src/remote/rocprofvis_ssh_connection_store.h
    src/view/src/remote/rocprofvis_ssh_fetch.cpp
    src/view/src/remote/rocprofvis_ssh_session.cpp
    src/view/src/remote/rocprofvis_ssh_auth_modal.cpp
    src/view/src/remote/rocprofvis_remote_trace_orchestrator.cpp
    src/view/src/remote/rocprofvis_remote_trace_orchestrator.h
    src/view/src/remote/rocprofvis_ssh_settings_dialog.cpp
    src/view/src/remote/rocprofvis_ssh_settings_dialog.h
    src/view/src/remote/rocprofvis_ssh_test_dialog.cpp
    src/view/src/remote/rocprofvis_ssh_test_dialog.h
    src/view/src/remote/rocprofvis_remote_file_browser.cpp
    src/view/src/remote/rocprofvis_remote_file_browser.h
)
endif(ROCPROFVIS_ENABLE_REMOTE)

# TEMPORARY (agentic profiling): the Ask Optiq assistant. Remove this guard
# when the feature graduates.
if(ROCPROFVIS_ENABLE_AGENTIC_PROFILING)
list(APPEND VIEW_FILES
    src/view/src/agenticprofiling/rocprofvis_ai_actions.cpp
    src/view/src/agenticprofiling/rocprofvis_ai_actions.h
    src/view/src/agenticprofiling/rocprofvis_ai_assistant.cpp
    src/view/src/agenticprofiling/rocprofvis_ai_assistant.h
    src/view/src/agenticprofiling/rocprofvis_ai_assistant_render.cpp
    src/view/src/agenticprofiling/rocprofvis_ai_client.cpp
    src/view/src/agenticprofiling/rocprofvis_ai_client.h
    src/view/src/agenticprofiling/rocprofvis_ai_data_tools.cpp
    src/view/src/agenticprofiling/rocprofvis_ai_prompts.cpp
    src/view/src/agenticprofiling/rocprofvis_ai_prompts.h
    src/view/src/agenticprofiling/rocprofvis_ai_script_tools.cpp
    src/view/src/agenticprofiling/rocprofvis_ai_settings.cpp
    src/view/src/agenticprofiling/rocprofvis_ai_tool_query.cpp
    src/view/src/agenticprofiling/rocprofvis_ai_tool_query.h
    src/view/src/agenticprofiling/rocprofvis_ai_tool_schema.cpp
    src/view/src/agenticprofiling/rocprofvis_ai_tool_schema.h
    src/view/src/agenticprofiling/rocprofvis_ai_tools.cpp
    src/view/src/agenticprofiling/rocprofvis_ai_tools.h
    src/view/src/agenticprofiling/rocprofvis_ai_tools_internal.h
    src/view/src/agenticprofiling/rocprofvis_ai_trace_view_actions.cpp
    src/view/src/agenticprofiling/rocprofvis_ai_ui_tools.cpp
)
endif(ROCPROFVIS_ENABLE_AGENTIC_PROFILING)

# TEMPORARY (remote profiling): the remote profiler session bridges the
# profiler and remote features, so it needs both flags.
if(ROCPROFVIS_ENABLE_PROFILER AND ROCPROFVIS_ENABLE_REMOTE)
list(APPEND VIEW_FILES
    src/view/src/profiler/rocprofvis_remote_profiler_session.cpp
    src/view/src/profiler/rocprofvis_remote_profiler_session.h
)
endif()

if(ROCPROFVIS_ENABLE_SCRIPTING)
list(APPEND VIEW_FILES
    src/view/src/rocprofvis_data_provider_script.cpp
    src/view/src/widgets/rocprofvis_script_editor.cpp
    src/view/src/widgets/rocprofvis_script_editor.h
)
endif()

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/resources/version.h.in"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/app/inc/rocprofvis_version.h"
    @ONLY
)

if(WIN32)
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/rocprofvis_app.rc.in"
        "${CMAKE_CURRENT_BINARY_DIR}/rocprofvis_res.rc"
        @ONLY
    )
    list(APPEND VIEW_FILES
        "${CMAKE_CURRENT_BINARY_DIR}/rocprofvis_res.rc"
        )
endif(WIN32)

# Shared app sources (no main): the prod exe adds main.cpp, the test exe adds test_main.cpp.
set(APP_FILES
    src/app/src/rocprofvis_imgui_backend.cpp
    src/app/src/rocprofvis_imgui_vulkan.cpp
    src/app/src/rocprofvis_imgui_opengl.cpp
    src/app/src/glfw_util.cpp
    src/app/src/rocprofvis_cli_parser.cpp
    src/app/src/rocprofvis_platform_helpers.cpp)
if(APPLE)
    list(APPEND APP_FILES src/app/src/rocprofvis_platform_helpers_macos.mm)
endif()

if(APPLE)
    add_executable(${TARGET_NAME} MACOSX_BUNDLE ${APP_FILES} src/app/src/main.cpp ${VIEW_FILES})
else()
    add_executable(${TARGET_NAME} ${APP_FILES} src/app/src/main.cpp ${VIEW_FILES})
endif()

add_dependencies(${TARGET_NAME} generate_icon_header)

if(WIN32)
    set_target_properties(${TARGET_NAME} PROPERTIES WIN32_EXECUTABLE TRUE)
    set_target_properties(${TARGET_NAME} PROPERTIES LINK_FLAGS "/ENTRY:mainCRTStartup")
endif(WIN32)

if(APPLE)
    set(MACOSX_BUNDLE_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/resources/mac/Info.plist.in")
    configure_file("${MACOSX_BUNDLE_PLIST}" "${CMAKE_CURRENT_BINARY_DIR}/Info.plist" @ONLY)
    set_target_properties(${TARGET_NAME} PROPERTIES
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_BINARY_DIR}/Info.plist"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.amd.roc-optiq"
        MACOSX_BUNDLE_BUNDLE_NAME "ROCm Optiq"
        MACOSX_BUNDLE_ICON_FILE "roc-optiq.icns"
        MACOSX_BUNDLE_SHORT_VERSION_STRING
            "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION_BUILD}"
    )
    
    # Set icon file location in bundle
    set_source_files_properties(
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/mac/roc-optiq.icns"
        PROPERTIES MACOSX_PACKAGE_LOCATION "Resources"
    )
    
    # Add icon to target sources
    target_sources(${TARGET_NAME} PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/mac/roc-optiq.icns"
    )
endif()

target_include_directories(${TARGET_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/app/inc)
target_include_directories(${TARGET_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/resources)
target_include_directories(${TARGET_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/controller/inc)
target_include_directories(${TARGET_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/view/inc)
target_include_directories(${TARGET_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/view/src)
target_include_directories(${TARGET_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/view/src/model)
target_include_directories(${TARGET_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/view/src/icons)
target_include_directories(${TARGET_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/view/src/widgets)
# TEMPORARY (profiler launch): remove when the profiler feature graduates.
if(ROCPROFVIS_ENABLE_PROFILER)
target_include_directories(${TARGET_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/view/src/profiler)
endif(ROCPROFVIS_ENABLE_PROFILER)
target_include_directories(${TARGET_NAME} PRIVATE ${Vulkan_INCLUDE_DIRS})
target_include_directories(${TARGET_NAME} PRIVATE ${Vulkan_INCLUDE_DIR})
target_include_directories(${TARGET_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/model/inc)
target_include_directories(${TARGET_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/core/inc)
target_link_libraries(${TARGET_NAME} PRIVATE imgui)
target_link_libraries(${TARGET_NAME} PRIVATE implot)
target_link_libraries(${TARGET_NAME} PRIVATE glfw)
target_link_libraries(${TARGET_NAME} PRIVATE ${TARGET_NAME}-controller)
target_link_libraries(${TARGET_NAME} PRIVATE datamodel)
target_link_libraries(${TARGET_NAME} PRIVATE ${TARGET_NAME}-core)
target_link_libraries(${TARGET_NAME} PRIVATE spdlog)
target_link_libraries(${TARGET_NAME} PRIVATE sqlite3)
target_link_libraries(${TARGET_NAME} PRIVATE json)
if(ROCPROFVIS_ENABLE_AGENTIC_PROFILING)
    target_link_libraries(${TARGET_NAME} PRIVATE cpp-httplib)
    # httplib.h is one 21k-line header, which overruns the default section limit.
    if(MSVC)
        set_source_files_properties(
            src/view/src/agenticprofiling/rocprofvis_ai_client.cpp PROPERTIES
            COMPILE_OPTIONS "/bigobj"
        )
    endif()
endif(ROCPROFVIS_ENABLE_AGENTIC_PROFILING)

if(BUILD_TESTING AND ROCPROFVIS_ENABLE_UI_TESTS)
    # Fail at configure if imgui_te was dropped (e.g. BUILD_TESTING shadowed).
    if(NOT TARGET imgui_te)
        message(FATAL_ERROR
            "ROCPROFVIS_ENABLE_UI_TESTS is ON but imgui_te was not created. "
            "Check that BUILD_TESTING is still set in thirdparty/CMakeLists.txt.")
    endif()

    set(IMGUI_TEST_ENGINE_DIR
        ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/imgui_test_engine/imgui_test_engine)
    add_library(imgui_test_engine STATIC
        ${IMGUI_TEST_ENGINE_DIR}/imgui_capture_tool.cpp
        ${IMGUI_TEST_ENGINE_DIR}/imgui_te_context.cpp
        ${IMGUI_TEST_ENGINE_DIR}/imgui_te_coroutine.cpp
        ${IMGUI_TEST_ENGINE_DIR}/imgui_te_engine.cpp
        ${IMGUI_TEST_ENGINE_DIR}/imgui_te_exporters.cpp
        ${IMGUI_TEST_ENGINE_DIR}/imgui_te_perftool.cpp
        ${IMGUI_TEST_ENGINE_DIR}/imgui_te_ui.cpp
        ${IMGUI_TEST_ENGINE_DIR}/imgui_te_utils.cpp
    )
    target_compile_definitions(imgui_test_engine PUBLIC
        IMGUI_ENABLE_TEST_ENGINE
        IMGUI_TEST_ENGINE_ENABLE_COROUTINE_STDTHREAD_IMPL=1
    )
    target_include_directories(imgui_test_engine PUBLIC
        ${IMGUI_TEST_ENGINE_DIR}
        ${IMGUI_TEST_ENGINE_DIR}/thirdparty/Str
    )
    # Engine links the WITH-define imgui variant so its hook bodies match the callsites.
    target_link_libraries(imgui_test_engine PUBLIC imgui_te)

    # Separate test executable: same app+view sources, compiled WITH the define, linked
    # against imgui_te (not the clean imgui). Uses test_main.cpp (the engine-wired copy of
    # main.cpp) so production main.cpp stays pristine and free of any test references.
    set(TEST_TARGET ${TARGET_NAME}-tests)
    add_executable(${TEST_TARGET} ${APP_FILES} src/app/test/test_main.cpp ${VIEW_FILES} src/app/test/app_tests.cpp)
    add_dependencies(${TEST_TARGET} generate_icon_header)
    target_compile_definitions(${TEST_TARGET} PRIVATE IMGUI_ENABLE_TEST_ENGINE)
    target_include_directories(${TEST_TARGET} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/app/inc
        ${CMAKE_CURRENT_SOURCE_DIR}/src/app/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/app/test
        ${CMAKE_CURRENT_SOURCE_DIR}/resources
        ${CMAKE_CURRENT_SOURCE_DIR}/src/controller/inc
        ${CMAKE_CURRENT_SOURCE_DIR}/src/view/inc
        ${CMAKE_CURRENT_SOURCE_DIR}/src/view/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/view/src/model
        ${CMAKE_CURRENT_SOURCE_DIR}/src/view/src/icons
        ${CMAKE_CURRENT_SOURCE_DIR}/src/view/src/widgets
        ${CMAKE_CURRENT_SOURCE_DIR}/src/view/test
        ${Vulkan_INCLUDE_DIRS}
        ${Vulkan_INCLUDE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/src/model/inc
        ${CMAKE_CURRENT_SOURCE_DIR}/src/core/inc
    )
    # TEMPORARY (profiler launch): remove when the profiler feature graduates.
    if(ROCPROFVIS_ENABLE_PROFILER)
    target_include_directories(${TEST_TARGET} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/view/src/profiler)
    endif(ROCPROFVIS_ENABLE_PROFILER)
    target_link_libraries(${TEST_TARGET} PRIVATE
        imgui_te
        imgui_test_engine
        implot
        glfw
        ${TARGET_NAME}-controller
        datamodel
        ${TARGET_NAME}-core
        spdlog
        sqlite3
        json
        ImGuiFileDialog
        "${Vulkan_LIBRARIES}"
        OpenGL::GL
    )
    if(ROCPROFVIS_ENABLE_AGENTIC_PROFILING)
        target_link_libraries(${TEST_TARGET} PRIVATE cpp-httplib)
    endif(ROCPROFVIS_ENABLE_AGENTIC_PROFILING)
    if(USE_NATIVE_FILE_DIALOG)
        target_link_libraries(${TEST_TARGET} PRIVATE nfd)
        target_compile_definitions(${TEST_TARGET} PRIVATE ROCPROFVIS_HAVE_NATIVE_FILE_DIALOG)
    endif()
    if(APPLE)
        target_link_libraries(${TEST_TARGET} PRIVATE "-framework AppKit")
    endif()
    add_test(NAME roc-optiq-ui-tests-trace COMMAND roc-optiq-tests --run-tests -f "${CMAKE_SOURCE_DIR}/sample/rocpd-transpose.db")
    set_tests_properties(roc-optiq-ui-tests-trace PROPERTIES RUN_SERIAL ON)
    set_tests_properties(roc-optiq-ui-tests-trace PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    add_test(NAME roc-optiq-ui-tests-compute COMMAND roc-optiq-tests --run-tests -f "${CMAKE_SOURCE_DIR}/sample/rocprof_compute_23ed6f36.db")
    set_tests_properties(roc-optiq-ui-tests-compute PROPERTIES RUN_SERIAL ON)
    set_tests_properties(roc-optiq-ui-tests-compute PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
endif()

# TEMPORARY (remote/SSH): only link the SSH/crypto stack into the executable
# when remote is built. Remove this guard when the remote feature graduates.
if(ROCPROFVIS_ENABLE_REMOTE)
if (CRYPTO_BACKEND STREQUAL "mbedTLS")
target_link_libraries(${TARGET_NAME} PRIVATE libssh2_static)
endif()

if (CRYPTO_BACKEND STREQUAL "OpenSSL")
target_link_libraries(${TARGET_NAME} PRIVATE libssh2_static OpenSSL::SSL OpenSSL::Crypto)
endif()
endif(ROCPROFVIS_ENABLE_REMOTE)

# SecretStore backends: the OS credential-vault APIs, linked only when something
# actually stores a secret - SSH passwords under remote, the API token under
# agentic profiling. On Linux libsecret is optional; when absent, SecretStore
# compiles a stub that reports itself unavailable and secrets stay in process
# memory only.
if(ROCPROFVIS_ENABLE_REMOTE OR ROCPROFVIS_ENABLE_AGENTIC_PROFILING)
set(ROCPROFVIS_SECRET_STORE_TARGETS ${TARGET_NAME})
if(TARGET ${TARGET_NAME}-tests)
    list(APPEND ROCPROFVIS_SECRET_STORE_TARGETS ${TARGET_NAME}-tests)
endif()
if(WIN32)
    foreach(secret_target ${ROCPROFVIS_SECRET_STORE_TARGETS})
        target_link_libraries(${secret_target} PRIVATE Advapi32)
    endforeach()
elseif(APPLE)
    foreach(secret_target ${ROCPROFVIS_SECRET_STORE_TARGETS})
        target_link_libraries(${secret_target} PRIVATE "-framework Security" "-framework CoreFoundation")
    endforeach()
else()
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(LIBSECRET QUIET libsecret-1)
    endif()
    if(LIBSECRET_FOUND)
        foreach(secret_target ${ROCPROFVIS_SECRET_STORE_TARGETS})
            target_include_directories(${secret_target} PRIVATE ${LIBSECRET_INCLUDE_DIRS})
            target_link_libraries(${secret_target} PRIVATE ${LIBSECRET_LIBRARIES})
            target_compile_definitions(${secret_target} PRIVATE ROCPROFVIS_HAVE_LIBSECRET)
        endforeach()
    else()
        message(WARNING
            "libsecret-1 not found: assistant tokens and SSH passwords will not "
            "be persisted on this Linux build. Install libsecret-1-dev / "
            "libsecret-devel to enable secure credential storage.")
    endif()
endif()
endif()

if(APPLE)
    # rocprofvis_platform_helpers_macos.mm uses AppKit (+[NSEvent modifierFlags]).
    target_link_libraries(${TARGET_NAME} PRIVATE "-framework AppKit")
endif()

if(APPLE)
    set(ROCPROFVIS_BUNDLE_VULKAN ON CACHE BOOL
        "Bundle the Vulkan loader into the macOS app. MoltenVK is resolved \
from the user's LunarG Vulkan SDK install at runtime; the entitlements \
plist allows dlopen of the LunarG-signed dylib under Hardened Runtime.")
    if(ROCPROFVIS_BUNDLE_VULKAN)
        set(ROCPROFVIS_VULKAN_LOADER_LIBRARY "")
        if(Vulkan_LIBRARY)
            set(ROCPROFVIS_VULKAN_LOADER_LIBRARY "${Vulkan_LIBRARY}")
        elseif(Vulkan_LIBRARIES)
            list(GET Vulkan_LIBRARIES 0 ROCPROFVIS_VULKAN_LOADER_LIBRARY)
        endif()
        if(ROCPROFVIS_VULKAN_LOADER_LIBRARY)
            get_filename_component(ROCPROFVIS_VULKAN_LOADER_DIR
                "${ROCPROFVIS_VULKAN_LOADER_LIBRARY}" DIRECTORY)
            if(EXISTS "${ROCPROFVIS_VULKAN_LOADER_DIR}/libvulkan.1.dylib")
                set(ROCPROFVIS_VULKAN_LOADER_LIBRARY
                    "${ROCPROFVIS_VULKAN_LOADER_DIR}/libvulkan.1.dylib")
            endif()
        endif()
        if(NOT ROCPROFVIS_VULKAN_LOADER_LIBRARY OR
           NOT EXISTS "${ROCPROFVIS_VULKAN_LOADER_LIBRARY}")
            message(FATAL_ERROR
                "ROCPROFVIS_BUNDLE_VULKAN is ON, but the Vulkan loader library "
                "was not found. Set Vulkan_LIBRARY to the SDK libvulkan.dylib.")
        endif()

        set(ROCPROFVIS_MOLTENVK_LIBRARY "" CACHE FILEPATH
            "Path to libMoltenVK.dylib to bundle into the macOS app. \
Auto-detected from the Vulkan SDK / Homebrew when empty.")
        if(NOT ROCPROFVIS_MOLTENVK_LIBRARY)
            foreach(_mvk_dir
                    "${ROCPROFVIS_VULKAN_LOADER_DIR}"
                    "$ENV{VULKAN_SDK}/lib"
                    "/opt/homebrew/lib"
                    "/usr/local/lib")
                if(_mvk_dir AND EXISTS "${_mvk_dir}/libMoltenVK.dylib")
                    set(ROCPROFVIS_MOLTENVK_LIBRARY "${_mvk_dir}/libMoltenVK.dylib")
                    break()
                endif()
            endforeach()
        endif()
        if(ROCPROFVIS_MOLTENVK_LIBRARY AND EXISTS "${ROCPROFVIS_MOLTENVK_LIBRARY}")
            message(STATUS "Bundling MoltenVK from ${ROCPROFVIS_MOLTENVK_LIBRARY}")
        else()
            message(WARNING
                "MoltenVK (libMoltenVK.dylib) was not found, so the bundled app "
                "will rely on a system Vulkan driver at runtime (LunarG SDK or "
                "'brew install molten-vk') or fall back to OpenGL. Set "
                "ROCPROFVIS_MOLTENVK_LIBRARY to bundle the driver.")
        endif()

    endif()

    set_target_properties(${TARGET_NAME} PROPERTIES
        BUILD_RPATH "@executable_path/../Frameworks;/usr/local/lib;/opt/homebrew/lib"
        INSTALL_RPATH "@executable_path/../Frameworks;/usr/local/lib;/opt/homebrew/lib"
    )
    # When static, glfw is libglfw3.a and is already linked into the binary,
    # so don't stage it. The stray .a breaks codesign --deep --strict.
    if(NOT GLFW_LINK_STATIC)
        add_custom_command(
            TARGET ${TARGET_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E create_symlink
                "$<TARGET_FILE_NAME:glfw>"
                "$<TARGET_FILE_DIR:glfw>/libglfw.3.dylib"
            COMMENT "Creating GLFW soname symlink for macOS runtime lookup"
        )
        add_custom_command(
            TARGET ${TARGET_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_BUNDLE_DIR:${TARGET_NAME}>/Contents/Frameworks"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:glfw>"
                "$<TARGET_BUNDLE_DIR:${TARGET_NAME}>/Contents/Frameworks"
            COMMAND ${CMAKE_COMMAND} -E create_symlink
                "$<TARGET_FILE_NAME:glfw>"
                "$<TARGET_BUNDLE_DIR:${TARGET_NAME}>/Contents/Frameworks/libglfw.3.dylib"
            COMMENT "Staging GLFW dylib into the app bundle"
        )
    endif()
    if(ROCPROFVIS_BUNDLE_VULKAN)
        add_custom_command(
            TARGET ${TARGET_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_BUNDLE_DIR:${TARGET_NAME}>/Contents/Frameworks"
                "$<TARGET_BUNDLE_DIR:${TARGET_NAME}>/Contents/Resources/Licenses"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${ROCPROFVIS_VULKAN_LOADER_LIBRARY}"
                "$<TARGET_BUNDLE_DIR:${TARGET_NAME}>/Contents/Frameworks/libvulkan.1.dylib"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_CURRENT_SOURCE_DIR}/resources/licenses/Vulkan-Loader-LICENSE.txt"
                "$<TARGET_BUNDLE_DIR:${TARGET_NAME}>/Contents/Resources/Licenses/Vulkan-Loader-LICENSE.txt"
            COMMENT "Staging Vulkan loader and license into the app bundle"
        )
        if(ROCPROFVIS_MOLTENVK_LIBRARY AND EXISTS "${ROCPROFVIS_MOLTENVK_LIBRARY}")
            add_custom_command(
                TARGET ${TARGET_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory
                    "$<TARGET_BUNDLE_DIR:${TARGET_NAME}>/Contents/Frameworks"
                    "$<TARGET_BUNDLE_DIR:${TARGET_NAME}>/Contents/Resources/vulkan/icd.d"
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${ROCPROFVIS_MOLTENVK_LIBRARY}"
                    "$<TARGET_BUNDLE_DIR:${TARGET_NAME}>/Contents/Frameworks/libMoltenVK.dylib"
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${CMAKE_CURRENT_SOURCE_DIR}/resources/mac/MoltenVK_icd.json"
                    "$<TARGET_BUNDLE_DIR:${TARGET_NAME}>/Contents/Resources/vulkan/icd.d/MoltenVK_icd.json"
                COMMENT "Staging MoltenVK driver and ICD manifest into the app bundle"
            )
        endif()
    endif()

    # When the OpenSSL backend is opt-in enabled, stage libssl/libcrypto into the
    # bundle's Frameworks so the .app is self-contained instead of relying on a
    # Homebrew install at runtime.
    #
    # IMPORTANT: the executable does not record OpenSSL::SSL's IMPORTED_LOCATION
    # (often the unversioned `libssl.dylib` symlink). It records each dylib's own
    # install name (LC_ID_DYLIB), which Homebrew sets to a VERSIONED, absolute
    # path such as `/opt/homebrew/.../libssl.3.dylib`. Rewriting based on the
    # unversioned IMPORTED_LOCATION therefore no-ops and the build leaks an
    # absolute Homebrew reference, so a clean machine crashes at launch with
    # "Library not loaded: /opt/homebrew/*/libssl.3.dylib".
    #
    # To be robust across OpenSSL layouts we resolve the real on-disk dylibs and
    # the executable's actual dependency paths at build time with otool, then
    # rewrite every OpenSSL reference to `@rpath/<versioned-name>`. The staged
    # files keep their versioned names so the load commands match.
    if(ROCPROFVIS_ENABLE_REMOTE AND CRYPTO_BACKEND STREQUAL "OpenSSL")
        get_target_property(ROCPROFVIS_OPENSSL_SSL_LIB OpenSSL::SSL IMPORTED_LOCATION)
        get_target_property(ROCPROFVIS_OPENSSL_CRYPTO_LIB OpenSSL::Crypto IMPORTED_LOCATION)
        if(NOT ROCPROFVIS_OPENSSL_SSL_LIB OR NOT ROCPROFVIS_OPENSSL_CRYPTO_LIB)
            message(FATAL_ERROR
                "CRYPTO_BACKEND=OpenSSL on macOS requires shared OpenSSL libraries; "
                "find_package(OpenSSL) did not resolve dylib locations. Install "
                "openssl@3 and set OPENSSL_ROOT_DIR.")
        endif()
        set(ROCPROFVIS_OPENSSL_FIXUP_SCRIPT
            "${CMAKE_CURRENT_BINARY_DIR}/rocprofvis_macos_openssl_fixup.sh")
        configure_file(
            "${CMAKE_CURRENT_SOURCE_DIR}/cmake/macos_openssl_fixup.sh.in"
            "${ROCPROFVIS_OPENSSL_FIXUP_SCRIPT}"
            @ONLY)
        add_custom_command(
            TARGET ${TARGET_NAME} POST_BUILD
            COMMAND /bin/bash "${ROCPROFVIS_OPENSSL_FIXUP_SCRIPT}"
                "$<TARGET_BUNDLE_DIR:${TARGET_NAME}>"
                "$<TARGET_FILE:${TARGET_NAME}>"
            COMMENT "Staging OpenSSL libraries into the app bundle"
            VERBATIM
        )
    endif()

    set(ROCPROFVIS_MACOS_CODESIGN_IDENTITY "" CACHE STRING
        "codesign --sign identity. Empty=skip, '-'=ad-hoc, \
'Developer ID Application: ... (TEAMID)' for notarization-ready release.")
    set(ROCPROFVIS_MACOS_ENTITLEMENTS
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/mac/roc-optiq.entitlements.plist"
        CACHE FILEPATH "Entitlements plist used when codesigning the .app")

    if(ROCPROFVIS_MACOS_CODESIGN_IDENTITY)
        # Ad-hoc signing can't use --options runtime / --timestamp / --entitlements.
        if(ROCPROFVIS_MACOS_CODESIGN_IDENTITY STREQUAL "-")
            set(_rocprofvis_codesign_runtime_args "")
        else()
            set(_rocprofvis_codesign_runtime_args
                --options runtime
                --timestamp
                --entitlements "${ROCPROFVIS_MACOS_ENTITLEMENTS}")
        endif()

        add_custom_command(
            TARGET ${TARGET_NAME} POST_BUILD
            COMMAND codesign --force --deep
                --sign "${ROCPROFVIS_MACOS_CODESIGN_IDENTITY}"
                ${_rocprofvis_codesign_runtime_args}
                "$<TARGET_BUNDLE_DIR:${TARGET_NAME}>"
            COMMAND codesign --verify --verbose=2
                "$<TARGET_BUNDLE_DIR:${TARGET_NAME}>"
            COMMENT "Codesigning bundle with identity '${ROCPROFVIS_MACOS_CODESIGN_IDENTITY}'"
            VERBATIM
        )
    endif()
endif()

# === NativeFileDialog Extended ===
# When enabled, the native (portal) dialog is compiled in, but the in-process
# ImGuiFileDialog is ALWAYS linked as well so the app can fall back to it at
# runtime (e.g. when running over ssh -X where xdg-desktop-portal cannot
# forward the dialog to the client).
if(USE_NATIVE_FILE_DIALOG)
  set(NFD_PORTAL ON CACHE BOOL "Use xdg-desktop-portal instead of GTK")
  add_subdirectory(thirdparty/nativefiledialog-extended)
  target_link_libraries(${TARGET_NAME} PRIVATE nfd)
  target_compile_definitions(${TARGET_NAME} PRIVATE ROCPROFVIS_HAVE_NATIVE_FILE_DIALOG)
endif()
# === ImGuiFileDialog (always linked as portable / SSH fallback) ===
target_link_libraries(${TARGET_NAME} PRIVATE ImGuiFileDialog)

# === Vulkan ===
target_link_libraries(${TARGET_NAME} PRIVATE "${Vulkan_LIBRARIES}")

# === OpenGL (fallback when Vulkan fails) ===
find_package(OpenGL REQUIRED)
target_link_libraries(${TARGET_NAME} PRIVATE OpenGL::GL)

# CPACK Packaging Enabling
set(BUILD_DIR ${CMAKE_CURRENT_BINARY_DIR})
if( NOT DEFINED BUILD_ID )
  if( DEFINED ENV{BUILD_ID} )
    set( BUILD_ID $ENV{BUILD_ID} )
  else()
    # Default BUILD_ID for local or development builds when not explicitly set
    set( BUILD_ID "0")
  endif()
endif()

# Package Maintainer Name and Email
set(PKG_MAINTAINER_NM "ROCm Optiq Support")
set(PKG_MAINTAINER_EMAIL "rocm-optiq@amd.com")

set(RUNTIME_COMP_TYPE "RV_RUNTIME")
set(PKG_DESCRIPTION "A visualizer for the ROCm Profiler Tools")
set(EXTENDED_PACKAGE_DESCRIPTION
	"This package provides `roc-optiq` which helps in post analysis of ROCm Systems profiling data.")

include(GNUInstallDirs)

set(CPACK_PACKAGING_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}"  CACHE STRING "Default packaging prefix.")

set(CPACK_GENERATOR "TGZ" CACHE STRING "package types to be produced")
# Detect Linux distro for additional package types
if(UNIX AND NOT APPLE)
  # Add DEB for Debian based distros
  if(EXISTS "/etc/debian_version")
    list(APPEND CPACK_GENERATOR "DEB")
  # Make RPM the default for RedHat based distros  
  elseif(EXISTS "/etc/redhat-release" OR EXISTS "/etc/oracle-release" OR EXISTS "/etc/system-release")
    list(APPEND CPACK_GENERATOR "RPM")
  endif()
endif()

# Default Value is 0, this value will be used
# When ROCM LibPatch Version when not explicitly set
set(ROCM_VERSION_FOR_PACKAGE "0")
if(DEFINED ENV{ROCM_LIBPATCH_VERSION})
  set(ROCM_VERSION_FOR_PACKAGE $ENV{ROCM_LIBPATCH_VERSION})
endif()


# Packaging directives
set(CPACK_PACKAGE_VENDOR "Advanced Micro Devices, Inc.")
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(CPACK_PACKAGE_VERSION
	"${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}.${PROJECT_VERSION_BUILD}")
set(CPACK_PACKAGE_CONTACT "${PKG_MAINTAINER_NM} <${PKG_MAINTAINER_EMAIL}>")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PKG_DESCRIPTION}")

# License Information
set(LICENSE_FILE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE.md" CACHE PATH "Path to License File in source dir" )
set(CPACK_RESOURCE_FILE_LICENSE "${LICENSE_FILE}")
set(CPACK_PACKAGE_LICENSE "LicenseRef-AMD_EULA")
set(CPACK_RPM_PACKAGE_LICENSE "LicenseRef-AMD_EULA")
# License file
install(FILES ${LICENSE_FILE}
       DESTINATION ${CMAKE_INSTALL_DOCDIR} RENAME LICENSE.txt
       COMPONENT ${RUNTIME_COMP_TYPE} )

# Install Instruction for Files included in  Package Component
if(APPLE)
    install(TARGETS ${TARGET_NAME}
            BUNDLE DESTINATION .
            COMPONENT ${RUNTIME_COMP_TYPE})
else()
    install(PROGRAMS ${BUILD_DIR}/${TARGET_NAME}
            DESTINATION ${CMAKE_INSTALL_BINDIR}
            COMPONENT ${RUNTIME_COMP_TYPE} )
endif()

# Debian package specific variables
set(CPACK_COMPONENTS_ALL RV_RUNTIME)
set(CPACK_DEB_COMPONENT_INSTALL ON)
set(CPACK_DEBIAN_RV_RUNTIME_PACKAGE_NAME "${PROJECT_NAME}")
# Set package version to 1 
if(NOT DEFINED CPACK_DEBIAN_PACKAGE_RELEASE)
  set(CPACK_DEBIAN_PACKAGE_RELEASE "1")
endif()
message(STATUS "Using CPACK_DEBIAN_PACKAGE_RELEASE ${CPACK_DEBIAN_PACKAGE_RELEASE}")
set(CPACK_DEBIAN_FILE_NAME "DEB-DEFAULT")
set(DEB_DEPENDS_LIST "libvulkan1")
if(NOT GLFW_LINK_STATIC)
    list(APPEND DEB_DEPENDS_LIST "libglfw3")
endif()
# DEB depends are explicit (unlike RPM AUTOREQPROV), so declare the system
# OpenSSL runtime only when the OpenSSL backend is built. The default mbedTLS
# backend links statically and needs no extra runtime dependency.
if(ROCPROFVIS_ENABLE_REMOTE AND CRYPTO_BACKEND STREQUAL "OpenSSL")
    list(APPEND DEB_DEPENDS_LIST "libssl3")
endif()
list(APPEND DEB_DEPENDS_LIST "libc6")
string(JOIN ", " DEB_DEPENDS_STRING ${DEB_DEPENDS_LIST})
set(CPACK_DEBIAN_PACKAGE_DEPENDS ${DEB_DEPENDS_STRING})
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/ROCm/roc-optiq")
set(CPACK_DEBIAN_RV_RUNTIME_PACKAGE_DEPENDS ${DEB_DEPENDS_STRING})
set(CPACK_DEBIAN_PACKAGE_DESCRIPTION "${EXTENDED_PACKAGE_DESCRIPTION}")

# RPM package specific variables
if(NOT APPLE)
    set(CMAKE_SKIP_RPATH TRUE)
endif()
set(CPACK_RPM_COMPONENT_INSTALL ON)
set(CPACK_RPM_RV_RUNTIME_PACKAGE_NAME "${PROJECT_NAME}")

# Set package version to 1 
if(NOT DEFINED CPACK_RPM_PACKAGE_RELEASE)
  set(CPACK_RPM_PACKAGE_RELEASE "1")
endif()

# 'dist' breaks manual builds on debian systems due to empty Provides
execute_process( COMMAND rpm --eval %{?dist}
                 RESULT_VARIABLE PROC_RESULT
                 OUTPUT_VARIABLE EVAL_RESULT
                 OUTPUT_STRIP_TRAILING_WHITESPACE)
if(PROC_RESULT EQUAL "0" AND NOT EVAL_RESULT STREQUAL "")
  string(APPEND CPACK_RPM_PACKAGE_RELEASE "%{?dist}")
endif()
message(STATUS "Using CPACK_RPM_PACKAGE_RELEASE: ${CPACK_RPM_PACKAGE_RELEASE}")
set(CPACK_RPM_FILE_NAME "RPM-DEFAULT")

# Use automatic shared libraries dependency detection
set(CPACK_RPM_PACKAGE_AUTOREQPROV "yes")
set(CPACK_RPM_PACKAGE_DESCRIPTION "${EXTENDED_PACKAGE_DESCRIPTION}")

if("DEB" IN_LIST CPACK_GENERATOR)
  message(STATUS "Expected DEB Filename: ${CPACK_DEBIAN_RV_RUNTIME_PACKAGE_NAME}_${CPACK_PACKAGE_VERSION}-${CPACK_DEBIAN_PACKAGE_RELEASE}_<ARCH>.deb")
endif()
if("RPM" IN_LIST CPACK_GENERATOR)
  message(STATUS "Expected RPM Filename: ${CPACK_RPM_RV_RUNTIME_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${CPACK_RPM_PACKAGE_RELEASE}.<ARCH>.rpm")
endif()

# Include packaging
include(CPack)

# ── WiX v4 MSI installer (Windows only) ──────────────────────────────────────
# Builds roc-optiq-<version>-win64.msi from wix/roc-optiq.wxs via the WiX v4
# CLI.  Prerequisites (one-time per build machine):
#   dotnet tool install --global wix
#   wix extension add WixToolset.UI.wixext
if(WIN32)
  find_program(WIX_EXECUTABLE wix DOC "WiX v4 CLI (wix.exe)")
  if(WIX_EXECUTABLE)
    set(_wix_src  "${CMAKE_SOURCE_DIR}/wix/roc-optiq.wxs")
    set(_wix_out  "${CMAKE_BINARY_DIR}/roc-optiq-${CPACK_PACKAGE_VERSION}-win64.msi")
    set(_wix_version "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}")
    add_custom_target(PACKAGE_WIX
      COMMAND "${WIX_EXECUTABLE}" build
        -arch x64
        -d "Version=${_wix_version}"
        -d "ExeFile=$<TARGET_FILE:${TARGET_NAME}>"
        -d "LicenseRtf=${CMAKE_SOURCE_DIR}/Installer/Documentation/License/License_Public.rtf"
        -d "BannerBmp=${CMAKE_SOURCE_DIR}/Installer/Installation/Files/banner.bmp"
        -d "DialogBmp=${CMAKE_SOURCE_DIR}/Installer/Installation/Files/main.bmp"
        -d "IconFile=${CMAKE_SOURCE_DIR}/resources/ROCm_Optiq.ico"
        -ext WixToolset.UI.wixext
        -out "${_wix_out}"
        "${_wix_src}"
      DEPENDS ${TARGET_NAME}
      COMMENT "Building WiX MSI installer: ${_wix_out}"
      VERBATIM
    )
  else()
    message(STATUS "wix.exe not found — PACKAGE_WIX target unavailable. "
                   "Install WiX v4: dotnet tool install --global wix")
  endif()
endif()
# ─────────────────────────────────────────────────────────────────────────────

# Copy runtime files to target folder
# Uses a no-op (cmake -E true) when there are no runtime DLLs (e.g. GLFW_LINK_STATIC=ON)
if(WIN32)
    add_custom_command(
        TARGET ${TARGET_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E
            $<IF:$<BOOL:$<TARGET_RUNTIME_DLLS:${TARGET_NAME}>>,copy_if_different,true>
            $<TARGET_RUNTIME_DLLS:${TARGET_NAME}>
            $<$<BOOL:$<TARGET_RUNTIME_DLLS:${TARGET_NAME}>>:$<TARGET_FILE_DIR:${TARGET_NAME}>>
        COMMAND_EXPAND_LISTS
        COMMENT "Copying dependent DLLs to executable directory"
    )

    # $<TARGET_RUNTIME_DLLS> only captures OpenSSL if find_package produced shared
    # imported targets with the .dll as IMPORTED_LOCATION (distribution-dependent),
    # so copy the resolved OpenSSL DLLs explicitly when that backend is enabled.
    if(ROCPROFVIS_ENABLE_REMOTE AND CRYPTO_BACKEND STREQUAL "OpenSSL")
        set(_ossl_dlls "")
        # 1) Preferred: the imported target's IMPORTED_LOCATION, when it is a DLL.
        foreach(_ossl_target OpenSSL::SSL OpenSSL::Crypto)
            if(TARGET ${_ossl_target})
                get_target_property(_loc ${_ossl_target} IMPORTED_LOCATION)
                if(_loc AND _loc MATCHES "\\.dll$")
                    list(APPEND _ossl_dlls "${_loc}")
                endif()
            endif()
        endforeach()
        # 2) Fallback: MSVC import libs live under lib/VC/... while DLLs are in
        #    the install root or bin/. FindOpenSSL sets IMPORTED_LOCATION(_RELEASE)
        #    to the .lib, not the .dll, and often does not set OPENSSL_ROOT_DIR at
        #    all—derive the prefix from OPENSSL_INCLUDE_DIR (../include) or hints.
        set(_ossl_install_prefix "")
        if(DEFINED OPENSSL_ROOT_DIR AND OPENSSL_ROOT_DIR)
            set(_ossl_install_prefix "${OPENSSL_ROOT_DIR}")
        elseif(DEFINED OPENSSL_INCLUDE_DIR AND OPENSSL_INCLUDE_DIR)
            get_filename_component(_ossl_install_prefix "${OPENSSL_INCLUDE_DIR}" DIRECTORY)
        elseif(DEFINED ENV{OPENSSL_ROOT_DIR} AND NOT "$ENV{OPENSSL_ROOT_DIR}" STREQUAL "")
            set(_ossl_install_prefix "$ENV{OPENSSL_ROOT_DIR}")
        endif()
        if(NOT _ossl_dlls AND _ossl_install_prefix)
            # Match only the DLLs for the OpenSSL we linked against. Install trees
            # (especially OpenSSL 4) may also ship older-generation names (e.g.
            # libcrypto-3-x64.dll next to libcrypto-4-x64.dll); a broad libssl*
            # glob would copy redundant runtimes.
            set(_ossl_ssl_glob "libssl*.dll")
            set(_ossl_crypto_glob "libcrypto*.dll")
            set(_ossl_maj "")
            if(DEFINED ROCPROFVIS_OPENSSL_VERSION_MAJOR AND NOT ROCPROFVIS_OPENSSL_VERSION_MAJOR STREQUAL "")
                set(_ossl_maj "${ROCPROFVIS_OPENSSL_VERSION_MAJOR}")
            elseif(DEFINED OPENSSL_VERSION_MAJOR AND NOT OPENSSL_VERSION_MAJOR STREQUAL "")
                set(_ossl_maj "${OPENSSL_VERSION_MAJOR}")
            endif()
            if(_ossl_maj)
                set(_ossl_ssl_glob "libssl-${_ossl_maj}*.dll")
                set(_ossl_crypto_glob "libcrypto-${_ossl_maj}*.dll")
            endif()
            file(GLOB _ossl_dlls
                "${_ossl_install_prefix}/${_ossl_ssl_glob}"
                "${_ossl_install_prefix}/${_ossl_crypto_glob}"
                "${_ossl_install_prefix}/bin/${_ossl_ssl_glob}"
                "${_ossl_install_prefix}/bin/${_ossl_crypto_glob}")
        endif()
        if(_ossl_install_prefix)
            message(STATUS "OpenSSL Windows runtime staging prefix: ${_ossl_install_prefix}")
        endif()
        if(_ossl_dlls)
            list(JOIN _ossl_dlls ", " _ossl_dlls_msg)
            message(STATUS "OpenSSL Windows runtime DLLs (POST_BUILD): ${_ossl_dlls_msg}")
            foreach(_ossl_dll ${_ossl_dlls})
                add_custom_command(
                    TARGET ${TARGET_NAME} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${_ossl_dll}"
                        "$<TARGET_FILE_DIR:${TARGET_NAME}>"
                    COMMENT "Copying OpenSSL runtime DLL ${_ossl_dll} to executable directory"
                )
            endforeach()
        else()
            message(WARNING
                "CRYPTO_BACKEND=OpenSSL: could not locate libssl/libcrypto DLLs to "
                "stage next to the executable. Set OPENSSL_ROOT_DIR (or OPENSSL_INCLUDE_DIR) "
                "so the install prefix is known, or place the DLLs on PATH so roc-optiq can run.")
        endif()
    endif()
endif()

if(PERFETTO_AVAILABLE)
	perfetto_copy_dll_to(${TARGET_NAME})
endif()
