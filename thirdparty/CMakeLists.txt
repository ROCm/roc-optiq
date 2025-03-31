find_package(Vulkan REQUIRED)
set(VULKAN_LIB "Vulkan::Vulkan")

add_library(imgui STATIC
    imgui/imgui.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_tables.cpp
    imgui/imgui_widgets.cpp
    imgui/imgui_demo.cpp
    imgui/backends/imgui_impl_glfw.cpp
    imgui/backends/imgui_impl_vulkan.cpp
)
target_compile_definitions(imgui PUBLIC IMGUI_DEFINE_MATH_OPERATORS)
target_include_directories(imgui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(imgui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/imgui)
target_include_directories(imgui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/imgui/backends)
target_include_directories(imgui PUBLIC ${Vulkan_INCLUDE_DIRS})
target_include_directories(imgui PUBLIC ${Vulkan_INCLUDE_DIR})

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

add_subdirectory(jsoncpp/)

set(GLFW_DIR glfw/)
option(GLFW_BUILD_EXAMPLES "Build the GLFW example programs" OFF)
option(GLFW_BUILD_TESTS "Build the GLFW test programs" OFF)
option(GLFW_BUILD_DOCS "Build the GLFW documentation" OFF)
option(GLFW_INSTALL "Generate installation target" OFF)
set(GLFW_LIBRARY_TYPE "STATIC" CACHE STRING
    "Library type override for GLFW (SHARED, STATIC, OBJECT, or empty to follow BUILD_SHARED_LIBS)")
option(GLFW_DOCUMENT_INTERNALS "Include internals in documentation" OFF)
add_subdirectory(${GLFW_DIR})
include_directories(${GLFW_DIR}/include)
include_directories(${GLFW_DIR}/deps)
