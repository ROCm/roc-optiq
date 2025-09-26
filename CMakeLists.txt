cmake_minimum_required(VERSION 3.15)
project(rocprof-visualizer)

# Define the version numbers
set(PROJECT_VERSION_MAJOR 0)
set(PROJECT_VERSION_MINOR 1)
set(PROJECT_VERSION_PATCH 6)
set(PROJECT_VERSION_BUILD 0)

# Name and path of the icon font header
set(ICON_HEADER ${CMAKE_SOURCE_DIR}/src/view/src/icons/rocprofvis_icon_data.h)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DVK_PROTOTYPES")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DVK_PROTOTYPES")

option(JSON_TRACE_SUPPORT "Build with support for JSON traces" OFF)
option(COMPUTE_UI_SUPPORT "Build with support for Rocprof Compute .CSVs" OFF)
option(ROCPROFVIS_DEVELOPER_MODE "Enable developer mode features" OFF)
option(ROCPROFVIS_ENABLE_INTERNAL_BANNER "Show diagonal internal build banner" ON)

# Add third-party libraries, core, and model BEFORE controller
add_subdirectory(thirdparty)

add_subdirectory(src/core)
add_subdirectory(src/model)
add_subdirectory(src/controller)

set(rocprofvis_source_files)
if(JSON_TRACE_SUPPORT) 
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DVK_PROTOTYPES -DJSON_TRACE_SUPPORT")
endif(JSON_TRACE_SUPPORT)
if(COMPUTE_UI_SUPPORT) 
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DVK_PROTOTYPES -DCOMPUTE_UI_SUPPORT")
endif(COMPUTE_UI_SUPPORT)
if(ROCPROFVIS_DEVELOPER_MODE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DROCPROFVIS_DEVELOPER_MODE")
endif(ROCPROFVIS_DEVELOPER_MODE)
if(ROCPROFVIS_ENABLE_INTERNAL_BANNER)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DVK_PROTOTYPES -DROCPROFVIS_ENABLE_INTERNAL_BANNER")
endif(ROCPROFVIS_ENABLE_INTERNAL_BANNER)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/model/inc)

# View related src files
set(VIEW_FILES
    src/view/src/rocprofvis_appwindow.cpp 
    src/view/src/rocprofvis_appwindow.h 
    src/view/src/rocprofvis_annotation_view.cpp
    src/view/src/rocprofvis_track_item.cpp 
    src/view/src/rocprofvis_flame_track_item.cpp 
    src/view/src/rocprofvis_timeline_arrow.cpp 
    src/view/src/rocprofvis_events_view.cpp 
    src/view/src/rocprofvis_settings_manager.cpp
    src/view/src/rocprofvis_font_manager.cpp 
    src/view/src/rocprofvis_annotations.cpp
    src/view/src/rocprofvis_settings_panel.cpp 
    src/view/src/rocprofvis_stickynote.cpp
    src/view/src/rocprofvis_analysis_view.cpp 
    src/view/src/rocprofvis_trace_view.cpp 
    src/view/src/rocprofvis_line_track_item.cpp 
    src/view/src/rocprofvis_timeline_view.cpp 
    src/view/src/rocprofvis_sidebar.cpp 
    src/view/src/rocprofvis_view_module.cpp
    src/view/src/rocprofvis_data_provider.cpp
    src/view/src/rocprofvis_raw_track_data.cpp
    src/view/src/rocprofvis_events.cpp
    src/view/src/rocprofvis_event_manager.cpp
    src/view/src/rocprofvis_utils.cpp
    src/view/src/rocprofvis_timeline_selection.cpp
    src/view/src/rocprofvis_track_topology.cpp
    src/view/src/rocprofvis_track_details.cpp
    src/view/src/rocprofvis_project.cpp
	src/view/src/rocprofvis_multi_track_table.cpp
    src/view/src/widgets/rocprofvis_widget.cpp	
    src/view/src/widgets/rocprofvis_debug_window.cpp
    src/view/src/widgets/rocprofvis_gui_helpers.cpp
    src/view/src/widgets/rocprofvis_infinite_scroll_table.cpp
    src/view/src/widgets/rocprofvis_dialog.cpp
    src/view/src/widgets/rocprofvis_notification_manager.cpp
    )

if(COMPUTE_UI_SUPPORT) 
list(APPEND VIEW_FILES
    src/view/src/rocprofvis_compute_root.cpp
	src/view/src/rocprofvis_compute_summary.cpp
	src/view/src/rocprofvis_compute_block.cpp
	src/view/src/rocprofvis_compute_table.cpp
	src/view/src/rocprofvis_compute_data_provider.cpp
	src/view/src/rocprofvis_compute_roofline.cpp
	src/view/src/rocprofvis_navigation_manager.cpp
    src/view/src/widgets/rocprofvis_compute_widget.cpp
)
endif(COMPUTE_UI_SUPPORT)

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

add_executable(rocprof-visualizer 
    src/app/src/rocprofvis_imgui_vulkan.cpp 
    src/app/src/main.cpp
    ${VIEW_FILES})

add_dependencies(rocprof-visualizer generate_icon_header)

if(WIN32)
    set_target_properties(rocprof-visualizer PROPERTIES WIN32_EXECUTABLE TRUE)
    set_target_properties(rocprof-visualizer PROPERTIES LINK_FLAGS "/ENTRY:mainCRTStartup")
endif(WIN32)

target_include_directories(rocprof-visualizer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/app/inc)
target_include_directories(rocprof-visualizer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/controller/inc)
target_include_directories(rocprof-visualizer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/view/inc)
target_include_directories(rocprof-visualizer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/view/src)
target_include_directories(rocprof-visualizer PRIVATE ${Vulkan_INCLUDE_DIRS})
target_include_directories(rocprof-visualizer PRIVATE ${Vulkan_INCLUDE_DIR})
target_include_directories(rocprof-visualizer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/model/inc)
target_include_directories(rocprof-visualizer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/core/inc)
target_link_libraries(rocprof-visualizer PRIVATE imgui)
target_link_libraries(rocprof-visualizer PRIVATE implot)
target_link_libraries(rocprof-visualizer PRIVATE ImGuiFileDialog)
target_link_libraries(rocprof-visualizer PRIVATE glfw)
target_link_libraries(rocprof-visualizer PRIVATE rocprof-visualizer-controller)
target_link_libraries(rocprof-visualizer PRIVATE datamodel)
target_link_libraries(rocprof-visualizer PRIVATE rocprof-visualizer-core)
target_link_libraries(rocprof-visualizer PRIVATE spdlog)
target_link_libraries(rocprof-visualizer PRIVATE sqlite3)
target_link_libraries(rocprof-visualizer PRIVATE json)
find_package(Vulkan REQUIRED)
target_link_libraries(rocprof-visualizer PRIVATE "${Vulkan_LIBRARIES}")

# Csv parser enables coverage in debug builds so we need to enable it too
if(COMPUTE_UI_SUPPORT AND ((CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX) AND (CMAKE_BUILD_TYPE STREQUAL "Debug" OR NOT CMAKE_BUILD_TYPE)))
    message(STATUS "Adding --coverage to linker flags for rocprof-visualizer due to potential gcov symbols from dependencies.")
    target_link_options(rocprof-visualizer PRIVATE --coverage)
endif()

