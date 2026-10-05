# Copyright Advanced Micro Devices, Inc.
# SPDX-License-Identifier: MIT

# Vendored CPython for ROCPROFVIS_ENABLE_SCRIPTING. Included from src/python
# only when ROCPROFVIS_VENDOR_PYTHON is on, so a build without scripting never
# downloads an interpreter.
#
# Defines RocProfVis::Python (headers, import library, shared runtime) and:
#   rocprofvis_python_vendor_deploy(<target>)
#       copies the runtime beside a build-tree executable
#   rocprofvis_python_vendor_install(<target> <component>)
#       installs the runtime and points the packaged binary at it
#
# The runtime looks for the copy relative to the executable (see
# resolve_layout in rocprofvis_python_runtime.cpp), so the two must agree.

include(FetchContent)
include(GNUInstallDirs)

# The embed uses the full C API rather than the stable ABI, so the headers,
# libpython and the standard library all have to come from this one build.
set(ROCPROFVIS_PYTHON_VENDOR_MINOR "3.12")
set(_rpv_version "3.12.14")
set(_rpv_release "20260924")
set(_rpv_base_url
    "https://github.com/astral-sh/python-build-standalone/releases/download/${_rpv_release}")

set(ROCPROFVIS_PYTHON_VENDOR_URL "" CACHE STRING
    "CPython archive to vendor instead of the pinned python-build-standalone \
release (URL or local path, install_only layout). Requires ROCPROFVIS_PYTHON_VENDOR_SHA256.")
set(ROCPROFVIS_PYTHON_VENDOR_SHA256 "" CACHE STRING
    "SHA256 of ROCPROFVIS_PYTHON_VENDOR_URL.")

set(_rpv_triple "")
set(_rpv_sha256 "")
if(WIN32)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8 AND NOT CMAKE_CXX_COMPILER_ARCHITECTURE_ID MATCHES "ARM")
        set(_rpv_triple "x86_64-pc-windows-msvc")
        set(_rpv_sha256 "c5bf8edfe858c1df9891be498b5bbc8761d383df5b9790658b088fea4870433a")
    endif()
elseif(APPLE)
    set(_rpv_arch "${CMAKE_SYSTEM_PROCESSOR}")
    if(CMAKE_OSX_ARCHITECTURES)
        set(_rpv_arch "${CMAKE_OSX_ARCHITECTURES}")
    endif()
    if(_rpv_arch STREQUAL "arm64")
        set(_rpv_triple "aarch64-apple-darwin")
        set(_rpv_sha256 "c2edb321cd32ec2b170df208db0446dccc4398db602ca27cf2079098fb1f7d9d")
    elseif(_rpv_arch STREQUAL "x86_64")
        set(_rpv_triple "x86_64-apple-darwin")
        set(_rpv_sha256 "7ea9761b9069c10b9a20531d568645849d604c59e9c7f11f6659f1e1790c968e")
    endif()
else()
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64)$")
        set(_rpv_triple "x86_64-unknown-linux-gnu")
        set(_rpv_sha256 "269b2c99e4db15b242bf01832f4fea1e8f1a664f273cff519393f296e9820b41")
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)$")
        set(_rpv_triple "aarch64-unknown-linux-gnu")
        set(_rpv_sha256 "c8499b61252c433280f134df954464d19811527b31cb920c35fc6967c1222e35")
    endif()
endif()

if(ROCPROFVIS_PYTHON_VENDOR_URL)
    if(NOT ROCPROFVIS_PYTHON_VENDOR_SHA256)
        message(FATAL_ERROR
            "ROCPROFVIS_PYTHON_VENDOR_URL is set without ROCPROFVIS_PYTHON_VENDOR_SHA256.")
    endif()
    set(_rpv_url "${ROCPROFVIS_PYTHON_VENDOR_URL}")
    set(_rpv_sha256 "${ROCPROFVIS_PYTHON_VENDOR_SHA256}")
elseif(_rpv_triple)
    set(_rpv_url
        "${_rpv_base_url}/cpython-${_rpv_version}%2B${_rpv_release}-${_rpv_triple}-install_only_stripped.tar.gz")
else()
    message(FATAL_ERROR
        "No vendored CPython is pinned for this platform. Point "
        "ROCPROFVIS_PYTHON_VENDOR_URL and ROCPROFVIS_PYTHON_VENDOR_SHA256 at a "
        "python-build-standalone ${ROCPROFVIS_PYTHON_VENDOR_MINOR} install_only "
        "archive, or set ROCPROFVIS_VENDOR_PYTHON=OFF to build against this "
        "machine's Python.")
endif()

set(_rpv_fetch_args URL "${_rpv_url}" URL_HASH "SHA256=${_rpv_sha256}")
# The Windows archive ships __pycache__, which stays valid only while every .py
# keeps the timestamp it was compiled against.
if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.24)
    list(APPEND _rpv_fetch_args DOWNLOAD_EXTRACT_TIMESTAMP FALSE)
