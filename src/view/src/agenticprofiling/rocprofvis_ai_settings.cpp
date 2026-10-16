// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

// Assistant settings: the Settings-dialog page, and the credential-store access
// behind its API-key field. These are SettingsPanel and SettingsManager members
// because the page edits the same UserSettings copy and OK/Cancel state as every
// other page, but the bodies live here so the assistant owns its own settings
// and the shared files carry none of it.
//
// Reading and writing the endpoint list stays in rocprofvis_settings_manager.cpp
// with every other Serialize/Deserialize pair: it runs unguarded, so a build
// with the assistant off still round-trips a saved configuration instead of
// erasing it.
#include "imgui.h"

#include "icons/rocprovfis_icon_defines.h"
#include "remote/rocprofvis_secret_store.h"
#include "rocprofvis_font_manager.h"
#include "rocprofvis_settings_manager.h"
#include "rocprofvis_settings_panel.h"
#include "widgets/rocprofvis_gui_helpers.h"

#include "spdlog/spdlog.h"

#include <map>
#include <string>

namespace RocProfVis
{
namespace View
{

namespace
{
// Credential-store entry for one endpoint. Every endpoint gets its own entry,
// so a key configured for one host can never be sent to another.
std::string
AssistantTokenKey(const std::string& provider_name)
{
    if(provider_name.empty())
    {
        return ASSISTANT_TOKEN_SECRET_KEY;
    }
    return std::string(ASSISTANT_TOKEN_SECRET_KEY) + "/" + provider_name;
}
}  // namespace

// Moves a key written before endpoints were named onto the first endpoint, once
// at load, then deletes the unnamed entry. Reading it as a live fallback
// instead would let any endpoint without a key of its own inherit this one -
// which is how a key for one provider ends up posted to another's host.
void
SettingsManager::MigrateLegacyAssistantToken()
{
    if(!SecretStore::IsAvailable() || m_usersettings.assistant.providers.empty())
    {
        return;
    }

    std::string legacy_token;
    if(!SecretStore::Get(ASSISTANT_TOKEN_SECRET_KEY, legacy_token) ||
       legacy_token.empty())
    {
        return;
    }

    // The single endpoint that existed before the list did is the first one.
    const std::string key =
        AssistantTokenKey(m_usersettings.assistant.providers.front().name);
    if(key == ASSISTANT_TOKEN_SECRET_KEY)
    {
        return;
    }

    std::string existing_token;
    const bool  already_migrated =
        SecretStore::Get(key, existing_token) && !existing_token.empty();
    if(!already_migrated && !SecretStore::Set(key, legacy_token))
    {
        // Leave the old entry alone rather than destroying the only copy.
        spdlog::warn("Could not migrate the saved assistant key; leaving it in place");
        return;
    }
    SecretStore::Erase(ASSISTANT_TOKEN_SECRET_KEY);
}

bool
SettingsManager::HasAssistantToken(const std::string& provider_name) const
{
    std::string unused;
    return GetAssistantToken(provider_name, unused);
}

// Reads this endpoint's key from the credential store, or from this session's
// memory when no store is available. Never falls through to another endpoint's
// key: an endpoint with nothing saved has no key.
bool
SettingsManager::GetAssistantToken(const std::string& provider_name,
                                   std::string&       out_token) const
{
    out_token.clear();
    if(SecretStore::IsAvailable() &&
       SecretStore::Get(AssistantTokenKey(provider_name), out_token) &&
       !out_token.empty())
    {
        return true;
    }

    out_token.clear();
    const std::map<std::string, std::string>::const_iterator session =
        m_assistant_token_session.find(provider_name);
    if(session != m_assistant_token_session.end() && !session->second.empty())
    {
        out_token = session->second;
        return true;
    }
    return false;
}

// Saves the key, or clears it when the token is empty.
bool
SettingsManager::SetAssistantToken(const std::string& provider_name,
                                   const std::string& token)
{
    if(token.empty())
    {
        return ClearAssistantToken(provider_name);
    }

    if(SecretStore::IsAvailable())
    {
        // The vault owns it now. A second copy in process memory would only
        // widen the window where the key can be read back out.
        m_assistant_token_session.erase(provider_name);
        return SecretStore::Set(AssistantTokenKey(provider_name), token);
    }
    m_assistant_token_session[provider_name] = token;
    return true;
}

// Forgets this endpoint's key. Other endpoints keep theirs.
bool
SettingsManager::ClearAssistantToken(const std::string& provider_name)
{
    m_assistant_token_session.erase(provider_name);
    if(!SecretStore::IsAvailable())
    {
        return true;
    }
    return SecretStore::Erase(AssistantTokenKey(provider_name));
}

void
SettingsPanel::RenderAssistantSettings()
{
    ImGuiStyle& style = ImGui::GetStyle();

    AssistantSettings& assistant = m_usersettings.assistant;
    if(assistant.providers.empty())
    {
        assistant.providers.push_back(MakeDefaultAssistantProvider());
        assistant.active = 0;
    }
    if(assistant.active >= assistant.providers.size())
    {
        assistant.active = 0;
    }
    AssistantProvider& provider = assistant.providers[assistant.active];
    ApplyAssistantEndpointDefaults(provider);

    ImGui::TextUnformatted("Assistant");
    ImGui::Separator();
    ImGui::TextWrapped(
        "OpenAI-compatible chat endpoint. The key is stored in the OS "
        "credential store, never in the settings file.");

    const float label_width =
        ImGui::CalcTextSize("Model").x + 2.0f * style.ItemSpacing.x;

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("URL");
    ImGui::SameLine(label_width);
    ImGui::SetNextItemWidth(-1.0f);
    if(InputTextStringWithHint("##assistant_url", "https://<host>/<path>",
                               provider.endpoint_url))
    {
        m_settings_changed = true;
    }
    if(ImGui::IsItemHovered())
    {
        SetTooltipStyled("Base URL, e.g. https://api.openai.com/v1. "
                         "/chat/completions is appended. Azure-style /azure "
                         "and /openai paths also insert the model as the "
                         "deployment id.");
    }

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Model");
    ImGui::SameLine(label_width);
    ImGui::SetNextItemWidth(-1.0f);
    if(InputTextStringWithHint("##assistant_model", ASSISTANT_DEFAULT_MODEL,
                               provider.model))
    {
        m_settings_changed = true;
    }
    if(ImGui::IsItemHovered())
    {
        SetTooltipStyled("Model name sent to the endpoint. On Azure-style URLs "
                         "this is the deployment id. Default is %s.",
                         ASSISTANT_DEFAULT_MODEL);
    }

    ImGui::Dummy(ImVec2(0.0f, style.ItemSpacing.y));
    ImGui::TextUnformatted("API key");
    ImGui::Separator();

    const bool token_stored =
        m_settings.HasAssistantToken(provider.name) && !m_assistant_clear_token;
    if(token_stored && m_assistant_token_draft.empty())
    {
        ImGui::TextUnformatted("A key is stored. Leave the field blank to keep it.");
    }
    else if(SecretStore::IsAvailable())
    {
        ImGui::TextWrapped(
            "Saved to the OS credential store when you press OK, never to the "
            "settings file.");
    }
    else
    {
        ImGui::TextWrapped(
            "No OS credential store is available. The key is kept in memory "
            "for this session only.");
    }

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Key");
    ImGui::SameLine();
    const float eye_w = ImGui::GetFrameHeight();
    ImGui::SetNextItemWidth(-(eye_w + style.ItemInnerSpacing.x));
    ImGuiInputTextFlags token_flags =
        m_assistant_show_token ? 0 : ImGuiInputTextFlags_Password;
    if(InputTextStringWithHint("##assistant_token",
                               token_stored ? "Stored — type to replace" : "API key",
                               m_assistant_token_draft, token_flags))
    {
        m_assistant_clear_token = false;
        m_settings_changed      = true;
    }
    ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
    ImFont* icon_font = m_fonts.GetFont(FontType::kIcon);
    ImGui::PushID("assistant_token_reveal");
    if(IconButton(m_assistant_show_token ? ICON_EYE_SLASH : ICON_EYE, icon_font,
                  ImVec2(eye_w, eye_w), m_assistant_show_token ? "Hide" : "Show"))
    {
        m_assistant_show_token = !m_assistant_show_token;
    }
    ImGui::PopID();

    if(ImGui::Button("Clear stored key"))
    {
        m_assistant_token_draft.clear();
        m_assistant_clear_token = true;
        m_settings_changed      = true;
    }
}

// Called from OK, so nothing typed on this page reaches the credential store
// until the user accepts the dialog.
void
SettingsPanel::ApplyAssistantTokenEdits()
{
    // Endpoints dropped by Reset go first: their keys have nothing pointing at
    // them once the configuration is gone.
    for(const std::string& orphan : m_assistant_orphaned_providers)
    {
        m_settings.ClearAssistantToken(orphan);
    }

    if(m_usersettings.assistant.active < m_usersettings.assistant.providers.size())
    {
        ApplyAssistantEndpointDefaults(
            m_usersettings.assistant.providers[m_usersettings.assistant.active]);
    }

    const std::string provider_name = ActiveAssistantProviderName();
    if(m_assistant_clear_token)
    {
        m_settings.ClearAssistantToken(provider_name);
    }
    else if(!m_assistant_token_draft.empty())
    {
        m_settings.SetAssistantToken(provider_name, m_assistant_token_draft);
    }
    DiscardAssistantTokenEdits();
}

void
SettingsPanel::DiscardAssistantTokenEdits()
{
    m_assistant_token_draft.clear();
    m_assistant_orphaned_providers.clear();
    m_assistant_clear_token = false;
}

// The endpoint being edited, which is the key its token is stored under.
std::string
SettingsPanel::ActiveAssistantProviderName() const
{
    const AssistantSettings& assistant = m_usersettings.assistant;
    if(assistant.active >= assistant.providers.size())
    {
        return std::string();
    }
    return assistant.providers[assistant.active].name;
}

// Endpoints that disappear here are remembered so OK can delete their keys too,
// rather than leaving entries in the credential store nothing refers to.
void
SettingsPanel::ResetAssistantOptions()
{
    const AssistantProvider default_provider = MakeDefaultAssistantProvider();
    for(const AssistantProvider& provider : m_usersettings.assistant.providers)
    {
        if(provider.name != default_provider.name)
        {
            m_assistant_orphaned_providers.push_back(provider.name);
        }
    }

    m_usersettings.assistant.providers.clear();
    m_usersettings.assistant.providers.push_back(default_provider);
    m_usersettings.assistant.active = 0;
    m_assistant_token_draft.clear();
    // The default endpoint has nothing configured now, so its key should go too.
    m_assistant_clear_token = true;
    m_settings_changed      = true;
}

}  // namespace View
}  // namespace RocProfVis
