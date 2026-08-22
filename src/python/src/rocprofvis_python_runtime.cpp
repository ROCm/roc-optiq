// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_python_runtime.h"
#include "rocprofvis_core.h"
#include "rocprofvis_core_assert.h"

#include <condition_variable>
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

char const* const kAllowlistedModules[] = {
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
    for(char const* allowed : kAllowlistedModules)
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

PyMethodDef kImportMethod = {
    "__import__", reinterpret_cast<PyCFunction>(restricted_import),
    METH_VARARGS | METH_KEYWORDS, nullptr};

char const* const kSafeBuiltinNames[] = {
    "None",       "True",      "False",      "abs",        "all",
    "any",        "bool",      "chr",        "dict",       "divmod",
    "enumerate",  "filter",    "float",      "format",     "frozenset",
    "hasattr",    "hash",      "hex",        "id",         "int",
    "isinstance", "issubclass","iter",       "len",        "list",
    "map",        "max",       "min",        "next",       "oct",
    "ord",        "pow",       "range",      "repr",       "reversed",
    "round",      "set",       "slice",      "sorted",     "str",
    "sum",        "tuple",     "zip",        "Exception",  "ValueError",
    "TypeError",  "RuntimeError", "ImportError", "StopIteration",
    "KeyError",   "IndexError", "ArithmeticError", "OverflowError",
    "ZeroDivisionError", "AssertionError", "AttributeError", "NameError",
};

struct WorkItem
{
    std::string                         source;
    rocprofvis_python_prepare_globals_t prepare_globals;
    void*                               user;
    rocprofvis_python_done_t            done;
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
                                    void* user, rocprofvis_python_done_t done);
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
    for(char const* name : kSafeBuiltinNames)
    {
        PyObject* value = PyDict_GetItemString(builtins, name);
        if(value)
        {
            PyDict_SetItemString(safe, name, value);
        }
    }

    PyObject* import_fn =
        PyCFunction_New(&kImportMethod, nullptr);
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

    for(char const* name : kAllowlistedModules)
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

void
Runtime::ExecWork(WorkItem& item)
{
    rocprofvis_python_result_t result = kRocProfVisPythonError;
    std::string                error;

    PyGILState_STATE gil = PyGILState_Ensure();
    PyObject*        globals = MakeScriptGlobals();
    if(globals)
    {
        if(item.prepare_globals)
        {
            item.prepare_globals(globals, item.user);
        }
        PyObject* py_result =
            PyRun_String(item.source.c_str(), Py_file_input, globals, globals);
        if(py_result)
        {
            Py_DECREF(py_result);
            result = kRocProfVisPythonSuccess;
        }
        else
        {
            if(PyErr_ExceptionMatches(PyExc_KeyboardInterrupt))
            {
                result = kRocProfVisPythonCancelled;
                PyErr_Clear();
                error = "script cancelled";
            }
            else
            {
                PyObject* type    = nullptr;
                PyObject* value   = nullptr;
                PyObject* traceback = nullptr;
                PyErr_Fetch(&type, &value, &traceback);
                PyErr_NormalizeException(&type, &value, &traceback);
                PyObject* str = value ? PyObject_Str(value) : nullptr;
                char const* msg =
                    str ? PyUnicode_AsUTF8(str) : "script error";
                error = msg ? msg : "script error";
                Py_XDECREF(str);
                Py_XDECREF(type);
                Py_XDECREF(value);
                Py_XDECREF(traceback);
                result = kRocProfVisPythonError;
            }
        }
        Py_DECREF(globals);
    }
    PyGILState_Release(gil);

    if(item.done)
    {
        item.done(item.user, result, error.empty() ? nullptr : error.c_str());
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
        return kRocProfVisPythonSuccess;
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
    m_cv.wait(lock, [this]() { return m_initialized; });
    return m_init_ok ? kRocProfVisPythonSuccess : m_init_result;
}

rocprofvis_python_result_t
Runtime::Post(char const* source, rocprofvis_python_prepare_globals_t prepare,
              void* user, rocprofvis_python_done_t done)
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
        m_queue.push(std::move(item));
    }
    m_cv.notify_one();
    return kRocProfVisPythonSuccess;
}

void
Runtime::Interrupt()
{
    if(Py_IsInitialized())
    {
        PyErr_SetInterrupt();
    }
}

void
Runtime::Shutdown()
{
    std::thread joining;
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        if(!m_running)
        {
            return;
        }
        m_stop = true;
        joining.swap(m_thread);
        m_running = false;
    }
    m_cv.notify_all();
    if(joining.joinable())
    {
        joining.join();
    }
    m_thread_id   = 0;
    m_initialized = false;
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
                       void* user, rocprofvis_python_done_t done)
{
    return RocProfVis::Python::Runtime::Get().Post(source, prepare_globals,
                                                   user, done);
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