endif()
message(STATUS "Vendoring CPython from ${_rpv_url}")
FetchContent_Declare(rocprofvis_python ${_rpv_fetch_args})
FetchContent_MakeAvailable(rocprofvis_python)
set(_rpv_root "${rocprofvis_python_SOURCE_DIR}")

string(REPLACE "." "" _rpv_nodot "${ROCPROFVIS_PYTHON_VENDOR_MINOR}")
add_library(RocProfVis::Python SHARED IMPORTED GLOBAL)
if(WIN32)
    set_target_properties(RocProfVis::Python PROPERTIES
        IMPORTED_LOCATION "${_rpv_root}/python${_rpv_nodot}.dll"
        IMPORTED_IMPLIB "${_rpv_root}/libs/python${_rpv_nodot}.lib"
        INTERFACE_INCLUDE_DIRECTORIES "${_rpv_root}/include")
elseif(APPLE)
    set_target_properties(RocProfVis::Python PROPERTIES
        IMPORTED_LOCATION "${_rpv_root}/lib/libpython${ROCPROFVIS_PYTHON_VENDOR_MINOR}.dylib"
        IMPORTED_SONAME "@rpath/libpython${ROCPROFVIS_PYTHON_VENDOR_MINOR}.dylib"
        INTERFACE_INCLUDE_DIRECTORIES "${_rpv_root}/include/python${ROCPROFVIS_PYTHON_VENDOR_MINOR}")
else()
    set_target_properties(RocProfVis::Python PROPERTIES
        IMPORTED_LOCATION "${_rpv_root}/lib/libpython${ROCPROFVIS_PYTHON_VENDOR_MINOR}.so.1.0"
        IMPORTED_SONAME "libpython${ROCPROFVIS_PYTHON_VENDOR_MINOR}.so.1.0"
        INTERFACE_INCLUDE_DIRECTORIES "${_rpv_root}/include/python${ROCPROFVIS_PYTHON_VENDOR_MINOR}")
endif()

# Where a package keeps the runtime, and the same place relative to the
# installed executable. Windows keeps it beside roc-optiq.exe, which the
# runtime already checks first.
set(_rpv_install_dir "")
set(_rpv_reldir "")
if(WIN32)
    set(_rpv_install_dir "${CMAKE_INSTALL_BINDIR}/python")
elseif(APPLE)
    set(_rpv_reldir "../Resources/python")
else()
    set(_rpv_install_dir "${CMAKE_INSTALL_LIBDIR}/roc-optiq/python")
    file(RELATIVE_PATH _rpv_reldir
        "${CMAKE_INSTALL_FULL_BINDIR}"
        "${CMAKE_INSTALL_FULL_LIBDIR}/roc-optiq/python")
endif()

# The trimmed tree that both the build tree and the package copy. Dropped:
# pip and the packaging tools (sys.path never names site-packages), Tk and
# IDLE, the build config, and CPython's own test extension modules. Also
# _dbm, which links Berkeley DB under the Sleepycat license (that obliges
# shipping source), and _crypt, which links the system libcrypt. Scripts
# cannot import either one.
set(_rpv_stage "${CMAKE_BINARY_DIR}/python_vendor")
set(_rpv_excludes
    PATTERN "site-packages" EXCLUDE
    PATTERN "ensurepip" EXCLUDE
    PATTERN "venv" EXCLUDE
    PATTERN "idlelib" EXCLUDE
    PATTERN "tkinter" EXCLUDE
    PATTERN "turtledemo" EXCLUDE
    PATTERN "lib2to3" EXCLUDE
    PATTERN "pydoc_data" EXCLUDE
    PATTERN "test" EXCLUDE
    PATTERN "config-*" EXCLUDE
    PATTERN "_tkinter*" EXCLUDE
    PATTERN "_test*" EXCLUDE
    PATTERN "_ctypes_test*" EXCLUDE
    PATTERN "_dbm*" EXCLUDE
    PATTERN "_crypt*" EXCLUDE
    PATTERN "tcl*.dll" EXCLUDE
    PATTERN "tk*.dll" EXCLUDE)

# Restaged whenever the archive or these rules change, so nothing from an
# older interpreter lingers in the copy.
file(SHA256 "${CMAKE_CURRENT_LIST_FILE}" _rpv_rules_hash)
set(_rpv_stamp "${CMAKE_BINARY_DIR}/python_vendor.stamp")
set(_rpv_stamp_value "${_rpv_sha256} ${_rpv_rules_hash}")
set(_rpv_stamped "")
if(EXISTS "${_rpv_stamp}")
    file(READ "${_rpv_stamp}" _rpv_stamped)
