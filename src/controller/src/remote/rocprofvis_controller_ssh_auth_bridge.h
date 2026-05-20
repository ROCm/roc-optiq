// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include <condition_variable>
#include <mutex>
#include <optional>
#include <string>
#include <variant>
#include <vector>
#include "rocprofvis_controller_future.h"
#include "rocprofvis_controller_remote_structs.h"

namespace RocProfVis
{
namespace Controller
{


// Thread-safe round-trip channel between a worker thread (libssh2 Connect)
// and the UI main thread. Worker calls AskPrompts() / AskHostKey() and
// blocks. UI polls Pending() each frame; if a request is pending it draws a
// modal and eventually calls Submit*() or Cancel().
class AuthBridge
{
public:

    // Worker-side: blocking. Returns nullopt if the user cancelled.
    std::optional<std::vector<std::string>> AskPrompts(Future* future, const PromptRequest& req);
    std::optional<HostKeyDecision>          AskHostKey(Future* future, const HostKeyRequest& req);

    // UI-side. No-op if no matching request is pending.
    void SubmitPromptResponses(std::vector<std::string> responses);
    void SubmitHostKeyDecision(HostKeyDecision decision);

    // UI-side. Wakes any blocked worker call with a cancellation result.
    void Cancel();

    // Reset state for a new attempt. Should only be called when no worker
    // is blocked.
    void Reset();

private:
    std::mutex                              m_mutex;
    std::condition_variable                 m_worker_cv;
    rocprofvis_controller_user_prompt_t     m_pending;
    bool                                    m_cancelled = false;
    std::optional<std::vector<std::string>> m_prompt_responses;
    std::optional<HostKeyDecision>          m_hostkey_decision;
};

}  // namespace DataModel
}  // namespace RocProfVis
