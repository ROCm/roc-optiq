# Copyright Advanced Micro Devices, Inc.
# SPDX-License-Identifier: MIT

set(TARGET_NAME roc-optiq)

cmake_minimum_required(VERSION 3.15)
project(${TARGET_NAME})

# Define the version numbers
set(PROJECT_VERSION_MAJOR 0)
set(PROJECT_VERSION_MINOR 3)
set(PROJECT_VERSION_PATCH 1)
set(PROJECT_VERSION_BUILD 0)

# Name and path of the icon font header
set(ICON_HEADER ${CMAKE_SOURCE_DIR}/src/view/src/icons/rocprofvis_icon_data.h)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DVK_PROTOTYPES")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DVK_PROTOTYPES")

option(COMPUTE_UI_SUPPORT "Build with support for Rocprof Compute" ON)
option(ROCPROFVIS_DEVELOPER_MODE "Enable developer mode features" OFF)
option(ROCPROFVIS_ENABLE_INTERNAL_BANNER "Show diagonal internal build banner" OFF)
option(USE_NATIVE_FILE_DIALOG "Use the OS native file dialog instead of ImGui file dialog" ON)
option(GLFW_LINK_STATIC "Link GLFW statically instead of dynamically" ON)
option(BUILD_TESTING "Build the testing tree" ON)

if(APPLE AND USE_NATIVE_FILE_DIALOG)
    set(USE_NATIVE_FILE_DIALOG OFF CACHE BOOL
        "Use the OS native file dialog instead of ImGui file dialog" FORCE)
    message(WARNING "Disabling native file dialog on macOS; it must run on the main thread.")
endif()

if(COMPUTE_UI_SUPPORT) 
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DCOMPUTE_UI_SUPPORT")
endif(COMPUTE_UI_SUPPORT)
if(ROCPROFVIS_DEVELOPER_MODE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DROCPROFVIS_DEVELOPER_MODE")
endif(ROCPROFVIS_DEVELOPER_MODE)
if(ROCPROFVIS_ENABLE_INTERNAL_BANNER)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DROCPROFVIS_ENABLE_INTERNAL_BANNER")
endif(ROCPROFVIS_ENABLE_INTERNAL_BANNER)
if(USE_NATIVE_FILE_DIALOG)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DUSE_NATIVE_FILE_DIALOG")
endif(USE_NATIVE_FILE_DIALOG)

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
add_subdirectory(src/core)
add_subdirectory(src/model)
add_subdirectory(src/controller)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/model/inc)

find_package(Vulkan REQUIRED)

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
    src/view/src/rocprofvis_hotkey_manager.cpp 
    src/view/src/rocprofvis_stickynote.cpp
    src/view/src/rocprofvis_click_manager.cpp
    src/view/src/rocprofvis_analysis_view.cpp 
    src/view/src/rocprofvis_trace_view.cpp 
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
    src/view/src/model/rocprofvis_trace_data_model.cpp
    src/view/src/rocprofvis_raw_track_data.cpp
    src/view/src/rocprofvis_events.cpp
    src/view/src/rocprofvis_event_manager.cpp
    src/view/src/rocprofvis_utils.cpp
    src/view/src/rocprofvis_timeline_selection.cpp
    src/view/src/rocprofvis_track_topology.cpp
    src/view/src/rocprofvis_track_details.cpp
    src/view/src/rocprofvis_project.cpp
    src/view/src/rocprofvis_multi_track_table.cpp
    src/view/src/rocprofvis_event_search.cpp
    src/view/src/rocprofvis_summary_view.cpp
    src/view/src/rocprofvis_time_to_pixel.cpp
    src/view/src/rocprofvis_minimap.cpp
    src/view/src/widgets/rocprofvis_widget.cpp
    src/view/src/widgets/rocprofvis_split_containers.cpp
    src/view/src/widgets/rocprofvis_flex_container.cpp
    src/view/src/widgets/rocprofvis_tab_container.cpp
    src/view/src/widgets/rocprofvis_editable_textfield.cpp
    src/view/src/widgets/rocprofvis_debug_window.cpp
    src/view/src/widgets/rocprofvis_gui_helpers.cpp
    src/view/src/widgets/rocprofvis_infinite_scroll_table.cpp
    src/view/src/widgets/rocprofvis_dialog.cpp
    src/view/src/widgets/rocprofvis_notification_manager.cpp
    )

