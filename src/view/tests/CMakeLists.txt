# Copyright Advanced Micro Devices, Inc.
# SPDX-License-Identifier: MIT

# Unit tests for the View layer that do NOT require a live ImGui/GLFW/Vulkan
# context. These exercise the pure-logic pieces of the view: utility
# functions, the time<->pixel transform, the view-side data models, and the
# header-only ID/metric helpers. UI behavior is exercised separately via the
# ImGui test engine and is intentionally out of scope here.
#
# Rather than depend on a "view" library (the view sources are compiled
# directly into the roc-optiq executable), this target compiles the small,
# self-contained subset of view sources it needs. Keep this list minimal: only
# add a source here if its unit under test genuinely needs it to link.

set(VIEW_TEST_NAME roc-optiq-view-tests)

set(VIEW_TEST_UNDER_TEST_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/rocprofvis_utils.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/rocprofvis_time_to_pixel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/model/rocprofvis_event_model.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/model/rocprofvis_summary_model.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/model/rocprofvis_topology_model.cpp
)

set(VIEW_TEST_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/rocprofvis_view_tests_main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/rocprofvis_view_utils_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/rocprofvis_view_time_to_pixel_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/rocprofvis_view_model_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/rocprofvis_view_id_tests.cpp
)

add_executable(${VIEW_TEST_NAME} ${VIEW_TEST_SRC} ${VIEW_TEST_UNDER_TEST_SRC})

target_include_directories(${VIEW_TEST_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/model
    ${CMAKE_CURRENT_SOURCE_DIR}/../inc
    ${CMAKE_SOURCE_DIR}/src/controller/inc
    # rocprofvis_requests.h pulls in rocprofvis_controller_analysis.h, which
    # lives in the controller's internal src/ tree (exported PUBLIC by the
    # controller target, but we don't link it here).
    ${CMAKE_SOURCE_DIR}/src/controller/src
    ${CMAKE_SOURCE_DIR}/src/model/inc
    ${CMAKE_SOURCE_DIR}/src/core/inc
)

# imgui provides ImVec2 (used by TimePixelTransform); spdlog is used by the
# event model. Catch2WithMain is unused for the harness (we provide our own
# main) but linking Catch2 brings in the framework. roc-optiq-core supplies the
# assert macros pulled in by rocprofvis_requests.h.
target_link_libraries(${VIEW_TEST_NAME} PRIVATE Catch2::Catch2)
target_link_libraries(${VIEW_TEST_NAME} PRIVATE imgui)
target_link_libraries(${VIEW_TEST_NAME} PRIVATE spdlog)
target_link_libraries(${VIEW_TEST_NAME} PRIVATE roc-optiq-core)

add_test(NAME ${VIEW_TEST_NAME} COMMAND ${VIEW_TEST_NAME})
set_tests_properties(${VIEW_TEST_NAME} PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
