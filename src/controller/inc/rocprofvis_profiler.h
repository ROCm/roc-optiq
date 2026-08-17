// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_controller_enums.h"
#include "rocprofvis_controller_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

/*
* Gets the executable name for a profiler tool, without directory or platform
* suffix (e.g. "rocprof-sys-run").
* This is the single source of truth for what each tool is called on disk;
* callers should query it rather than hard-coding names.
* String getter convention: pass buffer = nullptr to query the required size in
* *length, then call again with a buffer of at least that many bytes. *length is
* a byte count and does not include a null terminator, so a caller wanting a
* C string should allocate *length + 1 and terminate it itself.
* @param tool The profiler tool.
* @param buffer Destination buffer, or nullptr to query the length.
* @param length In/out byte count (must not be null).
* @returns kRocProfVisResultSuccess, or kRocProfVisResultInvalidEnum for an
*          unrecognized tool or kRPVProfilerToolNone.
*/
rocprofvis_result_t rocprofvis_profiler_tool_get_binary_name(rocprofvis_profiler_tool_t tool, char* buffer, uint32_t* length);

/*
* Finds the absolute path of a profiler tool on this machine, in the same way and
* the same order a launch would: inside tool_directory alone if one is given,
* otherwise $ROCM_PATH/bin (defaulting to /opt/rocm off Windows) then each $PATH
* entry.
* Exposed separately from any session so a caller can show what will actually run
* in a command preview, or check a tool is installed before committing to a run.
* Answers a question about the LOCAL machine only - a remote launch resolves its
* tool on the remote host, so a caller must not apply this to one.
* Same string getter convention as rocprofvis_profiler_tool_get_binary_name.
* @param tool The profiler tool.
* @param tool_directory Absolute directory to search, or null/empty for the
*        default search.
* @param buffer Destination buffer, or nullptr to query the length.
* @param length In/out byte count (must not be null).
* @returns kRocProfVisResultSuccess, kRocProfVisResultToolNotFound if the tool is
*          not present, or kRocProfVisResultInvalidArgument for an unknown tool
*          or a relative directory.
*/
rocprofvis_result_t rocprofvis_profiler_tool_resolve_path(rocprofvis_profiler_tool_t tool, char const* tool_directory, char* buffer, uint32_t* length);

/*
* Allocates a profiler configuration object.
* @returns A valid profiler config object, or nullptr on error.
*/
rocprofvis_profiler_config_t* rocprofvis_profiler_config_alloc(void);

/*
* Frees a profiler configuration object.
* @param config The profiler config to free.
*/
void rocprofvis_profiler_config_free(rocprofvis_profiler_config_t* config);

/*
* Selects which profiler binary to run.
* The tool is named, not pathed: the controller owns the mapping to a binary
* name and the search for it (see rocprofvis_profiler_tool_resolve_path), so no
* caller-supplied string becomes argv[0]. The search happens at launch, and a
* tool that cannot be found fails the launch with kRocProfVisResultToolNotFound
* instead of the child exiting 127.
* @param config The profiler config object.
* @param tool The profiler binary to run. kRPVProfilerToolNone is rejected.
* @returns kRocProfVisResultSuccess, or kRocProfVisResultInvalidEnum for an
*          unrecognized tool.
*/
rocprofvis_result_t rocprofvis_profiler_config_set_tool(rocprofvis_profiler_config_t* config, rocprofvis_profiler_tool_t tool);

/*
* Restricts where the configured tool is looked for, for a ROCm install in a
* non-standard location.
* This is a directory, never a path to an executable: the filename still comes
* from the tool table, so this cannot name a different program to run. It is a
* separate call from rocprofvis_profiler_config_set_tool so that a path can never
* be supplied where a tool was expected.
* Must be absolute - for a remote launch, an absolute path on the remote host.
* When set, the tool must be in this directory: resolution does not fall back to
* $ROCM_PATH or $PATH, so a directory that lacks the tool fails loudly rather
* than silently running a different build of it. Logged prominently at launch.
* @param config The profiler config object.
* @param tool_directory Absolute directory holding the profiler tools. Empty
*        restores the default search.
* @returns kRocProfVisResultSuccess or an error code.
*/
rocprofvis_result_t rocprofvis_profiler_config_set_tool_directory(rocprofvis_profiler_config_t* config, char const* tool_directory);

