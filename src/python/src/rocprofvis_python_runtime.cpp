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
#include <filesystem>
#include <functional>
#include <mutex>
#include <queue>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

#if defined(_WIN32)
#    ifndef NOMINMAX
#        define NOMINMAX
#    endif
#    ifndef WIN32_LEAN_AND_MEAN
#        define WIN32_LEAN_AND_MEAN
#    endif
#    include <windows.h>
#elif defined(__APPLE__)
#    include <mach-o/dyld.h>
#endif

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

// The longest path Windows hands back once long paths are enabled.
uint32_t const MAX_EXECUTABLE_PATH = 32768;

// A vendored build sets ROCPROFVIS_PYTHON_RELDIR to where the package keeps the
// interpreter, relative to the executable. A build against the build machine's
// Python sets the ROCPROFVIS_PYTHON_HOME / STDLIB / STDARCH paths instead.
#ifndef ROCPROFVIS_PYTHON_RELDIR
#    define ROCPROFVIS_PYTHON_RELDIR ""
#endif
#ifndef ROCPROFVIS_PYTHON_HOME
#    define ROCPROFVIS_PYTHON_HOME ""
#endif
#ifndef ROCPROFVIS_PYTHON_STDLIB
#    define ROCPROFVIS_PYTHON_STDLIB ""
#endif
#ifndef ROCPROFVIS_PYTHON_STDARCH
#    define ROCPROFVIS_PYTHON_STDARCH ""
#endif
char const* const PYTHON_RELDIR         = ROCPROFVIS_PYTHON_RELDIR;
char const* const SYSTEM_PYTHON_HOME    = ROCPROFVIS_PYTHON_HOME;
char const* const SYSTEM_PYTHON_STDLIB  = ROCPROFVIS_PYTHON_STDLIB;
char const* const SYSTEM_PYTHON_STDARCH = ROCPROFVIS_PYTHON_STDARCH;

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

// optiq is the one allowlisted module that is not standard library. The
// controller builds it for each run and it holds only the trace bindings, so
// it is handed over as it is rather than through a proxy.
bool
is_optiq_module(char const* name)
{
    return std::strcmp(name, "optiq") == 0 || std::strncmp(name, "optiq.", 6) == 0;
}

/*
 * The script's __import__. self is the _import function compiled from
 * MODULE_PROXY_SOURCE, bound when the script builtins are built, so every
 * standard library import resolves to a proxy and never to the module itself.
 */
PyObject*
restricted_import(PyObject* self, PyObject* args, PyObject* kwargs)
{
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
    if(is_optiq_module(name))
    {
        return PyImport_ImportModuleLevel(name, globals, locals,
                                          fromlist ? fromlist : Py_None, level);
    }
    if(level != 0)
    {
        // A script is not part of a package, so there is nothing for a
        // relative import to be relative to.
        return PyErr_Format(PyExc_ImportError,
                            "relative import of '%s' is not allowed in optiq "
                            "scripts",
                            name);
    }
    if(!self)
    {
        // Without the proxy importer the only thing left to return is the
        // real module, which is exactly what must not reach a script.
        return PyErr_Format(PyExc_ImportError,
                            "module '%s' is unavailable: the module proxies "
                            "were not built",
                            name);
    }
    return PyObject_CallFunction(self, "sO", name, fromlist ? fromlist : Py_None);
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

/*
 * Source screen, run over the parse tree before a script executes.
 *
 * Neither the import allowlist nor the reduced builtins bound what a script can
 * reach, because what an allowlisted module exposes are real functions and
 * classes (see MODULE_PROXY_SOURCE for what is held back): from any function
 * on it, __globals__ leads back to the unrestricted builtins, and from
 * any instance, __class__ leads to object.__subclasses__() and everything the
 * interpreter has already imported. Both are one expression:
 *
 *   json.dumps.__globals__['__builtins__']['__import__']('os')
 *   ().__class__.__base__.__subclasses__()
 *
 * Attribute access is the only way to walk either chain - getattr, vars, eval
 * and __import__ are all withheld - so refusing these names by parse tree
 * closes the published routes without an AST rewrite. Names, not values: the
 * screen cannot know what an expression evaluates to, and does not try.
 *
 * This raises the cost of an escape; it does not make one impossible. See the
 * threat model in .agents/SCRIPTING.md - approval before a run is the boundary
 * that matters, and this is defence behind it.
 */
char const* const SOURCE_SCREEN_SOURCE = R"PY(
import ast

_BLOCKED = frozenset((
    '__globals__', '__builtins__', '__subclasses__', '__bases__', '__base__',
    '__mro__', '__class__', '__code__', '__closure__', '__func__', '__self__',
    '__dict__', '__getattribute__', '__getattr__', '__setattr__',
    '__delattr__', '__reduce__', '__reduce_ex__', '__import__', '__loader__',
    '__spec__', '__init_subclass__', '__subclasshook__', '__wrapped__',
))

def _screen(source):
    for node in ast.walk(ast.parse(source)):
        if isinstance(node, ast.Attribute):
            if node.attr in _BLOCKED:
                return (node.attr, getattr(node, 'lineno', 0))
        elif isinstance(node, ast.Name):
            if node.id in _BLOCKED:
                return (node.id, getattr(node, 'lineno', 0))
    return None
)PY";

