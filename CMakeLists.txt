cmake_minimum_required(VERSION 3.15)
project(rocprof-visualizer)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DVK_PROTOTYPES")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DVK_PROTOTYPES")

add_subdirectory(thirdparty/)

add_library(rocprof-visualizer-controller STATIC
    src/controller/src/rocprofvis_controller_array.cpp
    src/controller/src/rocprofvis_controller_data.cpp
    src/controller/src/rocprofvis_controller_event.cpp
    src/controller/src/rocprofvis_controller_event_lod.cpp
    src/controller/src/rocprofvis_controller_future.cpp
    src/controller/src/rocprofvis_controller_graph.cpp
    src/controller/src/rocprofvis_controller_sample.cpp
    src/controller/src/rocprofvis_controller_sample_lod.cpp
    src/controller/src/rocprofvis_controller_segment.cpp
    src/controller/src/rocprofvis_controller_timeline.cpp
    src/controller/src/rocprofvis_controller_trace.cpp
    src/controller/src/rocprofvis_controller_json_trace.cpp
    src/controller/src/rocprofvis_controller_track.cpp
    src/controller/src/rocprofvis_controller.cpp
)
target_include_directories(rocprof-visualizer-controller PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/app/inc)
target_include_directories(rocprof-visualizer-controller PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/controller/inc)
target_include_directories(rocprof-visualizer-controller PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/jsoncpp)
target_link_libraries(rocprof-visualizer-controller PRIVATE json)

add_executable(rocprof-visualizer src/app/src/rocprofvis_trace_ui.cpp src/app/src/rocprofvis_imgui_vulkan.cpp src/app/src/main.cpp src/view/src/rocprofvis_line_chart.cpp src/view/src/rocprofvis_line_chart.h src/view/src/rocprofvis_main_view.cpp src/view/src/rocprofvis_grid.cpp src/view/src/rocprofvis_grid.h src/view/src/rocprofvis_flame_chart.cpp src/view/src/rocprofvis_flame_chart.h src/view/src/rocprofvis_graph_view_metadata.h src/view/src/rocprofvis_graph_view_metadata.cpp )
target_include_directories(rocprof-visualizer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/app/inc)
target_include_directories(rocprof-visualizer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/controller/inc)
target_include_directories(rocprof-visualizer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/view/inc)
target_include_directories(rocprof-visualizer PRIVATE ${Vulkan_INCLUDE_DIRS})
target_include_directories(rocprof-visualizer PRIVATE ${Vulkan_INCLUDE_DIR})
target_link_libraries(rocprof-visualizer PRIVATE imgui)
target_link_libraries(rocprof-visualizer PRIVATE imgui-flame-graph)
target_link_libraries(rocprof-visualizer PRIVATE implot)
target_link_libraries(rocprof-visualizer PRIVATE ImGuiFileDialog)
target_link_libraries(rocprof-visualizer PRIVATE glfw)
target_link_libraries(rocprof-visualizer PRIVATE rocprof-visualizer-controller)
find_package(Vulkan REQUIRED)
target_link_libraries(rocprof-visualizer PRIVATE "${Vulkan_LIBRARIES}")
