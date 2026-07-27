cmake_minimum_required(VERSION 3.15)

set(PERFETTO_PREBUILT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/..)
set(DOCKERFILE            ${CMAKE_CURRENT_SOURCE_DIR}/Dockerfile.perfetto)

#Platform-specific build paths
if(WIN32)
    # Short path avoids Windows 260 char path limit
    set(PERFETTO_PREBUILD_DIR C:/pf)
else()
    set(PERFETTO_PREBUILD_DIR ${CMAKE_SOURCE_DIR}/thirdparty/prebuild_lib)
endif()

set(PERFETTO_SRC_DIR ${PERFETTO_PREBUILD_DIR}/perfetto)

#Windows target
if(WIN32)

set(PERFETTO_OUT_DEBUG        ${PERFETTO_SRC_DIR}/out/debug)
set(PERFETTO_OUT_RELEASE      ${PERFETTO_SRC_DIR}/out/release)
set(PERFETTO_DEST_WIN_DEBUG   ${PERFETTO_PREBUILT_DIR}/lib/windows/debug)
set(PERFETTO_DEST_WIN_RELEASE ${PERFETTO_PREBUILT_DIR}/lib/windows/release)

add_custom_target(build_perfetto_libs_windows
    COMMAND ${CMAKE_COMMAND} -E make_directory ${PERFETTO_PREBUILD_DIR}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${PERFETTO_DEST_WIN_DEBUG}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${PERFETTO_DEST_WIN_RELEASE}

    COMMAND ${CMAKE_COMMAND} -E chdir ${PERFETTO_PREBUILD_DIR}
        cmd /c "if not exist perfetto git clone https://github.com/google/perfetto"
    COMMAND ${CMAKE_COMMAND} -E chdir ${PERFETTO_SRC_DIR}
        git checkout v47.0
    COMMAND ${CMAKE_COMMAND} -E chdir ${PERFETTO_SRC_DIR}
        python tools\\install-build-deps

    # Generate release
    COMMAND ${CMAKE_COMMAND} -E chdir ${PERFETTO_SRC_DIR}
        third_party/gn/gn.exe gen out/release --root=.
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/args_release.gn
        ${PERFETTO_SRC_DIR}/out/release/args.gn
    COMMAND ${CMAKE_COMMAND} -E chdir ${PERFETTO_SRC_DIR}
        third_party/gn/gn.exe gen out/release --root=.

    # Generate debug
    COMMAND ${CMAKE_COMMAND} -E chdir ${PERFETTO_SRC_DIR}
        third_party/gn/gn.exe gen out/debug --root=.
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/args_debug.gn
        ${PERFETTO_SRC_DIR}/out/debug/args.gn
    COMMAND ${CMAKE_COMMAND} -E chdir ${PERFETTO_SRC_DIR}
        third_party/gn/gn.exe gen out/debug --root=.

    # Build
    COMMAND ${CMAKE_COMMAND} -E chdir ${PERFETTO_SRC_DIR}
        third_party/ninja/ninja.exe -C out/release
            src/trace_processor:trace_processor
    COMMAND ${CMAKE_COMMAND} -E chdir ${PERFETTO_SRC_DIR}
        third_party/ninja/ninja.exe -C out/debug
            src/trace_processor:trace_processor

    # Copy artifacts
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${PERFETTO_OUT_RELEASE}/trace_processor.lib
        ${PERFETTO_DEST_WIN_RELEASE}/trace_processor.lib
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${PERFETTO_OUT_DEBUG}/trace_processor.lib
        ${PERFETTO_DEST_WIN_DEBUG}/trace_processor.lib
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${PERFETTO_SRC_DIR}/include/perfetto
        ${PERFETTO_PREBUILT_DIR}/include/perfetto
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${PERFETTO_OUT_RELEASE}/gen/build_config/perfetto_build_flags.h
        ${PERFETTO_PREBUILT_DIR}/gen/build_config/perfetto_build_flags.h

    COMMENT "Building Perfetto trace processor libraries for Windows"
    VERBATIM
)

#Linux via Docker — only available on Windows
set(PERFETTO_DEST_LINUX_RELEASE ${PERFETTO_PREBUILT_DIR}/lib/linux/release)
set(PERFETTO_DOCKER_OUTPUT      ${PERFETTO_PREBUILD_DIR}/docker_output)

