// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_ai_client.h"

#include "rocprofvis_ai_tools.h"

// Vendored single-header library; it does not compile clean at /W4.
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4100 4127 4244 4267 4456 4458 4996)
#endif
#include "httplib.h"
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include "json.h"
#include "spdlog/spdlog.h"

#include <cctype>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace RocProfVis
{
namespace View
{

namespace
{

constexpr int    ASSISTANT_CONNECT_TIMEOUT_SECONDS = 15;
constexpr int    ASSISTANT_HTTP_TIMEOUT_SECONDS    = 120;
constexpr int    ASSISTANT_MAX_COMPLETION_TOKENS   = 4096;
constexpr size_t ASSISTANT_MAX_HARMONY_CALLS       = 4;
constexpr int    ASSISTANT_FIRST_ERROR_STATUS      = 400;
constexpr double ASSISTANT_TEMPERATURE             = 0.7;
constexpr char   ASSISTANT_JSON_CONTENT_TYPE[]     = "application/json";
// Gateways in front of these endpoints authenticate on a subscription header
// rather than the bearer token, so the bearer is a placeholder and the real key
// travels in the header below. Both are wire-protocol names, not credentials.
constexpr char   ASSISTANT_BEARER_PLACEHOLDER[]    = "dummy";
constexpr char   ASSISTANT_KEY_HEADER[]            = "Ocp-Apim-Subscription-Key";
constexpr char   ASSISTANT_BAD_URL_ERROR[] =
    "The assistant URL is not a valid http(s) address.";

std::string
CurrentUserName()
{
#ifdef _WIN32
    char  buffer[256];
    DWORD size = static_cast<DWORD>(sizeof(buffer));
    if(GetUserNameA(buffer, &size) == TRUE && buffer[0] != '\0')
    {
        return std::string(buffer);
    }
#else
    const char* user = std::getenv("USER");
    if(user != nullptr && user[0] != '\0')
    {
        return std::string(user);
    }
#endif
    return "unknown";
}

std::string
ChatCompletionsUrl(std::string base_url)
{
    const std::string suffix = "/chat/completions";
    while(!base_url.empty() && base_url.back() == '/')
    {
        base_url.pop_back();
    }
    if(base_url.size() >= suffix.size() &&
       base_url.compare(base_url.size() - suffix.size(), suffix.size(), suffix) == 0)
    {
        return base_url;
    }
    return base_url + suffix;
}

// httplib::Client wants the origin ("https://host:port") and the path
// ("/some/path/chat/completions") as separate arguments.
bool
SplitUrl(const std::string& url, std::string& origin_out, std::string& path_out)
{
    const std::string separator = "://";
    const size_t      scheme_end = url.find(separator);
    if(scheme_end == std::string::npos)
    {
        return false;
    }

    const size_t path_start = url.find('/', scheme_end + separator.size());
    origin_out              = url.substr(0, path_start);
    path_out = (path_start == std::string::npos) ? "/" : url.substr(path_start);
    return true;
}

std::string
JsonString(jt::Json& node, const char* key)
{
    if(!node.contains(key) || !node[key].isString())
    {
        return std::string();
    }
    return node[key].getString();
}

// "content" is either a plain string or a list of {"text": "..."} parts.
std::string
JoinContent(jt::Json& content)
{
    if(content.isString())
    {
        return content.getString();
    }
    if(!content.isArray())
    {
        return std::string();
    }

    std::string joined;
    for(jt::Json& part : content.getArray())
    {
        if(part.isString())
        {
            joined += part.getString();
        }
        else if(part.isObject())
        {
            joined += JsonString(part, "text");
        }
    }
    return joined;
}

// Reads the balanced {...} starting at or after "from", skipping braces that sit
// inside string literals.
bool
ExtractJsonObject(const std::string& text, size_t from, std::string& object_out,
                  size_t& end_out)
{
    const size_t open = text.find('{', from);
    if(open == std::string::npos)
    {
        return false;
    }

    int  depth     = 0;
    bool in_string = false;
    bool escaped   = false;
    for(size_t i = open; i < text.size(); ++i)
    {
        const char c = text[i];
        if(in_string)
        {
            if(escaped)
            {
                escaped = false;
            }
            else if(c == '\\')
            {
                escaped = true;
            }
            else if(c == '"')
            {
                in_string = false;
            }
            continue;
        }

        if(c == '"')
        {
            in_string = true;
        }
        else if(c == '{')
        {
            ++depth;
        }
        else if(c == '}')
        {
            --depth;
            if(depth == 0)
            {
                object_out = text.substr(open, i - open + 1);
                end_out    = i + 1;
                return true;
            }
        }
    }
    return false;
}

// GPT-oss models answer in the "harmony" format, which names tools inline
// instead of filling in tool_calls, e.g. "to=functions.get_summary {...}". A
// single reply can name several, so walk the whole string.
void
ParseHarmonyToolCalls(const std::string& content, std::vector<AssistantToolCall>& calls)
{
    const std::string marker           = "to=";
    const std::string functions_prefix = "functions.";

    size_t search = 0;
    while(calls.size() < ASSISTANT_MAX_HARMONY_CALLS)
    {
        const size_t marker_pos = content.find(marker, search);
        if(marker_pos == std::string::npos)
        {
            return;
        }

        size_t start = marker_pos + marker.size();
        if(content.compare(start, functions_prefix.size(), functions_prefix) == 0)
        {
            start += functions_prefix.size();
        }

        size_t end = start;
        while(end < content.size() &&
              (std::isalnum(static_cast<unsigned char>(content[end])) != 0 ||
               content[end] == '_'))
        {
            ++end;
        }
        if(end == start)
        {
            search = marker_pos + marker.size();
            continue;
        }

        AssistantToolCall call;
        call.name      = content.substr(start, end - start);
        call.id        = "harmony_" + std::to_string(calls.size() + 1);
        call.arguments = "{}";

        // Only take an argument object that appears before the next tool name;
        // anything later belongs to that call, not this one.
        const size_t next_marker = content.find(marker, end);
        const size_t brace       = content.find('{', end);
        std::string  object;
        size_t       object_end = end;
        if(brace != std::string::npos &&
           (next_marker == std::string::npos || brace < next_marker) &&
           ExtractJsonObject(content, end, object, object_end))
        {
            call.arguments = object;
            search         = object_end;
        }
        else
        {
            search = end;
        }
        calls.push_back(call);
    }
}

void
ParseToolCalls(jt::Json& message, std::vector<AssistantToolCall>& calls)
{
    if(!message.contains("tool_calls") || !message["tool_calls"].isArray())
    {
        return;
    }

    std::vector<jt::Json>& nodes = message["tool_calls"].getArray();
    calls.reserve(nodes.size());
    for(size_t i = 0; i < nodes.size(); ++i)
    {
        jt::Json& node = nodes[i];
        if(!node.isObject() || !node.contains("function"))
        {
            continue;
        }

        jt::Json&         function = node["function"];
        AssistantToolCall call;
        call.name = JsonString(function, "name");
        if(call.name.empty())
        {
            continue;
        }

        call.id = JsonString(node, "id");
        if(call.id.empty())
        {
            call.id = "call_" + std::to_string(i + 1);
        }

        call.arguments = "{}";
        if(function.contains("arguments"))
        {
            jt::Json& arguments = function["arguments"];
            call.arguments =
                arguments.isString() ? arguments.getString() : arguments.toString();
        }
        calls.push_back(call);
    }
}

std::string
ErrorText(jt::Json& root)
{
    if(root.contains("error"))
    {
        jt::Json& error = root["error"];
        if(error.isString())
        {
            return error.getString();
        }
        const std::string message = JsonString(error, "message");
        return message.empty() ? "The endpoint returned an error." : message;
    }
    // A rejected subscription key comes back as a bare {"message": "..."}.
    if(!root.contains("choices"))
    {
        return JsonString(root, "message");
    }
    return std::string();
}

void
ParseChatCompletion(jt::Json& root, AssistantChatResult& result)
{
    result.error = ErrorText(root);
    if(!result.error.empty())
    {
        return;
    }

    if(!root.contains("choices") || !root["choices"].isArray() ||
       root["choices"].getArray().empty())
    {
        result.error = "The endpoint returned no choices. Check that the URL is an "
                       "OpenAI-compatible chat endpoint.";
        return;
    }

    jt::Json& choice     = root["choices"].getArray()[0];
    result.finish_reason = JsonString(choice, "finish_reason");

    if(!choice.contains("message") || !choice["message"].isObject())
    {
        result.reply = JsonString(choice, "text");
    }
    else
    {
        jt::Json& message = choice["message"];
        ParseToolCalls(message, result.tool_calls);
        if(message.contains("content"))
        {
            result.reply = JoinContent(message["content"]);
        }

        if(result.tool_calls.empty())
        {
            ParseHarmonyToolCalls(result.reply, result.tool_calls);
            if(!result.tool_calls.empty())
            {
                result.reply.clear();
            }
        }
        if(result.reply.empty() && result.tool_calls.empty())
        {
            result.reply = JsonString(message, "reasoning");
        }
    }

    if(result.reply.empty() && result.tool_calls.empty())
    {
        result.error =
            (result.finish_reason == "length")
                ? "The model hit the token limit before it finished. Try again."
                : "The model returned an empty answer.";
    }
}

jt::Json
MessageToJson(const AssistantMessage& message)
{
    jt::Json json;
    json["role"]    = message.role;
    json["content"] = message.content;

    if(message.role == "tool")
    {
        json["tool_call_id"] = message.tool_call_id;
        if(!message.name.empty())
        {
            json["name"] = message.name;
        }
        return json;
    }

    for(size_t i = 0; i < message.tool_calls.size(); ++i)
    {
        const AssistantToolCall& call = message.tool_calls[i];
        json["tool_calls"][i]["id"]                    = call.id;
        json["tool_calls"][i]["type"]                  = "function";
        json["tool_calls"][i]["function"]["name"]      = call.name;
        json["tool_calls"][i]["function"]["arguments"] =
            call.arguments.empty() ? "{}" : call.arguments;
    }
    return json;
}

jt::Json
BuildRequestBody(const AssistantChatRequest& request)
{
    jt::Json body;
    if(!request.model.empty())
    {
        body["model"] = request.model;
    }
    body["max_completion_tokens"] = ASSISTANT_MAX_COMPLETION_TOKENS;
    body["temperature"]           = ASSISTANT_TEMPERATURE;

    for(size_t i = 0; i < request.messages.size(); ++i)
    {
        body["messages"][i] = MessageToJson(request.messages[i]);
    }
    if(request.enable_tools)
    {
        body["tools"]       = BuildAssistantToolsJson();
        body["tool_choice"] = "auto";
    }
    return body;
}

std::string
ResponseError(int status, const std::string& body)
{
    if(status >= ASSISTANT_FIRST_ERROR_STATUS)
    {
        return "The endpoint returned HTTP " + std::to_string(status) + ".";
    }
    if(body.empty())
    {
        return "The endpoint returned an empty response.";
    }
    return "The endpoint did not return JSON. Check the URL.";
}

}  // namespace

AssistantChatResult
SendAssistantChat(const AssistantChatRequest& request)
{
    AssistantChatResult result;

    if(request.endpoint_url.empty())
    {
        result.error = "Set the assistant URL in Edit > Preferences > Assistant.";
        return result;
    }
    if(request.messages.empty())
    {
        result.error = "The assistant request had no messages.";
        return result;
    }

    const std::string url = ChatCompletionsUrl(request.endpoint_url);
    std::string       origin;
    std::string       path;
    if(!SplitUrl(url, origin, path))
    {
        result.error = ASSISTANT_BAD_URL_ERROR;
        return result;
    }

    httplib::Client client(origin);
    if(!client.is_valid())
    {
        result.error = ASSISTANT_BAD_URL_ERROR;
        return result;
    }
    client.set_connection_timeout(ASSISTANT_CONNECT_TIMEOUT_SECONDS);
    client.set_read_timeout(ASSISTANT_HTTP_TIMEOUT_SECONDS);
    client.set_write_timeout(ASSISTANT_HTTP_TIMEOUT_SECONDS);
    client.set_follow_location(true);

    httplib::Headers headers;
    headers.emplace("Authorization",
                    std::string("Bearer ") + ASSISTANT_BEARER_PLACEHOLDER);
    headers.emplace("user", CurrentUserName());
    if(!request.api_token.empty())
    {
        headers.emplace(ASSISTANT_KEY_HEADER, request.api_token);
    }

    const std::string     body = BuildRequestBody(request).toString();
    const httplib::Result response =
        client.Post(path, headers, body, ASSISTANT_JSON_CONTENT_TYPE);
    if(!response)
    {
        result.error =
            "HTTPS request failed (" + httplib::to_string(response.error()) + ").";
        spdlog::warn("Assistant request to {} failed: {}", url,
                     httplib::to_string(response.error()));
        return result;
    }

    std::pair<jt::Json::Status, jt::Json> parsed = jt::Json::parse(response->body);
    if(parsed.first != jt::Json::success)
    {
        result.error = ResponseError(response->status, response->body);
        spdlog::warn("Assistant response was not JSON (HTTP {}, {} bytes)",
                     response->status, response->body.size());
        return result;
    }

    ParseChatCompletion(parsed.second, result);
    result.ok = result.error.empty();
    return result;
}

}  // namespace View
}  // namespace RocProfVis
