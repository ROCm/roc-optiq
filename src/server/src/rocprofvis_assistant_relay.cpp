// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_assistant_relay.h"

#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <sstream>
#include <string>

#include "rocprofvis_ai_actions.h"
#include "rocprofvis_ai_client.h"
#include "rocprofvis_ai_prompts.h"
#include "rocprofvis_ai_tool_schema.h"
#include "rocprofvis_secret_store.h"
#include "rocprofvis_utils.h"

namespace RocProfVis
{
namespace View
{

// The show_panel schema lists the panels a client can open. The desktop
// defines this beside its UI actions; these are the web client's panels.
std::string
OptiqActions::PanelNameList()
{
    return "overview (minimap, map, histogram, activity), topology (sidebar, "
           "navbar, tree, tracks, left panel), details (analysis, tables, event "
           "table, bottom), summary (stats), log (console, output), annotations "
           "(notes, sticky notes). Whatever the user called it is fine: matching "
           "ignores case, spacing, and trailing words like \"view\" or \"panel\"";
}

}  // namespace View

namespace Server
{

// Where and how the desktop app keeps its Ask Optiq settings, so the web client
// works with whatever the user already configured there.
constexpr const char* SETTINGS_FILE    = "settings_application.json";
constexpr const char* TOKEN_SECRET_KEY = "assistant-api-token";
constexpr const char* DEFAULT_PROVIDER = "Default";
constexpr const char* DEFAULT_MODEL    = "gpt-5.6-luna";
constexpr int32_t     HTTP_OK          = 200;
constexpr int32_t     HTTP_BAD_REQUEST = 400;
constexpr int32_t     HTTP_INTERNAL    = 500;
constexpr int32_t     HTTP_BAD_GATEWAY = 502;

// HTTP workers read and write the settings file and the session keys at once.
static std::mutex g_mutex;
// Keys saved when there is no OS credential store, kept until the server stops,
// as the desktop app keeps them for its session.
static std::map<std::string, std::string> g_session_tokens;

struct Endpoint
{
    std::string name = DEFAULT_PROVIDER;
    std::string url;
    std::string model = DEFAULT_MODEL;
};

static std::string
StringField(jt::Json& json, const char* key)
{
    return json.isObject() && json.contains(key) && json[key].isString()
               ? json[key].getString()
               : std::string();
}

static std::filesystem::path
SettingsPath(bool create_dirs)
{
    return std::filesystem::u8path(View::get_application_config_path(create_dirs)) /
           SETTINGS_FILE;
}

// The desktop settings file; a missing file reads as empty. False when the file
// is there but is not a settings object, so it must not be overwritten.
static bool
ReadSettings(jt::Json& root)
{
    root.setObject();
    std::ifstream file(SettingsPath(false));
    if(!file.is_open())
    {
        return true;
    }
    std::stringstream text;
    text << file.rdbuf();
    std::pair<jt::Json::Status, jt::Json> parsed = jt::Json::parse(text.str());
    if(parsed.first != jt::Json::success || !parsed.second.isObject())
    {
        return false;
    }
    root = std::move(parsed.second);
    return true;
}

// The active endpoint's entry in the settings, added with the defaults when
// there is none.
static jt::Json&
ActiveProvider(jt::Json& root)
{
    jt::Json& assistant = root["settings"]["assistant"];
    jt::Json& providers = assistant["providers"];
    if(!providers.isArray() || providers.getArray().empty())
    {
        jt::Json provider;
        provider["name"]         = DEFAULT_PROVIDER;
        provider["endpoint_url"] = "";
        provider["model"]        = DEFAULT_MODEL;
        providers.setArray();
        providers.getArray().push_back(std::move(provider));
    }
    size_t active = 0;
    if(assistant.contains("active") && assistant["active"].isLong() &&
       assistant["active"].getLong() > 0 &&
       static_cast<size_t>(assistant["active"].getLong()) < providers.getArray().size())
    {
        active = static_cast<size_t>(assistant["active"].getLong());
    }
    return providers.getArray()[active];
}

static Endpoint
ToEndpoint(jt::Json& provider)
{
    Endpoint          endpoint;
    const std::string name  = StringField(provider, "name");
    const std::string model = StringField(provider, "model");
    endpoint.url            = StringField(provider, "endpoint_url");
    endpoint.name           = name.empty() ? endpoint.name : name;
    endpoint.model          = model.empty() ? endpoint.model : model;
    return endpoint;
}

// The active endpoint in the desktop settings, or the defaults when there is none.
static Endpoint
ActiveEndpoint()
{
    std::lock_guard<std::mutex> lock(g_mutex);
    jt::Json                    root;
    return ReadSettings(root) ? ToEndpoint(ActiveProvider(root)) : Endpoint();
}

// Each endpoint has its own entry, so a key saved for one host is never sent
// to another.
static std::string
TokenKey(const Endpoint& endpoint)
{
    return std::string(TOKEN_SECRET_KEY) + "/" + endpoint.name;
}

static std::string
StoredToken(const Endpoint& endpoint)
{
    std::string token;
    if(View::SecretStore::IsAvailable())
    {
        View::SecretStore::Get(TokenKey(endpoint), token);
        return token;
    }
    std::lock_guard<std::mutex> lock(g_mutex);
    auto                        session = g_session_tokens.find(endpoint.name);
    return session == g_session_tokens.end() ? token : session->second;
}

// Saves the key, or forgets it when token is empty.
static bool
StoreToken(const Endpoint& endpoint, const std::string& token)
{
    if(View::SecretStore::IsAvailable())
    {
        if(token.empty())
        {
            View::SecretStore::Erase(TokenKey(endpoint));
            return true;
        }
        return View::SecretStore::Set(TokenKey(endpoint), token);
    }
    std::lock_guard<std::mutex> lock(g_mutex);
    if(token.empty())
    {
        g_session_tokens.erase(endpoint.name);
    }
    else
    {
        g_session_tokens[endpoint.name] = token;
    }
    return true;
}

jt::Json
AssistantStatus()
{
    const Endpoint endpoint = ActiveEndpoint();
    jt::Json       tools    = View::BuildAssistantToolsJson();
    jt::Json       names;
    names.setArray();
    for(jt::Json& tool : tools.getArray())
    {
        jt::Json entry;
        entry["name"] = tool["function"]["name"];
        entry["label"] =
            View::AssistantToolStatusLabel(tool["function"]["name"].getString());
        names.getArray().push_back(std::move(entry));
    }

    jt::Json json;
    json["name"]             = endpoint.name;
    json["endpoint_url"]     = endpoint.url;
    json["model"]            = endpoint.model;
    json["has_token"]        = !StoredToken(endpoint).empty();
    json["credential_store"] = View::SecretStore::IsAvailable();
    json["tools"]            = std::move(names);
    return json;
}

jt::Json
AssistantSave(jt::Json body, int32_t& status_out)
{
    jt::Json json;
    Endpoint endpoint;
    status_out = HTTP_INTERNAL;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        jt::Json                    root;
        if(!ReadSettings(root))
        {
            json["error"] = "The desktop settings file could not be read, so nothing "
                            "was saved.";
            return json;
        }
        jt::Json& provider = ActiveProvider(root);
        bool      changed  = false;
        for(const char* field : { "endpoint_url", "model" })
        {
            if(body.contains(field) && body[field].isString())
            {
                provider[field] = body[field].getString();
                changed         = true;
            }
        }
        if(changed)
        {
            std::ofstream file(SettingsPath(true));
            file << root.toStringPretty();
            if(!file)
            {
                json["error"] = "Could not write the desktop settings file.";
                return json;
            }
        }
        endpoint = ToEndpoint(provider);
    }

