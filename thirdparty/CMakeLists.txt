# Copyright Advanced Micro Devices, Inc.
# SPDX-License-Identifier: MIT

find_package(Vulkan REQUIRED)
set(VULKAN_LIB "Vulkan::Vulkan")

add_subdirectory(spdlog)
add_subdirectory(catch2)

set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "Disable yaml-cpp tests")
set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "Disable yaml-cpp tools")
add_subdirectory(yaml-cpp)

add_library(imgui STATIC
    imgui/imgui.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_tables.cpp
    imgui/imgui_widgets.cpp
    imgui/backends/imgui_impl_glfw.cpp
    imgui/backends/imgui_impl_vulkan.cpp
    imgui/backends/imgui_impl_opengl3.cpp
)
target_compile_definitions(imgui PUBLIC IMGUI_DEFINE_MATH_OPERATORS)
# Base imgui is now ALWAYS clean (no test-engine define) -> production app links this.
target_include_directories(imgui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(imgui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/imgui)
target_include_directories(imgui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/imgui/backends)
target_include_directories(imgui PUBLIC ${Vulkan_INCLUDE_DIRS})
target_include_directories(imgui PUBLIC ${Vulkan_INCLUDE_DIR})

# Second imgui built from the SAME sources WITH the test-engine define.
# Only the test exe links this; production never sees it.
if(BUILD_TESTING AND ROCPROFVIS_ENABLE_UI_TESTS)
    add_library(imgui_te STATIC
        imgui/imgui.cpp
        imgui/imgui_draw.cpp
        imgui/imgui_tables.cpp
        imgui/imgui_widgets.cpp
        imgui/backends/imgui_impl_glfw.cpp
        imgui/backends/imgui_impl_vulkan.cpp
        imgui/backends/imgui_impl_opengl3.cpp
    )
    target_compile_definitions(imgui_te PUBLIC
        IMGUI_DEFINE_MATH_OPERATORS
        IMGUI_ENABLE_TEST_ENGINE
    )
    target_include_directories(imgui_te PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    target_include_directories(imgui_te PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/imgui)
    target_include_directories(imgui_te PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/imgui/backends)
    target_include_directories(imgui_te PUBLIC ${Vulkan_INCLUDE_DIRS})
    target_include_directories(imgui_te PUBLIC ${Vulkan_INCLUDE_DIR})
endif()

add_library(implot STATIC
    implot/implot.cpp
    implot/implot_items.cpp
)
target_compile_definitions(implot PUBLIC IMGUI_DEFINE_MATH_OPERATORS)
target_include_directories(implot PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/imgui)
target_include_directories(implot PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/implot)

add_library(ImGuiFileDialog STATIC
    ImGuiFileDialog/ImGuiFileDialog.cpp
)
target_compile_definitions(ImGuiFileDialog PUBLIC IMGUI_DEFINE_MATH_OPERATORS)
target_include_directories(ImGuiFileDialog PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/imgui)
target_include_directories(ImGuiFileDialog PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/ImGuiFileDialog)

# Add double-conversion library
add_library(double-conversion STATIC
    jsoncpp/double-conversion/bignum.cc
    jsoncpp/double-conversion/bignum-dtoa.cc
    jsoncpp/double-conversion/cached-powers.cc
    jsoncpp/double-conversion/double-to-string.cc
    jsoncpp/double-conversion/fast-dtoa.cc
    jsoncpp/double-conversion/fixed-dtoa.cc
    jsoncpp/double-conversion/string-to-double.cc
    jsoncpp/double-conversion/strtod.cc
)
target_include_directories(double-conversion PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/jsoncpp/double-conversion)

# Main JSON library
add_library(json STATIC
    jsoncpp/json.cpp
)
target_link_libraries(json PRIVATE double-conversion)
target_include_directories(json PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/jsoncpp)

add_executable(imgui_binary_to_compressed_c
	imgui/misc/fonts/binary_to_compressed_c.cpp
)
set(ICON_FILE ${CMAKE_CURRENT_SOURCE_DIR}/ionicons/fonts/ionicons.ttf)
add_custom_command(
    OUTPUT ${ICON_HEADER}
    COMMAND imgui_binary_to_compressed_c ${ICON_FILE} icon_font -u8 > ${ICON_HEADER}
    DEPENDS ${ICON_FILE} imgui_binary_to_compressed_c
    COMMENT "Building ${ICON_HEADER} from ${ICON_FILE}"
    VERBATIM
)

add_custom_target(generate_icon_header
    DEPENDS ${ICON_HEADER}
)

set(GLFW_DIR glfw/)
option(GLFW_BUILD_EXAMPLES "Build the GLFW example programs" OFF)
option(GLFW_BUILD_TESTS "Build the GLFW test programs" OFF)
option(GLFW_BUILD_DOCS "Build the GLFW documentation" OFF)
option(GLFW_INSTALL "Generate installation target" OFF)
set(GLFW_LIBRARY_TYPE "SHARED" CACHE STRING
    "Library type override for GLFW (SHARED, STATIC, OBJECT, or empty to follow BUILD_SHARED_LIBS)")
option(GLFW_DOCUMENT_INTERNALS "Include internals in documentation" OFF)
add_subdirectory(${GLFW_DIR})
include_directories(${GLFW_DIR}/include)
include_directories(${GLFW_DIR}/deps)