add_custom_target(build_perfetto_libs_linux
    COMMAND ${CMAKE_COMMAND} -E make_directory ${PERFETTO_PREBUILD_DIR}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${PERFETTO_DEST_LINUX_RELEASE}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${PERFETTO_DOCKER_OUTPUT}

    COMMAND docker build
        -f ${DOCKERFILE}
        -t perfetto-builder
        ${CMAKE_CURRENT_SOURCE_DIR}
    COMMAND docker run --rm
        -v ${PERFETTO_DOCKER_OUTPUT}:/output
        perfetto-builder

    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${PERFETTO_DOCKER_OUTPUT}/lib/libtrace_processor.a
        ${PERFETTO_DEST_LINUX_RELEASE}/libtrace_processor.a
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${PERFETTO_DOCKER_OUTPUT}/include/perfetto
        ${PERFETTO_PREBUILT_DIR}/include/perfetto
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${PERFETTO_DOCKER_OUTPUT}/gen/build_config/perfetto_build_flags.h
        ${PERFETTO_PREBUILT_DIR}/gen/build_config/perfetto_build_flags.h

    COMMENT "Building Perfetto trace processor library for Linux via Docker"
    VERBATIM
)

# Convenience target — builds both Windows and Linux (via Docker)
add_custom_target(build_perfetto_libs
    DEPENDS
        build_perfetto_libs_windows
        build_perfetto_libs_linux
    COMMENT "Building Perfetto trace processor libraries for all platforms"
)

#Linux native target
else() # UNIX

set(PERFETTO_OUT_RELEASE      ${PERFETTO_SRC_DIR}/out/release)
set(PERFETTO_DEST_LINUX_RELEASE ${PERFETTO_PREBUILT_DIR}/lib/linux/release)

add_custom_target(build_perfetto_libs_linux
    COMMAND ${CMAKE_COMMAND} -E make_directory ${PERFETTO_PREBUILD_DIR}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${PERFETTO_DEST_LINUX_RELEASE}

    # Clone only if not already present
    COMMAND bash -c
        "[ -d '${PERFETTO_SRC_DIR}/.git' ] || \
         git -C '${PERFETTO_PREBUILD_DIR}' clone \
         https://github.com/google/perfetto"
    COMMAND ${CMAKE_COMMAND} -E chdir ${PERFETTO_SRC_DIR}
        git checkout v47.0

    # Fetch dependencies — installs gn and ninja into third_party/
    COMMAND ${CMAKE_COMMAND} -E chdir ${PERFETTO_SRC_DIR}
        python3 tools/install-build-deps

    # Generate release
    COMMAND ${CMAKE_COMMAND} -E chdir ${PERFETTO_SRC_DIR}
        third_party/gn/gn gen out/release --root=.
            --args=is_debug=false
    COMMAND ${CMAKE_COMMAND} -E chdir ${PERFETTO_SRC_DIR}
        third_party/ninja/ninja -C out/release
            src/trace_processor:trace_processor

    # Copy artifacts
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${PERFETTO_OUT_RELEASE}/libtrace_processor.a
        ${PERFETTO_DEST_LINUX_RELEASE}/libtrace_processor.a
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${PERFETTO_SRC_DIR}/include/perfetto
        ${PERFETTO_PREBUILT_DIR}/include/perfetto
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${PERFETTO_OUT_RELEASE}/gen/build_config/perfetto_build_flags.h
        ${PERFETTO_PREBUILT_DIR}/gen/build_config/perfetto_build_flags.h

    COMMENT "Building Perfetto trace processor library natively on Linux"
    VERBATIM
)

# On Linux, build_perfetto_libs only builds Linux
add_custom_target(build_perfetto_libs
    DEPENDS build_perfetto_libs_linux
    COMMENT "Building Perfetto trace processor library for Linux"
)

# Stub target so build_perfetto_libs_windows can still be referenced on Linux
add_custom_target(build_perfetto_libs_windows
    COMMAND ${CMAKE_COMMAND} -E echo
        "build_perfetto_libs_windows is only available on Windows"
    COMMENT "Windows build not available on Linux"
)

endif() # WIN32 / UNIX

#Disable target
add_custom_target(disable_perfetto
    COMMAND ${CMAKE_COMMAND} -E remove_directory
        ${PERFETTO_PREBUILT_DIR}/include
    COMMAND ${CMAKE_COMMAND} -E remove_directory
        ${PERFETTO_PREBUILT_DIR}/gen
    COMMAND ${CMAKE_COMMAND} -E remove_directory
        ${PERFETTO_PREBUILT_DIR}/lib
    COMMAND ${CMAKE_COMMAND} -E echo
        "Perfetto artifacts removed. Re-run cmake to disable the feature."
    COMMENT "Disabling Perfetto — removing prebuilt libraries and headers"
    VERBATIM
)