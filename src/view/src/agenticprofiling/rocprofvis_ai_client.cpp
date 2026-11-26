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
#include <atomic>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <functional>
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

// One lookup's cancellation flag and live socket. Each replaced lookup keeps
// its own state until its worker exits, so cancelling one cannot stop another.
struct assistant_model_catalog_request_t
{
    std::atomic<bool>                     cancelled    = false;
    std::atomic<bool>                     stop_started = false;
    std::atomic<bool>                     finished     = false;
    std::mutex                            mutex;
    httplib::Client*                      client = nullptr;
    std::chrono::steady_clock::time_point deadline;
};

namespace
{

constexpr int  ASSISTANT_CONNECT_TIMEOUT_SECONDS   = 15;
// The reply is not streamed, so no byte arrives until the whole completion,
// reasoning included, is done. The read timeout has to cover all of that.
constexpr int  ASSISTANT_HTTP_TIMEOUT_SECONDS      = 300;
constexpr int  ASSISTANT_HTTP_OK                   = 200;
constexpr int  ASSISTANT_FIRST_REDIRECT_STATUS     = 300;
constexpr int  ASSISTANT_FIRST_ERROR_STATUS        = 400;
constexpr int  ASSISTANT_HTTP_NOT_FOUND            = 404;
constexpr int  ASSISTANT_HTTP_TOO_MANY_REQUESTS    = 429;
constexpr int  ASSISTANT_HTTP_BAD_GATEWAY          = 502;
constexpr int  ASSISTANT_HTTP_SERVICE_UNAVAILABLE  = 503;
constexpr int  ASSISTANT_HTTP_GATEWAY_TIMEOUT      = 504;
constexpr char ASSISTANT_JSON_CONTENT_TYPE[]       = "application/json";
constexpr char ASSISTANT_CHAT_COMPLETIONS_SUFFIX[] = "/chat/completions";
constexpr char ASSISTANT_MODELS_SUFFIX[]           = "/models";
constexpr char ASSISTANT_GATEWAY_MODELS_PATH[]     = "/v1/models";
constexpr char ASSISTANT_SELF_HOSTED_API_PATH[]    = "/v1";

// A model list is one small GET, so a host that is slow to give one is not
// worth waiting on. These also bound how long quitting waits on a lookup.
constexpr int ASSISTANT_CATALOG_CONNECT_TIMEOUT_SECONDS = 3;
constexpr int ASSISTANT_CATALOG_READ_TIMEOUT_SECONDS    = 5;
constexpr int ASSISTANT_CATALOG_TOTAL_TIMEOUT_SECONDS   = 8;
constexpr int ASSISTANT_CATALOG_STOP_RETRY_MS           = 1;
// The largest public list, OpenRouter's, is about a megabyte and some hundreds
// of models. Past these the body is not a list worth showing.
constexpr size_t ASSISTANT_CATALOG_MAX_BYTES    = 8 * 1024 * 1024;
constexpr size_t ASSISTANT_MAX_LISTED_MODELS    = 1000;
constexpr size_t ASSISTANT_MAX_MODEL_NAME_BYTES = 256;

// How many tools one reply may ask for. Generous next to what an investigation
// actually needs, and a ceiling on how much work a single malformed or hostile
// response can start. The inline "harmony" path below has its own, smaller cap.
constexpr size_t ASSISTANT_MAX_TOOL_CALLS_PER_ROUND = 16;

// A transient failure gets one more try, after a short wait the endpoint can
// lengthen with Retry-After, up to the cap.
constexpr int ASSISTANT_RETRY_WAIT_SECONDS      = 2;
constexpr int ASSISTANT_RETRY_WAIT_MAX_SECONDS  = 10;
constexpr int ASSISTANT_RETRY_POLL_MILLISECONDS = 100;
// A connection that failed later than this most likely timed out, or the
// endpoint was already working: retrying would double the wait and the bill.
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

// Largest reply read. A real one is far smaller; this stops a URL that points at
// something other than a chat endpoint from buffering the 100 MB httplib allows.
constexpr size_t ASSISTANT_MAX_RESPONSE_BYTES = 8 * 1024 * 1024;

// Longest provider error shown, in bytes. text.size() counts bytes, and toasts
// size to their text, so a page of it would draw one wider than the screen.
constexpr size_t        ASSISTANT_MAX_ERROR_BYTES = 500;
constexpr unsigned char UTF8_CONTINUATION_MASK    = 0xC0;
constexpr unsigned char UTF8_CONTINUATION_BITS    = 0x80;

// Blanking a shorter key could mangle ordinary words in the message.
constexpr size_t ASSISTANT_MIN_REDACTED_KEY_BYTES = 8;
constexpr char   ASSISTANT_REDACTED_KEY[]         = "[key]";
constexpr char   ASSISTANT_TRUNCATION_MARK[]      = "...";
// What trimming takes off either end of a pasted key: the characters
// std::isspace matches in the C locale.
constexpr char ASSISTANT_KEY_BLANKS[] = " \t\n\v\f\r";

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
    std::string  path   = url.substr(0, url.find_first_of("?#"));
    const size_t scheme = path.find("://");
    // erase() clamps, so a URL with no path leaves nothing to search.
    path.erase(0, path.find('/', scheme == std::string::npos ? 0 : scheme + 3));
    const bool azure = path.find("/azure") != std::string::npos ||
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

// Takes the query string and any trailing slashes off a base URL, so a route
// can be appended without landing inside the query. Returns the query.
std::string
TakeQuery(std::string& base_url)
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
    return query;
}

