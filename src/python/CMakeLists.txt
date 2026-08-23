# Copyright Advanced Micro Devices, Inc.
# SPDX-License-Identifier: MIT

set(TARGET_NAME roc-optiq-python)

cmake_minimum_required(VERSION 3.15)
project(${TARGET_NAME} VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Python3 3.8 REQUIRED COMPONENTS Interpreter Development)

add_library(${TARGET_NAME}
    src/rocprofvis_python_runtime.cpp
)

target_include_directories(${TARGET_NAME}
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/inc
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../core/inc
)

file(TO_CMAKE_PATH "${Python3_PREFIX}" ROCPROFVIS_PYTHON_HOME_PATH)
file(TO_CMAKE_PATH "${Python3_STDLIB}" ROCPROFVIS_PYTHON_STDLIB_PATH)
file(TO_CMAKE_PATH "${Python3_STDARCH}" ROCPROFVIS_PYTHON_STDARCH_PATH)

target_compile_definitions(${TARGET_NAME}
    PRIVATE
        ROCPROFVIS_PYTHON_HOME="${ROCPROFVIS_PYTHON_HOME_PATH}"
        ROCPROFVIS_PYTHON_STDLIB="${ROCPROFVIS_PYTHON_STDLIB_PATH}"
        ROCPROFVIS_PYTHON_STDARCH="${ROCPROFVIS_PYTHON_STDARCH_PATH}"
)

target_link_libraries(${TARGET_NAME}
    PUBLIC
        Python3::Python
    PRIVATE
        roc-optiq-core
        spdlog
)

# Official Windows CPython does not ship python3xx_d.lib. Debug still
# links the release import library; IMPORTED_LOCATION must stay the DLL
# so $<TARGET_RUNTIME_DLLS> copies python3xx.dll, not the .lib.
if(MSVC AND Python3_LIBRARY_RELEASE)
    set_property(TARGET Python3::Python PROPERTY
        IMPORTED_IMPLIB_DEBUG "${Python3_LIBRARY_RELEASE}")
    get_property(_py_dll TARGET Python3::Python PROPERTY IMPORTED_LOCATION_RELEASE)
    if(_py_dll AND _py_dll MATCHES "\\.dll$")
        set_property(TARGET Python3::Python PROPERTY
            IMPORTED_LOCATION_DEBUG "${_py_dll}")
    endif()
endif()

# Expose the Python prefix so tests can put python3xx.dll on PATH.
set(ROCPROFVIS_PYTHON3_RUNTIME_LIBRARY_DIRS "${Python3_RUNTIME_LIBRARY_DIRS}"
    CACHE INTERNAL "Python runtime dirs for scripting tests")
set(ROCPROFVIS_PYTHON3_LIBRARIES "${Python3_LIBRARIES}"
    CACHE INTERNAL "Python libraries for scripting tests")
