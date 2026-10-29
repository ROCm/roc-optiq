set(PROFILER_HUB_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(PROFILER_HUB_BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)
set(PROFILER_HUB_FETCH_DEPENDENCIES ON CACHE BOOL "" FORCE)
set(PROFILER_HUB_REPO    "git@github.com:ROCm/rocm-systems.git")
set(PROFILER_HUB_TAG     "vs-profiler-hub-tables")
set(PROFILER_HUB_SUBDIR  "profilers/profiler-hub")

find_package(Git REQUIRED)

set(PROFILER_HUB_DOWNLOAD_DIR "${CMAKE_BINARY_DIR}/_deps/rocm-systems")

# Re-fetch if the tag has changed since last configure
set(PROFILER_HUB_TAG_FILE "${PROFILER_HUB_DOWNLOAD_DIR}/.cmake_tag")
set(_needs_fetch FALSE)

if(NOT EXISTS "${PROFILER_HUB_DOWNLOAD_DIR}/${PROFILER_HUB_SUBDIR}")
    set(_needs_fetch TRUE)
elseif(EXISTS "${PROFILER_HUB_TAG_FILE}")
    file(READ "${PROFILER_HUB_TAG_FILE}" _cached_tag)
    string(STRIP "${_cached_tag}" _cached_tag)
    if(NOT _cached_tag STREQUAL PROFILER_HUB_TAG)
        message(STATUS "profiler-hub tag changed (${_cached_tag} -> ${PROFILER_HUB_TAG}), re-fetching...")
        file(REMOVE_RECURSE "${PROFILER_HUB_DOWNLOAD_DIR}")
        set(_needs_fetch TRUE)
    endif()
else()
    set(_needs_fetch TRUE)
endif()

if(_needs_fetch)
    message(STATUS "Fetching profiler-hub @ ${PROFILER_HUB_TAG}...")

    execute_process(
        COMMAND ${GIT_EXECUTABLE} clone
            --filter=blob:none --no-checkout --sparse --depth 1
            ${PROFILER_HUB_REPO}
            ${PROFILER_HUB_DOWNLOAD_DIR}
        RESULT_VARIABLE GIT_RESULT
    )
    if(NOT GIT_RESULT EQUAL 0)
        message(FATAL_ERROR "Failed to clone ${PROFILER_HUB_REPO}")
    endif()

    execute_process(
        COMMAND ${GIT_EXECUTABLE} sparse-checkout set ${PROFILER_HUB_SUBDIR}
        WORKING_DIRECTORY ${PROFILER_HUB_DOWNLOAD_DIR}
    )

    execute_process(
        COMMAND ${GIT_EXECUTABLE} -c lfs.fetchexclude="*" fetch --depth 1
                origin ${PROFILER_HUB_TAG}
        WORKING_DIRECTORY ${PROFILER_HUB_DOWNLOAD_DIR}
        RESULT_VARIABLE GIT_RESULT
    )
    if(NOT GIT_RESULT EQUAL 0)
        message(FATAL_ERROR "Failed to fetch ${PROFILER_HUB_TAG}")
    endif()

    execute_process(
        COMMAND ${GIT_EXECUTABLE} -c lfs.fetchexclude="*" checkout FETCH_HEAD
        WORKING_DIRECTORY ${PROFILER_HUB_DOWNLOAD_DIR}
    )

    # Cache the tag so we can detect changes next configure
    file(WRITE "${PROFILER_HUB_TAG_FILE}" "${PROFILER_HUB_TAG}")

    message(STATUS "profiler-hub ready")
else()
    execute_process(
        COMMAND ${GIT_EXECUTABLE} status --porcelain --untracked-files=no
        WORKING_DIRECTORY ${PROFILER_HUB_DOWNLOAD_DIR}
        OUTPUT_VARIABLE _profiler_hub_dirty
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --abbrev-ref HEAD
        WORKING_DIRECTORY ${PROFILER_HUB_DOWNLOAD_DIR}
        OUTPUT_VARIABLE _profiler_hub_branch
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(NOT _profiler_hub_dirty STREQUAL "")
        message(STATUS "profiler-hub has local changes; leaving the checkout as-is")
    elseif(NOT _profiler_hub_branch STREQUAL "HEAD")
        message(STATUS "profiler-hub is on branch ${_profiler_hub_branch}; not updating")
    else()
        message(STATUS "Updating profiler-hub @ ${PROFILER_HUB_TAG}...")
        execute_process(
            COMMAND ${GIT_EXECUTABLE} -c lfs.fetchexclude="*" fetch
                --depth 1 origin ${PROFILER_HUB_TAG}
            WORKING_DIRECTORY ${PROFILER_HUB_DOWNLOAD_DIR}
            RESULT_VARIABLE GIT_RESULT
        )
        if(NOT GIT_RESULT EQUAL 0)
            message(FATAL_ERROR "Failed to update ${PROFILER_HUB_TAG}")
        endif()
        execute_process(
            COMMAND ${GIT_EXECUTABLE} -c lfs.fetchexclude="*"
                checkout --force FETCH_HEAD
            WORKING_DIRECTORY ${PROFILER_HUB_DOWNLOAD_DIR}
            RESULT_VARIABLE GIT_RESULT
        )
        if(NOT GIT_RESULT EQUAL 0)
            message(FATAL_ERROR "Failed to check out updated ${PROFILER_HUB_TAG}")
        endif()
        message(STATUS "profiler-hub updated")
    endif()
endif()

set(PROFILER_HUB_SOURCE_DIR "${PROFILER_HUB_DOWNLOAD_DIR}/${PROFILER_HUB_SUBDIR}")
set(PROFILER_HUB_SOURCE_DIR "${PROFILER_HUB_SOURCE_DIR}" PARENT_SCOPE)  # export to parent
set(PROFILER_HUB_INCLUDE_DIR ${PROFILER_HUB_SOURCE_DIR}/include)
add_subdirectory(${PROFILER_HUB_SOURCE_DIR} profiler-hub)

