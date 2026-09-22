cmake_minimum_required(VERSION 3.16)

project(optiq_interface VERSION 1.0.0 LANGUAGES CXX)

add_library(optiq_interface STATIC
    trace_context.cpp
)

target_include_directories(optiq_interface
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        # profiler-hub's build-tree package config exposes its build/include
        # dir (containing a nested profiler-hub/ subfolder,
        # e.g. build/include/profiler-hub/c_interface/profiler_hub.h) via the
        # imported target below; add the nested dir directly so #include
        # "c_interface/profiler_hub.h" resolves the same way it did against
        # the source tree's flat include/c_interface/ layout.
        ${PROFILER_HUB_PREBUILT_DIR}/include/profiler-hub
)

# profiler-hub is linked as a prebuilt shared library (see
# thirdparty/CMakeLists.txt's find_package(profiler-hub ...)), not compiled
# from source.
target_link_libraries(optiq_interface PUBLIC profiler-hub::profiler-hub)

# The project sets CMAKE_SKIP_RPATH TRUE globally (see root CMakeLists.txt,
# needed for RPM packaging), which disables CMake's automatic build-tree
# RPATH - without it, executables silently fall back to whatever
# libprofiler-hub.so happens to be on the system's default library search
# path (e.g. a ROCm dev environment's LD_LIBRARY_PATH pointing at an
# unrelated, possibly stale install) instead of the prebuilt copy this
# build was actually configured against. An explicit linker rpath flag is
# unaffected by CMAKE_SKIP_RPATH (that variable only suppresses CMake's own
# automatic rpath computation) and, as an INTERFACE link option, propagates
# to every executable that transitively links optiq_interface.
# --disable-new-dtags forces the old-style DT_RPATH tag instead of
# DT_RUNPATH: DT_RUNPATH is searched AFTER LD_LIBRARY_PATH, so it would
# still lose to a dev environment's LD_LIBRARY_PATH (observed happening);
# DT_RPATH is searched BEFORE it, guaranteeing this exact build is used.
target_link_options(optiq_interface INTERFACE
    "-Wl,--disable-new-dtags,-rpath,${PROFILER_HUB_PREBUILT_DIR}/lib")

# PRIVATE + std_17: matches roc-optiq's project-wide standard (src/model
# sets CMAKE_CXX_STANDARD 17). A PUBLIC cxx_std_20 here previously leaked
# through datamodel into the roc-optiq executable target, silently
# recompiling the whole app at C++20 and breaking u8"..." string literals
# (char8_t* in C++20 vs char* in C++17) in the view layer's icon header.
target_compile_features(optiq_interface PRIVATE cxx_std_17)
