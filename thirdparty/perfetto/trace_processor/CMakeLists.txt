# Guard against multiple inclusion
if(TARGET perfetto_trace_processor)
     message(STATUS "Perfetto trace processor multiple inclusions")
    return()
endif()

set(PERFETTO_PREBUILT_DIR ${CMAKE_CURRENT_SOURCE_DIR})

#Detect whether prebuilt libraries and headers are available
if(WIN32)
    set(PERFETTO_LIB_CHECK
        ${PERFETTO_PREBUILT_DIR}/lib/windows/release/trace_processor.lib)
else()
    set(PERFETTO_LIB_CHECK
        ${PERFETTO_PREBUILT_DIR}/lib/linux/release/libtrace_processor.a)
endif()

set(PERFETTO_HEADER_CHECK
    ${PERFETTO_PREBUILT_DIR}/include/perfetto/trace_processor/trace_processor.h)

if(EXISTS ${PERFETTO_LIB_CHECK} AND EXISTS ${PERFETTO_HEADER_CHECK})
    set(PERFETTO_AVAILABLE TRUE  CACHE BOOL
        "Perfetto trace processor libraries are available" FORCE)
else()
    set(PERFETTO_AVAILABLE FALSE CACHE BOOL
        "Perfetto trace processor libraries are available" FORCE)
    message(STATUS
        "Perfetto trace processor not found — feature disabled. "
        "Run: cmake --build . --target build_perfetto_libs")
endif()

if(NOT PERFETTO_AVAILABLE)
    message(STATUS "Perfetto trace processor libraries are not available")
    return()
endif()

set(WRAPPER_SOURCE
    ${CMAKE_SOURCE_DIR}/src/model/src/database/rocprofvis_db_trace_processor_dll.cpp)

# Windows 
if(WIN32)

    # Static lib - pulled into DLL via /WHOLEARCHIVE
    add_library(perfetto_trace_processor_static STATIC IMPORTED GLOBAL)
    set_target_properties(perfetto_trace_processor_static PROPERTIES
        IMPORTED_LOCATION_RELEASE
            ${PERFETTO_PREBUILT_DIR}/lib/windows/release/trace_processor.lib
        IMPORTED_LOCATION_DEBUG
            ${PERFETTO_PREBUILT_DIR}/lib/windows/debug/trace_processor.lib
        IMPORTED_LOCATION
            ${PERFETTO_PREBUILT_DIR}/lib/windows/release/trace_processor.lib
    )

    # Wrapper DLL
    add_library(perfetto_trace_processor_dll SHARED ${WRAPPER_SOURCE})

    target_include_directories(perfetto_trace_processor_dll PRIVATE
        ${PERFETTO_PREBUILT_DIR}/include
        ${PERFETTO_PREBUILT_DIR}/gen/build_config
    )
    target_compile_definitions(perfetto_trace_processor_dll PRIVATE
        PERFETTO_IMPLEMENTATION
    )
    target_compile_options(perfetto_trace_processor_dll PRIVATE
        $<$<CONFIG:Debug>:/MDd>
        $<$<CONFIG:Release>:/MD>
    )
    target_link_options(perfetto_trace_processor_dll PRIVATE
        $<$<CONFIG:Release>:/WHOLEARCHIVE:${PERFETTO_PREBUILT_DIR}/lib/windows/release/trace_processor.lib>
        $<$<CONFIG:Debug>:/WHOLEARCHIVE:${PERFETTO_PREBUILT_DIR}/lib/windows/debug/trace_processor.lib>
    )
    target_link_libraries(perfetto_trace_processor_dll PRIVATE
        ws2_32 dbghelp
    )

# Linux / macOS 
else()

    find_program(CLANG_CXX clang++)
    if(NOT CLANG_CXX)
        message(FATAL_ERROR "clang++ not found - required to compile Perfetto wrapper on Linux")
    endif()

    message(STATUS "Using clang++ for Perfetto wrapper: ${CLANG_CXX}")

    set(PERFETTO_LINUX_LIB
        ${PERFETTO_PREBUILT_DIR}/lib/linux/release/libtrace_processor.a)

    set(WRAPPER_OBJ_DIR ${CMAKE_CURRENT_BINARY_DIR}/perfetto_wrapper)
    set(WRAPPER_OBJ     ${WRAPPER_OBJ_DIR}/wrapper.o)
    set(PERFETTO_SO     ${CMAKE_CURRENT_BINARY_DIR}/libperfetto_trace_processor_dll.so)

    # Compile wrapper - must use clang++ with libc++ to match Perfetto's ABI
    add_custom_command(
        OUTPUT ${WRAPPER_OBJ}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${WRAPPER_OBJ_DIR}  
        COMMAND ${CLANG_CXX}
            -c -fPIC -std=c++17
            -stdlib=libc++
            -DPERFETTO_IMPLEMENTATION
            -I${PERFETTO_PREBUILT_DIR}/include
            -I${PERFETTO_PREBUILT_DIR}/gen/build_config
            ${WRAPPER_SOURCE}
            -o ${WRAPPER_OBJ}
        DEPENDS ${WRAPPER_SOURCE}
        COMMENT "Compiling Perfetto wrapper with clang++"
        VERBATIM
    )

    # Link .so
    add_custom_command(
        OUTPUT ${PERFETTO_SO}
        COMMAND ${CLANG_CXX}
            -shared -fPIC
            -stdlib=libc++
            -o ${PERFETTO_SO}
            ${WRAPPER_OBJ}
            -Wl,--whole-archive
            ${PERFETTO_LINUX_LIB}
            -Wl,--no-whole-archive
            -Wl,-rpath,$ORIGIN
            -lpthread -ldl
        DEPENDS ${WRAPPER_OBJ} ${PERFETTO_LINUX_LIB}
        COMMENT "Linking Perfetto trace processor shared library"
        VERBATIM
    )

    add_custom_target(perfetto_trace_processor_dll_build
        ALL
        DEPENDS ${PERFETTO_SO}
    )

    add_library(perfetto_trace_processor_dll SHARED IMPORTED GLOBAL)
    set_target_properties(perfetto_trace_processor_dll PROPERTIES
        IMPORTED_LOCATION ${PERFETTO_SO}
    )

    add_dependencies(perfetto_trace_processor_dll
        perfetto_trace_processor_dll_build)

endif()

# Interface target - what all consuming code links against 
add_library(perfetto_trace_processor INTERFACE)

target_link_libraries(perfetto_trace_processor INTERFACE
    perfetto_trace_processor_dll
)
target_include_directories(perfetto_trace_processor INTERFACE
    ${PERFETTO_PREBUILT_DIR}/include
    ${PERFETTO_PREBUILT_DIR}/gen/build_config
)

# Helper - copy DLL/SO next to any executable 
function(perfetto_copy_dll_to target)
    if(WIN32)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:perfetto_trace_processor_dll>
                $<TARGET_FILE_DIR:${target}>
            COMMENT "Copying trace_processor.dll to ${target}"
        )
    else()
        # On Linux the .so must also be next to the executable
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:perfetto_trace_processor_dll>
                $<TARGET_FILE_DIR:${target}>
            COMMENT "$<TARGET_FILE:perfetto_trace_processor_dll> to $<TARGET_FILE_DIR:${target}>"
        )
    endif()
endfunction()