/*
 * Module proxies, built once at init. A script never holds a standard library
 * module itself, only a stand-in carrying that module's public names.
 *
 * The real modules keep the modules they import as plain attributes, and the
 * source screen has no reason to refuse a plain attribute. statistics.random,
 * typing.sys, collections._sys and enum.bltns each lead to os or to the
 * unrestricted builtins in one expression with no screened name in it. A list
 * of names to refuse cannot keep up with that, so the proxy copies by rule
 * instead: only the names in __all__ where the module defines one, never a
 * name that starts with an underscore, and never a value that is a module.
 *
 * A submodule is the one module-valued attribute a script may reach, and it
 * gets a proxy of its own built by the same rule: collections.abc works, and
 * what it hands back is not the real collections.abc. _import attaches those
 * proxies to their parent before it returns, because `from json import
 * decoder` falls back to sys.modules when the attribute is missing and would
 * find the real module there. _build imports every module before it proxies
 * any, because one allowlisted module importing another's submodule (typing
 * imports collections.abc) is what makes that submodule an attribute at all.
 *
 * A module this Python does not have is skipped, and importing it later fails
 * the ordinary way. Any other failure stops init: see BuildModuleProxies.
 *
 * operator.attrgetter and operator.methodcaller are left out because they are
 * getattr with the name in a string, which the source screen never inspects.
 *
 * The functions and classes copied across are the real ones and still refer
 * to their own module internally. That is what keeps them working, and it is
 * also why this is defence behind approval rather than containment - see the
 * threat model in .agents/SCRIPTING.md.
 *
 * The proxies live as long as the interpreter, so what one script sets on a
 * proxy is still there for the next. That was already true of the modules
 * they replace, and the run generation check in the bindings covers it.
 */
char const* const MODULE_PROXY_SOURCE = R"PY(
import importlib
import sys
import types

_OMITTED = {'operator': frozenset(('attrgetter', 'methodcaller'))}
_PROXIES = {}

def _proxy(module, name):
    proxy = _PROXIES.get(name)
    if proxy is not None:
        return proxy
    proxy = types.ModuleType(name, getattr(module, '__doc__', None))
    _PROXIES[name] = proxy
    names = getattr(module, '__all__', None)
    if names is None:
        names = list(vars(module))
    omitted = _OMITTED.get(name, ())
    for attr in names:
        if not isinstance(attr, str) or attr.startswith('_') or attr in omitted:
            continue
        try:
            value = getattr(module, attr)
        except AttributeError:
            continue
        if not isinstance(value, types.ModuleType):
            setattr(proxy, attr, value)
    for attr, value in list(vars(module).items()):
        if not isinstance(attr, str) or attr.startswith('_'):
            continue
        if not isinstance(value, types.ModuleType):
            continue
        child_name = name + '.' + attr
        if getattr(value, '__name__', None) == child_name:
            setattr(proxy, attr, _proxy(value, child_name))
    return proxy

