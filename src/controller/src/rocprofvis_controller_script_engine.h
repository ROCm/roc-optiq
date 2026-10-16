// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_controller_handle.h"
#include "rocprofvis_controller_job_system.h"
#include "rocprofvis_controller_script.h"

#include <atomic>
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
        // Set by Cancel. Read when the session starts, because a cancel can
        // arrive while it is still queued behind another script, and polled by
        // the bindings' wait loop, which is outside the engine's lock - hence
        // atomic rather than a plain bool.
        std::atomic<bool>                  cancelled{false};
    };

    static ScriptEngine& Get();

    rocprofvis_result_t ExecuteAsync(rocprofvis_controller_t* controller,
                                     char const* source,
                                     rocprofvis_controller_arguments_t* context,
                                     Future* future, ScriptResult*& result);

    rocprofvis_result_t Cancel(Future* future);

    // Called as a session begins executing, from the bindings' prepare hook.
    // The interrupt is process-global, so the engine has to know which session
    // it would land on before it sends one. False means this session was
    // cancelled while queued and must not run.
    bool BeginSession(Session* session);

    void DropSession(Session* session);

    // Which run is executing, counted up by every accepted BeginSession. The
    // bindings stamp it into each wrapper they hand a script and compare it on
    // the way back in, because a script can keep a wrapper past the end of its
    // run - parked on an allowlisted module, which lives as long as the
    // process, or held in a reference cycle the collector only breaks during a
    // later run. Comparing the pointers instead is not an option: the ABI
    // validates a handle by calling a virtual through it, so asking whether a
    // freed controller is still good is itself the use-after-free.
    uint64_t Generation() const { return m_generation.load(std::memory_order_relaxed); }

private:
    ScriptEngine() = default;
    ~ScriptEngine() = default;

    ScriptEngine(ScriptEngine const&)            = delete;
    ScriptEngine& operator=(ScriptEngine const&) = delete;

    rocprofvis_result_t EnsureRuntime();

    std::mutex                            m_mutex;
    std::unordered_map<Future*, Session*> m_sessions;
    // The session the interpreter is running right now, or null between runs.
    // Scripts run one at a time, but several traces can each have one queued,
    // so "cancel this future" is not the same as "stop whatever is running".
    Session*                              m_running = nullptr;
    // Read outside the lock by every wrapper method, written on the
    // interpreter thread as a run starts. Starts at 0, so the first run is 1
    // and a wrapper that somehow never got stamped cannot match.
    std::atomic<uint64_t>                 m_generation{0};
};

}  // namespace Controller
}  // namespace RocProfVis
