// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include <string>
#include <vector>

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
    std::string                   role;
    std::string                   content;
    std::string                   name;
    std::string                   tool_call_id;
    std::vector<AssistantToolCall> tool_calls;
};

struct AssistantChatRequest
{
    std::string                  endpoint_url;
    std::string                  model;
    std::string                  api_token;
    std::vector<AssistantMessage> messages;
    bool                         enable_tools = true;
};

struct AssistantChatResult
{
    bool                          ok = false;
    std::string                   reply;
    std::string                   error;
    std::string                   finish_reason;
    std::vector<AssistantToolCall> tool_calls;
};

// POST to an OpenAI-compatible chat endpoint via curl, matching the AMD
// OnPrem Python client: base_url + /chat/completions, Bearer dummy,
// Ocp-Apim-Subscription-Key, and a user header. The subscription key is
// written to a 0600 temp header file, never placed on the process command line.
AssistantChatResult SendAssistantChat(const AssistantChatRequest& request);

}  // namespace View
}  // namespace RocProfVis