def _build(names):
    modules = {}
    for name in names:
        try:
            modules[name] = importlib.import_module(name)
        except ImportError:
            continue
    top = {}
    for name, module in modules.items():
        top[name] = _proxy(module, name)
    return top

def _import(name, fromlist):
    importlib.import_module(name)
    parts = name.split('.')
    proxy = None
    for count in range(1, len(parts) + 1):
        prefix = '.'.join(parts[:count])
        child = _proxy(sys.modules[prefix], prefix)
        if proxy is not None:
            setattr(proxy, parts[count - 1], child)
        proxy = child
    if not fromlist:
        return _PROXIES[parts[0]]
    for attr in fromlist:
        if not isinstance(attr, str) or hasattr(proxy, attr):
            continue
        child_name = name + '.' + attr
        try:
            importlib.import_module(child_name)
        except ImportError:
            continue
        setattr(proxy, attr, _proxy(sys.modules[child_name], child_name))
    return proxy
)PY";

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
    std::string                          source;
    rocprofvis_python_prepare_globals_t  prepare_globals;
    rocprofvis_python_teardown_globals_t teardown_globals;
    void*                                user;
    rocprofvis_python_done_t             done;
    uint64_t                             timeout_ms;
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
                                    rocprofvis_python_teardown_globals_t teardown,
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
    bool      BuildSourceScreen();
    bool      BuildModuleProxies();
    bool      ScreenSource(std::string const& source, std::string& error);

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
    // The script's own budget, kept apart from m_next_interrupt because the
    // latter is rescheduled on every retry. Only this one says "timed out".
    std::chrono::steady_clock::time_point m_exec_deadline;
    // Python's own id for the interpreter thread, which is what
    // PyThreadState_SetAsyncExc addresses. Written once during init.
    unsigned long                         m_py_thread_ident = 0;

    // Compiled once at init and owned by the interpreter thread: the parse-tree
    // screen every script passes through before it runs.
    PyObject*                             m_screen_fn = nullptr;

    // Built once at init and owned by the interpreter thread, like the screen.
    // m_module_proxies maps each allowlisted standard library module to the
    // proxy a script gets in place of it, and m_proxy_import_fn is what the
    // script's __import__ calls to resolve an import to one.
    PyObject*                             m_module_proxies  = nullptr;
    PyObject*                             m_proxy_import_fn = nullptr;
};

// Where the interpreter's standard library lives. stdarch is empty unless a
// system install keeps platform modules apart from the pure-Python ones.
struct python_layout_t
{
    std::filesystem::path home;
    std::filesystem::path stdlib;
    std::filesystem::path stdarch;
};

/*
 * Converts a path to the wide string PyConfig takes. Returns null when the
 * path cannot be decoded. Release the result with free_wide_path.
 */
wchar_t*
alloc_wide_path(std::filesystem::path const& path)
{
    wchar_t* wide = nullptr;
#if defined(_WIN32)
    std::wstring const& native = path.native();
    size_t const        bytes  = (native.size() + 1) * sizeof(wchar_t);
    wide                       = static_cast<wchar_t*>(PyMem_RawMalloc(bytes));
    if(wide)
    {
        std::memcpy(wide, native.c_str(), bytes);
    }
#else
    wide = Py_DecodeLocale(path.c_str(), nullptr);
#endif
    return wide;
}

void
free_wide_path(wchar_t* wide)
{
    PyMem_RawFree(wide);
}

PyStatus
set_config_path(PyConfig& config, wchar_t** field, std::filesystem::path const& path)
{
    wchar_t* wide = alloc_wide_path(path);
    if(!wide)
    {
        return PyStatus_Error("could not decode a Python runtime path");
    }
    PyStatus status = PyConfig_SetString(&config, field, wide);
    free_wide_path(wide);
    return status;
}

