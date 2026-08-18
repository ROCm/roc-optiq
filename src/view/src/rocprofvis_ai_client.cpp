// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_ai_client.h"

#include "rocprofvis_ai_tools.h"

#include "json.h"
#include "spdlog/spdlog.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <process.h>
#include <windows.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace RocProfVis
{
namespace View
{

namespace
{

constexpr int    ASSISTANT_CURL_TIMEOUT_SECONDS  = 120;
constexpr int    ASSISTANT_READ_CHUNK_SIZE       = 4096;
constexpr int    ASSISTANT_MAX_COMPLETION_TOKENS = 4096;
constexpr char   ASSISTANT_CURL_BIN[]            = "curl";
constexpr char   ASSISTANT_DUMMY_API_KEY[]       = "dummy";
constexpr double ASSISTANT_TEMPERATURE           = 0.7;

class ScopedTempFile
{
public:
    ScopedTempFile() = default;

    ~ScopedTempFile()
    {
        if(!m_path.empty())
        {
            std::error_code error_code;
            std::filesystem::remove(m_path, error_code);
        }
    }

    bool Write(const std::string& prefix, const std::string& contents)
    {
        std::error_code error_code;
        std::filesystem::path dir = std::filesystem::temp_directory_path(error_code);
        if(error_code)
        {
            return false;
        }

#ifdef _WIN32
        const unsigned int pid = static_cast<unsigned int>(_getpid());
#else
        const unsigned int pid = static_cast<unsigned int>(getpid());
#endif
        m_path = dir / (prefix + std::to_string(pid) + ".tmp");

        std::ofstream out(m_path, std::ios::binary | std::ios::trunc);
        if(!out.is_open())
        {
            m_path.clear();
            return false;
        }
        out << contents;
        out.close();

#ifndef _WIN32
        chmod(m_path.c_str(), S_IRUSR | S_IWUSR);
#endif
        return true;
    }

    const std::string Path() const
    {
        return m_path.string();
    }

private:
    std::filesystem::path m_path;
};

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
    const char* login = getlogin();
    if(login != nullptr && login[0] != '\0')
    {
        return std::string(login);
    }
#endif
    return "unknown";
}

std::string
ChatCompletionsUrl(std::string base_url)
{
    while(!base_url.empty() && (base_url.back() == '/' || base_url.back() == '\\'))
    {
        base_url.pop_back();
    }
    const std::string suffix = "/chat/completions";
    if(base_url.size() >= suffix.size() &&
       base_url.compare(base_url.size() - suffix.size(), suffix.size(), suffix) == 0)
    {
        return base_url;
    }
    return base_url + suffix;
}

std::string
QuoteForShell(const std::string& value)
{
    std::string quoted = "\"";
    for(char c : value)
    {
        if(c == '"')
        {
            quoted += '\\';
        }
#ifdef _WIN32
        if(c == '%')
        {
            quoted += '%';
        }
#endif
        quoted += c;
    }
    quoted += '"';
    return quoted;
}

std::string
RunCurl(const std::string& command, std::string& error_out)
{
    error_out.clear();
#ifdef _WIN32
    FILE* pipe = _popen(command.c_str(), "r");
#else
    FILE* pipe = popen(command.c_str(), "r");
#endif
    if(pipe == nullptr)
    {
        error_out = "Could not start curl. Install curl and ensure it is on PATH.";
        return std::string();
    }

    std::string output;
    char        chunk[ASSISTANT_READ_CHUNK_SIZE];
    while(fgets(chunk, sizeof(chunk), pipe) != nullptr)
    {
        output += chunk;
    }

#ifdef _WIN32
    const int status = _pclose(pipe);
#else
    const int status = pclose(pipe);
#endif
    if(status != 0 && output.empty())
    {
        error_out = "curl failed (status " + std::to_string(status) +
                    "). Check the URL and that curl is installed.";
    }
    return output;
}

std::string
ReadJsonString(jt::Json& node)
{
    if(node.isString())
    {
        return node.getString();
    }
    if(node.isArray())
    {
        std::string joined;
        for(jt::Json& part : node.getArray())
        {
            if(part.isString())
            {
                joined += part.getString();
            }
            else if(part.isObject() && part.contains("text") && part["text"].isString())
            {
                joined += part["text"].getString();
            }
        }
        return joined;
    }
    return std::string();
}

bool
ParseHarmonyToolCall(const std::string& content, AssistantToolCall& call_out)
{
    const std::string marker = "to=";
    const size_t      pos0   = content.find(marker);
    if(pos0 == std::string::npos)
    {
        return false;
    }
    size_t pos = pos0 + marker.size();
    const std::string functions_prefix = "functions.";
    if(content.compare(pos, functions_prefix.size(), functions_prefix) == 0)
    {
        pos += functions_prefix.size();
    }
    size_t end = pos;
    while(end < content.size())
    {
        const unsigned char c = static_cast<unsigned char>(content[end]);
        if(!(std::isalnum(c) || content[end] == '_'))
        {
            break;
        }
        ++end;
    }
    if(end == pos)
    {
        return false;
    }
    call_out.name = content.substr(pos, end - pos);
    call_out.id   = "harmony_1";
    const size_t brace     = content.find('{', end);
    const size_t brace_end = content.rfind('}');
    if(brace != std::string::npos && brace_end != std::string::npos && brace_end > brace)
    {
        call_out.arguments = content.substr(brace, brace_end - brace + 1);
    }
    else
    {
        call_out.arguments = "{}";
    }
    return true;
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
        if(!node.isObject())
        {
            continue;
        }
        AssistantToolCall call;
        if(node.contains("id") && node["id"].isString())
        {
            call.id = node["id"].getString();
        }
        if(call.id.empty())
        {
            call.id = "call_" + std::to_string(i + 1);
        }
        if(node.contains("function") && node["function"].isObject())
        {
            jt::Json& function = node["function"];
            if(function.contains("name") && function["name"].isString())
            {
                call.name = function["name"].getString();
            }
            if(function.contains("arguments"))
            {
                if(function["arguments"].isString())
                {
                    call.arguments = function["arguments"].getString();
                }
                else
                {
                    call.arguments = function["arguments"].toString();
                }
            }
        }
        else if(node.contains("name") && node["name"].isString())
        {
            call.name = node["name"].getString();
            if(node.contains("arguments") && node["arguments"].isString())
            {
                call.arguments = node["arguments"].getString();
            }
        }
        if(!call.name.empty())
        {
            calls.push_back(call);
        }
    }
}

void
ParseChatCompletion(jt::Json& root, AssistantChatResult& result)
{
    if(root.contains("error"))
    {
        jt::Json& error_node = root["error"];
        if(error_node.isString())
        {
            result.error = error_node.getString();
            return;
        }
        if(error_node.isObject() && error_node.contains("message") &&
           error_node["message"].isString())
        {
            result.error = error_node["message"].getString();
            return;
        }
        result.error = "The endpoint returned an error.";
        return;
    }
    if(root.contains("message") && root["message"].isString() &&
       !root.contains("choices"))
    {
        result.error = root["message"].getString();
        return;
    }

    if(!root.contains("choices") || !root["choices"].isArray())
    {
        result.error = "Unexpected response (no choices). Check that the URL is an "
                       "OpenAI-compatible chat endpoint.";
        return;
    }

    std::vector<jt::Json>& choices = root["choices"].getArray();
    if(choices.empty())
    {
        result.error = "The endpoint returned no choices.";
        return;
    }

    jt::Json& first = choices[0];
    if(first.contains("finish_reason") && first["finish_reason"].isString())
    {
        result.finish_reason = first["finish_reason"].getString();
    }

    if(first.contains("message") && first["message"].isObject())
    {
        jt::Json& message = first["message"];
        ParseToolCalls(message, result.tool_calls);
        if(message.contains("function_call") && message["function_call"].isObject() &&
           result.tool_calls.empty())
        {
            AssistantToolCall call;
            call.id = "call_1";
            jt::Json& function_call = message["function_call"];
            if(function_call.contains("name") && function_call["name"].isString())
            {
                call.name = function_call["name"].getString();
            }
            if(function_call.contains("arguments") && function_call["arguments"].isString())
            {
                call.arguments = function_call["arguments"].getString();
            }
            if(!call.name.empty())
            {
                result.tool_calls.push_back(call);
            }
        }
        if(message.contains("content"))
        {
            result.reply = ReadJsonString(message["content"]);
        }
        if(result.tool_calls.empty() && !result.reply.empty())
        {
            AssistantToolCall harmony;
            if(ParseHarmonyToolCall(result.reply, harmony))
            {
                result.tool_calls.push_back(harmony);
                result.reply.clear();
            }
        }
        if(result.reply.empty() && result.tool_calls.empty() &&
           message.contains("reasoning") && message["reasoning"].isString())
        {
            result.reply = message["reasoning"].getString();
        }
    }
    else if(first.contains("text") && first["text"].isString())
    {
        result.reply = first["text"].getString();
    }

    if(result.reply.empty() && result.tool_calls.empty())
    {
        if(result.finish_reason == "length")
        {
            result.error = "The model hit the token limit before it finished. Try again.";
            return;
        }
        result.error = "The model returned an empty answer.";
        return;
    }
}

jt::Json
MessageToJson(const AssistantMessage& message)
{
    jt::Json json;
    json["role"] = message.role;
    if(message.role == "tool")
    {
        json["tool_call_id"] = message.tool_call_id;
        if(!message.name.empty())
        {
            json["name"] = message.name;
        }
        json["content"] = message.content;
        return json;
    }
    if(!message.tool_calls.empty())
    {
        json["content"] = message.content;
        for(size_t i = 0; i < message.tool_calls.size(); ++i)
        {
            const AssistantToolCall& call = message.tool_calls[i];
            json["tool_calls"][i]["id"]   = call.id;
            json["tool_calls"][i]["type"] = "function";
            json["tool_calls"][i]["function"]["name"]        = call.name;
            json["tool_calls"][i]["function"]["arguments"]   =
                call.arguments.empty() ? "{}" : call.arguments;
        }
        return json;
    }
    json["content"] = message.content;
    return json;
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

    jt::Json body;
    if(!request.model.empty())
    {
        body["model"] = request.model;
    }
    body["max_completion_tokens"] = ASSISTANT_MAX_COMPLETION_TOKENS;
    body["temperature"]           = ASSISTANT_TEMPERATURE;

    if(request.messages.empty())
    {
        result.error = "The assistant request had no messages.";
        return result;
    }
    for(size_t i = 0; i < request.messages.size(); ++i)
    {
        body["messages"][i] = MessageToJson(request.messages[i]);
    }
    if(request.enable_tools)
    {
        body["tools"]       = BuildAssistantToolsJson();
        body["tool_choice"] = "auto";
    }

    const std::string body_text = body.toString();

    ScopedTempFile body_file;
    if(!body_file.Write("roc-optiq-ai-body-", body_text))
    {
        result.error = "Could not write the request body to a temp file.";
        return result;
    }

    std::string header_text = "Authorization: Bearer ";
    header_text += ASSISTANT_DUMMY_API_KEY;
    header_text += "\n";
    header_text += "user: ";
    header_text += CurrentUserName();
    header_text += "\n";
    if(!request.api_token.empty())
    {
        header_text += "Ocp-Apim-Subscription-Key: ";
        header_text += request.api_token;
        header_text += "\n";
    }

    ScopedTempFile header_file;
    if(!header_file.Write("roc-optiq-ai-hdr-", header_text))
    {
        result.error = "Could not write the auth header to a temp file.";
        return result;
    }

    const std::string url = ChatCompletionsUrl(request.endpoint_url);

    std::ostringstream command;
    command << ASSISTANT_CURL_BIN << " -sS --max-time " << ASSISTANT_CURL_TIMEOUT_SECONDS
            << " -H " << QuoteForShell("Content-Type: application/json") << " -H @"
            << QuoteForShell(header_file.Path()) << " --data-binary @"
            << QuoteForShell(body_file.Path()) << " " << QuoteForShell(url);

    std::string curl_error;
    const std::string output = RunCurl(command.str(), curl_error);
    if(!curl_error.empty() && output.empty())
    {
        result.error = curl_error;
        return result;
    }

    std::pair<jt::Json::Status, jt::Json> parsed = jt::Json::parse(output);
    if(parsed.first != jt::Json::success)
    {
        if(output.empty())
        {
            result.error = curl_error.empty() ? "Empty response from the endpoint."
                                              : curl_error;
        }
        else
        {
            result.error = "The endpoint did not return JSON. Check the URL.";
            spdlog::warn("Assistant endpoint returned non-JSON ({} bytes)", output.size());
        }
        return result;
    }

    ParseChatCompletion(parsed.second, result);
    if(!result.error.empty())
    {
        return result;
    }

    result.ok = true;
    return result;
}

}  // namespace View
}  // namespace RocProfVis
