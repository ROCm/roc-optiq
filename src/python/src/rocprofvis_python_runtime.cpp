// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_python_runtime.h"
#include "rocprofvis_core.h"
#include "rocprofvis_core_assert.h"

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <exception>
#include <functional>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

#define PY_SSIZE_T_CLEAN
#include "rocprofvis_python.h"

namespace RocProfVis
{
namespace Python
{

namespace
{

// Nothing else bounds exec: a loop that never ends holds the one interpreter
// thread forever, and the caller only sees a future that never completes.
uint64_t const SCRIPT_TIMEOUT_MS = 30000;

// A script that catches KeyboardInterrupt swallows the first stop, so keep
// raising rather than assuming one is enough.
uint64_t const INTERRUPT_RETRY_MS = 1000;

// Enough traceback to find the failing line, without pasting a runaway
// recursion into the transcript.
size_t const MAX_ERROR_CHARS = 4000;

char const* const ALLOWLISTED_MODULES[] = {
    "math",       "statistics", "decimal",     "fractions", "itertools",
    "functools",  "operator",   "collections", "heapq",     "dataclasses",
    "typing",     "enum",       "json",        "re",        "datetime",
    "textwrap",   "string",     "optiq",
};

bool
is_allowlisted(char const* name)
{
    if(!name)
    {
        return false;
    }
    char const* dot = std::strchr(name, '.');
    size_t      top_len =
        dot ? static_cast<size_t>(dot - name) : std::strlen(name);
    for(char const* allowed : ALLOWLISTED_MODULES)
    {
        if(std::strlen(allowed) == top_len &&
           std::strncmp(name, allowed, top_len) == 0)
        {
            return true;
        }
    }
    return false;
}

PyObject*
restricted_import(PyObject* self, PyObject* args, PyObject* kwargs)
{
    (void) self;
    char const* name     = nullptr;
    PyObject*   globals  = nullptr;
    PyObject*   locals   = nullptr;
    PyObject*   fromlist = nullptr;
    int         level    = 0;
    static char const* kwlist[] = {"name", "globals", "locals", "fromlist",
                                   "level", nullptr};
    if(!PyArg_ParseTupleAndKeywords(args, kwargs, "s|OOOi:__import__",
                                    const_cast<char**>(kwlist), &name, &globals,
                                    &locals, &fromlist, &level))
    {
        return nullptr;
    }
    if(!is_allowlisted(name))
    {
        return PyErr_Format(PyExc_ImportError,
                            "module '%s' is not allowed in optiq scripts",
                            name);
    }
    return PyImport_ImportModuleLevel(name, globals, locals,
                                      fromlist ? fromlist : Py_None, level);
}

PyMethodDef IMPORT_METHOD = {
    "__import__", reinterpret_cast<PyCFunction>(restricted_import),
    METH_VARARGS | METH_KEYWORDS, nullptr};

char const* const SAFE_BUILTIN_NAMES[] = {
    "None",       "True",      "False",      "abs",        "all",
    "any",        "bool",      "chr",        "dict",       "divmod",
    "enumerate",  "filter",    "float",      "format",     "frozenset",
    "hasattr",    "hash",      "hex",        "id",         "int",
    "isinstance", "issubclass","iter",       "len",        "list",
    "map",        "max",       "min",        "next",       "oct",
    "ord",        "pow",       "range",      "repr",       "reversed",
    "round",      "set",       "slice",      "sorted",     "str",
    "sum",        "tuple",     "zip",        "bytes",
    // The class machinery. A `class` statement compiles to __build_class__, so
    // without it every class fails with a bare NameError - which also takes
    // down the allowlisted dataclasses and enum, since both are used by
    // declaring a class. The rest is what a class body reaches for.
    "__build_class__", "type", "object", "super", "property", "staticmethod",
    "classmethod",
    "Exception",  "ValueError",
    "TypeError",  "RuntimeError", "ImportError", "StopIteration",
    "KeyError",   "IndexError", "ArithmeticError", "OverflowError",
    "ZeroDivisionError", "AssertionError", "AttributeError", "NameError",
};

// Keeps the tail, not the head: the exception line and the frame that raised
// it come last, and those are what the reader needs.
std::string
TrimErrorTail(std::string const& text)
{
    if(text.size() <= MAX_ERROR_CHARS)
    {
        return text;
    }
    return "... earlier frames omitted ...\n" +
           text.substr(text.size() - MAX_ERROR_CHARS);
}

// Renders the exception being handled the way the interpreter would print it,
// traceback and all. Requires the GIL, and clears the error indicator.
std::string
FormatPythonError()
{
    PyObject* type      = nullptr;
    PyObject* value     = nullptr;
    PyObject* traceback = nullptr;
    PyErr_Fetch(&type, &value, &traceback);
    PyErr_NormalizeException(&type, &value, &traceback);
    if(value && traceback)
    {
        PyException_SetTraceback(value, traceback);
    }

    std::string text;
    PyObject*   module = PyImport_ImportModule("traceback");
    if(module)
    {
        // The script's own import allowlist does not apply here: that
        // __import__ only exists in the script globals, not to C callers.
        PyObject* lines = PyObject_CallMethod(
            module, "format_exception", "OOO", type ? type : Py_None,
            value ? value : Py_None, traceback ? traceback : Py_None);
        if(lines)
        {
            PyObject* separator = PyUnicode_FromString("");
            PyObject*   joined  = separator ? PyUnicode_Join(separator, lines) : nullptr;
            char const* utf8    = joined ? PyUnicode_AsUTF8(joined) : nullptr;
            if(utf8)
            {
                text = utf8;
            }
            Py_XDECREF(joined);
            Py_XDECREF(separator);
            Py_DECREF(lines);
        }
        // format_exception can fail on an exotic exception; the original error
        // is already captured, so drop whatever it raised.
        PyErr_Clear();
        Py_DECREF(module);
    }

    if(text.empty() && value)
    {
        PyObject*   str  = PyObject_Str(value);
        char const* utf8 = str ? PyUnicode_AsUTF8(str) : nullptr;
        if(utf8)
        {
            text = utf8;
        }
        Py_XDECREF(str);
        PyErr_Clear();
    }

    Py_XDECREF(type);
    Py_XDECREF(value);
    Py_XDECREF(traceback);
    return text.empty() ? std::string("script error") : TrimErrorTail(text);
}

std::string
DescribeDuration(uint64_t milliseconds)
{
    if(milliseconds >= 1000 && (milliseconds % 1000) == 0)
    {
        return std::to_string(milliseconds / 1000) + " seconds";
    }
    return std::to_string(milliseconds) + " ms";
}

struct WorkItem
{
    std::string                         source;
    rocprofvis_python_prepare_globals_t prepare_globals;
    void*                               user;
    rocprofvis_python_done_t            done;
    uint64_t                            timeout_ms;
};

class Runtime
{
public:
    static Runtime& Get()
    {
        static Runtime instance;
        return instance;
    }

