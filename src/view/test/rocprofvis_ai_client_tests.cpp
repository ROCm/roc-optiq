// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

// The assistant's endpoint client against a real HTTP server on loopback:
// where the model list is asked for, how the key travels, what is refused,
// and the /v1 a self-hosted URL with no path gets for chat.

#include "agenticprofiling/rocprofvis_ai_client.h"
#include "agenticprofiling/rocprofvis_ai_tool_schema.h"

// Vendored single-header library; it does not compile clean at /W4.
#ifdef _MSC_VER
#    pragma warning(push)
#    pragma warning(disable : 4100 4127 4244 4267 4456 4458 4996)
#endif
#include "httplib.h"
#ifdef _MSC_VER
#    pragma warning(pop)
#endif

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>
#include <string>
#include <thread>
#include <vector>

namespace RocProfVis
{
namespace View
{
// Send() attaches the tool schema, which lives with the tools. No request here
// offers tools, so the schema is never read.
jt::Json
BuildAssistantToolsJson(bool)
{
    return jt::Json();
}
}  // namespace View
}  // namespace RocProfVis

using namespace RocProfVis::View;

namespace
{

constexpr char JSON_TYPE[] = "application/json";

// An HTTP server on a free loopback port. Routes go on before Start().
class LocalServer
{
public:
    ~LocalServer() { Stop(); }

    httplib::Server& Http() { return m_server; }

    void Start()
    {
        m_port   = m_server.bind_to_any_port("127.0.0.1");
        m_thread = std::thread([this]() { m_server.listen_after_bind(); });
        m_server.wait_until_ready();
    }

    void Stop()
    {
        if(m_thread.joinable())
        {
            m_server.stop();
            m_thread.join();
        }
    }

    std::string Url(const std::string& path = std::string()) const
    {
        return "http://127.0.0.1:" + std::to_string(m_port) + path;
    }

private:
    httplib::Server m_server;
    std::thread     m_thread;
    int             m_port = 0;
};

// Polls the lookup the way frames do until it has answered for this URL.
bool
WaitForList(AssistantModelCatalog& catalog, const std::string& url)
{
    const std::chrono::steady_clock::time_point deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while(std::chrono::steady_clock::now() < deadline)
    {
        if(!catalog.Poll() && catalog.Url() == url)
        {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return false;
}

std::vector<std::string>
ListFrom(const std::string& url, const std::string& token = std::string())
{
    AssistantModelCatalog catalog;
    catalog.Request(url, token);
    REQUIRE(WaitForList(catalog, url));
    return catalog.Models();
}

void
ServeList(httplib::Server& server, const std::string& path, const std::string& body)
{
    server.Get(path, [body](const httplib::Request&, httplib::Response& response) {
        response.set_content(body, JSON_TYPE);
    });
}

AssistantChatRequest
ChatRequest(const std::string& url, const std::string& model)
{
    AssistantChatRequest request;
    request.endpoint_url = url;
    request.model        = model;
    request.enable_tools = false;
    AssistantMessage message;
    message.role    = "user";
    message.content = "hi";
    request.messages.push_back(message);
    return request;
}

std::string
Completion(const std::string& reply)
{
    return R"({"choices":[{"message":{"role":"assistant","content":")" + reply +
           R"("},"finish_reason":"stop"}]})";
}

}  // namespace

TEST_CASE("Assistant endpoint credential ownership is scheme host and port",
          "[assistant][models]")
{
    CHECK(AssistantEndpointsShareOrigin(" HTTPS://Example.COM/v1 ",
                                        "https://example.com/azure?apiVersion=1"));
    CHECK_FALSE(
        AssistantEndpointsShareOrigin("https://example.com/v1", "http://example.com/v1"));
    CHECK_FALSE(AssistantEndpointsShareOrigin("https://example.com/v1",
                                              "https://other.example/v1"));
    CHECK_FALSE(AssistantEndpointsShareOrigin("https://example.com/v1",
                                              "https://example.com:8443/v1"));
    CHECK_FALSE(AssistantEndpointsShareOrigin("not a URL", "not a URL"));
}

TEST_CASE("A hostname beginning with 127 is not trusted as loopback",
          "[assistant][models]")
{
    AssistantChatRequest request = ChatRequest("http://127.attacker.invalid", "model");
    request.api_token            = "must-not-leave";

    AssistantChatCall         call;
    const AssistantChatResult result = call.Send(request);
    CHECK_FALSE(result.ok);
    CHECK(result.error.find("plain http") != std::string::npos);
}

TEST_CASE("A reply too large to read is not asked for twice", "[assistant][chat]")
{
    // Past the reply cap httplib stops reading and reports a read error. The
    // endpoint has run the request by then, so posting it again would run it twice.
    constexpr size_t OVERSIZED_REPLY_BYTES = 9 * 1024 * 1024;
    LocalServer      server;
    std::atomic<int> posts{ 0 };
    server.Http().Post("/v1/chat/completions",
                       [&posts](const httplib::Request&, httplib::Response& response) {
                           ++posts;
                           response.set_content(std::string(OVERSIZED_REPLY_BYTES, 'x'),
                                                JSON_TYPE);
                       });
    server.Start();

    AssistantChatCall         call;
    const AssistantChatResult result = call.Send(ChatRequest(server.Url("/v1"), "model"));
    CHECK_FALSE(result.ok);
    CHECK(posts == 1);
}

