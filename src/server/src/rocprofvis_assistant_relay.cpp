// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_assistant_relay.h"

#include <filesystem>
#include <fstream>
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
constexpr int32_t     HTTP_BAD_GATEWAY = 502;

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

// The active endpoint in the desktop settings, or the defaults when there is none.
static Endpoint
ActiveEndpoint()
{
    Endpoint      endpoint;
    std::ifstream file(std::filesystem::u8path(View::get_application_config_path(false)) /
                       SETTINGS_FILE);
    std::stringstream text;
    text << file.rdbuf();
    std::pair<jt::Json::Status, jt::Json> parsed = jt::Json::parse(text.str());
    jt::Json&                             root   = parsed.second;
    if(parsed.first == jt::Json::success && root.isObject() &&
       root.contains("settings") && root["settings"].isObject() &&
       root["settings"].contains("assistant"))
    {
        jt::Json& assistant = root["settings"]["assistant"];
        if(assistant.isObject() && assistant.contains("providers") &&
           assistant["providers"].isArray() && !assistant["providers"].getArray().empty())
        {
            std::vector<jt::Json>& providers = assistant["providers"].getArray();
            size_t                 active    = 0;
            if(assistant.contains("active") && assistant["active"].isLong() &&
               assistant["active"].getLong() > 0 &&
               static_cast<size_t>(assistant["active"].getLong()) < providers.size())
            {
                active = static_cast<size_t>(assistant["active"].getLong());
            }
            jt::Json&         provider = providers[active];
            const std::string name     = StringField(provider, "name");
            const std::string model    = StringField(provider, "model");
            endpoint.url               = StringField(provider, "endpoint_url");
            endpoint.name              = name.empty() ? endpoint.name : name;
            endpoint.model             = model.empty() ? endpoint.model : model;
        }
    }
    return endpoint;
}

// The key the desktop app stored for this endpoint, if any.
static std::string
StoredToken(const Endpoint& endpoint)
{
    std::string token;
    if(View::SecretStore::IsAvailable())
    {
        View::SecretStore::Get(std::string(TOKEN_SECRET_KEY) + "/" + endpoint.name,
                               token);
    }
    return token;
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
AssistantChat(jt::Json body, int32_t& status_out)
{
    Endpoint          endpoint = ActiveEndpoint();
    const std::string url      = StringField(body, "endpoint_url");
    const std::string model    = StringField(body, "model");
    std::string       token    = StringField(body, "token");
    endpoint.url               = url.empty() ? endpoint.url : url;
    endpoint.model             = model.empty() ? endpoint.model : model;
    token                      = token.empty() ? StoredToken(endpoint) : token;

    jt::Json json;
    status_out = HTTP_BAD_REQUEST;
    if(endpoint.url.empty() || token.empty())
    {
        json["error"] = "Ask Optiq needs an endpoint URL and a key. Set them in the "
                        "desktop app's Preferences > Assistant, or in this page's "
                        "Ask Optiq settings.";
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