PyStatus
append_search_path(PyConfig& config, std::filesystem::path const& dir)
{
    wchar_t* wide = alloc_wide_path(dir);
    if(!wide)
    {
        return PyStatus_Error("could not decode a Python module path");
    }
    PyStatus status = PyWideStringList_Append(&config.module_search_paths, wide);
    free_wide_path(wide);
    return status;
}

// A directory that is not present is skipped, so a layout without that
// folder still initializes.
PyStatus
append_existing_dir(PyConfig& config, std::filesystem::path const& dir)
{
    std::error_code ec;
    if(!std::filesystem::is_directory(dir, ec))
    {
        return PyStatus_Ok();
    }
    return append_search_path(config, dir);
}

std::filesystem::path
executable_dir()
{
    std::filesystem::path exe;
#if defined(_WIN32)
    std::vector<wchar_t> buffer(MAX_EXECUTABLE_PATH, L'\0');
    DWORD const          length =
        GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if(length > 0 && length < buffer.size())
    {
        exe = std::filesystem::path(buffer.data(), buffer.data() + length);
    }
#elif defined(__APPLE__)
    uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    std::vector<char> buffer(size + 1, '\0');
    if(_NSGetExecutablePath(buffer.data(), &size) == 0)
    {
        exe = buffer.data();
    }
#else
    std::error_code link_ec;
    exe = std::filesystem::read_symlink("/proc/self/exe", link_ec);
#endif
    std::filesystem::path dir;
    if(!exe.empty())
    {
        std::error_code             ec;
        std::filesystem::path const resolved = std::filesystem::weakly_canonical(exe, ec);
        dir = (ec ? exe : resolved).parent_path();
    }
    return dir;
}

// CPython's own landmark: lib/pythonX.Y/os.py on Linux and macOS, Lib/os.py
// on Windows.
bool
layout_from_home(std::filesystem::path const& home, python_layout_t& layout)
{
    std::string const version_dir = "python" + std::to_string(PY_MAJOR_VERSION) +
                                    "." + std::to_string(PY_MINOR_VERSION);
    std::filesystem::path const candidates[] = {home / "lib" / version_dir,
                                                home / "Lib"};
    bool found = false;
    for(std::filesystem::path const& stdlib : candidates)
    {
        std::error_code ec;
        if(!found && std::filesystem::is_regular_file(stdlib / "os.py", ec))
        {
            layout.home   = home;
            layout.stdlib = stdlib;
            layout.stdarch.clear();
            found = true;
        }
    }
    return found;
}

/*
 * An explicit runtime_root wins. Otherwise the runtime shipped with the
 * executable, which sits beside it in the build tree and wherever
 * ROCPROFVIS_PYTHON_RELDIR points in a package. Last, the build machine's
 * Python, which only a build that did not vendor one knows about.
 */
bool
resolve_layout(std::string const& runtime_root, python_layout_t& layout)
{
    bool found = false;
    if(!runtime_root.empty())
    {
        found = layout_from_home(std::filesystem::path(runtime_root), layout);
        if(!found)
        {
            spdlog::error("Python init failed: no standard library under {}",
                          runtime_root);
        }
    }
    else
    {
        std::filesystem::path const exe_dir = executable_dir();
        if(!exe_dir.empty())
        {
            found = layout_from_home(exe_dir / "python", layout);
            if(!found && PYTHON_RELDIR[0] != '\0')
            {
                found = layout_from_home((exe_dir / PYTHON_RELDIR).lexically_normal(),
                                         layout);
            }
        }
        if(!found && SYSTEM_PYTHON_STDLIB[0] != '\0')
        {
            layout.home    = SYSTEM_PYTHON_HOME;
            layout.stdlib  = SYSTEM_PYTHON_STDLIB;
            layout.stdarch = SYSTEM_PYTHON_STDARCH;
            found          = true;
        }
        if(!found)
        {
            spdlog::error("Python init failed: no Python runtime beside {}",
                          exe_dir.generic_u8string());
        }
    }
    return found;
}

