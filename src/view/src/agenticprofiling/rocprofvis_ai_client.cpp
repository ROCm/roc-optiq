// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_ai_client.h"

#include "rocprofvis_ai_tool_schema.h"

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
#include "rocprofvis_core_string_utils.h"
#include "spdlog/spdlog.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstring>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#ifndef _WIN32
#include <csignal>
#include <ctime>
#include <sys/socket.h>
#endif

namespace RocProfVis
{
namespace View
{

namespace
{

constexpr int  ASSISTANT_CONNECT_TIMEOUT_SECONDS   = 15;
// The reply is not streamed, so no byte arrives until the whole completion,
// reasoning included, is done. The read timeout has to cover all of that.
constexpr int  ASSISTANT_HTTP_TIMEOUT_SECONDS      = 300;
constexpr int  ASSISTANT_FIRST_REDIRECT_STATUS     = 300;
constexpr int  ASSISTANT_FIRST_ERROR_STATUS        = 400;
constexpr int  ASSISTANT_HTTP_NOT_FOUND            = 404;
constexpr int  ASSISTANT_HTTP_TOO_MANY_REQUESTS    = 429;
constexpr int  ASSISTANT_HTTP_BAD_GATEWAY          = 502;
constexpr int  ASSISTANT_HTTP_SERVICE_UNAVAILABLE  = 503;
constexpr int  ASSISTANT_HTTP_GATEWAY_TIMEOUT      = 504;
constexpr char ASSISTANT_JSON_CONTENT_TYPE[]       = "application/json";
constexpr char ASSISTANT_CHAT_COMPLETIONS_SUFFIX[] = "/chat/completions";

// How many tools one reply may ask for. Generous next to what an investigation
// actually needs, and a ceiling on how much work a single malformed or hostile
// response can start. The inline "harmony" path below has its own, smaller cap.
constexpr size_t ASSISTANT_MAX_TOOL_CALLS_PER_ROUND = 16;

// A throttle, a gateway hiccup or a dropped connection usually clears within
// seconds, and failing the turn over one throws away every tool round before
// it. So a request gets one more try, after a short wait the endpoint can
// lengthen with Retry-After, up to the cap.
constexpr int ASSISTANT_HTTP_ATTEMPTS           = 2;
constexpr int ASSISTANT_RETRY_WAIT_SECONDS      = 2;
constexpr int ASSISTANT_RETRY_WAIT_MAX_SECONDS  = 10;
constexpr int ASSISTANT_RETRY_POLL_MILLISECONDS = 100;
// A connection that failed later than this had most likely timed out, or the
// endpoint was already working on the completion. Trying again would double a
// long wait, and possibly the bill.
constexpr int ASSISTANT_RETRY_FAST_FAILURE_SECONDS = 10;

// Stock OpenAI: the key is a bearer token and the model is named in the body.
constexpr char ASSISTANT_OPENAI_AUTH_HEADER[] = "Authorization";
constexpr char ASSISTANT_OPENAI_AUTH_PREFIX[] = "Bearer ";

// Azure OpenAI, and the API-management gateways that front it: the key is a
// subscription key and the deployment is named in the path.
constexpr char ASSISTANT_AZURE_AUTH_HEADER[]      = "Ocp-Apim-Subscription-Key";
constexpr char ASSISTANT_AZURE_OPENAI_API_VERSION[] = "2024-09-01-preview";

// max_completion_tokens also covers reasoning tokens, which are billed but
// never returned, so it has to pay for the thinking as well as the prose. Too
// low and a reasoning model spends the whole allowance thinking and answers
// with nothing.
constexpr int ASSISTANT_MAX_COMPLETION_TOKENS = 16384;

// Azure's max_tokens counts visible output only, and deployments cap it far
// lower than the budget above, so asking for that much is a 400.
constexpr int ASSISTANT_MAX_OUTPUT_TOKENS = 4096;

// A malformed reply could otherwise name an unbounded number of tools.
constexpr size_t ASSISTANT_MAX_HARMONY_CALLS = 4;

// The provider's error text goes into the transcript, a toast and any export,
// so its length is bounded rather than left to the endpoint.
constexpr size_t ASSISTANT_MAX_ERROR_TEXT_BYTES = 400;
// A key shorter than this is a placeholder or a typo, and blanking every
// occurrence of it could mangle ordinary words in the message.
constexpr size_t ASSISTANT_MIN_REDACTED_KEY_BYTES = 8;
constexpr char   ASSISTANT_REDACTED_KEY[]         = "[key]";
constexpr char   ASSISTANT_TRUNCATION_MARK[]      = "...";

constexpr char ASSISTANT_BAD_URL_ERROR[] =
    "The assistant URL is not a valid http(s) address.";
constexpr char ASSISTANT_MISSING_DEPLOYMENT_ERROR[] =
    "This Azure URL names the deployment in the path. Put the deployment id in "
    "the Model field.";
constexpr char ASSISTANT_INSECURE_URL_ERROR[] =
    "The assistant URL is plain http, which would put the API key on the wire "
    "in clear text. Use an https URL for this endpoint.";
// Deliberately does not repeat the URL, which is where the secret is.
constexpr char ASSISTANT_URL_CREDENTIALS_ERROR[] =
    "The assistant URL must not contain credentials. Put the key in the API "
    "key field.";
constexpr char ASSISTANT_REDIRECT_ERROR[] =
    "The endpoint answered with a redirect, which is not followed because the "
    "API key must not be handed to another host. Point the URL at the address "
    "the endpoint actually serves.";

// Which endpoint the URL implies. Everything that differs between them - the
// path, the auth header, and the token-limit field - keys off this, so the two
// shapes are described in one place rather than sniffed at each use.
enum class EndpointFlavour
{
    kOpenAi,
    kAzure
};

// Azure-style bases name the deployment in the path, which is what the /azure,
// /engines/ and /openai/deployments segments below identify. Only the path is
// searched: a host such as azure-llm.example or a query value is not a marker.
EndpointFlavour
FlavourFromUrl(const std::string& url)
{
    const std::string base       = url.substr(0, url.find_first_of("?#"));
    const size_t      scheme     = base.find("://");
    const size_t      path_start =
        base.find('/', scheme == std::string::npos ? 0 : scheme + 3);
    if(path_start == std::string::npos)
    {
        return EndpointFlavour::kOpenAi;
    }
    const std::string path  = base.substr(path_start);
    const bool        azure = path.find("/azure") != std::string::npos ||
                              path.find("/engines/") != std::string::npos ||
                              path.find("/openai/deployments") != std::string::npos;
    return azure ? EndpointFlavour::kAzure : EndpointFlavour::kOpenAi;
}

// True when the value ends with the given suffix.
bool
EndsWith(const std::string& value, const char* suffix)
{
    const size_t n = std::strlen(suffix);
    return value.size() >= n && value.compare(value.size() - n, n, suffix) == 0;
}

// True when the base URL already names the chat-completions route.
bool
HasCompletionsSuffix(const std::string& url)
{
    return EndsWith(url, ASSISTANT_CHAT_COMPLETIONS_SUFFIX);
}

// Azure's /openai deployments want apiVersion on the query when the user did
// not already supply one. /azure/engines does not.
void
EnsureAzureOpenAiVersion(const std::string& path, std::string& query)
{
    if(!query.empty() || path.find("/openai/deployments/") == std::string::npos)
    {
        return;
    }
    query  = "?apiVersion=";
    query += ASSISTANT_AZURE_OPENAI_API_VERSION;
}

// Undoes a /chat/completions that was appended to an Azure base without the
// deployment in between. Azure answers that with a 404, so a user who pasted
// the failing URL back into settings would otherwise be stuck with it.
void
StripDeploymentlessCompletions(std::string& base_url)
{
    if(!HasCompletionsSuffix(base_url))
    {
        return;
    }
    std::string without_suffix = base_url;
    without_suffix.resize(without_suffix.size() -
                          std::strlen(ASSISTANT_CHAT_COMPLETIONS_SUFFIX));
    if(EndsWith(without_suffix, "/azure") || EndsWith(without_suffix, "/openai") ||
       EndsWith(without_suffix, "/azure/engines") ||
       EndsWith(without_suffix, "/openai/deployments"))
    {
        base_url = without_suffix;
    }
}

// Inserts the deployment segment an Azure base is missing, e.g. /azure becomes
// /azure/engines/<model>. Empty return means the model field was not filled in.
bool
AppendAzureDeployment(std::string& base_url, const std::string& model)
{
    // A base that already names the deployment only needs the route appended.
    if(base_url.find("/engines/") != std::string::npos ||
       base_url.find("/deployments/") != std::string::npos)
    {
        return true;
    }

    const bool azure_base              = EndsWith(base_url, "/azure");
    const bool openai_base             = EndsWith(base_url, "/openai");
    const bool azure_engines_base      = EndsWith(base_url, "/azure/engines");
    const bool openai_deployments_base = EndsWith(base_url, "/openai/deployments");
    if(!azure_base && !openai_base && !azure_engines_base && !openai_deployments_base)
    {
        return true;
    }
    if(model.empty())
    {
        return false;
    }

    if(azure_base)
    {
        base_url += "/engines/";
    }
    else if(openai_base)
    {
        base_url += "/deployments/";
    }
    else
    {
        base_url += "/";
    }
    base_url += model;
    return true;
}

// Turns the configured base into the chat-completions URL. Any query string
// comes off first so the path suffix is not buried inside it. Empty return
// means an Azure base needs a deployment and the Model field was blank.
std::string
ChatCompletionsUrl(std::string base_url, const std::string& model,
                   EndpointFlavour flavour)
{
    std::string  query;
    const size_t query_start = base_url.find('?');
    if(query_start != std::string::npos)
    {
        query = base_url.substr(query_start);
        base_url.resize(query_start);
    }

    while(!base_url.empty() && base_url.back() == '/')
    {
        base_url.pop_back();
    }

    if(flavour == EndpointFlavour::kAzure)
    {
        StripDeploymentlessCompletions(base_url);
    }

    if(!HasCompletionsSuffix(base_url))
    {
        if(flavour == EndpointFlavour::kAzure &&
           !AppendAzureDeployment(base_url, model))
        {
            return std::string();
        }
        base_url += ASSISTANT_CHAT_COMPLETIONS_SUFFIX;
    }

    EnsureAzureOpenAiVersion(base_url, query);
    return base_url + query;
}

// httplib::Client wants the origin ("https://host:port") and the path
// ("/v1/chat/completions") as separate arguments.
bool
SplitUrl(const std::string& url, std::string& origin_out, std::string& path_out)
{
    const std::string separator  = "://";
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

// Removes any "user:password@" from the authority of a URL or origin. People
// do paste a key there, and it must not travel on into the log, an error
// message or anything derived from the URL.
void
StripUserInfo(std::string& url)
{
    const std::string separator  = "://";
    const size_t      scheme_end = url.find(separator);
    if(scheme_end == std::string::npos)
    {
        return;
    }

    const size_t authority_start = scheme_end + separator.size();
    size_t       authority_end   = url.find_first_of("/?#", authority_start);
    if(authority_end == std::string::npos)
    {
        authority_end = url.size();
    }
    // The last one counts: everything before it is userinfo, whatever it holds.
    for(size_t i = authority_end; i > authority_start; --i)
    {
        if(url[i - 1] == '@')
        {
            url.erase(authority_start, i - authority_start);
            return;
        }
    }
}

// The host of an origin ("https://host:port"), with the port and any IPv6
// brackets removed. Empty when the origin has no scheme separator.
std::string
HostFromOrigin(const std::string& origin)
{
    const std::string separator  = "://";
    const size_t      scheme_end = origin.find(separator);
    if(scheme_end == std::string::npos)
    {
        return std::string();
    }

    std::string host = origin.substr(scheme_end + separator.size());
    if(!host.empty() && host.front() == '[')
    {
        // An IPv6 literal is bracketed, so the colons inside it are not a port.
        const size_t close = host.find(']');
        return (close == std::string::npos) ? host : host.substr(1, close - 1);
    }

    const size_t colon = host.find(':');
    if(colon != std::string::npos)
    {
        host.resize(colon);
    }
    return host;
}

// True when the request never leaves this machine, which is what makes a plain
// http endpoint - a local Ollama or llama.cpp server - safe to send a key to.
// Only a literal 127.x.y.z address counts. A name that merely starts with
// "127." resolves wherever its DNS says, and digits and dots alone can never
// be such a name.
bool
OriginIsLoopback(const std::string& origin)
{
    const std::string host = Core::String::to_lower_copy(HostFromOrigin(origin));
    return host == "localhost" || host == "::1" ||
           (host.compare(0, 4, "127.") == 0 &&
            host.find_first_not_of("0123456789.") == std::string::npos);
}

// Whether the API key may be sent to this origin. TLS always qualifies; plain
// http only over loopback. Anything else would put the key on the wire in
// clear text, so Send() refuses rather than leaking it.
bool
OriginMayCarryToken(const std::string& origin)
{
    return Core::String::to_lower_copy(origin).compare(0, 8, "https://") == 0 ||
           OriginIsLoopback(origin);
}

// Whether cpp-httplib will read this origin the way the checks above did. When
// its own parser rejects an origin it quietly falls back to a plain-http client
// on port 80, so anything the two could disagree about is refused up front
// rather than left to fail at name resolution.
bool
OriginIsWellFormed(const std::string& origin)
{
    if(HostFromOrigin(origin).empty())
    {
        return false;
    }
    for(const char c : origin)
    {
        const unsigned char byte = static_cast<unsigned char>(c);
        // Spaces and control characters, including DEL.
        if(byte <= 0x20 || byte == 0x7F)
        {
            return false;
        }
    }
    // After a bracketed IPv6 literal only a port may follow. HostFromOrigin
    // stops at the bracket, so "[::1].example" would otherwise pass as loopback.
    const size_t close = origin.find(']');
    return close == std::string::npos || close + 1 == origin.size() ||
           origin[close + 1] == ':';
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

// Some models answer in the "harmony" format, which names tools inline instead
// of filling in tool_calls, e.g. "to=functions.get_summary {...}". A single
// reply can name several, so walk the whole string.
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

        // "to=" inside a longer word, flag or URL query ("--to=json", "goto=x",
        // "?to=abc") is the model's prose. A real harmony call follows a space
        // or a "<|...|>" token.
        if(marker_pos > 0)
        {
            const char before = content[marker_pos - 1];
            if(std::isalnum(static_cast<unsigned char>(before)) != 0 ||
               before == '_' || before == '-' || before == '?' || before == '&')
            {
                search = marker_pos + marker.size();
                continue;
            }
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

// Reads the standard tool_calls array off an assistant message.
void
ParseToolCalls(jt::Json& message, std::vector<AssistantToolCall>& calls)
{
    if(!message.contains("tool_calls") || !message["tool_calls"].isArray())
    {
        return;
    }

    std::vector<jt::Json>& nodes = message["tool_calls"].getArray();
    calls.reserve(std::min(nodes.size(), ASSISTANT_MAX_TOOL_CALLS_PER_ROUND));
    for(size_t i = 0; i < nodes.size(); ++i)
    {
        // A round is one batch of tools the panel runs before answering, so an
        // endpoint returning hundreds would turn one reply into a long run of
        // queries against the trace. Take what a real round needs and drop the
        // rest; the model can ask again next round.
        if(calls.size() >= ASSISTANT_MAX_TOOL_CALLS_PER_ROUND)
        {
            spdlog::warn("Assistant reply carried {} tool calls; using the first {}",
                         nodes.size(), ASSISTANT_MAX_TOOL_CALLS_PER_ROUND);
            break;
        }

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
            // A null stands for "no arguments" and keeps the empty object.
            // Any other type goes through as it is, so the tool can tell the
            // model what was wrong with it.
            jt::Json& arguments = function["arguments"];
            if(arguments.isString())
            {
                call.arguments = arguments.getString();
            }
            else if(!arguments.isNull())
            {
                call.arguments = arguments.toString();
            }
        }
        calls.push_back(call);
    }
}

// Pulls the provider's error message out of a response body, if it is one.
std::string
ErrorText(jt::Json& root)
{
    // Some gateways send "error": null beside a perfectly good reply.
    if(root.contains("error") && !root["error"].isNull())
    {
        jt::Json& error = root["error"];
        if(error.isString())
        {
            return error.getString();
        }
        const std::string message = JsonString(error, "message");
        return message.empty() ? "The endpoint returned an error." : message;
    }
    // A rejected key can come back as a bare {"message": "..."}.
    if(!root.contains("choices"))
    {
        return JsonString(root, "message");
    }
    return std::string();
}

// Turns a chat-completions body into a reply, tool calls, or an error.
// allow_tool_calls is false on the answer round, where tools were not offered:
// any "to=" in the prose there is the model's own text, not a call.
void
ParseChatCompletion(jt::Json& root, bool allow_tool_calls, AssistantChatResult& result)
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
        if(allow_tool_calls)
        {
            ParseToolCalls(message, result.tool_calls);
        }
        if(message.contains("content"))
        {
            result.reply = JoinContent(message["content"]);
        }

        if(allow_tool_calls && result.tool_calls.empty())
        {
            ParseHarmonyToolCalls(result.reply, result.tool_calls);
            if(!result.tool_calls.empty())
            {
                // What is left is the model's own commentary on the call it is
                // about to make, not an answer.
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

// Serializes one conversation message the way the chat API expects it.
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

// Builds the whole request body: model, budget, conversation, and tool schema.
// temperature and reasoning_effort are deliberately absent - reasoning models
// reject a non-default temperature, and every model's own default effort is
// what we want.
jt::Json
BuildRequestBody(const AssistantChatRequest& request, EndpointFlavour flavour)
{
    jt::Json body;
    // Azure names the deployment in the path, and a model beside it is ignored
    // at best and a 400 at worst, so let the path win rather than making the
    // user empty the field to match the endpoint.
    if(flavour == EndpointFlavour::kOpenAi)
    {
        body["model"] = request.model;
    }
    // Azure's API version predates max_completion_tokens and rejects it.
    if(flavour == EndpointFlavour::kAzure)
    {
        body["max_tokens"] = ASSISTANT_MAX_OUTPUT_TOKENS;
    }
    else
    {
        body["max_completion_tokens"] = ASSISTANT_MAX_COMPLETION_TOKENS;
    }

    for(size_t i = 0; i < request.messages.size(); ++i)
    {
        body["messages"][i] = MessageToJson(request.messages[i]);
    }
    if(request.enable_tools)
    {
        body["tools"]       = BuildAssistantToolsJson(request.compute_tools);
        body["tool_choice"] = "auto";
    }
    return body;
}

// Describes a response that arrived but was not usable JSON.
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

// True for the statuses that mean "not now" rather than "no": a throttle, or a
// gateway that could not reach the model. Everything else would fail the same
// way a second time.
bool
StatusIsTransient(int status)
{
    return status == ASSISTANT_HTTP_TOO_MANY_REQUESTS ||
           status == ASSISTANT_HTTP_BAD_GATEWAY ||
           status == ASSISTANT_HTTP_SERVICE_UNAVAILABLE ||
           status == ASSISTANT_HTTP_GATEWAY_TIMEOUT;
}

// How long to wait before the retry: what Retry-After asks for when it is a
// number of seconds, up to the cap, and the default otherwise. The header may
// also be an HTTP date or garbage, so the digits are read by hand rather than
// with a conversion that throws.
int
RetryWaitSeconds(const std::string& retry_after)
{
    int seconds = 0;
    for(size_t i = 0; i < retry_after.size(); ++i)
    {
        // Stopping at the cap also keeps a long run of digits from overflowing.
        if(std::isdigit(static_cast<unsigned char>(retry_after[i])) == 0 ||
           seconds >= ASSISTANT_RETRY_WAIT_MAX_SECONDS)
        {
            break;
        }
        seconds = seconds * 10 + (retry_after[i] - '0');
    }
    if(seconds <= 0)
    {
        return ASSISTANT_RETRY_WAIT_SECONDS;
    }
    return std::min(seconds, ASSISTANT_RETRY_WAIT_MAX_SECONDS);
}

// A write into a connection the peer has reset raises SIGPIPE, and its default
// action ends the whole app, every open trace with it. A request body is tens
// to hundreds of KB, so an endpoint that resets mid-upload is not rare, and
// cpp-httplib's client leaves the signal to its caller. macOS turns it off per
// socket. Linux cannot, so there the request's thread blocks it and drops any
// it raised. Either way the write fails, and is reported or retried like any
// other dropped connection.
#ifdef SO_NOSIGPIPE
void
SuppressSigpipe(socket_t sock)
{
    const int on = 1;
    setsockopt(sock, SOL_SOCKET, SO_NOSIGPIPE, &on, sizeof(on));
}
#endif

#ifdef __linux__
class ScopedSigpipeBlock
{
public:
    ScopedSigpipeBlock()
    : m_blocked(false)
    {
        sigemptyset(&m_sigpipe);
        sigaddset(&m_sigpipe, SIGPIPE);
        // When the thread already blocked SIGPIPE, a pending one is not ours to drop.
        if(pthread_sigmask(SIG_BLOCK, &m_sigpipe, &m_previous) == 0)
        {
            m_blocked = sigismember(&m_previous, SIGPIPE) == 0;
        }
    }

    ~ScopedSigpipeBlock()
    {
        if(!m_blocked)
        {
            return;
        }
        // A SIGPIPE raised here is pending on this thread, and would be
        // delivered the moment the mask is restored.
        const timespec no_wait = {};
        while(sigtimedwait(&m_sigpipe, nullptr, &no_wait) == SIGPIPE)
        {
        }
        pthread_sigmask(SIG_SETMASK, &m_previous, nullptr);
    }

    ScopedSigpipeBlock(const ScopedSigpipeBlock&)            = delete;
    ScopedSigpipeBlock& operator=(const ScopedSigpipeBlock&) = delete;

private:
    sigset_t m_sigpipe;
    sigset_t m_previous;
    bool     m_blocked;
};
#endif

// The URL with any query string elided and any userinfo removed. A user who
// pastes a key into either would otherwise see it copied into the log and into
// error text. The path stays: it cannot be told apart from a deployment name,
// and it is what a 404 needs to show.
std::string
LoggableUrl(const std::string& url)
{
    const size_t query_start = url.find('?');
    std::string  loggable    = url.substr(0, query_start);
    StripUserInfo(loggable);
    if(query_start != std::string::npos)
    {
        loggable += "?...";
    }
    return loggable;
}

// Makes the endpoint's error text safe to show and export. Some gateways echo
// the credential they were given in their auth error, so every copy of the key
// is blanked, and the text is cut to a bounded length. A key the provider has
// already partly masked is left as it is.
void
ScrubErrorText(const std::string& api_token, std::string& error)
{
    if(api_token.size() >= ASSISTANT_MIN_REDACTED_KEY_BYTES)
    {
        const size_t placeholder_size = std::strlen(ASSISTANT_REDACTED_KEY);
        size_t       found            = error.find(api_token);
        while(found != std::string::npos)
        {
            error.replace(found, api_token.size(), ASSISTANT_REDACTED_KEY);
            found = error.find(api_token, found + placeholder_size);
        }
    }

    if(error.size() > ASSISTANT_MAX_ERROR_TEXT_BYTES)
    {
        // Back up to the start of a UTF-8 sequence so the cut never leaves half
        // a code point for ImGui or the exported file.
        size_t cut = ASSISTANT_MAX_ERROR_TEXT_BYTES;
        while(cut > 0 && (static_cast<unsigned char>(error[cut]) & 0xC0) == 0x80)
        {
            --cut;
        }
        error.resize(cut);
        error += ASSISTANT_TRUNCATION_MARK;
    }
}

// Where a request is going, once the configured base URL has been turned into
// a concrete origin and path.
struct ResolvedEndpoint
{
    EndpointFlavour flavour = EndpointFlavour::kOpenAi;
    std::string     url;
    std::string     origin;
    std::string     path;
};

// Works out the address to post to and checks it is one we may use. Returns an
// empty string on success, or the reason the request cannot go out.
std::string
ResolveEndpoint(const AssistantChatRequest& request, ResolvedEndpoint& endpoint_out)
{
    endpoint_out.flavour = FlavourFromUrl(request.endpoint_url);
    endpoint_out.url =
        ChatCompletionsUrl(request.endpoint_url, request.model, endpoint_out.flavour);
    if(endpoint_out.url.empty())
    {
        return ASSISTANT_MISSING_DEPLOYMENT_ERROR;
    }
    if(!SplitUrl(endpoint_out.url, endpoint_out.origin, endpoint_out.path))
    {
        return ASSISTANT_BAD_URL_ERROR;
    }

    // cpp-httplib only recognises a lower-case scheme, while the key check
    // below is case-blind. Normalising here keeps the two in step, and lets
    // "HTTPS://" work. SplitUrl found the separator, at the same place in both.
    const size_t      scheme_size = endpoint_out.origin.find("://");
    const std::string scheme =
        Core::String::to_lower_copy(endpoint_out.origin.substr(0, scheme_size));
    if(scheme != "http" && scheme != "https")
    {
        return ASSISTANT_BAD_URL_ERROR;
    }
    endpoint_out.origin.replace(0, scheme_size, scheme);
    endpoint_out.url.replace(0, scheme_size, scheme);

    // The origin is scheme and authority only, so an @ in it is userinfo. It
    // could never connect - cpp-httplib takes "key@host" as the host name - and
    // refusing it here keeps the secret out of the request log.
    if(endpoint_out.origin.find('@') != std::string::npos)
    {
        return ASSISTANT_URL_CREDENTIALS_ERROR;
    }
    if(!OriginIsWellFormed(endpoint_out.origin))
    {
        return ASSISTANT_BAD_URL_ERROR;
    }
    if(!request.api_token.empty() && !OriginMayCarryToken(endpoint_out.origin))
    {
        return ASSISTANT_INSECURE_URL_ERROR;
    }
    return std::string();
}

// The auth header this endpoint shape expects, or none when no key is set.
httplib::Headers
AuthHeaders(const AssistantChatRequest& request, EndpointFlavour flavour)
{
    httplib::Headers headers;
    if(request.api_token.empty())
    {
        return headers;
    }
    if(flavour == EndpointFlavour::kAzure)
    {
        headers.emplace(ASSISTANT_AZURE_AUTH_HEADER, request.api_token);
    }
    else
    {
        headers.emplace(ASSISTANT_OPENAI_AUTH_HEADER,
                        std::string(ASSISTANT_OPENAI_AUTH_PREFIX) + request.api_token);
    }
    return headers;
}

// Turns a response that did arrive into a reply or an error.
void
InterpretResponse(const httplib::Response& response, bool allow_tool_calls,
                  AssistantChatResult& result)
{
    if(response.status >= ASSISTANT_FIRST_REDIRECT_STATUS &&
       response.status < ASSISTANT_FIRST_ERROR_STATUS)
    {
        result.error = ASSISTANT_REDIRECT_ERROR;
        return;
    }

    std::pair<jt::Json::Status, jt::Json> parsed = jt::Json::parse(response.body);
    if(parsed.first != jt::Json::success)
    {
        result.error = ResponseError(response.status, response.body);
        spdlog::warn("Assistant response was not JSON (HTTP {}, {} bytes)",
                     response.status, response.body.size());
    }
    else
    {
        ParseChatCompletion(parsed.second, allow_tool_calls, result);
        // An error status with a JSON body: lead with the status so a throttle,
        // a bad key and a wrong URL do not read alike, then the provider's own
        // words if it sent any.
        if(!result.error.empty() && response.status >= ASSISTANT_FIRST_ERROR_STATUS)
        {
            const std::string detail = ErrorText(parsed.second);
            result.error = "The endpoint returned HTTP " +
                           std::to_string(response.status) +
                           (detail.empty() ? std::string(".") : ": " + detail);
        }
    }
}

}  // namespace

// Lower-cased "scheme://host[:port]" of an endpoint URL, or empty when it has
// no scheme separator. Userinfo is dropped, so a key pasted into the URL is not
// carried into whatever the caller stores or compares.
std::string
AssistantUrlOrigin(const std::string& url)
{
    std::string origin;
    std::string path;
    // A base with no path can still carry a query, which is not part of the
    // origin.
    if(!SplitUrl(url.substr(0, url.find_first_of("?#")), origin, path))
    {
        return std::string();
    }
    StripUserInfo(origin);
    origin = Core::String::to_lower_copy(origin);
    // A default port names the same destination as no port at all.
    if(origin.compare(0, 8, "https://") == 0 && EndsWith(origin, ":443"))
    {
        origin.resize(origin.size() - 4);
    }
    else if(origin.compare(0, 7, "http://") == 0 && EndsWith(origin, ":80"))
    {
        origin.resize(origin.size() - 3);
    }
    return origin;
}

// True when the URL is plain http to a host that is not loopback. This is the
// same rule OriginMayCarryToken applies to the key, asked on behalf of the
// conversation: with no key set such an endpoint is still used.
bool
AssistantUrlIsCleartext(const std::string& url)
{
    const std::string origin = AssistantUrlOrigin(url);
    return origin.compare(0, 7, "http://") == 0 && !OriginIsLoopback(origin);
}

AssistantChatResult
AssistantChatCall::Send(const AssistantChatRequest& request)
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

    ResolvedEndpoint endpoint;
    result.error = ResolveEndpoint(request, endpoint);
    if(!result.error.empty())
    {
        return result;
    }

    httplib::Client client(endpoint.origin);
    if(!client.is_valid())
    {
        result.error = ASSISTANT_BAD_URL_ERROR;
        return result;
    }
    client.set_connection_timeout(ASSISTANT_CONNECT_TIMEOUT_SECONDS);
    client.set_read_timeout(ASSISTANT_HTTP_TIMEOUT_SECONDS);
    client.set_write_timeout(ASSISTANT_HTTP_TIMEOUT_SECONDS);
    // Redirects are deliberately not followed. cpp-httplib drops only a fixed
    // set of headers when a redirect crosses to another host, and the Azure
    // subscription key is not one of them, so following one could hand the key
    // to whatever host the redirect names. A chat endpoint has no reason to
    // redirect, so refusing costs nothing and InterpretResponse says why.
    client.set_follow_location(false);
#ifdef SO_NOSIGPIPE
    client.set_socket_options(SuppressSigpipe);
#endif
#ifdef __linux__
    const ScopedSigpipeBlock sigpipe_block;
#endif

    const httplib::Headers headers = AuthHeaders(request, endpoint.flavour);
    const std::string      body =
        BuildRequestBody(request, endpoint.flavour).toString();
    const std::string loggable = LoggableUrl(endpoint.url);
    spdlog::info("Assistant POST {}", loggable);

    if(!Adopt(&client))
    {
        result.cancelled = true;
        return result;
    }
    // The client stays adopted across both attempts so Cancel() can stop
    // either one.
    httplib::Result response;
    for(int attempt = 1;; ++attempt)
    {
        const std::chrono::steady_clock::time_point started =
            std::chrono::steady_clock::now();
        response =
            client.Post(endpoint.path, headers, body, ASSISTANT_JSON_CONTENT_TYPE);
        if(attempt >= ASSISTANT_HTTP_ATTEMPTS || Cancelled())
        {
            break;
        }

        int wait_seconds = ASSISTANT_RETRY_WAIT_SECONDS;
        if(response)
        {
            if(!StatusIsTransient(response->status))
            {
                break;
            }
            wait_seconds = RetryWaitSeconds(response->get_header_value("Retry-After"));
        }
        else
        {
            // Only a connection that broke straight away is worth another try.
            // A timeout, a refused connection, a DNS or a TLS failure would
            // fail the same way again, so those are reported at once.
            const httplib::Error error = response.error();
            const bool           reset = error == httplib::Error::Read ||
                                         error == httplib::Error::Write ||
                                         error == httplib::Error::ConnectionClosed ||
                                         error == httplib::Error::Unknown;
            const bool fast = std::chrono::steady_clock::now() - started <
                              std::chrono::seconds(ASSISTANT_RETRY_FAST_FAILURE_SECONDS);
            if(!reset || !fast)
            {
                break;
            }
        }

        spdlog::warn("Assistant request to {} failed transiently, retrying in {} s",
                     loggable, wait_seconds);
        // Wait in short slices so Cancel() is noticed promptly; the panel
        // blocks on this thread when it abandons a request.
        const std::chrono::steady_clock::time_point wake =
            std::chrono::steady_clock::now() + std::chrono::seconds(wait_seconds);
        while(!Cancelled() && std::chrono::steady_clock::now() < wake)
        {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(ASSISTANT_RETRY_POLL_MILLISECONDS));
        }
        if(Cancelled())
        {
            break;
        }
    }
    Release();

    if(!response)
    {
        // Cancel() closed this socket on purpose, so the failure is expected.
        if(Cancelled())
        {
            result.cancelled = true;
            return result;
        }
        result.error =
            "HTTPS request failed (" + httplib::to_string(response.error()) + ").";
        spdlog::warn("Assistant request to {} failed: {}", loggable,
                     httplib::to_string(response.error()));
        return result;
    }

    // A cancel that landed during the retry wait leaves the first, failed
    // response in hand. The caller walked away, so it is not an error to show.
    if(Cancelled())
    {
        result.cancelled = true;
        return result;
    }

    InterpretResponse(*response, request.enable_tools, result);
    ScrubErrorText(request.api_token, result.error);
    // A 404 is nearly always the URL being shaped wrong, so name what we asked
    // for. Added after the scrub so a long provider message cannot cut it off;
    // the query and any userinfo are already off it.
    if(!result.error.empty() && response->status == ASSISTANT_HTTP_NOT_FOUND)
    {
        result.error.append(" Requested ");
        result.error.append(loggable);
        result.error.append(".");
    }
    result.ok = result.error.empty();
    return result;
}

// Aborts the request in flight, if any. Safe to call from another thread.
void
AssistantChatCall::Cancel()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_cancelled = true;
    if(m_client != nullptr)
    {
        m_client->stop();
    }
}

// Publishes the live socket for Cancel(), or refuses if already cancelled.
bool
AssistantChatCall::Adopt(httplib::Client* client)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if(m_cancelled)
    {
        return false;
    }
    m_client = client;
    return true;
}

// Withdraws the socket before it goes out of scope.
void
AssistantChatCall::Release()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_client = nullptr;
}

// True once Cancel() has been called.
bool
AssistantChatCall::Cancelled() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_cancelled;
}

}  // namespace View
}  // namespace RocProfVis