    rocprofvis_python_result_t Init(char const* runtime_root);
    rocprofvis_python_result_t Post(char const* source,
                                    rocprofvis_python_prepare_globals_t prepare,
                                    void* user, rocprofvis_python_done_t done,
                                    uint64_t timeout_ms);
    void                       Interrupt();
    void                       Shutdown();
    unsigned long long         InterpreterThreadId() const;

private:
    Runtime() = default;
    ~Runtime()
    {
        Shutdown();
    }

    Runtime(Runtime const&)            = delete;
    Runtime& operator=(Runtime const&) = delete;

    void ThreadMain();
    void WatchdogMain();
    void RaiseInInterpreter();
    void BeginExecDeadline(uint64_t timeout_ms);
    bool EndExecDeadline();
    bool InitializeInterpreter();
    void FinalizeInterpreter();
    void ExecWork(WorkItem& item);
    PyObject* MakeScriptGlobals();

    std::string                m_runtime_root;
    std::thread                m_thread;
    unsigned long long         m_thread_id = 0;
    std::mutex                 m_mutex;
    std::condition_variable    m_cv;
    std::queue<WorkItem>       m_queue;
    bool                       m_running       = false;
    bool                       m_initialized   = false;
    bool                       m_init_ok       = false;
    rocprofvis_python_result_t m_init_result   = kRocProfVisPythonError;
    bool                       m_stop          = false;

