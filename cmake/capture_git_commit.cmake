# Records HEAD for official builds. Local builds keep "unknown", so --version
# never names a commit the binary may not match.
#
# GitHub Actions sets GITHUB_ACTIONS, and those jobs check out one commit
# and configure immediately, so a configure-time rev-parse is the tree that
# was compiled. There is no override: a local build cannot record a hash.
#
# Linux jobs configure inside a container. The checkout is owned by the
# runner user, so Git refuses to read it ("dubious ownership") unless this
# command opts in. -c is protected config, so safe.directory applies only
# to this rev-parse and is not written to a gitconfig.

set(ROCPROFVIS_GIT_COMMIT "unknown")

if(DEFINED ENV{GITHUB_ACTIONS})
    find_package(Git QUIET)
    if(GIT_FOUND)
        execute_process(
            COMMAND "${GIT_EXECUTABLE}" -c "safe.directory=*" rev-parse HEAD
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
            RESULT_VARIABLE _rocprofvis_git_result
            OUTPUT_VARIABLE _rocprofvis_git_commit
            ERROR_QUIET
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        if(_rocprofvis_git_result EQUAL 0 AND _rocprofvis_git_commit MATCHES "^[0-9a-fA-F]+$")
            set(ROCPROFVIS_GIT_COMMIT "${_rocprofvis_git_commit}")
        endif()
    endif()
endif()