TEST_CASE("A long provider error is cut on a character boundary", "[assistant][chat]")
{
    // The error reads "The endpoint returned HTTP 400: " and then the message,
    // and is cut at 500 bytes, which here falls inside the two-byte "é".
    const std::string prefix  = "The endpoint returned HTTP 400: ";
    const std::string message = std::string(499 - prefix.size(), 'x') + "\xC3\xA9" +
                                std::string(100, 'y');
    LocalServer       server;
    server.Http().Post("/v1/chat/completions",
                       [message](const httplib::Request&, httplib::Response& response) {
                           const std::string body =
                               R"({"error":{"message":")" + message + R"("}})";
                           response.status = 400;
                           response.set_content(body, JSON_TYPE);
                       });
    server.Start();

    AssistantChatCall         call;
    const AssistantChatResult result = call.Send(ChatRequest(server.Url("/v1"), "model"));
    CHECK_FALSE(result.ok);
    CHECK(result.error == prefix + std::string(499 - prefix.size(), 'x') + "...");
}

TEST_CASE("The model list is the data ids, sorted, without repeats or unusable names",
          "[assistant][models]")
{
    LocalServer server;
    ServeList(server.Http(), "/v1/models",
              R"({"object":"list","data":[{"id":"zeta"},{"id":"Alpha"},{"id":"beta"},)"
              R"({"id":"hash##model"},{"id":"zeta"},{"id":"bad\u0007name"},{"id":7},)"
              R"({"object":"model"},"bare"]})");
    server.Start();

    CHECK(ListFrom(server.Url("/v1")) ==
          std::vector<std::string>{ "Alpha", "beta", "hash##model", "zeta" });
}

TEST_CASE("A plain-http URL with no path is listed and chatted with under /v1",
          "[assistant][models]")
{
    LocalServer server;
    ServeList(server.Http(), "/v1/models", R"({"data":[{"id":"llama3.2:latest"}]})");
    server.Http().Post("/v1/chat/completions",
                       [](const httplib::Request&, httplib::Response& response) {
                           response.set_content(Completion("from v1"), JSON_TYPE);
                       });
    server.Http().Post("/chat/completions",
                       [](const httplib::Request&, httplib::Response& response) {
                           response.set_content(Completion("from root"), JSON_TYPE);
                       });
    server.Start();

    CHECK(ListFrom(server.Url()) == std::vector<std::string>{ "llama3.2:latest" });
    CHECK(ListFrom(server.Url("/")) == std::vector<std::string>{ "llama3.2:latest" });

    AssistantChatCall         bare;
    const AssistantChatResult v1 =
        bare.Send(ChatRequest(server.Url(), "llama3.2:latest"));
    CHECK(v1.ok);
    CHECK(v1.reply == "from v1");

    // Naming the route keeps it, for a server that really does serve the root.
    AssistantChatCall         named;
    const AssistantChatResult root =
        named.Send(ChatRequest(server.Url("/chat/completions"), "llama3.2:latest"));
    CHECK(root.ok);
    CHECK(root.reply == "from root");
}

TEST_CASE("An Azure-style URL lists the gateway's /v1 models with the subscription key",
          "[assistant][models]")
{
    LocalServer server;
    server.Http().Get(
        "/v1/models", [](const httplib::Request& request, httplib::Response& response) {
            const bool keyed =
                request.get_header_value("Ocp-Apim-Subscription-Key") == "secret" &&
                !request.has_header("Authorization");
            if(!keyed)
            {
                response.status = 401;
                return;
            }
            response.set_content(R"({"data":[{"id":"gpt-5.6-luna"}]})", JSON_TYPE);
        });
    server.Start();

    // A pasted key brings whitespace along, which no header may carry.
    CHECK(ListFrom(server.Url("/azure"), " secret\n") ==
          std::vector<std::string>{ "gpt-5.6-luna" });
    CHECK(ListFrom(server.Url("/azure")).empty());
}

TEST_CASE("An OpenAI-style URL lists beside its chat route with a bearer key",
          "[assistant][models]")
{
    LocalServer server;
    server.Http().Get("/api/v1/models", [](const httplib::Request& request,
                                           httplib::Response&      response) {
        if(request.get_header_value("Authorization") != "Bearer secret")
        {
            response.status = 401;
            return;
        }
        response.set_content(R"({"data":[{"id":"openai/gpt-4o"}]})", JSON_TYPE);
    });
    server.Start();

    CHECK(ListFrom(server.Url("/api/v1"), "secret") ==
          std::vector<std::string>{ "openai/gpt-4o" });
    CHECK(ListFrom(server.Url("/api/v1/chat/completions"), "secret") ==
          std::vector<std::string>{ "openai/gpt-4o" });
}