    // The deadline thread. Both the deadline and an explicit cancel are
    // delivered from here, so the caller of Interrupt - usually the UI thread -
    // never waits on the GIL.
    std::thread                           m_watchdog;
    std::condition_variable               m_watchdog_cv;
    bool                                  m_exec_active         = false;
    bool                                  m_exec_timed_out      = false;
    bool                                  m_interrupt_requested = false;
    std::chrono::steady_clock::time_point m_next_interrupt;
    // Python's own id for the interpreter thread, which is what
    // PyThreadState_SetAsyncExc addresses. Written once during init.
    unsigned long                         m_py_thread_ident = 0;
};

wchar_t*
DecodePath(char const* path)
{
    if(!path)
    {
        return nullptr;
    }
    return Py_DecodeLocale(path, nullptr);
}

bool
Runtime::InitializeInterpreter()
{
    PyConfig config;
    PyConfig_InitIsolatedConfig(&config);
    config.site_import            = 0;
    config.user_site_directory    = 0;
    config.use_environment        = 0;
    config.install_signal_handlers = 0;

    char const* home = m_runtime_root.empty() ? nullptr : m_runtime_root.c_str();
#ifndef ROCPROFVIS_PYTHON_HOME
#define ROCPROFVIS_PYTHON_HOME ""
#endif
#ifndef ROCPROFVIS_PYTHON_STDLIB
#define ROCPROFVIS_PYTHON_STDLIB ""
#endif
#ifndef ROCPROFVIS_PYTHON_STDARCH
#define ROCPROFVIS_PYTHON_STDARCH ""
#endif
    if(!home || home[0] == '\0')
    {
        home = ROCPROFVIS_PYTHON_HOME;
    }

    PyStatus status = PyStatus_Ok();
    if(home && home[0] != '\0')
    {
        status = PyConfig_SetBytesString(&config, &config.home, home);
    }
    if(!PyStatus_Exception(status))
    {
        status = PyConfig_SetBytesString(&config, &config.program_name,
                                         "roc-optiq");
    }

    char const* stdlib  = ROCPROFVIS_PYTHON_STDLIB;
    char const* stdarch = ROCPROFVIS_PYTHON_STDARCH;
    if(!PyStatus_Exception(status) && stdlib && stdlib[0] != '\0')
    {
        config.module_search_paths_set = 1;
        wchar_t* w_stdlib              = DecodePath(stdlib);
        if(w_stdlib)
        {
            status = PyWideStringList_Append(&config.module_search_paths,
                                             w_stdlib);
            PyMem_RawFree(w_stdlib);
        }
        if(!PyStatus_Exception(status) && stdarch && stdarch[0] != '\0')
        {
            wchar_t* w_stdarch = DecodePath(stdarch);
            if(w_stdarch)
            {
                status = PyWideStringList_Append(&config.module_search_paths,
                                                 w_stdarch);
                PyMem_RawFree(w_stdarch);
            }
        }
    }

    if(!PyStatus_Exception(status))
    {
        status = Py_InitializeFromConfig(&config);
    }
    PyConfig_Clear(&config);
    if(PyStatus_Exception(status))
    {
        spdlog::error("Python init failed: {}",
                      status.err_msg ? status.err_msg : "unknown");
        return false;
    }
    return true;
}

void
Runtime::FinalizeInterpreter()
{
    if(Py_IsInitialized())
    {
        Py_FinalizeEx();
    }
}

PyObject*
Runtime::MakeScriptGlobals()
{
    PyObject* globals = PyDict_New();
    if(!globals)
    {
        return nullptr;
    }

    PyObject* builtins = PyEval_GetBuiltins();
    PyObject* safe     = PyDict_New();
    if(!safe)
    {
        Py_DECREF(globals);
        return nullptr;
    }
    for(char const* name : SAFE_BUILTIN_NAMES)
    {
        PyObject* value = PyDict_GetItemString(builtins, name);
        if(value)
        {
            PyDict_SetItemString(safe, name, value);
        }
    }

    PyObject* import_fn = PyCFunction_New(&IMPORT_METHOD, nullptr);
    if(import_fn)
    {
        PyDict_SetItemString(safe, "__import__", import_fn);
        Py_DECREF(import_fn);
    }
    PyDict_SetItemString(globals, "__builtins__", safe);
    Py_DECREF(safe);

    PyObject* script_name = PyUnicode_FromString("__optiq_script__");
    if(script_name)
    {
        PyDict_SetItemString(globals, "__name__", script_name);
        Py_DECREF(script_name);
    }

    for(char const* name : ALLOWLISTED_MODULES)
    {
        if(std::strcmp(name, "optiq") == 0)
        {
            continue;
        }
        PyObject* mod = PyImport_ImportModule(name);
        if(mod)
        {
            PyDict_SetItemString(globals, name, mod);
            Py_DECREF(mod);
        }
        else
        {
            PyErr_Clear();
        }
    }
    return globals;
}

/*
 * Raises KeyboardInterrupt inside the interpreter thread.
 *
 * Deliberately not PyErr_SetInterrupt. That trips the SIGINT flag for
 * PyErr_CheckSignals to act on, but the isolated config sets
 * install_signal_handlers to 0, so no Python-level handler is registered for
 * it. CPython then refuses the signal - "OSError: Signal 2 ignored due to race
 * condition" - and that ignored-handler path clears the error indicator on its
 * way out, wiping the exception set here.
 *
 * Raising into the thread takes effect the next time that thread runs a
 * bytecode. A script parked in a fetch has released the GIL, so it only sees
 * this once the binding hands control back to the interpreter.
 *
 * Needs the GIL, so it runs on the watchdog rather than on whoever asked to
 * cancel - usually the UI thread, which must not wait on it.
 */
void
Runtime::RaiseInInterpreter()
{
    if(!Py_IsInitialized())
    {
        return;
    }

    unsigned long ident = 0;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        ident = m_py_thread_ident;
    }
    if(ident == 0)
    {
        return;
    }

