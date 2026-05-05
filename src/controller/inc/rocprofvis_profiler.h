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
* Launches a profiler process asynchronously.
* @param config The profiler configuration.
* @param future The future object to track the profiler execution.
* @returns kRocProfVisResultSuccess or an error code.
*/
rocprofvis_result_t rocprofvis_profiler_launch_async(rocprofvis_profiler_config_t* config, rocprofvis_controller_future_t* future);

/*
* Gets the current state of the profiler execution.
* @param future The future returned from rocprofvis_profiler_launch_async.
* @param state Output parameter for the profiler state.
* @returns kRocProfVisResultSuccess or an error code.
*/
rocprofvis_result_t rocprofvis_profiler_get_state(rocprofvis_controller_future_t* future, rocprofvis_profiler_state_t* state);

/*
* Gets the profiler output (stdout/stderr).
* @param future The future returned from rocprofvis_profiler_launch_async.
* @param buffer Buffer to write the output to, or null to query the length.
* @param length Pointer to integer containing buffer size, or to write required size.
* @returns kRocProfVisResultSuccess or an error code.
*/
rocprofvis_result_t rocprofvis_profiler_get_output(rocprofvis_controller_future_t* future, char* buffer, uint32_t* length);

/*
* Gets the path to the generated trace file after profiler completion.
* @param future The future returned from rocprofvis_profiler_launch_async.
* @param buffer Buffer to write the trace path to, or null to query the length.
* @param length Pointer to integer containing buffer size, or to write required size.
* @returns kRocProfVisResultSuccess or an error code.
*/
rocprofvis_result_t rocprofvis_profiler_get_trace_path(rocprofvis_controller_future_t* future, char* buffer, uint32_t* length);

/*
* Gets the exit code of a completed profiler process.
* @param future The future returned from rocprofvis_profiler_launch_async.
* @param exit_code Output parameter for the process exit code.
* @returns kRocProfVisResultSuccess or an error code.
*/
rocprofvis_result_t rocprofvis_profiler_get_exit_code(rocprofvis_controller_future_t* future, int32_t* exit_code);

/*
* Cancels a running profiler process.
* @param future The future returned from rocprofvis_profiler_launch_async.
* @returns kRocProfVisResultSuccess or an error code.
*/
rocprofvis_result_t rocprofvis_profiler_cancel(rocprofvis_controller_future_t* future);

#ifdef __cplusplus
}
#endif