TEST_CASE("A listed name that quotes the key is not offered", "[assistant][models]")
{
    LocalServer server;
    ServeList(server.Http(), "/v1/models",
              R"({"data":[{"id":"gpt-4o"},{"id":"echo-sk-test-0123456789"}]})");
    server.Start();

    CHECK(ListFrom(server.Url("/v1"), "sk-test-0123456789") ==
          std::vector<std::string>{ "gpt-4o" });
}

TEST_CASE("An upper-case scheme is listed like a lower-case one", "[assistant][models]")
{
    LocalServer server;
    ServeList(server.Http(), "/v1/models", R"({"data":[{"id":"gpt-4o"}]})");
    server.Start();

    // cpp-httplib takes only a lower-case scheme, and gives no client for another.
    const std::string url = "HTTP" + server.Url("/v1").substr(std::string("http").size());
    CHECK(ListFrom(url) == std::vector<std::string>{ "gpt-4o" });
}

TEST_CASE(
    "No list comes from an error, a redirect, a body that is not one, or a huge one",
    "[assistant][models]")
{
    const std::string list = R"({"data":[{"id":"listed"}]})";
    std::atomic<int>  redirected_hits(0);
    LocalServer       server;
    server.Http().Get("/missing/models",
                      [](const httplib::Request&, httplib::Response& response) {
                          response.status = 404;
                      });
    // Followed, this would land on a real list and hand it the key.
    server.Http().Get("/moved/models",
                      [](const httplib::Request&, httplib::Response& response) {
                          response.set_redirect("/v1/models");
                      });
    server.Http().Get(
        "/v1/models",
        [&redirected_hits, list](const httplib::Request&, httplib::Response& response) {
            ++redirected_hits;
            response.set_content(list, JSON_TYPE);
        });
    server.Http().Get("/text/models",
                      [](const httplib::Request&, httplib::Response& response) {
                          response.set_content("not json", "text/plain");
                      });
    ServeList(server.Http(), "/object/models", R"({"data":{"id":"listed"}})");
    ServeList(server.Http(), "/huge/models", std::string(9 * 1024 * 1024, ' ') + list);
    server.Start();

    for(const char* path : { "/missing", "/moved", "/text", "/object", "/huge" })
    {
        INFO(path);
        CHECK(ListFrom(server.Url(path)).empty());
    }
    CHECK(redirected_hits == 0);
}

TEST_CASE("A host that is not listening gives an empty list without a long wait",
          "[assistant][models]")
{
    LocalServer gone;
    gone.Start();
    const std::string url = gone.Url("/v1");
    gone.Stop();

    const std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
    CHECK(ListFrom(url).empty());
    // A refused connect is quick, though Windows retries it for a second or two.
    CHECK(std::chrono::steady_clock::now() - start < std::chrono::seconds(5));
}

TEST_CASE("Dropping a lookup mid-response does not wait for the server to finish",
          "[assistant][models]")
{
    // Declared first, so it outlives the server threads that write it.
    std::atomic<bool> streaming(false);
    LocalServer       server;
    server.Http().Get("/slow/models", [&streaming](const httplib::Request&,
                                                   httplib::Response& response) {
        response.set_chunked_content_provider(
            JSON_TYPE, [&streaming](size_t, httplib::DataSink& sink) {
                streaming = true;
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                return sink.write(" ", 1);
            });
    });
    server.Start();

    std::chrono::steady_clock::time_point dropped;
    {
        AssistantModelCatalog catalog;
        catalog.Request(server.Url("/slow"), "");
        const std::chrono::steady_clock::time_point deadline =
            std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while(!streaming && std::chrono::steady_clock::now() < deadline)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        REQUIRE(streaming);
        dropped = std::chrono::steady_clock::now();
    }
    CHECK(std::chrono::steady_clock::now() - dropped < std::chrono::seconds(1));
}

TEST_CASE("Asking again while a lookup runs sends nothing new unless the key changed",
          "[assistant][models]")
{
    std::atomic<int> hits(0);
    LocalServer      server;
    server.Http().Get("/v1/models",
                      [&hits](const httplib::Request&, httplib::Response& response) {
                          ++hits;
                          std::this_thread::sleep_for(std::chrono::milliseconds(100));
                          response.set_content(R"({"data":[{"id":"m"}]})", JSON_TYPE);
                      });
    server.Start();

    AssistantModelCatalog catalog;
    const std::string     url = server.Url("/v1");
    catalog.Request(url, "key");
    catalog.Request(url, "key");
    REQUIRE(WaitForList(catalog, url));
    CHECK(hits == 1);

    catalog.Request(url, "other key");
    CHECK(catalog.Models().empty());
    CHECK(catalog.Url().empty());
    REQUIRE(WaitForList(catalog, url));
    CHECK(hits == 2);
    CHECK(catalog.Models() == std::vector<std::string>{ "m" });
}
