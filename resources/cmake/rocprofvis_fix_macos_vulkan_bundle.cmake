function(rocprofvis_run_install_name_tool)
    execute_process(
        COMMAND install_name_tool ${ARGN}
        RESULT_VARIABLE _result
        ERROR_VARIABLE _error
        OUTPUT_VARIABLE _output)
    if(NOT _result EQUAL 0)
        string(REPLACE "\n" " " _error "${_error}")
        string(REPLACE "\n" " " _output "${_output}")
        message(WARNING
            "install_name_tool ${ARGN} failed with ${_result}: ${_error}${_output}")
    endif()
endfunction()

if(NOT EXISTS "${ROCPROFVIS_APP_EXECUTABLE}")
    message(FATAL_ERROR "App executable not found: ${ROCPROFVIS_APP_EXECUTABLE}")
endif()

if(EXISTS "${ROCPROFVIS_BUNDLED_VULKAN_LOADER}")
    rocprofvis_run_install_name_tool(
        -id "@rpath/libvulkan.1.dylib" "${ROCPROFVIS_BUNDLED_VULKAN_LOADER}")
else()
    message(FATAL_ERROR
        "Bundled Vulkan loader not found: ${ROCPROFVIS_BUNDLED_VULKAN_LOADER}")
endif()

if(EXISTS "${ROCPROFVIS_BUNDLED_MOLTENVK}")
    rocprofvis_run_install_name_tool(
        -id "@rpath/libMoltenVK.dylib" "${ROCPROFVIS_BUNDLED_MOLTENVK}")
else()
    message(FATAL_ERROR "Bundled MoltenVK not found: ${ROCPROFVIS_BUNDLED_MOLTENVK}")
endif()

execute_process(
    COMMAND otool -L "${ROCPROFVIS_APP_EXECUTABLE}"
    RESULT_VARIABLE _otool_result
    OUTPUT_VARIABLE _otool_output
    ERROR_VARIABLE _otool_error)
if(NOT _otool_result EQUAL 0)
    message(FATAL_ERROR
        "otool failed for ${ROCPROFVIS_APP_EXECUTABLE}: ${_otool_error}")
endif()

string(REPLACE "\n" ";" _otool_lines "${_otool_output}")
foreach(_line IN LISTS _otool_lines)
    string(STRIP "${_line}" _line)
    if(_line MATCHES "^([^ ]*libvulkan[^ ]*\\.dylib)")
        set(_vulkan_dependency "${CMAKE_MATCH_1}")
        if(NOT _vulkan_dependency STREQUAL
           "@executable_path/../Frameworks/libvulkan.1.dylib")
            rocprofvis_run_install_name_tool(
                -change "${_vulkan_dependency}"
                "@executable_path/../Frameworks/libvulkan.1.dylib"
                "${ROCPROFVIS_APP_EXECUTABLE}")
        endif()
    endif()
endforeach()