bool
OriginIsLoopback(const std::string& origin);

// A plain-http loopback URL with no path is a self-hosted server - Ollama, LM
// Studio, vLLM, llama.cpp - and those serve the API under /v1. Remote servers
// keep the path they were given, so discovery never changes their chat route.
void
AddSelfHostedApiPath(std::string& base_url)
{
    const std::string scheme = "http://";
    const bool        plain_http =
        Core::String::to_lower_copy(base_url).compare(0, scheme.size(), scheme) == 0;
    if(plain_http && OriginIsLoopback(base_url) &&
       base_url.find('/', scheme.size()) == std::string::npos)
    {
        base_url += ASSISTANT_SELF_HOSTED_API_PATH;
    }
}

// Turns the configured base into the chat-completions URL. Any query string
// comes off first so the path suffix is not buried inside it. Empty return
// means an Azure base needs a deployment and the Model field was blank.
std::string
ChatCompletionsUrl(std::string base_url, const std::string& model,
                   EndpointFlavour flavour)
{
    std::string query = TakeQuery(base_url);

    if(flavour == EndpointFlavour::kAzure)
    {
        StripDeploymentlessCompletions(base_url);
    }
    else
    {
        AddSelfHostedApiPath(base_url);
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

// True only for a numeric 127/8 IPv4 address. A hostname such as
// 127.attacker.example must not inherit loopback's plaintext-key exception.
bool
IsIpv4Loopback(const std::string& host)
{
    if(host.compare(0, 4, "127.") != 0)
    {
        return false;
    }
    size_t start = 4;
    for(int32_t octet = 0; octet < 3; ++octet)
    {
        const size_t end = host.find('.', start);
        if((octet < 2 && end == std::string::npos) ||
           (octet == 2 && end != std::string::npos))
        {
            return false;
        }
        const size_t stop = end == std::string::npos ? host.size() : end;
        if(stop == start || stop - start > 3)
        {
            return false;
        }
        int32_t value = 0;
        for(size_t i = start; i < stop; ++i)
        {
            if(std::isdigit(static_cast<unsigned char>(host[i])) == 0)
            {
                return false;
            }
            value = value * 10 + static_cast<int32_t>(host[i] - '0');
        }
        if(value > 255)
        {
            return false;
        }
        start = stop + 1;
    }
    return true;
}

// True when the request never leaves this machine, which is what makes a plain
// http endpoint - a local Ollama or llama.cpp server - safe to send a key to.
bool
OriginIsLoopback(const std::string& origin)
{
    const std::string host = Core::String::to_lower_copy(HostFromOrigin(origin));
    return host == "localhost" || host == "::1" || IsIpv4Loopback(host);
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

// Whether cpp-httplib will read this origin the way the checks above did. An
// origin its parser rejects quietly becomes a plain-http client on port 80.
bool
OriginIsWellFormed(const std::string& origin)
{
    if(HostFromOrigin(origin).empty())
    {
        return false;
    }
    for(const char c : origin)
    {
        // Spaces and control characters, including DEL.
        if(static_cast<unsigned char>(c) <= 0x20 || c == 0x7F)
        {
            return false;
        }
    }
    // Only a port may follow a bracketed IPv6 literal. HostFromOrigin stops at
    // the bracket, so "[::1].example" would otherwise pass as loopback.
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

// Reads a non-negative token count. Usage numbers arrive as ints from a stock
// endpoint and as doubles from others, and a count a 32-bit int cannot hold is
// rejected rather than wrapped.
bool
JsonCount(jt::Json& node, const char* key, int& out)
{
    if(!node.contains(key))
    {
        return false;
    }
    jt::Json& value = node[key];
    double    raw   = 0.0;
    if(value.isLong())
    {
        raw = static_cast<double>(value.getLong());
    }
    else if(value.isDouble())
    {
        raw = value.getDouble();
    }
    else
    {
        return false;
    }
    if(!std::isfinite(raw) || raw < 0.0 || raw > static_cast<double>(INT32_MAX))
    {
        return false;
    }
    out = static_cast<int>(raw);
    return true;
}

// Copies the usage object when the endpoint sent one. Missing fields stay zero.
// A body with no usage object leaves has_usage false, which is what a local
// server that does not count tokens looks like.
void
ReadUsage(jt::Json& root, AssistantChatResult& result)
{
    if(!root.contains("usage") || !root["usage"].isObject())
    {
        return;
    }
    jt::Json&  usage      = root["usage"];
    const bool has_prompt = JsonCount(usage, "prompt_tokens", result.prompt_tokens);
    const bool has_completion =
        JsonCount(usage, "completion_tokens", result.completion_tokens);
    if(!has_prompt && !has_completion)
    {
        return;
    }
    result.has_usage = true;
    if(usage.contains("prompt_tokens_details") &&
       usage["prompt_tokens_details"].isObject())
    {
        result.has_cached_tokens = JsonCount(usage["prompt_tokens_details"],
                                             "cached_tokens", result.cached_tokens);
    }
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
        // "?to=abc") is the model's prose, not a call.
        const char before = (marker_pos > 0) ? content[marker_pos - 1] : ' ';
        if(std::isalnum(static_cast<unsigned char>(before)) != 0 || before == '_' ||
           before == '-' || before == '?' || before == '&')
        {
            search = marker_pos + marker.size();
            continue;
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
        // A null stands for "no arguments" and keeps the empty object.
        if(function.contains("arguments") && !function["arguments"].isNull())
        {
            jt::Json& arguments = function["arguments"];
            call.arguments =
                arguments.isString() ? arguments.getString() : arguments.toString();
        }
        calls.push_back(call);
    }
}

// Cuts a provider's message to ASSISTANT_MAX_ERROR_BYTES, backing off to the
// start of a character so the cut does not split a UTF-8 sequence.
std::string
ClampErrorText(std::string text)
{
    if(text.size() > ASSISTANT_MAX_ERROR_BYTES)
    {
        size_t cut = ASSISTANT_MAX_ERROR_BYTES;
        while(cut > 0 && (static_cast<unsigned char>(text[cut]) &
                          UTF8_CONTINUATION_MASK) == UTF8_CONTINUATION_BITS)
        {
            --cut;
        }
        text.resize(cut);
        text += ASSISTANT_TRUNCATION_MARK;
    }
    return text;
}

// Pulls the provider's error message out of a response body, if it is one.
// Uncut: ScrubErrorText bounds it once the key has been blanked.
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
    // Read before the choices checks. A length-limit body still carries usage,
    // and that count is worth keeping even when the reply itself is unusable.
    ReadUsage(root, result);

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

// A copy of text with the key blanked. Nothing the model reads may carry it,
// whether a script printed it, a trace recorded it, or the user typed it.
std::string
WithoutKey(const std::string& api_token, std::string text)
{
    AssistantRedactKey(api_token, text);
    return text;
}

// Serializes one conversation message the way the chat API expects it.
jt::Json
MessageToJson(const AssistantMessage& message, const std::string& api_token)
{
    jt::Json json;
    json["role"]    = message.role;
    json["content"] = WithoutKey(api_token, message.content);

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
            call.arguments.empty() ? std::string("{}")
                                   : WithoutKey(api_token, call.arguments);
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
        // Same trim as the Azure path. A pasted model can end in a newline,
        // and that would be sent as a different model name.
        body["model"] = Core::String::trim_copy(request.model);
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
        body["messages"][i] = MessageToJson(request.messages[i], request.api_token);
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

// Seconds to wait before trying a request that started at "started" once more,
// or 0 when the outcome would only repeat. A throttle or a gateway failure
// waits as long as a numeric Retry-After asks, up to the cap. Of the transport
// failures only a connection that broke straight away is retried: a timeout, a
// refused connection, a DNS or a TLS failure would fail the same way again.
int
RetryWaitSeconds(const httplib::Result&                       response,
                 const std::chrono::steady_clock::time_point& started)
{
    if(!response)
    {
        const httplib::Error error = response.error();
        const bool           reset = error == httplib::Error::Read ||
                                     error == httplib::Error::Write ||
                                     error == httplib::Error::ConnectionClosed ||
                                     error == httplib::Error::Unknown;
        const bool fast = std::chrono::steady_clock::now() - started <
                          std::chrono::seconds(ASSISTANT_RETRY_FAST_FAILURE_SECONDS);
        return (reset && fast) ? ASSISTANT_RETRY_WAIT_SECONDS : 0;
    }
    if(response->status != ASSISTANT_HTTP_TOO_MANY_REQUESTS &&
       response->status != ASSISTANT_HTTP_BAD_GATEWAY &&
       response->status != ASSISTANT_HTTP_SERVICE_UNAVAILABLE &&
       response->status != ASSISTANT_HTTP_GATEWAY_TIMEOUT)
    {
        return 0;
    }

    // The header may also be an HTTP date or garbage, so the digits are read by
    // hand. Stopping at the cap keeps a long run of them from overflowing.
    int seconds = 0;
    for(const char c : response->get_header_value("Retry-After"))
    {
        if(std::isdigit(static_cast<unsigned char>(c)) == 0 ||
           seconds >= ASSISTANT_RETRY_WAIT_MAX_SECONDS)
        {
            break;
        }
        seconds = seconds * 10 + (c - '0');
    }
    return (seconds == 0) ? ASSISTANT_RETRY_WAIT_SECONDS
                          : std::min(seconds, ASSISTANT_RETRY_WAIT_MAX_SECONDS);
}

// A write into a connection the peer has reset raises SIGPIPE, whose default
// action ends the app, and cpp-httplib's client leaves the signal to its
// caller. macOS turns it off per socket. Linux cannot, so there the request's
// thread blocks it and drops any it raised. Either way the write just fails.
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
    {
        sigemptyset(&m_sigpipe);
        sigaddset(&m_sigpipe, SIGPIPE);
        // When the thread already blocked SIGPIPE, a pending one is not ours to drop.
        m_blocked = pthread_sigmask(SIG_BLOCK, &m_sigpipe, &m_previous) == 0 &&
                    sigismember(&m_previous, SIGPIPE) == 0;
    }

    ~ScopedSigpipeBlock()
    {
        if(!m_blocked)
        {
            return;
        }
        // One raised meanwhile is pending on this thread, and would be
        // delivered the moment the mask is restored.
        const timespec no_wait = {};
        while(sigtimedwait(&m_sigpipe, nullptr, &no_wait) == SIGPIPE)
        {
        }
        pthread_sigmask(SIG_SETMASK, &m_previous, nullptr);
    }

private:
    sigset_t m_sigpipe;
    sigset_t m_previous;
    bool     m_blocked;
};
#endif

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

// Makes the endpoint's error text safe to show and export: some gateways echo
// the key they were given, so every copy is blanked before the text is cut, or
// a key straddling the cut would survive as a prefix the search no longer finds.
void
ScrubErrorText(const std::string& api_token, std::string& error)
{
    AssistantRedactKey(api_token, error);
    error = ClampErrorText(std::move(error));
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

// Whether the request carries a key at all, asked without copying it.
bool
HasApiToken(const AssistantChatRequest& request)
{
    return request.api_token.find_first_not_of(ASSISTANT_KEY_BLANKS) !=
           std::string::npos;
}

// Works out the address to post to and checks it is one we may use. Returns an
// empty string on success, or the reason the request cannot go out.
std::string
ResolveEndpoint(const AssistantChatRequest& request, ResolvedEndpoint& endpoint_out)
{
    // A pasted URL can carry a trailing newline, which httplib refuses in the
    // request line, or a space, which ends up in a host name that never resolves.
    // The model is trimmed too: Azure concatenates it into the path, and a
    // trailing space there is a deployment name that does not exist.
    const std::string base_url = Core::String::trim_copy(request.endpoint_url);
    const std::string model    = Core::String::trim_copy(request.model);
    endpoint_out.flavour = FlavourFromUrl(base_url);
    endpoint_out.url     = ChatCompletionsUrl(base_url, model, endpoint_out.flavour);
    if(endpoint_out.url.empty())
    {
        return ASSISTANT_MISSING_DEPLOYMENT_ERROR;
    }
    if(!SplitUrl(endpoint_out.url, endpoint_out.origin, endpoint_out.path))
    {
        return ASSISTANT_BAD_URL_ERROR;
    }

    // cpp-httplib only recognises a lower-case scheme, while the key check
    // below is case-blind. Normalising keeps the two in step.
    const size_t      scheme_size = endpoint_out.origin.find("://");
    const std::string scheme =
        Core::String::to_lower_copy(endpoint_out.origin.substr(0, scheme_size));
    if(scheme != "http" && scheme != "https")
    {
        return ASSISTANT_BAD_URL_ERROR;
    }
    endpoint_out.origin.replace(0, scheme_size, scheme);
    endpoint_out.url.replace(0, scheme_size, scheme);

    // An @ in the origin is userinfo. cpp-httplib could not connect with it,
    // and refusing it here keeps a pasted key out of the log and error text.
    if(endpoint_out.origin.find('@') != std::string::npos)
    {
        return ASSISTANT_URL_CREDENTIALS_ERROR;
    }
    if(!OriginIsWellFormed(endpoint_out.origin))
    {
        return ASSISTANT_BAD_URL_ERROR;
    }
    if(HasApiToken(request) && !OriginMayCarryToken(endpoint_out.origin))
    {
        return ASSISTANT_INSECURE_URL_ERROR;
    }
    return std::string();
}

// The auth header this endpoint shape expects, or none when no key is set.
httplib::Headers
AuthHeaders(const std::string& api_token, EndpointFlavour flavour)
{
    httplib::Headers headers;
    // Without the whitespace a paste brings along: httplib refuses a header
    // value that ends in a space or newline, so every request would fail.
    std::string token = Core::String::trim_copy(api_token);
    if(!token.empty())
    {
        if(flavour == EndpointFlavour::kAzure)
        {
            headers.emplace(ASSISTANT_AZURE_AUTH_HEADER, token);
        }
        else
        {
            headers.emplace(ASSISTANT_OPENAI_AUTH_HEADER,
                            std::string(ASSISTANT_OPENAI_AUTH_PREFIX) + token);
        }
    }
    AssistantWipeSecret(token);
    return headers;
}

// The auth header is this client's own copy of the key, so it is wiped once
// the request is over. httplib keeps copies of its own that it does not wipe.
void
WipeAuthHeaders(httplib::Headers& headers)
{
    for(httplib::Headers::value_type& header : headers)
    {
        AssistantWipeSecret(header.second);
    }
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
        // Lead with the status so a throttle, a bad key and a wrong URL do not
        // read alike, then the provider's own words if it sent any.
        if(!result.error.empty() && response.status >= ASSISTANT_FIRST_ERROR_STATUS)
        {
            const std::string detail = ErrorText(parsed.second);
            result.error = "The endpoint returned HTTP " +
                           std::to_string(response.status) +
                           (detail.empty() ? std::string(".") : ": " + detail);
        }
    }
}

// Where the endpoint lists its models. An OpenAI-style list sits beside the
// chat route. An Azure-style URL is usually a gateway, which lists the names
// its deployments take at the host's /v1/models.
std::string
ModelCatalogUrl(std::string base_url, EndpointFlavour flavour)
{
    const std::string query = TakeQuery(base_url);
    if(flavour == EndpointFlavour::kAzure)
    {
        std::string origin;
        std::string path;
        if(!SplitUrl(base_url, origin, path))
        {
            return std::string();
        }
        return origin + ASSISTANT_GATEWAY_MODELS_PATH + query;
    }

    AddSelfHostedApiPath(base_url);
    if(HasCompletionsSuffix(base_url))
    {
        base_url.resize(base_url.size() - std::strlen(ASSISTANT_CHAT_COMPLETIONS_SUFFIX));
    }
    return base_url + ASSISTANT_MODELS_SUFFIX + query;
}

// A name the Model field can show and a request can carry: bounded, and free
// of control characters, which would break the request line or the JSON body.
bool
IsModelName(const std::string& name)
{
    return !name.empty() && name.size() <= ASSISTANT_MAX_MODEL_NAME_BYTES &&
           std::none_of(name.begin(), name.end(), [](char c) {
               return std::iscntrl(static_cast<unsigned char>(c)) != 0;
           });
}

// The ids of an OpenAI-style list, {"data": [{"id": ...}, ...]}, in the order
// a person scans them, without repeats or anything that is not a usable name.
std::vector<std::string>
ParseModelCatalog(const std::string& body)
{
    std::vector<std::string>              models;
    std::pair<jt::Json::Status, jt::Json> parsed = jt::Json::parse(body);
    jt::Json&                             root   = parsed.second;
    if(parsed.first != jt::Json::success || !root.isObject() || !root.contains("data") ||
       !root["data"].isArray())
    {
        return models;
    }

    for(jt::Json& entry : root["data"].getArray())
    {
        if(models.size() >= ASSISTANT_MAX_LISTED_MODELS)
        {
            break;
        }
        const std::string id = entry.isObject() ? JsonString(entry, "id") : std::string();
        if(IsModelName(id))
        {
            models.push_back(id);
        }
    }

    // Case-insensitive first, then exact, so equal names end up adjacent.
    std::sort(models.begin(), models.end(),
              [](const std::string& a, const std::string& b) {
                  const std::string folded_a = Core::String::to_lower_copy(a);
                  const std::string folded_b = Core::String::to_lower_copy(b);
                  return folded_a != folded_b ? folded_a < folded_b : a < b;
              });
    models.erase(std::unique(models.begin(), models.end()), models.end());
    return models;
}

// Marks a request finished on every return from its worker.
struct model_catalog_finish_t
{
    std::shared_ptr<assistant_model_catalog_request_t> request;
    ~model_catalog_finish_t() { request->finished = true; }
};

// Stops a lookup's live socket, if it has one. This may wait on cpp-httplib's
// socket mutex, so interactive cancellation calls it on a worker.
void
StopModelCatalogRequest(const std::shared_ptr<assistant_model_catalog_request_t>& request)
{
    std::lock_guard<std::mutex> lock(request->mutex);
    if(request->client != nullptr)
    {
        request->client->stop();
    }
}

// Marks a lookup cancelled immediately and stops its socket away from the UI.
// An invalid future means another caller already started the stop.
std::future<void>
CancelModelCatalogRequestAsync(
    const std::shared_ptr<assistant_model_catalog_request_t>& request)
{
    if(request == nullptr)
    {
        return std::future<void>();
    }
    request->cancelled = true;
    if(request->stop_started.exchange(true))
    {
        return std::future<void>();
    }
    return std::async(std::launch::async, [request]() {
        // stop() before Get() has no persistent effect. Retry
        // until Get either starts and is stopped, or the
        // worker observes cancellation and returns.
        while(!request->finished)
        {
            StopModelCatalogRequest(request);
            if(!request->finished)
            {
                std::this_thread::sleep_for(
                    std::chrono::milliseconds(ASSISTANT_CATALOG_STOP_RETRY_MS));
            }
        }
    });
}

// Final teardown has no UI thread to protect, so it can stop synchronously.
void
CancelModelCatalogRequestNow(
    const std::shared_ptr<assistant_model_catalog_request_t>& request)
{
    if(request == nullptr)
    {
        return;
    }
    request->cancelled = true;
    if(!request->stop_started.exchange(true))
    {
        while(!request->finished)
        {
            StopModelCatalogRequest(request);
            if(!request->finished)
            {
                std::this_thread::sleep_for(
                    std::chrono::milliseconds(ASSISTANT_CATALOG_STOP_RETRY_MS));
            }
        }
    }
}

// Publishes the worker's client unless cancellation won the race.
bool
AdoptModelCatalogClient(const std::shared_ptr<assistant_model_catalog_request_t>& request,
                        httplib::Client*                                          client)
{
    std::lock_guard<std::mutex> lock(request->mutex);
    if(request->cancelled)
    {
        return false;
    }
    request->client = client;
    return true;
}

// Withdraws the client before its stack frame goes away.
void
ReleaseModelCatalogClient(
    const std::shared_ptr<assistant_model_catalog_request_t>& request)
{
    std::lock_guard<std::mutex> lock(request->mutex);
    request->client = nullptr;
}

// One model-list request, run on a lookup's worker thread. Returns empty for
// anything but a usable list, and as soon as the lookup is cancelled; the body
// receiver is where a cancel lands once the response has started.
std::vector<std::string>
FetchModelCatalog(const std::string& endpoint_url, const std::string& api_token,
                  const std::shared_ptr<assistant_model_catalog_request_t>& request)
{
    const model_catalog_finish_t finish{ request };
    const std::string            base_url = Core::String::trim_copy(endpoint_url);
    const EndpointFlavour        flavour  = FlavourFromUrl(base_url);
    const std::string            url      = ModelCatalogUrl(base_url, flavour);
    std::string                  origin;
    std::string                  path;
    if(url.empty() || !SplitUrl(url, origin, path))
    {
        return std::vector<std::string>();
    }

    httplib::Client client(origin);
    if(!client.is_valid())
    {
        return std::vector<std::string>();
    }
    client.set_connection_timeout(ASSISTANT_CATALOG_CONNECT_TIMEOUT_SECONDS);
    client.set_read_timeout(ASSISTANT_CATALOG_READ_TIMEOUT_SECONDS);
    // Not followed, for the reason Send() gives: the key could go to another host.
    client.set_follow_location(false);

    const std::string      token = Core::String::trim_copy(api_token);
    const httplib::Headers headers =
        OriginMayCarryToken(origin) ? AuthHeaders(token, flavour) : httplib::Headers();
    const std::string loggable = LoggableUrl(url);

    if(!AdoptModelCatalogClient(request, &client))
    {
        return std::vector<std::string>();
    }
    if(request->cancelled)
    {
        ReleaseModelCatalogClient(request);
        return std::vector<std::string>();
    }
    std::string           body;
    const httplib::Result response =
        client.Get(path, headers, [&body, &request](const char* data, size_t length) {
            if(request->cancelled ||
               std::chrono::steady_clock::now() >= request->deadline ||
               body.size() + length > ASSISTANT_CATALOG_MAX_BYTES)
            {
                return false;
            }
            body.append(data, length);
            return true;
        });
    ReleaseModelCatalogClient(request);
    if(request->cancelled)
    {
        return std::vector<std::string>();
    }
    if(!response)
    {
        spdlog::info("Assistant model list from {} failed: {}", loggable,
                     httplib::to_string(response.error()));
        return std::vector<std::string>();
    }
    if(response->status != ASSISTANT_HTTP_OK)
    {
        spdlog::info("Assistant model list from {} returned HTTP {}", loggable,
                     response->status);
        return std::vector<std::string>();
    }

    std::vector<std::string> models = ParseModelCatalog(body);
    spdlog::info("Assistant endpoint lists {} models at {}", models.size(), loggable);
    return models;
}

}  // namespace

std::string
AssistantUrlOrigin(const std::string& url)
{
    std::string origin;
    std::string path;
    // Trimmed the way ResolveEndpoint trims it, so this names the host that is
    // contacted. A base with no path can still carry a query, which is not
    // part of it.
    const std::string trimmed = Core::String::trim_copy(url);
    if(!SplitUrl(trimmed.substr(0, trimmed.find_first_of("?#")), origin, path))
    {
        return std::string();
    }
    // Userinfo is dropped, up to the last @, so a key pasted into the URL is
    // not carried into whatever the caller stores or compares.
    const size_t authority = origin.find("://") + 3;
    const size_t at        = origin.rfind('@');
    if(at != std::string::npos && at >= authority)
    {
        origin.erase(authority, at + 1 - authority);
    }
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

// The rule OriginMayCarryToken applies to the key, asked for the conversation:
// with no key set such an endpoint is still used.
bool
AssistantUrlIsCleartext(const std::string& url)
{
    const std::string origin = AssistantUrlOrigin(url);
    return origin.compare(0, 7, "http://") == 0 && !OriginIsLoopback(origin);
}

void
AssistantWipeSecret(std::string& text)
{
    // Out to the capacity, so what an earlier and longer value left behind goes
    // too. Volatile keeps the compiler from dropping the writes as dead stores.
    text.resize(text.capacity());
    volatile char* bytes = text.data();
    for(size_t i = 0; i < text.size(); i++)
    {
        bytes[i] = '\0';
    }
    text.clear();
}

void
AssistantRedactKey(const std::string& api_token, std::string& text)
{
    // Trimmed in place by offsets, so the search makes no further copy of it.
    const size_t first = api_token.find_first_not_of(ASSISTANT_KEY_BLANKS);
    if(first == std::string::npos)
    {
        return;
    }
    const size_t length = api_token.find_last_not_of(ASSISTANT_KEY_BLANKS) + 1 - first;
    // Blanking a shorter key could mangle ordinary words in the text.
    if(length < ASSISTANT_MIN_REDACTED_KEY_BYTES)
    {
        return;
    }
    const char* const key   = api_token.data() + first;
    size_t            found = text.find(key, 0, length);
    while(found != std::string::npos)
    {
        text.replace(found, length, ASSISTANT_REDACTED_KEY);
        found = text.find(key, found + std::strlen(ASSISTANT_REDACTED_KEY), length);
    }
}

bool
AssistantEndpointsShareOrigin(const std::string& first, const std::string& second)
{
    std::string normalized_first  = Core::String::trim_copy(first);
    std::string normalized_second = Core::String::trim_copy(second);
    TakeQuery(normalized_first);
    TakeQuery(normalized_second);

    std::string first_origin;
    std::string first_path;
    std::string second_origin;
    std::string second_path;
    if(!SplitUrl(normalized_first, first_origin, first_path) ||
       !SplitUrl(normalized_second, second_origin, second_path))
    {
        return false;
    }
    return Core::String::to_lower_copy(first_origin) ==
           Core::String::to_lower_copy(second_origin);
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
        spdlog::warn("Assistant request not sent: {}", result.error);
        return result;
    }

#ifdef __linux__
    // Before the client, whose destructor can write a TLS close_notify.
    const ScopedSigpipeBlock sigpipe_block;
#endif
    httplib::Client client(endpoint.origin);
    if(!client.is_valid())
    {
        result.error = ASSISTANT_BAD_URL_ERROR;
        return result;
    }
    client.set_connection_timeout(ASSISTANT_CONNECT_TIMEOUT_SECONDS);
    client.set_read_timeout(ASSISTANT_HTTP_TIMEOUT_SECONDS);
    client.set_write_timeout(ASSISTANT_HTTP_TIMEOUT_SECONDS);
    // Past this, httplib stops reading and reports Error::Read, the same failure
    // as a dropped connection, so the panel cannot say which one happened.
    client.set_payload_max_length(ASSISTANT_MAX_RESPONSE_BYTES);
    // Redirects are deliberately not followed. cpp-httplib drops only a fixed
    // set of headers when a redirect crosses to another host, and the Azure
    // subscription key is not one of them, so following one could hand the key
    // to whatever host the redirect names. A chat endpoint has no reason to
    // redirect, so refusing costs nothing and InterpretResponse says why.
    client.set_follow_location(false);
#ifdef SO_NOSIGPIPE
    client.set_socket_options(SuppressSigpipe);
#endif

    httplib::Headers  headers = AuthHeaders(request.api_token, endpoint.flavour);
    const std::string body =
        BuildRequestBody(request, endpoint.flavour).toString();
    const std::string loggable = LoggableUrl(endpoint.url);
    spdlog::info("Assistant POST {}", loggable);

    if(!Adopt(&client))
    {
        WipeAuthHeaders(headers);
        return result;
    }
    const std::chrono::steady_clock::time_point started =
        std::chrono::steady_clock::now();
    // A cancel that lands before the request is under way finds no socket to
    // close, so the body write checks for one and gives up.
    const httplib::UploadProgress unless_cancelled = [this](size_t, size_t) {
        return !Cancelled();
    };
    httplib::Result response = client.Post(endpoint.path, headers, body,
                                           ASSISTANT_JSON_CONTENT_TYPE, unless_cancelled);
    // The client stays adopted across the retry so Cancel() can stop that too.
    const int wait_seconds = RetryWaitSeconds(response, started);
    if(wait_seconds > 0 && !Cancelled())
    {
        spdlog::warn("Assistant request to {} failed transiently, retrying in {} s",
                     loggable, wait_seconds);
        // Wait in short slices: Cancel() must be noticed promptly, because the
        // panel's destructor waits on this thread at quit.
        const std::chrono::steady_clock::time_point wake =
            std::chrono::steady_clock::now() + std::chrono::seconds(wait_seconds);
        while(!Cancelled() && std::chrono::steady_clock::now() < wake)
        {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(ASSISTANT_RETRY_POLL_MILLISECONDS));
        }
        if(!Cancelled())
        {
            response = client.Post(endpoint.path, headers, body,
                                   ASSISTANT_JSON_CONTENT_TYPE, unless_cancelled);
        }
    }
    Release();
    WipeAuthHeaders(headers);

    if(!response)
    {
        // Cancel() closed this socket on purpose, so the failure is expected.
        if(Cancelled())
        {
            return result;
        }
        // Not "HTTPS": a local server on plain http fails here too, and so does a
        // request httplib refused to write.
        result.error = "The assistant request failed (" +
                       httplib::to_string(response.error()) + ").";
        spdlog::warn("Assistant request to {} failed: {}", loggable,
                     httplib::to_string(response.error()));
        return result;
    }

    // A cancel during the retry wait leaves the first, failed response in hand.
    if(Cancelled())
    {
        return result;
    }

    InterpretResponse(*response, request.enable_tools, result);
    // An endpoint that echoes the key would otherwise put it in the transcript,
    // a toast, the log, and any export, or hand it to a tool as an argument.
    ScrubErrorText(request.api_token, result.error);
    AssistantRedactKey(request.api_token, result.reply);
    for(AssistantToolCall& call : result.tool_calls)
    {
        AssistantRedactKey(request.api_token, call.id);
        AssistantRedactKey(request.api_token, call.name);
        AssistantRedactKey(request.api_token, call.arguments);
    }
    // A 404 is nearly always the URL being shaped wrong, so name what we asked
    // for. Added after the scrub so a long provider message cannot cut it off.
    if(!result.error.empty() && response->status == ASSISTANT_HTTP_NOT_FOUND)
    {
        result.error.append(" Requested ");
        result.error.append(loggable);
        result.error.append(".");
    }
    result.ok = result.error.empty();
    if(!result.ok)
    {
        // The status only: a provider's error text can quote part of the key.
        spdlog::warn("Assistant request to {} returned an error (HTTP {})", loggable,
                     response->status);
    }
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

// The futures join their workers as they are destroyed. A cancelled worker
// returns at its next body chunk, or when its timeouts run out.
AssistantModelCatalog::~AssistantModelCatalog()
{
    CancelModelCatalogRequestNow(m_request);
}

void
AssistantModelCatalog::Request(const std::string& endpoint_url,
                               const std::string& api_token)
{
    // Only a hash of the key is kept, to tell one request from the next.
    const size_t key_hash = std::hash<std::string>()(Core::String::trim_copy(api_token));
    if(m_lookup.valid())
    {
        if(m_lookup_url == endpoint_url && m_lookup_key_hash == key_hash)
        {
            return;
        }
        std::future<void> stop = CancelModelCatalogRequestAsync(m_request);
        if(stop.valid())
        {
            m_stops.push_back(std::move(stop));
        }
        m_abandoned.push_back(std::move(m_lookup));
    }

    // Nothing from an earlier response remains selectable during a refresh,
    // even when the URL and key are unchanged.
    m_models.clear();
    m_url.clear();
    ++m_revision;

    m_lookup_url      = endpoint_url;
    m_lookup_key_hash = key_hash;
    if(Core::String::trim_copy(endpoint_url).empty())
    {
        m_request.reset();
        m_url = endpoint_url;
        return;
    }

    m_request           = std::make_shared<assistant_model_catalog_request_t>();
    m_request->deadline = std::chrono::steady_clock::now() +
                          std::chrono::seconds(ASSISTANT_CATALOG_TOTAL_TIMEOUT_SECONDS);
    m_lookup =
        std::async(std::launch::async, [endpoint_url, api_token, request = m_request]() {
            return FetchModelCatalog(endpoint_url, api_token, request);
        });
}

bool
AssistantModelCatalog::Poll()
{
    m_abandoned.erase(
        std::remove_if(m_abandoned.begin(), m_abandoned.end(),
                       [](const std::future<std::vector<std::string>>& lookup) {
                           return lookup.wait_for(std::chrono::seconds(0)) ==
                                  std::future_status::ready;
                       }),
        m_abandoned.end());
    m_stops.erase(std::remove_if(m_stops.begin(), m_stops.end(),
                                 [](const std::future<void>& stop) {
                                     return stop.wait_for(std::chrono::seconds(0)) ==
                                            std::future_status::ready;
                                 }),
                  m_stops.end());

    if(!m_lookup.valid())
    {
        return false;
    }
    if(m_request != nullptr && std::chrono::steady_clock::now() >= m_request->deadline)
    {
        std::future<void> stop = CancelModelCatalogRequestAsync(m_request);
        if(stop.valid())
        {
            m_stops.push_back(std::move(stop));
        }
    }
    if(m_lookup.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
    {
        return true;
    }
    m_models = m_lookup.get();
    m_url    = m_lookup_url;
    ++m_revision;
    return false;
}

const std::vector<std::string>&
AssistantModelCatalog::Models() const
{
    return m_models;
}

const std::string&
AssistantModelCatalog::Url() const
{
    return m_url;
}

uint64_t
AssistantModelCatalog::Revision() const
{
    return m_revision;
}

}  // namespace View
}  // namespace RocProfVis