    PyGILState_STATE gil = PyGILState_Ensure();
    // Under the GIL the interpreter thread cannot be between scripts, so "is it
    // still running" and the raise happen together. Without that, a script
    // which finished while the watchdog waited would leave a raise pending for
    // whatever ran next.
    bool still_running = false;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        still_running = m_exec_active;
    }
    if(still_running)
    {
        // More than one means the exception was set on several frames, which
        // CPython requires be reverted before it is safe to try again.
        if(PyThreadState_SetAsyncExc(ident, PyExc_KeyboardInterrupt) > 1)
        {
            PyThreadState_SetAsyncExc(ident, nullptr);
        }
    }
    PyGILState_Release(gil);
}

void
Runtime::BeginExecDeadline(uint64_t timeout_ms)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_exec_active         = true;
    m_exec_timed_out      = false;
    m_interrupt_requested = false;
    m_next_interrupt      = std::chrono::steady_clock::now() +
                       std::chrono::milliseconds(timeout_ms);
    m_watchdog_cv.notify_all();
}

// Returns whether the watchdog had already fired.
bool
Runtime::EndExecDeadline()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_exec_active = false;
    m_watchdog_cv.notify_all();
    return m_exec_timed_out;
}

void
Runtime::ExecWork(WorkItem& item)
{
    rocprofvis_python_result_t result = kRocProfVisPythonError;
    std::string                error;

    PyGILState_STATE gil = PyGILState_Ensure();
    // The watchdog can fire between the last script ending and this one
    // starting, so drop anything still pending rather than letting it land on a
    // script that has not run a line yet. Passing null clears the async
    // exception; PyErr_Clear alone does not reach it.
    PyErr_CheckSignals();
    PyErr_Clear();
    PyThreadState_SetAsyncExc(PyThread_get_thread_ident(), nullptr);

    PyObject* globals = MakeScriptGlobals();
    if(!globals)
    {
        error = PyErr_Occurred() ? FormatPythonError()
                                 : "could not build the script environment";
    }
    else
    {
        if(item.prepare_globals)
        {
            item.prepare_globals(globals, item.user);
        }

        const uint64_t timeout_ms =
            item.timeout_ms == 0 ? SCRIPT_TIMEOUT_MS : item.timeout_ms;
        BeginExecDeadline(timeout_ms);
        PyObject* py_result =
            PyRun_String(item.source.c_str(), Py_file_input, globals, globals);
        const bool timed_out = EndExecDeadline();

        if(py_result)
        {
            Py_DECREF(py_result);
            result = kRocProfVisPythonSuccess;
        }
        else if(PyErr_ExceptionMatches(PyExc_KeyboardInterrupt))
        {
            PyErr_Clear();
            if(timed_out)
            {
                // A timeout is a script that needs fixing, not a user walking
                // away, so it is reported as a failure and says how long it ran.
                result = kRocProfVisPythonError;
                error =
                    "script timed out after " + DescribeDuration(timeout_ms) +
                    " and was stopped";
            }
            else
            {
                result = kRocProfVisPythonCancelled;
                error  = "script cancelled";
            }
        }
        else
        {
            error  = FormatPythonError();
            result = kRocProfVisPythonError;
        }
        Py_DECREF(globals);
    }
    PyGILState_Release(gil);

    if(item.done)
    {
        item.done(item.user, result, error.empty() ? nullptr : error.c_str());
    }
}

