// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

typedef enum rocprofvis_python_result_t
{
    kRocProfVisPythonSuccess          = 0,
    kRocProfVisPythonError            = 1,
    kRocProfVisPythonInvalidArgument  = 2,
    kRocProfVisPythonCancelled        = 3,
    kRocProfVisPythonNotInitialized   = 4,
    kRocProfVisPythonBusy             = 5,
} rocprofvis_python_result_t;

/*
 * Called on the interpreter thread with the GIL held. py_dict is a
 * PyObject* globals mapping for this exec. The runtime never includes
 * controller headers; the controller uses this hook to inject optiq.
 */
typedef void (*rocprofvis_python_prepare_globals_t)(void* py_dict, void* user);

/*
 * Called on the interpreter thread after exec, with the GIL released.
 * error_message is valid only during the callback.
 */
typedef void (*rocprofvis_python_done_t)(void* user, rocprofvis_python_result_t result,
                                         char const* error_message);

/*
 * Starts the dedicated interpreter thread and initializes CPython with
 * an isolated config. runtime_root may be null to use the compile-time
 * Python prefix. Safe to call once; a second call is success if already
 * initialized.
 */
rocprofvis_python_result_t rocprofvis_python_init(char const* runtime_root);

/*
 * Posts source to the interpreter thread and returns immediately. done
 * is invoked when exec finishes (or if posting fails, it is not called
 * and the return value is the error).
 */
rocprofvis_python_result_t rocprofvis_python_exec(
    char const*                         source,
    rocprofvis_python_prepare_globals_t prepare_globals, void* user,
    rocprofvis_python_done_t done);

/*
 * Requests the current script to stop (PyErr_SetInterrupt). Does not
 * wait. No-op if idle.
 */
void rocprofvis_python_interrupt(void);

/*
 * Stops the interpreter thread and finalizes CPython. Blocks until the
 * thread exits.
 */
void rocprofvis_python_shutdown(void);

/*
 * Native handle of the interpreter thread, or 0 if not initialized.
 * Used by tests to prove exec does not run on the caller thread.
 */
unsigned long long rocprofvis_python_interpreter_thread_id(void);

#ifdef __cplusplus
}
#endif
