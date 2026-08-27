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
 * error_message is valid only during the callback. On failure it carries
 * the formatted traceback, so the line that raised can be read off it.
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
 * and the return value is the error). Scripts are queued and run one at
 * a time.
 *
 * timeout_ms bounds the run; 0 uses the built-in default. A script that
 * outstays it is interrupted and reported as an error rather than a
 * cancellation, because a timeout is a script to fix and only an
 * explicit interrupt is a cancellation.
 */
rocprofvis_python_result_t rocprofvis_python_exec(
    char const*                         source,
    rocprofvis_python_prepare_globals_t prepare_globals, void* user,
    rocprofvis_python_done_t done, unsigned long long timeout_ms);

/*
 * Requests the current script to stop by raising KeyboardInterrupt in
 * the interpreter thread. Returns immediately; delivery happens on the
 * runtime's own thread, so the caller never waits on the GIL. No-op if
 * idle, and not guaranteed - a script can catch the exception, so the
 * request is repeated until the script actually ends.
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
