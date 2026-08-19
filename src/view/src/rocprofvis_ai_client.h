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
    // How this route wants the key presented, and the quirks it needs in the
    // body. See AssistantProvider in the settings manager.
    std::string                  auth_header;
    std::string                  auth_prefix;
    bool                         send_bearer_placeholder = false;
    bool                         use_legacy_max_tokens   = false;
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

// POST to an OpenAI-compatible chat endpoint via cpp-httplib (in-process HTTPS
// with mbedTLS). The configured base URL gets /chat/completions appended; the
// key is read from the OS credential store at call time and never persisted
// here.
AssistantChatResult SendAssistantChat(const AssistantChatRequest& request);

}  // namespace View
}  // namespace RocProfVis
