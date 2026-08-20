// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include <mutex>
#include <string>
#include <vector>

namespace httplib
{
class Client;
}  // namespace httplib

namespace RocProfVis
{
namespace View
{

struct AssistantToolCall
{
    std::string id;
    std::string name;
    std::string arguments;
};

struct AssistantMessage
{
    std::string                    role;
    std::string                    content;
    std::string                    name;
    std::string                    tool_call_id;
    std::vector<AssistantToolCall> tool_calls;
};

struct AssistantChatRequest
{
    std::string endpoint_url;
    std::string model;
    std::string api_token;
    std::string                   auth_header;
    std::string                   auth_prefix;
    bool                          use_legacy_max_tokens = false;
    std::vector<AssistantMessage> messages;
    bool                          enable_tools = true;
};

struct AssistantChatResult
{
    bool                           ok        = false;
    bool                           cancelled = false;
    std::string                    reply;
    std::string                    error;
    std::string                    finish_reason;
    std::vector<AssistantToolCall> tool_calls;
};

/**
 * @brief One POST to an OpenAI-compatible chat endpoint, which the caller can
 * abandon.
 *
 * The configured base URL gets /chat/completions appended. Azure OpenAI
 * bases (/azure, /openai) also insert /engines/<model> or
 * /deployments/<model> from the Model field. The key travels in whichever
 * header the URL implies. TLS is in-process through cpp-httplib so the GUI
 * never shells out to curl.
 *
 * Send() blocks on a worker thread until the endpoint answers, which is minutes
 * if the server is wedged. Cancel() closes the socket out from under it, so the
 * panel can walk away from a request - on quit, or when the user clears the
 * conversation - instead of waiting out the read timeout.
 */
class AssistantChatCall
{
public:
    // Posts the conversation and blocks until the endpoint answers.
    AssistantChatResult Send(const AssistantChatRequest& request);

    // Aborts the request in flight, if any. Safe to call from another thread.
    void Cancel();

private:
    // Publishes the live socket for Cancel(), or refuses if already cancelled.
    bool Adopt(httplib::Client* client);

    // Withdraws the socket before it goes out of scope.
    void Release();

    // True once Cancel() has been called.
    bool Cancelled() const;

    mutable std::mutex m_mutex;
    httplib::Client*   m_client    = nullptr;
    bool               m_cancelled = false;
};

}  // namespace View
}  // namespace RocProfVis
