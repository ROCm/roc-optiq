# Copyright Advanced Micro Devices, Inc.
# SPDX-License-Identifier: MIT

find_package(Vulkan REQUIRED)
set(VULKAN_LIB "Vulkan::Vulkan")

add_subdirectory(spdlog)
add_subdirectory(catch2)

set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "Disable yaml-cpp tests")
set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "Disable yaml-cpp tools")
add_subdirectory(yaml-cpp)

# TEMPORARY (remote/SSH + agentic profiling): mbedTLS serves two independent
# features - it is the default crypto backend for libssh2, and cpp-httplib links
# it for the assistant's in-process HTTPS. Either one alone is reason to build
# it, and CMake rejects a second add_subdirectory, so the decision is made once
# here. Default to the vendored backend so the out-of-the-box build needs no
# system OpenSSL; OpenSSL is opt-in via -DCRYPTO_BACKEND=OpenSSL and is then
# resolved as an external dependency (find_package below), not vendored.
set(CRYPTO_BACKEND "mbedTLS" CACHE STRING "Crypto backend (OpenSSL or mbedTLS)")

set(ROCPROFVIS_NEED_MBEDTLS OFF)
if(ROCPROFVIS_ENABLE_AGENTIC_PROFILING)
    set(ROCPROFVIS_NEED_MBEDTLS ON)
endif()
if(ROCPROFVIS_ENABLE_REMOTE AND CRYPTO_BACKEND STREQUAL "mbedTLS")
    set(ROCPROFVIS_NEED_MBEDTLS ON)
endif()

if(ROCPROFVIS_NEED_MBEDTLS)
    set(ENABLE_PROGRAMS OFF CACHE BOOL "" FORCE)
    set(ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    set(ENABLE_DOCUMENTATION OFF CACHE BOOL "" FORCE)
    set(MBEDTLS_FATAL_WARNINGS OFF CACHE BOOL "" FORCE)
    set(_prev_mbedtls_build_testing ${BUILD_TESTING})
    set(_prev_skip_install_rules ${CMAKE_SKIP_INSTALL_RULES})
    set(BUILD_TESTING OFF)
    set(BUILD_EXAMPLES OFF)
    set(CMAKE_SKIP_INSTALL_RULES ON)
    add_subdirectory(mbedtls EXCLUDE_FROM_ALL)
    set(CMAKE_SKIP_INSTALL_RULES ${_prev_skip_install_rules})
    set(BUILD_TESTING ${_prev_mbedtls_build_testing})
endif(ROCPROFVIS_NEED_MBEDTLS)

# TEMPORARY (agentic profiling): cpp-httplib (MIT), the in-process HTTPS
# client, a submodule pinned to v0.53.1. TLS via vendored mbedTLS so the GUI
# never shells out to curl.exe, which flashes a console on Windows and takes
# the request down with it. Upstream keeps httplib.h at its repo root and we
# compile it header-only, so its own CMakeLists is deliberately not added.
if(ROCPROFVIS_ENABLE_AGENTIC_PROFILING)
if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/cpp-httplib/httplib.h)
    message(FATAL_ERROR
        "ROCPROFVIS_ENABLE_AGENTIC_PROFILING is ON but thirdparty/cpp-httplib is "
        "empty. Run: git submodule update --init thirdparty/cpp-httplib")
endif()
add_library(cpp-httplib INTERFACE)
target_include_directories(cpp-httplib INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/cpp-httplib
    ${CMAKE_CURRENT_SOURCE_DIR}/mbedtls/include
)
target_compile_definitions(cpp-httplib INTERFACE
    CPPHTTPLIB_MBEDTLS_SUPPORT
    CPPHTTPLIB_NO_EXCEPTIONS
)
target_link_libraries(cpp-httplib INTERFACE mbedtls mbedx509 mbedcrypto)
if(WIN32)
    target_link_libraries(cpp-httplib INTERFACE crypt32 ws2_32)
endif()
if(APPLE)
    target_compile_definitions(cpp-httplib INTERFACE
        CPPHTTPLIB_USE_CERTS_FROM_MACOSX_KEYCHAIN
    )
    target_link_libraries(cpp-httplib INTERFACE
        "-framework Security" "-framework CoreFoundation"
    )
endif()
endif(ROCPROFVIS_ENABLE_AGENTIC_PROFILING)

