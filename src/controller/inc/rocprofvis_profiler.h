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
* Canonical on-disk name for `tool` (e.g. "rocprof-sys-run"), no directory or
* platform suffix. Query rather than hard-coding.
* String getters: pass buffer = nullptr to query *length (byte count, no
* terminator). Allocate *length + 1 and terminate the C string yourself.
* @param tool The profiler tool.
* @param buffer Destination, or nullptr to query length.
* @param length In/out byte count (must not be null).
* @returns kRocProfVisResultSuccess, or kRocProfVisResultInvalidEnum for an
*          unrecognized tool or kRPVProfilerToolNone.
*/
rocprofvis_result_t rocprofvis_profiler_tool_get_binary_name(rocprofvis_profiler_tool_t tool, char* buffer, uint32_t* length);

/*
* Absolute path of `tool` on this machine, same order as a local launch:
* tool_directory alone if set, else $ROCM_PATH/bin (default /opt/rocm off
* Windows), then $PATH. Local only — do not use for a remote launch.
* Same string-getter convention as rocprofvis_profiler_tool_get_binary_name.
* @param tool The profiler tool.
* @param tool_directory Absolute search dir, or null/empty for the default.
* @param buffer Destination, or nullptr to query length.
* @param length In/out byte count (must not be null).
* @returns kRocProfVisResultSuccess, kRocProfVisResultToolNotFound, or
*          kRocProfVisResultInvalidArgument (unknown tool or relative directory).
*/
rocprofvis_result_t rocprofvis_profiler_tool_resolve_path(rocprofvis_profiler_tool_t tool, char const* tool_directory, char* buffer, uint32_t* length);

/*
* Allocates a profiler configuration object.
* @returns A valid config, or nullptr on error.
*/
rocprofvis_profiler_config_t* rocprofvis_profiler_config_alloc(void);

/*
* Frees a profiler configuration object.
* @param config The profiler config to free.
*/
void rocprofvis_profiler_config_free(rocprofvis_profiler_config_t* config);

/*
* Names the binary to run (not a path). The controller maps the enum and
* searches at launch; a missing tool is kRocProfVisResultToolNotFound, not
* child exit 127. No caller-supplied string becomes argv[0].
* @param config The profiler config object.
* @param tool Rejects kRPVProfilerToolNone.
* @returns kRocProfVisResultSuccess, or kRocProfVisResultInvalidEnum.
*/
rocprofvis_result_t rocprofvis_profiler_config_set_tool(rocprofvis_profiler_config_t* config, rocprofvis_profiler_tool_t tool);

/*
* Directory to search for the tool (non-standard ROCm install). Filename still
* comes from the tool table. Must be absolute (remote: on the remote host).
* When set, no fallback to $ROCM_PATH or $PATH. Empty restores the default.
* @param config The profiler config object.
* @param tool_directory Absolute directory, or empty for the default search.
* @returns kRocProfVisResultSuccess or an error code.
*/
rocprofvis_result_t rocprofvis_profiler_config_set_tool_directory(rocprofvis_profiler_config_t* config, char const* tool_directory);

/*
* Expected output location. Not on argv — add the profiler's own flag via
* rocprofvis_profiler_config_add_profiler_arg. Unread for now; kept for a later
* per-stage artifact destination. Do not add readers.
* @param config The profiler config object.
* @param output_directory Directory where profiler output is expected.
* @returns kRocProfVisResultSuccess or an error code.
*/
rocprofvis_result_t rocprofvis_profiler_config_set_output_directory(rocprofvis_profiler_config_t* config, char const* output_directory);

/*
* Child cwd only (chdir after fork, lpCurrentDirectory, remote "cd"). Never
* changes the caller's cwd. Must exist.
* @param config The profiler config object.
* @param working_directory Directory to run the profiler in.
* @returns kRocProfVisResultSuccess or an error code.
*/
rocprofvis_result_t rocprofvis_profiler_config_set_working_directory(rocprofvis_profiler_config_t* config, char const* working_directory);

/*
* Adds an environment variable set in the child before exec.
* @param config The profiler config object.
* @param name Variable name (must not be null).
* @param value Variable value (must not be null).
* @returns kRocProfVisResultSuccess or an error code.
*/
rocprofvis_result_t rocprofvis_profiler_config_add_env_var(rocprofvis_profiler_config_t* config, char const* name, char const* value);

