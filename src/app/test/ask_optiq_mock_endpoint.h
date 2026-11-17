// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT
#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace RocProfVis
{
namespace AppTest
{

// One tool call the scripted model makes. arguments is the JSON text it sends.
struct MockToolCall
{
    std::string name;
    std::string arguments;
};

// What the endpoint does with one request. Requests take replies in order.
struct MockReply
{
    int                       status = 200;
    // A completion carrying this text, or these tool calls when there are any.
    std::string               content;
    std::string               finish_reason;
    std::vector<MockToolCall> tool_calls;
    // Sent as the whole body instead of a completion when set.
    std::string               body;
    std::string               retry_after;
    int                       delay_ms = 0;
    // Sends the headers and then ends the connection, which the client reads as
    // a dropped connection.
    bool                      drop = false;
};

MockReply Answer(const std::string& content, const std::string& finish_reason = "stop");
MockReply Calls(const std::vector<MockToolCall>& calls);
MockReply Failure(int status, const std::string& body, const std::string& retry_after = "");

/*
 * An OpenAI-compatible chat endpoint the Ask Optiq tests script, served from
 * this process so the tests need nothing running beside them. Every request is
 * recorded verbatim, so a test can read back exactly what the panel sent,
 * including each tool reply it fed the model.
 *
 * cpp-httplib's server ignores SIGPIPE for the whole process when it is
 * constructed, so a test that checks SIGPIPE handling installs its own handler
 * for the duration.
 */
class MockChatEndpoint
{
public:
    // Started on first use and served from its own thread until the app exits.
    static MockChatEndpoint& Instance();
    ~MockChatEndpoint();

    bool        Running() const;
    // The base URL to configure: http://127.0.0.1:<port>/v1
    std::string Url() const;

    // Replaces the script and forgets the requests seen so far.
    void                     Script(const std::vector<MockReply>& replies);
    std::vector<std::string> Requests() const;
    // Refuses bodies past this many bytes with a 413, closing the connection
    // with the rest unread. Zero lifts the limit.
    void                     LimitBody(size_t bytes);

private:
    MockChatEndpoint();

    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

}  // namespace AppTest
}  // namespace RocProfVis
