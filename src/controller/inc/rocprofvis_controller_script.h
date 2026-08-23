// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_controller.h"

#ifdef __cplusplus
extern "C"
{
#endif

/*
 * Runs a Python source string on the dedicated interpreter thread.
 * Returns immediately. Wait on future for completion. controller may
 * be null when the script does not read a trace. context may be null
 * (all tracks, timeline min-max). On success *result is a
 * script-result handle the caller must free.
 */
rocprofvis_result_t rocprofvis_script_execute_async(
    rocprofvis_controller_t*              controller,
    char const*                           source,
    rocprofvis_controller_arguments_t*    context,
    rocprofvis_controller_future_t*       future,
    rocprofvis_controller_script_result_t** result);

/*
 * Requests cancellation of an in-flight script. The future completes
 * after the interpreter unwinds. Callers must not assume cancel always
 * succeeds.
 */
rocprofvis_result_t rocprofvis_script_cancel(rocprofvis_controller_future_t* future);

void rocprofvis_script_result_free(rocprofvis_controller_script_result_t* result);

#ifdef __cplusplus
}
#endif