bool
Runtime::InitializeInterpreter()
{
    python_layout_t layout;
    if(!resolve_layout(m_runtime_root, layout))
    {
        return false;
    }

    PyConfig config;
    PyConfig_InitIsolatedConfig(&config);
    config.site_import            = 0;
    config.user_site_directory    = 0;
    config.use_environment        = 0;
    config.install_signal_handlers = 0;

    PyStatus status = PyStatus_Ok();
    if(!layout.home.empty())
    {
        status = set_config_path(config, &config.home, layout.home);
    }
    if(!PyStatus_Exception(status))
    {
        status = PyConfig_SetBytesString(&config, &config.program_name,
                                         "roc-optiq");
    }

    config.module_search_paths_set = 1;
    if(!PyStatus_Exception(status))
    {
        status = append_search_path(config, layout.stdlib);
    }
    if(!PyStatus_Exception(status) && !layout.stdarch.empty() &&
       layout.stdarch != layout.stdlib)
    {
        status = append_search_path(config, layout.stdarch);
    }
    // Pinning module_search_paths skips the directories CPython would add
    // for extension modules. _random (imported by statistics) is
    // lib-dynload/_random*.so in a system Linux Python. On Windows those
    // modules live in <prefix>/DLLs.
    if(!PyStatus_Exception(status))
    {
        status = append_existing_dir(config, layout.stdlib / "lib-dynload");
    }
    if(!PyStatus_Exception(status) && !layout.home.empty())
    {
        status = append_existing_dir(config, layout.home / "DLLs");
    }

    if(!PyStatus_Exception(status))
    {
        status = Py_InitializeFromConfig(&config);
    }
    PyConfig_Clear(&config);
    if(PyStatus_Exception(status))
    {
        spdlog::error("Python init failed: {} (stdlib {})",
                      status.err_msg ? status.err_msg : "unknown",
                      layout.stdlib.generic_u8string());
        return false;
    }
    spdlog::info("Python runtime: {}", layout.stdlib.generic_u8string());

    // Compiled once, here, rather than per exec. ScreenSource fails closed
    // without it, so a runtime that cannot build the screen is not usable.
    if(!BuildSourceScreen())
    {
        spdlog::error("Python init failed: could not compile the script screen");
        FinalizeInterpreter();
        return false;
    }
    // Same reasoning: MakeScriptGlobals refuses to build an environment
    // without the proxies, because the alternative is handing a script the
    // real modules.
    if(!BuildModuleProxies())
    {
        spdlog::error("Python init failed: could not build the module proxies");
        FinalizeInterpreter();
        return false;
    }
    return true;
}

void
Runtime::FinalizeInterpreter()
{
    if(Py_IsInitialized())
    {
        Py_CLEAR(m_screen_fn);
        Py_CLEAR(m_module_proxies);
        Py_CLEAR(m_proxy_import_fn);
        Py_FinalizeEx();
    }
    m_screen_fn       = nullptr;
    m_module_proxies  = nullptr;
    m_proxy_import_fn = nullptr;
}

// Compiles the screen into a throwaway module dict with the real builtins, so
// it keeps working when a script is handed the reduced set. Runs on the
// interpreter thread with the GIL held.
bool
Runtime::BuildSourceScreen()
{
    PyObject* globals = PyDict_New();
    if(!globals)
    {
        PyErr_Clear();
        return false;
    }
    PyDict_SetItemString(globals, "__builtins__", PyEval_GetBuiltins());

    PyObject* code = PyRun_String(SOURCE_SCREEN_SOURCE, Py_file_input, globals, globals);
    if(!code)
    {
        PyErr_Clear();
        Py_DECREF(globals);
        return false;
    }
    Py_DECREF(code);

    // Borrowed from the dict, so hold a reference of our own before it goes.
    PyObject* screen = PyDict_GetItemString(globals, "_screen");
    Py_XINCREF(screen);
    m_screen_fn = screen;
    Py_DECREF(globals);
    return m_screen_fn != nullptr;
}

