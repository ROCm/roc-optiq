cmake_minimum_required(VERSION 3.15)

set(PERFETTO_PREBUILT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/..)
set(DOCKERFILE            ${CMAKE_CURRENT_SOURCE_DIR}/Dockerfile.perfetto)

# Short path for Windows builds — avoids 260 char path limit
set(PERFETTO_PREBUILD_DIR C:/pf)
set(PERFETTO_SRC_DIR      ${PERFETTO_PREBUILD_DIR}/perfetto)

# Windows target
set(PERFETTO_OUT_DEBUG      ${PERFETTO_SRC_DIR}/out/debug)
set(PERFETTO_OUT_RELEASE    ${PERFETTO_SRC_DIR}/out/release)
set(PERFETTO_DEST_WIN_DEBUG   ${PERFETTO_PREBUILT_DIR}/lib/windows/debug)
set(PERFETTO_DEST_WIN_RELEASE ${PERFETTO_PREBUILT_DIR}/lib/windows/release)

add_custom_target(build_perfetto_libs_windows
    # Create directories
    COMMAND ${CMAKE_COMMAND} -E make_directory ${PERFETTO_PREBUILD_DIR}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${PERFETTO_DEST_WIN_DEBUG}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${PERFETTO_DEST_WIN_RELEASE}

    # Clone only if not already present
    COMMAND ${CMAKE_COMMAND} -E chdir ${PERFETTO_PREBUILD_DIR}
        cmd /c "if not exist perfetto git clone https://github.com/google/perfetto"

    # Checkout version
    COMMAND ${CMAKE_COMMAND} -E chdir ${PERFETTO_SRC_DIR}
        git checkout v47.0

    # Fetch dependencies
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

    # Build release
    COMMAND ${CMAKE_COMMAND} -E chdir ${PERFETTO_SRC_DIR}
        third_party/ninja/ninja.exe -C out/release
            src/trace_processor:trace_processor

    # Build debug
    COMMAND ${CMAKE_COMMAND} -E chdir ${PERFETTO_SRC_DIR}
        third_party/ninja/ninja.exe -C out/debug
            src/trace_processor:trace_processor

    # Copy artifacts to prebuilt directory
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${PERFETTO_OUT_RELEASE}/trace_processor.lib
        ${PERFETTO_DEST_WIN_RELEASE}/trace_processor.lib
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${PERFETTO_OUT_DEBUG}/trace_processor.lib
        ${PERFETTO_DEST_WIN_DEBUG}/trace_processor.lib

    # Copy headers — only needed once, same for both platforms
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${PERFETTO_SRC_DIR}/include/perfetto
        ${PERFETTO_PREBUILT_DIR}/include/perfetto
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${PERFETTO_OUT_RELEASE}/gen/build_config/perfetto_build_flags.h
        ${PERFETTO_PREBUILT_DIR}/gen/build_config/perfetto_build_flags.h

    COMMENT "Building Perfetto trace processor libraries for Windows"
    VERBATIM
)

#Linux target (via Docker — runs on any host with Docker installed)
set(PERFETTO_DEST_LINUX_RELEASE ${PERFETTO_PREBUILT_DIR}/lib/linux/release)
set(PERFETTO_DOCKER_OUTPUT      ${PERFETTO_PREBUILD_DIR}/docker_output)

add_custom_target(build_perfetto_libs_linux
    # Create directories
    COMMAND ${CMAKE_COMMAND} -E make_directory ${PERFETTO_PREBUILD_DIR}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${PERFETTO_DEST_LINUX_RELEASE}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${PERFETTO_DOCKER_OUTPUT}

    # Build Docker image
    COMMAND docker build
        -f ${DOCKERFILE}
        -t perfetto-builder
        ${CMAKE_CURRENT_SOURCE_DIR}

    # Run container — artifacts go to docker_output
    COMMAND docker run --rm
        -v ${PERFETTO_DOCKER_OUTPUT}:/output
        perfetto-builder

    # Copy library
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${PERFETTO_DOCKER_OUTPUT}/lib/libtrace_processor.a
        ${PERFETTO_DEST_LINUX_RELEASE}/libtrace_processor.a

    # Copy headers (may already exist from Windows build — copy_directory is safe)
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${PERFETTO_DOCKER_OUTPUT}/include/perfetto
        ${PERFETTO_PREBUILT_DIR}/include/perfetto
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${PERFETTO_DOCKER_OUTPUT}/gen/build_config/perfetto_build_flags.h
        ${PERFETTO_PREBUILT_DIR}/gen/build_config/perfetto_build_flags.h

    COMMENT "Building Perfetto trace processor library for Linux via Docker"
    VERBATIM
)

#Convenience target — builds both
add_custom_target(build_perfetto_libs
    DEPENDS
        build_perfetto_libs_windows
        build_perfetto_libs_linux
    COMMENT "Building Perfetto trace processor libraries for all platforms"
)

#Disable target - removes prebuilt artifacts to disable Perfetto
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