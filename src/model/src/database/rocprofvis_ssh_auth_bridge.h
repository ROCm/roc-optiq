// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include <condition_variable>
#include <mutex>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace RocProfVis
{
namespace DataModel
{

struct PromptItem
{
    std::string text;
    bool        echo = true;  // false => password-style
};

struct PromptRequest
{
    std::string             name;
    std::string             instruction;
    std::vector<PromptItem> prompts;
};

enum class HostKeyState
{
    NotFound,
    Mismatch
};

struct HostKeyRequest
{
    std::string  host;
    int          port = 22;
    std::string  fingerprint_sha256_b64;  // user-friendly display
    std::string  key_type;                // "ssh-rsa", "ssh-ed25519", ...
    HostKeyState state = HostKeyState::NotFound;
};

enum class HostKeyDecision
{
    Reject,
    TrustOnce,
    TrustPermanently
};

// Thread-safe round-trip channel between a worker thread (libssh2 Connect)
// and the UI main thread. Worker calls AskPrompts() / AskHostKey() and
// blocks. UI polls Pending() each frame; if a request is pending it draws a
// modal and eventually calls Submit*() or Cancel().
class AuthBridge
{
public:
    using Pending =
        std::variant<std::monostate, PromptRequest, HostKeyRequest>;

    Pending Peek();

    // Worker-side: blocking. Returns nullopt if the user cancelled.
    std::optional<std::vector<std::string>> AskPrompts(const PromptRequest& req);
    std::optional<HostKeyDecision>          AskHostKey(const HostKeyRequest& req);

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
    Pending                                 m_pending;
    bool                                    m_cancelled = false;
    std::optional<std::vector<std::string>> m_prompt_responses;
    std::optional<HostKeyDecision>          m_hostkey_decision;
};

}  // namespace DataModel
}  // namespace RocProfVis