# TEMPORARY (remote/SSH): libssh2 and its crypto-backend selection stay gated
# until remote graduates. mbedTLS itself is handled above, since the assistant
# needs it too.
if(ROCPROFVIS_ENABLE_REMOTE)

# mbedtls/libssh2 set BUILD_TESTING OFF; restore after so imgui_te is still built.
set(_prev_build_testing ${BUILD_TESTING})
set(_prev_build_examples ${BUILD_EXAMPLES})

set(ENABLE_ZLIB_COMPRESSION OFF)

if (CRYPTO_BACKEND STREQUAL "mbedTLS")

	unset(ROCPROFVIS_OPENSSL_VERSION_MAJOR CACHE)
	set(CRYPTO_BACKEND mbedTLS CACHE STRING "" FORCE)

    	set(ENABLE_MBEDTLS ON CACHE BOOL "" FORCE)
    	set(ENABLE_OPENSSL OFF CACHE BOOL "" FORCE)

	set(MBEDTLS_FOUND TRUE)
	set(MBEDTLS_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/thirdparty/mbedtls/include)
	set(MBEDCRYPTO_LIBRARY mbedcrypto)
	set(MBEDX509_LIBRARY mbedx509)
	set(MBEDTLS_LIBRARY mbedtls)

endif()

if (CRYPTO_BACKEND STREQUAL "OpenSSL")

	set(ENABLE_TESTING OFF CACHE BOOL "" FORCE)
	set(ENABLE_DOCUMENTATION OFF CACHE BOOL "" FORCE)
	set(BUILD_TESTING OFF)
	set(BUILD_EXAMPLES OFF)

    # Resolve OpenSSL as an external dependency (Vulkan-style), not vendored.
    # OPENSSL_ROOT_DIR may be supplied by the user/CI as a hint (e.g. macOS
    # Homebrew openssl@3, or a Windows OpenSSL install root); FindOpenSSL honors
    # it when set. We intentionally do NOT force it to a repo-local path.
    # Request both components explicitly: without this FindOpenSSL can report
    # success after locating only libcrypto, leaving OpenSSL::SSL uncreated.
    find_package(OpenSSL REQUIRED COMPONENTS SSL Crypto)

    # libssh2 (added below) runs its own find_package(OpenSSL) in its
    # subdirectory scope, which would otherwise leave OpenSSL::SSL/::Crypto
    # scoped below thirdparty and invisible to the root target that links them.
    # Promote the imported targets to GLOBAL so every scope sees the same ones.
    if(TARGET OpenSSL::SSL)
        set_target_properties(OpenSSL::SSL PROPERTIES IMPORTED_GLOBAL TRUE)
    endif()
    if(TARGET OpenSSL::Crypto)
        set_target_properties(OpenSSL::Crypto PROPERTIES IMPORTED_GLOBAL TRUE)
    endif()

    # Root CMakeLists Windows DLL staging needs the linked major; FindOpenSSL
    # sets OPENSSL_VERSION_MAJOR only in this directory scope.
    if(WIN32 AND DEFINED OPENSSL_VERSION_MAJOR AND NOT OPENSSL_VERSION_MAJOR STREQUAL "")
        set(ROCPROFVIS_OPENSSL_VERSION_MAJOR "${OPENSSL_VERSION_MAJOR}" CACHE INTERNAL
            "OpenSSL major for Windows libssl/libcrypto DLL staging glob")
    endif()

    	set(ENABLE_MBEDTLS OFF CACHE BOOL "" FORCE)
    	set(ENABLE_OPENSSL ON CACHE BOOL "" FORCE)

endif()


set(LIBSSH2_INSTALL OFF CACHE BOOL "" FORCE)
set(LIBSSH2_DISABLE_INSTALL ON CACHE BOOL "" FORCE)

add_subdirectory(libssh2)

target_include_directories(libssh2_static PUBLIC 
${CMAKE_SOURCE_DIR}/thirdparty/libssh2/include)

if (CRYPTO_BACKEND STREQUAL "mbedTLS")
	target_include_directories(libssh2_static PRIVATE
    		${CMAKE_SOURCE_DIR}/thirdparty/mbedtls/include
	)

	target_link_libraries(libssh2_static PRIVATE
    	mbedtls
    	mbedx509
    	mbedcrypto
	)
endif()

set(BUILD_TESTING ${_prev_build_testing})
set(BUILD_EXAMPLES ${_prev_build_examples})

endif() # ROCPROFVIS_ENABLE_REMOTE

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
