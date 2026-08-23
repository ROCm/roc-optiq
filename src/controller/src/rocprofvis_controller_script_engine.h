// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_controller_handle.h"
#include "rocprofvis_controller_job_system.h"
#include "rocprofvis_controller_script.h"

#include <mutex>
#include <string>
#include <unordered_map>

namespace RocProfVis
{
namespace Controller
{

class Future;

class ScriptResult : public Handle
{
public:
    ScriptResult();
    ~ScriptResult() override;

    rocprofvis_controller_object_type_t GetType(void) final;

    void AppendText(char const* text);
    void SetErrorMessage(char const* message);

    rocprofvis_result_t GetString(rocprofvis_property_t property, uint64_t index,
                                  char* value, uint32_t* length) final;

private:
    mutable std::mutex m_mutex;
    std::string        m_text;
    std::string        m_error_message;
};

class ScriptEngine
{
public:
    struct Session
    {
        Future*                            future     = nullptr;
        Job*                               job        = nullptr;
        ScriptResult*                      result     = nullptr;
        rocprofvis_controller_t*           controller = nullptr;
        rocprofvis_controller_arguments_t* context    = nullptr;
    };

    static ScriptEngine& Get();

    rocprofvis_result_t ExecuteAsync(rocprofvis_controller_t* controller,
                                     char const* source,
                                     rocprofvis_controller_arguments_t* context,
                                     Future* future, ScriptResult*& result);

    rocprofvis_result_t Cancel(Future* future);

    void DropSession(Session* session);

private:
    ScriptEngine() = default;
    ~ScriptEngine() = default;

    ScriptEngine(ScriptEngine const&)            = delete;
    ScriptEngine& operator=(ScriptEngine const&) = delete;

    rocprofvis_result_t EnsureRuntime();

    std::mutex                            m_mutex;
    std::unordered_map<Future*, Session*> m_sessions;
};

}  // namespace Controller
}  // namespace RocProfVis