// Stops a script that has outstayed its deadline, and delivers an explicit
// cancel on behalf of whoever asked for one. Both go out through
// RaiseInInterpreter, and both are re-sent on an interval because a bare
// `except:` swallows the first raise.
void
Runtime::WatchdogMain()
{
    while(true)
    {
        bool fire = false;
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            if(!m_exec_active)
            {
                if(m_stop)
                {
                    break;
                }
                // Nothing to deliver a cancel to, and it must not carry over
                // to whatever runs next.
                m_interrupt_requested = false;
                m_watchdog_cv.wait(lock,
                                   [this]() { return m_stop || m_exec_active; });
                continue;
            }

            // Shutdown joins the interpreter thread, so a script that never
            // ends would hold the join open. Clamped rather than zeroed so
            // this still fires at most once per retry.
            if(m_stop)
            {
                const std::chrono::steady_clock::time_point soon =
                    std::chrono::steady_clock::now() +
                    std::chrono::milliseconds(INTERRUPT_RETRY_MS);
                if(m_next_interrupt > soon)
                {
                    m_next_interrupt = soon;
                }
            }

            // Stopping is deliberately not part of the predicate: a hung
            // script has to be interrupted, not left behind.
            const bool woke_early =
                m_watchdog_cv.wait_until(lock, m_next_interrupt, [this]() {
                    return !m_exec_active || m_interrupt_requested;
                });

            if(!m_exec_active)
            {
                // Finished on its own while we waited.
                m_interrupt_requested = false;
                continue;
            }

            // Woken early means an explicit cancel, delivered the same way but
            // reported as cancelled rather than as a script to go and fix.
            if(!woke_early)
            {
                m_exec_timed_out = true;
            }
            m_interrupt_requested = false;
            m_next_interrupt      = std::chrono::steady_clock::now() +
                               std::chrono::milliseconds(INTERRUPT_RETRY_MS);
            fire                  = true;
        }
        if(fire)
        {
            RaiseInInterpreter();
        }
    }
}

void
Runtime::ThreadMain()
{
    m_thread_id =
        static_cast<unsigned long long>(
            std::hash<std::thread::id>{}(std::this_thread::get_id()));

    bool ok = InitializeInterpreter();
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_init_ok     = ok;
        m_init_result = ok ? kRocProfVisPythonSuccess : kRocProfVisPythonError;
        m_initialized = true;
        // Python's own id for this thread, which is what the watchdog raises
        // into. Only meaningful once the interpreter is up.
        if(ok)
        {
            m_py_thread_ident = PyThread_get_thread_ident();
        }
    }
    m_cv.notify_all();
    if(!ok)
    {
        return;
    }

    PyThreadState* saved = PyEval_SaveThread();

    while(true)
    {
        WorkItem item;
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_cv.wait(lock, [this]() { return m_stop || !m_queue.empty(); });
            if(m_stop && m_queue.empty())
            {
                break;
            }
            item = std::move(m_queue.front());
            m_queue.pop();
        }
        ExecWork(item);
    }

    PyEval_RestoreThread(saved);
    FinalizeInterpreter();
}

