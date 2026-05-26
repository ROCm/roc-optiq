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
* Sets the profiler type in the configuration.
* @param config The profiler config object.
* @param profiler_type The type of profiler to launch.
* @returns kRocProfVisResultSuccess or an error code.
*/
rocprofvis_result_t rocprofvis_profiler_config_set_type(rocprofvis_profiler_config_t* config, rocprofvis_profiler_type_t profiler_type);

/*
* Sets the profiler executable path in the configuration.
* @param config The profiler config object.
* @param profiler_path Path to the profiler executable.
* @returns kRocProfVisResultSuccess or an error code.
*/
rocprofvis_result_t rocprofvis_profiler_config_set_profiler_path(rocprofvis_profiler_config_t* config, char const* profiler_path);

/*
* Sets the target executable path in the configuration.
* @param config The profiler config object.
* @param target_executable Path to the target application to profile.
* @returns kRocProfVisResultSuccess or an error code.
*/
rocprofvis_result_t rocprofvis_profiler_config_set_target_executable(rocprofvis_profiler_config_t* config, char const* target_executable);

/*
* Sets the target application arguments in the configuration.
* @param config The profiler config object.
* @param target_args Arguments to pass to the target application.
* @returns kRocProfVisResultSuccess or an error code.
*/
rocprofvis_result_t rocprofvis_profiler_config_set_target_args(rocprofvis_profiler_config_t* config, char const* target_args);

/*
* Sets the profiler arguments in the configuration.
* @param config The profiler config object.
* @param profiler_args Arguments to pass to the profiler.
* @returns kRocProfVisResultSuccess or an error code.
*/
rocprofvis_result_t rocprofvis_profiler_config_set_profiler_args(rocprofvis_profiler_config_t* config, char const* profiler_args);

/*
* Sets the output directory in the configuration.
* @param config The profiler config object.
* @param output_directory Directory where profiler output should be saved.
* @returns kRocProfVisResultSuccess or an error code.
*/
rocprofvis_result_t rocprofvis_profiler_config_set_output_directory(rocprofvis_profiler_config_t* config, char const* output_directory);

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
* Adds a single argument to the profiler argument list.
* Arguments added this way are passed before --output and -- target.
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
* Gets the current state of the profiler execution.
* @param profiler The profiler session handle.
* @param state Output parameter for the profiler state.
* @returns kRocProfVisResultSuccess or an error code.
*/
rocprofvis_result_t rocprofvis_profiler_get_state(rocprofvis_profiler_t* profiler, rocprofvis_profiler_state_t* state);

/*
* Gets the profiler output (stdout/stderr).
* @param profiler The profiler session handle.
* @param buffer Buffer to write the output to, or null to query the length.
* @param length Pointer to integer containing buffer size, or to write required size.
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
* Gets the path to the generated trace file after profiler completion.
* @param profiler The profiler session handle.
* @param buffer Buffer to write the trace path to, or null to query the length.
* @param length Pointer to integer containing buffer size, or to write required size.
* @returns kRocProfVisResultSuccess or an error code.
*/
rocprofvis_result_t rocprofvis_profiler_get_trace_path(rocprofvis_profiler_t* profiler, char* buffer, uint32_t* length);

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
