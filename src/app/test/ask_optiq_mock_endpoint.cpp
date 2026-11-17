// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT
#include "ask_optiq_mock_endpoint.h"

#include <chrono>
#include <deque>
#include <limits>
#include <mutex>
#include <thread>

#include "httplib.h"
#include "json.h"

namespace RocProfVis
{
namespace AppTest
{

namespace
{

// A chat completion holding the reply's text, or its tool calls when it has
// any. Call ids run on across the whole session so none repeats in a history.
std::string
CompletionBody(const MockReply& reply, uint64_t& next_call_id)
{
    jt::Json  root;
    jt::Json& choice = root["choices"][0];
    choice["index"]  = 0;
    jt::Json& message = choice["message"];
    message["role"]   = "assistant";
    if(reply.tool_calls.empty())
    {
        message["content"]      = reply.content;
        choice["finish_reason"] = reply.finish_reason.empty() ? "stop" : reply.finish_reason;
    }
    else
    {
        message["content"] = nullptr;
        for(size_t i = 0; i < reply.tool_calls.size(); ++i)
        {
            jt::Json& call                = message["tool_calls"][i];
            call["id"]                    = "call_" + std::to_string(++next_call_id);
            call["type"]                  = "function";
            call["function"]["name"]      = reply.tool_calls[i].name;
            call["function"]["arguments"] = reply.tool_calls[i].arguments;
        }
        choice["finish_reason"] =
            reply.finish_reason.empty() ? "tool_calls" : reply.finish_reason;
    }
    return root.toString();
}

}  // namespace

MockReply
Answer(const std::string& content, const std::string& finish_reason)
{
    MockReply reply;
    reply.content       = content;
    reply.finish_reason = finish_reason;
    return reply;
}

MockReply
Calls(const std::vector<MockToolCall>& calls)
{
    MockReply reply;
    reply.tool_calls = calls;
    return reply;
}

MockReply
Failure(int status, const std::string& body, const std::string& retry_after)
{
    MockReply reply;
    reply.status      = status;
    reply.body        = body;
    reply.retry_after = retry_after;
    return reply;
}

struct MockChatEndpoint::Impl
{
    httplib::Server          server;
    std::thread              thread;
    int                      port = -1;
    mutable std::mutex       mutex;
    std::deque<MockReply>    script;
    std::vector<std::string> requests;
    uint64_t                 next_call_id = 0;
};

MockChatEndpoint&
MockChatEndpoint::Instance()
{
    static MockChatEndpoint endpoint;
    return endpoint;
}

MockChatEndpoint::MockChatEndpoint()
: m_impl(std::make_unique<Impl>())
{
    Impl* impl = m_impl.get();
    impl->server.Post("/v1/chat/completions", [impl](const httplib::Request& request,
                                                     httplib::Response&      response) {
        MockReply reply;
        {
            std::lock_guard<std::mutex> lock(impl->mutex);
            impl->requests.push_back(request.body);
            if(impl->script.empty())
            {
                reply = Failure(500, R"({"error":{"message":"mock: script exhausted"}})");
            }
            else
            {
                reply = impl->script.front();
                impl->script.pop_front();
            }
        }
        if(reply.delay_ms > 0)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(reply.delay_ms));
        }
        if(reply.drop)
        {
            response.set_chunked_content_provider(
                "application/json", [](size_t, httplib::DataSink&) { return false; });
            return;
        }
        response.status = reply.status;
        if(!reply.retry_after.empty())
        {
            response.set_header("Retry-After", reply.retry_after);
        }
        std::string body = reply.body;
        if(body.empty())
        {
            std::lock_guard<std::mutex> lock(impl->mutex);
            body = CompletionBody(reply, impl->next_call_id);
        }
        response.set_content(body, "application/json");
    });

    impl->port = impl->server.bind_to_any_port("127.0.0.1");
    if(impl->port > 0)
    {
        impl->thread = std::thread([impl]() { impl->server.listen_after_bind(); });
        impl->server.wait_until_ready();
    }
}

MockChatEndpoint::~MockChatEndpoint()
{
    m_impl->server.stop();
    if(m_impl->thread.joinable())
    {
        m_impl->thread.join();
    }
}

bool
MockChatEndpoint::Running() const
{
    return m_impl->port > 0 && m_impl->server.is_running();
}

std::string
MockChatEndpoint::Url() const
{
    return "http://127.0.0.1:" + std::to_string(m_impl->port) + "/v1";
}

void
MockChatEndpoint::Script(const std::vector<MockReply>& replies)
{
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    m_impl->script.assign(replies.begin(), replies.end());
    m_impl->requests.clear();
}

std::vector<std::string>
MockChatEndpoint::Requests() const
{
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    return m_impl->requests;
}

void
MockChatEndpoint::LimitBody(size_t bytes)
{
    m_impl->server.set_payload_max_length(bytes == 0 ? (std::numeric_limits<size_t>::max)()
                                                     : bytes);
}

}  // namespace AppTest
}  // namespace RocProfVis