    const bool clear = body.contains("clear_token") && body["clear_token"].isBool() &&
                       body["clear_token"].getBool();
    const std::string token = StringField(body, "token");
    if((clear || !token.empty()) && !StoreToken(endpoint, clear ? std::string() : token))
    {
        json["error"] = "Could not save the key to the OS credential store.";
        return json;
    }
    status_out = HTTP_OK;
    return AssistantStatus();
}

jt::Json
AssistantChat(jt::Json body, int32_t& status_out)
{
    const Endpoint    endpoint = ActiveEndpoint();
    const std::string token    = StoredToken(endpoint);

    jt::Json json;
    status_out = HTTP_BAD_REQUEST;
    if(endpoint.url.empty() || token.empty())
    {
        json["error"] =
            "Ask Optiq needs an endpoint URL and a key. Set them in Preferences.";
        return json;
    }
    if(!body.contains("messages") || !body["messages"].isArray())
    {
        json["error"] = "Send {\"messages\": [...]}.";
        return json;
    }

    View::AssistantChatRequest request;
    request.endpoint_url = endpoint.url;
    request.model        = endpoint.model;
    request.api_token    = token;
    request.enable_tools =
        !(body.contains("final") && body["final"].isBool() && body["final"].getBool());

    View::AssistantMessage system;
    system.role    = "system";
    system.content = View::AssistantSystemPrompt();
    request.messages.push_back(system);
    // The system prompt is the server's; a client cannot replace it.
    for(jt::Json& item : body["messages"].getArray())
    {
        View::AssistantMessage message;
        message.role         = StringField(item, "role");
        message.content      = StringField(item, "content");
        message.name         = StringField(item, "name");
        message.tool_call_id = StringField(item, "tool_call_id");
        if(message.role != "user" && message.role != "assistant" &&
           message.role != "tool")
        {
            continue;
        }
        if(item.contains("tool_calls") && item["tool_calls"].isArray())
        {
            for(jt::Json& call : item["tool_calls"].getArray())
            {
                message.tool_calls.push_back(View::AssistantToolCall{
                    StringField(call, "id"), StringField(call, "name"),
                    StringField(call, "arguments") });
            }
        }
        request.messages.push_back(std::move(message));
    }

    View::AssistantChatCall   call;
    View::AssistantChatResult result = call.Send(request);
    jt::Json                  calls;
    calls.setArray();
    for(const View::AssistantToolCall& tool_call : result.tool_calls)
    {
        jt::Json entry;
        entry["id"]        = tool_call.id;
        entry["name"]      = tool_call.name;
        entry["arguments"] = tool_call.arguments;
        calls.getArray().push_back(std::move(entry));
    }
    json["reply"]         = result.reply;
    json["finish_reason"] = result.finish_reason;
    json["tool_calls"]    = std::move(calls);
    if(!result.ok)
    {
        json["error"] = result.error.empty() ? "The model did not answer." : result.error;
    }
    status_out = result.ok ? HTTP_OK : HTTP_BAD_GATEWAY;
    return json;
}

}  // namespace Server
}  // namespace RocProfVis