if(COMPUTE_UI_SUPPORT) 
list(APPEND VIEW_FILES
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
    src/view/src/compute/rocprofvis_compute_tester.cpp
    src/view/src/model/compute/rocprofvis_compute_data_model.cpp
    src/view/src/widgets/rocprofvis_compute_widget.cpp
    src/view/src/widgets/rocprofvis_query_builder.cpp
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

if(APPLE)
    add_executable(${TARGET_NAME} MACOSX_BUNDLE
        src/app/src/rocprofvis_imgui_backend.cpp
        src/app/src/rocprofvis_imgui_vulkan.cpp
        src/app/src/rocprofvis_imgui_opengl.cpp
        src/app/src/main.cpp
        src/app/src/glfw_util.cpp
        src/app/src/rocprofvis_cli_parser.cpp
        ${VIEW_FILES})
else()
    add_executable(${TARGET_NAME}
        src/app/src/rocprofvis_imgui_backend.cpp
        src/app/src/rocprofvis_imgui_vulkan.cpp
        src/app/src/rocprofvis_imgui_opengl.cpp
        src/app/src/main.cpp
        src/app/src/glfw_util.cpp
        src/app/src/rocprofvis_cli_parser.cpp
        ${VIEW_FILES})
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

if(APPLE)
    set(ROCPROFVIS_BUNDLE_VULKAN ON CACHE BOOL
        "Bundle Vulkan loader and MoltenVK runtime into the macOS app")
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

        set(ROCPROFVIS_VULKAN_SDK_DIR "")
        if(DEFINED ENV{VULKAN_SDK})
            set(ROCPROFVIS_VULKAN_SDK_DIR "$ENV{VULKAN_SDK}")
        endif()

        find_library(ROCPROFVIS_MOLTENVK_LIBRARY
            NAMES MoltenVK
            PATHS
                "${ROCPROFVIS_VULKAN_SDK_DIR}/lib"
                "${ROCPROFVIS_VULKAN_SDK_DIR}/../MoltenVK/dylib/macOS"
                "${ROCPROFVIS_VULKAN_SDK_DIR}/../MoltenVK/dylib"
            NO_DEFAULT_PATH)
        if(NOT ROCPROFVIS_MOLTENVK_LIBRARY)
            find_library(ROCPROFVIS_MOLTENVK_LIBRARY NAMES MoltenVK)
        endif()
        if(NOT ROCPROFVIS_MOLTENVK_LIBRARY)
            message(FATAL_ERROR
                "ROCPROFVIS_BUNDLE_VULKAN is ON, but libMoltenVK.dylib was not "
                "found. Install the Vulkan SDK or set CMAKE_PREFIX_PATH.")
        endif()

        find_file(ROCPROFVIS_MOLTENVK_ICD_JSON
            NAMES MoltenVK_icd.json
            PATHS
                "${ROCPROFVIS_VULKAN_SDK_DIR}/share/vulkan/icd.d"
                "${ROCPROFVIS_VULKAN_SDK_DIR}/../MoltenVK/icd"
            NO_DEFAULT_PATH)
        if(NOT ROCPROFVIS_MOLTENVK_ICD_JSON)
            find_file(ROCPROFVIS_MOLTENVK_ICD_JSON
                NAMES MoltenVK_icd.json
                PATH_SUFFIXES share/vulkan/icd.d)
        endif()

        set(ROCPROFVIS_BUNDLED_MOLTENVK_ICD_JSON
            "${CMAKE_CURRENT_BINARY_DIR}/MoltenVK_icd.json")
        if(ROCPROFVIS_MOLTENVK_ICD_JSON)
            file(READ "${ROCPROFVIS_MOLTENVK_ICD_JSON}"
                 ROCPROFVIS_MOLTENVK_ICD_CONTENTS)
            string(REGEX REPLACE
                "\"library_path\"[ \t]*:[ \t]*\"[^\"]+\""
                "\"library_path\": \"../../../Frameworks/libMoltenVK.dylib\""
                ROCPROFVIS_MOLTENVK_ICD_CONTENTS
                "${ROCPROFVIS_MOLTENVK_ICD_CONTENTS}")
            file(WRITE "${ROCPROFVIS_BUNDLED_MOLTENVK_ICD_JSON}"
                 "${ROCPROFVIS_MOLTENVK_ICD_CONTENTS}")
        else()
            file(WRITE "${ROCPROFVIS_BUNDLED_MOLTENVK_ICD_JSON}"
                "{\n"
                "    \"file_format_version\": \"1.0.0\",\n"
                "    \"ICD\": {\n"
                "        \"library_path\": \"../../../Frameworks/libMoltenVK.dylib\",\n"
                "        \"api_version\": \"1.3.0\",\n"
                "        \"is_portability_driver\": true\n"
                "    }\n"
                "}\n")
        endif()
    endif()

    set_target_properties(${TARGET_NAME} PROPERTIES
        BUILD_RPATH "@executable_path/../Frameworks;/usr/local/lib;/opt/homebrew/lib"
        INSTALL_RPATH "@executable_path/../Frameworks;/usr/local/lib;/opt/homebrew/lib"
    )
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
    if(ROCPROFVIS_BUNDLE_VULKAN)
        add_custom_command(
            TARGET ${TARGET_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_BUNDLE_DIR:${TARGET_NAME}>/Contents/Frameworks"
                "$<TARGET_BUNDLE_DIR:${TARGET_NAME}>/Contents/Resources/vulkan/icd.d"
                "$<TARGET_BUNDLE_DIR:${TARGET_NAME}>/Contents/Resources/Licenses"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${ROCPROFVIS_VULKAN_LOADER_LIBRARY}"
                "$<TARGET_BUNDLE_DIR:${TARGET_NAME}>/Contents/Frameworks/libvulkan.1.dylib"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${ROCPROFVIS_MOLTENVK_LIBRARY}"
                "$<TARGET_BUNDLE_DIR:${TARGET_NAME}>/Contents/Frameworks/libMoltenVK.dylib"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${ROCPROFVIS_BUNDLED_MOLTENVK_ICD_JSON}"
                "$<TARGET_BUNDLE_DIR:${TARGET_NAME}>/Contents/Resources/vulkan/icd.d/MoltenVK_icd.json"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_CURRENT_SOURCE_DIR}/resources/licenses/Vulkan-Loader-LICENSE.txt"
                "$<TARGET_BUNDLE_DIR:${TARGET_NAME}>/Contents/Resources/Licenses/Vulkan-Loader-LICENSE.txt"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_CURRENT_SOURCE_DIR}/resources/licenses/MoltenVK-LICENSE.txt"
                "$<TARGET_BUNDLE_DIR:${TARGET_NAME}>/Contents/Resources/Licenses/MoltenVK-LICENSE.txt"
            COMMAND install_name_tool -id "@rpath/libvulkan.1.dylib"
                "$<TARGET_BUNDLE_DIR:${TARGET_NAME}>/Contents/Frameworks/libvulkan.1.dylib"
            COMMAND install_name_tool -id "@rpath/libMoltenVK.dylib"
                "$<TARGET_BUNDLE_DIR:${TARGET_NAME}>/Contents/Frameworks/libMoltenVK.dylib"
            COMMAND install_name_tool -change "${ROCPROFVIS_VULKAN_LOADER_LIBRARY}"
                "@executable_path/../Frameworks/libvulkan.1.dylib"
                "$<TARGET_BUNDLE_DIR:${TARGET_NAME}>/Contents/MacOS/$<TARGET_FILE_NAME:${TARGET_NAME}>"
            COMMAND install_name_tool -change "@rpath/libvulkan.1.dylib"
                "@executable_path/../Frameworks/libvulkan.1.dylib"
                "$<TARGET_BUNDLE_DIR:${TARGET_NAME}>/Contents/MacOS/$<TARGET_FILE_NAME:${TARGET_NAME}>"
            COMMAND install_name_tool -change "/usr/local/lib/libvulkan.1.dylib"
                "@executable_path/../Frameworks/libvulkan.1.dylib"
                "$<TARGET_BUNDLE_DIR:${TARGET_NAME}>/Contents/MacOS/$<TARGET_FILE_NAME:${TARGET_NAME}>"
            COMMAND install_name_tool -change "/opt/homebrew/lib/libvulkan.1.dylib"
                "@executable_path/../Frameworks/libvulkan.1.dylib"
                "$<TARGET_BUNDLE_DIR:${TARGET_NAME}>/Contents/MacOS/$<TARGET_FILE_NAME:${TARGET_NAME}>"
            COMMENT "Staging Vulkan loader and MoltenVK into the app bundle"
        )
    endif()
endif()

# === NativeFileDialog Extended ===
if(USE_NATIVE_FILE_DIALOG)
  set(NFD_PORTAL ON CACHE BOOL "Use xdg-desktop-portal instead of GTK")
  add_subdirectory(thirdparty/nativefiledialog-extended)
  target_link_libraries(${TARGET_NAME} PRIVATE nfd)
endif()
# === ImGuiFileDialog ===
if(NOT USE_NATIVE_FILE_DIALOG)
  target_link_libraries(${TARGET_NAME} PRIVATE ImGuiFileDialog)
endif()

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
endif()
