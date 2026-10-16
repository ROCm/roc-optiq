# Copyright Advanced Micro Devices, Inc.
# SPDX-License-Identifier: MIT

find_package(Vulkan REQUIRED)
set(VULKAN_LIB "Vulkan::Vulkan")

option(PROFILER_HUB_BUILD_ROC_OPTIQ_IFACE_LIB "Build RocOptiq interface library" OFF)

if(PROFILER_HUB_BUILD_ROC_OPTIQ_IFACE_LIB)
    message(
        STATUS
        "Build RocOptiq interface library"
    )

set(PROFILER_HUB_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(PROFILER_HUB_BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)
add_subdirectory(profiler-hub)
add_subdirectory(profiler-hub-client/optiq)
set(PROFILER_HUB_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/profiler-hub/include)
endif()
add_subdirectory(spdlog)
add_subdirectory(catch2)

set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "Disable yaml-cpp tests")
set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "Disable yaml-cpp tools")
add_subdirectory(yaml-cpp)

# TEMPORARY (remote/SSH + agentic profiling): libssh2 and the Ask Optiq
# HTTPS client share one TLS backend. CRYPTO_BACKEND selects it for both.
# Default is vendored mbedTLS so an out-of-the-box build needs no system
# OpenSSL. OpenSSL is opt-in via -DCRYPTO_BACKEND=OpenSSL and is resolved
# with find_package, not vendored. mbedTLS is added once, and only when
# that backend is selected and at least one of the two features is on.
set(CRYPTO_BACKEND "mbedTLS" CACHE STRING "Crypto backend (OpenSSL or mbedTLS)")

set(ROCPROFVIS_USES_CRYPTO OFF)
if(ROCPROFVIS_ENABLE_REMOTE OR ROCPROFVIS_ENABLE_AGENTIC_PROFILING)
    set(ROCPROFVIS_USES_CRYPTO ON)
endif()

if(ROCPROFVIS_USES_CRYPTO
    AND NOT CRYPTO_BACKEND STREQUAL "mbedTLS"
    AND NOT CRYPTO_BACKEND STREQUAL "OpenSSL")
    message(FATAL_ERROR
        "CRYPTO_BACKEND must be \"mbedTLS\" or \"OpenSSL\" "
        "(got \"${CRYPTO_BACKEND}\").")
endif()

set(ROCPROFVIS_NEED_MBEDTLS OFF)
if(ROCPROFVIS_USES_CRYPTO AND CRYPTO_BACKEND STREQUAL "mbedTLS")
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

# Resolve OpenSSL once, in this directory, before cpp-httplib and libssh2.
# libssh2 runs its own find_package in a subdirectory; promoting the imported
# targets to GLOBAL keeps that lookup and the root executable on the same
# targets. OPENSSL_ROOT_DIR may be supplied by the user or CI as a hint
# (Homebrew openssl@3, a Windows install root). FindOpenSSL honors it.
# Request both components: without them FindOpenSSL can succeed after
# locating only libcrypto, leaving OpenSSL::SSL uncreated.
if(ROCPROFVIS_USES_CRYPTO AND CRYPTO_BACKEND STREQUAL "OpenSSL")
    find_package(OpenSSL REQUIRED COMPONENTS SSL Crypto)

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
endif()

# TEMPORARY (agentic profiling): cpp-httplib (MIT), the in-process HTTPS
# client, a submodule pinned to v0.53.1. TLS uses CRYPTO_BACKEND, the same
# choice as libssh2, so the GUI never shells out to curl.exe (that flashes
# a console on Windows and takes the request down with it). Upstream keeps
# httplib.h at its repo root and we compile it header-only, so its own
# CMakeLists is deliberately not added.
if(ROCPROFVIS_ENABLE_AGENTIC_PROFILING)
if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/cpp-httplib/httplib.h)
    message(FATAL_ERROR
        "ROCPROFVIS_ENABLE_AGENTIC_PROFILING is ON but thirdparty/cpp-httplib is "
        "empty. Run: git submodule update --init thirdparty/cpp-httplib")
endif()
add_library(cpp-httplib INTERFACE)
target_include_directories(cpp-httplib INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/cpp-httplib
)
target_compile_definitions(cpp-httplib INTERFACE
    CPPHTTPLIB_NO_EXCEPTIONS
)
if(CRYPTO_BACKEND STREQUAL "OpenSSL")
    target_compile_definitions(cpp-httplib INTERFACE
        CPPHTTPLIB_OPENSSL_SUPPORT
    )
    target_link_libraries(cpp-httplib INTERFACE OpenSSL::SSL OpenSSL::Crypto)
else()
    target_include_directories(cpp-httplib INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/mbedtls/include
    )
    target_compile_definitions(cpp-httplib INTERFACE
        CPPHTTPLIB_MBEDTLS_SUPPORT
    )
    target_link_libraries(cpp-httplib INTERFACE mbedtls mbedx509 mbedcrypto)
endif()
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
# until remote graduates. The TLS library itself is resolved above and shared
# with the Ask Optiq client.
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

	# OpenSSL itself was resolved above, shared with cpp-httplib.
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
