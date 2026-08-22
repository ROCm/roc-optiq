// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "python/rocprofvis_controller_python.h"
#include "rocprofvis_controller_script_engine.h"

#define PY_SSIZE_T_CLEAN
#include "rocprofvis_python.h"

namespace RocProfVis
{
namespace Controller
{

namespace
{

char const* const kSessionCapsuleName = "rocprofvis.script_result";

ScriptResult*
session_from_module(PyObject* module)
{
    ScriptResult* result = nullptr;
    if(module)
    {
        PyObject* capsule = PyObject_GetAttrString(module, "_result");
        if(capsule)
        {
            result = static_cast<ScriptResult*>(
                PyCapsule_GetPointer(capsule, kSessionCapsuleName));
            Py_DECREF(capsule);
        }
        else
        {
            PyErr_Clear();
        }
    }
    return result;
}

PyObject*
result_text(PyObject* self, PyObject* args)
{
    char const* text = nullptr;
    PyObject*   ret  = nullptr;
    if(PyArg_ParseTuple(args, "s:text", &text))
    {
        ScriptResult* result = session_from_module(self);
        if(result)
        {
            result->AppendText(text);
            Py_INCREF(Py_None);
            ret = Py_None;
        }
        else
        {
            PyErr_SetString(PyExc_RuntimeError, "optiq result session is missing");
        }
    }
    return ret;
}

PyMethodDef kResultMethods[] = {
    {"text", result_text, METH_VARARGS,
     "Append a text item to the script result."},
    {nullptr, nullptr, 0, nullptr}};

PyObject*
make_result_module(ScriptResult* script_result)
{
    PyObject* result_mod = PyModule_New("optiq.result");
    if(result_mod)
    {
        if(PyModule_AddFunctions(result_mod, kResultMethods) != 0)
        {
            Py_DECREF(result_mod);
            result_mod = nullptr;
        }
        else
        {
            PyObject* capsule =
                PyCapsule_New(script_result, kSessionCapsuleName, nullptr);
            if(capsule)
            {
                PyObject_SetAttrString(result_mod, "_result", capsule);
                Py_DECREF(capsule);
            }
        }
    }
    return result_mod;
}

}  // namespace

void
optiq_prepare_globals(void* py_dict, void* script_session)
{
    PyObject*                globals = static_cast<PyObject*>(py_dict);
    ScriptEngine::Session*   session =
        static_cast<ScriptEngine::Session*>(script_session);
    if(!globals || !session || !session->result)
    {
        return;
    }

    PyObject* optiq = PyModule_New("optiq");
    if(!optiq)
    {
        PyErr_Clear();
        return;
    }

    PyObject* result_mod = make_result_module(session->result);
    if(result_mod)
    {
        PyObject_SetAttrString(optiq, "result", result_mod);
        Py_DECREF(result_mod);
    }

    PyDict_SetItemString(globals, "optiq", optiq);
    Py_DECREF(optiq);
}

}  // namespace Controller
}  // namespace RocProfVis
