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
#include "rocprofvis_ai_client.h"
#include "rocprofvis_font_manager.h"
#include "rocprofvis_settings_manager.h"
#include "rocprofvis_settings_panel.h"
#include "widgets/rocprofvis_gui_helpers.h"
#include "widgets/rocprofvis_notification_manager.h"

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
// named after the endpoint. The name alone does not tie a key to a host - the
// URL under a name can change - so the value also records the origin the key
// was saved for; see SplitStoredToken.
std::string
AssistantTokenKey(const std::string& provider_name)
{
    if(provider_name.empty())
    {
        return ASSISTANT_TOKEN_SECRET_KEY;
    }
    return std::string(ASSISTANT_TOKEN_SECRET_KEY) + "/" + provider_name;
}

// Separates the origin from the key in a stored value. A line break cannot be
// typed into either field, so it cannot occur inside a key.
constexpr char ASSISTANT_TOKEN_ORIGIN_SEPARATOR = '\n';

// Overwrites a key before the string holding it is emptied or freed, so the
// bytes do not sit in released memory for a crash dump to pick up. The whole
// buffer is covered, not just the current length, and the writes go through a
// volatile pointer so the compiler cannot drop them as dead stores.
void
WipeSecret(std::string& text)
{
    text.resize(text.capacity());
    volatile char* bytes = text.data();
    for(size_t i = 0; i < text.size(); i++)
    {
        bytes[i] = '\0';
    }
    text.clear();
}

// The origin ("scheme://host[:port]", lower-cased) the named endpoint points at
// right now. Empty when there is no such endpoint or its URL has no scheme,
// which is also a URL the client refuses to send anything to.
std::string
AssistantProviderOrigin(const AssistantSettings& assistant,
                        const std::string&       provider_name)
{
    for(const AssistantProvider& provider : assistant.providers)
    {
        if(provider.name == provider_name)
        {
            return AssistantUrlOrigin(provider.endpoint_url);
        }
    }
    return std::string();
}

// What is kept for a key: the origin it was saved for, a line break, the key.
std::string
JoinStoredToken(const std::string& origin, const std::string& token)
{
    std::string stored = origin;
    stored.push_back(ASSISTANT_TOKEN_ORIGIN_SEPARATOR);
    stored.append(token);
    return stored;
}

// Takes a stored value apart again. A value written before origins were
// recorded has no line break: it is all key, and its origin comes back empty.
void
SplitStoredToken(const std::string& stored, std::string& origin_out,
                 std::string& token_out)
{
    const size_t separator = stored.find(ASSISTANT_TOKEN_ORIGIN_SEPARATOR);
    if(separator == std::string::npos)
    {
        origin_out.clear();
        token_out = stored;
        return;
    }
    origin_out = stored.substr(0, separator);
    token_out  = stored.substr(separator + 1);
}

// True when the URL has a part that is often a credential: user:password@ in
// front of the host, or a query string. The settings file stores the URL as
// typed, so the page warns about these. A query that is only the Azure API
// version is not a secret and is left alone; guessing which other parameter
// names are keys would never be complete, so any other query warns.
bool
UrlMayCarrySecret(const std::string& url)
{
    const std::string scheme_separator = "://";
    const size_t      scheme_end       = url.find(scheme_separator);
    const size_t      authority_start =
        (scheme_end == std::string::npos) ? 0 : scheme_end + scheme_separator.size();
    const size_t authority_end = url.find_first_of("/?#", authority_start);
    const size_t userinfo_end  = url.find('@', authority_start);
    if(userinfo_end != std::string::npos &&
       (authority_end == std::string::npos || userinfo_end < authority_end))
    {
        return true;
    }

    const size_t query_start = url.find('?');
    if(query_start == std::string::npos)
    {
        return false;
    }
    std::string  query          = url.substr(query_start + 1);
    const size_t fragment_start = query.find('#');
    if(fragment_start != std::string::npos)
    {
        query.resize(fragment_start);
    }
    if(query.empty())
    {
        return false;
    }
    const bool only_api_version =
        query.find('&') == std::string::npos &&
        (query.compare(0, 12, "api-version=") == 0 ||
         query.compare(0, 11, "apiVersion=") == 0);
    return !only_api_version;
}
}  // namespace

// Moves a key written before endpoints were named onto the first endpoint, once
// at load, then deletes the unnamed entry. Reading it as a live fallback
// instead would let any endpoint without a key of its own inherit this one -
// which is how a key for one provider ends up posted to another's host. The
// moved value has no origin recorded yet; GetAssistantToken adds it on the
// first read.
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
    // Whether the key counts depends on the origin saved inside the value, so
    // presence cannot be answered without reading it. The copy is wiped rather
    // than just dropped.
    std::string unused;
    const bool  has_token = GetAssistantToken(provider_name, unused);
    WipeSecret(unused);
    return has_token;
}

