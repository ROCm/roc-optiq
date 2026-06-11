// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_controller_ssh_auth_bridge.h"

namespace RocProfVis
{
namespace Controller
{

std::optional<std::vector<std::string>>
AuthBridge::AskPrompts(Future* future, const PromptRequest& req)
{
    std::unique_lock<std::mutex> lk(m_mutex);
    m_pending = req;
    future->AskPrompts(m_pending);
    m_prompt_responses.reset();
    m_worker_cv.wait(lk, [&] { return m_prompt_responses.has_value() || m_cancelled; });
    m_pending = std::monostate{};
    if(m_cancelled)
    {
        return std::nullopt;
    }
    auto out = std::move(*m_prompt_responses);
    m_prompt_responses.reset();
    return out;
}

std::optional<HostKeyDecision>
AuthBridge::AskHostKey(Future* future, const HostKeyRequest& req)
{
    std::unique_lock<std::mutex> lk(m_mutex);
    m_pending = req;
    future->AskPrompts(m_pending);
    m_hostkey_decision.reset();
    m_worker_cv.wait(lk, [&] { return m_hostkey_decision.has_value() || m_cancelled; });
    m_pending = std::monostate{};
    if(m_cancelled)
    {
        return std::nullopt;
    }
    auto d = *m_hostkey_decision;
    m_hostkey_decision.reset();
    return d;
}

void AuthBridge::SubmitPromptResponses(std::vector<std::string> responses)
{
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        if(!std::holds_alternative<PromptRequest>(m_pending)) return;
        m_prompt_responses = std::move(responses);
    }
    m_worker_cv.notify_all();
}

void AuthBridge::SubmitHostKeyDecision(HostKeyDecision decision)
{
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        if(!std::holds_alternative<HostKeyRequest>(m_pending)) return;
        m_hostkey_decision = decision;
    }
    m_worker_cv.notify_all();
}

void AuthBridge::Cancel()
{
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        m_cancelled = true;
    }
    m_worker_cv.notify_all();
}

void AuthBridge::Reset()
{
    std::lock_guard<std::mutex> lk(m_mutex);
    m_pending = std::monostate{};
    m_cancelled = false;
    m_prompt_responses.reset();
    m_hostkey_decision.reset();
}

}  // namespace Controller
}  // namespace RocProfVis