/*
* Records where the profiler is expected to write its output.
* This does not put anything on the command line: profilers spell their output
* flag differently, and some take none at all, so pass it explicitly with
* rocprofvis_profiler_config_add_profiler_arg.
* Nothing reads this value yet. It is kept because a staged pipeline will need
* a destination the controller acts on - moving a produced artifact there for
* tools that can only write to their own working directory - at which point it
* becomes a per-stage setting rather than a per-config one. Until then, do not
* add readers that would make it a second source of truth for a path the
* command line already carries.
* @param config The profiler config object.
* @param output_directory Directory where profiler output is expected.
* @returns kRocProfVisResultSuccess or an error code.
*/
rocprofvis_result_t rocprofvis_profiler_config_set_output_directory(rocprofvis_profiler_config_t* config, char const* output_directory);

/*
* Sets the working directory of the profiler process.
* The directory is applied to the child process only (chdir after fork on
* POSIX, lpCurrentDirectory on Windows, a "cd" prefix for remote launches);
* the calling process's working directory is never changed. Required by tools
* that write output relative to their own working directory rather than to a
* path given on the command line.
* @param config The profiler config object.
* @param working_directory Directory to run the profiler process in. Must exist.
* @returns kRocProfVisResultSuccess or an error code.
*/
rocprofvis_result_t rocprofvis_profiler_config_set_working_directory(rocprofvis_profiler_config_t* config, char const* working_directory);

/*
* Adds an environment variable to the profiler configuration.
* These are set in the child process environment before exec.
* @param config The profiler config object.
* @param name Environment variable name (must not be null).
* @param value Environment variable value (must not be null).
* @returns kRocProfVisResultSuccess or an error code.
*/
rocprofvis_result_t rocprofvis_profiler_config_add_env_var(rocprofvis_profiler_config_t* config, char const* name, char const* value);

/*
* Appends a single argument to the profiler command line.
* This is the only way arguments reach the process. The full command line is
* argv[0] = the resolved tool path followed by these arguments in the order added, so
* the caller composes the entire command - including any output flag, "--"
* separator, target executable, and target arguments.
* Each call adds exactly one argv entry, which is passed to the process
* verbatim: no splitting on whitespace and no shell interpretation, so
* arguments containing spaces or quotes arrive intact.
* @param config The profiler config object.
* @param arg The argument string (must not be null).
* @returns kRocProfVisResultSuccess or an error code.
*/
rocprofvis_result_t rocprofvis_profiler_config_add_profiler_arg(rocprofvis_profiler_config_t* config, char const* arg);

/*
* Sets the connection type to local execution (default).
* @param config The profiler config object.
* @returns kRocProfVisResultSuccess or an error code.
*/
rocprofvis_result_t rocprofvis_profiler_config_set_connection_local(rocprofvis_profiler_config_t* config);

/*
* Sets the connection type to SSH (not yet implemented, will return kRocProfVisResultNotSupported at launch).
* @param config The profiler config object.
* @param host Remote hostname (must not be null).
* @param user Remote username (must not be null).
* @param port SSH port number.
* @param identity_file Path to SSH identity file (may be null for default).
* @param remote_stage_dir Remote directory for staging output (may be null).
* @returns kRocProfVisResultSuccess or an error code.
*/
rocprofvis_result_t rocprofvis_profiler_config_set_connection_ssh(rocprofvis_profiler_config_t* config,
    char const* host, char const* user, int port, char const* identity_file, char const* remote_stage_dir);

/*
* Allocates a profiler session handle. The session owns the in-process
* profiler controller state (process handle, captured output, trace path,
* exit code). Status queries take this handle rather than the future, so the
* caller can free the future independently once it completes.
* @returns A valid profiler session handle, or nullptr on error.
*/
rocprofvis_profiler_t* rocprofvis_profiler_alloc(void);