/*
 * Reads this endpoint's key from this session's memory, then from the
 * credential store. A session copy exists only when there was no store to save
 * to or the write to it failed, so it is the newer key and wins over whatever
 * the store still holds. Never falls through to another endpoint's key: an
 * endpoint with nothing saved has no key.
 *
 * The key is returned only when the origin saved with it is the origin the
 * endpoint's URL has now. The name alone would not do: the URL under a name can
 * be edited, in the dialog or in the settings file, and the key would follow it
 * to the new host. A key with no origin - saved by an older build, or while the
 * URL had no scheme - belongs to no host yet, so it is given the current origin
 * here, once, and is held to it from then on.
 */
bool
SettingsManager::GetAssistantToken(const std::string& provider_name,
                                   std::string&       out_token) const
{
    out_token.clear();

    const std::string origin =
        AssistantProviderOrigin(m_usersettings.assistant, provider_name);
    if(origin.find(ASSISTANT_TOKEN_ORIGIN_SEPARATOR) != std::string::npos)
    {
        // Only a hand-edited settings file can put a line break in the URL, and
        // such an origin could not be told apart from the key once saved.
        return false;
    }

    std::string saved_origin;
    const std::map<std::string, std::string>::iterator session =
        m_assistant_token_session.find(provider_name);
    if(session != m_assistant_token_session.end() && !session->second.empty())
    {
        SplitStoredToken(session->second, saved_origin, out_token);
        if(saved_origin.empty() && !origin.empty())
        {
            WipeSecret(session->second);
            session->second = JoinStoredToken(origin, out_token);
            saved_origin    = origin;
        }
    }
    else
    {
        const std::string key = AssistantTokenKey(provider_name);
        std::string       stored;
        if(!SecretStore::IsAvailable() || !SecretStore::Get(key, stored) ||
           stored.empty())
        {
            return false;
        }
        SplitStoredToken(stored, saved_origin, out_token);
        WipeSecret(stored);
        if(saved_origin.empty() && !origin.empty())
        {
            std::string bound = JoinStoredToken(origin, out_token);
            if(!SecretStore::Set(key, bound))
            {
                // The key still works for this read; the next one tries again.
                spdlog::warn("Could not record which host the saved assistant key "
                             "belongs to");
            }
            WipeSecret(bound);
            saved_origin = origin;
        }
    }

    if(saved_origin == origin && !out_token.empty())
    {
        return true;
    }
    WipeSecret(out_token);
    return false;
}

// Saves the key, or clears it when the token is empty. Returns false when the
// credential store refused it; the key is then kept for this session only.
// The origin of the endpoint's URL is saved with it, which is what
// GetAssistantToken checks before handing the key back.
bool
SettingsManager::SetAssistantToken(const std::string& provider_name,
                                   const std::string& token)
{
    if(token.empty())
    {
        return ClearAssistantToken(provider_name);
    }

    std::string stored = JoinStoredToken(
        AssistantProviderOrigin(m_usersettings.assistant, provider_name), token);
    const std::map<std::string, std::string>::iterator session =
        m_assistant_token_session.find(provider_name);
    if(session != m_assistant_token_session.end())
    {
        // Whichever way this goes the old session copy is replaced or dropped.
        WipeSecret(session->second);
        m_assistant_token_session.erase(session);
    }

    bool saved = true;
    if(SecretStore::IsAvailable())
    {
        // When the vault takes it, it owns it. A second copy in process memory
        // would only widen the window where the key can be read back out.
        saved = SecretStore::Set(AssistantTokenKey(provider_name), stored);
        if(!saved)
        {
            // Keep the key for this session rather than dropping what was just
            // typed.
            spdlog::warn("Could not save the assistant key to the credential store; "
                         "keeping it for this session only");
            m_assistant_token_session[provider_name] = stored;
        }
    }
    else
    {
        m_assistant_token_session[provider_name] = stored;
    }
    WipeSecret(stored);
    return saved;
}

