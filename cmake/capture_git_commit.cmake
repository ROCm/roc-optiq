# Records the HEAD commit for the generated version header.
# Sets ROCPROFVIS_GIT_COMMIT to the full hash, or "unknown" when git is
# missing, this directory is not a checkout, or rev-parse fails.
# Watching HEAD (and the branch ref it names) reconfigures the next build
# after a new commit so the baked-in hash stays current.

set(ROCPROFVIS_GIT_COMMIT "unknown")

find_package(Git QUIET)
if(GIT_FOUND)
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" rev-parse HEAD
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        RESULT_VARIABLE _rocprofvis_git_result
        OUTPUT_VARIABLE _rocprofvis_git_commit
        ERROR_QUIET
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(_rocprofvis_git_result EQUAL 0 AND _rocprofvis_git_commit MATCHES "^[0-9a-fA-F]+$")
        set(ROCPROFVIS_GIT_COMMIT "${_rocprofvis_git_commit}")
    endif()

    execute_process(
        COMMAND "${GIT_EXECUTABLE}" rev-parse --git-path HEAD
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        RESULT_VARIABLE _rocprofvis_head_result
        OUTPUT_VARIABLE _rocprofvis_head_path
        ERROR_QUIET
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(_rocprofvis_head_result EQUAL 0 AND NOT _rocprofvis_head_path STREQUAL "")
        if(NOT IS_ABSOLUTE "${_rocprofvis_head_path}")
            set(_rocprofvis_head_path "${CMAKE_SOURCE_DIR}/${_rocprofvis_head_path}")
        endif()
        if(EXISTS "${_rocprofvis_head_path}")
            set_property(DIRECTORY APPEND
                PROPERTY CMAKE_CONFIGURE_DEPENDS "${_rocprofvis_head_path}")

            file(READ "${_rocprofvis_head_path}" _rocprofvis_head_contents)
            string(STRIP "${_rocprofvis_head_contents}" _rocprofvis_head_contents)
            if(_rocprofvis_head_contents MATCHES "^ref: (.+)$")
                set(_rocprofvis_ref "${CMAKE_MATCH_1}")
                execute_process(
                    COMMAND "${GIT_EXECUTABLE}" rev-parse --git-path "${_rocprofvis_ref}"
                    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
                    RESULT_VARIABLE _rocprofvis_ref_result
                    OUTPUT_VARIABLE _rocprofvis_ref_path
                    ERROR_QUIET
                    OUTPUT_STRIP_TRAILING_WHITESPACE
                )
                if(_rocprofvis_ref_result EQUAL 0 AND NOT _rocprofvis_ref_path STREQUAL "")
                    if(NOT IS_ABSOLUTE "${_rocprofvis_ref_path}")
                        set(_rocprofvis_ref_path "${CMAKE_SOURCE_DIR}/${_rocprofvis_ref_path}")
                    endif()
                    if(EXISTS "${_rocprofvis_ref_path}")
                        set_property(DIRECTORY APPEND
                            PROPERTY CMAKE_CONFIGURE_DEPENDS "${_rocprofvis_ref_path}")
                    endif()
                endif()
            endif()
        endif()
    endif()
endif()