endif()
if(NOT _rpv_stamped STREQUAL _rpv_stamp_value)
    file(REMOVE_RECURSE "${_rpv_stage}")
    if(WIN32)
        file(COPY "${_rpv_root}/Lib" "${_rpv_root}/DLLs" "${_rpv_root}/LICENSE.txt"
            DESTINATION "${_rpv_stage}" ${_rpv_excludes})
    else()
        file(COPY "${_rpv_root}/lib/python${ROCPROFVIS_PYTHON_VENDOR_MINOR}"
            DESTINATION "${_rpv_stage}/lib" ${_rpv_excludes})
        if(NOT APPLE)
            file(COPY "${_rpv_root}/lib/libpython${ROCPROFVIS_PYTHON_VENDOR_MINOR}.so.1.0"
                DESTINATION "${_rpv_stage}/lib")
        endif()
    endif()
    file(WRITE "${_rpv_stamp}" "${_rpv_stamp_value}")
endif()

if(APPLE)
    # The bundle carries this tree under Contents/Resources, where
    # codesign --deep signs nothing, and notarization rejects an unsigned
    # Mach-O anywhere in the bundle.
    file(GLOB_RECURSE _rpv_macho "${_rpv_stage}/*.so" "${_rpv_stage}/*.dylib")
    if(_rpv_macho)
        list(JOIN _rpv_macho "\n  " _rpv_macho_list)
        message(FATAL_ERROR
            "The vendored CPython stage holds Mach-O files that the bundle "
            "signing step does not sign:\n  ${_rpv_macho_list}\n"
            "Exclude them in ${CMAKE_CURRENT_LIST_FILE}, or sign them before "
            "the bundle is codesigned.")
    endif()
endif()

if(WIN32)
    set(_rpv_license "${_rpv_stage}/LICENSE.txt")
else()
    set(_rpv_license "${_rpv_stage}/lib/python${ROCPROFVIS_PYTHON_VENDOR_MINOR}/LICENSE.txt")
endif()

set_target_properties(RocProfVis::Python PROPERTIES
    ROCPROFVIS_PYTHON_STAGE_DIR "${_rpv_stage}"
    ROCPROFVIS_PYTHON_INSTALL_DIR "${_rpv_install_dir}"
    ROCPROFVIS_PYTHON_RELDIR "${_rpv_reldir}"
    ROCPROFVIS_PYTHON_LICENSE "${_rpv_license}")

# Copies the runtime to where a build-tree executable looks for it:
# <exe dir>/python, or Contents/Resources/python with libpython in
# Contents/Frameworks for a macOS bundle.
function(rocprofvis_python_vendor_deploy target)
    get_target_property(_stage RocProfVis::Python ROCPROFVIS_PYTHON_STAGE_DIR)
    get_target_property(_license RocProfVis::Python ROCPROFVIS_PYTHON_LICENSE)
    set(_copy copy_directory)
    if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.26)
        set(_copy copy_directory_if_different)
    endif()
    get_target_property(_bundle ${target} MACOSX_BUNDLE)
    if(APPLE AND _bundle)
        set(_contents "$<TARGET_BUNDLE_DIR:${target}>/Contents")
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory
                "${_contents}/Frameworks" "${_contents}/Resources/Licenses"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:RocProfVis::Python>" "${_contents}/Frameworks"
            COMMAND ${CMAKE_COMMAND} -E ${_copy} "${_stage}" "${_contents}/Resources/python"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${_license}" "${_contents}/Resources/Licenses/Python-LICENSE.txt"
            COMMENT "Staging the vendored Python runtime and license into the app bundle"
            VERBATIM)
    else()
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E ${_copy} "${_stage}" "$<TARGET_FILE_DIR:${target}>/python"
            COMMENT "Staging the vendored Python runtime beside ${target}"
            VERBATIM)
    endif()
endfunction()

# Installs the runtime where a packaged executable looks for it. The macOS
# bundle already carries it from rocprofvis_python_vendor_deploy.
function(rocprofvis_python_vendor_install target component)
    get_target_property(_stage RocProfVis::Python ROCPROFVIS_PYTHON_STAGE_DIR)
    get_target_property(_install_dir RocProfVis::Python ROCPROFVIS_PYTHON_INSTALL_DIR)
    get_target_property(_reldir RocProfVis::Python ROCPROFVIS_PYTHON_RELDIR)
    get_target_property(_license RocProfVis::Python ROCPROFVIS_PYTHON_LICENSE)
    if(NOT APPLE)
        install(FILES "${_license}" DESTINATION "${CMAKE_INSTALL_DOCDIR}"
            RENAME Python-LICENSE.txt COMPONENT ${component})
    endif()
    if(WIN32)
        install(DIRECTORY "${_stage}/" DESTINATION "${_install_dir}"
            COMPONENT ${component})
        install(FILES "$<TARGET_FILE:RocProfVis::Python>"
            DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT ${component})
    elseif(NOT APPLE)
        install(DIRECTORY "${_stage}/" DESTINATION "${_install_dir}"
            COMPONENT ${component} USE_SOURCE_PERMISSIONS)
        # The package ships the build-tree binary as it is, so that one binary
        # needs both runpaths: the build-tree copy and the packaged one.
        set_target_properties(${target} PROPERTIES
            BUILD_WITH_INSTALL_RPATH TRUE
            INSTALL_RPATH "$ORIGIN/python/lib;$ORIGIN/${_reldir}/lib")
    endif()
endfunction()
