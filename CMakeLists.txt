cmake_minimum_required(VERSION 3.15)
project(rocprof-visualizer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DVK_PROTOTYPES")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DVK_PROTOTYPES")

option(JSON_SUPPORT "Build with support for JSON traces" ON)

add_subdirectory(src/controller)

set(rocprofvis_source_files)
if(JSON_SUPPORT) 
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DVK_PROTOTYPES -DJSON_SUPPORT")
endif(JSON_SUPPORT)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/model/inc)

# View related src files
set(VIEW_FILES
    src/view/src/rocprofvis_appwindow.cpp 
    src/view/src/rocprofvis_appwindow.h 
    src/view/src/rocprofvis_charts.cpp 
    src/view/src/rocprofvis_flame_chart.cpp 
    src/view/src/rocprofvis_grid.cpp 
    src/view/src/rocprofvis_home_screen.cpp 
    src/view/src/rocprofvis_line_chart.cpp 
    src/view/src/rocprofvis_boxplot.cpp 
    src/view/src/rocprofvis_main_view.cpp 
    src/view/src/rocprofvis_sidebar.cpp 
    src/view/src/rocprofvis_view_module.cpp
    src/view/src/rocprofvis_data_provider.cpp
    src/view/src/rocprofvis_raw_track_data.cpp
    src/view/src/widgets/rocprofvis_widget.cpp
    src/view/src/widgets/rocprofvis_debug_window.cpp
)

add_executable(rocprof-visualizer 
    src/app/src/rocprofvis_imgui_vulkan.cpp 
    src/app/src/main.cpp
    ${VIEW_FILES})

target_include_directories(rocprof-visualizer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/app/inc)
target_include_directories(rocprof-visualizer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/controller/inc)
target_include_directories(rocprof-visualizer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/view/inc)
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
find_package(Vulkan REQUIRED)
target_link_libraries(rocprof-visualizer PRIVATE "${Vulkan_LIBRARIES}")

