# Copyright Advanced Micro Devices, Inc.
# SPDX-License-Identifier: MIT

add_subdirectory(spdlog)
add_subdirectory(catch2)

set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "Disable yaml-cpp tests")
set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "Disable yaml-cpp tools")
add_subdirectory(yaml-cpp)

# TEMPORARY (remote/SSH): libssh2's TLS backend. CRYPTO_BACKEND selects it.
# Default is vendored mbedTLS so an out-of-the-box build needs no system
# OpenSSL. OpenSSL is opt-in via -DCRYPTO_BACKEND=OpenSSL and is resolved
# with find_package, not vendored. mbedTLS is added once, and only when
# that backend is selected and remote is on.
set(CRYPTO_BACKEND "mbedTLS" CACHE STRING "Crypto backend (OpenSSL or mbedTLS)")

set(ROCPROFVIS_USES_CRYPTO OFF)
if(ROCPROFVIS_ENABLE_REMOTE)
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

# Resolve OpenSSL once, in this directory, before libssh2. libssh2 runs its
# own find_package in a subdirectory; promoting the imported targets to GLOBAL
# keeps that lookup and the root targets on the same targets. OPENSSL_ROOT_DIR
# may be supplied by the user or CI as a hint (Homebrew openssl@3, a Windows
# install root). FindOpenSSL honors it. Request both components: without them
# FindOpenSSL can succeed after locating only libcrypto, leaving OpenSSL::SSL
# uncreated.
if(ROCPROFVIS_USES_CRYPTO AND CRYPTO_BACKEND STREQUAL "OpenSSL")
    find_package(OpenSSL REQUIRED COMPONENTS SSL Crypto)

    if(TARGET OpenSSL::SSL)
        set_target_properties(OpenSSL::SSL PROPERTIES IMPORTED_GLOBAL TRUE)
    endif()
    if(TARGET OpenSSL::Crypto)
        set_target_properties(OpenSSL::Crypto PROPERTIES IMPORTED_GLOBAL TRUE)
    endif()
endif()

# cpp-httplib (MIT), a submodule pinned to v0.53.1, serves the trace server's
# HTTP API. The server speaks plain HTTP, so no TLS backend is compiled in.
# Upstream keeps httplib.h at its repo root and we compile it header-only, so
# its own CMakeLists is deliberately not added.
if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/cpp-httplib/httplib.h)
    message(FATAL_ERROR
        "thirdparty/cpp-httplib is empty. "
        "Run: git submodule update --init thirdparty/cpp-httplib")
endif()
find_package(Threads REQUIRED)
add_library(cpp-httplib INTERFACE)
target_include_directories(cpp-httplib SYSTEM INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/cpp-httplib
)
target_compile_definitions(cpp-httplib INTERFACE
    CPPHTTPLIB_NO_EXCEPTIONS
)
target_link_libraries(cpp-httplib INTERFACE Threads::Threads)
if(WIN32)
    target_link_libraries(cpp-httplib INTERFACE ws2_32)
endif()

# TEMPORARY (remote/SSH): libssh2 and its crypto-backend selection stay gated
# until remote graduates. The TLS library itself is resolved above.
if(ROCPROFVIS_ENABLE_REMOTE)

# mbedtls/libssh2 set BUILD_TESTING OFF; restore after so the test tree is still built.
set(_prev_build_testing ${BUILD_TESTING})
set(_prev_build_examples ${BUILD_EXAMPLES})

if (CRYPTO_BACKEND STREQUAL "mbedTLS")

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

	# OpenSSL itself was resolved above.
    	set(ENABLE_MBEDTLS OFF CACHE BOOL "" FORCE)
    	set(ENABLE_OPENSSL ON CACHE BOOL "" FORCE)

endif()

# Vendored zlib for SSH transport compression. Static only, and libssh2's
# find_package(ZLIB) is pointed at it so no system zlib is required.
set(ENABLE_ZLIB_COMPRESSION ON CACHE BOOL "" FORCE)
set(ZLIB_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(ZLIB_BUILD_STATIC ON CACHE BOOL "" FORCE)
set(ZLIB_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(ZLIB_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(zlib)

add_library(ZLIB::ZLIB ALIAS zlibstatic)
set(ZLIB_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/zlib CACHE PATH "" FORCE)
set(ZLIB_LIBRARY zlibstatic CACHE STRING "" FORCE)

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
