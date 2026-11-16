// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "agenticprofiling/rocprofvis_ai_endpoint.h"

#include <catch2/catch_test_macros.hpp>

#include <string>

using namespace RocProfVis::View;

namespace
{

// The address a request posts to, worked out the way the client does it.
std::string
PostUrl(const std::string& base, const std::string& model = "gpt")
{
    return ChatCompletionsUrl(base, model, FlavourFromUrl(base));
}

}  // namespace

TEST_CASE("OpenAI-style bases get the chat-completions route", "[assistant][endpoint]")
{
    CHECK(FlavourFromUrl("https://api.openai.com/v1") == EndpointFlavour::kOpenAi);
    CHECK(PostUrl("https://api.openai.com/v1") ==
          "https://api.openai.com/v1/chat/completions");
    CHECK(PostUrl("https://api.openai.com/v1/") ==
          "https://api.openai.com/v1/chat/completions");
    CHECK(PostUrl("https://api.openai.com/v1/chat/completions") ==
          "https://api.openai.com/v1/chat/completions");
    // A query string stays at the end, after the route.
    CHECK(PostUrl("https://host/v1?key=abc") == "https://host/v1/chat/completions?key=abc");
}

TEST_CASE("A host whose name starts with azure is not an Azure endpoint",
          "[assistant][endpoint]")
{
    CHECK(FlavourFromUrl("https://azure-proxy.example.com/v1") == EndpointFlavour::kOpenAi);
    CHECK(FlavourFromUrl("https://azure.example.com/v1") == EndpointFlavour::kOpenAi);
    CHECK(PostUrl("https://azure-proxy.example.com/v1") ==
          "https://azure-proxy.example.com/v1/chat/completions");
}

TEST_CASE("Azure bases put the model in the path as the deployment", "[assistant][endpoint]")
{
    CHECK(FlavourFromUrl("https://host/azure") == EndpointFlavour::kAzure);
    CHECK(PostUrl("https://host/azure", "dep") ==
          "https://host/azure/engines/dep/chat/completions");
    CHECK(PostUrl("https://host/azure/engines", "dep") ==
          "https://host/azure/engines/dep/chat/completions");
    CHECK(PostUrl("https://host/openai/deployments/dep") ==
          "https://host/openai/deployments/dep/chat/completions"
          "?apiVersion=2024-09-01-preview");
    // A version the user put on the URL is kept.
    CHECK(PostUrl("https://host/openai/deployments/dep?api-version=2025-01-01") ==
          "https://host/openai/deployments/dep/chat/completions?api-version=2025-01-01");
    // A deployment-less route pasted back from an error message is repaired.
    CHECK(PostUrl("https://host/azure/chat/completions", "dep") ==
          "https://host/azure/engines/dep/chat/completions");
    // With no deployment there is nothing to post to.
    CHECK(PostUrl("https://host/azure", "").empty());
}

TEST_CASE("A URL splits into its origin and path", "[assistant][endpoint]")
{
    std::string origin;
    std::string path;
    REQUIRE(SplitUrl("https://host:8443/v1/chat/completions", origin, path));
    CHECK(origin == "https://host:8443");
    CHECK(path == "/v1/chat/completions");

    REQUIRE(SplitUrl("http://localhost", origin, path));
    CHECK(origin == "http://localhost");
    CHECK(path == "/");

    CHECK_FALSE(SplitUrl("host/v1", origin, path));
}
