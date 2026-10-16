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

// One function call the model asked for. Arguments arrive as a JSON string.
struct AssistantToolCall
{
    std::string id;
    std::string name;
    std::string arguments;
};

// One entry of the conversation, in the shape the chat API expects.
struct AssistantMessage
{
    std::string                    role;
    std::string                    content;
    std::string                    name;          // tool replies only
    std::string                    tool_call_id;  // tool replies only
    std::vector<AssistantToolCall> tool_calls;    // assistant messages only
};

// One request to the chat endpoint.
struct AssistantChatRequest
{
    std::string                   endpoint_url;
    std::string                   model;
    std::string                   api_token;
    std::vector<AssistantMessage> messages;
    // False on the round that writes the answer, which goes out without the
    // tool schema.
    bool enable_tools = true;
};

// What came back, or why nothing did.
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
 * @brief One POST to an OpenAI chat-completions endpoint, which the caller can
 * abandon.
 *
 * The endpoint shape is inferred from the URL. A stock OpenAI base just gets
 * /chat/completions appended, names the model in the body, and sends the key as
 * Authorization: Bearer. An Azure-style base - Azure OpenAI itself, or an
 * API-management gateway in front of it - additionally takes the deployment
 * from the Model field into the path, and sends the key as
 * Ocp-Apim-Subscription-Key. TLS is in-process through cpp-httplib so the GUI
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