/*
* Frees a profiler session handle. Cancels any in-flight profiler process.
* The bound future is NOT freed; callers must free it separately via
* rocprofvis_controller_future_free.
* @param profiler The profiler session to free.
*/
void rocprofvis_profiler_free(rocprofvis_profiler_t* profiler);

/*
* Launches a profiler process asynchronously and binds it to the given future.
* @param profiler The profiler session handle (owns the controller state).
* @param config The profiler configuration.
* @param future The future object used to track job completion / cancellation.
* @returns kRocProfVisResultSuccess or an error code.
*/
rocprofvis_result_t rocprofvis_profiler_launch_async(rocprofvis_profiler_t* profiler, rocprofvis_profiler_config_t* config, rocprofvis_controller_future_t* future);

/*
* TEMPORARY (remote/SSH): remote profiler launch C ABI. Remove this guard when
* the remote feature graduates.
*
* Launches a profiler process on a remote host over an existing SSH connection.
* The connection must already be connected and authenticated (e.g. by the View's
* SshSession) and must be idle (no other SSH operation in flight) for the
* duration of the remote profiler run. The connection is borrowed; the caller
* retains ownership and must keep it alive until the profiler reaches a terminal
* state. Downloading the produced trace back is the caller's responsibility.
* @param profiler The profiler session handle (owns the controller state).
* @param config The profiler configuration (should have SSH connection set).
* @param connection The connected/authenticated SSH connection handle.
* @param future The future object used to track job completion / cancellation.
* @returns kRocProfVisResultSuccess or an error code.
*/
#ifdef ROCPROFVIS_ENABLE_REMOTE
rocprofvis_result_t rocprofvis_profiler_launch_remote_async(rocprofvis_profiler_t* profiler, rocprofvis_profiler_config_t* config, rocprofvis_controller_connection_t* connection, rocprofvis_controller_future_t* future);
#endif

/*
* Gets the current state of the profiler execution.
* @param profiler The profiler session handle.
* @param state Output parameter for the profiler state.
* @returns kRocProfVisResultSuccess or an error code.
*/
rocprofvis_result_t rocprofvis_profiler_get_state(rocprofvis_profiler_t* profiler, rocprofvis_profiler_state_t* state);

/*
* Gets the profiler output (stdout/stderr).
* Call with a null buffer (or *length of 0) to query the length in bytes, then
* again with a buffer to receive the bytes. No null terminator is written, and
* *length is a byte count rather than a capacity, so the queried length can be
* passed straight back in; allocate length + 1 bytes and terminate the string
* yourself. On the second call *length is updated to the number of bytes copied.
* @param profiler The profiler session handle.
* @param buffer Buffer to write the output to, or null to query the length.
* @param length In: bytes to copy. Out: bytes available, or bytes copied.
* @returns kRocProfVisResultSuccess or an error code.
*/
rocprofvis_result_t rocprofvis_profiler_get_output(rocprofvis_profiler_t* profiler, char* buffer, uint32_t* length);

/*
* Clears the accumulated profiler output buffer.
* After calling this, rocprofvis_profiler_get_output will only return output
* produced after the clear.
* @param profiler The profiler session handle.
* @returns kRocProfVisResultSuccess or an error code.
*/
rocprofvis_result_t rocprofvis_profiler_clear_output(rocprofvis_profiler_t* profiler);

/*
* Gets the exit code of a completed profiler process.
* @param profiler The profiler session handle.
* @param exit_code Output parameter for the process exit code.
* @returns kRocProfVisResultSuccess or an error code.
*/
rocprofvis_result_t rocprofvis_profiler_get_exit_code(rocprofvis_profiler_t* profiler, int32_t* exit_code);

/*
* Cancels a running profiler process. Forwards cancellation to the bound
* future as well so any job waiting on it unblocks.
* @param profiler The profiler session handle.
* @returns kRocProfVisResultSuccess or an error code.
*/
rocprofvis_result_t rocprofvis_profiler_cancel(rocprofvis_profiler_t* profiler);

#ifdef __cplusplus
}
#endif