// Compiles MODULE_PROXY_SOURCE the same way as the screen and builds a proxy
// for every allowlisted standard library module. Returns false unless both the
// proxies and the importer are in hand, and leaves neither behind in that
// case. Runs on the interpreter thread with the GIL held.
bool
Runtime::BuildModuleProxies()
{
    PyObject* globals = PyDict_New();
    if(!globals)
    {
        PyErr_Clear();
        return false;
    }
    PyDict_SetItemString(globals, "__builtins__", PyEval_GetBuiltins());

    PyObject* code = PyRun_String(MODULE_PROXY_SOURCE, Py_file_input, globals, globals);
    if(!code)
    {
        spdlog::error("Python module proxies: {}", FormatPythonError());
        Py_DECREF(globals);
        return false;
    }
    Py_DECREF(code);

    PyObject* names = PyList_New(0);
    bool      ok    = names != nullptr;
    for(char const* name : ALLOWLISTED_MODULES)
    {
        // optiq is not standard library and does not exist until a run
        // injects it, so there is nothing to proxy.
        if(ok && !is_optiq_module(name))
        {
            PyObject* text = PyUnicode_FromString(name);
            ok             = text && PyList_Append(names, text) == 0;
            Py_XDECREF(text);
        }
    }

    // Both borrowed from the dict. The functions keep the dict alive through
    // their own globals once it is released below.
    PyObject* build     = ok ? PyDict_GetItemString(globals, "_build") : nullptr;
    PyObject* import_fn = ok ? PyDict_GetItemString(globals, "_import") : nullptr;
    PyObject* proxies   = nullptr;
    if(build && import_fn)
    {
        proxies = PyObject_CallFunctionObjArgs(build, names, nullptr);
    }

    if(proxies && PyDict_Check(proxies))
    {
        Py_INCREF(import_fn);
        m_proxy_import_fn = import_fn;
        m_module_proxies  = proxies;
    }
    else
    {
        if(PyErr_Occurred())
        {
            spdlog::error("Python module proxies: {}", FormatPythonError());
        }
        Py_XDECREF(proxies);
    }
    Py_XDECREF(names);
    Py_DECREF(globals);
    return m_module_proxies != nullptr && m_proxy_import_fn != nullptr;
}

/*
 * Refuses a script that names one of the interpreter-internal attributes.
 * Returns false and fills error when the script must not run.
 *
 * A SyntaxError here is reported the same way, because the script was never
 * going to run: the message is the parser's own, so it still names the line.
 */
bool
Runtime::ScreenSource(std::string const& source, std::string& error)
{
    if(!m_screen_fn)
    {
        // Without the screen there is no guardrail to enforce, and running
        // anyway would silently drop it. Fail closed.
        error = "the script screen is unavailable, so the script was not run";
        return false;
    }

    PyObject* found = PyObject_CallFunction(m_screen_fn, "s#", source.c_str(),
                                            static_cast<Py_ssize_t>(source.size()));
    if(!found)
    {
        error = FormatPythonError();
        return false;
    }
    if(found == Py_None)
    {
        Py_DECREF(found);
        return true;
    }

    char const*   name = nullptr;
    long long     line = 0;
    PyObject*     name_obj = PyTuple_Check(found) && PyTuple_GET_SIZE(found) == 2
                                 ? PyTuple_GET_ITEM(found, 0)
                                 : nullptr;
    PyObject*     line_obj = name_obj ? PyTuple_GET_ITEM(found, 1) : nullptr;
    if(name_obj)
    {
        name = PyUnicode_AsUTF8(name_obj);
    }
    if(line_obj)
    {
        line = PyLong_AsLongLong(line_obj);
    }
    PyErr_Clear();

    error = "line " + std::to_string(line) + ": '" +
            std::string(name ? name : "?") +
            "' is not available to optiq scripts. It reaches interpreter "
            "internals rather than trace data; use the documented optiq API.";
    Py_DECREF(found);
    return false;
}

