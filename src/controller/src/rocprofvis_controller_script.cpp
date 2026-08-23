// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_controller_script_engine.h"
#include "python/rocprofvis_controller_python.h"
#include "rocprofvis_controller_future.h"
#include "rocprofvis_controller_reference.h"
#include "rocprofvis_core_assert.h"
#include "rocprofvis_python_runtime.h"

namespace RocProfVis
{
namespace Controller
{

typedef Reference<rocprofvis_controller_future_t, Future, kRPVControllerObjectTypeFuture>
    FutureRef;
typedef Reference<rocprofvis_controller_script_result_t, ScriptResult,
                  kRPVControllerObjectTypeScriptResult>
    ScriptResultRef;

namespace
{

rocprofvis_result_t
map_python_result(rocprofvis_python_result_t result)
{
    rocprofvis_result_t mapped = kRocProfVisResultUnknownError;
    switch(result)
    {
        case kRocProfVisPythonSuccess:
            mapped = kRocProfVisResultSuccess;
            break;
        case kRocProfVisPythonCancelled:
            mapped = kRocProfVisResultCancelled;
            break;
        case kRocProfVisPythonInvalidArgument:
            mapped = kRocProfVisResultInvalidArgument;
            break;
        default:
            mapped = kRocProfVisResultUnknownError;
            break;
    }
    return mapped;
}

void
on_python_done(void* user, rocprofvis_python_result_t python_result,
               char const* error_message)
{
    ScriptEngine::Session* session = static_cast<ScriptEngine::Session*>(user);
    if(!session)
    {
        return;
    }
    if(error_message && session->result)
    {
        session->result->SetErrorMessage(error_message);
    }
    if(session->job)
    {
        session->job->Complete(map_python_result(python_result));
    }
    ScriptEngine::Get().DropSession(session);
}

}  // namespace

ScriptResult::ScriptResult()
: Handle(__kRPVControllerScriptResultPropertiesFirst,
         __kRPVControllerScriptResultPropertiesLast)
{}

ScriptResult::~ScriptResult() = default;

rocprofvis_controller_object_type_t
ScriptResult::GetType(void)
{
    return kRPVControllerObjectTypeScriptResult;
}

void
ScriptResult::AppendText(char const* text)
{
    if(!text)
    {
        return;
    }
    std::lock_guard<std::mutex> lock(m_mutex);
    if(!m_text.empty())
    {
        m_text += '\n';
    }
    m_text += text;
}

void
ScriptResult::SetErrorMessage(char const* message)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_error_message = message ? message : "";
}

rocprofvis_result_t
ScriptResult::GetString(rocprofvis_property_t property, uint64_t index, char* value,
                        uint32_t* length)
{
    (void) index;
    std::lock_guard<std::mutex> lock(m_mutex);
    rocprofvis_result_t         result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerScriptResultText:
            result = GetStdStringImpl(value, length, m_text);
            break;
        case kRPVControllerScriptResultErrorMessage:
            result = GetStdStringImpl(value, length, m_error_message);
            break;
        default:
            result = UnhandledProperty(property);
            break;
    }
    return result;
}

ScriptEngine&
ScriptEngine::Get()
{
    static ScriptEngine instance;
    return instance;
}

void
ScriptEngine::DropSession(Session* session)
{
    if(!session)
    {
        return;
    }
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if(session->future)
        {
            m_sessions.erase(session->future);
        }
    }
    delete session;
}

rocprofvis_result_t
ScriptEngine::EnsureRuntime()
{
    rocprofvis_python_result_t python_result = rocprofvis_python_init(nullptr);
    rocprofvis_result_t        result        = kRocProfVisResultSuccess;
    if(python_result != kRocProfVisPythonSuccess)
    {
        result = kRocProfVisResultUnknownError;
    }
    return result;
}

rocprofvis_result_t
ScriptEngine::ExecuteAsync(rocprofvis_controller_t* controller, char const* source,
                           rocprofvis_controller_arguments_t* context, Future* future,
                           ScriptResult*& result)
{
    rocprofvis_result_t error = kRocProfVisResultInvalidArgument;
    result                    = nullptr;
    if(source && future)
    {
        error = EnsureRuntime();
        if(error == kRocProfVisResultSuccess)
        {
            Job* job = new Job(
                [](Future*) -> rocprofvis_result_t {
                    return kRocProfVisResultUnknownError;
                },
                future);
            future->Set(job);
            ScriptResult* script_result = new ScriptResult();
            Session*      session       = new Session();
            session->future             = future;
            session->job                = job;
            session->result             = script_result;
            session->controller         = controller;
            session->context            = context;
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_sessions[future] = session;
            }
            rocprofvis_python_result_t python_result = rocprofvis_python_exec(
                source, optiq_prepare_globals, session, on_python_done);
            if(python_result == kRocProfVisPythonSuccess)
            {
                result = script_result;
                error  = kRocProfVisResultSuccess;
            }
            else
            {
                DropSession(session);
                delete script_result;
                job->Complete(map_python_result(python_result));
                error = map_python_result(python_result);
            }
        }
    }
    return error;
}

rocprofvis_result_t
ScriptEngine::Cancel(Future* future)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(future)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if(m_sessions.find(future) != m_sessions.end())
        {
            rocprofvis_python_interrupt();
            result = kRocProfVisResultSuccess;
        }
        else
        {
            result = kRocProfVisResultNotLoaded;
        }
    }
    return result;
}

}  // namespace Controller
}  // namespace RocProfVis

extern "C"
{

rocprofvis_result_t
rocprofvis_script_execute_async(rocprofvis_controller_t* controller, char const* source,
                                rocprofvis_controller_arguments_t*     context,
                                rocprofvis_controller_future_t*        future,
                                rocprofvis_controller_script_result_t** result)
{
    rocprofvis_result_t error = kRocProfVisResultInvalidArgument;
    RocProfVis::Controller::FutureRef future_ref(future);
    if(result && future_ref.IsValid())
    {
        *result = nullptr;
        RocProfVis::Controller::ScriptResult* script_result = nullptr;
        error = RocProfVis::Controller::ScriptEngine::Get().ExecuteAsync(
            controller, source, context, future_ref.Get(), script_result);
        *result = reinterpret_cast<rocprofvis_controller_script_result_t*>(
            script_result);
    }
    return error;
}

rocprofvis_result_t
rocprofvis_script_cancel(rocprofvis_controller_future_t* future)
{
    rocprofvis_result_t error = kRocProfVisResultInvalidArgument;
    RocProfVis::Controller::FutureRef future_ref(future);
    if(future_ref.IsValid())
    {
        error = RocProfVis::Controller::ScriptEngine::Get().Cancel(future_ref.Get());
    }
    return error;
}

void
rocprofvis_script_result_free(rocprofvis_controller_script_result_t* result)
{
    RocProfVis::Controller::ScriptResultRef result_ref(result);
    if(result_ref.IsValid())
    {
        delete result_ref.Get();
    }
}

}