/*
* Appends one argv entry after argv[0] (the resolved tool). Caller composes
* the rest (output flag, "--", target). No whitespace split, no shell.
* @param config The profiler config object.
* @param arg The argument (must not be null).
* @returns kRocProfVisResultSuccess or an error code.
*/
rocprofvis_result_t rocprofvis_profiler_config_add_profiler_arg(rocprofvis_profiler_config_t* config, char const* arg);

/*
* Sets local execution (default).
* @param config The profiler config object.
* @returns kRocProfVisResultSuccess or an error code.
*/
rocprofvis_result_t rocprofvis_profiler_config_set_connection_local(rocprofvis_profiler_config_t* config);

/*
* Sets SSH execution. Launch still returns kRocProfVisResultNotSupported
* unless using rocprofvis_profiler_launch_remote_async.
* @param config The profiler config object.
* @param host Remote hostname (must not be null).
* @param user Remote username (must not be null).
* @param port SSH port.
* @param identity_file Identity file, or null for default.
* @param remote_stage_dir Remote staging directory, or null.
* @returns kRocProfVisResultSuccess or an error code.
*/
rocprofvis_result_t rocprofvis_profiler_config_set_connection_ssh(rocprofvis_profiler_config_t* config,
    char const* host, char const* user, int port, char const* identity_file, char const* remote_stage_dir);

/*
* Allocates a session (process, captured output, exit code). Status queries
* use this handle, not the future, so the future can be freed independently.
* @returns A valid session, or nullptr on error.
*/
rocprofvis_profiler_t* rocprofvis_profiler_alloc(void);

/*
* Frees a session and cancels any in-flight process. Does not free the bound
* future; call rocprofvis_controller_future_free separately.
* @param profiler The profiler session to free.
*/
void rocprofvis_profiler_free(rocprofvis_profiler_t* profiler);

/*
* Launches asynchronously and binds `future`. A missing tool returns
* kRocProfVisResultToolNotFound and leaves the session Failed.
* @param profiler The profiler session.
* @param config The profiler configuration.
* @param future Job completion / cancellation.
* @returns kRocProfVisResultSuccess or an error code.
*/
rocprofvis_result_t rocprofvis_profiler_launch_async(rocprofvis_profiler_t* profiler, rocprofvis_profiler_config_t* config, rocprofvis_controller_future_t* future);

/*
* TEMPORARY (remote/SSH): remove this guard when remote graduates.
* Runs on a borrowed, idle, already-authenticated connection. Caller keeps it
* alive until a terminal state. Trace download is the caller's job.
* @param profiler The profiler session.
* @param config The profiler configuration.
* @param connection Connected SSH handle.
* @param future Job completion / cancellation.
* @returns kRocProfVisResultSuccess or an error code.
*/
#ifdef ROCPROFVIS_ENABLE_REMOTE
rocprofvis_result_t rocprofvis_profiler_launch_remote_async(rocprofvis_profiler_t* profiler, rocprofvis_profiler_config_t* config, rocprofvis_controller_connection_t* connection, rocprofvis_controller_future_t* future);
#endif

/*
* Current profiler state.
* @param profiler The profiler session.
* @param state Output state.
* @returns kRocProfVisResultSuccess or an error code.
*/
rocprofvis_result_t rocprofvis_profiler_get_state(rocprofvis_profiler_t* profiler, rocprofvis_profiler_state_t* state);

/*
* Captured stdout/stderr. Same string-getter convention as
* rocprofvis_profiler_tool_get_binary_name (no terminator written).
* @param profiler The profiler session.
* @param buffer Destination, or null to query length.
* @param length In: bytes to copy. Out: bytes available, or bytes copied.
* @returns kRocProfVisResultSuccess or an error code.
*/
rocprofvis_result_t rocprofvis_profiler_get_output(rocprofvis_profiler_t* profiler, char* buffer, uint32_t* length);

/*
* Drops captured output so later get_output calls only return new data.
* @param profiler The profiler session.
* @returns kRocProfVisResultSuccess or an error code.
*/
rocprofvis_result_t rocprofvis_profiler_clear_output(rocprofvis_profiler_t* profiler);

/*
* Exit code of a completed process.
* @param profiler The profiler session.
* @param exit_code Output exit code.
* @returns kRocProfVisResultSuccess or an error code.
*/
rocprofvis_result_t rocprofvis_profiler_get_exit_code(rocprofvis_profiler_t* profiler, int32_t* exit_code);

/*
* Cancels a running process and the bound future.
* @param profiler The profiler session.
* @returns kRocProfVisResultSuccess or an error code.
*/
rocprofvis_result_t rocprofvis_profiler_cancel(rocprofvis_profiler_t* profiler);

#ifdef __cplusplus
}
#endif
