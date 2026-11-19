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
    // Which of the two disjoint tool sets to send. Carried here so the client
    // chooses a schema without knowing anything about traces.
    bool compute_tools = false;
};

// What came back, or why nothing did. A call that was cancelled says neither,
// with ok false and no error: whoever cancelled it discards the result unread.
struct AssistantChatResult
{
    bool                           ok = false;
    std::string                    reply;
    std::string                    error;
    std::string                    finish_reason;
    std::vector<AssistantToolCall> tool_calls;
    // Set when the body carried a usage object. Endpoints that omit it leave
    // this false, and the panel shows only its own estimate of the context.
    bool has_usage         = false;
    int  prompt_tokens     = 0;
    int  completion_tokens = 0;
    bool has_cached_tokens = false;
    int  cached_tokens     = 0;
};

// Lower-cased "scheme://host[:port]" of an endpoint URL, or empty when it has
// no scheme separator. What a stored key is bound to.
std::string AssistantUrlOrigin(const std::string& url);

// True when the URL is plain http to a host that is not loopback, i.e. the
// conversation would cross the network unencrypted.
bool AssistantUrlIsCleartext(const std::string& url);

// Overwrites the whole buffer of a copy of the key before it is emptied or
// freed, so the bytes do not sit in released memory.
void AssistantWipeSecret(std::string& text);

// Blanks every copy of the key in text. Looks for the key as it is sent,
// without the whitespace a paste brings along, because that is the form an
// echo of it carries. A key too short to tell from ordinary words is left.
void AssistantRedactKey(const std::string& api_token, std::string& text);

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
 *
 * A throttle (429), a gateway failure (502, 503, 504) or a connection that
 * drops straight away is tried once more after a short wait, which Cancel()
 * also cuts short. A peer that resets the connection while the request is
 * still being written fails the request rather than raising SIGPIPE.
 */
class AssistantChatCall
{
public:
    // Posts the conversation and blocks until the endpoint answers.
    AssistantChatResult Send(const AssistantChatRequest& request);

    // Aborts the request in flight, if any. Safe to call from another thread.
    // Waits out a connect in progress, up to its timeout, so do not call it
    // from a thread that must not stall.
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