// Forgets this endpoint's key. Other endpoints keep theirs.
bool
SettingsManager::ClearAssistantToken(const std::string& provider_name)
{
    const std::map<std::string, std::string>::iterator session =
        m_assistant_token_session.find(provider_name);
    if(session != m_assistant_token_session.end())
    {
        WipeSecret(session->second);
        m_assistant_token_session.erase(session);
    }
    if(!SecretStore::IsAvailable())
    {
        return true;
    }
    if(!SecretStore::Erase(AssistantTokenKey(provider_name)))
    {
        spdlog::warn("Could not remove the assistant key from the credential store");
        return false;
    }
    return true;
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
        "OpenAI-compatible chat endpoint. The key entered below is stored in "
        "the OS credential store, never in the settings file.");

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
    // Both stay on screen for as long as they apply, not in the tooltip: each
    // describes something the URL as typed will do that the field cannot show.
    const bool url_cleartext    = AssistantUrlIsCleartext(provider.endpoint_url);
    const bool url_holds_secret = UrlMayCarrySecret(provider.endpoint_url);
    if(url_cleartext || url_holds_secret)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, m_settings.GetColor(Colors::kTextWarning));
        if(url_cleartext)
        {
            ImGui::TextWrapped(
                "Unencrypted connection: questions and trace data sent to this "
                "endpoint can be read or altered on the network.");
        }
        if(url_holds_secret)
        {
            ImGui::TextWrapped(
                "Anything in the URL, including a key in the query string, is "
                "saved in plain text in the settings file. Put the key in the "
                "Key field instead.");
        }
        ImGui::PopStyleColor();
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

    if(m_assistant_token_stored < 0)
    {
        m_assistant_token_stored = m_settings.HasAssistantToken(provider.name) ? 1 : 0;
        m_assistant_store_available = SecretStore::IsAvailable() ? 1 : 0;
    }
    const bool token_stored = m_assistant_token_stored == 1 && !m_assistant_clear_token;
    // The store was asked about the URL the dialog opened with. Once the URL
    // names another origin that key is not this endpoint's any more: it would
    // be refused on the next question, so OK removes it instead of leaving it
    // behind, and the page says so rather than offering to keep it.
    const bool origin_changed = AssistantOriginChanged();
    const bool token_kept     = token_stored && !origin_changed;
    if(token_stored && origin_changed && m_assistant_token_draft.empty())
    {
        ImGui::TextWrapped(
            "The stored key belongs to the previous host and is removed when you "
            "press OK. Enter the key for this endpoint.");
    }
    else if(token_kept && m_assistant_token_draft.empty())
    {
        ImGui::TextUnformatted("A key is stored. Leave the field blank to keep it.");
    }
    else if(m_assistant_store_available == 1)
    {
        ImGui::TextWrapped(
            "The key typed here is saved to the OS credential store when you "
            "press OK, never to the settings file.");
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
                               token_kept ? "Stored — type to replace" : "API key",
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
        WipeSecret(m_assistant_token_draft);
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
    bool erase_failed = false;
    for(const std::string& orphan : m_assistant_orphaned_providers)
    {
        if(!m_settings.ClearAssistantToken(orphan))
        {
            erase_failed = true;
        }
    }

    if(m_usersettings.assistant.active < m_usersettings.assistant.providers.size())
    {
        ApplyAssistantEndpointDefaults(
            m_usersettings.assistant.providers[m_usersettings.assistant.active]);
    }

    const std::string provider_name = ActiveAssistantProviderName();
    if(m_assistant_clear_token)
    {
        if(!m_settings.ClearAssistantToken(provider_name))
        {
            erase_failed = true;
        }
    }
    else if(!m_assistant_token_draft.empty())
    {
        if(!m_settings.SetAssistantToken(provider_name, m_assistant_token_draft))
        {
            // The dialog closes on OK, so this is the only place the user can
            // learn the key was not saved.
            NotificationManager::GetInstance().Show(
                "Could not save the API key to the credential store. It will be "
                "kept for this session only.",
                NotificationLevel::Warning);
        }
    }
    else if(m_assistant_token_stored == 1 && AssistantOriginChanged())
    {
        // The URL moved to another host and no key was typed for it. The stored
        // key was saved for the old host and would be refused for this one, so
        // it goes now, and the user is told why the endpoint has no key.
        if(!m_settings.ClearAssistantToken(provider_name))
        {
            erase_failed = true;
        }
        NotificationManager::GetInstance().Show(
            "The assistant URL now points at a different host, so the stored API "
            "key was removed. Enter the key for the new endpoint.",
            NotificationLevel::Warning);
    }

    if(erase_failed)
    {
        // Same reasoning as a failed save: nothing else tells the user that the
        // key they asked to remove is still there.
        NotificationManager::GetInstance().Show(
            "Could not remove the API key from the credential store. It may "
            "still be stored on this machine.",
            NotificationLevel::Warning);
    }
    DiscardAssistantTokenEdits();
}

// True when the endpoint being edited no longer points at the origin it had
// when the dialog opened. An endpoint that had no usable URL then was not tied
// to any host, so giving it one is not a change.
bool
SettingsPanel::AssistantOriginChanged() const
{
    const std::string provider_name = ActiveAssistantProviderName();
    const std::string initial_origin =
        AssistantProviderOrigin(m_usersettings_initial.assistant, provider_name);
    return !initial_origin.empty() &&
           AssistantProviderOrigin(m_usersettings.assistant, provider_name) !=
               initial_origin;
}

void
SettingsPanel::DiscardAssistantTokenEdits()
{
    // The draft is the key in plain text, so its buffer is overwritten rather
    // than just marked empty.
    WipeSecret(m_assistant_token_draft);
    m_assistant_orphaned_providers.clear();
    m_assistant_clear_token  = false;
    m_assistant_token_stored = -1;
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
    WipeSecret(m_assistant_token_draft);
    // The default endpoint has nothing configured now, so its key should go too.
    m_assistant_clear_token = true;
    // The endpoint being edited may have a different name now, so ask the
    // credential store again.
    m_assistant_token_stored = -1;
    m_settings_changed       = true;
}

}  // namespace View
}  // namespace RocProfVis