PyObject*
Runtime::MakeScriptGlobals()
{
    if(!m_module_proxies || !m_proxy_import_fn)
    {
        // Init refuses to finish without these, so this is not expected. It is
        // checked anyway because the fallback would be the real modules. Fail
        // closed, as ScreenSource does.
        PyErr_SetString(PyExc_RuntimeError,
                        "the module proxies are unavailable, so the script was "
                        "not run");
        return nullptr;
    }

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

    // Bound to the proxy importer, which restricted_import receives as self.
    PyObject* import_fn = PyCFunction_New(&IMPORT_METHOD, m_proxy_import_fn);
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
        // The proxy, never the module. A name with no proxy is a module this
        // Python does not have, and is left out as it always was. Borrowed.
        PyObject* proxy = PyDict_GetItemString(m_module_proxies, name);
        if(proxy && PyDict_SetItemString(globals, name, proxy) != 0)
        {
            Py_DECREF(globals);
            return nullptr;
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
    m_exec_deadline       = std::chrono::steady_clock::now() +
                      std::chrono::milliseconds(timeout_ms);
    m_next_interrupt = m_exec_deadline;
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

    PyObject* globals = ScreenSource(item.source, error) ? MakeScriptGlobals() : nullptr;
    if(!globals)
    {
        if(error.empty())
        {
            error = PyErr_Occurred() ? FormatPythonError()
                                     : "could not build the script environment";
        }
    }
    else
    {
        // Anything but success skips the run. A hook that could not finish the
        // environment must not let the script go ahead: it would resolve
        // `optiq` from whatever the last run left in sys.modules, which the
        // interpreter keeps for the life of the process and whose session is
        // long freed. The same door reports a script cancelled while it was
        // still queued, which no interrupt can reach - the watchdog only
        // raises once exec is under way.
        const rocprofvis_python_result_t prepared =
            item.prepare_globals ? item.prepare_globals(globals, item.user)
                                 : kRocProfVisPythonSuccess;
        if(prepared == kRocProfVisPythonCancelled)
        {
            result = kRocProfVisPythonCancelled;
            error  = "script cancelled";
            PyErr_Clear();
        }
        else if(prepared != kRocProfVisPythonSuccess)
        {
            result = prepared;
            error  = PyErr_Occurred() ? FormatPythonError()
                                      : "could not build the script environment";
        }
        else
        {
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
        }

        // Before the globals go, and before `done` lets the caller free what
        // the script was reading. Runs on the failed-prepare path too, since a
        // hook can register part of an environment and then give up.
        if(item.teardown_globals)
        {
            item.teardown_globals(globals, item.user);
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
            m_watchdog_cv.wait_until(lock, m_next_interrupt, [this]() {
                return !m_exec_active || m_interrupt_requested;
            });

            if(!m_exec_active)
            {
                // Finished on its own while we waited.
                m_interrupt_requested = false;
                continue;
            }

            // A cancel is delivered the same way as a deadline, so the budget
            // decides which one this is. Checking woke_early instead would call
            // every retry tick after a cancel a timeout, since those waits
            // expire normally.
            if(std::chrono::steady_clock::now() >= m_exec_deadline)
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
              rocprofvis_python_teardown_globals_t teardown, void* user,
              rocprofvis_python_done_t done, uint64_t timeout_ms)
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
        item.source           = source;
        item.prepare_globals  = prepare;
        item.teardown_globals = teardown;
        item.user             = user;
        item.done             = done;
        item.timeout_ms       = timeout_ms;
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
                       rocprofvis_python_teardown_globals_t teardown_globals,
                       void* user, rocprofvis_python_done_t done,
                       unsigned long long timeout_ms)
{
    return RocProfVis::Python::Runtime::Get().Post(
        source, prepare_globals, teardown_globals, user, done, timeout_ms);
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
