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

#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace RocProfVis
{
namespace View
{

namespace
{

constexpr int  ASSISTANT_CONNECT_TIMEOUT_SECONDS   = 15;
constexpr int  ASSISTANT_HTTP_TIMEOUT_SECONDS      = 120;
constexpr int  ASSISTANT_FIRST_REDIRECT_STATUS     = 300;
constexpr int  ASSISTANT_FIRST_ERROR_STATUS        = 400;
constexpr int  ASSISTANT_HTTP_NOT_FOUND            = 404;
constexpr char ASSISTANT_JSON_CONTENT_TYPE[]       = "application/json";
constexpr char ASSISTANT_CHAT_COMPLETIONS_SUFFIX[] = "/chat/completions";

// How many tools one reply may ask for. Generous next to what an investigation
// actually needs, and a ceiling on how much work a single malformed or hostile
// response can start. The inline "harmony" path below has its own, smaller cap.
constexpr size_t ASSISTANT_MAX_TOOL_CALLS_PER_ROUND = 16;

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

constexpr char ASSISTANT_BAD_URL_ERROR[] =
    "The assistant URL is not a valid http(s) address.";
constexpr char ASSISTANT_MISSING_DEPLOYMENT_ERROR[] =
    "This Azure URL names the deployment in the path. Put the deployment id in "
    "the Model field.";
constexpr char ASSISTANT_INSECURE_URL_ERROR[] =
    "The assistant URL is plain http, which would put the API key on the wire "
    "in clear text. Use https, or clear the key to talk to this endpoint "
    "without one.";
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
// /engines/ and /openai/deployments segments below identify.
EndpointFlavour
FlavourFromUrl(const std::string& url)
{
    const bool azure = url.find("/azure") != std::string::npos ||
                       url.find("/engines/") != std::string::npos ||
                       url.find("/openai/deployments") != std::string::npos;
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
bool
OriginIsLoopback(const std::string& origin)
{
    const std::string host = Core::String::to_lower_copy(HostFromOrigin(origin));
    return host == "localhost" || host == "::1" ||
           host.compare(0, 4, "127.") == 0;
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
            jt::Json& arguments = function["arguments"];
            call.arguments =
                arguments.isString() ? arguments.getString() : arguments.toString();
        }
        calls.push_back(call);
    }
}

// Pulls the provider's error message out of a response body, if it is one.
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
        body["tools"]       = BuildAssistantToolsJson();
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

// The URL with any query string elided. A user who pastes a key into the URL
// would otherwise see it copied into the log and into error text.
std::string
LoggableUrl(const std::string& url)
{
    const size_t query_start = url.find('?');
    if(query_start == std::string::npos)
    {
        return url;
    }
    return url.substr(0, query_start) + "?...";
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
InterpretResponse(const httplib::Response& response, const std::string& url,
                  bool allow_tool_calls, AssistantChatResult& result)
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
    }

    // A 404 is nearly always the URL being shaped wrong, so name what we asked
    // for. The query comes off first in case a key was pasted into it.
    if(!result.error.empty() && response.status == ASSISTANT_HTTP_NOT_FOUND)
    {
        result.error.append(" Requested ");
        result.error.append(LoggableUrl(url));
        result.error.append(".");
    }
}

}  // namespace

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
    const httplib::Result response =
        client.Post(endpoint.path, headers, body, ASSISTANT_JSON_CONTENT_TYPE);
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

    InterpretResponse(*response, endpoint.url, request.enable_tools, result);
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