rocprofvis_python_result_t
Runtime::Init(char const* runtime_root)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    if(m_running)
    {
        // A failed startup leaves m_running set but no thread draining the
        // queue, so reporting success here would park every later script on a
        // future that can never complete.
        return m_init_ok ? kRocProfVisPythonSuccess : m_init_result;
    }
    m_runtime_root = runtime_root ? runtime_root : "";
    m_stop         = false;
    m_initialized  = false;
    m_init_ok      = false;
    try
    {
        m_thread  = std::thread([this]() { ThreadMain(); });
        m_running = true;
    }
    catch(std::exception const&)
    {
        spdlog::error("Failed to start Python interpreter thread");
        return kRocProfVisPythonError;
    }
    try
    {
        m_watchdog = std::thread([this]() { WatchdogMain(); });
    }
    catch(std::exception const&)
    {
        // Scripts still run, they just cannot be stopped on a deadline. Better
        // than refusing to start the interpreter that is already up.
        spdlog::error("Failed to start Python watchdog thread; scripts will not "
                      "time out");
    }
    m_cv.wait(lock, [this]() { return m_initialized; });
    return m_init_ok ? kRocProfVisPythonSuccess : m_init_result;
}

rocprofvis_python_result_t
Runtime::Post(char const* source, rocprofvis_python_prepare_globals_t prepare,
              void* user, rocprofvis_python_done_t done, uint64_t timeout_ms)
{
    if(!source || !done)
    {
        return kRocProfVisPythonInvalidArgument;
    }
    rocprofvis_python_result_t init_result = Init(nullptr);
    if(init_result != kRocProfVisPythonSuccess)
    {
        return init_result;
    }
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if(!m_running || m_stop)
        {
            return kRocProfVisPythonNotInitialized;
        }
        WorkItem item;
        item.source          = source;
        item.prepare_globals = prepare;
        item.user            = user;
        item.done            = done;
        item.timeout_ms      = timeout_ms;
        m_queue.push(std::move(item));
    }
    m_cv.notify_one();
    return kRocProfVisPythonSuccess;
}

// Asks for the running script to stop. Delivery is the watchdog's job, because
// raising into the interpreter needs the GIL and this is usually called from
// the UI thread, which must not wait on it.
void
Runtime::Interrupt()
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if(!m_exec_active)
        {
            return;
        }
        m_interrupt_requested = true;
    }
    m_watchdog_cv.notify_all();
}

void
Runtime::Shutdown()
{
    std::thread joining;
    std::thread joining_watchdog;
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        if(!m_running)
        {
            return;
        }
        m_stop = true;
        joining.swap(m_thread);
        joining_watchdog.swap(m_watchdog);
        m_running = false;
    }
    m_cv.notify_all();
    m_watchdog_cv.notify_all();
    if(joining.joinable())
    {
        joining.join();
    }
    if(joining_watchdog.joinable())
    {
        joining_watchdog.join();
    }
    m_thread_id       = 0;
    m_py_thread_ident = 0;
    m_initialized     = false;
}

unsigned long long
Runtime::InterpreterThreadId() const
{
    return m_thread_id;
}

}  // namespace

}  // namespace Python
}  // namespace RocProfVis

extern "C"
{

rocprofvis_python_result_t
rocprofvis_python_init(char const* runtime_root)
{
    return RocProfVis::Python::Runtime::Get().Init(runtime_root);
}

rocprofvis_python_result_t
rocprofvis_python_exec(char const* source,
                       rocprofvis_python_prepare_globals_t prepare_globals,
                       void* user, rocprofvis_python_done_t done,
                       unsigned long long timeout_ms)
{
    return RocProfVis::Python::Runtime::Get().Post(source, prepare_globals,
                                                   user, done, timeout_ms);
}

void
rocprofvis_python_interrupt(void)
{
    RocProfVis::Python::Runtime::Get().Interrupt();
}

void
rocprofvis_python_shutdown(void)
{
    RocProfVis::Python::Runtime::Get().Shutdown();
}

unsigned long long
rocprofvis_python_interpreter_thread_id(void)
{
    return RocProfVis::Python::Runtime::Get().InterpreterThreadId();
}

}
